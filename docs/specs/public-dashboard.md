# Public weather dashboard (`/dashboard`)

**Status:** implemented 2026-11-20 (backend, slug migration and React frontend)
**Owner request:** 2026-11-20
**Scope:** a read-only, unauthenticated, mobile-first replica of the useful parts
of the Weather Underground PWS dashboard, backed by new public read APIs.

## Confirmed decisions (2026-11-20)

| Topic | Decision |
| --- | --- |
| URL | `/dashboard/<slug>/`, one path per station. The slug is a unique, editable `devices.slug`; the heading shows `Device.name`. `/dashboard` redirects to the default slug when configured. |
| Slug administration | Generated from the display name when omitted, unique across devices, and editable together with the name/time zone in `/admin`. Renaming the display name does not change the slug. |
| Exposure | No auth on the page or public read APIs. Keep an explicit `AKASHAVANI_PUBLIC_DASHBOARD_ENABLED` switch (default off) so exposure is a deployment choice. |
| Location | Always use the device's reported site configuration. If latitude, longitude or altitude is missing, show **N/A**; no server-side override. |
| Time-of-day panel | Wunderground-style current-day scrubber that updates the condition cards; "Now" returns to the latest observation. |
| Units | **Metric only**: °C, km/h, mm, hPa, W/m², UV index. No unit switch, no imperial conversion in the UI or CSV. |
| Map / header / footer | Omitted. |
| Collapse All / Expand All | Omitted. |

## 0. Goal and non-goals

### Goal

Serve a public dashboard at `/dashboard/<slug>/` that shows the station in the
same shape as <https://www.wunderground.com/dashboard/pws/ICHIKK2>:

1. station heading: name, elevation, latitude/longitude, connected state and
   last-seen time;
2. current-condition cards from Temperature through Radiation;
3. a time-of-day strip for the current day;
4. Day / Week / Month / Custom date selection, with year and month dropdowns
   for the custom range;
5. a Summary table;
6. interactive graphs with cursor/touch readouts, plus a Table view;
7. a Download Data (CSV) button.

No authentication is required for the page or its read APIs. Existing APIs and
`/admin` keep their current auth behaviour.

### Non-goals

- Reimplementing Wunderground's header, footer, advertising, search, map,
  nearby stations, radar, air quality, or Wunderground branding.
- Copying Wunderground CSS, JavaScript, custom elements or image assets. The
  layout and interactions are replicated; the implementation is original.
- Grafana replacement. Grafana remains available separately.
- Per-user accounts, saved preferences on the server, or write access.
- Changing the ingest/archive contract. The only device schema additions are the
  unique `devices.slug` and the optional operator `devices.monitoring_url`;
  observations and archive rows are untouched.

## 1. Key constraints from the existing data

- `ArchiveRecord` is the only durable history. Observations may be sparse and
  are not guaranteed to contain the full field set. The dashboard reads
  **archive** rows, not raw observations.
- Archive rows are 5-minute buckets. A summary "high" is the highest stored
  5-minute mean, not the true instantaneous peak. This must be stated in the UI
  help text and the docs.
- All stored values are SI. The API returns SI; the UI displays metric units
  only: °C, km/h, mm, hPa, W/m² and UV index. Wind speed and gust are converted
  from m/s to km/h for display. There is no unit switch and no imperial
  conversion.
- `barometer_hpa` is only derived when the device has a site altitude. Without
  it, the dashboard must label `pressure_hpa` as station pressure, not
  sea-level pressure.
- `rain_rate_instant_mm_h` is the Davis instantaneous rate;
  `rain_rate_15m_mm_h` is the Weewx-style trailing rate. The graph uses the
  instant rate when present and falls back to the 15-minute rate. Both are in
  the CSV.
- The live bench device `ak-2731e0` currently has `site: null`, so elevation and
  coordinates are unavailable until the device configuration is saved. The UI
  must show **N/A** for each missing value, not invent values. Live devices are
  expected to report their site coordinates.
- The live archive currently contains mostly pressure/BME280 fields because the
  ISS frames are not yet flowing. The UI must handle null series gracefully.

## 2. Page layout

The page is one scrolling column. Width is capped at about 1400 px; all cards
use the same radius, border and spacing tokens.

