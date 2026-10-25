# 0003 — Host-testable domain cores

## Context

Embedded testing is slow and hardware-dependent. Most bugs in this project are
in framing, time handling, retries, path layout and protocol validation, none of
which need a board.

## Decision

Keep domain logic free of Arduino headers wherever practical:

- `AkashvaniCore`, `AkashvaniDavis`, `AkashvaniProtocol` and `ArchivePath` are
  compiled into a native CMake/GoogleTest target.
- Hardware wrappers (`Rfm95Radio`, `SdArchive`, `WifiService`, `Ds3231Clock`,
  real displays) stay thin and are exercised by the bring-up sketch and later HIL
  tests.
- New per-frame logic must not allocate dynamically.

## Consequences

- Fast tests and captured-frame regression tests are possible without hardware.
- Some duplication of types or shims may be needed; prefer moving pure logic down
  rather than mocking Arduino.
- A successful host test is not proof that the firmware compiles; CI also builds
  both sketches.
- The native target must be kept in sync when pure files are added.
