import { cumulativeRain } from './timeline'
import type { Current, DisplayValues, HistoryPoint } from './types'

export function displayFromCurrent(
  current: Current | null,
  todayRain: number | null,
): DisplayValues {
  return {
    time: current?.date_time ?? null,
    out_temp_c: current?.out_temp_c ?? null,
    app_temp_c: current?.app_temp_c ?? null,
    dewpoint_c: current?.dewpoint_c ?? null,
    out_humidity_pct: current?.out_humidity_pct ?? null,
    wind_speed_m_s: current?.wind_speed_m_s ?? null,
    wind_gust_m_s: current?.wind_gust_m_s ?? null,
    wind_dir_deg: current?.wind_dir_deg ?? null,
    pressure_hpa: current?.pressure_hpa ?? null,
    barometer_hpa: current?.barometer_hpa ?? null,
    rain_rate_mm_h:
      current?.rain_rate_instant_mm_h ?? current?.rain_rate_15m_mm_h ?? null,
    rain_total_mm: todayRain,
    solar_wm2: current?.solar_wm2 ?? null,
    uv_index: current?.uv_index ?? null,
  }
}

export function displayFromPoint(
  point: HistoryPoint | null,
  todayPoints: HistoryPoint[],
  index: number,
): DisplayValues {
  return {
    time: point?.t ?? null,
    out_temp_c: point?.out_temp_c ?? null,
    app_temp_c: point?.app_temp_c ?? null,
    dewpoint_c: point?.dewpoint_c ?? null,
    out_humidity_pct: point?.out_humidity_pct ?? null,
    wind_speed_m_s: point?.wind_speed_m_s ?? null,
    wind_gust_m_s: point?.wind_gust_m_s ?? null,
    wind_dir_deg: point?.wind_dir_deg ?? null,
    pressure_hpa: point?.pressure_hpa ?? null,
    barometer_hpa: point?.barometer_hpa ?? null,
    rain_rate_mm_h: point?.rain_rate_mm_h ?? null,
    rain_total_mm: point ? cumulativeRain(todayPoints, index) : null,
    solar_wm2: point?.solar_wm2 ?? null,
    uv_index: point?.uv_index ?? null,
  }
}