```text
┌──────────────────────────────────────────────────────────────┐
│ Station name                                    [Connected]  │
│ Elev 306 m, 13.486 °N  77.709 °E      Last seen 11:20 IST    │
├──────────────────────────────────────────────────────────────┤
│ Temp │ Humidity │ Wind │ Precip │ Pressure │ UV │ Radiation  │
├──────────────────────────────────────────────────────────────┤
│ 12a   3a   6a   9a   12p   3p   6p   9p   12a                 │
│ ───────────────●─────────────────────────────  Now           │
├──────────────────────────────────────────────────────────────┤
│ ‹  Fri, Nov 20, 2026  ›   [Day] [Week] [Month] [Custom]      │
│ Summary table                          [Graph|Table] [CSV]  │
│ Temperature & Dew Point                                       │
│ Wind Speed & Gust                                             │
│ Wind Direction                                                │
│ Precipitation                                                 │
│ Pressure                                                      │
│ Solar Radiation & UV Index                                    │
└──────────────────────────────────────────────────────────────┘
```

Mobile behaviour:

- widgets: two per row at phone widths, three/four at tablet, seven at desktop;
- heading wraps; the status pill stays in the top-right;
- date navigation and presets wrap into two rows;
- Summary table scrolls horizontally with a sticky first column;
- charts are 240–280 px high and support touch tooltips;
- the CSV button becomes full width;
- no page-level horizontal scroll.

## 3. Behaviour

### 3.1 Heading

- **Title:** `Device.name`; fall back to `Device.id` if empty.
- **Elevation/coordinates:** `Elev {altitude_m} m, {lat} °N/S {lon} °E/W`, in
  the Wunderground style. Show **N/A** for any missing altitude, latitude or
  longitude. Do not hide the panel and do not substitute a server-side site.
- **Connected:** green dot when `last_seen_at` is within the existing 90-second
  online window; grey/red dot otherwise. Label `Connected` / `Offline`.
- **Last seen:** relative time plus absolute time in the station time zone.
  Also show a smaller "Data as of" time for the current reading.
- **Monitor Device:** when the device has a `monitoring_url`, show it as a
  **Monitor Device** link after Last seen / Data as of. The URL must be absolute
  `http`/`https`; no other scheme is rendered.
- **Station switcher:** the top-right `Station: <slug>` is a dropdown of public
  station slugs when more than one station is available. Choosing a slug
  navigates to `/dashboard/<slug>/`; a single station or a failed list request
  falls back to plain text.
- **Units:** metric only, with unit suffixes in the labels. Wind is shown in
  km/h; the API and archive store m/s.
- **Station time zone:** all displayed times use `Device.timezone`. The browser
  zone is never substituted. `Asia/Calcutta` (the live alias) works.

### 3.2 Current-condition cards

Ordered Temperature, Humidity, Wind, Precipitation, Pressure, UV, Radiation.

| Card | Main value | Secondary | Graphic |
| --- | --- | --- | --- |
| Temperature | `out_temp_c` | "Feels like" `app_temp_c` (fallback windchill/heatindex/temp) | semicircular arc gauge |
| Humidity | `out_humidity_pct` | dew point `dewpoint_c` | ring gauge |
| Wind | `wind_speed_m_s` shown in km/h | gust in km/h and compass direction | compass needle |
| Precipitation | today's rain total | current rate | rain bucket/droplet |
| Pressure | sea-level pressure if `barometer_hpa` exists, else station pressure | "sea-level" or "station" label | dial with needle |
| UV | `uv_index` | category (Low/Moderate/High/Very High/Extreme) | coloured scale |
| Radiation | `solar_wm2` | — | fill ring |

- The values come from the latest archive row, not from the selected history
  range.
- If `last_seen_at` is more than 30 minutes old (or missing), the cards show
  `NA` and their gauges are empty rather than presenting the stale archive
  value. Dragging the timeline to a historical observation still shows that
  observation.
- Missing values render as `—` with a `No sensor data` tooltip; the card is not
  hidden.
- A small "as of {time}" label appears when the timeline slider is scrubbed
  away from Now.
- PM2.5 is omitted: the station has no air-quality sensor.

