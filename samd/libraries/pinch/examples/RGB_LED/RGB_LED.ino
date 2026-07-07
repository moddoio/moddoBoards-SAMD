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

// pinch RGB LED demo
// LEDs are common anode (3V3), so LOW = on, HIGH = off.
// analogWrite levels are inverted below so 0 = off, 255 = full brightness.

void setRGB(uint8_t r, uint8_t g, uint8_t b)
{
  analogWrite(PIN_LED_RED,   255 - r);
  analogWrite(PIN_LED_GREEN, 255 - g);
  analogWrite(PIN_LED_BLUE,  255 - b);
}

void fadeChannel(uint8_t r, uint8_t g, uint8_t b)
{
  for (int v = 0; v <= 255; v++)
  {
    setRGB((r * v) / 255, (g * v) / 255, (b * v) / 255);
    delay(4);
  }

  for (int v = 255; v >= 0; v--)
  {
    setRGB((r * v) / 255, (g * v) / 255, (b * v) / 255);
    delay(4);
  }
}

void setup()
{
  setRGB(0, 0, 0);
}

void loop()
{
  setRGB(255,   0,   0); delay(500); // red
  setRGB(  0, 255,   0); delay(500); // green
  setRGB(  0,   0, 255); delay(500); // blue
  setRGB(255, 255,   0); delay(500); // yellow
  setRGB(  0, 255, 255); delay(500); // cyan
  setRGB(255,   0, 255); delay(500); // magenta
  setRGB(255, 255, 255); delay(500); // white
  setRGB(  0,   0,   0); delay(500); // off

  fadeChannel(255,   0,   0); // red intensity sweep
  fadeChannel(  0, 255,   0); // green
  fadeChannel(  0,   0, 255); // blue
  fadeChannel(255, 255, 255); // white
}
