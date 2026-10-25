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
3. RFM95 version register over SPI.
4. Wi-Fi scan (without joining a network).

It intentionally does not transmit. Use the transmitter simulator and receiver
pair for the RF link test once `DavisEncoder`/`Rfm95Radio` are ported.

Keep the pin constants in `bringup/bringup.ino` in sync with
`sketches/receiver/config.h`; the duplication is deliberate so a bring-up build
does not depend on a production sketch's headers.
