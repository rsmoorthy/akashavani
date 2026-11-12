# AGENTS.md

Instructions for AI agents and developers resuming work on this repository.

## Project

Akashvani targets an ESP32-S3 system for receiving Davis weather-station RF
data, archiving it to micro-SD, and uploading it to a Python/FastAPI server.
The two current bench boards are ESP32-D0WD-V3 (classic); one acts as the
receiver and one simulates a Davis transmitter for bench testing.

Read first:

1. `README.md`
2. `docs/index.md`
3. `docs/architecture/firmware.md`
4. `docs/development/handoff.md` — current state and the agreed plan
5. `docs/development/nextsteps.md` — agreed backlog (owner-observed gaps)

## Current status

The scaffold is committed and validated **without production hardware**. A
bench smoke test of the two ESP32 dev boards was run on 2026-10-26; see
`docs/development/handoff.md`. The firmware builds, the native and server test
suites pass, and the Docker image runs. The real RF and Davis implementations
are not ported yet; they are explicit stubs:

| Seam | File |
| --- | --- |
| RFM69 driver (current hardware) | `firmware/libraries/AkashvaniRadio/src/Rfm69Radio.cpp` |
| RFM95 modem config / FIFO / IRQ / transmit (alternate seam) | `firmware/libraries/AkashvaniRadio/src/Rfm95Radio.cpp` |
| Davis frame decode | `firmware/libraries/AkashvaniDavis/src/DavisDecoder.cpp` |
| Davis frame encode | `firmware/libraries/AkashvaniDavis/src/DavisEncoder.cpp` |
| ST7735 display | `firmware/libraries/AkashvaniDisplay/src/St7735Display.cpp` |
| TLS pinning / OTA signature + rollback | `AkashvaniNet/src/OtaService.cpp`, `ApiClient.cpp` |

OTA download is now a two-step command flow (`download_new_fw` stages into the
inactive slot and verifies SHA-256; `boot_with_new_fw` selects it and restarts).
This was exercised end-to-end on USB1. TLS pinning, image signatures and
bootloader rollback are still not implemented; do not call OTA production-ready.

Do not describe these as working. Do not invent register values, RF parameters,
Davis framing or pin assignments.

## Agreed working method

Work on **one vertical slice at a time**. The user supplies the spec, working
reference code, test vectors and acceptance criteria before porting begins.

Order:

1. RFM69 receive + Davis decode
2. Davis encode + simulator transmit
3. RTC/SNTP + SD archive
4. Wi-Fi AP+STA + local admin
5. Cloud upload + retry/idempotency
6. Commands + OTA hardening
7. Display / touch UI

For each slice:

- write or update a short spec under `specs/` (create the directory when the
  first spec arrives) or `docs/`;
- list acceptance criteria and required evidence (serial log, SD sample, curl
  response, photo for visual parts);
- make one focused branch/commit;
- run the relevant host tests and both firmware builds;
- report what was actually observed versus what is assumed.

Do not batch-implement all remaining specs. Do not hide unverified assumptions.

## Hardware protocol

- The user has a receiver board and a transmitter board (RFM69) and will
  connect them for bench work.
- Before flashing, confirm the serial ports and get explicit permission.
- The agent can run `arduino-cli` build/upload/monitor if ports are reachable,
  but cannot see an LCD, measure RF, press buttons or observe power.
- Ask the user for physical/visual/RF observations.
- Do not transmit without an antenna or dummy load. Confirm frequency, power and
  local regulatory constraints.
- Treat the pin map in `docs/hardware/pinmap.md` as unverified until the user
  confirms the assembled boards.

## Non-negotiable design rules

- Keep the provisioning AP available even when STA fails.
- Archive to SD before uploading; the card is the durable offline copy.
- Observations with invalid time must not be uploaded or filed under a guessed
  date.
- `message_id` is the idempotency key; reuse it on retries.
- Keep per-frame logic allocation-free and bounded.
- Keep domain/protocol logic free of Arduino headers so it remains host-testable.
- Wire-format changes require: JSON Schema + server Pydantic model + C++ constants
  + examples/tests + docs.
- Never commit secrets, site details or private captured data.
- OTA must not be considered production-ready until SHA-256/signature verification
  and rollback are implemented and tested.

## Build and test

```bash
make setup
make fw-build-all
make fw-test
make server-test
make lint
make docs-build
```

Firmware build notes:

- Use `build.defines` for additive defines; overriding `build.extra_flags`
  removes ESP32 core defines and breaks libraries.
- Local libraries are supplied with `--libraries firmware/libraries`; sketch
  profiles cannot be combined with that flag.
- Default target is 4 MB / `min_spiffs`; `AKASHVANI_FLASH_SIZE=16M` selects the
  16 MB scheme.
- `firmware/VERSION` is injected as `AKASHVANI_FW_VERSION` by the build script.

## Where to change what

- Pin/timing settings: `firmware/sketches/*/config.h` plus
  `docs/hardware/pinmap.md`.
- Reusable firmware logic: `firmware/libraries/`.
- Sketch composition/policy only: `firmware/sketches/*/app.cpp`.
- Host-safe logic tests: `firmware/tests/native/`.
- Server routes: `server/src/akashvani_server/api/`; schema in `schemas.py`,
  tables in `models.py`, migration required for schema changes.
- Wire contract: `protocol/schema/` and `docs/protocol/README.md`.
- Architecture decisions: `docs/adr/`; supersede rather than rewrite.
- Current handoff/open questions: `docs/development/handoff.md`.

## Open questions for the user

See `docs/development/handoff.md`. The important ones are:

- exact board/pin map and whether it differs from the scaffold;
- serial ports and permission to flash/monitor;
- RF bench setup and Davis RF parameters;
- 4 MB vs 16 MB final board;
- display choice (1" ST7735 vs 3.5" touch);
- license.
