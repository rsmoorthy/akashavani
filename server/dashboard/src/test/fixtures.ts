import type {
  Current,
  DisplayValues,
  History,
  HistoryPoint,
  HistorySummary,
  Station,
} from '../types'

export function stationFixture(overrides: Partial<Station> = {}): Station {
  return {
    id: 'ak-2731e0',
    slug: 'roof-receiver',
    name: 'Roof receiver',
    timezone: 'Asia/Calcutta',
    monitoring_url: null,
    latitude: 13.486,
    longitude: 77.709,
    altitude_m: 306,
    online: true,
    last_seen_at: '2026-11-20T05:25:06Z',
    first_archive_at: '2026-11-12T06:35:00Z',
    latest_archive_at: '2026-11-20T05:20:00Z',
    site_date: '2026-11-20',
    ...overrides,
  }
}

export function currentFixture(overrides: Partial<Current> = {}): Current {
  return {
    date_time: '2026-11-20T05:20:00Z',
    out_temp_c: 27,
    app_temp_c: 26.8,
    dewpoint_c: 11.9,
    out_humidity_pct: 39,
    wind_speed_m_s: 1.2,
    wind_gust_m_s: 2.8,
    wind_dir_deg: 210,
    pressure_hpa: 1008.5,
    barometer_hpa: 1008.5,
    rain_rate_instant_mm_h: 0,
    rain_rate_15m_mm_h: 0,
    rain_mm: 0,
    solar_wm2: 836,
    uv_index: 4.3,
    ...overrides,
  }
}

export function pointFixture(t: string, values: Partial<HistoryPoint> = {}): HistoryPoint {
  return {
    t,
    out_temp_c: null,
    app_temp_c: null,
    dewpoint_c: null,
    out_humidity_pct: null,
    wind_speed_m_s: null,
    wind_gust_m_s: null,
    wind_dir_deg: null,
    pressure_hpa: null,
    barometer_hpa: null,
    rain_rate_mm_h: null,
    rain_mm: null,
    solar_wm2: null,
    uv_index: null,
    ...values,
  }
}

export function summaryMetric(high: number | null = null) {
  return { high, low: high, average: high, total: high }
}

export function summaryFixture(): HistorySummary {
  return {
    temperature: summaryMetric(24),
    dew_point: summaryMetric(14),
    humidity: summaryMetric(60),
    precipitation_rate: { high: 2, low: null, average: 1, total: null },
    precipitation_accumulation: { high: null, low: null, average: null, total: 1.5 },
    wind_speed: summaryMetric(3),
    wind_gust: summaryMetric(5),
    wind_direction: { high: null, low: null, average: 210, total: null },
    pressure: { high: 1010, low: 1008, average: null, total: null },
  }
}

export function historyFixture(overrides: Partial<History> = {}): History {
  return {
    station_id: 'ak-2731e0',
    timezone: 'Asia/Calcutta',
    period: 'day',
    start_date: '2026-11-20',
    end_date: '2026-11-20',
    start: '2026-11-19T18:30:00Z',
    end: '2026-11-20T18:30:00Z',
    bucket_seconds: 300,
    sample_count: 1,
    coverage: { days_in_range: 1, days_with_data: 1, includes_today: true },
    points: [pointFixture('2026-11-20T03:30:00Z', { out_temp_c: 24 })],
    summary: summaryFixture(),
    ...overrides,
  }
}

export function displayValuesFixture(overrides: Partial<DisplayValues> = {}): DisplayValues {
  return {
    time: '2026-11-20T05:20:00Z',
    out_temp_c: 27,
    app_temp_c: 26.8,
    dewpoint_c: 11.9,
    out_humidity_pct: 39,
    wind_speed_m_s: 1.2,
    wind_gust_m_s: 2.8,
    wind_dir_deg: 210,
    pressure_hpa: 1008.5,
    barometer_hpa: 1008.5,
    rain_rate_mm_h: 0,
    rain_total_mm: 0,
    solar_wm2: 836,
    uv_index: 4.3,
    ...overrides,
  }
}