### 3.3 Time-of-day strip

This is the Wunderground timeline slider, not a sunrise/sunset arc.

- Covers the current station-local day, 00:00–24:00, with major ticks at
  12a/3a/6a/9a/12p/3p/6p/9p/12a.
- Defaults to the latest observation ("Now").
- Dragging selects the nearest archive point and updates all current-condition
  cards and the "as of" label.
- "Now" returns to the latest observation and resumes live updates.
- If there is no data today, the slider is disabled and says "No observations
  today".
- It is independent of the history range selection: viewing last month does not
  change the timeline day.

### 3.4 Date-range selection

Presets: **Day**, **Week**, **Month**, **Custom**. Back/forward arrows move the
anchor date by one day/week/month. Forward is disabled when the range reaches
the station-local today.

| Preset | Range | Chart bucket |
| --- | --- | --- |
| Day | selected local date, 00:00–24:00 | 5 minutes |
| Week | 7 local days ending on the selected date | 1 hour |
| Month | selected local calendar month | 1 day |
| Custom | inclusive start/end dates | 5 min if ≤ 1 day, 1 h if ≤ 7 days, 1 day otherwise |

- Custom opens a panel with **year, month and day dropdowns** for both start and
  end (the owner explicitly asked for dropdowns rather than a calendar). The
  panel validates start ≤ end, end ≤ today and a maximum span of 31 days.
- The station is in the path: `/dashboard/ak-2731e0/`. The URL also carries the
  selection so links are shareable:
  `/dashboard/ak-2731e0/?period=day&date=2026-11-20` and
  `/dashboard/ak-2731e0/?period=custom&start=2026-11-01&end=2026-11-20`.
- `/dashboard` redirects to the configured default station, or shows a short
  "no default station configured" message when none is set.
- Back/forward browser navigation restores the previous selection.
- Range boundaries are resolved in the station time zone on the server, so DST
  transitions do not shift the requested local day.

### 3.5 Summary table

Rows match Wunderground's nine rows: Temperature, Dew Point, Humidity,
Precipitation Rate, Precipitation Accumulation, Wind Speed, Wind Gust, Wind
Direction, Pressure. Wind Speed and Wind Gust are displayed in km/h. Columns:
High, Low, Average, Total.

- Summary is computed from all raw 5-minute archive rows in the range, not from
  the chart buckets.
- Temperature, dew point, humidity, wind speed and gust: high = max, low = min,
  average = mean of non-null rows.
- Precipitation rate: high = max; average = mean of non-zero rates; low = —.
- Precipitation accumulation: for a single day, Total = sum; for multi-day,
  High = highest daily total, Average = mean of non-zero daily totals,
  Total = sum.
- Wind direction: circular (vector) mean only; High/Low/Total = —.
- Pressure: high/low only, matching Wunderground; average = —.
- Partial ranges get an asterisk and a note ("today is still in progress" or
  "some days have no data"), so a total is never presented as final when it is
  not.
- Empty values render `—`.

Solar radiation and UV are shown in the charts and data table but are not added
to the Summary table in v1, matching Wunderground.

### 3.6 Graphs and Table view

Six chart sections, each with a title and legend:

1. Temperature & Dew Point
2. Wind Speed & Gust (km/h)
3. Wind Direction
4. Precipitation
5. Pressure
6. Solar Radiation & UV Index

- Chart.js (the same library Wunderground uses) via a small React wrapper.
- `interaction: { mode: "index", intersect: false }`, so moving the cursor over
  a chart shows every series at that timestamp; a vertical crosshair line is
  drawn.
- Touch works: a tap/drag shows the same readout.
- Tooltips use station-local time and metric units.
- Missing buckets are gaps, not interpolated straight lines.
- Wind direction uses a 0–360° axis labelled N/E/S/W.
- Solar and UV share an x-axis with solar on the left and UV on the right.
- Subtle alternating day/night or day bands behind the line, like WU; low
  contrast, disabled under `prefers-reduced-motion`.
- No per-chart collapse control and no Collapse All/Expand All (owner: not
  needed).

