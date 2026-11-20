import type { HistoryQuery } from './api'
import {
  addDaysISO,
  addMonthsISO,
  compareISO,
  dateRangeLabel,
  dateToISO,
  daysInMonth,
  endOfMonthISO,
  parseISODate,
  startOfMonthISO,
} from './format'
import type { Period } from './types'

export interface RangeSelection {
  period: Period
  date: string
  start?: string
  end?: string
}

export interface DateWindow {
  start: string
  end: string
}

const PERIODS: Period[] = ['day', 'week', 'month', 'custom']
export const MAX_CUSTOM_DAYS = 31

export function isPeriod(value: string | null): value is Period {
  return value !== null && PERIODS.includes(value as Period)
}

export function defaultSelection(siteDate: string): RangeSelection {
  return { period: 'day', date: siteDate }
}

export function rangeDates(selection: RangeSelection, siteDate: string): DateWindow {
  const date = selection.date || siteDate
  switch (selection.period) {
    case 'day':
      return { start: date, end: date }
    case 'week':
      return { start: addDaysISO(date, -6), end: date }
    case 'month':
      return { start: startOfMonthISO(date), end: endOfMonthISO(date) }
    case 'custom':
      return {
        start: selection.start || addDaysISO(siteDate, -6),
        end: selection.end || siteDate,
      }
  }
}

export function historyQuery(selection: RangeSelection, siteDate: string): HistoryQuery {
  if (selection.period === 'custom') {
    const window = rangeDates(selection, siteDate)
    return { period: 'custom', startDate: window.start, endDate: window.end }
  }
  return { period: selection.period, date: selection.date || siteDate }
}

export function selectionLabel(selection: RangeSelection, siteDate: string): string {
  const window = rangeDates(selection, siteDate)
  return dateRangeLabel(selection.period, window.start, window.end)
}

export function isTodaySelection(selection: RangeSelection, siteDate: string): boolean {
  return selection.period === 'day' && selection.date === siteDate
}

export function canGoForward(selection: RangeSelection, siteDate: string): boolean {
  return compareISO(rangeDates(selection, siteDate).end, siteDate) < 0
}

export function canGoBack(
  selection: RangeSelection,
  firstDate: string | null,
  siteDate: string,
): boolean {
  if (!firstDate) return true
  const window = rangeDates(selection, siteDate)
  return compareISO(window.start, firstDate) > 0
}

export function shiftSelection(
  selection: RangeSelection,
  direction: -1 | 1,
  siteDate: string,
): RangeSelection {
  const date = selection.date || siteDate
  if (selection.period === 'custom') {
    const window = rangeDates(selection, siteDate)
    const span = Math.max(1, daysBetweenISO(window.start, window.end) + 1)
    return {
      ...selection,
      start: addDaysISO(window.start, direction * span),
      end: addDaysISO(window.end, direction * span),
    }
  }
  if (selection.period === 'day') {
    return { ...selection, date: addDaysISO(date, direction) }
  }
  if (selection.period === 'week') {
    return { ...selection, date: addDaysISO(date, direction * 7) }
  }
  const shifted = addMonthsISO(date, direction)
  return { ...selection, date: compareISO(shifted, siteDate) > 0 ? siteDate : shifted }
}

export function readSelection(search: string, siteDate: string): RangeSelection {
  const params = new URLSearchParams(search)
  const rawPeriod = params.get('period')
  const period: Period = isPeriod(rawPeriod) ? rawPeriod : 'day'

  if (period === 'custom') {
    const start = params.get('start') || ''
    const end = params.get('end') || ''
    const valid = parseISODate(start) && parseISODate(end)
    if (valid && compareISO(start, end) <= 0 && compareISO(end, siteDate) <= 0) {
      const span = daysBetweenISO(start, end) + 1
      if (span <= MAX_CUSTOM_DAYS) {
        return { period, date: end, start, end }
      }
    }
    return {
      period: 'custom',
      date: siteDate,
      start: addDaysISO(siteDate, -6),
      end: siteDate,
    }
  }

  const requested = params.get('date') || siteDate
  const date = parseISODate(requested) && compareISO(requested, siteDate) <= 0 ? requested : siteDate
  return { period, date }
}

export function writeSelection(selection: RangeSelection, siteDate: string): string {
  const params = new URLSearchParams({ period: selection.period })
  if (selection.period === 'custom') {
    const window = rangeDates(selection, siteDate)
    params.set('start', window.start)
    params.set('end', window.end)
  } else {
    params.set('date', selection.date || siteDate)
  }
  return `?${params.toString()}`
}

export function daysBetweenISO(start: string, end: string): number {
  const first = parseISODate(start)
  const last = parseISODate(end)
  if (!first || !last) return 0
  const startMs = Date.UTC(first.year, first.month - 1, first.day)
  const endMs = Date.UTC(last.year, last.month - 1, last.day)
  return Math.round((endMs - startMs) / 86400000)
}

export function validateCustomRange(
  start: string,
  end: string,
  siteDate: string,
): string | null {
  if (!parseISODate(start) || !parseISODate(end)) return 'Choose a valid start and end date.'
  if (compareISO(start, end) > 0) return 'The end date must be on or after the start date.'
  if (compareISO(end, siteDate) > 0) return 'The end date cannot be in the future.'
  const span = daysBetweenISO(start, end) + 1
  if (span > MAX_CUSTOM_DAYS) return `Select at most ${MAX_CUSTOM_DAYS} days.`
  return null
}

export function customSelection(start: string, end: string, siteDate: string): RangeSelection {
  return { period: 'custom', date: end || siteDate, start, end }
}

export function yearOptions(firstDate: string | null, siteDate: string): number[] {
  const site = parseISODate(siteDate)
  const first = firstDate ? parseISODate(firstDate) : null
  const endYear = site ? site.year : new Date().getUTCFullYear()
  const startYear = first ? first.year : endYear - 10
  const years: number[] = []
  for (let year = endYear; year >= Math.min(startYear, endYear); year -= 1) {
    years.push(year)
  }
  return years
}

export function isoFromParts(year: number, month: number, day: number): string {
  return dateToISO(year, month, Math.min(day, daysInMonth(year, month)))
}
