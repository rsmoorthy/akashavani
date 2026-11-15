# AkashavaniCore

Small, dependency-light utilities used by every other Akashavani library.

The files marked *host-safe* below do not include `Arduino.h` and are compiled
into the native test target (`firmware/tests/native`).

| File | Purpose | Host-safe |
| --- | --- | --- |
| `Status.h/.cpp` | Small status enum shared by drivers | yes |
| `Crc.h/.cpp` | CRC-16 CCITT-FALSE, XMODEM and ARC | yes |
| `Hex.h/.cpp` | Byte/hex conversion | yes |
| `TimeUtils.h/.cpp` | UTC ISO-8601 format/parse without libc time zones | yes |
| `DeviceId.h/.cpp` | Device ID validation and MAC-derived IDs | yes |
| `Log.h/.cpp` | Serial logger with levels and tags | no |

## Conventions

- No exceptions, no RTTI and no dynamic allocation in hot paths.
- Drivers return `akashavani::Status`; only `Log` touches `Serial`.
- Keep host-safe files free of Arduino headers. Add new pure logic here rather
  than in a sketch.
