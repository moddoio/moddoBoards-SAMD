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

#ifndef _BL_CDC_EXPORT_H_
#define _BL_CDC_EXPORT_H_

#include <stdint.h>
#include <stdbool.h>

/* Bootloader USB export table, ABI contract with the application.
 *
 * The table is a const struct placed at the FIXED flash address
 * BL_CDC_EXPORT_ADDR (last 64 bytes of the 4KB bootloader region).
 * Applications locate it by address, validate magic and version, and call
 * the bootloader's polled USB driver through it instead of carrying their
 * own USB stack. Version 2 additionally lets the application extend the
 * device with its own interfaces (e.g. HID) through an EP0 hook.
 *
 * THIS HEADER EXISTS IN TWO REPOSITORIES AND THE COPIES MUST STAY
 * BYTE-IDENTICAL, together they ARE the ABI:
 *   - ArduinoBootloader: bootloaders/sam-ba/inc/bl_cdc_export.h   (master)
 *   - ArduinoBoards:     samd/cores/arduino/USB/bl_cdc_export.h   (copy)
 * Edit the bootloader copy, then copy the file verbatim to the BSP.
 *
 * Bootloader and application only work together when their table versions
 * match. An application built with CDC_BOOTLOADER validates magic, version,
 * and ram_end at runtime; on a board whose bootloader predates this table
 * (or has an incompatible version) the USB port simply stays inert, no
 * enumeration, writes discarded, available() == 0. The fix is flashing the
 * matching bootloader (also shipped in the BSP at samd/bootloaders/pinch/
 * for Burn Bootloader), not debugging the sketch.
 *
 * ABI rules, breaking any of these requires bumping BL_CDC_EXPORT_VERSION,
 * rebuilding/reflashing the bootloader, and re-shipping its binary in the
 * BSP:
 *   - The table address, field order, and field meaning are frozen.
 *   - New fields may only be appended (minor additions may keep the version
 *     if all existing offsets are unchanged and ram_end still covers the
 *     bootloader's statics).
 *
 * Version history:
 *   1: CDC only (usb_init, is_configured, putc/getc/rx_ready, write/read,
 *       line_coding, current_connection, ram_end).
 *   2: Dropped putc/getc/is_rx_ready (redundant or misleading; read_buf
 *       and write_buf cover everything). Added the EP0 hook slot, generic
 *       endpoint write, control-transfer primitives, and the endpoint
 *       descriptor table pointer so applications can add interfaces (HID).
 *       Endpoint table grown to 5 entries (EP4 free for the application).
 *       NOTE: .bl_shared_ram now fills the applications' 512-byte RAM
 *       reservation exactly (ram_end == 0x20000200); any further shared
 *       state requires growing the reservation in the BSP linker script
 *       (flash_16KB_blcdc.ld) in the same change.
 *
 * Runtime contract for the application:
 *   - GCLK0 must run at 48MHz before usb_init() is called (the driver
 *     routes GCLK0 to the USB peripheral itself).
 *   - The driver's statics (including the endpoint descriptor table that
 *     USB hardware DMAs from) live in the dedicated .bl_shared_ram section
 *     occupying exactly [0x20000000, ram_end). The application must not
 *     place data there; it must verify at runtime that its own image
 *     respects ram_end before enabling the driver.
 *   - The driver is fully polled: usb_is_configured() services EP0
 *     enumeration and must be called often.
 *   - cdc_write_buf BLOCKS until the host reads the data (bus reset
 *     escapes); call only when configured and DTR is up.
 *   - cdc_read_buf is non-blocking, <=64 bytes per call, and must be called
 *     unconditionally: the FIRST call arms OUT reception and returns 0
 *     until a packet lands.
 *   - Calls may spike ~300 bytes of caller stack during enumeration
 *     (string descriptor buffer).
 *   - Without an EP0 hook the device enumerates with the BOOTLOADER's
 *     VID/PID and strings.
 *   - Auto-reset: the host's SET_LINE_CODING / SET_CONTROL_LINE_STATE are
 *     visible through line_coding / current_connection; the application is
 *     responsible for watching for the 1200bps touch and resetting itself.
 *
 * EP0 hook contract (version 2):
 *   - Register by writing a function pointer through ep0_hook_slot AFTER
 *     usb_init() (which resets the slot to NULL; NULL = no hook, plain
 *     bootloader CDC behavior).
 *   - The hook runs inside the enumeration dispatch, in whatever context
 *     is polling the driver. It receives a stack copy of the 8 SETUP
 *     bytes (the live bank may be overwritten by a data stage).
 *   - Return nonzero after fully handling a request, data stage via
 *     usb_ep_write(..., 0) / ctrl_read(), then status stage via ctrl_zlp()
 *     (for OUT requests) or letting the host's status-OUT complete (for IN
 *     requests). Return zero to fall through to the built-in handler
 *     (standard requests, CDC class requests, SET_CONFIGURATION, ...).
 *   - Overriding GET_DESCRIPTOR(device/configuration) from the hook is how
 *     an application presents a composite (e.g. CDC+HID) identity; use a
 *     DIFFERENT PID than the bootloader's, or hosts will reuse cached
 *     descriptors.
 *   - The application may claim endpoint 4: configure it directly via USB
 *     registers plus endpoint_table (entries 0..4), typically once
 *     usb_is_configured() first reports nonzero.
 */

