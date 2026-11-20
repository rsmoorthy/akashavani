import { zonedParts } from './format'
import type { HistoryPoint } from './types'

const DATA_FIELDS: (keyof HistoryPoint)[] = [
  'out_temp_c',
  'app_temp_c',
  'dewpoint_c',
  'out_humidity_pct',
  'wind_speed_m_s',
  'wind_gust_m_s',
  'wind_dir_deg',
  'pressure_hpa',
  'barometer_hpa',
  'rain_rate_mm_h',
  'rain_mm',
  'solar_wm2',
  'uv_index',
]

export function hasData(point: HistoryPoint): boolean {
  return DATA_FIELDS.some((field) => point[field] !== null && point[field] !== undefined)
}

export function minuteOfDay(iso: string, timeZone: string): number {
  const parts = zonedParts(iso, timeZone)
  return parts.hour * 60 + parts.minute
}

export function latestDataIndex(points: HistoryPoint[]): number {
  for (let index = points.length - 1; index >= 0; index -= 1) {
    if (hasData(points[index])) return index
  }
  return -1
}

export function nearestDataIndex(
  points: HistoryPoint[],
  minute: number,
  timeZone: string,
): number {
  let bestIndex = -1
  let bestDistance = Number.POSITIVE_INFINITY
  for (let index = 0; index < points.length; index += 1) {
    if (!hasData(points[index])) continue
    const distance = Math.abs(minuteOfDay(points[index].t, timeZone) - minute)
    if (distance < bestDistance) {
      bestDistance = distance
      bestIndex = index
    }
  }
  return bestIndex
}

export function cumulativeRain(points: HistoryPoint[], index: number): number | null {
  let found = false
  let total = 0
  for (let cursor = 0; cursor <= Math.min(index, points.length - 1); cursor += 1) {
    const value = points[cursor].rain_mm
    if (value !== null) {
      found = true
      total += value
    }
  }
  return found ? total : null
}

export function formatMinute(minute: number): string {
  const hour = Math.floor(minute / 60) % 24
  const minutes = ((minute % 60) + 60) % 60
  const hour12 = hour % 12 || 12
  return `${hour12}:${String(minutes).padStart(2, '0')} ${hour < 12 ? 'AM' : 'PM'}`
}

export const TIMELINE_TICKS = [
  { minute: 0, label: '12a' },
  { minute: 180, label: '3a' },
  { minute: 360, label: '6a' },
  { minute: 540, label: '9a' },
  { minute: 720, label: '12p' },
  { minute: 900, label: '3p' },
  { minute: 1080, label: '6p' },
  { minute: 1260, label: '9p' },
  { minute: 1440, label: '12a' },
]