A global **Graph / Table** segmented control switches the whole chart area to a
single scrollable data table at the same bucket resolution. The table has a
sticky timestamp column and one column per series. This satisfies the "same
graphs can be shown as Tables" requirement without duplicating six tables.

### 3.7 Download Data

`Download Data` downloads a CSV for the currently selected range:

- one row per raw 5-minute archive record, not per chart bucket;
- UTC timestamp and station-local timestamp columns;
- all display fields plus the rain-rate variants;
- metric units only;
- filename `akashavani-{slug}-{start_date}_{end_date}.csv`;
- maximum 31 days per request, enforced by the server;
- an empty range returns a CSV with headers only.

### 3.8 Refresh and errors

- Current station/status refreshes every 60 seconds; today's slider data and
  the visible history refresh every 5 minutes.
- Refresh pauses when the tab is hidden and resumes on visibility.
- A failed request shows a non-blocking error banner with Retry. Data already
  loaded remains visible, but the current-condition cards still apply the
  30-minute staleness rule.
- A range with no data shows the Summary table with dashes, a "No data for this
  period" note, and an empty chart state. It never shows fabricated values.
- This is an interactive dashboard; provide a `<noscript>` message instead of a
  server-rendered fallback.

## 4. Public API

Base path: `/api/v1/public`. No token, cookie or admin bearer is accepted or
required. The router is disabled (404) unless `AKASHAVANI_PUBLIC_DASHBOARD_ENABLED`
is true.

### 4.1 `GET /api/v1/public/stations/{slug}`

Station metadata, status and latest reading. The frontend derives `slug` from
the `/dashboard/<slug>/` path.

```json
{
  "station": {
    "id": "ak-2731e0",
    "slug": "roof-receiver",
    "name": "Roof receiver",
    "timezone": "Asia/Calcutta",
    "monitoring_url": "https://status.example.net/roof-receiver",
    "latitude": 13.486,
    "longitude": 77.709,
    "altitude_m": 306.0,
    "online": false,
    "last_seen_at": "2026-11-19T03:30:06Z",
    "first_archive_at": "2026-11-01T00:00:00Z",
    "latest_archive_at": "2026-11-19T03:30:00Z",
    "site_date": "2026-11-19"
  },
  "current": {
    "date_time": "2026-11-19T03:30:00Z",
    "out_temp_c": 27.0,
    "app_temp_c": 26.8,
    "dewpoint_c": 11.9,
    "out_humidity_pct": 39.0,
    "wind_speed_m_s": 0.0,
    "wind_gust_m_s": 0.0,
    "wind_dir_deg": null,
    "pressure_hpa": 915.3,
    "barometer_hpa": null,
    "rain_rate_instant_mm_h": 0.0,
    "rain_rate_15m_mm_h": 0.0,
    "rain_mm": 0.0,
    "solar_wm2": 836.6,
    "uv_index": 4.3
  },
  "today_rain_mm": null
}
```

- `online` uses the same 90-second window as the admin screen.
- `current` is the latest archive row; `null` if none.
- `today_rain_mm` is the sum for the station-local day, `null` if no data.
- `site_date` is the server's current date in the station zone. The client uses
  it for the date-picker maximum and the timeline day, avoiding browser-zone
  drift.
- `first_archive_at` bounds the date picker. It may be null.
- `monitoring_url` is the optional admin-provided link; `null` when unset. The
  dashboard renders it as **Monitor Device**.
- No MAC address, token hash, config, Wi-Fi, commands, RSSI, firmware or admin
  fields are returned.

### 4.2 `GET /api/v1/public/stations`

Compact public station list for the dashboard switcher. The allowlist applies
exactly as it does to `/{slug}`.

```json
[
  {"slug": "roof-receiver", "name": "Roof receiver"},
  {"slug": "garden", "name": "Garden station"}
]
```

- Sorted by slug.
- No credentials, coordinates or health fields.
- Same public cache header as `/{slug}`.

### 4.3 `GET /api/v1/public/stations/{slug}/history`

Query parameters:

| Name | Values | Notes |
| --- | --- | --- |
| `period` | `day`, `week`, `month`, `custom` | default `day` |
| `date` | `YYYY-MM-DD` | anchor for day/week/month; default `site_date` |
| `start_date` | `YYYY-MM-DD` | required for custom |
| `end_date` | `YYYY-MM-DD` | required for custom, inclusive |

