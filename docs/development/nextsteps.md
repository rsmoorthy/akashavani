# Next steps (owner-observed gaps)

**Started:** 2026-11-11
**Revised:** 2026-11-11 (owner review of points 1–4; SD/offline and raw-log items added)
**Status:** agreed backlog. Items marked **done** were completed in this pass;
the rest are work items.

## Status (2026-11-12 implementation pass)

| Item | Status |
| --- | --- |
| 0. Remove `station_id` | **done** |
| 1. 30 s health, `last_radio_at`, always-on 5-minute radio packet | **done** |
| 2. Server per-device status data | **done** (online, last radio, latest bucket/frames/RSSI, uptime, queue depth, heap, reset reason, Wi-Fi RSSI) |
| 3. Admin console (no login, 50 commands) | unchanged by decision; extra status data added |
| 4. Command set | unchanged plus `get_pending_commands`, `get_data_again` |
| 5. `last_sent` + SD catch-up + 3-year retention + AP page | **done** (MessagePack batches; server also accepts JSON/CBOR) |
| 6. Troubleshooting commands | **done** (`get_data_again`, server `download_data` CSV, `download_raw_data` + raw CSV) |
| 7. Raw 8-byte weekday log | **done** (weekly truncate, 5-minute flush) |
| 8. One command in flight, async ack, timeout/probe/3 retries | **done** |

Implemented deviations to be aware of:

- Bulk archive uploads use **MessagePack** (`application/msgpack`, encoded by
  ArduinoJson); the batch endpoint also accepts JSON and CBOR.
- `download_raw_data` uploads raw frames in JSON batches (100 per request) and is
  capped at 20 000 frames per command; the ack reports `truncated` when the cap
  is hit. The admin downloads the accumulated CSV.
- `get_data_again` runs synchronously inside the command handler and acks when
  the replay finishes (bounded to one week and 4 minutes). It blocks RF receive
  while it runs, which is acceptable for a manual troubleshooting command.
- A command counts as in flight while `delivered`; after the 5-minute ACK
  timeout it becomes `timed_out`, a `get_pending_commands` probe is queued, and
  the original is requeued when the probe reports it is no longer running. Three
  attempts maximum.

## Start here (new session)

1. Read `AGENTS.md`, `docs/development/handoff.md`, then this file.
2. Only the items marked **Done** are implemented; everything else is a work
   item agreed with the owner.
3. Work one vertical slice at a time. For each: protocol schema + server model
   and migration + firmware + native/server tests + docs, then one focused
   commit. Run `make fw-build-all`, `make fw-test`, `make server-test`,
   `make lint`, `make docs-build`.

Bench state at handoff (2026-11-12, verified):

- **USB0 = simulator** (`08:b6:1f:b8:38:50`), **USB1 = receiver** (`cc:7b:5c:27:31:e0`).
  Check MACs with `esptool read_mac` before flashing — `arduino-cli board list`
  does not show them and the ports have swapped once.
- Receiver `ak-2731e0`, device token `secret123`, shared API token `secret`,
  server `http://192.168.29.31:8000`, AP `akashvani-e0` / `doordarshan`, STA
  `Advaita` (`192.168.29.84`), simulator `192.168.29.144` (idle).
- Server DB: `server/data/akashvani.db`; admin token `dev-admin-token`;
  `/admin` at `http://127.0.0.1:8000/admin`. The dev server is currently run
  without `--reload` (`setsid nohup uvicorn ... :8000`), so restart it after
  server code changes.
- Migrations applied: `7d1c93a5e2b4` (station_id dropped),
  `b6e2f0a91c47` (last_radio_at), `c4d81f2a6b09` (command attempts),
  `d5e9a1c3f720` (device health).
- Receiver SSD1306 top rows are still ghosted (accepted); the simulator panel is
  clean.
- Obsolete device row `station-1` (from an accidental flash) can be deleted.

Suggested implementation order:

1. ~~`station_id` removal (section 0)~~ — **done 2026-11-12**.
2. `last_radio_at` header + always-on 5-minute radio packet (section 1).
3. `last_sent` + SD catch-up with CBOR batches (section 5).
4. Admin status data (sections 2 and 3).
5. One command in flight, async ack, `get_pending_commands` (section 8).
6. Raw 8-byte weekday log (section 7).
7. Troubleshooting commands (section 6) + batch endpoint.

