# Development handoff

> Current agreed backlog: `docs/development/nextsteps.md`. It is the starting
> point for the next implementation session.

**Date:** 2026-10-26
**Latest slice:** 2026-11-13, interrupt-driven RFM69 receive + channel scan task, bench soak
**Scaffold commit:** `ab8078c` (`Initial Akashvani scaffold`)
**Bench smoke test:** 2026-11-11 (`/dev/ttyUSB0`, `/dev/ttyUSB1`)

This page records what was completed, what was verified, what is still a stub,
and the plan agreed with the project owner. Update it at each slice boundary.

## What was built

- Monorepo layout for firmware, protocol, server, docs and tooling.
- Eight Arduino libraries under `firmware/libraries/`:
  Core, Radio, Davis, Protocol, Storage, Net, Rtc, Display.
- Two thin sketches: `receiver` and `transmitter_sim`.
- RFM69 (current hardware) / RFM95 (alternate) seams. RFM69 now has a working
  Davis SX1231 driver; RFM95 remains a stub. Davis decoder/encoder implemented,
  deterministic Davis simulator, SD JSONL archive, NVS provisioning, AP+STA Wi-Fi, local admin,
  cloud upload queue, command polling, two-step HTTP-pull OTA, LAN ArduinoOTA
  and local admin.
- Native C++ tests (CMake + GoogleTest) and an on-target bring-up sketch.
- FastAPI server with device-token auth, idempotent JSON/CBOR ingest, command
  lease/ack, firmware release/check/download and a small admin API.
- Two-step command-driven OTA (`get_fw_version`, `download_new_fw`,
  `boot_with_new_fw`) with streaming SHA-256 verification, plus a multipart
  firmware upload endpoint and `/admin` UI.
- SQLite development database, PostgreSQL option, Alembic initial migration.
- Docker image and compose files for local development.
- JSON Schemas, examples, architecture/operations docs and ADRs.

## Verified on this machine

### Scaffold (no hardware)

No device was attached during scaffold work.

| Check | Result |
| --- | --- |
| `make fw-test` | 36/36 at scaffold time; 41/41 after the RF/Davis slice |
| receiver build | compiles, now 1,226,845 bytes of the 1,966,080-byte OTA slot |
| transmitter simulator build | compiles |
| bring-up sketch build | compiles |
| `make server-test` | 42/42 tests pass (was 34 before the command/OTA slice) |
| `make lint` | Ruff + mypy pass |
| `make docs-build` | strict MkDocs build passes |
| Docker image + compose | builds, migrates, health/ingest verified |
| OTA export | binary + SHA-256 produced |

### Bench smoke test (2026-10-26)

Two ESP32 dev boards were attached on `/dev/ttyUSB0` and `/dev/ttyUSB1`. Both
report as **ESP32-D0WD-V3 (revision 3.1), 4 MB flash, no PSRAM**, not the
ESP32-S3 assumed by the scaffold. Both use a CP2102 USB-UART. The smoke-test
sketch is `firmware/tests/hardware/smoke/smoke.ino`; the focused reset/pin test
is `firmware/tests/hardware/rfm69_pins/rfm69_pins.ino`; the read-only RFM69 scan
is `firmware/tests/hardware/rfm_scan/rfm_scan.ino`. No transmission was
performed.

| Component | USB0 (receiver) | USB1 (simulator) | Evidence |
| --- | --- | --- | --- |
| ESP32 | ESP32-D0WD-V3, 4 MB, no PSRAM, MAC `cc:7b:5c:27:31:e0` | ESP32-D0WD-V3, 4 MB, no PSRAM, MAC `08:b6:1f:b8:38:50` | serial banner, esptool `chip_id` |
| I²C bus | SDA 21, SCL 22 | SDA 21, SCL 22 | I²C scan |
| DS3231 | present at `0x68`; time invalid (`2002-02-08`), OSF clear; +2 s tick PASS; 31.25 °C | present at `0x68`; time invalid (`2000-10-07`), OSF clear; +2 s tick PASS; 27.25 °C | register read/tick test |
| BME280 | present at `0x76`; chip id `0x60`; T 29.17 °C, P 910.78 hPa, RH 45.47 % | present at `0x76`; chip id `0x60`; T 26.57 °C, P 910.65 hPa, RH 51.46 % | compensated measurement |
| EEPROM | present at `0x57` (DS3231 module) | present at `0x57` (DS3231 module) | I²C scan |
| micro-SD | mounted on VSPI CS **16**, 29 557 MB SDHC; write/read/remove PASS | not fitted / not detected | `SD.begin` + file test |
| OLED | not fitted / not detected | SSD1306 at `0x3C`; init 25/26 commands ACKed, clear/pattern PASS; **visual confirmation still required** | I²C + SSD1306 commands |
| RFM69 | **detected and responding** on SPI 18/19/23, CS 5, RESET 17, DIO0 13; version reg reads `0x00`, but RFM69 defaults match; register write/read PASS; RX RSSI ~-106 dBm | **detected and responding** on SPI 18/19/23, CS 5, DIO0 13; version reg reads `0x00`; register write/read PASS; retains prototype Davis config (19.2 kbps, 4.76 kHz dev, 911.915 MHz, sync `CB 89`); RESET pin 9 **not connected** (schematic omission) | see below |

Observed bench wiring differs from the scaffold placeholders:

| Signal | Observed on bench | Scaffold assumption |
| --- | --- | --- |
| I²C | SDA 21, SCL 22 | SDA 4, SCL 5 |
| SPI (radio/SD) | VSPI SCK 18, MISO 19, MOSI 23 | SPI2 SCK 12, MISO 13, MOSI 11 |
| micro-SD CS | 16 | 9 |
| RFM69 CS / RESET / DIO0 | 5 / 17 / 13 (USB0 confirmed); 5 / **not connected** / 13 (USB1, pin 9 open) | 10 / 16 / 15 |
| display | I²C SSD1306 `0x3C` (0.95–0.96 inch class) | SPI ST7735, CS 8 / DC 7 / RST 6 |

**RFM69 status (USB0):** the radio is present and responding. The decisive
detail was RESET **17**: the SX1231 RESET is **active high**, so the previous
probe held the chip in reset by leaving GPIO 17 high. With GPIO 17 driven low,
the chip reports `opmode=0x04` (standby), bitrate `0x1A0B` (4.8 kbps),
deviation `0x0052` (5 kHz), frequency `0xE4C000` (915 MHz) and
`irqFlags1=0x80` (ModeReady). The version register (`0x42`) reads `0x00` on this
module rather than the expected `0x24`, but the register defaults and a passing
sync-register write/read (`0xAA`/`0x55`) identify it as an RFM69-compatible
part. RX mode was entered without transmitting and RSSI was about -106 dBm.
`DIO0` (GPIO 13) was low with no packet present.

