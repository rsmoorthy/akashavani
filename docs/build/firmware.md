# Building firmware

## Prerequisites

| Tool | Version | Notes |
| --- | --- | --- |
| `arduino-cli` | 1.0+ | 1.0.1 tested |
| ESP32 core | 3.0.2 | installed by `make fw-setup` |
| `g++` / `cmake` | C++17 / 3.20+ | native tests; `make server-setup` installs CMake in `.venv` |
| `python3` | 3.11+ | server, docs and native-test tooling |
| serial port | — | only needed to flash/monitor |

```bash
make setup              # Arduino core/libs + Python venv
# or independently:
make fw-setup
make server-setup
```

`make fw-setup` pins:

- `esp32:esp32@3.0.2`
- `ArduinoJson@7.1.0`
- `RTClib@2.1.4` (brings Adafruit BusIO)

Local libraries are **not** installed globally. Every compile adds
`--libraries firmware/libraries`, so the checkout is self-contained.

## Build

```bash
make fw-build BOARD=receiver
make fw-build BOARD=transmitter_sim
make fw-build BOARD=bringup
```

Outputs land in `firmware/build/<board>/`:

```text
receiver.ino.bin          application
receiver.ino.bootloader.bin
receiver.ino.partitions.bin
receiver.ino.merged.bin   full-flash image
receiver.ino.elf / .map
```

The sketch name comes from the folder; `transmitter_sim.ino.bin` is the simulator
binary.

## Flash and monitor

```bash
arduino-cli board list
make fw-flash BOARD=receiver PORT=/dev/ttyACM0
make fw-monitor PORT=/dev/ttyACM0 BAUD=115200
```

`CDCOnBoot=cdc` applies to the ESP32-S3 native USB. The current bench boards are
ESP32 classic with a CP2102 UART bridge, so the default FQBN is
`esp32:esp32:esp32`. If the production board becomes an S3, override
`AKASHVANI_FQBN`.

## Board options and flash size

The default is a 4 MB ESP32 classic with the stock `min_spiffs` scheme:

```text
esp32:esp32:esp32:FlashSize=4M,PartitionScheme=min_spiffs
```

The receiver sketch also carries `partitions.csv`; the ESP32 core pre-build hook
copies it into the build directory. It defines two OTA application slots of
`0x1E0000` bytes (1,966,080 bytes = 1.875 MiB) each, plus 128 KiB SPIFFS and
64 KiB coredump.

For a 16 MB board:

```bash
AKASHVANI_FLASH_SIZE=16M make fw-build BOARD=receiver
```

This selects `app3M_fat9M_16MB`. To use an exact FQBN:

```bash
AKASHVANI_FQBN='esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc' \
  make fw-build BOARD=receiver
```

The receiver currently uses **1,033,593 bytes** (52%) of the 1.875 MiB OTA slot
with `kRfTestMode = true`. The full receiver with Wi-Fi, SD, web and OTA enabled
is about 1.25 MB. See `firmware/partitions/README.md` before changing the table.
OTA requires two app slots; do not trade them away without an ADR.

## Version

`firmware/VERSION` is read by `tools/fw-build.sh` and passed as
`AKASHVANI_FW_VERSION` (`build.defines`), so the binary, the `X-Firmware-Version`
header and the OTA release name agree.

```bash
echo 0.2.0 > firmware/VERSION
make fw-build BOARD=receiver
make fw-export
```

Do not derive the version from git on the device; builds must be reproducible
from a source archive.

## OTA image export

```bash
make fw-export
```

Builds the receiver, copies
`akashvani-receiver-<version>.bin` into `firmware/build/ota/` and
`server/data/firmware/`, and writes a `.sha256` file. The script prints the
`curl` command to register the release with a local server.

## Build internals

`tools/fw-build.sh`:

1. maps the board name to a sketch;
2. selects the FQBN (4 MB default, 16 MB env, or explicit);
3. compiles with `--libraries firmware/libraries` and a shared core cache;
4. injects the version through `build.defines`.

!!! note "Why `build.defines`?"
    Overriding `build.extra_flags` replaces the core's complete flag string,
    dropping `-DESP32` and USB defines. That breaks libraries such as Adafruit
    BusIO. Use `build.defines` (or `compiler.cpp.extra_flags`) for additive
    defines.

## Adding an external library

1. Pin the version in `tools/fw-setup.sh`.
2. Add it to the relevant `library.properties` under `depends=`.
3. Document why it is needed and its license.
4. Prefer a library that compiles on the host if you need to test its logic.

Local project libraries follow the standard Arduino layout
(`library.properties` + `src/`). Add one under `firmware/libraries/` and include
it by header name; no registration step is required.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| `BitOrder does not name a type` | a build property replaced `build.extra_flags`; use `build.defines` |
| `RFM69 not found` | SPI pins, 3.3 V, CS/DIO0 wiring, module power/reset; antenna not required for a version-register probe |
| `mount failed` | card format (FAT32), CS pin, 3.3 V-safe breakout |
| `DS3231 not found` | I²C scan (`make fw-test-hw`), address 0x68, pull-ups |
| AP not visible | 3.3 V rail, AP password length, serial log |
| OTA upload too large | partition scheme / app size |
| Native tests cannot find GoogleTest | first configure needs network; CMake fetches a pinned release |

## Clean

```bash
make fw-clean
```

Removes `firmware/build/`, the Arduino core cache and the native-test build
directory. It does not remove the installed ESP32 core or Python venv.
