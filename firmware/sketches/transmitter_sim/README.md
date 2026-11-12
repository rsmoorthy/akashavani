# Transmitter simulator sketch

Bench transmitter for testing the receiver before a real Davis station is
available. It generates bounded, deterministic observations with
`DavisSimulator`, encodes them as real eight-byte Davis frames (including the
CRC) and sends them through `Rfm69Radio`.

```bash
make fw-build BOARD=transmitter_sim
make fw-flash BOARD=transmitter_sim PORT=/dev/ttyUSB1
```

## Behaviour

- Writes the full Davis RFM69 configuration on every boot. The USB1 board has no
  RFM69 RESET connection, so the chip must never be assumed to be in a known
  state.
- Sends one frame immediately and then exactly every 3 s, matching the ISS slot.
- Rotates through the ISS message types so the receiver exercises every decoder
  path.
- Puts a wrapping test sequence in byte 5 (unused by decoded fields) so the
  receiver can measure gaps.
- Default bench power is 0 dBm. The prototype's +20 dBm setting was reduced
  because adjacent boards showed receiver saturation and avoidable CRC errors.
  Change `config::kRadioPowerDbm` only with an antenna/dummy load and attention
  to local limits.

## Serial commands

| Command | Action |
| --- | --- |
| `R1` | continuous transmission, no skipped channels |
| `R2` | continuous transmission, random 1-5 channel slots skipped |
| `R3` | continuous transmission with automatic restarts at 3 and 8 minutes |
| `S` | stop |
| `X` | restart now: channel 1, sequence 0 |
| `?` | print status |

`tools/rf-bench.py` drives these commands and timestamps both boards' serial
logs. The simulator has no SD card, archive or cloud path. It does join the site
Wi-Fi from `secrets.h` purely so the SSD1306 status panel can show the live SSID
(`config::kEnableWifi = false` disables this), and it renders the panel through
`Ssd1306Display`. Wi-Fi connection loss is harmless and retried every 15 s.
