# Development handoff

**Date:** 2026-10-25
**Scaffold commit:** `ab8078c` (`Initial Akashvani scaffold`)

This page records what was completed, what was verified, what is still a stub,
and the plan agreed with the project owner. Update it at each slice boundary.

## What was built

- Monorepo layout for firmware, protocol, server, docs and tooling.
- Eight Arduino libraries under `firmware/libraries/`:
  Core, Radio, Davis, Protocol, Storage, Net, Rtc, Display.
- Two thin sketches: `receiver` and `transmitter_sim`.
- RFM95/RFM69 seams, Davis decoder/encoder seams, deterministic Davis
  simulator, SD JSONL archive, NVS provisioning, AP+STA Wi-Fi, local admin,
  cloud upload queue, command polling, LAN and HTTP-pull OTA seams.
- Native C++ tests (CMake + GoogleTest) and an on-target bring-up sketch.
- FastAPI server with device-token auth, idempotent JSON/CBOR ingest, command
  lease/ack, firmware release/check/download and a small admin API.
- SQLite development database, PostgreSQL option, Alembic initial migration.
- Docker image and compose files for local development.
- JSON Schemas, examples, architecture/operations docs and ADRs.

## Verified on this machine

No device was attached during scaffold work.

| Check | Result |
| --- | --- |
| `make fw-test` | 36/36 native tests pass |
| receiver build | compiles, ~1.18 MB of 1.9 MB OTA slot |
| transmitter simulator build | compiles |
| bring-up sketch build | compiles |
| `make server-test` | 34/34 tests pass |
| `make lint` | Ruff + mypy pass |
| `make docs-build` | strict MkDocs build passes |
| Docker image + compose | builds, migrates, health/ingest verified |
| OTA export | binary + SHA-256 produced |

Unverified: all real hardware behaviour, RF reception/transmission, SD writes on
a real card, RTC validity on a real module, display output, TLS and OTA on a
device.

## Explicit stubs / gaps

| Area | Path | What is missing |
| --- | --- | --- |
| RFM95 | `AkashvaniRadio/src/Rfm95Radio.cpp` | modem registers, bitrate/deviation/bandwidth, FIFO, IRQ, raw mode, transmit |
| RFM69 | `AkashvaniRadio/src/Rfm69Radio.cpp` | entire SX1231 driver |
| Davis decode | `AkashvaniDavis/src/DavisDecoder.cpp` | CRC/sync, field extraction, range checks |
| Davis encode | `AkashvaniDavis/src/DavisEncoder.cpp` | frame construction for simulator |
| Display | `AkashvaniDisplay/src/St7735Display.cpp` | reset/init/draw; panel disabled by default |
| TLS | `AkashvaniNet/src/ApiClient.cpp`, `OtaService.cpp` | CA pinning, remove `setInsecure()` |
| OTA security | `AkashvaniNet/src/OtaService.cpp` | streaming SHA-256 verification, signatures, rollback |
| Command set | `receiver/app.cpp` | `set_config` and `upload_logs` return unsupported |
| Retention | `AkashvaniStorage` | no card cleanup policy |

## Agreed collaboration model

The owner will:

1. connect the receiver and transmitter boards (transmitter has RFM95 only);
2. provide specs per feature;
3. share the working Davis RFM95 code, register values and captured frames.

We will implement and test **one vertical slice at a time**, not a big-bang
delivery.

### Slice order

1. **RFM95 receive + Davis decode.** Receiver prints validated frames with the
   expected values; corrupt frames rejected. Compare against reference captures.
2. **Davis encode + simulator transmit.** Transmitter sends synthetic frames;
   receiver decodes them; values match.
3. **RTC/SNTP + SD archive.** Valid time, daily JSONL, power-cycle recovery.
4. **Wi-Fi AP+STA + local admin.** Provisioning, status, credential changes.
5. **Cloud upload + retry/idempotency.** Offline queue, duplicate handling,
   server reconciliation.
6. **Commands + OTA hardening.** Lease/ack, checksum/signature, rollback drill.
7. **Display/touch UI.** 1" panel first; larger touch panel later.

For each slice, collect:

- a short spec (create `specs/<slice>.md` when the first arrives);
- known-good reference code where applicable;
- raw test vectors/expected values;
- acceptance criteria;
- evidence: serial log, SD file sample, curl response, or user observation for
  physical/visual results.

## First slice checklist

- [ ] Confirm receiver and transmitter serial ports.
- [ ] Confirm permission to run `arduino-cli upload` / `monitor`.
- [ ] Confirm board pin map (or correct `config.h` + `docs/hardware/pinmap.md`).
- [ ] Confirm RF bench: antenna/dummy load, attenuator, distance, legal frequency
      and power.
- [ ] Receive the working RFM95 register configuration.
- [ ] Receive 3–10 raw Davis frames plus expected decoded values.
- [ ] Write `specs/rf-davis-receive.md` with acceptance criteria.
- [ ] Port the driver and decoder into the existing seams.
- [ ] Add captured frames under `firmware/tests/native/vectors/` with RF settings.
- [ ] Run receiver + reference transmitter, capture serial evidence.

## Open decisions

- Final board: 4 MB is currently viable at ~1.18 MB app size; 16 MB may be
  needed for TLS, a larger UI or future features.
- Display: keep the 1" ST7735 or move to a 3.5" touch panel.
- Frequency/band and legal power for the deployment site.
- RFM69 migration timing; the interface is ready but the driver is not.
- Whether local admin should be available on the site LAN or only the AP.
- License is still TBD.
- Production CA, OTA signing and rollback policy.

## Environment notes

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

Then read `AGENTS.md`, open the relevant spec, and start the next slice only.
Update this page with what was actually verified before moving to the following
slice.
