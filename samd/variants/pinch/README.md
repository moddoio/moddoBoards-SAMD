# pinch

A tiny Arduino-compatible board built around the **Microchip ATSAMD11D14A**
(Cortex-M0+ at 48 MHz, **16 KB flash / 4 KB SRAM**), with native USB-C, an
on-board RGB LED, a reset button, and a fine-pitch breakout header.

- **MCU:** ATSAMD11D14A-MUT (24-pin QFN)
- **Logic level:** 3.3 V (on-board LDO)
- **USB:** USB-C, native full-speed (CDC serial plus optional HID)
- **GPIO:** 15 in the Arduino pin map (D0 to D14). 12 are on the header
  (D0 to D11) and 3 drive the RGB LED (D12 to D14).
- **On board:** RGB LED (common-anode, active-low), reset button
- **Header:** 16-pin, 1.27 mm pitch (J2)
- **Debug:** SWD available on D5 (SWCLK) and D6 (SWDIO)

## Pinout

| Arduino | MCU pin | Analog | PWM | Peripheral |
|---------|---------|--------|-----|------------|
| D0  | PA15 |    | ✓ | I2C SCL |
| D1  | PA14 |    | ✓ | I2C SDA |
| D2  | PA09 |    | ✓ | |
| D3  | PA08 |    | ✓ | |
| D4  | PA17 |    | ✓ | |
| D5  | PA30 |    | ✓ | Serial1 TX, SWCLK |
| D6  | PA31 |    | ✓ | Serial1 RX, SWDIO |
| D7  | PA07 | A0 |   | SPI MOSI |
| D8  | PA06 | A1 | ✓ | SPI MISO |
| D9  | PA05 | A2 |   | SPI SCK |
| D10 | PA04 | A3 |   | SPI SS |
| D11 | PA02 | A4 |   | DAC0 |
| D12 | PA16 |    | ✓ | **LED_R / LED_BUILTIN** |
| D13 | PA11 |    | ✓ | **LED_B** |
| D14 | PA10 |    | ✓ | **LED_G** |

**PWM** — ✓ marks pins usable with `analogWrite()`; the carrier frequency is set
by **Tools → Timer PWM Frequency** (default 732 Hz). D11 is the DAC:
`analogWrite(A4)` there outputs a true analog voltage, not PWM.

### J2 header (1.27 mm)

```
        1  D0 / SCL               2  3V3
        3  D1 / SDA               4  D7 / A0 / MOSI
        5  D2                     6  D8 / A1 / MISO
        7  D3                     8  D9 / A2 / SCK
        9  D4                    10  D10 / A3 / SS
       11  RST                   12  VUSB
       13  D5 / TX1 / SWCLK      14  D11 / DAC0 / A4
       15  D6 / RX1 / SWDIO      16  GND
```

## Board options (Tools menu)

With pinch selected, the **Tools** menu exposes these submenus. The default for
each is marked below. Most sketches can leave every option at its default.

### USB Config

| Option | What you get | Empty-sketch flash |
|--------|--------------|--------------------|
| **USB (CDC & HID)** *(default)* | `Serial` over USB, plus HID (Keyboard, Mouse, and similar) when a sketch uses the HID libraries. Uses the bootloader's USB driver, so almost no USB code is in your sketch. | ~29 % |
| **USB disabled** | No USB. `Serial` falls back to **Serial1** (hardware UART on D5/D6). For lowest power and microamp sleep. | ~25 % |

### Clock Source

pinch has no external crystal; the 48 MHz system clock comes from the MCU's
internal oscillator.

| Option | What it does |
|--------|--------------|
| **Internal Oscillator** *(default)* | Runs the 48 MHz clock open-loop from the factory calibration. Works with or without USB and needs nothing external. |
| **Internal USB Calibrated Oscillator** | While connected to a USB host, continuously trims the 48 MHz clock against USB frame timing for better accuracy, then falls back to open-loop when unplugged. Prefer this when USB timing or an accurate serial baud rate matters. |

### Timer PWM Frequency

Sets the output frequency of `analogWrite()` (PWM). The default is
**732.4 Hz (16-bit)**, a good general-purpose choice.

- **16-bit options (732.4 Hz down to 30.52 Hz):** finer duty-cycle resolution,
  but the lower frequencies can flicker visibly on LEDs.
- **8-bit options (187500 Hz down to 1465 Hz):** coarser resolution, but fast
  enough to be flicker-free on LEDs and quiet on motors.

Raise the frequency for smooth LED dimming or quiet motor drive; lower it only
if you need very fine steps.

### Floating Point

pinch has no hardware floating-point unit, so float math runs in software. This
option controls how `print()` and `String` format floating-point values.

| Option | Trade-off |
|--------|-----------|
| **Print & String use auto-promoted doubles only** *(default)* | Smallest flash. Everything formats through the double-precision routines. Best fit for this 16 KB chip. |
| **Print uses separate singles and doubles** | Adds single-precision `print()` formatting (faster for `float`) at some flash cost. |
| **String uses separate singles and doubles** | The same, for `String`. |
| **Print & String use separate singles and doubles** | Both of the above. Largest flash. |

Leave this at the default unless you specifically need single-precision float
formatting.

## Getting started

Blink the built-in (red) LED. It is **active-low**, so `LOW` turns it on:

```cpp
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}
void loop() {
  digitalWrite(LED_BUILTIN, LOW);   // on
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);  // off
  delay(500);
}
```

For full-color output, see **File → Examples → pinch → RGB_LED**.

## Peripherals

- **Serial (USB):** `Serial` (also `SerialUSB`), the USB virtual COM port.
- **Serial1 (UART):** TX on D5, RX on D6, 3.3 V levels.
- **Wire (I2C):** SDA on D1, SCL on D0.
- **SPI:** MOSI D7, MISO D8, SCK D9, SS D10.
- **Analog in:** `analogRead(A0..A4)` on D7 to D11. D0/D1 are not
  analog-capable (no usable ADC); use them as digital I/O or I2C.
- **DAC:** `analogWrite(DAC0)` (A4 / D11).
- **PWM:** `analogWrite()` on the pins marked **PWM** in the pinout above,
  including the RGB LEDs.
- **HID:** the stock `Keyboard` and `Mouse` libraries work in the
  "USB (CDC & HID)" configuration.

## Uploading

pinch uses native USB, so no external programmer is needed.

- **Normal upload:** the IDE resets the board into the bootloader automatically
  (1200 bps touch) and flashes over USB.
- **Manual bootloader entry:** double-tap the RST button. The board stays in the
  bootloader (the red LED indicates bootloader mode) until you upload.
- Use manual entry if a sketch has disabled USB, hangs, or never services USB.

**Tools → Burn Bootloader** reinstalls the moddo bootloader over SWD (needs a
debug probe on D5/D6).

## Constraints

This is a small MCU. Keep these in mind:

- About **12 KB** of flash for your sketch (the bootloader occupies the first
  4 KB).
- **4 KB** of SRAM total.
- Analog inputs are **A0 through A4** (D7 to D11). D0/D1 have no usable ADC.
- In "USB (CDC & HID)" mode, at most **one** HID interface. A Keyboard and Mouse
  combo is fine because they share one interface, but not, say, HID plus a
  separate MIDI class.

## License

LGPL-2.1-or-later. See the [LICENSE](../../LICENSE) in the platform root.
