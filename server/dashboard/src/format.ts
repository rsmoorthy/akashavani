import type { Period } from './types'

const MONTHS = [
  'Jan',
  'Feb',
  'Mar',
  'Apr',
  'May',
  'Jun',
  'Jul',
  'Aug',
  'Sep',
  'Oct',
  'Nov',
  'Dec',
]
const MONTHS_LONG = [
  'January',
  'February',
  'March',
  'April',
  'May',
  'June',
  'July',
  'August',
  'September',
  'October',
  'November',
  'December',
]
const COMPASS = [
  'N',
  'NNE',
  'NE',
  'ENE',
  'E',
  'ESE',
  'SE',
  'SSE',
  'S',
  'SSW',
  'SW',
  'WSW',
  'W',
  'WNW',
  'NW',
  'NNW',
]

const formatterCache = new Map<string, Intl.DateTimeFormat>()

function formatter(timeZone: string, options: Intl.DateTimeFormatOptions): Intl.DateTimeFormat {
  const key = `${timeZone}:${JSON.stringify(options)}`
  let result = formatterCache.get(key)
  if (!result) {
    result = new Intl.DateTimeFormat('en-US', { timeZone, ...options })
    formatterCache.set(key, result)
  }
  return result
}

export interface ZonedParts {
  year: number
  month: number
  day: number
  hour: number
  minute: number
  second: number
}

export function zonedParts(iso: string, timeZone: string): ZonedParts {
  const date = new Date(iso)
  if (Number.isNaN(date.getTime())) {
    return { year: 0, month: 1, day: 1, hour: 0, minute: 0, second: 0 }
  }
  const values: Record<string, number> = {}
  for (const part of formatter(timeZone, {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hourCycle: 'h23',
  }).formatToParts(date)) {
    if (part.type !== 'literal') values[part.type] = Number(part.value)
  }
  return {
    year: values.year,
    month: values.month,
    day: values.day,
    hour: values.hour,
    minute: values.minute,
    second: values.second,
  }
}

export function formatLocalTime(iso: string | null, timeZone: string): string {
  if (!iso) return '—'
  const date = new Date(iso)
  if (Number.isNaN(date.getTime())) return '—'
  return formatter(timeZone, {
    hour: 'numeric',
    minute: '2-digit',
    hour12: true,
  }).format(date)
}

export function formatLocalDateTime(iso: string | null, timeZone: string): string {
  if (!iso) return '—'
  const date = new Date(iso)
  if (Number.isNaN(date.getTime())) return '—'
  return formatter(timeZone, {
    month: 'short',
    day: 'numeric',
    year: 'numeric',
    hour: 'numeric',
    minute: '2-digit',
    hour12: true,
  }).format(date)
}

function weekdayShort(iso: string, timeZone: string): string {
  const date = new Date(iso)
  if (Number.isNaN(date.getTime())) return ''
  return formatter(timeZone, { weekday: 'short' }).format(date)
}

export function formatLocalDate(iso: string | null, timeZone: string): string {
  if (!iso) return '—'
  const date = new Date(iso)
  if (Number.isNaN(date.getTime())) return '—'
  return formatter(timeZone, {
    weekday: 'short',
    month: 'short',
    day: 'numeric',
    year: 'numeric',
  }).format(date)
}

export function formatRelative(iso: string | null, now = Date.now()): string {
  if (!iso) return '—'
  const date = new Date(iso)
  if (Number.isNaN(date.getTime())) return '—'
  const seconds = (now - date.getTime()) / 1000
  const future = seconds < 0
  const absolute = Math.abs(seconds)
  if (absolute < 45) return future ? 'in a moment' : 'just now'
  if (absolute < 3600) {
    const minutes = Math.max(1, Math.round(absolute / 60))
    return future ? `in ${minutes} min` : `${minutes} min ago`
  }
  if (absolute < 86400) {
    const hours = Math.round(absolute / 3600)
    return future ? `in ${hours} h` : `${hours} h ago`
  }
  const days = Math.round(absolute / 86400)
  return future ? `in ${days} d` : `${days} d ago`
}

export function formatNumber(value: number | null | undefined, digits = 1): string {
  if (value === null || value === undefined || Number.isNaN(value)) return '—'
  return value.toFixed(digits)
}

export function withUnit(
  value: number | null | undefined,
  unit: string,
  digits = 1,
): string {
  const number = formatNumber(value, digits)
  return number === '—' ? number : `${number} ${unit}`
}

export function formatTemperature(value: number | null | undefined, digits = 1): string {
  return withUnit(value, '°C', digits)
}

export function formatHumidity(value: number | null | undefined): string {
  const number = formatNumber(value, 0)
  return number === '—' ? number : `${number}%`
}

export function formatWind(value: number | null | undefined, digits = 1): string {
  return withUnit(value, 'm/s', digits)
}

export function formatRain(value: number | null | undefined, digits = 2): string {
  return withUnit(value, 'mm', digits)
}

export function formatRainRate(value: number | null | undefined, digits = 2): string {
  return withUnit(value, 'mm/h', digits)
}

export function formatPressure(value: number | null | undefined): string {
  return withUnit(value, 'hPa', 1)
}

export function formatSolar(value: number | null | undefined): string {
  return withUnit(value, 'W/m²', 0)
}

export function formatUv(value: number | null | undefined): string {
  return formatNumber(value, 1)
}

export function degreesToCompass(degrees: number | null | undefined): string {
  if (degrees === null || degrees === undefined || Number.isNaN(degrees)) return '—'
  const normalised = ((degrees % 360) + 360) % 360
  return COMPASS[Math.round(normalised / 22.5) % 16]
}

