/*
  Copyright (c) 2026 moddo inc. All rights reserved.

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

// Opens the pinch webpage in the default browser when the board is plugged in.
// Select "USB (CDC & HID)" under Tools > USB Config.

#include <Keyboard.h>

#define KEYBOARD_LAUNCHER_WINDOWS 1
#define KEYBOARD_LAUNCHER_LINUX   2
#define KEYBOARD_LAUNCHER_MACOS   3

// Change this to KEYBOARD_LAUNCHER_LINUX or KEYBOARD_LAUNCHER_MACOS
// to target a different operating system.
// It can also be supplied as a compiler definition, for example:
//   -DKEYBOARD_LAUNCHER_OS=KEYBOARD_LAUNCHER_LINUX
#ifndef KEYBOARD_LAUNCHER_OS
#define KEYBOARD_LAUNCHER_OS KEYBOARD_LAUNCHER_WINDOWS
#endif

constexpr char PINCH_URL[] = "https://moddo.io/pages/pinch";

void openOnWindows()
{
  // Open the Run dialog with Windows+R.
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();

  // A URL entered in Run opens in the default browser.
  delay(500);
  Keyboard.print(PINCH_URL);
  Keyboard.write(KEY_RETURN);
}

void openOnLinux()
{
  // Open a terminal using the common Linux desktop shortcut Ctrl+Alt+T.
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.press('t');
  delay(100);
  Keyboard.releaseAll();

  // Use the desktop's default application for the URL.
  delay(1000);
  Keyboard.print("xdg-open ");
  Keyboard.print(PINCH_URL);
  Keyboard.write(KEY_RETURN);
}

void openOnMacOS()
{
  // Open Spotlight with Command+Space, then launch Terminal.
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press(' ');
  delay(100);
  Keyboard.releaseAll();

  delay(500);
  Keyboard.print("Terminal");
  Keyboard.write(KEY_RETURN);

  // macOS's open command launches the URL in the default browser.
  delay(1000);
  Keyboard.print("open ");
  Keyboard.print(PINCH_URL);
  Keyboard.write(KEY_RETURN);
}

void setup()
{
  Keyboard.begin();

  // Give the computer time to finish recognizing the USB keyboard.
  delay(500);

#if KEYBOARD_LAUNCHER_OS == KEYBOARD_LAUNCHER_WINDOWS
  openOnWindows();
#elif KEYBOARD_LAUNCHER_OS == KEYBOARD_LAUNCHER_LINUX
  openOnLinux();
#elif KEYBOARD_LAUNCHER_OS == KEYBOARD_LAUNCHER_MACOS
  openOnMacOS();
#else
#error "Unsupported KEYBOARD_LAUNCHER_OS value"
#endif

  Keyboard.end();
}

void loop()
{
  // Launch only once per power-up.
}
