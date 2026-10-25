# Changelog

All notable changes are recorded here. The firmware version lives in
`firmware/VERSION`; the server version is in `server/pyproject.toml`.

## [Unreleased]

### Added

- Repository scaffold: firmware libraries and two thin sketches.
- arduino-cli build/flash/monitor/test/OTA-export scripts.
- Native CMake + GoogleTest suite for Arduino-free logic.
- Hardware bring-up sketch.
- FastAPI ingest, command lease/ack, firmware release and admin APIs.
- SQLite development / PostgreSQL production configuration, Alembic migration.
- Docker image and compose files.
- JSON Schemas and examples for observations, commands, acknowledgements and OTA.
- Architecture, protocol, hardware, build, operations and ADR documentation.
- CI for firmware tests/build, server tests/lint, docs and Docker.

### Known gaps

- RFM95 modem configuration and Davis frame codec are not ported.
- Display, RFM69 and TLS/OTA security hardening are not implemented.
