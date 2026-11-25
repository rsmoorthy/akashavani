# SD archive

The SD card is the durable copy of every accepted observation. It is independent
of cloud connectivity.

## 5-minute archive records

The receiver now collates decoded Davis frames into one SI archive record per
5-minute bucket and writes that record to the card. The raw per-frame upload path
remains available for diagnosis, but the durable weather archive is the 5-minute
record. Each line has `"type": "archive"` and the sums/counts described in
`docs/specs/davis-archive-collation.md`. Rows written by current firmware also
carry `boot_id` (16 hex characters, generated once per boot) and `uptime_ms`
(device `millis()` at bucket close), so the server can group rows by boot and
count restarts. The server divides the sums, converts rain tips using the
configured bucket size and stores the result in the SQLite `archive` table; the
complete uploaded envelope is kept in `raw_archive`.

## Boot and restart tracking

Rows written by current firmware carry the boot that produced them. To list
boots observed in the archive:

```sql
SELECT boot_id, MIN(date_time) AS first_bucket, MAX(date_time) AS last_bucket,
       MAX(uptime_ms) AS uptime_at_last_bucket
FROM archive
WHERE device_id = 'ak-3f2a1b'
GROUP BY boot_id
ORDER BY first_bucket;
```

To count observed boots in a period:

```sql
SELECT COUNT(DISTINCT boot_id)
FROM archive
WHERE device_id = 'ak-3f2a1b'
  AND date_time BETWEEN '2026-11-01T00:00:00Z' AND '2026-12-01T00:00:00Z';
```

A change in `boot_id` between adjacent rows is a restart. `uptime_ms` restarts
at zero on each boot and wraps after about 49.7 days, so use `boot_id` (not a
drop in `uptime_ms`) to detect reboots. A boot shorter than one 5-minute bucket
produces no row and is therefore not counted.

## Layout

```text
/archive/<device-id>/<YYYY>/<MM>/<DD>.jsonl
```

Example:

```text
/archive/ak-3f2a1b/2026/10/25.jsonl
```

Each line is one complete JSON object, identical to the upload envelope. The
receiver writes the line and immediately flushes by default.

```json
{"v":1,"type":"observation","device_id":"ak-3f2a1b","message_id":"ak-3f2a1b-1792908000-42","observed_at":"2026-10-25T06:00:00Z","sequence":42,"source":{"protocol":"davis","channel":0,"transmitter_id":1,"rssi_dbm":-72},"data":{"temperature_c":21.5,"humidity_pct":55,"wind_speed_ms":2.5}}
```

## Why JSON Lines

- a truncated final line loses one record, not the whole file;
- the card is readable on any machine without custom tooling;
- the archived object is the same object sent to the cloud, simplifying
  reconciliation;
- append-only writes avoid rewriting the card.

If volume becomes a problem, replace the format behind `IArchive` with compressed
binary chunks. Do not change the cloud payload as part of that change.

## Time and paths

The path uses the receiver's UTC date at write time, not the station's local
date. The observation's own `observed_at` is inside the record. The receiver
refuses to write when the clock is invalid, because a wrong directory is worse
than a deferred record. The device's configured time zone is display-only and
never affects the path or the stored timestamp.

## Offload and recovery

The admin UI **Storage** tab can inspect the card without removing it:

- `storage_status` reports SD totals, archive write status, `last_sent`, raw/log
  counters and the current archive path.
- `sd_list` lists `/archive`, `/raw`, `/logs`, `/state` or `/`.
- `sd_read` returns one chunk of a file; increase `offset` until `eof`.

The device uploads the listing or chunk to the server, and the admin page offers
a download link. Paths outside those roots and `..` components are rejected.

For physical offload:

1. Power down and remove the card, or use `sd_list`/`sd_read` over the network.
2. Copy `/archive` to a workstation.
3. Validate JSON lines:

   ```bash
   jq -c . archive/ak-3f2a1b/2026/10/*.jsonl >/dev/null
   ```

4. Reconcile against the server using `(device_id, message_id)`; duplicates are
   harmless.
5. Optionally re-post missing records with the same `message_id`.

Do not reformat or delete the card until the offload is verified.

## Automatic catch-up and `last_sent`

`last_sent` is a durable watermark in `/archive/<device-id>/last_sent`. The
receiver advances it only to the next contiguous 5-minute bucket acknowledged by
the server. It does not send a live bucket while an older bucket is still
unsent; SD replay runs first. If SD is missing a bucket, the watermark holds at
the gap and the condition is logged. Server-side idempotency makes replaying
duplicates safe.

## Device logs

When SD is mounted, Info/Warn/Error lines are also buffered in a fixed 4 KB RAM
buffer and written to `/logs/<device-id>/<YYYY>/<MM>/<DD>.log`. The main loop
flushes when the buffer is full or after 60 seconds. Per-frame `rx` lines are
Debug and are not written to SD. The sink caps each daily file at 256 KB and
counts dropped lines. Use `sd_read` or `sd_list` to retrieve logs remotely; the
tail of the buffer can be lost on an unexpected power cut.

## Retention

The scaffold never deletes. Define a retention policy before the card fills:

- capacity depends on record rate and JSON size;
- a 60-second cadence is ~1,440 records/day, likely a few MB/year;
- keep at least the longest expected offline window plus a margin.

Add retention as an explicit, tested operation (oldest month first) rather than
an ad-hoc delete in the receive path.

## Wear and durability

- Flushing every record favours durability over card wear; adjust
  `SdArchiveConfig::flushEachRecord` if needed.
- Use an industrial/High Endurance card for field deployment.
- Power loss can leave a partial last line; parsers should skip it.
- Test unmount/power loss and reinsertion on the bench.
- A missing card is logged but does not stop reception; those records are not
  archived. Monitor `archive_ready` on the local status page.

## Privacy

Observations contain station metadata and RSSI, not credentials. The card also
does not contain Wi-Fi or cloud tokens. Still treat a lost card as sensitive site
data and erase it before disposal.