USB1 was retested on 2026-10-26 with the same wiring. **RFM69 status (USB1):**
the radio is also present and responding, but it does not enter reset-default
state. The register map is stable and readable (GPIO 17 has no effect because
pin 9 is open; see below):
`opmode=0x10` (RX, later `0x04` standby), `bitrate=0x0683` (19.2 kbps),
`fdev=0x004E` (4.76 kHz), `frf=0xE3FA92` (911.915 MHz), `sync=0xCB 0x89`,
`irqFlags1=0x80` (ModeReady), RSSI about -86 to -93 dBm, DIO0 low. This is the
Davis configuration from `prototype/lib2/DavisRFM69` (19.2 kbps, 4.8 kHz
deviation, sync bytes `0xCB`/`0x89`, FRF entry `0xE3 0xFA 0x92` in the US
frequency table), so the module keeps its previous configuration across MCU
resets. Sync-register write/read passes (`0xAA`->`0xAA`, `0x55`->`0x55`) and
writing `opmode=0x04` reads back `0x04`. Pulsing RESET 17 high then low did not
restore defaults or change the configuration. Version `0x42` again reads
`0x00`, the same clone behaviour as USB0. No transmission was performed.

**USB1 RESET (confirmed physically, 2026-10-26):** RFM69 pin 9 (RESET) is **not
connected anywhere** on the USB1 board. It was left out of the schematic as
"not required", which is not correct in general. The omission is accepted for
this board because USB1 is only the **transmitter/simulator** bench board, not
a development or production receiver, and the radio was proven configurable
over SPI without it. `transmitter_sim` should be developed and run on USB1.

Consequences to keep in mind for USB1:

- The radio state at MCU boot is unknown. Sketch upload, EN reset and the
  Arduino auto-reset do **not** reset the RFM69; the chip keeps its previous
  mode and configuration (observed as RX and standby on different runs while
  the Davis bitrate/FrF/sync persisted). Simulator firmware must write the full
  radio configuration on every startup and read it back, never assume reset
  defaults.
- Power-cycle the board (unplug/replug USB) before an RF test run and after a
  crash or firmware change, so receiver and simulator start from a known state.
- A reset during a transmit could leave the chip in TX, i.e. a continuous
  carrier. Return `opmode` to standby after each burst and power-cycle if the
  state is uncertain.
- Pin 9 is left floating. If noise causes spurious resets, fit a pull-down on
  pin 9 (if reachable) or accept the occasional power-cycle. Do not carry the
  omission to the receiver or a production board: there RESET must be wired so
  every boot starts from a known state.

The focused test `firmware/tests/hardware/rfm69_pins/rfm69_pins.ino` now accepts
a configured-but-responding module (version `0x24`, reset defaults, or a passing
sync-register write/read) instead of requiring reset defaults, so it reports
USB1 as RFM69-compatible. The RFM69/Davis path is now implemented and was
bench-tested on 2026-10-27; see the RF/Davis slice section at the end.

Unverified: OLED visual output and TLS.
**HTTP-pull OTA was verified end-to-end on USB1 on 2026-10-26** (see the OTA
section at the end); image signatures and rollback are still missing. The RTC
responds and SNTP sets it when Wi-Fi is up, but the DS3231's stored time was
invalid before that.

## Explicit stubs / gaps

| Area | Path | What is missing |
| --- | --- | --- |
| RFM95 (alternate) | `AkashvaniRadio/src/Rfm95Radio.cpp` | modem registers, bitrate/deviation/bandwidth, FIFO, IRQ, raw mode, transmit |
| Display | `AkashvaniDisplay/src/St7735Display.cpp` | reset/init/draw; panel disabled by default. Bench board has an I²C SSD1306 at `0x3C`, which needs a different driver. |
| TLS | `AkashvaniNet/src/ApiClient.cpp`, `OtaService.cpp` | CA pinning, remove `setInsecure()` |
| OTA security | `AkashvaniNet/src/OtaService.cpp` | signatures, TLS pinning, bootloader rollback (SHA-256 streaming is implemented) |
| Command set | `receiver/app.cpp` | `set_config` and `upload_logs` return unsupported |
| Retention | `AkashvaniStorage` | no card cleanup policy |
| Production RF sync | `receiver/app.cpp`, `Rfm69Radio` | blind reverse-scan restart acquisition is a bench aid; decide the field policy |

Implemented in this slice: the RFM69 SX1231 driver, Davis CRC/decoder/encoder,
the 51-channel hop table and the command-driven simulator. See
`docs/specs/rf-davis-receive.md`.

## Agreed collaboration model

The owner will:

1. connect the receiver and transmitter boards (both carry RFM69);
2. provide specs per feature;
3. share the working Davis RFM69 code, register values and captured frames.

We will implement and test **one vertical slice at a time**, not a big-bang
delivery.

### Slice order

1. **RFM69 receive + Davis decode.** Receiver prints validated frames with the
   expected values; corrupt frames rejected. Compare against reference captures.
2. **Davis encode + simulator transmit.** Transmitter sends synthetic frames
   (runs on USB1, the simulator board); receiver decodes them; values match.
3. **RTC/SNTP + SD archive.** Valid time, daily JSONL, power-cycle recovery.
4. **Wi-Fi AP+STA + local admin.** Provisioning, status, credential changes.
5. **Cloud upload + retry/idempotency.** Offline queue, duplicate handling,
   server reconciliation.
6. **Commands + OTA hardening.** Lease/ack and the two-step SHA-256 OTA flow
   are done and bench-tested; signatures, TLS pinning and rollback remain.
7. **Display/touch UI.** Bench OLED first; scaffold ST7735 seam or a larger touch panel later.

For each slice, collect:

- a short spec (create `specs/<slice>.md` when the first arrives);
- known-good reference code where applicable;
- raw test vectors/expected values;
- acceptance criteria;
- evidence: serial log, SD file sample, curl response, or user observation for
  physical/visual results.

## First slice checklist

- [x] Confirm receiver and transmitter serial ports (`/dev/ttyUSB0`, `/dev/ttyUSB1`).
- [x] Confirm permission to run `arduino-cli upload` / `monitor`.
- [x] Confirm bench pin map. I²C 21/22, VSPI 18/19/23, SD CS 16 and USB0
      RFM69 CS 5 / RESET 17 / DIO0 13 are observed and applied to the receiver
      config. USB1 RFM69 is CS 5 / DIO0 13 with RESET (pin 9) left unconnected.
- [ ] Confirm RF bench: antenna/dummy load, attenuator, distance, legal frequency
      and power. The 2026-10-27 test ran at 0 dBm with the two boards adjacent;
      antenna/dummy-load presence was not independently observed by the agent.
- [x] Receive the working RFM69 register configuration (prototype code is in
      `prototype/lib2/DavisRFM69`; USB0 hardware now responds with RESET 17 low).
- [x] Receive 3–10 raw Davis frames plus expected decoded values (capture in
      `prototype/minicom.cap`, vectors in
      `firmware/tests/native/vectors/davis_iss_frames.txt`).
- [x] Write `docs/specs/rf-davis-receive.md` with acceptance criteria.
- [x] Port the driver and decoder into the existing seams.
- [x] Add captured frames under `firmware/tests/native/vectors/` with RF settings.
- [x] Run receiver + reference transmitter, capture serial evidence (see the RF
      slice section at the end).

