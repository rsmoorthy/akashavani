# SD archive

The SD card is the durable copy of every accepted observation. It is independent
of cloud connectivity.

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
than a deferred record.

## Offload and recovery

1. Power down and remove the card, or copy over the network in a future admin
   feature.
2. Copy `/archive` to a workstation.
3. Validate JSON lines:

   ```bash
   jq -c . archive/ak-3f2a1b/2026/10/*.jsonl >/dev/null
   ```

4. Reconcile against the server using `(device_id, message_id)`; duplicates are
   harmless.
5. Optionally re-post missing records with the same `message_id`.

Do not reformat or delete the card until the offload is verified.

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
