# Testing

## Test pyramid

| Layer | Tool | Hardware | Command |
| --- | --- | --- | --- |
| Pure firmware logic | CMake + GoogleTest | no | `make fw-test` |
| Firmware compile | arduino-cli | no | `make fw-build-all` |
| Hardware bring-up | Arduino sketch | yes | `make fw-test-hw`, then flash |
| API and schema | pytest + TestClient | no | `make server-test` |
| Dashboard logic and components | Vitest + Testing Library | no | `make dashboard-test` |
| Container | Docker build/run | no | `make docker-build`, `make docker-up` |
| End-to-end RF | receiver + simulator + server | yes | manual, see below |

## Native firmware tests

```bash
make fw-test
```

This configures `firmware/tests/native` with CMake, builds, and runs CTest. The
first configure fetches GoogleTest 1.14.0 if it is not installed. Covered today:

- CRC-16 CCITT-FALSE / XMODEM / ARC known vectors;
- hex conversion and error paths;
- UTC ISO-8601 format/parse, leap day and invalid dates;
- device ID validation and MAC derivation;
- Davis decoder length rejection, simulator determinism/bounds, validity mask;
- protocol envelope and command-type validation;
- archive path layout;
- mock radio queue/transmit behaviour.

Only Arduino-free files are compiled. If a test needs `Arduino.h`, the logic is
in the wrong layer.

### Adding a test

1. Put pure logic in a library file with no Arduino include.
2. Add the source to `firmware/tests/native/CMakeLists.txt`.
3. Add a `test_*.cpp` with GoogleTest cases.
4. Include captured Davis frames under `vectors/` with the RF settings recorded.

## Dashboard tests

```bash
make dashboard-test     # Vitest + TypeScript
make dashboard-lint     # ESLint
make dashboard-build    # production build
```

The frontend suite covers range parsing and URL state, station-zone
date/time formatting, timeline selection and cumulative rain, API URL
construction, metric labels, Summary/History tables and widget rendering with
missing values. Browser acceptance is manual/Playwright against a populated
database; see [Public dashboard](dashboard.md).

## Firmware compile checks

```bash
make fw-build-all
```

CI builds both sketches. A successful native test run does not prove the
Arduino libraries compile, so keep both.

## Hardware bring-up

```bash
make fw-test-hw
make fw-flash BOARD=bringup PORT=/dev/ttyACM0
make fw-monitor PORT=/dev/ttyACM0
```

`make fw-monitor` passes the ESP32 FQBN because the bench boards report as
"Unknown" and `arduino-cli monitor` refuses to open an unidentified port.
Log lines are CRLF-terminated, so plain `minicom -D /dev/ttyUSB0 -b 115200` and
`screen` render correctly.

To capture the boot banner, press the board's **EN/reset button** rather than
unplugging USB. A USB power cycle also re-enumerates the CP2102 serial bridge;
the open monitor then sees a dead/garbled port and must be restarted, by which
time boot has finished. If you do power-cycle, expect bootloader junk and reopen
the console.

Both sketches hold their first log line for **3 s** after power-up
(`config::kSerialStartupDelayMs`) so a console started just after applying power
still catches the application banner. The ROM bootloader text printed before
that delay cannot be held.

The bring-up sketch scans I²C, mounts the SD card, probes the RFM69 version
register and scans Wi-Fi. It does not transmit. Record the output in the hardware
notes when a board is first assembled.

The separate bench smoke test under `firmware/tests/hardware/smoke/` discovers
the actual pins, checks the RFM69/SD/I²C devices and prints a summary. The
current bench boards are ESP32-D0WD-V3, so build it with
`esp32:esp32:esp32`, not the S3 FQBN:

```bash
arduino-cli compile --fqbn 'esp32:esp32:esp32:FlashSize=4M,PartitionScheme=min_spiffs,PSRAM=disabled' \
  --output-dir /tmp/smoke firmware/tests/hardware/smoke
arduino-cli upload --port /dev/ttyUSB0 --fqbn 'esp32:esp32:esp32:FlashSize=4M,PartitionScheme=min_spiffs,PSRAM=disabled' \
  --input-dir /tmp/smoke --protocol serial firmware/tests/hardware/smoke
arduino-cli monitor --port /dev/ttyUSB0 --fqbn esp32:esp32:esp32 --config baudrate=115200
```

`firmware/tests/hardware/rfm_scan/` is a read-only SPI/CS scanner for when the
radio does not answer on the expected pins. Neither sketch transmits.

The Davis RFM69 path is now exercised directly, without the server or SD card:

```bash
# receiver on USB0, simulator on USB1
make fw-flash BOARD=receiver PORT=/dev/ttyUSB0
make fw-flash BOARD=transmitter_sim PORT=/dev/ttyUSB1

# 15-minute continuous, random-skip and restart scenarios
make rf-bench DURATION=900

# or one scenario
make rf-bench SCENARIO=1 DURATION=900

# summarise the produced logs
make rf-bench-report RX_LOG=/tmp/opencode/rf-bench/<stamp>-receiver.log \
  TX_LOG=/tmp/opencode/rf-bench/<stamp>-transmitter.log
```

The receiver runs `config::kRfTestMode` and logs `RX`, `RX_BAD`, `HOP`,
`SEARCH`, `RESYNC` and `STATS`. The simulator accepts `R1`/`R2`/`R3`, `S`, `X`
and `?` over serial. See `specs/rf-davis-receive.md` for acceptance criteria and
the required evidence.

Once the archive/cloud slice is active, the end-to-end RF test is:

1. flash `transmitter_sim` on the simulator board;
2. flash `receiver` on the receiver board with `kRfTestMode = false`;
3. run the local server;
4. confirm observations appear in `GET /api/v1/admin/observations`;
5. confirm daily JSONL files appear on the SD card;
6. power-cycle the server and confirm the receiver retries/queues without losing
   the archived copy.

## Server tests

```bash
make server-test
```

The suite uses a temporary SQLite database per test and a full FastAPI lifespan,
including device seeding. It covers:

- health/readiness;
- JSON and CBOR ingestion, validation and idempotent retries;
- device/admin authentication and token rotation;
- command enqueue → lease → ack, including error cases;
- firmware registration, version check and download;
- JSON Schema validation of `protocol/examples/`.

The protocol test uses `protocol/schema` and `protocol/examples` from the repo
root, so contract drift fails CI.

## Lint and types

```bash
make server-lint
```

Ruff enforces import order, common bugs and formatting; mypy checks the server
package. Python tests are type-annotated where practical.

C++ formatting uses `.clang-format`:

```bash
clang-format -i firmware/libraries/**/src/*.cpp firmware/libraries/**/src/*.h
```

(Not yet enforced in CI; install `clang-format` and add a check when the ported
code lands.)

## Continuous integration

`.github/workflows/ci.yml` runs:

1. native firmware tests;
2. both firmware builds with a cached ESP32 core;
3. server Ruff + mypy + pytest;
4. Docker image build.

Hardware tests are manual by definition. The simulator/receiver end-to-end test
should become an automated bench job when a self-hosted runner with boards is
available.

## What is not covered yet

- RF timing, sensitivity and packet loss.
- SD wear and power-loss recovery.
- Display and touch UI.
- TLS certificate validation, OTA signing and rollback drills.
- Long-duration memory/queue behaviour.

Add these as hardware-in-the-loop tests rather than weakening the host tests.