## Open decisions

- Board type: production target is ESP32-S3, but both bench boards are ESP32
  classic. Confirm whether production follows the bench board or the S3 plan.
- Final board: 4 MB is currently viable at ~1.23 MB app size; 16 MB may be
  needed for TLS, a larger UI or future features.
- Display: the bench board has an I²C SSD1306 at `0x3C`; the scaffold has an SPI ST7735 seam. Confirm the production display.
- Frequency/band and legal power for the deployment site.
- RFM69 driver port; USB0 and USB1 both respond. USB1 has no RFM69 RESET
  connection by design omission, accepted because it is only the simulator;
  its radio keeps the prototype Davis configuration across MCU resets.
- Whether local admin should be available on the site LAN or only the AP.
- License is still TBD.
- Production CA, OTA signing and rollback policy.

## Environment notes

- Bench boards: ESP32-D0WD-V3 (classic), 4 MB, no PSRAM, CP2102 USB-UART.
- ESP32 core `3.0.2`; ArduinoJson `7.1.0`; RTClib `2.1.4` (Adafruit BusIO
  `1.17.4`).
- `arduino-cli 1.0.1` was used for validation.
- Python `3.11`, FastAPI 0.162 / SQLAlchemy 2.0 in the local venv.
- `.venv` and all build/output directories are git-ignored.
- Native tests fetch GoogleTest 1.14.0 on first configure.
- Docker image was built with Python 3.12.

## How to resume

```bash
git log --oneline -3
git status
make setup
make test
make fw-build-all
```

# Development 2 handoff

**Date:** 2026-10-26 17:30
**Bench smoke test:** 2026-10-26 (`/dev/ttyUSB0`)

Your pin map works on USB0. The RFM69 is alive.
USB0 result with your wiring
Signal	Value
SPI	SCK 18, MISO 19, MOSI 23
RFM69 CS	5
RFM69 RESET	17 — active high; must be left LOW to run
RFM69 DIO0/INT	13
The key was RESET 17. My earlier probe left it high, which held the chip in reset. Driving it low gives:
opmode      = 0x04      standby
bitrate     = 0x1A0B    4.8 kbps
deviation   = 0x0052    5 kHz
frequency   = 0xE4C000  915 MHz
irqFlags1   = 0x80      ModeReady
sync write/read: 0xAA -> 0xAA, 0x55 -> 0x55   PASS
RX RSSI     ~ -106 dBm
DIO0        = 0         no packet present
The version register 0x42 reads 0x00, not the usual 0x24, so this module is likely an RFM69-compatible variant/clone. The default register values, ModeReady flag, and passing register write/read identify it as RFM69-compatible. I did not transmit.
Updated USB0 status
Component	Status
RFM69	PASS — SPI 18/19/23, CS 5, RESET 17 held low, DIO0 13
micro-SD	PASS — VSPI CS 16, write/read/remove verified
DS3231	present at 0x68; ticking, but stored time is invalid
BME280	present at 0x76; chip ID 0x60, plausible readings
OLED	not fitted
Changes made
- Updated firmware/tests/hardware/smoke/smoke.ino to pulse RESET 17 and keep SD CS 16 high. The unified smoke test now reports RFM69: PASS on USB0.
- Added focused test firmware/tests/hardware/rfm69_pins/rfm69_pins.ino.
- Updated docs/development/handoff.md, docs/hardware/pinmap.md, docs/hardware/README.md, and configuration docs with the confirmed USB0 mapping and the RESET-17/version-0x00 finding.
- make docs-build passes.
USB1 was not retested with the RESET 17 mapping, since you asked specifically for USB0. Its earlier image still reports RFM69 not found. I can run the same test on USB1 next if you want.

# OTA + command slice handoff

**Date:** 2026-10-26 23:40
**Board:** USB1 (`/dev/ttyUSB1`), ESP32-D0WD-V3, 4 MB, MAC `08:b6:1f:b8:38:50`
**Server:** `http://192.168.29.31:8000`, admin UI at `/admin`

## What changed

- Receiver and simulator FQBNs now target the actual ESP32 classic bench
  boards (`esp32:esp32:esp32`), not the ESP32-S3 scaffold placeholder.
- Receiver pins now match the observed bench wiring: I2C 21/22, VSPI 18/19/23,
  SD CS 16, RFM69 CS 5 / RESET 17 / DIO0 13.
- The 4 MB receiver sketch carries an explicit partition table
  (`firmware/sketches/receiver/partitions.csv`): two `0x1E0000` app slots
  (1,966,080 bytes = 1.875 MiB each), 128 KiB SPIFFS, 64 KiB coredump.
- Device identity: hardcoded `station-1`, token, SSID, password and server URL
  for this phase. The Wi-Fi password and token live in git-ignored
  `firmware/sketches/receiver/secrets.h`; `secrets.example.h` is committed.
- Devices send `X-Station-Id`, `X-Device-Mac` and `X-Firmware-Version` on every
  API call. The server records station, MAC, last-seen time and firmware version.
- Command polling no longer requires a valid clock and runs every 5 s.
- Command results are sent back in the acknowledgement, not just `ok`/`error`.
- OTA commands: `get_fw_version`, `download_new_fw`, `boot_with_new_fw`.
  Download stages into the inactive slot with streaming SHA-256 verification and
  does **not** change the boot partition; boot selects the staged slot and
  restarts. The staged release is recorded in NVS so a boot command cannot
  select a different image.
- Server: files can be uploaded through
  `POST /api/v1/admin/firmware/upload` (multipart), and the admin UI at `/admin`
  lists devices/releases/commands and queues OTA commands.
- Alembic revision `4f2a7c91b3d0` adds `devices.station_id` and
  `devices.mac_address`.

## Verified end-to-end on USB1

| Step | Result |
| --- | --- |
| Flash `0.1.0-dev` over USB | PASS, ~1.23 MB image |
| Download `0.2.0` via command | PASS, 1,233,360 bytes staged in `app1`, SHA-256 matched |
| Boot staged `0.2.0` | PASS, device restarted into `app1`, boot log reports `Akashvani 0.2.0` |
| Second OTA to `0.2.1` | PASS, staged in `app0`, restarted into `app0` |
| `get_fw_version` | PASS, reports `0.2.1`, partition `app0`, station 1, MAC |
| `boot_with_new_fw` without staged image | Correctly failed: `no staged image recorded` |
| Admin UI `/admin` | HTTP 200; device identity and command history visible |

Firmware size after adding OTA: **1,226,845 bytes** of 1,966,080 (62% of the OTA
slot), leaving ~739 KB free. The `0.2.1` OTA image is 1,233,424 bytes.

## Evidence

- Serial log: `/tmp/opencode/usb1-ota.log` (boot, download progress, SHA line,
  boot-partition selection, reset, new boot banner).
- Server log: `/tmp/opencode/akashvani-server.log`.
- Server command rows show `acked` with result JSON for download/boot and
  `failed` for the deliberate boot-without-download test.

## Still missing / not production-ready

