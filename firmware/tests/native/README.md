# Native tests

Host-side tests for everything that does not need Arduino headers. They are
built with CMake and GoogleTest; if GoogleTest is not installed, CMake fetches a
pinned release.

```bash
make fw-test
# or
cmake -S firmware/tests/native -B firmware/tests/native/build
cmake --build firmware/tests/native/build --parallel 1
ctest --test-dir firmware/tests/native/build --output-on-failure
```

## What belongs here

- CRC, hex, time and device-ID helpers.
- Davis frame validation, CRC, decode/encode round-trip and captured vectors.
- Protocol envelope/command validation.
- Archive path layout.
- The deterministic weather simulator, channel table and mock radio.

## What does not

- Anything including `Arduino.h`, Wi-Fi, SD, Preferences or display drivers.
  Those are exercised by the on-target bring-up sketch in
  `firmware/tests/hardware/`.

## Adding captured Davis frames

Put binary vectors under `firmware/tests/native/vectors/` and load them from a
test. Record the RFM69 settings and the station model in a comment next to the
vector. Do not embed credentials or site identifiers.
