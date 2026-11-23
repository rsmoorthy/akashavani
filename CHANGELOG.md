# Changelog

All notable changes are recorded here. The firmware version lives in
`firmware/VERSION`; the server version is in `server/pyproject.toml`.

## [Unreleased]

### Added

- Repository scaffold: firmware libraries and two thin sketches.- arduino-cli build/flash/monitor/test/OTA-export scripts.
- Native CMake + GoogleTest suite for Arduino-free logic.
- Hardware bring-up sketch.
- FastAPI ingest, command lease/ack, firmware release and admin APIs.
- SQLite development / PostgreSQL production configuration, Alembic migration.
- Docker image and compose files.
- JSON Schemas and examples for observations, commands, acknowledgements and OTA.
- Architecture, protocol, hardware, build, operations and ADR documentation.
- CI for firmware tests/build, server tests/lint, docs and Docker.
- Project handoff, current slice plan and agent instructions in `AGENTS.md` and
  `docs/development/handoff.md`.
- Two-step command-driven OTA: `get_fw_version`, `download_new_fw`
  (inactive-slot staging with streaming SHA-256) and `boot_with_new_fw`.
- Admin firmware upload (multipart), command history API and `/admin` UI.
- Device identity reporting (`X-Station-Id`, `X-Device-Mac`,
  `X-Firmware-Version`) stored on the device row.
- Explicit 4 MB receiver partition table with two 1.875 MiB OTA slots.
- Bench builds target the ESP32 classic boards; pins match the observed wiring.
- Provisioning AP with a MAC-derived SSID (`akashavani-XX`), fixed address
  `10.90.0.1` and a configurable `10.90.0.11`-`10.90.0.20` DHCP pool.
- Mobile-friendly status/config page on port 80, refreshed every 5 s, with
  station ID, server URL and six site Wi-Fi SSID/password pairs.
- Up to six ordered site Wi-Fi profiles tried sequentially by the STA side.
- `ap_client` bench sketch that exercises the AP page from the second board.
- RFM69 SX1231 driver implementing the validated Davis RF settings, bit order,
  RSSI, FIFO and 51-channel US hopping table.
- Davis CRC-16/CCITT decoder and encoder for every message type present in the
  captured ISS frames, with host tests using captured vectors.
- Command-driven transmitter simulator (`R1`/`R2`/`R3`, `S`, `X`, `?`) and a
  receiver RF bench mode with gap/RSSI/resync statistics.
- `tools/rf-bench.py` and `tools/rf-bench-report.py` for the three 15-minute
  RF scenarios, plus `make rf-bench`.
- `Ssd1306Display` I²C status panel driver (Adafruit SSD1306/GFX) showing the
  connected Wi-Fi SSID, IP/RSSI and application lines; enabled in both sketches.
- Wi-Fi station link in `transmitter_sim` (SSID display only, no cloud path).

### Added

- `X-Last-Radio-At` header and `devices.last_radio_at`; the 5-minute archive
  bucket is emitted even with zero valid frames, so radio health reaches the
  server every 5 minutes.
- `/admin` status columns: online/offline, last radio, latest bucket, frames and
  RSSI; CSV export of stored archive buckets (`GET /api/v1/admin/archive.csv`).
- SD `last_sent` marker, offline catch-up replay (oldest first, 20-record
  batches, `/api/v1/archive/batch` with per-record outcomes), 3-year archive
  retention, and `last_sent` on the AP page.
- Rolling raw 8-byte frame log on SD (one file per weekday, weekly truncate,
  5-minute flush) and a `raw_frames` status counter.
- Command lifecycle: one command in flight per device, attempt counter, ACK
  timeout with `get_pending_commands` probe and up to three retries.
- `get_data_again` command (bounded SD replay, upsert by `message_id`).
- `download_raw_data` command and raw CSV export; archive CSV export.
- Device health headers and admin columns (uptime, queue depth, free heap, reset
  reason, Wi-Fi RSSI).
- Archive catch-up batches use MessagePack (server also accepts JSON and CBOR).

### Changed

- Archive envelopes now carry `boot_id` and `uptime_ms`; the server stores them
  on the `archive` row. The original uploaded envelope moved from `archive.raw`
  to the new `raw_archive` table (migration `a7d3e1f9c2b4`).
- Renamed the project to `Akashavani` (corrected spelling): Python package,
  firmware libraries, classes, files, product strings, AP SSID prefix, default
  admin password and OTA artifact names now use the new spelling. JSON keys,
  HTTP routes, headers, device IDs and NVS keys are unchanged.