Response:

```json
{
  "station_id": "ak-2731e0",
  "timezone": "Asia/Calcutta",
  "period": "day",
  "start_date": "2026-11-19",
  "end_date": "2026-11-19",
  "start": "2026-11-18T18:30:00Z",
  "end": "2026-11-19T18:30:00Z",
  "bucket_seconds": 300,
  "sample_count": 288,
  "coverage": {
    "days_in_range": 1,
    "days_with_data": 1,
    "includes_today": false
  },
  "points": [
    {
      "t": "2026-11-19T03:30:00Z",
      "out_temp_c": 27.0,
      "app_temp_c": 26.8,
      "dewpoint_c": 11.9,
      "out_humidity_pct": 39.0,
      "wind_speed_m_s": 0.0,
      "wind_gust_m_s": 0.0,
      "wind_dir_deg": null,
      "pressure_hpa": 915.3,
      "barometer_hpa": null,
      "rain_rate_mm_h": 0.0,
      "rain_mm": 0.0,
      "solar_wm2": 836.6,
      "uv_index": 4.3
    }
  ],
  "summary": {
    "temperature": {"high": 29.1, "low": 18.2, "average": 23.4, "total": null},
    "dew_point": {"high": 15.1, "low": 8.2, "average": 11.7, "total": null},
    "humidity": {"high": 88.0, "low": 35.0, "average": 61.0, "total": null},
    "precipitation_rate": {"high": 2.5, "low": null, "average": 0.4, "total": null},
    "precipitation_accumulation": {"high": null, "low": null, "average": null, "total": 4.2},
    "wind_speed": {"high": 6.1, "low": 0.0, "average": 2.2, "total": null},
    "wind_gust": {"high": 9.4, "low": 1.2, "average": 3.6, "total": null},
    "wind_direction": {"high": null, "low": null, "average": 213.4, "total": null},
    "pressure": {"high": 1012.1, "low": 1008.9, "average": null, "total": null}
  }
}
```

- `points` is a complete bucket grid across the range, including null buckets,
  so chart spacing matches time and gaps are visible.
- `ArchiveRecord.date_time` is the **bucket end** reported by the receiver.
  Range queries use bucket-start boundaries: the private implementation maps
  each row to `date_time - interval_minutes` for day/hour attribution and
  includes the final row whose bucket ends exactly at the range end. This keeps
  a 00:00 row from leaking into the next day's summary. CSV timestamps remain
  the stored bucket-end values.
- `pressure_hpa` in a point is the display value: `barometer_hpa` when present,
  else station pressure. `barometer_hpa` is kept separately so the UI can label
  it honestly.
- `rain_rate_mm_h` is `rain_rate_instant_mm_h` when present, else
  `rain_rate_15m_mm_h`.
- Bucket aggregations:
  - temperature, dew point, humidity, pressure, wind speed, solar, UV: mean;
  - wind gust: max;
  - wind direction: vector mean;
  - precipitation rate: max;
  - precipitation accumulation: sum.
- `sample_count` is the number of raw rows used by the summary.
- `coverage` lets the UI mark partial totals: `days_in_range` is the number of
  station-local calendar days in the selected range, `days_with_data` counts
  days with at least one archive row, and `includes_today` is true when the
  range contains the station-local current day. The Summary table uses it for
  the "today is still in progress" / "some days have no data" asterisk.
- Range validation: `end_date >= start_date`; day/week/custom end at or before
  `site_date`; a calendar-month view may include future days, which simply have
  null points. Inclusive span ≤ `AKASHAVANI_PUBLIC_MAX_RANGE_DAYS` (default 31).
  Invalid parameters return 422 with a readable message.
- Unknown device returns 404. A known device with no rows returns an empty grid,
  `sample_count: 0`, and null summary values.

### 4.4 `GET /api/v1/public/stations/{slug}/export.csv`

Same `period`/`date`/`start_date`/`end_date` parameters. Metric only.

Columns:

```text
timestamp_utc,timestamp_site,out_temp_c,app_temp_c,dewpoint_c,
out_humidity_pct,wind_speed_m_s,wind_gust_m_s,wind_dir_deg,
pressure_hpa,barometer_hpa,rain_rate_instant_mm_h,
rain_rate_15m_mm_h,rain_mm,solar_wm2,uv_index
```

The CSV includes both timestamps and both pressure definitions. Timestamps are
the stored archive bucket ends. It is a raw 5-minute export.

### 4.5 Caching

- Public JSON: `Cache-Control: public, max-age=60`.
- CSV: `Cache-Control: no-store`.
- Hashed dashboard assets: `Cache-Control: public, max-age=31536000, immutable`.
- `index.html`: `Cache-Control: no-cache`.
- No CORS changes: the dashboard is same-origin.

## 5. Server implementation

New and changed files (indicative):

```text
server/src/akashavani_server/
  api/public.py              new router, no auth dependency
  api/admin.py               slug generation/edit + PATCH device identity
  services/dashboard.py      range resolution, bucketing, summary, CSV rows
  services/presence.py       shared 90-second online rule (also used by admin)
  slugs.py                   slugify, validation, uniqueness helpers
  schemas.py                 public response models, DevicePatch, Slug
  config.py                  public-dashboard settings
  main.py                    include public router; mount dashboard assets
  web.py                     /dashboard static serving
server/migrations/versions/   add unique devices.slug and backfill
server/tests/
  test_public_dashboard.py   API, range, DST, summary, CSV, no-leak tests
server/dashboard/            React/Vite/TypeScript frontend (see below)
```

Details:

- Dashboard routing: `/dashboard` returns a 302 to
  `/dashboard/{AKASHAVANI_PUBLIC_DEFAULT_DEVICE}/` (or a small HTML message when
  unset); `GET /dashboard/{slug}/` and its no-trailing-slash form return the
  built `index.html`; `/dashboard/assets/*` is a static mount. Unknown slugs
  still get the SPA, and the API's 404 becomes a "station not found" state.
- Keep routers thin; put range math, bucketing and summary in
  `services/dashboard.py` as pure functions so they are unit-testable without a
  database.
- Resolve local date boundaries with `zoneinfo` and `Device.timezone`. Test a
  DST-transition zone even though the live station is in India.
- A single range query returns all rows; aggregation is done in Python. The
  maximum range (31 days × 288 rows ≈ 8,928 rows) is small and avoids
  SQL-dialect-specific date functions for SQLite and PostgreSQL.
- Compute the bucket grid before filling values so the chart always has one
  point per interval, including nulls.
- Add `PublicStationOut`, `PublicCurrentOut`, `HistoryOut`, `HistoryPointOut`,
  `SummaryMetricOut`, etc. to `schemas.py`; do not return ORM rows directly.
- Compute `today_rain_mm` and `current` with bounded queries (latest row, and a
  sum for the local day).
- Reuse the online rule from admin; move the constant/function rather than
  importing a private admin helper.
- Add a unique `devices.slug` and a migration that backfills existing rows
  deterministically; the live development DB was migrated on a copy and
  verified (2026-11-20).
- No changes to existing ingest/command/OTA routes. The existing admin
  create/rotate endpoint remains, with optional `slug`; `PATCH
  /api/v1/admin/devices/{id}` edits name/slug/time zone without rotating the
  device token.

## 6. Frontend implementation

### Choice

React 19 + TypeScript + Vite, source under `server/dashboard/`, built to
`server/dashboard/dist`, served by FastAPI at `/dashboard/<slug>/`.

Why React:

- interactive range, graph/table and timeline state is easier to keep
  consistent in components;
- typed API models catch contract drift;
- Chart.js lifecycle and responsive resizing are safer in a component;
- the owner is comfortable with React and it leaves room for a future admin
  integration.

Why Chart.js:

- Wunderground itself uses Chart.js;
- built-in index-mode tooltips, touch support and responsive canvas;
- one small dependency; a custom crosshair plugin is about 30 lines.

No Tailwind, no component library, no icon package. Plain CSS with custom
properties and system fonts keeps the bundle small and the look original.

### Structure

