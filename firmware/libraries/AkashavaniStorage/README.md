# AkashavaniStorage

The SD card is the durable copy of every received observation, independent of
whether the cloud upload succeeded.

```text
/archive/<device-id>/<YYYY>/<MM>/<DD>.jsonl
```

Each line is one self-contained JSON object. The format is append-only and is
not rewritten or compacted on the device. See
[`docs/operations/archive.md`](../../../docs/operations/archive.md).

`ArchivePath.*` is host-safe and covered by native tests. `SdArchive.*` is the
ESP32 writer and is intentionally thin: mount, create the day's directory, append
a line, flush.

## Why JSON Lines on SD?

- Recovery is trivial: copy the card and read text.
- A truncated final line only loses one record.
- The same payload can be archived before and after canonicalisation.
- No database or filesystem-specific tooling on the device.

If record volume makes JSON too large, switch to a compressed binary format
behind `IArchive`; the rest of the firmware does not care.
