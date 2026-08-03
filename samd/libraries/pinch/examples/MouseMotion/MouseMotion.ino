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

// Periodically reports minimal mouse motion to the connected computer.
// Select "USB (CDC & HID)" under Tools > USB Config.

#include <Mouse.h>

constexpr uint32_t MOTION_INTERVAL_MS = 500;
constexpr int8_t MOTION_RADIUS_PIXELS = 25;

int8_t currentX = 0;
int8_t currentY = 0;

void setup()
{
  Mouse.begin();

  // Give the computer time to finish recognizing the USB pointer.
  delay(500);

  randomSeed(analogRead(A0) ^ micros());
}

void loop()
{
  // Choose a random point within a 50 x 50 pixel area around the start.
  const int8_t nextX = static_cast<int8_t>(random(-MOTION_RADIUS_PIXELS,
                                                   MOTION_RADIUS_PIXELS + 1));
  const int8_t nextY = static_cast<int8_t>(random(-MOTION_RADIUS_PIXELS,
                                                   MOTION_RADIUS_PIXELS + 1));

  Mouse.move(nextX - currentX, nextY - currentY);
  currentX = nextX;
  currentY = nextY;

  delay(MOTION_INTERVAL_MS);
}
