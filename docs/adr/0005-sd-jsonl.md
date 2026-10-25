# 0005 — Daily JSON Lines archive on micro-SD

## Context

The receiver must preserve observations when the cloud is unavailable. The card
is removed and read on a workstation. Power loss is expected.

## Decision

- Store one JSON object per line at
  `/archive/<device>/<YYYY>/<MM>/<DD>.jsonl`.
- Append and flush per record by default.
- Never rewrite or compact on the device.
- Use the same canonical object as the upload.

## Consequences

- Recovery is trivial and a truncated last line costs one record.
- Storage overhead is higher than a binary format; acceptable at weather-station
  rates.
- Card wear is traded for durability; revisit with batching if measurements show
  a problem.
- Retention and offload are explicit operations, not hidden firmware behaviour.