```text
server/dashboard/
  package.json / package-lock.json
  tsconfig.json
  vite.config.ts                 base "/dashboard/", /api proxy
  index.html
  src/main.tsx
  src/App.tsx
  src/api.ts                     fetch wrappers + AbortController
  src/types.ts                   mirrors the public response models
  src/format.ts                  metric labels + station-zone time/date formatting
  src/stationPath.ts             reads /dashboard/<slug>/ from the URL
  src/useDashboard.ts            bootstrap/range/today/refresh state
  src/components/
    StationHeader.tsx
    WidgetCard.tsx
    widgets/*.tsx                 SVG gauge variants
    TimelineSlider.tsx
    RangeControls.tsx
    CustomRangePanel.tsx
    SummaryTable.tsx
    ChartSection.tsx
    HistoryTable.tsx
    DownloadButton.tsx
    ErrorBanner.tsx
  src/charts/
    chartTheme.ts
    crosshairPlugin.ts
    TempDewPointChart.tsx ... six charts
  src/styles/app.css
  src/test/*.test.ts(x)
```

### State and fetching

- `useDashboard` owns: station bootstrap, selected period/date, history,
  today's points, selected timeline point, loading/error.
- The station slug is read from `/dashboard/<slug>/`; `/dashboard` has already
  redirected by the time React runs.
- On load: fetch `/stations/{id}` and a `day` history for today for the slider
  in parallel.
- On range change: fetch history only; keep the header/current.
- On timeline move: no network request; select the nearest point from today's
  points and pass it to the widget grid.
- Query parameters are the source of truth for period/date; update with
  `history.replaceState`. Listen for `popstate`.
- Auto-refresh pauses when `document.hidden`.
- Abort in-flight requests on range changes to avoid out-of-order rendering.

### Accessibility and polish

- Semantic headings, `<button>` for controls, visible focus rings, `aria-label`
  on icon-only arrows.
- Charts: `role="img"` with an `aria-label` summary; the Table view is the
  accessible alternative.
- `prefers-color-scheme` light/dark palette; `prefers-reduced-motion`.
- No external fonts, analytics or trackers. This is a weather station UI, not a
  marketing page.

## 7. Build, Docker, CI

### Local

```bash
make dashboard-setup     # npm ci in server/dashboard
make dashboard-dev       # Vite on 5173, proxies /api to 127.0.0.1:8000
make dashboard-build     # production assets in server/dashboard/dist
make dashboard-test      # unit tests + typecheck + lint
make server-dev          # serves /dashboard/<slug>/ from the built dist
```

### Docker

- Add a `node:22-alpine` stage to `server/Dockerfile` to run `npm ci` and
  `npm run build`.
- Copy `dist` to `/app/dashboard/dist` in the runtime stage.
- Set `AKASHAVANI_DASHBOARD_DIR=/app/dashboard/dist`.
- Keep the Python wheel independent of the frontend; the Docker image and the
  repository are the supported deployment paths.
- Update `server/.dockerignore` for `dashboard/node_modules`.

### CI

Add a `dashboard` job alongside the existing jobs:

```yaml
- uses: actions/setup-node@v4
  with: { node-version: "22", cache: npm, cache-dependency-path: server/dashboard/package-lock.json }
- run: npm ci
  working-directory: server/dashboard
- run: npm run lint
- run: npm run typecheck
- run: npm test -- --run
- run: npm run build
```

The server job does not need Node. The Docker job already builds the image, so
it covers the integration path.

### Config

| Variable | Default | Purpose |
| --- | --- | --- |
| `AKASHAVANI_PUBLIC_DASHBOARD_ENABLED` | `false` (recommended) | expose `/api/v1/public` and `/dashboard` |
| `AKASHAVANI_PUBLIC_DEFAULT_SLUG` | empty | station slug `/dashboard` redirects to; if empty, `/dashboard` shows a configuration message |
| `AKASHAVANI_PUBLIC_DEVICE_IDS` | empty | optional comma-separated allowlist of devices visible through the public API; empty = all |
| `AKASHAVANI_PUBLIC_MAX_RANGE_DAYS` | `31` | custom-range and CSV cap |
| `AKASHAVANI_DASHBOARD_DIR` | `./dashboard/dist` | built frontend assets |
| `AKASHAVANI_PUBLIC_CACHE_SECONDS` | `60` | public JSON cache header |

