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
2. On a blank device only, apply the temporary bench identity (`station-1`,
   local server, credentials from git-ignored `secrets.h`). Once settings are
   saved through the AP page, NVS wins.
3. Start the DS3231 and seed system time; SNTP follows when Wi-Fi is up.
4. Mount the SD card (a missing card does not stop the receiver).
5. Probe the RFM69 and write the full Davis configuration. In RF bench mode
   (`config::kRfTestMode = true`) Wi-Fi, SD, OTA and upload are skipped and the
   receiver only logs RF frames while hopping channels.
6. Bring up the provisioning AP at `10.90.0.1` and the configured site Wi-Fi,
   then the local status/config server on port 80 and ArduinoOTA.
7. Poll the server every 5 s for commands and run the OTA flow.

## RF bench mode

`config::kRfTestMode` is `true` for the current RF/Davis slice. The receiver
boots directly into a channel-hopping log loop and accepts serial commands:

- `R` reset channel/statistics;
- `S` or `?` print statistics.

The loop logs `RX`, `RX_BAD`, `HOP`, `SEARCH`, `RESYNC` and periodic `STATS`
lines. See [`docs/specs/rf-davis-receive.md`](../../../docs/specs/rf-davis-receive.md) and
`tools/rf-bench.py`. Set `kRfTestMode = false` to restore the full receiver.

## OTA commands

The receiver handles `get_config`, `push_config`, `download_new_fw` and
`boot_with_new_fw`. Download stages into the inactive slot and verifies SHA-256;
boot selects the staged slot and restarts. See
[`docs/operations/ota.md`](../../../docs/operations/ota.md).

`partitions.csv` in this directory defines the 4 MB layout with two 1.875 MiB
OTA slots. Do not remove it without replacing the partition scheme.

## Changing pins

Edit `config.h`. The current values match the observed ESP32-D0WD-V3 bench
wiring. Confirm them against any new carrier before powering the radio and SD
card together. See
[`docs/hardware/pinmap.md`](../../../docs/hardware/pinmap.md).

Copy `secrets.example.h` to `secrets.h` and fill in the local Wi-Fi password and
device token. `secrets.h` is git-ignored.

## Local admin

Connect to `akashvani-<last MAC byte>` (default password `doordarshan`), open
`http://10.90.0.1/`. The page refreshes status every 5 s and lets you save the
station ID, the Python server URL and up to six site Wi-Fi networks. Changing a
Wi-Fi network restarts the board. The AP password and DHCP range are constants
in `config.h`.