#define BL_CDC_EXPORT_ADDR      (0x00000FC0ul)
#define BL_CDC_EXPORT_MAGIC     (0x63646350ul)  /* "Pcdc" (little-endian) */
#define BL_CDC_EXPORT_VERSION   (2u)

/* Mirrors the CDC line coding written by the host (usb_cdc_line_coding_t). */
typedef struct
{
  uint32_t dwDTERate;
  uint8_t bCharFormat;
  uint8_t bParityType;
  uint8_t bDataBits;
} bl_cdc_line_coding_t;

/* EP0 hook: receives the 8 SETUP bytes, returns nonzero if handled. */
typedef uint8_t (*bl_cdc_ep0_hook_t)(const uint8_t setup[8]);

/* OR into usb_ep_write's ep_num for a control IN response whose length
 * equals the request's wLength AND is a multiple of 64: the host ends the
 * transfer at wLength, so the driver must not queue a trailing auto-ZLP
 * (it would never be drained and the write would spin until bus reset). */
#define BL_EP_WRITE_EXACT       (0x80u)

typedef struct
{
  uint32_t magic;                                 /* BL_CDC_EXPORT_MAGIC */
  uint16_t version;                               /* BL_CDC_EXPORT_VERSION */
  uint16_t reserved;
  void* (*usb_init)(void);                        /* init USB hw, returns CDC handle */
  uint8_t (*usb_is_configured)(void* cdc);        /* pumps EP0; !=0 once host configured */
  uint32_t (*cdc_write_buf)(const void* data, uint32_t length);  /* blocking TX */
  uint32_t (*cdc_read_buf)(void* data, uint32_t length);         /* non-blocking RX */
  volatile bl_cdc_line_coding_t* line_coding;     /* last SET_LINE_CODING from host */
  volatile uint8_t* current_connection;           /* last SET_CONTROL_LINE_STATE (bit0=DTR) */
  const void* ram_end;                            /* end of bootloader statics in SRAM */
  volatile bl_cdc_ep0_hook_t* ep0_hook_slot;      /* write your EP0 hook here (v2) */
  uint32_t (*usb_ep_write)(const void* data, uint32_t length, uint8_t ep_num); /* IN transfer on any endpoint; ep 0 = control data stage (v2) */
  uint32_t (*ctrl_read)(void* dst, uint32_t length);  /* control OUT data stage (v2) */
  void (*ctrl_zlp)(void);                         /* control status stage (v2) */
  void (*ctrl_stall)(void);                       /* stall control IN (v2) */
  void* endpoint_table;                           /* UsbDeviceDescriptor[5] hw table (v2) */
} bl_cdc_export_t;

#define BL_CDC_EXPORT ((const bl_cdc_export_t*)BL_CDC_EXPORT_ADDR)

#endif // _BL_CDC_EXPORT_H_
