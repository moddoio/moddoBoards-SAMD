/*
  Copyright (c) 2026 moddo inc.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/* Composite (CDC+HID) support for CDC_BOOTLOADER builds.
 *
 * The bootloader owns EP0 and the CDC interfaces; this file extends the
 * device with PluggableUSB interfaces (HID) through the version-2
 * .bl_cdc_export EP0 hook. It also provides the minimal USBDeviceClass
 * surface (sendControl/packMessages/send/armSend) that the stock HID
 * library calls, backed by the bootloader driver.
 *
 * Linking is pay-per-use: nothing in the core references this file's
 * symbols directly, BootloaderCDC.cpp reaches it only through weak
 * references. The file is pulled from core.a when a sketch links something
 * that references USBDevice (i.e. the HID library), and CDC-only sketches
 * never carry it.
 */

#if defined(CDC_BOOTLOADER)

#include <Arduino.h>
#include "bl_cdc_export.h"
#include "PluggableUSB.h"

#include <string.h>

/* Composite identity: must differ from the bootloader's CDC-only PID
 * (0xE44B), or hosts reuse the cached 2-interface configuration for the
 * composite device. Matches the PID the native CDC+HID stack used.
 */
#define BL_COMPOSITE_PID  0xE44E

/* CDC function block of the configuration descriptor, byte-identical to
 * the bootloader's own (interfaces 0-1, endpoints 1-3) minus the 9-byte
 * configuration header. The hardware setup for these endpoints stays with
 * the bootloader's USB_Configure().
 */
static const uint8_t cdcInterfacesBlock[] =
{
  /* Communication Class Interface Descriptor */
  0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x00, 0x00,
  /* Header Functional Descriptor: CDC 1.1 */
  0x05, 0x24, 0x00, 0x10, 0x01,
  /* ACM Functional Descriptor */
  0x04, 0x24, 0x02, 0x00,
  /* Union Functional Descriptor: master 0, slave 1 */
  0x05, 0x24, 0x06, 0x00, 0x01,
  /* Call Management Functional Descriptor */
  0x05, 0x24, 0x01, 0x00, 0x01,
  /* Endpoint 3: interrupt IN, 8 bytes */
  0x07, 0x05, 0x83, 0x03, 0x08, 0x00, 0xFF,
  /* Data Class Interface Descriptor */
  0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
  /* Endpoint 1: bulk IN, 64 bytes */
  0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00,
  /* Endpoint 2: bulk OUT, 64 bytes */
  0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
};

/* PluggableUSB.cpp records each module's endpoint types here; normally
 * defined by USBCore.cpp, which CDC_BOOTLOADER compiles out. Contents are
 * not consulted, EP4 is configured as interrupt IN below, matching what
 * the HID library requests.
 */
uint32_t EndPoints[USB_ENDPOINTS];

static const bl_cdc_export_t* s_table;
static void* s_cdcHandle;
static uint8_t s_lastConfigured;

/* Control IN responses are assembled here and sent as one transfer (the
 * buffer is in RAM, which the bootloader's DMA path requires). Sized for a
 * config descriptor or a multi-report HID report descriptor; contributions
 * beyond the capacity are dropped, which shows up as a truncated (broken)
 * descriptor rather than memory corruption.
 */
static uint8_t s_ctrlBuf[256];
static uint16_t s_ctrlLen;
static uint16_t s_wLength;
static bool s_pack;
static bool s_dry;
static bool s_ctrlSent;
static uint16_t s_dryTotal;

static void ctrlFlush(void)
{
  uint32_t n = s_ctrlLen;
  uint8_t ep = 0;

  if (n > s_wLength)
  {
    n = s_wLength;
  }

  // A response ending exactly at wLength on a packet boundary must not
  // queue the driver's trailing auto-ZLP, the host stops the IN stage at
  // wLength and the pending ZLP would wedge the next write.
  if (n != 0 && n == s_wLength && (n & 63) == 0)
  {
    ep |= BL_EP_WRITE_EXACT;
  }

  s_ctrlLen = 0;
  s_ctrlSent = true;
  s_table->usb_ep_write(s_ctrlBuf, n, ep);
}

