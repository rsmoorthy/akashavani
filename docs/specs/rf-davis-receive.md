# RFM69 + Davis receive/transmit bench slice

**Status:** implemented and under bench test (2026-10-27)
**Boards:** receiver USB0 (`cc:7b:5c:27:31:e0`), simulator USB1 (`08:b6:1f:b8:38:50`)
**Reference:** `prototype/lib2/DavisRFM69`, capture `prototype/minicom.cap`

## Goal

Port the validated prototype RFM69/Davis settings into the firmware seams, build
a deterministic transmitter simulator, and prove that the receiver tracks the
Davis 51-channel, 3-second hop sequence under three bench conditions:

1. continuous transmission with no skipped channels (15 min);
2. random omission of 1-5 consecutive channel slots (15 min);
3. two transmitter restarts, each resuming from channel 1.

This slice only logs the received frames. Archive, cloud upload and a complete
Davis observation model remain gated behind a valid clock and are not part of
the success criteria.

## RF configuration (must not drift)

Taken from the working prototype and written on every boot, because the USB1
module has no RESET connection and retains its previous configuration:

| Setting | Value |
| --- | --- |
| Modulation | FSK, packet mode, Gaussian shaping BT=0.5 |
| Bitrate | 19.2 kbps (`0x0683`) |
| Deviation | 4.8 kHz (`0x004E`) |
| Sync | `0xCB 0x89`, 2 bytes, tolerance 2 |
| Payload | fixed 8 bytes (6 data + 2 Davis CRC) |
| Hardware CRC | off; Davis CRC-16/CCITT is checked in software |
| Preamble | 4 bytes |
| RX bandwidth | about 25 kHz (`REG_RXBW = 0x4C`) |
| AFC | auto, auto-clear; AFC bandwidth about 50 kHz |
| DIO0 | PayloadReady in RX, PacketSent in TX |
| Power | 0 dBm on the bench simulator; +20 dBm is the prototype setting |
| Channels | 51-entry US table, 902.38-927.47 MHz |

The bit order is also preserved: bytes are bit-reversed on transmit and receive,
matching the prototype and the over-the-air Davis format.

### Bench power reduction

The prototype ran the RFM69HW at its maximum (+20 dBm). With the two boards a
few centimetres apart, the receiver RSSI was about -33 dBm and the continuous
test lost roughly 5-10% of frames. At 0 dBm the RSSI drops to about -48 dBm and
the first 3-minute run delivered 57/58 frames. The transmitter defaults to
0 dBm for the bench. This changes only `REG_PALEVEL`/`REG_OCP`/test-PA registers,
not the modulation, sync or framing. Restore a legal field power with an antenna
and local regulatory check before deployment.

### Possible register improvements (not applied yet)

The tested settings work, so this slice keeps them. Two observations for a
later controlled experiment:

- Carson bandwidth for 19.2 kbps and 4.8 kHz deviation is about 28.8 kHz; the
  25 kHz RX filter is slightly narrow. A 50 kHz RX filter may improve margin at
  the cost of selectivity.
- `REG_RSSITHRESH = 0xA5` (-82.5 dBm) is not a packet filter in normal RX mode,
  but it is marginal for the bench RSSI seen here. It matters for listen-mode
  and RSSI interrupt use, not for the packet-ready interrupt path.

Do not change these values without repeating the packet-success test.

## Frame format

Eight bytes on air, exactly as captured:

```
byte 0  message id (high nibble) | battery-low (bit 3) | transmitter id (0..7)
byte 1  wind speed, mph
byte 2  wind direction, 0..255
byte 3  message-specific low / high byte
byte 4  message-specific high bits
byte 5  unused by decoded weather fields; simulator test sequence
byte 6  CRC high
byte 7  CRC low
```

CRC-16/CCITT, initial value 0, polynomial `0x1021`, over bytes 0-5. Verified
against the capture, for example `87 05 2D 32 5B 0F -> B9 91`.

The simulator writes a wrapping slot sequence into byte 5 so the receiver can
measure gaps. This is a bench-only use of an otherwise unused byte; real Davis
frames may contain any value there.

### UV and solar (message IDs 0x4 and 0x6)

Both messages carry a 10-bit value in bytes 3-4:

```
raw = (byte3 << 2) | (byte4 >> 6)      // bits 9..2 from byte 3, 1..0 from byte 4
```

