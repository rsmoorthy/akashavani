# 0007 — SQLite for development, PostgreSQL for production

## Context

Local development should start with one command and no database container.
Production needs concurrent writers, backups and operational maturity.

## Decision

- Default `AKASHAVANI_DATABASE_URL` to a local SQLite file.
- Support PostgreSQL through the same SQLAlchemy models and an optional compose
  override.
- Use Alembic as the only production schema mechanism.
- Auto-create tables only when `environment=test`.

## Consequences

- Fast onboarding and tests.
- SQLite ignores `SELECT ... FOR UPDATE SKIP LOCKED`; command leasing is safe
  with one worker. Multiple production workers require PostgreSQL.
- JSON columns and timestamp behaviour differ subtly between backends; the API
  normalizes where it matters and tests run on SQLite. Add a Postgres CI job
  before relying on backend-specific features.
- Backups and retention become operational responsibilities, not app behaviour.