- OTA images are not signed; secure boot/signature verification is not done.
- TLS still uses `setInsecure()` for HTTPS URLs. Bench server is HTTP.
- No bootloader rollback/health-check policy. A staged image that boots but is
  unhealthy will stay in that slot.
- The hardcoded dev credentials and station ID must be replaced by provisioning.
- No authentication on the HTML shell at `/admin`; the API calls it makes still
  require the admin bearer token, which the operator enters in the page.
- RFM69, Davis codec and SD archive remain stubs, so the uploaded firmware does
  not yet receive weather frames.

# AP + web configuration slice handoff

**Date:** 2026-10-27
**Receiver:** USB0 (`/dev/ttyUSB0`), ESP32-D0WD-V3, MAC `cc:7b:5c:27:31:e0`
**Test client:** USB1 (`/dev/ttyUSB1`), temporary `ap_client` image
**Firmware:** `0.3.0`

## What changed

- `WifiService` now always brings up the AP with:
  - SSID `akashvani-XX`, where `XX` is the last STA MAC byte (`akashvani-e0`);
  - password and DHCP/IP range from `config.h` (default `doordarshan`,
    `10.90.0.1`, pool `10.90.0.11`-`10.90.0.20`);
  - an exact DHCP end address set through the netif DHCP server API.
- Site Wi-Fi is now an ordered list of up to six SSID/password profiles, tried
  sequentially by the STA side.
- `WebConfigServer` is a mobile-friendly single page with a 5-second status
  refresh and a configuration form for station ID, server URL and six site
  networks. Password fields stay hidden until "Show passwords"; the form
  validates SSID/password lengths, duplicates and the server URL in JavaScript.
- Server-side validation rejects station IDs outside 1-65535, non-HTTP(S) URLs,
  more than six profiles and passwords outside 8-63 characters.
- Wi-Fi changes are saved to NVS and trigger an automatic restart; station ID
  and server URL changes apply immediately.
- Development identity/credentials are now seeded only on a blank device. Once
  anything is saved through the page, NVS wins on later boots.
- Removed the old admin-token prompt from the AP page. The AP WPA2 password is
  the current security boundary; see the gaps below.
- Added `firmware/tests/hardware/ap_client` and `BOARD=ap_client` to the build
  helper.

## Verified on USB0 + USB1

| Step | Result |
| --- | --- |
| AP boot | PASS: `akashvani-e0`, `10.90.0.1`, DHCP `10.90.0.11`-`10.90.0.20` |
| Client joins AP | PASS: `10.90.0.11`, gateway `10.90.0.1` |
| `GET /api/status` | PASS: firmware, time/SNTP, STA SSID/IP, AP state, weather placeholder |
| `GET /api/settings` | PASS: station, server URL and six profile slots |
| Unchanged POST | PASS: saved, `reboot_required=false`, no restart |
| Changed Wi-Fi POST | PASS: saved, `reboot_required=true`, board restarted |
| Persistence after reboot | PASS: temporary profile survived, then was removed again |
| HTML page | PASS: title, six Wi-Fi rows, status refresh script present |
| Validation | PASS: station ID 0 and `ftp://` URL rejected with HTTP 400 |

Firmware image size: **1,246,544 bytes** (up from 1,226,845).

## Evidence

- Receiver serial: `/tmp/opencode/ap-receiver-serial3.log` (AP up, DHCP range,
  settings saved, two scheduled restarts, persisted profile count).
- Client serial: `/tmp/opencode/ap-client-serial3.log` (DHCP address, GET/POST
  responses, reboot/persistence/restore and validation results).

The temporary `ap_client` image was replaced with `transmitter_sim` on USB1
after the test.

## Still missing / not production-ready

- The AP page has no separate login; anyone with the AP password can change
  station ID, server URL and Wi-Fi profiles. Add authentication before field use.
- The device token is not editable on the page; a blank bench device still uses
  the development token from `secrets.h`.
- The AP password is a shared firmware constant, not per board.
- Extensive browser testing on a phone (layout, password toggle, validation
  messages, reboot/reconnect UX) is still to be done manually by the owner.
- Weather fields were empty at the time of that slice; the Davis decoder is now ported (see the RF slice at the end).
- TLS pinning, OTA signatures and rollback remain open from the OTA slice.

# RFM69 + Davis receive/transmit slice handoff

**Date:** 2026-10-27
**Receiver:** USB0 / `/dev/ttyUSB3` after a USB re-enumeration, ESP32-D0WD-V3,
MAC `cc:7b:5c:27:31:e0`
**Simulator:** USB1 / `/dev/ttyUSB2`, ESP32-D0WD-V3,
MAC `08:b6:1f:b8:38:50`
**Spec:** `docs/specs/rf-davis-receive.md`

## What changed

- `Rfm69Radio` is no longer a stub. It implements the SX1231 packet path from
  `prototype/lib2/DavisRFM69`: 19.2 kbps FSK, 4.8 kHz deviation, Gaussian BT=0.5,
  `CB 89` sync, fixed 8-byte payload, hardware CRC off, DSSS/DAGC/AFC settings,
  RSSI, FIFO access and the 51-entry US Davis frequency table
  (`DavisChannels.h`).
- The driver writes the complete configuration on every boot, because USB1 has
  no RFM69 RESET connection and retains its previous mode/registers. RESET is
  driven low (the SX1231 RESET is active high).
- `DavisDecoder` now validates the 8-byte frame, checks the Davis
  CRC-16/CCITT and decodes every message type seen in the capture: wind,
  temperature, humidity, rain rate, rain counter, solar, gust and goldcap.
- `DavisEncoder` mirrors the decoder and produces real CRC-correct Davis
  frames. The simulator rotates through the message types and writes a wrapping
  test sequence into byte 5 (unused by decoded fields).
- Receiver RF bench mode (`config::kRfTestMode = true`) logs `RX`, `RX_BAD`,
  `HOP`, `SEARCH`, `RESYNC` and `STATS`, and accepts `R`/`S`/`?` over serial.
- The simulator is command driven: `R1` continuous, `R2` random 1-5 skipped
  channel slots, `R3` two automatic restarts, `S` stop, `X` restart now, `?`
  status. It sends one frame immediately and then exactly every 3 s.
- `tools/rf-bench.py` runs the scenarios and timestamps both serial streams;
  `tools/rf-bench-report.py` produces the summary table. `make rf-bench` and
  `make rf-bench-report` wrap them.
- Native tests now include captured CRC vectors, every message type
  round-trip, good/bad CRC and the channel table. The vector source is
  `firmware/tests/native/vectors/davis_iss_frames.txt`.

## RF configuration used

| Setting | Value |
| --- | --- |
| Modulation | FSK, packet mode, Gaussian BT=0.5 |
| Bitrate / deviation | 19.2 kbps / 4.8 kHz |
| Sync | `0xCB 0x89`, tolerance 2 |
| Payload | fixed 8 bytes, software Davis CRC |
| RX bandwidth | 25 kHz (`REG_RXBW = 0x4C`) |
| AFC | auto, auto-clear; AFC BW 50 kHz |
| Preamble | 4 bytes |
| Channels | 51-entry US table, 902.38-927.47 MHz |
| Bench power | 0 dBm (prototype was +20 dBm) |