| ID | Field | Decode | Missing sensor |
| --- | --- | --- | --- |
| `0x4` | UV index | `raw / 50` | `raw >= 0x3FF` |
| `0x6` | solar radiation | `raw * 1.757936` W/m2 | `raw >= 0x3FE` |

Source: the kobuki/VPTools reference (`Examples/ISSRx/ISSRx.ino` and
`DavisRFM69.h`, which label `0x4` as `VP2P_UV` and `0x6` as `VP2P_SOLAR`). When
the sentinel matches, the frame is still valid for its wind fields but the UV or
solar field is left unset rather than reported as zero.

These two formulas are reference-derived, not yet checked against a local
console. The captured `msg 4` and `msg 6` frames in
`firmware/tests/native/vectors/davis_iss_frames.txt` decode to 2.90 UV and
592.4 W/m2 with this code. The legacy prototype mapping of `0x7` to solar
radiation is unchanged and unverified; the reference maps `0x7` to the Vue solar
panel voltage instead.

## Test firmware behaviour

- Receiver (`config::kRfTestMode = true`) skips Wi-Fi, SD, OTA and upload so RF
  timing is not disturbed. It logs every valid frame with channel, sequence,
  gap, RSSI, message type, decoded value and raw bytes.
- It hops forward immediately after a valid frame, then every 3.2 s while no
  valid frame arrives. A CRC failure does not advance the channel.
- After seven consecutive misses it switches to a reverse scan at 3.5 s per
  channel to acquire a transmitter that restarted at an arbitrary channel. The
  longer dwell and slight offset from the 3 s transmit grid are deliberate: they
  make the scan slide past a burst instead of staying phase-locked to it. It
  returns to normal forward hopping on the first valid frame.
- The transmitter sends one frame immediately and then exactly every 3 s. The
  slot clock advances by 3 s even when a slot is skipped.
- Transmitter serial commands: `R1`, `R2`, `R3`, `S`, `X`, `?`.
- Receiver serial commands: `R` (reset state/statistics), `S` or `?` (stats).
- `tools/rf-bench.py` orchestrates the three scenarios and timestamps both
  serial streams.

## Acceptance criteria

### Test 1 - continuous

- At least 15 minutes of transmission (about 300 slots).
- Receiver reports a valid CRC frame for every transmitted sequence except
  isolated losses caused by noise. Target packet success rate >= 99%.
- Every valid frame logs an RSSI.
- Forward channel tracking is visible in the `HOP` lines.

### Test 2 - random skipped channels

- At least 15 minutes.
- Transmitter omits 1-5 consecutive channel slots at random, with an 8-slot
  cooldown between skip runs so a normal skip run cannot masquerade as a lost
  lock.
- Receiver remains locked: no `SEARCH` mode for ordinary skip runs; each skip
  gap is followed by a valid frame on the expected later channel.
- Ground truth is the transmitter's `SKIP`/`TX` lines; receiver `gap` values
  should account for the skipped slots.

### Test 3 - restarts

- Two transmitter restarts after the run has been established, at 3 and 8
  minutes; each resets the channel to 1 and the sequence to 0.
- Receiver detects a silence longer than 6 s, logs `RESYNC` with the silence
  duration, and returns to valid frames.
- Record the time from the restart to the first valid frame for each restart.
  A blind reverse scan can take one or two channel cycles (roughly 80-180 s);
  it is not expected to be instantaneous. Do not assume the simulator's
  channel-1 restart is known to the receiver.

## Required evidence

- receiver serial log with `RX`, `RX_BAD`, `HOP`, `SEARCH`, `RESYNC` and `STATS`;
- transmitter serial log with `TX`, `SKIP`, `SKIP_PLAN` and `RESTART`;
- parsed counts: transmitted, skipped, valid, bad CRC, gaps, resyncs, success
  rate, average/min/max RSSI, catch-up time after each restart;
- both firmware builds and `make fw-test` passing.

## Out of scope

- Archive/SD, cloud upload, OTA and display.
- Proving the full Davis field set (pressure and UV are not in the ISS frame
  subset decoded here).
- Antenna/RF compliance, range and regulatory power checks.
- Production channel-sync policy after a real Davis restart. The reverse scan
  is a bench acquisition aid, not yet an agreed field behaviour.
