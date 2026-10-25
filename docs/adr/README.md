# Architecture decision records

| ADR | Decision | Status |
| --- | --- | --- |
| [0001](0001-monorepo.md) | Keep firmware, protocol and server in one repository | accepted |
| [0002](0002-arduino-cli.md) | arduino-cli with local Arduino libraries and explicit FQBNs | accepted |
| [0003](0003-host-testable-cores.md) | Keep protocol/domain logic Arduino-free and test on the host | accepted |
| [0004](0004-http-json-cbor.md) | HTTP with JSON default and CBOR optional | accepted |
| [0005](0005-sd-jsonl.md) | Daily JSON Lines archive on micro-SD | accepted |
| [0006](0006-ota.md) | Both LAN ArduinoOTA and remote HTTP pull OTA | accepted |
| [0007](0007-database.md) | SQLite for development, PostgreSQL for production, Alembic migrations | accepted |

ADRs are short: context, decision, consequences. Supersede rather than rewrite
when a decision changes.