---

## 0. Remove `station_id` entirely — **DONE**

Implemented: dropped `devices.station_id` (Alembic `7d1c93a5e2b4`), removed the
`X-Station-Id` fallback (identity fallback is now `X-Device-Id` only), removed
the field from `DeviceOut`, `/admin`, the AP page, NVS, `ApiClient`,
`config::kStationId`, `self_test` and the boot log. Server tests updated.

Owner decision: drop the station number from the system. It is neither the
device name (`device_id`, MAC-derived `ak-xxxxxx`) nor a credential (the device
token), and it is the source of the ambiguous identity fallback.

- Server: drop `devices.station_id` (new Alembic revision), remove the
  `X-Station-Id` fallback from `require_device`, remove the field from
  `DeviceOut`/`DeviceSettings` JSON and the `/admin` "Station" column.
- Firmware: remove `config::kStationId`, `DeviceSettings.stationId`, the NVS
  `station` key, the AP-page field, the `X-Station-Id` header, the `self_test`
  result field and the boot-log field.
- Use `devices.name` for the human label (add an edit action in `/admin`).
- Shared-token fallback: **proposed to remove** so identity is token-only
  (confirm with the owner at implementation). If kept, require `X-Device-Id`.
- Wire/API change: older firmware sending `X-Station-Id` is ignored; admin API
  consumers lose the field.

## 1. Periodic health / command update — **DONE**

**Done in this pass**

- Command/health poll interval changed from 5 s to **30 s**
  (`config::kCommandPollIntervalSeconds`, `SettingsStore` default, plus a
  one-time NVS migration for boards that stored the old 60 s value).
- The device token is now editable on the AP page, so a board can be pointed at
  its server-side device record without reflashing.

**Already existed**

- The receiver leases commands every poll interval whenever cloud settings exist
  and the STA link is up; RF data is not required. Every authenticated request
  updates `devices.last_seen_at` and refreshes MAC/firmware.