export function uvCategory(value: number | null | undefined): string {
  if (value === null || value === undefined || Number.isNaN(value)) return 'No data'
  if (value < 3) return 'Low'
  if (value < 6) return 'Moderate'
  if (value < 8) return 'High'
  if (value < 11) return 'Very high'
  return 'Extreme'
}

export interface DateParts {
  year: number
  month: number
  day: number
}

export function parseISODate(value: string): DateParts | null {
  const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(value)
  if (!match) return null
  const year = Number(match[1])
  const month = Number(match[2])
  const day = Number(match[3])
  if (month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month)) return null
  return { year, month, day }
}

export function dateToISO(year: number, month: number, day: number): string {
  return `${String(year).padStart(4, '0')}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}`
}

export function dateFromISO(value: string): Date {
  const parts = parseISODate(value)
  if (!parts) return new Date(Date.UTC(2000, 0, 1))
  return new Date(Date.UTC(parts.year, parts.month - 1, parts.day))
}

export function daysInMonth(year: number, month: number): number {
  return new Date(Date.UTC(year, month, 0)).getUTCDate()
}

export function addDaysISO(value: string, days: number): string {
  const date = dateFromISO(value)
  date.setUTCDate(date.getUTCDate() + days)
  return dateToISO(date.getUTCFullYear(), date.getUTCMonth() + 1, date.getUTCDate())
}

export function addMonthsISO(value: string, months: number): string {
  const parts = parseISODate(value)
  if (!parts) return value
  const total = parts.year * 12 + (parts.month - 1) + months
  const year = Math.floor(total / 12)
  const month = (total % 12) + 1
  const day = Math.min(parts.day, daysInMonth(year, month))
  return dateToISO(year, month, day)
}

export function compareISO(left: string, right: string): number {
  return left < right ? -1 : left > right ? 1 : 0
}

export function todayInZone(timeZone: string, now = new Date()): string {
  const parts = zonedParts(now.toISOString(), timeZone)
  return dateToISO(parts.year, parts.month, parts.day)
}

export function startOfMonthISO(value: string): string {
  const parts = parseISODate(value)
  if (!parts) return value
  return dateToISO(parts.year, parts.month, 1)
}

export function endOfMonthISO(value: string): string {
  const parts = parseISODate(value)
  if (!parts) return value
  return dateToISO(parts.year, parts.month, daysInMonth(parts.year, parts.month))
}

export function weekStartISO(value: string): string {
  return addDaysISO(value, -6)
}

function utcDateLabel(value: string, options: Intl.DateTimeFormatOptions): string {
  return new Intl.DateTimeFormat('en-US', { timeZone: 'UTC', ...options }).format(
    dateFromISO(value),
  )
}

export function dateRangeLabel(period: Period, startDate: string, endDate: string): string {
  if (period === 'day') {
    return utcDateLabel(startDate, {
      weekday: 'short',
      month: 'short',
      day: 'numeric',
      year: 'numeric',
    })
  }
  if (period === 'month') {
    const parts = parseISODate(startDate)
    if (!parts) return startDate
    return `${MONTHS_LONG[parts.month - 1]} ${parts.year}`
  }
  const start = parseISODate(startDate)
  const end = parseISODate(endDate)
  if (!start || !end) return `${startDate} – ${endDate}`
  if (start.year === end.year && start.month === end.month) {
    return `${MONTHS[start.month - 1]} ${start.day} – ${end.day}, ${end.year}`
  }
  if (start.year === end.year) {
    return `${MONTHS[start.month - 1]} ${start.day} – ${MONTHS[end.month - 1]} ${end.day}, ${end.year}`
  }
  return `${MONTHS[start.month - 1]} ${start.day}, ${start.year} – ${MONTHS[end.month - 1]} ${end.day}, ${end.year}`
}

export function axisLabel(
  iso: string,
  timeZone: string,
  bucketSeconds: number,
): string {
  const parts = zonedParts(iso, timeZone)
  const hour12 = parts.hour % 12 || 12
  const minute = String(parts.minute).padStart(2, '0')
  const ampm = parts.hour < 12 ? 'AM' : 'PM'
  if (bucketSeconds <= 300) {
    return `${hour12}:${minute} ${ampm}`
  }
  if (bucketSeconds < 86400) {
    return `${weekdayShort(iso, timeZone)} ${hour12} ${ampm}`
  }
  return `${MONTHS[parts.month - 1]} ${parts.day}`
}

export function tooltipTitle(
  iso: string,
  timeZone: string,
  bucketSeconds: number,
): string {
  const parts = zonedParts(iso, timeZone)
  const hour12 = parts.hour % 12 || 12
  const minute = String(parts.minute).padStart(2, '0')
  const ampm = parts.hour < 12 ? 'AM' : 'PM'
  const date = `${weekdayShort(iso, timeZone)}, ${MONTHS[parts.month - 1]} ${parts.day}`
  if (bucketSeconds <= 300) return `${date}, ${hour12}:${minute} ${ampm}`
  if (bucketSeconds < 86400) return `${date}, ${hour12} ${ampm}`
  return `${date}, ${parts.year}`
}

export function tableTime(iso: string, timeZone: string, bucketSeconds: number): string {
  const parts = zonedParts(iso, timeZone)
  const hour12 = parts.hour % 12 || 12
  const minute = String(parts.minute).padStart(2, '0')
  const ampm = parts.hour < 12 ? 'AM' : 'PM'
  if (bucketSeconds <= 300) return `${hour12}:${minute} ${ampm}`
  if (bucketSeconds < 86400) {
    return `${MONTHS[parts.month - 1]} ${parts.day}, ${hour12}:${minute} ${ampm}`
  }
  return `${MONTHS[parts.month - 1]} ${parts.day}`
}
