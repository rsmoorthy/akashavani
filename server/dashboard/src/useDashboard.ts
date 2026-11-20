import { useCallback, useEffect, useMemo, useState } from 'react'

import { ApiError, fetchHistory, fetchStation } from './api'
import { dateToISO, zonedParts } from './format'
import {
  defaultSelection,
  historyQuery,
  isTodaySelection,
  rangeDates,
  readSelection,
  writeSelection,
} from './range'
import type { RangeSelection } from './range'
import type { History, StationResponse } from './types'

export function slugFromPath(pathname = window.location.pathname): string | null {
  const parts = pathname.split('/').filter(Boolean)
  if (parts[0] !== 'dashboard' || !parts[1]) return null
  try {
    return decodeURIComponent(parts[1])
  } catch {
    return null
  }
}

function firstDateOf(response: StationResponse): string | null {
  if (!response.station.first_archive_at) return null
  const parts = zonedParts(response.station.first_archive_at, response.station.timezone)
  return dateToISO(parts.year, parts.month, parts.day)
}

function errorMessage(error: unknown): string {
  if (error instanceof ApiError && error.status === 404) {
    return 'Station not found. Check the dashboard URL.'
  }
  if (error instanceof Error) return error.message
  return 'Could not load the station.'
}

export interface DashboardHook {
  slug: string | null
  station: StationResponse | null
  history: History | null
  todayHistory: History | null
  selection: RangeSelection | null
  firstDate: string | null
  loading: boolean
  refreshing: boolean
  error: string | null
  setSelection: (selection: RangeSelection) => void
  refresh: () => void
}

export function useDashboard(): DashboardHook {
  const slug = useMemo(() => slugFromPath(), [])
  const [station, setStation] = useState<StationResponse | null>(null)
  const [selection, setSelectionState] = useState<RangeSelection | null>(null)
  const [history, setHistory] = useState<History | null>(null)
  const [todayHistory, setTodayHistory] = useState<History | null>(null)
  const [loading, setLoading] = useState(true)
  const [refreshing, setRefreshing] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [stationTick, setStationTick] = useState(0)
  const [dataTick, setDataTick] = useState(0)

  const siteDate = station?.station.site_date ?? null

  useEffect(() => {
    if (!slug) {
      setLoading(false)
      return
    }
    const controller = new AbortController()
    fetchStation(slug, controller.signal)
      .then((response) => {
        setStation(response)
        setSelectionState((current) => {
          if (current) return current
          const initial = readSelection(window.location.search, response.station.site_date)
          return initial.period ? initial : defaultSelection(response.station.site_date)
        })
        setError(null)
      })
      .catch((caught: unknown) => {
        if (caught instanceof DOMException && caught.name === 'AbortError') return
        setError(errorMessage(caught))
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false)
      })
    return () => controller.abort()
  }, [slug, stationTick])

  useEffect(() => {
    if (!siteDate) return
    const onPopState = () => {
      setSelectionState(readSelection(window.location.search, siteDate))
    }
    window.addEventListener('popstate', onPopState)
    return () => window.removeEventListener('popstate', onPopState)
  }, [siteDate])

  useEffect(() => {
    if (!station || !selection || !siteDate) return
    const search = writeSelection(selection, siteDate)
    if (window.location.search !== search) {
      window.history.replaceState(null, '', `${window.location.pathname}${search}`)
    }
  }, [station, selection, siteDate])

  useEffect(() => {
    if (!slug || !siteDate || !selection) return
    const controller = new AbortController()
    setRefreshing(true)
    fetchHistory(slug, historyQuery(selection, siteDate), controller.signal)
      .then((response) => {
        setHistory(response)
        setError(null)
      })
      .catch((caught: unknown) => {
        if (caught instanceof DOMException && caught.name === 'AbortError') return
        setError(errorMessage(caught))
      })
      .finally(() => {
        if (!controller.signal.aborted) setRefreshing(false)
      })
    return () => controller.abort()
  }, [slug, siteDate, selection, dataTick])

  useEffect(() => {
    if (!slug || !siteDate || !selection) return
    if (isTodaySelection(selection, siteDate)) {
      setTodayHistory(null)
      return
    }
    const controller = new AbortController()
    fetchHistory(slug, { period: 'day', date: siteDate }, controller.signal)
      .then((response) => {
        setTodayHistory(response)
      })
      .catch((caught: unknown) => {
        if (caught instanceof DOMException && caught.name === 'AbortError') return
        setError(errorMessage(caught))
      })
    return () => controller.abort()
  }, [slug, siteDate, selection, dataTick])

  useEffect(() => {
    const stationTimer = window.setInterval(() => {
      if (!document.hidden) setStationTick((value) => value + 1)
    }, 60_000)
    const dataTimer = window.setInterval(() => {
      if (!document.hidden) setDataTick((value) => value + 1)
    }, 300_000)
    const onVisibility = () => {
      if (!document.hidden) {
        setStationTick((value) => value + 1)
        setDataTick((value) => value + 1)
      }
    }
    document.addEventListener('visibilitychange', onVisibility)
    return () => {
      window.clearInterval(stationTimer)
      window.clearInterval(dataTimer)
      document.removeEventListener('visibilitychange', onVisibility)
    }
  }, [])

  const matchingHistory = useMemo(() => {
    if (!history || !selection || !siteDate) return null
    const expected = rangeDates(selection, siteDate)
    if (history.period !== selection.period) return null
    if (history.start_date !== expected.start || history.end_date !== expected.end) {
      return null
    }
    return history
  }, [history, selection, siteDate])

  const sliderHistory = useMemo(() => {
    if (!selection || !siteDate) return null
    return isTodaySelection(selection, siteDate) ? matchingHistory : todayHistory
  }, [selection, siteDate, matchingHistory, todayHistory])

  const setSelection = useCallback((next: RangeSelection) => {
    setSelectionState(next)
  }, [])

  const refresh = useCallback(() => {
    setStationTick((value) => value + 1)
    setDataTick((value) => value + 1)
  }, [])

  return {
    slug,
    station,
    history: matchingHistory,
    todayHistory: sliderHistory,
    selection,
    firstDate: station ? firstDateOf(station) : null,
    loading,
    refreshing,
    error,
    setSelection,
    refresh,
  }
}
