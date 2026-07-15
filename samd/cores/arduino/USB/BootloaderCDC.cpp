/*
  Copyright (c) 2026 moddo inc.  All right reserved.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public
  License along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#if defined(CDC_BOOTLOADER)

#include <Arduino.h>
#include "BootloaderCDC.h"
#include "bl_cdc_export.h"
#include "Reset.h"

// The application's RAM origin, provided by flash_16KB_blcdc.ld. The
// bootloader's statics must end at or below this address.
extern "C" uint32_t __data_start__;

// Composite (CDC+HID) support, BootloaderUSBComposite.cpp. Weak references
// on purpose: that file is only linked when the sketch uses the HID library
// (it satisfies the USBDevice/EndPoints symbols HID pulls in); for CDC-only
// sketches these stay NULL and cost nothing.
extern "C" void bootloaderCDC_compositeInit(const void* table, void* cdcHandle) __attribute__((weak));
extern "C" void bootloaderCDC_compositeTask(uint8_t configured) __attribute__((weak));

#define CDC_LINESTATE_DTR 0x01

static bool checkExportTable(void)
{
  const bl_cdc_export_t* t = BL_CDC_EXPORT;

  if (t->magic != BL_CDC_EXPORT_MAGIC || t->version != BL_CDC_EXPORT_VERSION)
  {
    return false;
  }

  if ((uint32_t)t->ram_end > (uint32_t)&__data_start__)
  {
    return false;
  }

  return true;
}

bool BootloaderCDC::ensureUsable(void)
{
  if (_unusable)
  {
    return false;
  }

  if (!_initialized)
  {
    if (!checkExportTable())
    {
      _unusable = true;
      return false;
    }

    _cdc = BL_CDC_EXPORT->usb_init();
    _initialized = true;

    if (bootloaderCDC_compositeInit)
    {
      bootloaderCDC_compositeInit(BL_CDC_EXPORT, _cdc);
    }

  }

  return true;
}

void BootloaderCDC::pump(void)
{
  if (!ensureUsable())
  {
    return;
  }

  const bl_cdc_export_t* t = BL_CDC_EXPORT;

  uint8_t configured = t->usb_is_configured(_cdc);

  if (bootloaderCDC_compositeTask)
  {
    bootloaderCDC_compositeTask(configured);
  }

  if (configured == 0)
  {
    return;
  }

  // Arduino auto-reset: host sets 1200bps and drops DTR before upload.
  // Edge-triggered: initiateReset() restarts its countdown on every call,
  // so re-arming continuously from this polled path would never fire.
  bool resetCondition = (t->line_coding->dwDTERate == 1200)
                     && ((*t->current_connection & CDC_LINESTATE_DTR) == 0);

  if (resetCondition && !_resetArmed)
  {
    _resetArmed = true;
    initiateReset(250);
  }
  else if (!resetCondition && _resetArmed)
  {
    _resetArmed = false;
    cancelReset();
  }

}

void BootloaderCDC::begin(unsigned long baudrate)
{
  (void)baudrate;
  ensureUsable();
}

void BootloaderCDC::begin(unsigned long baudrate, uint16_t config)
{
  (void)config;
  begin(baudrate);
}

void BootloaderCDC::end(void)
{
  // The bootloader driver has no detach entry point; keep the port up.
}

void BootloaderCDC::fillRxBuffer(void)
{
  const bl_cdc_export_t* t = BL_CDC_EXPORT;

  // Call cdc_read_buf unconditionally: the first call arms OUT reception
  // (the endpoint NAKs until then), and it returns 0 until a packet lands.
  // Gating on cdc_is_rx_ready() would deadlock, it only reports transfers
  // on an already-armed bank.
  if (_rxCount == 0)
  {
    _rxHead = 0;
    _rxCount = (uint8_t)t->cdc_read_buf(_rxBuffer, sizeof(_rxBuffer));
  }

}

int BootloaderCDC::available(void)
{
  if (!ensureUsable())
  {
    return 0;
  }

  pump();
  fillRxBuffer();
  return _rxCount;
}

int BootloaderCDC::peek(void)
{
  if (available() == 0)
  {
    return -1;
  }

  return _rxBuffer[_rxHead];
}

int BootloaderCDC::read(void)
{
  if (available() == 0)
  {
    return -1;
  }

  _rxCount--;
  return _rxBuffer[_rxHead++];
}

void BootloaderCDC::flush(void)
{
  // Writes complete synchronously in the bootloader driver.
}

size_t BootloaderCDC::write(const uint8_t* buffer, size_t size)
{
  if (!ensureUsable())
  {
    return size;
  }

  pump();

  const bl_cdc_export_t* t = BL_CDC_EXPORT;

  // Only transmit while the host has the port open, the driver blocks
  // until the host reads, so writing to a closed port would hang.
  if (t->usb_is_configured(_cdc) == 0 || (*t->current_connection & CDC_LINESTATE_DTR) == 0)
  {
    return size;
  }

  return t->cdc_write_buf(buffer, (uint32_t)size);
}

size_t BootloaderCDC::write(uint8_t c)
{
  return write(&c, 1);
}

BootloaderCDC::operator bool()
{
  if (!ensureUsable())
  {
    return false;
  }

  pump();

  const bl_cdc_export_t* t = BL_CDC_EXPORT;
  return t->usb_is_configured(_cdc) != 0 && (*t->current_connection & CDC_LINESTATE_DTR) != 0;
}

BootloaderCDC SerialUSB;

void bootloaderCDC_pump(void)
{
  SerialUSB.pump();
}

void serialEvent(void) __attribute__((weak));

/* Called by main() after every loop() iteration. Critical for this polled
 * driver: a sketch whose loop() never calls delay() or Serial (e.g. an
 * empty sketch) would otherwise attach USB at boot and then never service
 * enumeration, the host reports an unrecognized device and the post-upload
 * port never appears. Weak so a sketch can still take the hook over.
 */
__attribute__((weak)) void serialEventRun(void)
{
  SerialUSB.pump();

  if (serialEvent && SerialUSB.available())
  {
    serialEvent();
  }

}

#endif // CDC_BOOTLOADER
