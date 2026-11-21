# Akashavani

Akashavani receives weather data broadcast by a Davis weather station over RF,
archives it locally, and forwards it to a server. It also contains a
transmitter simulator for bench testing the receiver before deployment.

> **Project status: scaffold.** The repository structure, build, test and
> deployment machinery are in place. Command polling and two-step OTA have been
> bench-tested on USB1. Hardware drivers and the Davis RF protocol implementation
> are intentionally stubbed; the validated prototype code will be ported into the
> library seams described below. See
> [`docs/development/roadmap.md`](docs/development/roadmap.md).

## Repository layout

```text
.
├── firmware/                 ESP32 Arduino code (S3 target; bench boards classic)
│   ├── libraries/            reusable components; the bulk of the firmware
│   │   ├── AkashavaniCore     logging, CRC, time and small utilities
│   │   ├── AkashavaniRadio    radio abstraction (RFM69 current hardware, RFM95 seam retained)
│   │   ├── AkashavaniDavis    Davis frame decoding/encoding + transmitter simulator
│   │   ├── AkashavaniProtocol versioned wire contracts (JSON/CBOR)
│   │   ├── AkashavaniStorage  micro-SD archive writer
│   │   ├── AkashavaniNet      Wi-Fi AP+STA, HTTP API, local admin, OTA
│   │   ├── AkashavaniRtc      DS3231 + NTP timekeeping
│   │   └── AkashavaniDisplay  display abstraction (1" LCD now, larger panel later)
│   ├── sketches/
│   │   ├── receiver/         production receiver firmware (thin .ino)
│   │   └── transmitter_sim/  Davis transmitter simulator (thin .ino)
│   ├── tests/                native host tests and on-target bring-up sketches
│   ├── partitions/           reference partition tables (4 MB / 16 MB)
│   └── VERSION
├── protocol/                 JSON Schemas and example payloads (shared contract)
├── server/                   Python/FastAPI cloud endpoint, Dockerised
│   └── dashboard/            React/TypeScript public weather dashboard
├── docs/                     architecture, hardware, protocol and runbooks
├── tools/                    developer scripts invoked by the Makefile
└── .github/workflows/        CI
```

## Quick start

Prerequisites: `arduino-cli` (1.0+), `python3.11+`, Node.js 24, `git`, and
optionally Docker. No hardware is required to build or run the test suites.

```bash
make setup                 # ESP32 core, Python venv, dashboard npm packages
make fw-build BOARD=receiver
make fw-build BOARD=transmitter_sim
make fw-test               # native C++ tests (CMake + GoogleTest)
make server-test           # FastAPI tests
make dashboard-test        # dashboard unit tests + TypeScript
make dashboard-build       # build the dashboard served at /dashboard/<slug>/
make server-dev            # http://localhost:8000/docs (foreground; Ctrl-C)
make server-stop           # stop a backgrounded server on AKASHAVANI_PORT
make docker-up             # Dockerised server on :8000
```

Flashing and monitoring need a connected board:

```bash
make fw-flash BOARD=receiver PORT=/dev/ttyACM0
make fw-monitor BOARD=receiver PORT=/dev/ttyACM0
```

## Documentation

Start at [`docs/index.md`](docs/index.md), or build the docs site with
`make docs-serve` (requires MkDocs, installed by `make server-setup`).

- [Architecture](docs/architecture/overview.md)
- [Hardware and pin map](docs/hardware/README.md)
- [Protocol](docs/protocol/README.md)
- [Public dashboard](docs/build/dashboard.md)
- [Firmware build](docs/build/firmware.md)
- [Server and Docker](docs/build/server.md)
- [Testing](docs/build/testing.md)
- [Provisioning](docs/operations/provisioning.md)
- [OTA](docs/operations/ota.md)
- [Current handoff and next slice](docs/development/handoff.md)

## Ground rules for porting the prototype

Before changing firmware, read [`AGENTS.md`](AGENTS.md) and the
[development handoff](docs/development/handoff.md). The project is built one
vertical slice at a time using the owner's specs and validated RF reference code.

1. Hardware access belongs in a library under `firmware/libraries/`; a sketch
   only wires components together.
2. Protocol decoding, encoding and state machines must stay free of Arduino
   headers where practical, so they can be tested on the host.
3. Wire-format changes require a schema change in `protocol/` and a protocol
   version decision.
4. Secrets and site-specific credentials are never committed. Use the runtime
   settings store (NVS) or `.env` files that are git-ignored.
5. Keep `docs/` honest: mark unverified hardware assumptions explicitly.

## License

TBD. Add a `LICENSE` file before publishing this repository.
