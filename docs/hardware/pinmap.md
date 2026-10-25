# Pin map

!!! warning "Placeholder assignment"
    These GPIOs match `firmware/sketches/receiver/config.h` and the bring-up
    sketch. Verify against the actual ESP32-S3 carrier board before applying
    power. Change the config header and this table together.

## Receiver

| GPIO | Function | Config constant | Notes |
| ---: | --- | --- | --- |
| 12 | SPI SCK | `kSpiSckPin` | shared bus |
| 13 | SPI MISO | `kSpiMisoPin` | shared bus |
| 11 | SPI MOSI | `kSpiMosiPin` | shared bus |
| 10 | RFM95 CS | `kRadioCsPin` | active low |
| 15 | RFM95 DIO0 | `kRadioDio0Pin` | interrupt input |
| 16 | RFM95 RESET | `kRadioResetPin` | active low |
| 9 | micro-SD CS | `kSdCsPin` | active low |
| 4 | I²C SDA | `kI2cSdaPin` | DS3231; external pull-up |
| 5 | I²C SCL | `kI2cSclPin` | DS3231; external pull-up |
| 8 | Display CS | `kDisplayCsPin` | reserved; display disabled |
| 7 | Display DC | `kDisplayDcPin` | reserved |
| 6 | Display RST | `kDisplayResetPin` | reserved |
| 2 | Status LED | `kStatusLedPin` | active high |

No touch-panel pins are assigned yet.

## Transmitter simulator

| GPIO | Function | Config constant |
| ---: | --- | --- |
| 12 | SPI SCK | `kSpiSckPin` |
| 13 | SPI MISO | `kSpiMisoPin` |
| 11 | SPI MOSI | `kSpiMosiPin` |
| 10 | RFM95 CS | `kRadioCsPin` |
| 15 | RFM95 DIO0 | `kRadioDio0Pin` |
| 16 | RFM95 RESET | `kRadioResetPin` |
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