The prototype's +20 dBm setting saturated the receiver at close range
(RSSI about -33 dBm) and lost roughly 5-10% of frames. At 0 dBm the receiver
sees about -48 dBm and the final continuous run was 98.7%. This is a bench power
change only; confirm antenna/dummy load, distance and legal ERP before field use.

## Final test results

All three runs were 900 s per scenario. The receiver was reset before each run
with `R`; the transmitter was restarted with the scenario command.

| Test | Slots | TX | Skipped | Valid | Bad CRC | Gaps | Resyncs | Searches | Success | RSSI avg/min/max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 1 continuous | 301 | 301 | 0 | 297 | 105 | 4 (max 1) | 0 | 0 | **98.7%** | -49 / -71 / -37 dBm |
| 2 random skips | 301 | 231 | 70 | 230 | 87 | 71 (max 5) | 19 | 0 | **99.6%** | -49 / -71 / -37 dBm |
| 3 two restarts | 299 | 299 | 0 | 228 | 95 | 424* | 2 | 2 | **76.3%** | -48 / -73 / -37 dBm |

\* Test 3's gap counter is inflated by the deliberate sequence reset at each
restart. The real loss is exactly the two outages below: 11 + 60 = 71 slots,
which is 299 - 228. There were no other lost frames and no channel mismatches.

Test 1 had four isolated single-packet losses. Test 2's 71 gap slots match the
70 intentional skips plus one genuine lost packet; the receiver never entered
search mode for a normal skip run. All 297/230/228 valid frames matched the
transmitter's channel within 0.5 s, with zero channel mismatches.

### Restart catch-up (test 3)

- Restart 1 at 181.6 s: first valid frame after **33.0 s**, channel 11, seq 10.
- Restart 2 at 481.6 s: first valid frame after **180.1 s**, channel 9, seq 59.

The blind reverse scan takes one or two full channel cycles. The second restart
missed the first two near-miss windows and caught on the next pass. This is the
main remaining RF design question: a production policy could park on a channel
for at most one 153 s transmitter cycle, or use the known channel-1 restart
behaviour, instead of blind reverse scanning.

## Evidence

- Test 1: `/tmp/opencode/rf-bench-final-test1/`
  (`20261027-181829-receiver.log`, `20261027-181829-transmitter.log`,
  `report.md`)
- Test 2: `/tmp/opencode/rf-bench-final-test2/`
  (`20261027-180308-receiver.log`, `20261027-180308-transmitter.log`,
  `report.md`)
- Test 3: `/tmp/opencode/rf-bench-final-test3/`
  (`20261027-174652-receiver.log`, `20261027-174652-transmitter.log`,
  `report.md`)
- Native tests: `make fw-test` -> 41/41.
- Builds: receiver 1,034,041 bytes (52% of the OTA slot) with RF test mode on;
  simulator 294,585 bytes.
- Docs: `make docs-build` passes.

An earlier combined 45-minute run hit a USB re-enumeration at the end and lost
the tail of test 3. The final results above are from separate per-scenario runs;
the serial ports became `/dev/ttyUSB2` (simulator) and `/dev/ttyUSB3` (receiver)
after that re-enumeration.

## Bugs found and fixed during the bench work

- The receiver initially hopped channels after a bad CRC. Noise can pass the
  two-byte sync with tolerance 2, so this broke a good lock. It now ignores bad
  CRCs, like the prototype.
- Search mode triggered one hop too early, on the slot where a normal 5-slot
  skip should have resumed. It now starts after seven misses.
- Reverse search at exactly 3.0 s could stay phase-locked to the 3.0 s
  transmitter. It now uses 3.5 s per channel.
- Rain-rate encoding initially used the wrong low/high-rate thresholds and
  rejected the simulator's 1.2 mm/h rain. Fixed and covered by the round-trip
  test. The 10-bit raw value is carried in byte 3 plus byte 4 bits 4-5.

## Not done / cautions

- `config::kRfTestMode` is still `true`. The receiver therefore does **not**
  start Wi-Fi, SD, OTA or upload, and the normal production `loop()` still does a
  single receive without channel hopping. Turning test mode off does not yet
  give a production Davis receiver; the hop/sync policy must be integrated into
  the normal path first.
- No SD archive or cloud upload was exercised with real/simulated RF frames.
- Pressure and UV are not decoded because they are not in this ISS frame
  subset.
- The restart catch-up policy is a bench aid, not an agreed field behaviour.
- The 25 kHz RX filter is slightly narrow for 19.2 kbps / 4.8 kHz deviation
  (Carson bandwidth about 28.8 kHz). A 50 kHz filter is a candidate experiment;
  it was not changed here because the tested prototype uses 25 kHz.
- Antenna/dummy-load presence and legal field power were not independently
  verified by the agent. The boards were adjacent and the simulator was set to
  0 dBm.
- TLS pinning, OTA signatures and rollback remain open from earlier slices.

## How to rerun

```bash
make fw-flash BOARD=receiver PORT=/dev/ttyUSB3
make fw-flash BOARD=transmitter_sim PORT=/dev/ttyUSB2

# one 15-minute scenario
make rf-bench SCENARIO=1 DURATION=900

# or all three
make rf-bench DURATION=900

python3 tools/rf-bench-report.py \
  /tmp/opencode/rf-bench/<stamp>-receiver.log \
  /tmp/opencode/rf-bench/<stamp>-transmitter.log
```

The serial ports can change after a USB re-enumeration; check
`arduino-cli board list` and the boot banner before flashing.

# Archive collation slice handoff

**Date:** 2026-11-01
**Spec:** `docs/specs/davis-archive-collation.md`
**Receiver:** `/dev/ttyUSB0`; **simulator:** `/dev/ttyUSB1`

## What changed

- New host-testable `AkashvaniArchive` library: five-minute sums/counts, vector
  wind sums, gust maximum/direction, rain tip delta, supercap latest/minimum and
  RF quality counters.
- Receiver production path now hops/searches, samples the BME280, collates into
  5-minute SI records, writes them to SD and uploads them to
  `POST /api/v1/archive` with a shared bearer token.
- Hotspot page has an **API token** field, stored in NVS (`api_token`), default
  `secret`.
- Server `archive` table + Alembic migration, sums/counts ingest, SI rain and
  derived calculations, and `GET /api/v1/admin/archive`.
- `AKASHVANI_API_TOKEN` defaults to `secret`; the same token is accepted by the
  admin API. Site config defaults: 13.4867 N, 77.7064 E, 940 m,
  Asia/Kolkata, 0.254 mm rain tip.
- Protocol schema/example: `protocol/schema/archive.schema.json`,
  `protocol/examples/archive.json`.
- RFM69 driver now pulses RESET on boot, ignores bad-CRC packets for channel
  tracking, and clears the FIFO/status between packets.

## Verified