/* Minimal USBDeviceClass surface used by the HID library (HID.cpp calls
 * USBDevice.sendControl / packMessages / send / armSend). Any other
 * USBDeviceClass method stays undefined, a build error on its use is
 * preferable to silently broken behavior.
 */
uint32_t USBDeviceClass::sendControl(const void* data, uint32_t len)
{
  if (s_dry)
  {
    s_dryTotal += len;
    return len;
  }

  if (len > (uint32_t)(sizeof(s_ctrlBuf) - s_ctrlLen))
  {
    len = sizeof(s_ctrlBuf) - s_ctrlLen;
  }

  memcpy(&s_ctrlBuf[s_ctrlLen], data, len);
  s_ctrlLen += len;

  if (!s_pack)
  {
    ctrlFlush();
  }

  return len;
}

void USBDeviceClass::packMessages(bool val)
{
  if (val)
  {
    s_pack = true;
    s_ctrlLen = 0;
  }
  else
  {
    s_pack = false;
    ctrlFlush();
  }

}

uint32_t USBDeviceClass::send(uint32_t ep, const void* data, uint32_t len)
{
  ep &= 0x0F;

  // EP4 is the only pluggable endpoint backed by the bootloader's hardware
  // table (see USB_ENDPOINTS); anything else would DMA past it.
  if (s_table == NULL || !s_lastConfigured || ep != 4)
  {
    return 0;
  }

  return s_table->usb_ep_write(data, len, (uint8_t)ep);
}

uint32_t USBDeviceClass::armSend(uint32_t ep, const void* data, uint32_t len)
{
  (void)ep;
  s_ctrlSent = true;
  return s_table->usb_ep_write(data, len, 0);
}

USBDeviceClass USBDevice;

static void configureHidEndpoint(void)
{
  UsbDeviceDescriptor* ept = (UsbDeviceDescriptor*)s_table->endpoint_table;

  USB->DEVICE.DeviceEndpoint[4].EPCFG.reg = USB_DEVICE_EPCFG_EPTYPE1(4); // interrupt IN
  ept[4].DeviceDescBank[1].PCKSIZE.bit.SIZE = 3;                         // 64 bytes
  USB->DEVICE.DeviceEndpoint[4].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_BK1RDY;
}

/* True when at least one PluggableUSB module has registered (probed with a
 * dry-run interface pass). */
static bool modulesPresent(void)
{
  uint8_t ifCount = 0;

  s_dry = true;
  s_dryTotal = 0;
  PluggableUSB().getInterface(&ifCount);
  s_dry = false;

  return s_dryTotal != 0;
}

static void sendCompositeConfigDescriptor(void)
{
  uint8_t ifCount = 2;

  s_dry = true;
  s_dryTotal = 0;
  PluggableUSB().getInterface(&ifCount);
  s_dry = false;

  uint16_t total = sizeof(ConfigDescriptor) + sizeof(IADDescriptor)
                 + sizeof(cdcInterfacesBlock) + s_dryTotal;

  s_pack = true;
  s_ctrlLen = 0;

  ConfigDescriptor cfg = D_CONFIG(total, ifCount);
  USBDevice.sendControl(&cfg, sizeof(cfg));

  IADDescriptor iad = D_IAD(0, 2, 0x02 /* CDC */, 0x02 /* ACM */, 0x00);
  USBDevice.sendControl(&iad, sizeof(iad));

  USBDevice.sendControl(cdcInterfacesBlock, sizeof(cdcInterfacesBlock));

  ifCount = 2;
  PluggableUSB().getInterface(&ifCount);

  s_pack = false;
  ctrlFlush();
}

