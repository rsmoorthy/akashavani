# Firmware

ESP32-S3 firmware, built with `arduino-cli`. The sketches are deliberately thin;
reusable code lives in `libraries/` as Arduino libraries.

```text
firmware/
├── libraries/                 shared components (see table below)
├── sketches/
│   ├── receiver/              production receiver
│   └── transmitter_sim/       bench transmitter simulator
├── tests/
│   ├── native/                host tests (CMake + GoogleTest)
│   └── hardware/bringup/      on-target bring-up checks
├── partitions/                partition-scheme guidance
├── build/                     generated, git-ignored
└── VERSION                    release version used by OTA tooling
```

## Libraries

| Library | Responsibility | Host-testable |
| --- | --- | --- |
| `AkashvaniCore` | status, logging, CRC, hex, UTC time, device IDs | partly |
| `AkashvaniRadio` | `IRadio`, RFM95 driver seam, mock radio | partly |
| `AkashvaniDavis` | observation model, codec seams, simulator | yes |
| `AkashvaniProtocol` | envelope/command contract and validators | yes |
| `AkashvaniStorage` | daily JSONL archive on micro-SD | partly |
| `AkashvaniNet` | NVS settings, AP+STA, local admin, upload, OTA | no |
| `AkashvaniRtc` | DS3231 and SNTP time service | partly |
| `AkashvaniDisplay` | display state and no-op driver | yes |

## Build

```bash
make fw-setup
make fw-build BOARD=receiver
make fw-build BOARD=transmitter_sim
make fw-test
```

The default target is a 4 MB ESP32-S3 using the stock `min_spiffs` partition
scheme (1.9 MB app slot with OTA). Set `AKASHVANI_FLASH_SIZE=16M` for the 16 MB
scheme, or override the whole FQBN with `AKASHVANI_FQBN`.

## Dependency policy

External dependencies are pinned in `tools/fw-setup.sh` and documented in
[`docs/build/firmware.md`](../docs/build/firmware.md). Local libraries are never
installed globally; `arduino-cli --libraries firmware/libraries` adds them to
each build, so a checkout is self-contained.

## Size budget

The receiver currently uses about 1.2 MB of a 1.9 MB OTA slot with the scaffold
and no TLS pinning. A 4 MB board is viable, but the 3.5" touch UI or certificate
bundles may force the 16 MB variant. Re-check `make fw-build` output before
committing to a fleet-wide board.
