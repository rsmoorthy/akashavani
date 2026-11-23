import type { History, Period, StationListItem, StationResponse } from './types'

const API_ROOT = '/api/v1/public/stations'

export class ApiError extends Error {
  readonly status: number

  constructor(message: string, status: number) {
    super(message)
    this.name = 'ApiError'
    this.status = status
  }
}

async function getJson<T>(url: string, signal?: AbortSignal): Promise<T> {
  const response = await fetch(url, {
    headers: { Accept: 'application/json' },
    signal,
  })
  if (!response.ok) {
    let message = `${response.status} ${response.statusText}`
    try {
      const body = (await response.json()) as { detail?: unknown }
      if (typeof body.detail === 'string') message = body.detail
    } catch {
      // The response was not JSON; keep the HTTP status text.
    }
    throw new ApiError(message, response.status)
  }
  return (await response.json()) as T
}

function stationPath(slug: string): string {
  return `${API_ROOT}/${encodeURIComponent(slug)}`
}

export function fetchStation(slug: string, signal?: AbortSignal): Promise<StationResponse> {
  return getJson<StationResponse>(stationPath(slug), signal)
}

export function fetchStations(signal?: AbortSignal): Promise<StationListItem[]> {
  return getJson<StationListItem[]>(API_ROOT, signal)
}

export interface HistoryQuery {
  period: Period
  date?: string
  startDate?: string
  endDate?: string
}

function historyParams(query: HistoryQuery): URLSearchParams {
  const params = new URLSearchParams({ period: query.period })
  if (query.period === 'custom') {
    if (query.startDate) params.set('start_date', query.startDate)
    if (query.endDate) params.set('end_date', query.endDate)
  } else if (query.date) {
    params.set('date', query.date)
  }
  return params
}

export function fetchHistory(
  slug: string,
  query: HistoryQuery,
  signal?: AbortSignal,
): Promise<History> {
  const params = historyParams(query)
  return getJson<History>(`${stationPath(slug)}/history?${params.toString()}`, signal)
}

export function exportUrl(slug: string, query: HistoryQuery): string {
  const params = historyParams(query)
  return `${stationPath(slug)}/export.csv?${params.toString()}`
}