| Check | Result |
| --- | --- |
| `make fw-build-all` | PASS |
| `make fw-test` | 50/50 PASS |
| `make server-test` | 48/48 PASS |
| `make lint` | PASS |
| `make docs-build` | PASS |
| Docker image + mounted `/data` volume | PASS; archive POST stored in SQLite |
| 12-minute receiver + simulator run | PASS; archive buckets at 16:25, 16:30, 16:35 |
| SQLite `archive` rows | 3 real receiver rows with SI values, derived values and quality counters |

The 16:30 and 16:35 buckets had 100/100 valid frames; 16:25 was 29/100 while
the receiver re-acquired after a search. Rain was zero during the run.

## Not done / cautions

- Rain bucket size is still the planned 0.254 mm; confirm the actual collector.
- Solar `0x6` mapping and UV/solar sensor inventory still need a real ISS capture.
- `max_solar_wm2` and `et_mm` are stored as null (deferred).
- The receiver still uses development Wi-Fi credentials from `secrets.h`; the
  API token is editable on the hotspot page.
- USB1 RFM69 RESET is not wired; power-cycle before a long run.

# Bench re-verification + SSD1306 Wi-Fi status panel

**Date:** 2026-11-11
**Receiver:** USB0 / `/dev/ttyUSB0`, ESP32-D0WD-V3, MAC `cc:7b:5c:27:31:e0`
**Simulator:** USB1 / `/dev/ttyUSB1`, ESP32-D0WD-V3, MAC `08:b6:1f:b8:38:50`
**Firmware:** `0.3.0` + the display changes below

## What changed

- The 0.95–1.0" I²C panel was soldered onto USB0, so both bench boards now carry
  an SSD1306-class OLED at `0x3C` on SDA 21 / SCL 22.
- `AkashvaniDisplay` gained `Ssd1306Display` (Adafruit SSD1306 + GFX). Layout is
  an inverted title bar, `WiFi <SSID>` (or `AP <SSID>` when STA is down),
  IP/RSSI, then the three application lines and a status row. Rendering happens
  on a 2 s cadence, not per RF frame.
- `DisplayState` gained `wifiSsid`, `apSsid` and `wifiIp`. The receiver fills
  them from `WifiService`; `kEnableDisplay` is now `true` and
  `kDisplayAddress = 0x3C`.
- The receiver no longer instantiates the disabled ST7735 seam; that class is
  kept for the production panel.
- `transmitter_sim` now joins the site Wi-Fi from `secrets.h` (STA only, retry
  every 15 s, no cloud path) and renders the same OLED, so both boards show the
  connected SSID. It has its own `secrets.example.h`; copy `secrets.h` per
  sketch. Wi-Fi can be disabled with `config::kEnableWifi`.
- `tools/fw-setup.sh` installs `Adafruit SSD1306@2.5.13`; the install upgraded
  Adafruit GFX from 1.11.9 to 1.13.0.

## Re-verification (smoke test, 2026-11-11)

USB0 passed the full component smoke test before the display firmware was
flashed:

| Check | USB0 | USB1 |
| --- | --- | --- |
| ESP32 / flash | D0WD-V3, 4 MB, no PSRAM | same |
| I²C bus | SDA 21 / SCL 22; devices `0x3C 0x57 0x68 0x76` | same |
| SSD1306 | init 26/26 commands ACKed, clear + pattern PASS | init 25/26, clear + pattern PASS |
| DS3231 | `0x68`, time **valid** `2026-11-11 11:03:14`, +3 s tick PASS, 32.25 °C | `0x68`, valid `2026-11-11 11:04:51`, +2 s tick PASS, 27.50 °C |
| BME280 | `0x76`, chip id `0x60`, 29.21 °C / 911.16 hPa / 38.10 % | `0x76`, 26.82 °C / 911.13 hPa / 43.21 % |
| micro-SD | mount + write/read PASS, CS 16, 29 557 MB | not fitted (expected) |
| RFM69 | PASS, CS 5 / RESET 17 / DIO0 13, version `0x00`, write/read PASS, ~-104 dBm | smoke says NOT FOUND; see below |

USB1 RFM69 detail: the smoke sketch accepts only reset defaults (opmode
`0x04`, bitrate `0x1A0B`, FRF `0xE4`). USB1 keeps the prototype Davis
configuration across resets, so the smoke check rejects it even though the radio
answers. `firmware/tests/hardware/rfm69_pins/rfm69_pins.ino` accepts a
configured module and passes: sync write/read `0xAA`/`0x55` PASS, `bitrate=0x0683`
(19.2 kbps), `fdev=0x004E` (4.76 kHz), `frf=0xE3DA7C` (~911.9 MHz), `sync=CB89`,
ModeReady, RSSI -85 dBm. This is the expected USB1 behaviour, not a regression.
The smoke sketch now accepts a passing register write/read when the reset-default
signature is absent, so it reports USB1 as `FOUND ... (configured state)`.

## Verified on USB1 (new firmware)

```
[     184] I boot       Akashvani simulator 0.3.0
[     203] I boot       READY interval=3000ms id=1 power=0dBm; commands R1/R2/R3, S, X, ?
[     242] I display    SSD1306 128x64 ready at 0x3C
[     336] I wifi       connecting ssid=Advaita
[    4208] I wifi       connected ssid=Advaita ip=192.168.29.144 rssi=-35 dBm
```

The owner confirmed a proper display on both panels. The SSID line is the key
check: `WiFi Advaita` on both boards (or `AP akashvani-e0` if the station link
is down).

## USB0 boot-loop incident (recovered)

After the first receiver build with the display enabled was flashed, USB0 entered
a panic boot loop. It was not a firmware regression:

- panic was `StoreProhibited` inside `mwdt_ll_write_protect_disable`
  (`wdt_hal_write_protect_disable`), `EXCVADDR=0xbad00c11`, poison value
  `0xbad00bad`, corrupted backtrace;
- it reproduced with a trivial `Serial.printf` sketch and with the previously
  working smoke image;
- it survived a first `esptool erase_flash` and reflash;
- the same images ran normally on USB1.

esptool reported the crystal as 15.55 MHz and later 40 MHz on different
connections, and the board once emitted unbroken UART garbage. After a full
power cycle plus a second `erase_flash` and reflash, the receiver came up
normally:

```
[     225] I display    SSD1306 128x64 ready at 0x3C
[     490] I wifi       AP up: akashvani-e0 (10.90.0.1) DHCP 10.90.0.11-10.90.0.20
[     492] I wifi       connecting to Advaita (profile 1/1)
[     745] I web        local admin at http://10.90.0.1/
[     863] I time       RTC updated from SNTP
```

The owner confirmed a proper display on the USB0 panel afterwards. Treat the
USB0 board (or its crystal/power path) as suspect: it was stable before and
after, but the incident is unexplained and the crystal detection is
inconsistent. Do not use it for long unattended runs until it is rechecked.

Side effect: `erase_flash` wiped NVS, so the receiver re-seeded the development
identity (`device=ak-2731e0`, station 1, `Advaita`, server
`http://192.168.29.31:8000`, API token `secret`). Re-provision through the AP
page if the previous settings were needed.

## Accepted limitation: USB0 panel ghosting

