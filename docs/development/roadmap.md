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

## Phase 1 — port the RF path (implemented, bench-verified 2026-10-27)

1. Capture raw frames from the prototype with the exact RFM69 settings; store
   them as test vectors with the date/station model.
2. Port receiver configuration into `Rfm69Radio`; keep register access there.
3. Port frame validation and field extraction into `DavisDecoder`; add
   `TooShort`, `CrcMismatch`, `InvalidValue` tests.
4. Port the matching encoder into `DavisEncoder`.
5. Port the transmitter modulation into `Rfm69Radio::transmit`.
6. Run receiver + simulator on the bench.

All six are done; see `docs/specs/rf-davis-receive.md` and the RF slice in
`docs/development/handoff.md`. The exit criterion is only partly met: the
receiver logs the simulated observations, but SD archive and cloud upload were
not exercised with RF in this slice. Corrupt frames are rejected.

## Phase 2 — hardware integration

- Validate SD mount/flush/power-loss behaviour with the intended card.
- Validate DS3231 battery/lost-power handling and SNTP write-back.
- Choose and implement the display driver; keep `NullDisplay` fallback.
- Measure current draw in AP, STA, RX and OTA modes.
- Decide 4 MB vs 16 MB from measured image size and UI plans.
- RFM69 driver behind `IRadio` is ported; remaining RF work is the production
  channel-sync/restart policy and a wider-bandwidth experiment.

## Phase 3 — production hardening

- TLS: pin a CA, remove `setInsecure()`, handle clock-before-TLS correctly.
- OTA: SHA-256 streaming verification and a two-step download/boot command flow
  are done and bench-tested; still need signing, TLS pinning and rollback policy.
- Provisioning: unique per-board AP passwords, an authenticated AP page, token
  editing, optional physical reset.
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
| `Rfm69Radio` | ported; production sync/restart policy and RF bandwidth/power decisions remain |
| `DavisDecoder/Encoder` | ported for the ISS message set; pressure/UV not in this subset |
| `SdArchive` | no hardware validation, no retention |
| `AkashavaniDisplay` | no real driver; touch UI not designed |
| `AkashavaniNet` | no TLS pinning, unsigned OTA, limited command set |
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

Capture the prototype's RFM69 configuration and a handful of raw Davis frames.
Everything in Phase 1 depends on those two artifacts.
