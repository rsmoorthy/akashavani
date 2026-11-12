# Pin map

!!! warning "Bench wiring applied"
    The receiver `config.h` now matches the observed ESP32-D0WD-V3 bench wiring
    below. The production target may still be an ESP32-S3 carrier; verify against
    the assembled board before applying power and change the config header and
    this table together.

## Receiver

| GPIO | Function | Config constant | Notes |
| ---: | --- | --- | --- |
| 18 | SPI SCK | `kSpiSckPin` | VSPI, shared bus |
| 19 | SPI MISO | `kSpiMisoPin` | VSPI, shared bus |
| 23 | SPI MOSI | `kSpiMosiPin` | VSPI, shared bus |
| 5 | RFM69 CS | `kRadioCsPin` | active low |
| 13 | RFM69 DIO0 | `kRadioDio0Pin` | interrupt input |
| 17 | RFM69 RESET | `kRadioResetPin` | active high; leave low to run. USB1 has pin 9 unconnected |
| 16 | micro-SD CS | `kSdCsPin` | active low |
| 21 | I²C SDA | `kI2cSdaPin` | DS3231/BME280/OLED; external pull-up |
| 22 | I²C SCL | `kI2cSclPin` | DS3231/BME280/OLED; external pull-up |
| 8 | Display CS | `kDisplayCsPin` | unused ST7735 seam |
| 7 | Display DC | `kDisplayDcPin` | unused ST7735 seam |
| 6 | Display RST | `kDisplayResetPin` | unused ST7735 seam |
| 2 | Status LED | `kStatusLedPin` | active high |

No touch-panel pins are assigned yet.

## Observed bench wiring (2026-10-26)

These values were measured on the two ESP32-D0WD-V3 dev boards, not assumed.
They differ from the scaffold table above, which remains an S3 placeholder.

| Signal | USB0 receiver | USB1 simulator |
| --- | --- | --- |
| I²C SDA / SCL | 21 / 22 | 21 / 22 |
| SPI SCK / MISO / MOSI | 18 / 19 / 23 | 18 / 19 / 23 (radio pins; no SD fitted) |
| micro-SD CS | 16 | not fitted |
| RFM69 CS / RESET / DIO0 | 5 / 17 / 13 (confirmed; RESET active high, leave LOW to run) | 5 / **not connected** / 13 (USB1 pin 9 left open by schematic omission; simulator-only) |
| OLED | SSD1306 at `0x3C` (soldered 2026-11-11) | I²C SSD1306 at `0x3C` |
| DS3231 | `0x68` | `0x68` |
| BME280 | `0x76` | `0x76` |
| RTC-module EEPROM | `0x57` | `0x57` |

The on-target smoke test is `firmware/tests/hardware/smoke/smoke.ino`; the
focused reset/pin test is `firmware/tests/hardware/rfm69_pins/rfm69_pins.ino`.
The USB0 RFM69 responds with RESET 17 held low, but its version register reads
`0x00` while the other registers match RFM69 defaults. Confirm the module model
before treating this as a validated production schematic.

The USB1 RFM69 also responds on SPI 18/19/23 with CS 5 and DIO0 13, but it was
found carrying the prototype Davis configuration (19.2 kbps, 4.76 kHz
deviation, 911.915 MHz, sync `0xCB 0x89`) rather than reset defaults. Register
write/read passes, and writing `opmode` works. Its RESET pin 9 is not connected
anywhere (a schematic omission), so that radio keeps its configuration across
MCU resets and must be fully re-initialised by firmware at every boot. This is
accepted because USB1 is only the transmitter/simulator board; unplug/replug USB
to power-cycle both MCU and radio. Its version register also reads `0x00`. The
receiver and any production board still require a wired RESET.


## Transmitter simulator

| GPIO | Function | Config constant |
| ---: | --- | --- |
| 12 | SPI SCK | `kSpiSckPin` |
| 13 | SPI MISO | `kSpiMisoPin` |
| 11 | SPI MOSI | `kSpiMosiPin` |
| 10 | RFM69 CS | `kRadioCsPin` |
| 15 | RFM69 DIO0 | `kRadioDio0Pin` |
| 16 | RFM69 RESET | `kRadioResetPin` |
| 2 | Status LED | `kStatusLedPin` |

The simulator has no SD, RTC, display or Wi-Fi code.

## ESP32-S3 pins to avoid

- **Strapping pins:** GPIO0, GPIO3, GPIO45, GPIO46. Pulling them at boot changes
  the boot mode or flash voltage. Avoid them for CS/reset lines unless the
  design accounts for the boot state.
- **USB:** GPIO19/20 are used by native USB; leave them for flashing/serial.
- **Octal PSRAM:** GPIO33–37 are used on modules with octal PSRAM. The current
  scaffold disables PSRAM, but the 16 MB board choice may change this.
- **Input-only / boot behaviour:** check the specific module datasheet; not all
  ESP32-S3 dev boards expose the same pins.

## Bus and power checks

- All devices are 3.3 V logic. Confirm the SD breakout does not back-feed 5 V.
- Add 10 kΩ pull-ups on I²C SDA/SCL if the RTC module does not provide them.
- Fit 100 nF decoupling at each device and bulk capacitance near the radio.
- Keep CS lines high during boot; floating CS can make a device drive MISO.
- The DS3231 battery is for timekeeping only.

## Changing pins

1. Edit the relevant `config.h`.
2. Update this table.
3. Rebuild and run `make fw-test-hw`.
4. Record the board revision in the hardware overview.
