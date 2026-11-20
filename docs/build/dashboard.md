# Public dashboard

The dashboard is a React + TypeScript + Vite application in
`server/dashboard/`. It is served as static files by FastAPI at
`/dashboard/<slug>/`; it does not need a Node runtime in production.

## Prerequisites

- Node.js 24 (Vite 8 requires Node 22.12 or newer; Node 24 is the tested
  toolchain)
- npm
- the Python server environment (`make server-setup`)

## Develop

```bash
make dashboard-setup     # npm ci
make server-dev          # API on http://127.0.0.1:8000
make dashboard-dev       # Vite on http://127.0.0.1:5173
```

The Vite dev server proxies `/api` to the FastAPI server. Open
`http://127.0.0.1:5173/dashboard/<slug>/`, or use the built page through
FastAPI after `make dashboard-build`.

The public API and page must be enabled in `server/.env` or the environment:

```dotenv
AKASHAVANI_PUBLIC_DASHBOARD_ENABLED=true
AKASHAVANI_PUBLIC_DEFAULT_SLUG=simulator
```

`/dashboard` redirects to the default slug. Direct links use
`/dashboard/<slug>/`. Slugs are managed in `/admin` under **Station identity
and public URL**.

## Build

```bash
make dashboard-build     # writes server/dashboard/dist
make server-dev          # serves /dashboard/<slug>/
```

FastAPI reads `AKASHAVANI_DASHBOARD_DIR` (default `./dashboard/dist`). The
Docker image builds the dashboard in a Node stage and copies it to
`/app/dashboard/dist`; the Python wheel does not contain frontend assets.

## Test

```bash
make dashboard-test      # Vitest + TypeScript check
make dashboard-lint      # ESLint
make dashboard-build     # production bundle
```

The frontend tests cover range parsing, station-local formatting, timeline
selection, unit labels, API URL construction and the table/widget components.
The backend tests cover the public API contract, range resolution, DST
boundaries, summary calculations, CSV export and slug uniqueness.

## Structure

```text
server/dashboard/
├── src/
│   ├── App.tsx                 page composition and timeline selection
│   ├── api.ts                  public API client
│   ├── format.ts               metric labels + station-zone date/time
│   ├── range.ts                Day/Week/Month/Custom state and URL
│   ├── timeline.ts             current-day scrubber helpers
│   ├── useDashboard.ts         fetch, refresh and selection state
│   ├── charts/                 Chart.js configuration and six charts
│   ├── components/             header, widgets, controls, tables, charts
│   └── styles/app.css          responsive light/dark layout
├── index.html
├── vite.config.ts
└── package.json
```

The chart layer uses Chart.js index-mode tooltips and a small crosshair plugin.
The summary is computed by the server from raw 5-minute archive rows; the
frontend never invents or interpolates missing values.
