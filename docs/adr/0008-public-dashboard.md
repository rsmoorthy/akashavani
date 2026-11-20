# 0008 — Public read-only dashboard

## Context

The project owner's end users are comfortable with the Weather Underground PWS
dashboard. Grafana remains the operator-facing time-series tool, but the public
site needs a familiar, mobile-first view of the current conditions, summary,
graphs and CSV export.

The server already stores the required history in `archive` rows at 5-minute
resolution. The dashboard must not require an operator token and must not expose
device credentials.

## Decision

- Serve a React + TypeScript + Vite dashboard at `/dashboard/<slug>/`.
- Add a dedicated, unauthenticated read-only API under `/api/v1/public`.
- Use an explicit `AKASHAVANI_PUBLIC_DASHBOARD_ENABLED` switch; exposure is a
  deployment decision, not an implicit consequence of starting the server.
- Give each device a unique, editable `slug` instead of putting the display name
  or a mutable name-based slug in the URL. The database migration backfills
  existing rows.
- Keep the existing authenticated APIs and `/admin` unchanged.
- Build the frontend in a Node stage for Docker; the Python wheel does not
  contain frontend assets.

## Consequences

- The public API exposes station metadata, coordinates, elevation and weather
  history. Deploy behind HTTPS and use an allowlist or reverse-proxy rate limit
  when hosting more than one station.
- There is no application-level authentication or write path for the public
  dashboard. Any future write feature needs a separate authenticated design.
- The frontend build adds Node 24 and npm to the development/CI toolchain, but
  not to the runtime image.
- The dashboard must handle sparse archive data honestly: `—` for missing
  values, no interpolation, and partial-range notes on totals.
- Slug renames are explicit in `/admin`; display-name changes do not break
  public links.
