# 0004 — HTTP JSON by default, CBOR optional

## Context

The receiver must upload observations, poll commands and fetch firmware over
constrained links. JSON is easy to inspect and debug; CBOR reduces payload size
and parsing overhead. The server is Python and already has Pydantic validation.

## Decision

- Use HTTP(S) with a versioned envelope.
- Default to `application/json` during development.
- Accept `application/cbor` on ingest with the same keys/structure.
- Keep one schema source in `protocol/` and mirror it in Pydantic and C++.
- Use a per-device token in a header and a stable `message_id` for idempotency.

## Consequences

- Debugging with curl is trivial; CBOR can be enabled after measuring size/CPU.
- The firmware needs a CBOR library only if it chooses to send CBOR; the server
  supports both now.
- No long-lived MQTT connection; the device polls, which is simpler and works
  through restrictive networks, at the cost of command latency.
- Polling frequency and backoff must respect server load.