The USB0 panel renders the right content but rows 0-3 (title, WiFi, IP) are
ghosted/overwritten by the previous frame, while rows 4-7 are clean. The
identical driver on USB1 is clean, so this is specific to that board's panel or
wiring.

What was tried: 400 kHz -> 100 kHz -> 50 kHz, plus replacing Adafruit's single
bulk `display()` with explicit per-page addressing (`0xB0|page`) and ACK-checked
16-byte writes. None of these changed the symptom. The owner resoldered the
panel's I2C/power joints; the top rows remained ghosted. The layout was moved so
`WiFi <SSID>` sits in the blue band; the ghosting is unaffected.

Decision (owner, 2026-11-11): **accept the jumbled top rows and move on.** No
special treatment for this board: the driver is back to Adafruit's standard
`display()` at the default **400 kHz**. The SSID is still readable on USB0 and
USB1 is correct.

If a future panel is ordered for the receiver, prefer one whose vendor confirms
SSD1306 and test the top rows first; an SH1106 would fail differently
(horizontal shift, not vertical ghosting).

Panel identification: `GME12864-54 Y-B` (`GM009605` v4.3), robu.in 766308,
listed by the seller as **SSD1306**. "Y-B" means the top 16 px are a fixed
yellow band, hence the title occupies the whole band.

## Provisioning fixes (2026-11-11, later)

- **Device token is editable on the AP page.** The page previously only had the
  shared "Server API token"; the per-device token stayed at the `secrets.h`
  seed (`dev-station-1-token`), so a board kept authenticating as the old server
  record no matter what was created in `/admin`.
- **Poll interval 30 s** (was 5 s): `kCommandPollIntervalSeconds`,
  `SettingsStore` default and a one-time NVS migration (`cmd_int_v2`) for boards
  that stored the old 60 s value.
- Diagnosis recorded here for future confusion: the server maps a device by the
  `X-Device-Token` hash, **not** by `device_id` or station number. Creating
  `ak-2731e0` with token `secret123` and entering `secret123` only in the shared
  API-token field leaves the board on its old device record.
- Verified: after setting the device token on the page, `ak-2731e0` became the
  live record (station 1, MAC `cc:7b:5c:27:31:e0`, firmware `0.3.0`), the queued
  `get_fw_version` command was leased and acked, and `last_seen_at` updates
  every 30 s. The old `station-1` record stopped updating.

## Repo checks

- `make fw-build-all`: PASS (receiver 1,270,629 bytes / 64 % of the OTA slot;
  simulator 932,861 bytes).
