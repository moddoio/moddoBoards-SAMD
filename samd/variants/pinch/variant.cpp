/*
  Copyright (c) 2014-2015 Arduino LLC.  All right reserved.

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

#include "variant.h"

/*
 * ATSAMD11D14AM PinDescription table
 *
 * Compact map (default): NUM_PIN_DESCRIPTION_ENTRIES entries, index = Arduino
 *   D number (D0..D14 = index 0..14),
 *   followed by USB D-/D+ at 15/16. digitalWrite(N) writes pin Dn.
 *
 * Standard map (opt-in): 32 entries, index = PA pin number (0-31).
 *   NOT_A_PORT placeholders fill positions for unused/non-existent pins.
 *
 * SAMD11D 24-pin QFN peripherals (datasheet Table 6-1, only on-package pins):
 *   SERCOM (col C / col D for each pin)
 *     PA04: SERCOM0/PAD[2] | SERCOM0/PAD[0]   PA05: SERCOM0/PAD[3] | SERCOM0/PAD[1]
 *     PA06: SERCOM0/PAD[0] | SERCOM0/PAD[2]   PA07: SERCOM0/PAD[1] | SERCOM0/PAD[3]
 *     PA08: SERCOM1/PAD[2] | SERCOM0/PAD[2]   PA09: SERCOM1/PAD[3] | SERCOM0/PAD[3]
 *     PA10: SERCOM0/PAD[2] | SERCOM2/PAD[2]   PA11: SERCOM0/PAD[3] | SERCOM2/PAD[3]
 *     PA14: SERCOM0/PAD[0] | SERCOM2/PAD[0]   PA15: SERCOM0/PAD[1] | SERCOM2/PAD[1]
 *     PA16: SERCOM1/PAD[2] | SERCOM2/PAD[2]   PA17: SERCOM1/PAD[3] | SERCOM2/PAD[3]
 *     PA30: SERCOM1/PAD[0] | SERCOM1/PAD[2]   PA31: SERCOM1/PAD[1] | SERCOM1/PAD[3]
 *
 *   TCC0 (col F): PA04=WO[0], PA05=WO[1], PA06=WO[2], PA07=WO[3],
 *                 PA08=WO[4], PA09=WO[5], PA10=WO[2], PA11=WO[3],
 *                 PA14=WO[0], PA15=WO[1], PA16=WO[6], PA17=WO[7],
 *                 PA30=WO[2], PA31=WO[3]
 *                 (WO[4:7] share CC registers with WO[0:3] respectively)
 *   TC1 (col E):  PA04=WO[0], PA05=WO[1], PA14=WO[0], PA15=WO[1], PA16=WO[0], PA17=WO[1]
 *   TC2 (col E):  PA06=WO[0], PA07=WO[1], PA10=WO[0], PA11=WO[1], PA30=WO[0], PA31=WO[1]
 *
 *   ADC: PA02=AIN0, PA04=AIN2, PA05=AIN3, PA06=AIN4, PA07=AIN5,
 *        PA10=AIN8, PA11=AIN9,
 *        PA14=AIN6, PA15=AIN7 (channels routed; no sample/hold front-end)
 *
 * pinch J2 connector mapping. Hardware peripherals run on:
 *   I2C  : SERCOM2-ALT (SDA=PA14, SCL=PA15)
 *   UART1: SERCOM1-ALT (TX=PA30 pad2, RX=PA31 pad3; main mux is pads 0/1)
 *   SPI  : SERCOM0-ALT (SS=PA04 pad0, SCK=PA05 pad1, MISO=PA06 pad2,
 *          MOSI=PA07 pad3; main mux has the pad pairs swapped)
 *
 * INT / PWM columns: per datasheet Table 6-1 PORT Function Multiplexing
 * (SAMD11D 24-pin QFN). INT shows the EIC channel (or NMI). PWM "X" means
 * the pin can be configured for PWM output.
 *
 * +-------------+------+--------+-------------------+---------+--------+-----+
 * | Arduino Pin | PIN  | IC Pin | Notes             | Notes 2 |  INT   | PWM |
 * +-------------+------+--------+-------------------+---------+--------+-----+
 * | 00          | PA15 | 12     | SCL               |         | INT[1] |  X  |
 * | 01          | PA14 | 11     | SDA               |         |  NMI   |  X  |
 * | 02          | PA09 |  8     |                   |         | INT[7] |  X  |
 * | 03          | PA08 |  7     |                   |         | INT[6] |  X  |
 * | 04          | PA17 | 14     |                   |         | INT[1] |  X  |
 * | 05          | PA30 | 19     | TX1               | SWCLK   | INT[2] |  X  |
 * | 06          | PA31 | 20     | RX1               | SWDIO   | INT[3] |  X  |
 * | 07          | PA07 |  6     | A0                | MOSI    | INT[7] |  X  |
 * | 08          | PA06 |  5     | A1                | MISO    | INT[6] |  X  |
 * | 09          | PA05 |  4     | A2                | SCK     | INT[5] |  X  |
 * | 10          | PA04 |  3     | A3                | SS      | INT[4] |  X  |
 * | 11          | PA02 |  1     | A4                | DAC0    | INT[2] |     |
 * | 12          | PA16 | 13     | LED_R/LED_BUILTIN |         | INT[0] |  X  |
 * | 13          | PA11 | 10     | LED_B             |         | INT[3] |  X  |
 * | 14          | PA10 |  9     | LED_G             |         | INT[2] |  X  |
 * +-------------+------+--------+-------------------+---------+--------+-----+
 *
 * EIC channels are a shared resource: each INT[n] line can be muxed to ONE
 * pin at a time. attachInterrupt() only works on the pin that "owns" the
 * channel in the PinDescription below. Current ownership:
 *   INT[0]: PA16        INT[4]: PA04
 *   INT[1]: PA17        INT[5]: PA05
 *   INT[2]: PA02        INT[6]: PA06
 *   INT[3]: PA31        INT[7]: PA07
 *   NMI   : PA14   (requires custom NMI_Handler, attachInterrupt() is a no-op)
 * Pins that share a channel with the owner are marked EXTERNAL_INT_NONE.
 *
 * PWM channel assignments below share counters when pins use the same
 * TCC0/TC1/TC2 compare channel. The following pairs/groups output the
 * SAME waveform if PWM is enabled on both:
 *   TCC0_CH0: PA04, PA08(WO[4])
 *   TCC0_CH1: PA05, PA09(WO[5])
 *   TCC0_CH2: PA06, PA16(WO[6])
 *   TCC0_CH3: PA07, PA17(WO[7])
 *   TC1_CH0 : PA14
 *   TC1_CH1 : PA15
 *   TC2_CH0 : PA10, PA30
 *   TC2_CH1 : PA11, PA31
 */

