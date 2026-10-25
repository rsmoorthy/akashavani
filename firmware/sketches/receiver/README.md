# Receiver sketch

The `.ino` only forwards `setup()`/`loop()` to `app.cpp`. Everything reusable
lives in `firmware/libraries/`.

```bash
make fw-build BOARD=receiver
make fw-flash BOARD=receiver PORT=/dev/ttyACM0
make fw-monitor BOARD=receiver PORT=/dev/ttyACM0
```

## Boot behaviour

1. Load settings from NVS; generate an `ak-xxxxxx` device ID from the MAC on a
   blank device.
2. Start the DS3231 and seed system time; SNTP follows when Wi-Fi is up.
3. Mount the SD card (a missing card does not stop the receiver).
4. Probe the RFM95. The scaffold stops after chip detection; port the prototype
   driver in `AkashvaniRadio` to start receiving.
5. Bring up the provisioning AP and the configured site Wi-Fi, then the local
   admin server and ArduinoOTA.

## Changing pins

Edit `config.h`. The map is a placeholder; confirm it against the carrier board
before powering the radio and SD card together. See
[`docs/hardware/pinmap.md`](../../../docs/hardware/pinmap.md).

## Local admin

Connect to `Akashvani-<device-id>` (default password `akashvani-setup`), open
`http://192.168.4.1/`, and enter the admin password (default
`akashvani-admin`). Change both defaults before deployment.
