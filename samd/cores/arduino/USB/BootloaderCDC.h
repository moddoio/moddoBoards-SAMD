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

#ifndef _BOOTLOADER_CDC_H_
#define _BOOTLOADER_CDC_H_

#if defined(CDC_BOOTLOADER)

#include "Stream.h"

/* SerialUSB backed by the bootloader's polled USB CDC driver, reached
 * through the fixed-address export table (bl_cdc_export.h). No USB stack is
 * compiled into the application.
 *
 * REQUIRES A MATCHING BOOTLOADER: the board must run a bootloader that
 * carries the bl_cdc_export table with the same version this core was built
 * against (Burn Bootloader flashes the copy shipped in
 * samd/bootloaders/pinch/). On an older/incompatible bootloader the port
 * stays inert (no enumeration, writes discarded) BY DESIGN, flash the
 * matching bootloader instead of debugging the sketch. See bl_cdc_export.h
 * for the full ABI contract, including the rule that its two copies (one
 * per repo) must stay byte-identical.
 *
 * Differences from the stock CDC SerialUSB:
 *   - The port enumerates on the first begin() / delay() call, not at boot.
 *   - The device presents the BOOTLOADER's VID/PID and strings.
 *   - The driver is polled: enumeration and the 1200bps-touch watch advance
 *     inside Serial calls, delay(), and after every loop() iteration (via
 *     serialEventRun). Only a sketch that blocks inside loop() forever
 *     without calling delay()/Serial goes unserviced, such a sketch needs
 *     a manual (double-tap) reset to be reflashed.
 *   - write() blocks while the host has the port open but stops reading.
 *   - If the export table is missing/incompatible (magic/version/ram_end
 *     mismatch), the port stays inert: writes are discarded, available()
 *     is 0, operator bool() is false.
 */
class BootloaderCDC : public Stream
{
public:
  void begin(unsigned long baudrate);
  void begin(unsigned long baudrate, uint16_t config);
  void end(void);

  virtual int available(void);
  virtual int peek(void);
  virtual int read(void);
  virtual void flush(void);
  virtual size_t write(uint8_t c);
  virtual size_t write(const uint8_t* buffer, size_t size);
  using Print::write;

  operator bool();

  // Services enumeration and the auto-reset watch; called from delay().
  void pump(void);

private:
  bool ensureUsable(void);
  void fillRxBuffer(void);

  bool _initialized = false;
  bool _unusable = false;
  bool _resetArmed = false;
  void* _cdc = nullptr;

  uint8_t _rxBuffer[64];
  uint8_t _rxHead = 0;
  uint8_t _rxCount = 0;
};

extern BootloaderCDC SerialUSB;

extern "C" void bootloaderCDC_pump(void);

#endif // CDC_BOOTLOADER

#endif // _BOOTLOADER_CDC_H_
