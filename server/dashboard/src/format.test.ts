import { describe, expect, it } from 'vitest'

import {
  addDaysISO,
  addMonthsISO,
  dateRangeLabel,
  degreesToCompass,
  formatNumber,
  formatRelative,
  formatTemperature,
  formatWind,
  isStale,
  tooltipTitle,
  uvCategory,
  zonedParts,
} from './format'

describe('format', () => {
  it('formats numbers and units with a dash for missing values', () => {
    expect(formatNumber(27.04, 1)).toBe('27.0')
    expect(formatNumber(null)).toBe('—')
    expect(formatTemperature(27)).toBe('27.0 °C')
    expect(formatTemperature(null)).toBe('—')
  })

  it('shows wind in km/h', () => {
    expect(formatWind(1.2)).toBe('4.3 km/h')
    expect(formatWind(null)).toBe('—')
  })

  it('marks data stale after 30 minutes without a device report', () => {
    const now = Date.parse('2026-11-20T06:00:00Z')
    expect(isStale('2026-11-20T05:31:00Z', now)).toBe(false)
    expect(isStale('2026-11-20T05:30:00Z', now)).toBe(false)
    expect(isStale('2026-11-20T05:29:59Z', now)).toBe(true)
    expect(isStale(null, now)).toBe(true)
  })

  it('resolves station-local time parts', () => {
    expect(zonedParts('2026-11-19T18:35:00Z', 'Asia/Calcutta')).toEqual({
      year: 2026,
      month: 11,
      day: 20,
      hour: 0,
      minute: 5,
      second: 0,
    })
  })

  it('uses the station zone for weekday and tooltip titles', () => {
    expect(tooltipTitle('2026-11-19T18:35:00Z', 'Asia/Calcutta', 300)).toBe(
      'Fri, Nov 20, 12:05 AM',
    )
  })

  it('renders relative times', () => {
    expect(formatRelative('2026-11-20T05:20:00Z', Date.parse('2026-11-20T05:25:00Z'))).toBe(
      '5 min ago',
    )
    expect(formatRelative('2026-11-20T05:20:00Z', Date.parse('2026-11-20T05:20:20Z'))).toBe(
      'just now',
    )
  })

  it('converts degrees to compass points', () => {
    expect(degreesToCompass(0)).toBe('N')
    expect(degreesToCompass(22.5)).toBe('NNE')
    expect(degreesToCompass(210)).toBe('SSW')
    expect(degreesToCompass(null)).toBe('—')
  })

  it('classifies UV values', () => {
    expect(uvCategory(1)).toBe('Low')
    expect(uvCategory(4)).toBe('Moderate')
    expect(uvCategory(7)).toBe('High')
    expect(uvCategory(9)).toBe('Very high')
    expect(uvCategory(12)).toBe('Extreme')
  })

  it('adds days and clamps month arithmetic', () => {
    expect(addDaysISO('2026-11-30', 1)).toBe('2026-12-01')
    expect(addMonthsISO('2026-01-31', 1)).toBe('2026-02-28')
    expect(addMonthsISO('2026-12-15', 1)).toBe('2027-01-15')
  })

  it('labels day, week, month and custom ranges', () => {
    expect(dateRangeLabel('day', '2026-11-20', '2026-11-20')).toBe(
      'Fri, Nov 20, 2026',
    )
    expect(dateRangeLabel('week', '2026-11-14', '2026-11-20')).toBe(
      'Nov 14 – 20, 2026',
    )
    expect(dateRangeLabel('custom', '2026-11-28', '2026-12-04')).toBe(
      'Nov 28 – Dec 4, 2026',
    )
    expect(dateRangeLabel('month', '2026-11-01', '2026-11-30')).toBe('November 2026')
  })
})