static uint8_t ep0Hook(const uint8_t setupBytes[8])
{
  USBSetup setup;
  memcpy(&setup, setupBytes, sizeof(setup));
  s_wLength = setup.wLength;

  /* Standard device-to-host GET_DESCRIPTOR: replace the device and
   * configuration descriptors with the composite identity; everything else
   * (strings, qualifier) falls through to the bootloader. */
  if (setup.bmRequestType == 0x80 && setup.bRequest == 0x06)
  {
    if (setup.wValueH == 0x01)
    {
      DeviceDescriptor dev = D_DEVICE(0xEF, 0x02, 0x01, 64,
                                      USB_VID, BL_COMPOSITE_PID, 0x100,
                                      0, 0, 0, 1);
      s_pack = false;
      s_ctrlLen = 0;
      USBDevice.sendControl(&dev, sizeof(dev));
      return 1;
    }

    if (setup.wValueH == 0x02)
    {
      sendCompositeConfigDescriptor();
      return 1;
    }

    return 0;
  }

  /* Standard interface-directed GET_DESCRIPTOR: HID report/class
   * descriptors, answered by the plugged modules. */
  if (setup.bmRequestType == 0x81 && setup.bRequest == 0x06)
  {
    s_pack = false;
    s_ctrlLen = 0;

    if (PluggableUSB().getDescriptor(setup) != 0)
    {
      return 1;
    }

    s_table->ctrl_stall();
    return 1;
  }

  /* SET_CONFIGURATION: configure the pluggable endpoint right away (before
   * the host's first interrupt-IN poll), then fall through so the
   * bootloader configures its own endpoints and records the state. */
  if (setup.bmRequestType == 0x00 && setup.bRequest == 0x09)
  {
    configureHidEndpoint();
    return 0;
  }

  /* Class requests directed at pluggable interfaces (2+). CDC class
   * requests (interfaces 0-1) fall through to the bootloader, which keeps
   * line_coding / DTR, and with them the 1200bps touch, working. */
  if ((setup.bmRequestType & 0x7F) == 0x21 && setup.wIndex >= 2)
  {
    s_ctrlSent = false;

    if (PluggableUSB().setup(setup))
    {
      if ((setup.bmRequestType & 0x80) == 0)
      {
        s_table->ctrl_zlp();
      }
      else if (!s_ctrlSent)
      {
        // Handled, but no data was staged (e.g. HID's GET_REPORT stub):
        // stall instead of leaving the host to time out on an empty IN.
        s_table->ctrl_stall();
      }

      return 1;
    }

    s_table->ctrl_stall();
    return 1;
  }

  return 0;
}

/* Called (via weak reference) from BootloaderCDC::ensureUsable() right
 * after usb_init(). Installs the EP0 hook only when at least one
 * PluggableUSB module registered, otherwise the device keeps the
 * bootloader's plain CDC identity.
 */
extern "C" void bootloaderCDC_compositeInit(const void* table, void* cdcHandle)
{
  s_table = (const bl_cdc_export_t*)table;
  s_cdcHandle = cdcHandle;
  s_lastConfigured = 0;

  if (modulesPresent())
  {
    *s_table->ep0_hook_slot = ep0Hook;
  }

}

/* Called (via weak reference) from BootloaderCDC::pump(). Configures the
 * pluggable interrupt-IN endpoint when the host (re)configures the device;
 * a bus reset clears the endpoint and the configured state together, so
 * the rising edge re-fires after re-enumeration.
 */
extern "C" void bootloaderCDC_compositeTask(uint8_t configured)
{
  if (s_table == NULL)
  {
    return;
  }

  if (*s_table->ep0_hook_slot == NULL)
  {
    /* A global constructor touching SerialUSB can run compositeInit before
     * the HID library's constructors have plugged their modules (cross-TU
     * order is unspecified). Keep probing until the host configures the
     * device, after that the CDC-only identity is already enumerated and
     * switching would need a re-attach. */
    if (!configured && modulesPresent())
    {
      *s_table->ep0_hook_slot = ep0Hook;
    }

    s_lastConfigured = configured;
    return;
  }

  if (configured && !s_lastConfigured)
  {
    /* Normally already done by the hook's SET_CONFIGURATION path; this
     * covers a configuration observed only after the fact (idempotent). */
    configureHidEndpoint();
  }

  s_lastConfigured = configured;
}

#endif // CDC_BOOTLOADER
