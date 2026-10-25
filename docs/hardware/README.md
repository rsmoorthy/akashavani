# Hardware overview

!!! warning "No hardware is connected"
    This page records the intended prototype wiring and the assumptions that
    still need verification. Do not treat it as a validated schematic.

## Boards

| Board | Purpose | Fitted components |
| --- | --- | --- |
| Receiver | Production data collection | ESP32-S3, RFM95W, DS3231, micro-SD, 1" LCD, status LED; RFM69 planned |
| Transmitter simulator | Bench RF source | ESP32-S3, RFM95W, status LED only |

Both boards use a **3.3 V** ESP32-S3 module. The RFM95W is a 3.3 V part; do not
power it from 5 V. SD-card breakout boards vary: use one with a 3.3 V regulator
and level shifters, or verify that it is safe on 3.3 V logic.

## Bill of materials (current prototype)

| Item | Notes |
| --- | --- |
| ESP32-S3 dev board, 4 MB flash | 16 MB variant planned for headroom |
| RFM95W 868/915 MHz module | antenna matched to the legal band |
| DS3231 RTC module | CR2032 backup cell; I²C at 0x68 |
| micro-SD breakout | SPI mode, 3.3 V-safe |
| 1.0" ST7735-class LCD | 80x160, SPI; driver not yet implemented |
| 3.5" touch panel | not selected; upgrade path |
| RFM69 module | planned second radio option |
| Davis weather station | existing station; model to be recorded |

## Shared SPI bus

The radio, SD card and display share one SPI bus. Each device needs a separate
chip-select and must be deselected when idle. The scaffold initialises the bus
in each driver; be careful not to reconfigure the bus while another transfer is
in progress.

```text
ESP32-S3 ── SCK/MOSI/MISO ──┬── RFM95  (CS 10)
                             ├── microSD (CS 9)
                             └── display (CS 8, disabled by default)
```

## Bring-up order

1. Power the board alone; check 3.3 V rails and current draw.
2. Run the bring-up sketch and confirm the I²C scan finds `0x68`.
3. Insert a known-good FAT32 card; confirm mount and size.
4. Connect the RFM95; confirm version register `0x12`.
5. Confirm Wi-Fi scan sees the lab network.
6. Only then connect the display and enable its driver.
7. Pair the simulator and receiver on a bench with a dummy load/antenna.

See [Pin map](pinmap.md) for the exact GPIO assumptions and
[Testing](../build/testing.md) for the commands.

## Power and RF notes

- Provide bulk capacitance near the RFM95; transmit bursts draw current.
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
