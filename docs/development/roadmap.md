# Roadmap

The scaffold is complete enough to accept the prototype code without reshaping
the repo. The order below minimises the time spent debugging two unknowns at
once.

## Phase 0 — scaffold (done)

- Monorepo layout, libraries, two thin sketches.
- arduino-cli build scripts, 4 MB/16 MB profiles, OTA export.
- Native tests, hardware bring-up sketch.
- FastAPI ingest/command/OTA service, Docker, PostgreSQL option.
- Protocol schemas, operations docs, ADRs.

## Phase 1 — port the RF path

1. Capture raw frames from the prototype with the exact RFM95 settings; store
   them as test vectors with the date/station model.
2. Port receiver configuration into `Rfm95Radio`; keep register access there.
3. Port frame validation and field extraction into `DavisDecoder`; add
   `TooShort`, `CrcMismatch`, `InvalidValue` tests.
4. Port the matching encoder into `DavisEncoder`.
5. Port the transmitter modulation into `Rfm95Radio::transmit`.
6. Run receiver + simulator on the bench.

**Exit criteria:** a simulator observation appears in the receiver log, on the SD
card and in the cloud with the same values; injected corrupt frames are rejected.

## Phase 2 — hardware integration

- Validate SD mount/flush/power-loss behaviour with the intended card.
- Validate DS3231 battery/lost-power handling and SNTP write-back.
- Choose and implement the display driver; keep `NullDisplay` fallback.
- Measure current draw in AP, STA, RX and OTA modes.
- Decide 4 MB vs 16 MB from measured image size and UI plans.
- Add the RFM69 driver behind `IRadio` once the RFM95 path is stable.

## Phase 3 — production hardening

- TLS: pin a CA, remove `setInsecure()`, handle clock-before-TLS correctly.
- OTA: verify SHA-256 while streaming, sign images, enable rollback policy.
- Provisioning: unique default AP/admin passwords, optional physical reset.
- Auth: per-device tokens everywhere, rotation runbook, admin network policy.
- Observability: structured logs, upload success/failure counters, queue depth.
- Database: PostgreSQL in production, backups, retention/partitioning.
- Local admin: CSRF consideration, rate limiting, audit log.

## Phase 4 — field readiness

- Enclosure, antenna placement, surge/ESD and power protection.
- Site RF survey and legal frequency/power check.
- SD retention and offload runbook.
- Firmware update and rollback drill.
- Spare-parts and board-provisioning process.
- Onsite acceptance checklist.

## Known gaps by component

| Component | Gap |
| --- | --- |
| `Rfm95Radio` | modem config, FIFO/IRQ, raw mode, transmit |
| `DavisDecoder/Encoder` | CRC, framing, field extraction, range checks |
| `SdArchive` | no hardware validation, no retention |
| `AkashvaniDisplay` | no real driver; touch UI not designed |
| `AkashvaniNet` | no TLS pinning, no OTA hash, limited command set |
| Server | no upload endpoint for logs, no admin UI, single-region SQLite default |
| Tests | no automated RF/HIL job |

## Risks

| Risk | Impact | Mitigation |
| --- | --- | --- |
| 4 MB flash too small | cannot ship UI + TLS + OTA | measure early; 16 MB variant |
| Davis protocol differs from prototype assumptions | no data | capture vectors before porting |
| SD corruption on power loss | lost archive | flush policy, industrial card, power design |
| Clock invalid at boot | dropped observations | RTC battery + SNTP + clear status |
| Unverified OTA image | bricked device | checksum, signing, rollback, bench drills |
| Shared SPI contention | missed frames | CS discipline, timing measurements |

## Next action

Capture the prototype's RFM95 configuration and a handful of raw Davis frames.
Everything in Phase 1 depends on those two artifacts.
