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

// pinch DAC demo. Outputs a triangle wave on DAC0 (D11 / A4 / PA02).
// Watch D11 on an oscilloscope to see the waveform, or measure it against
// GND with a multimeter to see the voltage ramp up and down between 0 and
// about 3.3 V.
//
// analogWrite() drives the DAC at 8-bit resolution by default, so the value
// ranges 0..255. The DAC output is high impedance; do not drive a load
// directly from it.

void setup()
{
  // No setup needed. analogWrite(DAC0, ...) enables the DAC on first use.
}

void loop()
{
  // Ramp up from 0 to full scale.
  for (int v = 0; v <= 255; v++)
  {
    analogWrite(DAC0, v);
    delay(2);
  }

  // Ramp back down to 0.
  for (int v = 255; v >= 0; v--)
  {
    analogWrite(DAC0, v);
    delay(2);
  }

}