Security note: no auth is a deliberate product decision. The public API exposes
the station name, coordinates, elevation, weather history and the optional
monitoring URL. Never put credentials in the monitoring URL. Deploy behind HTTPS
and consider reverse-proxy rate limiting. The enable flag is off by default so
exposure is explicit.

## 8. Tests and acceptance

### Backend tests

- slug generation from a name, uniqueness suffixes, reserved names and rename
  without breaking existing links;
- station payload and 404 for unknown slug;
- public station list and allowlist filtering;
- optional `monitoring_url` saved from admin, validated as http(s), published on
  the station payload, and cleared with an empty value;
- `/dashboard` redirect to the default slug and the no-default message;
- `/` redirects to `/dashboard`;
- `online` boundary at 90 seconds;
- missing site values render as `null` in the API (the UI shows N/A);
- no sensitive fields in any public payload;
- day/week/month/custom boundaries in `Asia/Calcutta` and a DST zone;
- future and inverted ranges rejected; 31-day cap enforced;
- bucket grid includes null gaps;
- summary high/low/mean, nonzero precipitation mean, daily precipitation
  totals, circular wind direction, pressure high/low only;
- CSV headers, row count, UTC/site timestamps, metric values and filename;
- disabled flag returns 404 for every public route;
- allowlist hides non-listed devices;
- cache headers.

### Frontend tests

- metric labels and formatting;
- station-zone date/time formatting;
- nearest-observation selection for the timeline, including no-data;
- range URL encode/decode and back/forward state;
- SummaryTable and HistoryTable rendering with null values;
- custom range validation (inverted, future, > 31 days);
- empty and error states;
- Monitor Device link and station switcher rendering.

### Manual/browser acceptance

- `make server-test`, `make lint`, `make docs-build`, `make dashboard-test`,
  `make dashboard-build`, `make docker-build`.
- Playwright screenshots at 390×844 and 1280×800 (light and dark if time
  permits); no page-level horizontal scroll at 360 px.
- Cursor and touch tooltip evidence for at least the temperature and wind
  charts.
- `curl` evidence for `/api/v1/public/stations`,
  `/api/v1/public/stations/roof-receiver`, `/history?period=day`,
  `/history?period=custom`, and `export.csv` without any Authorization header.
- A CSV sample with both timestamps.
- Empty-data evidence using a device with no archive rows.

Acceptance is not complete until the owner confirms the visual layout and the
mobile experience; the agent can provide screenshots but cannot judge site
aesthetics or real sensor values.

## 9. Resolved defaults

The implementation uses the recommended defaults; no item remains blocking.

1. **Summary rows:** Wunderground's nine rows; solar/UV stay in the charts and
   table only.
2. **Historical resolution:** Day = 5 min, Week = 1 h, Month = 1 day, Custom
   auto; summary always from 5-minute rows.
3. **Data table:** one combined table for all series at the chart bucket
   resolution.
4. **Branding:** `Device.name` in the heading, with the slug shown as the
   station identifier.
5. **Rate limiting:** no application-level limit in v1; deploy behind HTTPS and
   a reverse proxy with rate limiting.

## 10. Implementation evidence (2026-11-20)

- Backend: 94 pytest tests pass; Ruff and mypy pass.
- Frontend: 34 Vitest tests pass; TypeScript and ESLint pass; Vite production
  build succeeds (about 150 kB gzipped JS including Chart.js).
- Migration tested on a copy of the live development database: up, down and up
  again, with deterministic slug backfill.
- Browser checks with a populated demo database:
  - desktop 1280×1000 and mobile 390×844 screenshots;
  - no page-level horizontal overflow at 390 px;
  - Day, Week, Graph/Table, Custom dropdown and timeline scrubber exercised;
  - no console or page errors.
- Manual unauthenticated `curl` checks for station, day/week history and CSV;
  CSV export and hashed asset caching headers verified.
- Docker image built with the Node frontend stage; a container smoke test
  returned 200 for `/dashboard/simulator/`, its JS asset and the public API.
- The live development server on port 8000 was not restarted and the live
  database was not modified; the migration will apply on the next start.