- The 5-minute packet has the right fields (`frames_expected/valid/crc_bad`,
  `resync_count`, `search_count`, `gap_slots`, RSSI min/avg/max) but it is
  **not** sent when there was no RF data: `ArchiveCollector::ensureBucket()`
  explicitly drops buckets with `framesValid == 0` ("a radio outage does not
  create empty records"). Verified on 2026-11-11: no archive row appears for a
  5-minute window without Davis frames.

**To do**

- Emit the 5-minute radio packet even when `framesValid == 0`, with the
  counters zeroed and environmental means null. Either drop the
  `hasData_ && framesValid > 0` guard or add a separate radio-status packet;
  decide whether empty rows belong in the `archive` table or a new
  `radio_status` table.
- Add **"last radio seen time"** (owner decision: a request header, because that
  rides on every authenticated request which already acts as last-seen): send
  e.g. `X-Last-Radio-Age` (seconds) or `X-Last-Radio-At` (epoch) on every device
  request; the server stores `devices.last_radio_at` (new column + migration).
  It stays null/old when no RF frame was ever decoded.
- Server-side online/offline derivation: a device is *reachable* if
  `last_seen_at` is within ~3 × poll interval (90 s), independent of RF.
- Make the "station cannot reach the server" case visible: an explicit
  online/offline flag on the device row/status API, so an operator can
  distinguish (a) online + RF data, (b) online + no RF data, and (c) offline.
- Heartbeat failures (server unreachable) should be logged/counted on the
  device and surfaced on the board status page.

## 2. Server-side per-device status view (add data, no new login) — **DONE**

Owner decision: show **all available data**; do not trim or limit what the page
displays.

**Already existed**

- `GET /api/v1/admin/devices` and the `/admin` table show id, name, station,
  MAC, firmware and last seen (station goes away in section 0).

**To do (extend the existing `/admin` page; keep the pasted admin token)**

- Add derived **online/offline** (based on `last_seen_at` age) and the
  **last radio seen** column from item 1.
- Show the latest archive bucket per station: bucket time, `frames_valid /
  frames_expected`, CRC-bad, and RSSI, so "reachable but no RF" is obvious.
- Add a per-device detail panel: last observation time, last archive upload
  status, queue depth if reported, reset reason, uptime, last radio seen.
- Keep the existing "last 50 commands" view unchanged (owner decision: no trim).

## 3. Admin console

**Owner decision:** no real login/session needed for now. Keep the pasted bearer
token in `/admin` and keep the last 50 commands.

**To do**

- Add the extra status data from item 2 to the same page.
- Keep the current command types; no restriction to three.

## 4. Command set (current behaviour)

Implemented end-to-end (server queues → device leases → executes → ack with a
structured result):

| Command | Behaviour |
| --- | --- |
| `reboot` | Replies, then `ESP.restart()` |
| `sync_time` | Forces an SNTP sync; reports failure if SNTP is unreachable |
| `check_for_update` | Schedules an OTA check on the next loop |
| `get_fw_version` | Version, partition, build date/time, free sketch, MAC |
| `download_new_fw` | Stages the image in the inactive slot, verifies SHA-256 |
| `boot_with_new_fw` | Boots the staged image only if the requested version matches |
| `self_test` | Uptime, Wi-Fi, radio, archive and time-valid flags |

Accepted by the server but **not implemented on the device** (acks
`not implemented`): `set_config`, `upload_logs`.


## 5. SD archive, offline catch-up and `last_sent` — **DONE**

**Exists today**

- Every completed 5-minute record is appended to the SD card as JSONL at
  `/archive/<device>/<YYYY>/<MM>/<DD>.jsonl` — one file per day, i.e. 365 files
  per year (nested directories rather than a flat `…-YYYY-MM-DD` suffix; same
  data model).
- The server deduplicates by `(device_id, message_id)` and
  `(device_id, date_time)`, so a re-sent record is accepted as a duplicate.
- Uploads are queued in RAM only (`kMaxPendingUploads = 8`) with exponential
  backoff; the SD card is the durable copy.

**To do**

1. Keep the existing nested per-day path
   `/archive/<device>/<YYYY>/<MM>/<DD>.jsonl` (owner decision). One file per day,
   365 per year.
2. Add a `last_sent` marker file on SD (e.g. `/state/last_sent`) holding the
   bucket-end timestamp of the newest record the server acknowledged
   (idempotent ack only).
3. **Catch-up on reconnect:** replay SD records newer than `last_sent`, oldest
   first, until the server acks or the link drops. This must handle multi-day
   and multi-week gaps (worst observed ~2 weeks, ~4000 buckets at 5 min): stream
   from SD, never load the whole range into RAM, keep the existing per-record
   `message_id` for idempotency, and advance `last_sent` only after a successful
   ack. Send **up to 20 records per request** (bulk archive upload) so a long
   outage does not cost one HTTP round-trip per bucket, and rate-limit the
   batches so catch-up does not starve the RF loop.
4. Show `last_sent` on the AP page (and in `/api/status`).
5. Retention: **keep 3 years, delete older**. Apply to the SD per-day archive
   (remove whole day files older than 3 years, e.g. at boot and daily) and to
   the server archive/observation rows. Capacity check: ~0.35 MB/day,
   ~130 MB/year, ~390 MB for 3 years on the 29 557 MB card; a 2-week catch-up
   backlog is ~5 MB.


## 6. Troubleshooting commands (data re-fetch / export) — **DONE**

These are operator tools for repairing or inspecting data; they are not part of
the normal upload path.

| Command | Payload | Behaviour |
| --- | --- | --- |
| `get_data_again` | `start`, `end` (date/time) | Device re-sends the SD archive records in the range. Server **upserts rows with the same `message_id`** (and `(device_id, date_time)`), never duplicates, never a wholesale DB replace. Batched (up to 20 records), rate-limited, asynchronous: the command ack arrives after the replay completes. |
| `download_data` | `start`, `end` | Produce a CSV of the stored archive rows for the range in a temp location on the server for the operator to download. |
| `download_raw_data` | `start`, `end` | Device uploads the raw 8-byte frames with receive timestamps for the range (max one week, 20k frames); the server writes a CSV that the admin downloads. |

Open design points to settle before implementing:

- Date/time format in the payload: **local IST** (owner decision) for all
  operator-facing times and CSV output. Practical implementation: keep epoch
  seconds/UTC internally and convert to IST (+05:30, no DST) at the edges
  (raw log lines, CSV, admin page) so comparisons stay unambiguous.
- Maximum range for these troubleshooting commands: **one week** (automatic
  catch-up has no cap; these manual tools do).
- Transport for the returned data: new device endpoints (e.g.
  `POST /api/v1/devices/me/archive/replay` and `…/raw/replay`), batched and
  rate-limited, rather than putting bulk data in a command ack.
- Long-running commands must support an ack that arrives minutes later (see
  item 8); the server must not expire them while the device is working.
- Authorization: these must be admin-only (they are queued through `/admin`).
- CSV naming/location and cleanup of the temp files.

## 7. Raw frame log on SD — **DONE**

**Missing today** (frames are only logged to serial).

- Append every **CRC-valid** 8-byte frame to SD together with its receive
  timestamp. File per weekday: `raw-data-monday`, … `raw-data-sunday` (owner
  naming; decide extension, e.g. `.csv`/`.jsonl`).
- Record at least: receive time (**local IST**, owner decision), the 8 bytes,
  RSSI and RF channel would also help diagnose new message types.
- Each weekday file is **overwritten when that weekday comes round again**, so
  there are exactly seven rolling files (the previous week's data for that day
  is replaced). No retention policy needed.
- Buffer frames in RAM and flush to SD **once per 5 minutes**; do not write per
  frame.
- Not uploaded as part of normal operation; only exposed by
  `download_raw_data`.
- Timestamps are IST in the file; keep epoch comparisons internally.
- Sizing: at one ISS frame every 3 s, ~28.8k frames/day. A line like
  `2026-11-11T16:25:03Z,81 06 01 2B EE 00 6F 3B` (~45 bytes) is ~1.3 MB/day,
  so the seven weekly files hold at most ~9 MB — no retention problem.

## 8. One command in flight, sync or async completion — **DONE**

New global rule from the owner: the server sends **at most one command at a time
to a device**. The next command is only queued/delivered after the previous one
is completed. Completion may be immediate (reboot, get_fw_version, sync_time) or
take minutes (data replay, OTA download).

Current behaviour to change:

- The device leases up to 4 commands per poll (`leaseCommands(..., 4, ...)`) and
  executes them in one loop pass. Change the lease limit to 1, and let the
  server **accept and queue** a second command from the admin, delivering only
  the queue head per device (owner decision). The admin UI shows the queued
  state.
- Command lifecycle needs an explicit "running" state (or `accepted`) plus a
  long-running timeout, because the ack can arrive much later than delivery.
  Today `CommandAck.status` only allows `ok`/`error`.
- While a command is running, it must not be expired by the normal TTL. Options:
  extend `expires_at` on delivery/acceptance, or make the TTL start only after
  delivery.
- **Reboot / lost-ack recovery** (owner design):
  1. The server runs an ACK timeout for every delivered command.
  2. On timeout it queues `get_pending_commands` (new command) to ask the device
     what it still has in flight. This is a recovery probe and is allowed even
     though one command is otherwise in flight.
  3. If the device reports nothing pending, the server requeues the original
     command. A command is attempted at most **3 times**, then it is marked
     failed.
  4. A device that answers with `error` is not retried.
  This needs a per-command attempt counter in the DB and an explicit
  `attempts`/`last_attempt_at` pair; `get_pending_commands` needs a response
  shape (command ids + state) and should be implemented on the device.
- The ACK response should carry the final result (already supported). Consider
  optional progress reporting (e.g. batches sent/total) for long replays; that
  is a nice-to-have, not required for the first pass.

## Batch transport (decided)

- **CBOR** for every bulk endpoint (archive catch-up, `get_data_again`,
  `download_raw_data`). The server already accepts CBOR for observations.
- 20 records per request (working size; CBOR keeps a batch small, but bulk
  requests get a larger HTTP timeout than the current 10 s).
- **Whole-batch retry** on partial failure; safe because each record keeps its
  `message_id` and uniqueness constraints are per record.
- **Per-record outcome** in the response (accepted / duplicate / rejected) so
  the device knows exactly which `message_id`s it can advance past.
- Ordering: oldest first; `last_sent` advances only when the whole batch is
  accepted.

## Open questions for the owner

None outstanding for this backlog. `set_config` and `upload_logs` remain
unimplemented stubs; the owner will decide on them separately.