#if defined PIN_MAP_COMPACT
const PinDescription g_APinDescription[]=
{
  // [0]  D0 : PA15 (SCL) | SERCOM2-ALT pad1 (I2C SCL), TC1/WO[1]. EIC INT[1] shared with PA17 (owner). No usable ADC on this pin, so not exposed as analog.
  { PORTA, 15, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD|PER_ATTR_SERCOM_ALT), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TC1_CH1, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [1]  D1 : PA14 (SDA) | SERCOM2-ALT pad0 (I2C SDA), TC1/WO[0]. No usable ADC on this pin, so not exposed as analog.
  { PORTA, 14, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD|PER_ATTR_SERCOM_ALT), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TC1_CH0, No_ADC_Channel, EXTERNAL_INT_NMI, GCLK_CCL_NONE },
  // [2]  D2 : PA09 | SERCOM1 pad3 / SERCOM0-ALT pad3 (both taken by Serial1/SPI), TCC0/WO[5] = CH1 alias (shares CC with PA05). EIC INT[7] shared with PA07 (owner).
  { PORTA,  9, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH1, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [3]  D3 : PA08 | SERCOM1 pad2 / SERCOM0-ALT pad2 (both taken by Serial1/SPI), TCC0/WO[4] = CH0 alias (shares CC with PA04). EIC INT[6] shared with PA06 (owner).
  { PORTA,  8, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH0, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [4]  D4 : PA17 | TCC0_CH3 (WO[7]), SERCOM1 pad3
  { PORTA, 17, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH3, No_ADC_Channel, EXTERNAL_INT_1, GCLK_CCL_NONE },
  // [5]  D5 : PA30 (TX1/SWCLK) | SERCOM1-ALT pad2 (Serial1 TX; mux C is pad0), TC2/WO[0] (shares CC with PA10). EIC INT[2] shared with PA02 (owner).
  { PORTA, 30, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD|PER_ATTR_SERCOM_ALT), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TC2_CH0, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [6]  D6 : PA31 (RX1/SWDIO) | SERCOM1-ALT pad3 (Serial1 RX; mux C is pad1), TC2/WO[1] (shares CC with PA11). EIC INT[3] owner.
  { PORTA, 31, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD|PER_ATTR_SERCOM_ALT), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TC2_CH1, No_ADC_Channel, EXTERNAL_INT_3, GCLK_CCL_NONE },
  // [7]  D7 : PA07 (A0/MOSI) | AIN5, TCC0_CH3, SERCOM0-ALT pad3 (SPI MOSI; mux C is pad1)
  { PORTA,  7, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_ALT), (PIN_ATTR_ADC|PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH3, ADC_Channel5, EXTERNAL_INT_7, GCLK_CCL_NONE },
  // [8]  D8 : PA06 (A1/MISO) | AIN4, TCC0_CH2, SERCOM0-ALT pad2 (SPI MISO; mux C is pad0)
  { PORTA,  6, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_ALT), (PIN_ATTR_ADC|PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH2, ADC_Channel4, EXTERNAL_INT_6, GCLK_CCL_NONE },
  // [9]  D9 : PA05 (A2/SCK) | AIN3, TCC0_CH1, SERCOM0-ALT pad1 (SPI SCK; mux C is pad3)
  { PORTA,  5, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_ALT), (PIN_ATTR_ADC|PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH1, ADC_Channel3, EXTERNAL_INT_5, GCLK_CCL_NONE },
  // [10] D10: PA04 (A3/SS) | AIN2, TCC0_CH0, SERCOM0-ALT pad0 (SPI SS; mux C is pad2), VREFB
  { PORTA,  4, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_ALT), (PIN_ATTR_ADC|PIN_ATTR_REF|PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH0, ADC_Channel2, EXTERNAL_INT_4, GCLK_CCL_NONE },
  // [11] D11: PA02 (A4/DAC0) | AIN0, DAC output
  { PORTA,  2, PIO_MULTI, PER_ATTR_DRIVE_STRONG, (PIN_ATTR_ADC|PIN_ATTR_DAC|PIN_ATTR_DIGITAL|PIN_ATTR_EXTINT), NOT_ON_TIMER, ADC_Channel0, EXTERNAL_INT_2, GCLK_CCL_NONE },
  // [12] D12: PA16 (LED_R/LED_BUILTIN) | active low, TCC0_CH2 (WO[6]), SERCOM1 pad2
  { PORTA, 16, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH2, No_ADC_Channel, EXTERNAL_INT_0, GCLK_CCL_NONE },
  // [13] D13: PA11 (LED_B) | active low, TC2/WO[1] (shares CC with PA31). EIC INT[3] shared with PA31 (owner).
  { PORTA, 11, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_EXTINT), TC2_CH1, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [14] D14: PA10 (LED_G) | active low, TC2/WO[0] (shares CC with PA30). EIC INT[2] shared with PA02 (owner).
  { PORTA, 10, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_EXTINT), TC2_CH0, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [15] PA24: USB D-
  { PORTA, 24, PIO_MULTI, PER_ATTR_DRIVE_STRONG, (PIN_ATTR_DIGITAL|PIN_ATTR_COM), NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [16] PA25: USB D+
  { PORTA, 25, PIO_MULTI, PER_ATTR_DRIVE_STRONG, (PIN_ATTR_DIGITAL|PIN_ATTR_COM), NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
} ;
#elif defined PIN_MAP_STANDARD
const PinDescription g_APinDescription[]=
{
  // [0]  PA00: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [1]  PA01: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [2]  PA02: D11/A4/DAC0 | AIN0, DAC output
  { PORTA,  2, PIO_MULTI, PER_ATTR_DRIVE_STRONG, (PIN_ATTR_ADC|PIN_ATTR_DAC|PIN_ATTR_DIGITAL|PIN_ATTR_EXTINT), NOT_ON_TIMER, ADC_Channel0, EXTERNAL_INT_2, GCLK_CCL_NONE },
  // [3]  PA03: off-connector | AIN1, VREFA (silicon only)
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [4]  PA04: D10/A3/SS | AIN2, TCC0_CH0, SERCOM0-ALT pad0 (SPI SS; mux C is pad2), VREFB
  { PORTA,  4, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_ALT), (PIN_ATTR_ADC|PIN_ATTR_REF|PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH0, ADC_Channel2, EXTERNAL_INT_4, GCLK_CCL_NONE },
  // [5]  PA05: D9/A2/SCK | AIN3, TCC0_CH1, SERCOM0-ALT pad1 (SPI SCK; mux C is pad3)
  { PORTA,  5, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_ALT), (PIN_ATTR_ADC|PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH1, ADC_Channel3, EXTERNAL_INT_5, GCLK_CCL_NONE },
  // [6]  PA06: D8/A1/MISO | AIN4, TCC0_CH2, SERCOM0-ALT pad2 (SPI MISO; mux C is pad0)
  { PORTA,  6, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_ALT), (PIN_ATTR_ADC|PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH2, ADC_Channel4, EXTERNAL_INT_6, GCLK_CCL_NONE },
  // [7]  PA07: D7/A0/MOSI | AIN5, TCC0_CH3, SERCOM0-ALT pad3 (SPI MOSI; mux C is pad1)
  { PORTA,  7, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_ALT), (PIN_ATTR_ADC|PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH3, ADC_Channel5, EXTERNAL_INT_7, GCLK_CCL_NONE },
  // [8]  PA08: D3 | SERCOM1 pad2 / SERCOM0-ALT pad2 (both taken by Serial1/SPI), TCC0/WO[4] = CH0 alias (shares CC with PA04). EIC INT[6] shared with PA06 (owner).
  { PORTA,  8, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH0, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [9]  PA09: D2 | SERCOM1 pad3 / SERCOM0-ALT pad3 (both taken by Serial1/SPI), TCC0/WO[5] = CH1 alias (shares CC with PA05). EIC INT[7] shared with PA07 (owner).
  { PORTA,  9, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH1, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [10] PA10: LED_G (D14) | active low, TC2/WO[0] (shares CC with PA30). EIC INT[2] shared with PA02 (owner).
  { PORTA, 10, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_EXTINT), TC2_CH0, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [11] PA11: LED_B (D13) | active low, TC2/WO[1] (shares CC with PA31). EIC INT[3] shared with PA31 (owner).
  { PORTA, 11, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_EXTINT), TC2_CH1, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [12] PA12: unused (SAMD11D14A does not have PA12/PA13)
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [13] PA13: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [14] PA14: D1/SDA | SERCOM2-ALT pad0 (I2C SDA), TC1/WO[0]. No usable ADC on this pin, so not exposed as analog.
  { PORTA, 14, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD|PER_ATTR_SERCOM_ALT), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TC1_CH0, No_ADC_Channel, EXTERNAL_INT_NMI, GCLK_CCL_NONE },
  // [15] PA15: D0/SCL | SERCOM2-ALT pad1 (I2C SCL), TC1/WO[1]. EIC INT[1] shared with PA17 (owner). No usable ADC on this pin, so not exposed as analog.
  { PORTA, 15, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD|PER_ATTR_SERCOM_ALT), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TC1_CH1, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [16] PA16: LED_R / LED_BUILTIN (D12) | active low, TCC0_CH2 (WO[6]), SERCOM1 pad2
  { PORTA, 16, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH2, No_ADC_Channel, EXTERNAL_INT_0, GCLK_CCL_NONE },
  // [17] PA17: D4 | TCC0_CH3 (WO[7]), SERCOM1 pad3
  { PORTA, 17, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_ALT|PER_ATTR_SERCOM_STD), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TCC0_CH3, No_ADC_Channel, EXTERNAL_INT_1, GCLK_CCL_NONE },
  // [18] PA18: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [19] PA19: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [20] PA20: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [21] PA21: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [22] PA22: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [23] PA23: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [24] PA24: USB D-
  { PORTA, 24, PIO_MULTI, PER_ATTR_DRIVE_STRONG, (PIN_ATTR_DIGITAL|PIN_ATTR_COM), NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [25] PA25: USB D+
  { PORTA, 25, PIO_MULTI, PER_ATTR_DRIVE_STRONG, (PIN_ATTR_DIGITAL|PIN_ATTR_COM), NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [26] PA26: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [27] PA27: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [28] PA28: unused (SAMD11D14A does not expose PA28)
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [29] PA29: unused
  { NOT_A_PORT,  0, PIO_NOT_A_PIN, PER_ATTR_NONE, PIN_ATTR_NONE, NOT_ON_TIMER, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [30] PA30: D5/TX1/SWCLK | SERCOM1-ALT pad2 (Serial1 TX; mux C is pad0), TC2/WO[0] (shares CC with PA10). EIC INT[2] shared with PA02 (owner).
  { PORTA, 30, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD|PER_ATTR_SERCOM_ALT), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TC2_CH0, No_ADC_Channel, EXTERNAL_INT_NONE, GCLK_CCL_NONE },
  // [31] PA31: D6/RX1/SWDIO | SERCOM1-ALT pad3 (Serial1 RX; mux C is pad1), TC2/WO[1] (shares CC with PA11). EIC INT[3] owner.
  { PORTA, 31, PIO_MULTI, (PER_ATTR_DRIVE_STRONG|PER_ATTR_TIMER_STD|PER_ATTR_SERCOM_ALT), (PIN_ATTR_DIGITAL|PIN_ATTR_TIMER|PIN_ATTR_SERCOM|PIN_ATTR_EXTINT), TC2_CH1, No_ADC_Channel, EXTERNAL_INT_3, GCLK_CCL_NONE },
} ;
#endif

// SAMD11D14A: TCC_INST_NUM=1 (TCC0), TC_INST_NUM=2 (TC1, TC2)
const void* g_apTCInstances[TCC_INST_NUM+TC_INST_NUM]={ TCC0, TC1, TC2 } ;

// Multi-serial objects instantiation (SAMD11D14A has SERCOM0, SERCOM1, SERCOM2)
SERCOM sercom0( SERCOM0 ) ;
SERCOM sercom1( SERCOM1 ) ;
SERCOM sercom2( SERCOM2 ) ;

// Serial1: SERCOM1-ALT: TX=PA30(pad2), RX=PA31(pad3)
#if defined(ONE_UART) || defined(TWO_UART)
Uart Serial1( SERCOM_INSTANCE_SERIAL1, PIN_SERIAL1_RX, PIN_SERIAL1_TX, PAD_SERIAL1_RX, PAD_SERIAL1_TX ) ;

void SERCOM1_Handler()
{
  Serial1.IrqHandler();
}
#endif
