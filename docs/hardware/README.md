# Hardware overview

!!! warning "Bench hardware differs from the scaffold"
    The two bench boards are ESP32-D0WD-V3 (classic) with RFM69, DS3231,
    BME280, an I²C SSD1306 OLED and (on USB0) a micro-SD card. The production
    target is still ESP32-S3. The pin tables in the scaffold remain
    placeholders; observed bench pins are recorded in the
    [pin map](pinmap.md#observed-bench-wiring-2026-10-26). Do not treat any of
    this as a validated production schematic.

## Boards

| Board | Purpose | Fitted components |
| --- | --- | --- |
| Receiver | Production data collection | ESP32-S3 target, RFM69W/HCW, DS3231, micro-SD, 1" LCD, status LED |
| Transmitter simulator | Bench RF source | ESP32-S3 target, RFM69, SSD1306 status panel, status LED |

The production target is a **3.3 V** ESP32-S3 module. The two current bench
boards are ESP32-D0WD-V3 (classic ESP32, 4 MB, no PSRAM). The RFM69 is a 3.3 V part; do not
power it from 5 V. SD-card breakout boards vary: use one with a 3.3 V regulator
and level shifters, or verify that it is safe on 3.3 V logic.

## Bill of materials (current prototype)

| Item | Notes |
| --- | --- |
| ESP32-D0WD-V3 bench board, 4 MB flash | current bench hardware; ESP32-S3 16 MB is the production target |
| RFM69W/HCW 868/915 MHz module | antenna matched to the legal band |
| DS3231 RTC module | CR2032 backup cell; I²C at 0x68 |
| micro-SD breakout | SPI mode, 3.3 V-safe |
| 1.0" I²C SSD1306 OLED | fitted on both bench boards at `0x3C`; USB0 soldered 2026-11-11 |
| 3.5" touch panel | not selected; upgrade path |
| RFM69 module | USB0 responds on CS 5 with RESET 17 held low; version register reads `0x00` but RFM69 defaults match. USB1 has no RESET connection (pin 9 open) and is the simulator only |
| Davis weather station | existing station; model to be recorded |

## Shared SPI bus

The radio, SD card and display share one SPI bus. Each device needs a separate
chip-select and must be deselected when idle. The scaffold initialises the bus
in each driver; be careful not to reconfigure the bus while another transfer is
in progress.

```text
ESP32 ── SCK 18 / MISO 19 / MOSI 23 ──┬── RFM69 (CS 5, DIO0 13; RESET 17 on USB0, not connected on USB1)
                                       └── microSD (CS 16 observed on USB0)

ESP32 ── I²C SDA 21 / SCL 22 ──┬── DS3231 (0x68)
                               ├── BME280 (0x76)
                               ├── RTC EEPROM (0x57)
                               └── SSD1306 OLED (0x3C, USB1 only)
```

## Bring-up order

1. Power the board alone; check 3.3 V rails and current draw.
2. Run the bring-up sketch and confirm the I²C scan finds `0x68`.
3. Insert a known-good FAT32 card; confirm mount and size.
4. Connect the RFM69. On USB0, hold RESET 17 low and confirm the register signature; the version register may read `0x00` on some modules. On USB1, RESET is not connected: power-cycle the board and expect the module to retain its previous configuration.
5. Confirm Wi-Fi scan sees the lab network.
6. Only then connect the display and enable its driver.
7. Pair the simulator and receiver on a bench with a dummy load/antenna.

See [Pin map](pinmap.md) for the exact GPIO assumptions and
[Testing](../build/testing.md) for the commands.

## Power and RF notes

- Provide bulk capacitance near the RFM69; transmit bursts draw current.
- Keep the antenna away from the SD card and the ESP32 antenna.
- Never transmit without an antenna or dummy load.
- Check local regulations for frequency, duty cycle and power. The default
  `915 MHz` / `17 dBm` values are placeholders.
- The DS3231 backup cell only preserves time; it does not power the board.
- Use a level shifter if any module exposes 5 V pull-ups on the SPI lines.

## Mechanical and enclosure

Not designed yet. Account for:

- SD-card access for archival offload;
- antenna clearance and connector;
- display window and touch-panel flex;
- RTC battery replacement;
- USB/service access for flashing and serial logs;
- strain relief for the antenna and power cables.

Record the final board revision, pin map and BOM in this directory before the
first onsite deployment.
