export type Period = 'day' | 'week' | 'month' | 'custom'

export interface Station {
  id: string
  slug: string
  name: string
  timezone: string
  latitude: number | null
  longitude: number | null
  altitude_m: number | null
  online: boolean
  last_seen_at: string | null
  first_archive_at: string | null
  latest_archive_at: string | null
  site_date: string
}

export interface Current {
  date_time: string
  out_temp_c: number | null
  app_temp_c: number | null
  dewpoint_c: number | null
  out_humidity_pct: number | null
  wind_speed_m_s: number | null
  wind_gust_m_s: number | null
  wind_dir_deg: number | null
  pressure_hpa: number | null
  barometer_hpa: number | null
  rain_rate_instant_mm_h: number | null
  rain_rate_15m_mm_h: number | null
  rain_mm: number | null
  solar_wm2: number | null
  uv_index: number | null
}

export interface StationResponse {
  station: Station
  current: Current | null
  today_rain_mm: number | null
}

export interface HistoryPoint {
  t: string
  out_temp_c: number | null
  app_temp_c: number | null
  dewpoint_c: number | null
  out_humidity_pct: number | null
  wind_speed_m_s: number | null
  wind_gust_m_s: number | null
  wind_dir_deg: number | null
  pressure_hpa: number | null
  barometer_hpa: number | null
  rain_rate_mm_h: number | null
  rain_mm: number | null
  solar_wm2: number | null
  uv_index: number | null
}

export interface SummaryMetric {
  high: number | null
  low: number | null
  average: number | null
  total: number | null
}

export interface HistorySummary {
  temperature: SummaryMetric
  dew_point: SummaryMetric
  humidity: SummaryMetric
  precipitation_rate: SummaryMetric
  precipitation_accumulation: SummaryMetric
  wind_speed: SummaryMetric
  wind_gust: SummaryMetric
  wind_direction: SummaryMetric
  pressure: SummaryMetric
}

export interface Coverage {
  days_in_range: number
  days_with_data: number
  includes_today: boolean
}

export interface History {
  station_id: string
  timezone: string
  period: Period
  start_date: string
  end_date: string
  start: string
  end: string
  bucket_seconds: number
  sample_count: number
  coverage: Coverage
  points: HistoryPoint[]
  summary: HistorySummary
}

export interface DisplayValues {
  time: string | null
  out_temp_c: number | null
  app_temp_c: number | null
  dewpoint_c: number | null
  out_humidity_pct: number | null
  wind_speed_m_s: number | null
  wind_gust_m_s: number | null
  wind_dir_deg: number | null
  pressure_hpa: number | null
  barometer_hpa: number | null
  rain_rate_mm_h: number | null
  rain_total_mm: number | null
  solar_wm2: number | null
  uv_index: number | null
}

export interface SelectedObservation {
  index: number
  point: HistoryPoint
}

export type ViewMode = 'graph' | 'table'