- Provisioning AP password is supplied at build time from `firmware/.env`
  (`AK_AP_PASSWORD`) or `receiver/secrets.h`; no default password is compiled
  in. It remains in the image and is not stored in NVS.
- Removed `AKASHAVANI_SITE_TIMEZONE`; there is no site-wide zone. Each device
  stores an IANA display zone (NVS `tz`, default `UTC`, set on its AP page) and
  reports it with its configuration. The server stores it in `devices.timezone`
  (migration `e2c7b5a41f90`).
- UTC is now the only stored and transported time zone: SD archive paths and
  records, the rolling raw log (UTC weekday and `...Z` stamps, previously IST),
  observations, commands and API responses.
- Device configuration exchange: `PUT /api/v1/devices/me/config` on boot, after
  an AP-page save and daily; the server mirrors the document in
  `devices.config` (migration `f3d18c6b2a70`). `get_config` and `push_config`
  replace `get_fw_version` for manual sync. `get_fw_version` is removed.
- Per-device latitude, longitude and altitude replace the site-wide
  `AKASHAVANI_SITE_*` variables and are editable on the AP page and `/admin`.
  Archive derivations store `NULL` when altitude is not yet known.
- `/admin` was reorganised into Devices, Download CSV, Firmware, Commands and
  Site Settings tabs with a mobile-friendly layout and a full device-config
  editor.
- `/admin` gained a Site TZ / UTC / Browser time selector (default Site TZ),
  relative Last Seen/Last Radio, second-resolution timestamps and CSV exports
  with both UTC and Site TZ columns. The token and time-display controls now
  share one collapsible panel that starts collapsed when a token is saved, so
  the device table sits at the top of the page.
- The public dashboard shows wind speed and gust in km/h (the API and archive
  remain m/s) and replaces the current-condition cards with `NA` when the
  device has not reported for more than 30 minutes.
- The server root `/` now redirects to `/dashboard`. `/admin` renders uptime as
  minutes, hours + minutes or days + hours. Station Identity gained an optional
  monitoring URL (`devices.monitoring_url`, migration `a3d8f2c6b1e4`), shown as a
  **Monitor Device** link on the public dashboard; the dashboard station label is
  a slug switcher backed by `GET /api/v1/public/stations`.
- Removed `station_id` end-to-end: DB column (migration `7d1c93a5e2b4`),
  `X-Station-Id` header/fallback, `DeviceOut`, `/admin` column, AP-page field,
  NVS key, `self_test` field and boot log. Identity is the device token plus
  the MAC-derived `device_id`; the shared-token fallback uses `X-Device-Id`.
- Command/health poll interval is 30 s (was 5 s; stored 60 s values migrate
  once). The device token is editable on the AP page.
- Serial log lines are CRLF-terminated, so plain `minicom`/`screen` no longer
  staircase after the first line.
- Provisioning docs read the device ID from the board's own status page; the
  serial console is optional.
- A blank board keeps its MAC-derived `ak-xxxxxx` device ID: the development
  seed and the station-ID field no longer rewrite it, and the status page shows
  the MAC alongside it.
- `make fw-monitor` supplies the ESP32 FQBN so it works with the unidentified
  bench ports.
- Both sketches hold their first log line for 3 s after power-up
  (`kSerialStartupDelayMs`) so a field console can attach and catch the banner.
- Command polling no longer requires a valid clock and defaults to 5 s.
- Command acknowledgements carry a structured `result`.
- Development identity/credentials are only seeded on a blank device; NVS wins
  after the first save through the AP page.
- Firmware version `0.3.2`.
- The local AP page is split into Status and Configuration tabs. The weather
  section shows last-known Temp, Int Temp, Wind and Rain values with relative
  ages, current-bucket valid/invalid frame counts and a relative last-frame age.
- RFM69 receive is interrupt-driven: DIO0 notifies a high-priority task that
  puts the radio in standby, drains the FIFO into a 128-frame RAM queue and owns
  the channel-hop/search timers, so blocking OTA/replay commands do not stop
  frame reception. Only CRC-valid frames are queued; invalid noise is counted
  and discarded. Frames carry their receive epoch and channel. Per bucket, the
  invalid-frame RSSI min/avg/max is sent to the server and stored on the archive
  row.

### Known gaps

- RFM69 modem configuration and Davis frame codec are not ported.
- The SSD1306 panel is implemented and visually confirmed on both bench boards;
  no automated visual test exists.
- OTA is SHA-256 verified but unsigned; TLS pinning and rollback are still
  release blockers.
