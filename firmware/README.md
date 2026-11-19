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
| `AkashavaniCore` | status, logging, CRC, hex, UTC time, device IDs | partly |
| `AkashavaniRadio` | `IRadio`, RFM69/RFM95 driver seams, mock radio | partly |
| `AkashavaniDavis` | observation model, codec seams, simulator | yes |
| `AkashavaniProtocol` | envelope/command contract and validators | yes |
| `AkashavaniStorage` | daily JSONL archive on micro-SD | partly |
| `AkashavaniNet` | NVS settings, AP+STA, local admin, upload, OTA | no |
| `AkashavaniRtc` | DS3231 and SNTP time service | partly |
| `AkashavaniDisplay` | display state and no-op driver | yes |

## Build

```bash
make fw-setup
make fw-build BOARD=receiver
make fw-build BOARD=transmitter_sim
make fw-test
```

The default target is the ESP32 classic bench board with an explicit 4 MB table
(1.875 MiB app slot with OTA). Set `AKASHAVANI_FLASH_SIZE=16M` for the 16 MB
scheme, or override the whole FQBN with `AKASHAVANI_FQBN`. The production board
may move to ESP32-S3; use the FQBN override for that.

## Dependency policy

External dependencies are pinned in `tools/fw-setup.sh` and documented in
[`docs/build/firmware.md`](../docs/build/firmware.md). Local libraries are never
installed globally; `arduino-cli --libraries firmware/libraries` adds them to
each build, so a checkout is self-contained.

## Size budget

The receiver currently uses **1,226,845 bytes** (62%) of the 1,966,080-byte OTA
slot, leaving about 739 KB free. A 4 MB board remains viable; the 3.5" touch UI
or certificate bundles may still force the 16 MB variant. Re-check
`make fw-build` output before committing to a fleet-wide board.
