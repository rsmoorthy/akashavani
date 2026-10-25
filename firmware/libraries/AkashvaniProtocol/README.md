# AkashvaniProtocol

The C++ half of the wire contract. The normative definition lives in
[`protocol/schema/`](../../../protocol/schema); this library contains only the
constants and validators needed by firmware. The server has Pydantic models
that mirror the same schemas.

Supported encodings:

- `application/json` (default, human-readable, used during development)
- `application/cbor` (preferred on constrained links once verified)

Both encodings use the same envelope and field names. Protocol version `1` is
not changed in place: additive optional fields are allowed, incompatible changes
require a new version.

## Envelope

```json
{
  "v": 1,
  "type": "observation",
  "device_id": "ak-3f2a1b",
  "message_id": "ak-3f2a1b-1760000000-42",
  "observed_at": "2026-10-25T06:00:00Z",
  "sequence": 42
}
```

`message_id` is the idempotency key. Re-sending the same observation after a
timeout must use the same ID. See `docs/protocol/README.md`.
