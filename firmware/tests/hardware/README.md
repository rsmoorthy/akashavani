# On-target bring-up

These sketches verify the electrical and driver assumptions that host tests
cannot cover. They are not part of the production firmware and are compiled only
when explicitly requested.

```bash
make fw-test-hw
make fw-flash BOARD=bringup PORT=/dev/ttyACM0
make fw-monitor PORT=/dev/ttyACM0
```

`bringup` checks:

1. I2C bus scan and DS3231 presence.
2. SD card mount and card size.
3. The RFM95 seam's version register (0x12). The current bench hardware is
   RFM69 (0x24); use `smoke` or `rfm_scan` for that.
4. Wi-Fi scan (without joining a network).

`smoke` discovers the actual bench pins and checks the RFM69, SD card, DS3231,
BME280 and SSD1306 OLED. `rfm69_pins` is the focused bench test for the observed
wiring (SPI 18/19/23, CS 5, RESET 17, DIO0 13); it accepts either reset defaults
or a configured module that passes a register write/read. `rfm_scan` is a
read-only SPI/CS scanner for a radio that does not answer on the expected pins.
`ap_client` runs on the second board and exercises the receiver's provisioning
AP and HTTP page: it scans for `akashavani-*`, connects with the bench AP
password, checks the DHCP address, reads status/settings, performs a Wi-Fi
profile change that must trigger a reboot, verifies persistence, restores the
original list and spot-checks server-side validation. None of these sketches
transmit on the RF bands.

The current bench boards are ESP32-D0WD-V3 (classic), so compile these with
`esp32:esp32:esp32`, not the S3 FQBN. Use the transmitter simulator and receiver
pair for the RF link test once `DavisEncoder`/`Rfm69Radio` are ported.

Keep the pin constants in `bringup/bringup.ino` in sync with
`sketches/receiver/config.h`; the duplication is deliberate so a bring-up build
does not depend on a production sketch's headers. The observed bench pins are
recorded in `docs/hardware/pinmap.md`.
