# AkashvaniDavis

Owns the Davis-specific domain: what a weather observation is, how a radio
frame becomes one, and how the simulator produces one.

| File | Purpose | Host-safe |
| --- | --- | --- |
| `DavisObservation.h` | Canonical observation + validity mask | yes |
| `DavisConstants.h` | Frame and station constants | yes |
| `DavisDecoder.*` | Radio frame -> observation seam | yes |
| `DavisEncoder.*` | Observation -> radio frame seam (simulator) | yes |
| `DavisSimulator.*` | Deterministic random-walk weather generator | yes |

## Porting the decoder

`decodeDavisFrame()` validates nothing beyond frame length today and returns
`DavisDecodeStatus::UnsupportedProtocol`. Port the prototype in one place:

1. Keep frame validation (length, CRC, sync) separate from field extraction.
2. Set `DavisObservation::validFields` only for fields actually present.
3. Never log secrets; frames may contain station IDs but not credentials.
4. Add vectors captured from the station under `firmware/tests/native/` as
   soon as they are available, with the RFM95 settings used to capture them.

The receiver and transmitter simulator share the same `DavisObservation` model,
which prevents the two boards from drifting apart.
