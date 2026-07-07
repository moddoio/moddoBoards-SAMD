/*
  Copyright (c) 2015 Arduino LLC.  All right reserved.

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

/*
  Copyright (c) 2026 moddo inc.  All right reserved.

  Modified by moddo for the Arduino board package.
*/

#define ARDUINO_MAIN
#include "Arduino.h"
#include "sam.h"

// Weak empty variant initialization function.
// May be redefined by variant files.
void initVariant() __attribute__((weak));
void initVariant() { }

// Initialize C library
extern "C" void __libc_init_array(void);

/*
 * \brief Main entry point of Arduino application
 */
int main( void )
{
  init();

  __libc_init_array();

  initVariant();

  delay(1);
#if defined(USBCON) && !defined(USB_DISABLED) && !defined(CDC_BOOTLOADER)
  USBDevice.init();
  USBDevice.attach();
#elif defined(CDC_BOOTLOADER)
  // Bring the bootloader's USB CDC up at boot; enumeration is then pumped
  // from delay() and Serial calls (the driver is fully polled).
  bootloaderCDC_pump();
#endif

  setup();

  for (;;)
  {
    loop();
    if (serialEventRun) serialEventRun();
  }

  return 0;
}
