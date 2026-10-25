# Akashvani

Akashvani is a weather-data pipeline built around a Davis weather station:

- an **ESP32-S3 receiver** listens to the station's RF transmissions, decodes
  them, archives every observation to a micro-SD card, and uploads canonical
  JSON/CBOR to a server;
- a **transmitter simulator** board sends synthetic Davis-shaped observations
  so the receiver and cloud path can be tested before going onsite;
- a **cloud server** (FastAPI + SQLAlchemy, Dockerised) ingests observations,
  queues commands, and serves firmware releases.

!!! warning "Scaffold status"
    The build, test and deployment skeleton is in place. The validated
    prototype's RFM95 driver and Davis frame codec are **not yet ported**; the
    seams are marked `TODO(port)`. There is no hardware attached to the
    development machine. Treat every pin assignment and register value as
    unverified until the bring-up checklist passes.

## Start here

| I want to... | Read |
| --- | --- |
| Understand the system | [Architecture overview](architecture/overview.md) |
| Build and flash firmware | [Firmware build](build/firmware.md) |
| Run the server locally | [Server build](build/server.md) |
| Provision a board | [Provisioning](operations/provisioning.md) |
| Understand the wire format | [Protocol](protocol/README.md) |
| Port the prototype | [Roadmap](development/roadmap.md) |
| Check pins before wiring | [Pin map](hardware/pinmap.md) |

## Repository at a glance

```text
firmware/   Arduino libraries + two thin sketches + tests
protocol/   JSON Schemas and examples shared by firmware and server
server/     FastAPI ingest/command/OTA service, Docker and Alembic
docs/       This site
tools/      Build, flash, test and export scripts
```

## Design constraints

- **ESP32-S3, 4 MB flash** now; 16 MB is a documented upgrade path. OTA and a
  future touch UI make flash headroom the main hardware risk.
- **Concurrent AP + STA Wi-Fi.** The AP is always available for provisioning and
  local admin, even when the site network is down.
- **Offline-first.** The SD archive is written before upload; a cloud outage
  must not lose observations.
- **One loop.** The firmware uses a cooperative `loop()` with bounded waits.
  Long blocking operations (SNTP, OTA) are called out explicitly.
- **Versioned contract.** `protocol/` is the source of truth; incompatible
  changes require a new protocol version.
- **Test without hardware.** Protocol, time, CRC, archive paths and the weather
  simulator run in native C++ tests; the server has an integration-style test
  suite.

## Known gaps

The scaffold deliberately leaves the following unimplemented:

- Davis RF decoding/encoding and the RFM95 modem configuration.
- Real SD-card and display drivers (the SD writer is implemented but untested on
  hardware; the display is a no-op seam).
- TLS certificate pinning and OTA image signature/hash verification.
- RFM69 support and raw/direct-mode reception.
- Authentication hardening for the local admin UI beyond a shared password.

See [Roadmap](development/roadmap.md) for the proposed order of work.
