import { describe, expect, it } from 'vitest'

import {
  canGoForward,
  customSelection,
  historyQuery,
  rangeDates,
  readSelection,
  shiftSelection,
  validateCustomRange,
  writeSelection,
  yearOptions,
} from './range'

describe('range selection', () => {
  const siteDate = '2026-11-20'

  it('defaults to the station-local day', () => {
    expect(readSelection('', siteDate)).toEqual({ period: 'day', date: siteDate })
  })

  it('reads and writes week and month selections', () => {
    expect(readSelection('?period=week&date=2026-11-18', siteDate)).toEqual({
      period: 'week',
      date: '2026-11-18',
    })
    expect(writeSelection({ period: 'week', date: '2026-11-18' }, siteDate)).toBe(
      '?period=week&date=2026-11-18',
    )
    expect(writeSelection(customSelection('2026-11-01', '2026-11-20', siteDate), siteDate)).toBe(
      '?period=custom&start=2026-11-01&end=2026-11-20',
    )
  })

  it('falls back when a custom range is invalid or too long', () => {
    expect(readSelection('?period=custom&start=2026-11-01&end=2026-11-20', siteDate)).toEqual({
      period: 'custom',
      date: '2026-11-20',
      start: '2026-11-01',
      end: '2026-11-20',
    })
    expect(
      readSelection('?period=custom&start=2026-10-01&end=2026-11-20', siteDate),
    ).toMatchObject({
      period: 'custom',
      start: '2026-11-14',
      end: '2026-11-20',
    })
    expect(
      readSelection('?period=custom&start=2026-11-25&end=2026-12-02', siteDate),
    ).toMatchObject({
      period: 'custom',
      start: '2026-11-14',
      end: '2026-11-20',
    })
  })

  it('computes date windows and history query parameters', () => {
    expect(rangeDates({ period: 'week', date: '2026-11-20' }, siteDate)).toEqual({
      start: '2026-11-14',
      end: '2026-11-20',
    })
    expect(historyQuery({ period: 'month', date: '2026-11-12' }, siteDate)).toEqual({
      period: 'month',
      date: '2026-11-12',
    })
    expect(
      historyQuery(customSelection('2026-11-01', '2026-11-10', siteDate), siteDate),
    ).toEqual({ period: 'custom', startDate: '2026-11-01', endDate: '2026-11-10' })
  })

  it('shifts ranges by their period', () => {
    expect(shiftSelection({ period: 'day', date: '2026-11-19' }, 1, siteDate)).toEqual({
      period: 'day',
      date: '2026-11-20',
    })
    expect(shiftSelection({ period: 'week', date: '2026-11-20' }, -1, siteDate)).toEqual({
      period: 'week',
      date: '2026-11-13',
    })
    expect(shiftSelection({ period: 'month', date: '2026-01-31' }, 1, siteDate)).toEqual({
      period: 'month',
      date: '2026-02-28',
    })
  })

  it('disables forward navigation at the station date', () => {
    expect(canGoForward({ period: 'day', date: '2026-11-20' }, siteDate)).toBe(false)
    expect(canGoForward({ period: 'day', date: '2026-11-19' }, siteDate)).toBe(true)
  })

  it('validates custom ranges', () => {
    expect(validateCustomRange('2026-11-01', '2026-11-20', siteDate)).toBeNull()
    expect(validateCustomRange('2026-11-20', '2026-11-01', siteDate)).toContain(
      'on or after',
    )
    expect(validateCustomRange('2026-11-01', '2026-12-01', siteDate)).toContain('future')
    expect(validateCustomRange('2026-10-01', '2026-11-20', siteDate)).toContain('31 days')
  })

  it('creates year options from the first archive date', () => {
    expect(yearOptions('2024-06-01', '2026-11-20')).toEqual([2026, 2025, 2024])
  })
})
