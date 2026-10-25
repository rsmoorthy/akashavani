# Firmware architecture

## Layers

```text
sketch .ino        Arduino entry points only
  app.cpp          composition, scheduling, policy
    libraries/     reusable components (no sketch-specific logic)
      ESP32 core / Arduino APIs
```

Sketch folders contain `config.h` (pins, timings) and a small `app.cpp`. The
`.ino` forwards `setup()`/`loop()` so the application can be split into normal
translation units.

## Receiver boot sequence

```mermaid
flowchart TD
  A[Serial + log] --> B[Load NVS settings]
  B --> C[Display begin]
  C --> D[DS3231 + time service]
  D --> E[SD mount]
  E --> F[RFM95 probe]
  F --> G[AP + STA Wi-Fi]
  G --> H[Local admin server]
  H --> I[ArduinoOTA]
  I --> J[Ready]
```

Nothing after the settings load is allowed to abort boot. Each subsystem sets a
readiness flag; the loop and status page reflect partial availability.

## Main loop

The loop is cooperative and must stay responsive enough for the web server and
OTA. Current sequence:

1. service Wi-Fi reconnect backoff;
2. handle local HTTP requests;
3. handle ArduinoOTA;
4. update display state (throttled);
5. receive one radio frame with a 100 ms timeout;
6. retry SNTP if connected and time invalid;
7. drain at most one queued upload;
8. poll commands on an interval;
9. apply a deferred OTA check;
10. update status/LED (throttled).

Blocking calls and their bounds:

| Call | Bound | Notes |
| --- | --- | --- |
| `radio.receive()` | 100 ms | configurable |
| `syncFromNtp()` | 15 s | only when time invalid and online |
| `postObservation()` | connect 5 s, request 10 s | one per loop |
| OTA download | streamed | stalls fail after 15 s without data |

If the touch UI or TLS handshakes increase latency, move networking to a
separate FreeRTOS task or reduce radio timeout. Do not add unbounded blocking
loops to `loop()`.

## Radio seam

`IRadio` exposes packet-oriented receive/transmit. The RFM95 implementation
currently verifies the chip ID and returns `Unsupported` from `begin()` because
the modem configuration is not ported. The receiver then stays idle but keeps
its AP/admin path alive.

When porting:

- keep register access inside `Rfm95Radio`;
- if Davis needs raw/direct mode, add a raw receive method to `IRadio` rather
  than exposing registers;
- put the receive timestamp in `RadioFrame` as close to the IRQ as possible;
- do not decode inside the driver.

RFM69 will be a second implementation. `DavisDecoder` must not know which chip
delivered the frame.

## Davis seam

`DavisObservation` is the canonical in-memory model. It uses a validity mask
rather than sentinel values, so an absent pressure reading cannot become 0 hPa.
The decoder must:

1. reject impossible lengths before parsing;
2. verify CRC/sync;
3. validate sensor ranges;
4. set only fields actually present;
5. never allocate per frame.

`DavisSimulator` is deterministic given a seed, so test failures reproduce. It
produces bounded random walks, not RF modulation. `DavisEncoder` is the matching
seam for the simulator board.

## Settings and provisioning

`SettingsStore` uses NVS via `Preferences`. On a blank device it derives a device
ID `ak-xxxxxx` from the STA MAC and fills development defaults. The AP SSID is
`Akashvani-<id>`. Settings survive firmware updates unless the NVS partition is
erased.

Sensitive values (Wi-Fi password, cloud token, admin password) are stored in
plain NVS. This is acceptable for the scaffold but should be hardened for
production (encrypted NVS, per-device tokens, physical access assumptions).

## Storage policy

- one JSON object per line, daily files;
- append + flush by default (durability over SD wear);
- no deletion or rotation on the device;
- the card is the source of truth for offline recovery.

If flash or card wear becomes a problem, batch flushes behind a small buffer and
accept losing the last few records on power loss. Make that a deliberate ADR
change.

## Firmware versioning

`firmware/VERSION` is passed to the compiler as `AKASHVANI_FW_VERSION` by
`tools/fw-build.sh`. The same version is used by OTA tooling and embedded in
`X-Firmware-Version` on uploads. Bump it once per release; do not derive it from
git state on the device.

## Testing boundary

Host tests compile only files with no Arduino dependencies. Anything touching
`Arduino.h`, Wi-Fi, SD, NVS or a display must either:

- isolate its pure logic into a host-safe file, or
- be exercised by `firmware/tests/hardware/bringup`.

This is why `ArchivePath`, the simulator, protocol validation and CRC live apart
from their hardware wrappers.