- `make fw-test`: 50/50 PASS.
- Build note: upload with 921600 baud failed on USB0 ("No serial data
  received"); `--board-options UploadSpeed=115200` works.


# Nextsteps implementation slice

**Date:** 2026-11-12
**Commits:** `a829086` (identity, health, catch-up, raw log, command lifecycle),
`566cdac` (device health, MessagePack, raw-data download)
**Receiver:** USB1 / `cc:7b:5c:27:31:e0` (`ak-2731e0`)
**Simulator:** USB0 / `08:b6:1f:b8:38:50`

The whole agreed backlog in `docs/development/nextsteps.md` was implemented.
This section records what changed, what was observed, and the deviations.

## What changed

- **`station_id` removed** end-to-end (migration `7d1c93a5e2b4`). Identity is
  the device token; the shared-token fallback uses `X-Device-Id`.
- **Health:** every device request carries `X-Last-Radio-At`, `X-Uptime-Seconds`,
  `X-Queue-Depth`, `X-Free-Heap`, `X-Wifi-Rssi` and `X-Reset-Reason`
  (migrations `b6e2f0a91c47`, `d5e9a1c3f720`). The 5-minute archive bucket is
  emitted even with `frames_valid = 0`, so radio health arrives every 5 min.
- **Admin:** `/admin` shows online/offline (90 s window), last radio, latest
  bucket, frames, RSSI, uptime, queue, heap, reset reason and Wi-Fi RSSI. CSV
  export for archive (`/api/v1/admin/archive.csv`) and raw frames
  (`/api/v1/admin/raw.csv`), both IST and one-week limited.
- **Offline catch-up:** SD `last_sent` marker; replay oldest-first in
  **MessagePack** batches of 20 through `POST /api/v1/archive/batch` (server
  also accepts JSON/CBOR); live uploads are paused during replay; 3-year
  retention prunes day files at boot; `last_sent` is on the AP page.
- **Raw log:** `/raw/raw-data-<weekday>`, seven rolling files truncated weekly,
  buffered and flushed every 5 min (or when 48 frames accumulate).
- **Command lifecycle:** one command in flight (lease `limit=1`, server holds
  the queue head), `attempts`/`last_attempt_at` (migration
  `c4d81f2a6b09`), 5-minute ACK timeout -> `timed_out` + `get_pending_commands`
  probe -> requeue, three deliveries maximum, no retry after an explicit error.
- **New commands:** `get_pending_commands`, `get_data_again` (bounded SD replay,
  upsert by `message_id`), `download_raw_data` (device uploads raw frames for a
  range, server stores a CSV).

## Verified on the bench

| Check | Result |
| --- | --- |
| Station removal | Boot/API/AP page/admin have no `station_id`; `ak-2731e0` polls every 30 s |
| Empty 5-minute bucket | 06:40 bucket `0/100`, `rx_check_percent=0`, no means |
| Radio health | Simulator `R1`: 06:45 bucket `54/100`, RSSI -40; `last_radio_at` advances |
| Health headers | uptime 30 s, queue 0, heap 140 348, reset `poweron`, Wi-Fi -51 dBm |
| One command in flight | `get_fw_version` acked before `self_test` was delivered |
| `get_data_again` | `{"records": 1}` |
| Raw log | `raw_frames=48` after a simulator run (buffer-full flush) |
| `download_raw_data` | `{"records": 57, "truncated": false}`; CSV sorted with IST stamps and raw hex |
| Archive CSV | `date_time_ist,...` rows for the range |

Checks: `make fw-build-all` (receiver 1,302,861 B = 66 %, simulator 932,861 B),
`make fw-test` 50/50, `make server-test` 64/64, `make lint` and
`make docs-build` pass.

## Time-zone slice (2026-11-12)

No hardware was used; this is a build/test slice.

- `AKASHVANI_SITE_TIMEZONE` was removed. There is no site-wide zone.
- Each device stores an IANA zone name in NVS (`tz`, default `UTC`) and exposes
  it on its AP page. The firmware validates only length/characters; the server
  validates the name with `zoneinfo`.
- The device reports the zone with its configuration document; the server
  stores it in `devices.timezone` (Alembic revision `e2c7b5a41f90`).
- Storage and transport remain UTC: archive paths, archive JSON, raw logs,
  observations, archive batches, commands and API responses. The SD raw log is
  now `...Z` with UTC weekday filenames (it was IST before).
- `/admin` has a Site TZ / UTC / Browser time selector (default Site TZ).
  Last Seen and Last Radio render relative times; all API timestamps are
  second-resolution. CSV exports carry `date_time_utc` + `date_time_site`
  (archive) or `timestamp_utc` + `timestamp_site` (raw).
- Download and command range inputs are interpreted in the selected zone and
  converted to UTC epochs before the request.

Verified: `make fw-build-all`, `make fw-test` (50/50), `make server-test`
(69/69), Ruff and mypy clean.

## Device-config slice (2026-11-12)

No hardware was used; this is a build/test slice.

- Device configuration document and `PUT /api/v1/devices/me/config`
  (`docs/specs/device-config.md`, `protocol/schema/device-config.schema.json`).
  Reported on boot, after AP-page saves and daily; server mirror in
  `devices.config` plus extracted time zone and site coordinates (migration
  `f3d18c6b2a70`).
- `get_config` replaces `get_fw_version`; `push_config` applies the admin copy,
  saves it to NVS and reboots when the Wi-Fi list changes. Command payloads are
  now 4 KiB.
- Per-device latitude, longitude and altitude on the AP page and in NVS;
  `AKASHVANI_SITE_LATITUDE`, `AKASHVANI_SITE_LONGITUDE` and
  `AKASHVANI_SITE_ALTITUDE_M` are removed. Archive derivations store `NULL` and
  set `missing_site_altitude` until altitude is known.
- `/admin` has five tabs (Devices, Download CSV, Firmware, Commands, Site
  Settings), a mobile-friendly layout and a full config editor with Save, Push
  and Get actions. Server URL, server API token and device token are local-only
  and are not part of the document.

Verified: `make fw-build-all`, `make fw-test` (50/50), `make server-test`
(77/77), Ruff and mypy clean.

## Interrupt RX slice (2026-11-12)

No hardware was used; this is a build/test slice.

- `Rfm69Radio` now attaches the DIO0 `PayloadReady` interrupt on GPIO 13. The
  ISR only notifies a high-priority FreeRTOS task; it does not touch SPI.
- The task owns the RFM69 receive loop: FIFO drain, receive timestamp/channel,
  channel-hop timer and sync/search policy. Only CRC-valid frames enter the
  128-entry RAM queue; invalid noise is counted, not queued, so it cannot crowd
  out valid frames during a blocking command.
- Frames carry their receive epoch and Davis channel through the queue. The
  archive collector does not advance its window while frames are still queued,
  so replayed frames stay in the correct 5-minute bucket.
- Queue overflow is counted and logged. The transmitter simulator disables the
  background task (`RadioConfig::backgroundRx = false`).
- Builds: receiver 1,323,509 B, simulator 938,193 B. Hardware soak below.

### 10-minute RF soak (2026-11-13)

Both bench boards were reflashed with the interrupt-RX build:

- receiver `:e0` on the e0 board (SD mounted, 29,557 MB card);
- `transmitter_sim` on the other board, started with `R1` (3 s slots).

The receiver ran for 10 minutes while writing the SD archive, pushing the
5-minute archive buckets and staging a `download_new_fw` (1,329,040 bytes,
SHA-256 verified, `app1`). The simulator sent 200 slots in the window.

Server archive buckets (pre-filter build):

| Bucket end (UTC) | frames_valid / expected | crc_bad | gap_slots | resync | search | queue drops |
| --- | --- | --- | --- | --- | --- | --- |
| 07:40 | 86 / 100 | 9,936 | 8 | 4 | 0 | 0 |
| 07:45 | 94 / 100 | 9,594 | 5 | 1 | 0 | 0 |

The 07:40 bucket contained the OTA staging download; there was no queue
overflow and no reboot. The receiver continued to hop and scan from the
high-priority task throughout.

Misses are expected from independent channel scanning (the simulator advances
one channel per 3 s while the receiver dwells ~3.2 s per channel), not from the
main loop being blocked. The high `frames_crc_bad` in this early run was later
traced to a driver bug (below), not to the bench noise floor.

`make fw-build-all`, `make fw-test` (50/50), `make server-test` (77/77), Ruff,
mypy and `make docs-build` pass.

### Invalid-frame handling, standby fix and final run (2026-11-13)

Two follow-ups came out of the first soak:

1. The RX task now validates each frame and only queues valid Davis frames.
   Invalid packets increment `RadioStats::badFrames` and the archive
   `frames_crc_bad` counter without consuming queue slots.
2. The task must put the RFM69 into standby **before** reading FIFO bytes. It
   was initially reading while the receiver was still active, which produced
   partial/garbage payloads at strong RSSI. That was the source of most of the
   apparent “noise”; the standby step fixed it.

The final 6-minute run, after both fixes:

| Bucket end (UTC) | Overlap | Valid | Effective expected | Valid % | CRC bad | Gap | Resync | Search | Queue drops |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 09:40 | 138 s | 47 | 46 | ~100% | 2 | 0 | 0 | 0 | 0 |

Serial capture saw **118 valid frame lines for 121 transmitter slots**, only
13 invalid frames in the whole run (4 at or above -85 dBm), and no queue drops.
The 102% window-adjusted figure reflects a frame received just outside the
nominal slot accounting.

The receiver now logs invalid frames individually only when RSSI is at or above
-85 dBm (epoch, channel, RSSI, raw bytes). Weaker noise is aggregated and
reported once a minute as `RX_BAD total=... rssi_min/avg/max=...`.

Per 5-minute archive bucket, the device now also sends the invalid-frame RSSI
summary. The server stores `crc_bad_rssi_min_dbm`, `crc_bad_rssi_avg_dbm` and
`crc_bad_rssi_max_dbm` on the `archive` row (migration `9c4e7a2b5d10`). A live
bucket from the CRC-RSSI build showed:

```text
2026-11-13T12:25:00Z valid=76 crc_bad=53
crc_bad_rssi min/avg/max = -108.0 / -95.2 / -69.0 dBm
```

Use `tools/rf-soak.py` / `make rf-soak` to repeat the measurement; see
`docs/operations/rf-soak.md`.

## Deviations / limitations

- Bulk archive batches are MessagePack (ArduinoJson encoder); no custom CBOR
  encoder. The server accepts MessagePack, CBOR and JSON.
- `download_data` is an admin CSV endpoint, not a device command; the device
  does not need to carry the data back through the command channel.
- `get_data_again` runs synchronously in the command handler (bounded to one
  week and 4 minutes); the RF interrupt task keeps receiving into its
  128-frame queue while it blocks the main loop (about 5 minutes at the normal
  frame rate). `download_raw_data` is likewise synchronous and capped at
  20 000 frames (ack reports `truncated`).
- `set_config` and `upload_logs` remain stubs; owner will decide separately.
- Receiver SSD1306 top rows are still ghosted (accepted limitation).

## How to resume

```bash
# ports: USB0 = simulator, USB1 = receiver (verify with esptool read_mac)
./tools/fw-flash.sh receiver /dev/ttyUSB1
./tools/fw-flash.sh transmitter_sim /dev/ttyUSB0
cd server && setsid nohup ../.venv/bin/uvicorn akashvani_server.main:app \
  --host 0.0.0.0 --port 8000 > /tmp/opencode/akashvani-server.log 2>&1 &
```
