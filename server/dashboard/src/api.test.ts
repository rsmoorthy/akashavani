import { afterEach, describe, expect, it, vi } from 'vitest'

import { exportUrl, fetchHistory, fetchStation, fetchStations } from './api'

const response = (body: unknown, status = 200) => ({
  ok: status >= 200 && status < 300,
  status,
  statusText: status === 404 ? 'Not Found' : 'OK',
  json: async () => body,
})

afterEach(() => {
  vi.unstubAllGlobals()
})

describe('public API client', () => {
  it('fetches a station by encoded slug', async () => {
    const fetchMock = vi.fn().mockResolvedValue(response({ station: { id: 'x' } }))
    vi.stubGlobal('fetch', fetchMock)
    await fetchStation('roof receiver')
    expect(fetchMock).toHaveBeenCalledWith(
      '/api/v1/public/stations/roof%20receiver',
      expect.objectContaining({ headers: { Accept: 'application/json' } }),
    )
  })

  it('fetches the station list', async () => {
    const fetchMock = vi.fn().mockResolvedValue(response([]))
    vi.stubGlobal('fetch', fetchMock)
    await fetchStations()
    expect(fetchMock).toHaveBeenCalledWith(
      '/api/v1/public/stations',
      expect.objectContaining({ headers: { Accept: 'application/json' } }),
    )
  })

  it('builds day, week and custom history queries', async () => {
    const fetchMock = vi.fn().mockResolvedValue(response({ points: [] }))
    vi.stubGlobal('fetch', fetchMock)
    await fetchHistory('roof-receiver', { period: 'week', date: '2026-11-18' })
    await fetchHistory('roof-receiver', {
      period: 'custom',
      startDate: '2026-11-01',
      endDate: '2026-11-20',
    })
    expect(fetchMock.mock.calls[0][0]).toBe(
      '/api/v1/public/stations/roof-receiver/history?period=week&date=2026-11-18',
    )
    expect(fetchMock.mock.calls[1][0]).toBe(
      '/api/v1/public/stations/roof-receiver/history?period=custom&start_date=2026-11-01&end_date=2026-11-20',
    )
  })

  it('builds an export URL', () => {
    expect(
      exportUrl('roof-receiver', {
        period: 'custom',
        startDate: '2026-11-01',
        endDate: '2026-11-20',
      }),
    ).toBe(
      '/api/v1/public/stations/roof-receiver/export.csv?period=custom&start_date=2026-11-01&end_date=2026-11-20',
    )
  })

  it('surfaces a readable API error', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn().mockResolvedValue(response({ detail: 'station not found' }, 404)),
    )
    await expect(fetchStation('missing')).rejects.toEqual(
      expect.objectContaining({ status: 404, message: 'station not found' }),
    )
  })
})
