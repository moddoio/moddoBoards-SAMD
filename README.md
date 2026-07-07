# moddo SAMD Boards

Arduino core (board support package) for moddo's SAMD-based boards.

This package adds moddo boards to the Arduino IDE and CLI. Install it, pick your
board, and upload sketches over native USB with no external programmer.

## Supported boards

| Board | MCU | Flash / RAM | Highlights | Docs |
|-------|-----|-------------|------------|------|
| **pinch** | ATSAMD11D14A (Cortex-M0+, 48 MHz) | 16 KB / 4 KB | USB-C, RGB LED, reset button, 12 GPIO on a 1.27 mm header | [samd/variants/pinch/README.md](samd/variants/pinch/README.md) |

More SAMD boards will be added as additional rows. Each shares this core and
gets its own doc under `samd/variants/<board>/`.

## Installation

1. In the Arduino IDE, open **File → Preferences**.
2. Add this URL to **Additional Boards Manager URLs**:
   ```
   https://raw.githubusercontent.com/moddoio/moddoBoards-SAMD/main/package_moddo_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search for **moddo SAMD Boards**, and click **Install**.
4. Select your board under **Tools → Board → moddo SAMD Boards**.

The toolchain (arm-none-eabi-gcc), `bossac`, and CMSIS are pulled in
automatically from the Arduino package index, so there is nothing else to
download.

## Bootloader

moddo boards ship with a moddo USB bootloader (a SAM-BA variant) that provides
driverless USB uploads and, on some boards, exports its USB stack to the
application to save flash. The compiled bootloader binary is included in this
package and can be (re)flashed with **Tools → Burn Bootloader**.

Board-specific bootloader details (entering bootloader mode, USB identity) are
documented in each board's README.

## Repository layout

```
package_moddo_index.json    Boards Manager index (points at the release archive)
samd/
  boards.txt                Board definitions (one block per board)
  platform.txt              Compiler and tool recipes (shared by all boards)
  cores/arduino/            The Arduino core, shared by every board
  variants/<board>/         Per-board pin map and README
  libraries/                Bundled libraries and examples
  bootloaders/<board>/      Bootloader binary for Burn Bootloader
  LICENSE                   LGPL-2.1
```

## License

Licensed under the **GNU Lesser General Public License v2.1 or later**
(LGPL-2.1-or-later). See [LICENSE](LICENSE). Sketches you compile against this
core are **not** covered by the LGPL; only modifications to the core itself are.

## Credits

Derived from the [Arduino SAMD core](https://github.com/arduino/ArduinoCore-samd)
(Arduino LLC) and the [MattairTech ArduinoCore](https://github.com/mattairtech/ArduinoCore-samd),
both LGPL-2.1. Third-party components retain their original licenses.

## Links

- moddo: <https://moddo.io>
