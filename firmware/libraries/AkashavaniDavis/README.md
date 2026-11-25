# AkashavaniDavis

Owns the Davis-specific domain: what a weather observation is, how a radio
frame becomes one, and how the simulator produces one.

| File | Purpose | Host-safe |
| --- | --- | --- |
| `DavisObservation.h` | Canonical observation + validity mask | yes |
| `DavisConstants.h` | Frame length, sync bytes, station constants | yes |
| `DavisCrc.h` | Davis CRC-16/CCITT | yes |
| `DavisDecoder.*` | Radio frame -> observation | yes |
| `DavisEncoder.*` | Observation -> radio frame (simulator) | yes |
| `DavisSimulator.*` | Deterministic random-walk weather generator | yes |

## Frame format

Davis ISS frames are always eight bytes: six data bytes followed by the
CRC-16/CCITT (initial value 0, polynomial `0x1021`) over the first six bytes.
The decoder rejects wrong lengths, CRC errors, unknown message IDs and
out-of-range temperature, humidity or wind direction. Fields absent from a
particular ISS message remain unset in `DavisObservation::validFields`.

The encoder mirrors the decoder for all message types currently known:
goldcap voltage, rain rate, solar radiation (`0x6`), temperature, gust speed,
humidity and rain counter. The legacy `0x7` message is kept for protocol
completeness but is not treated as solar radiation; a real VP2 Plus interleaves
it with `0x6`, and including it inflated the solar archive mean. The bench
simulator writes a wrapping slot sequence into byte 5, which is unused by the
decoded fields, so the receiver can measure gaps.

Captured vectors used by the native tests include
`87 05 2D 32 5B 0F B9 91` (temperature, transmitter 7). See
`prototype/minicom.cap`.

The receiver and transmitter simulator share the same `DavisObservation` model,
which prevents the two boards from drifting apart.
