import { useEffect, useState } from 'react'

import { formatLocalDateTime } from './format'
import { HistorySection } from './components/HistorySection'
import { StationHeader } from './components/StationHeader'
import { TimelineSlider } from './components/TimelineSlider'
import { WidgetGrid } from './components/WidgetGrid'
import type { SelectedObservation } from './types'
import { useDashboard } from './useDashboard'
import { displayFromCurrent, displayFromPoint } from './values'

function LoadingPage() {
  return (
    <main className="page">
      <div className="panel skeleton-header">
        <div className="skeleton skeleton-line short" />
        <div className="skeleton skeleton-title" />
        <div className="skeleton skeleton-line" />
      </div>
      <div className="widget-grid">
        {Array.from({ length: 7 }, (_, index) => (
          <div className="panel skeleton skeleton-widget" key={index} />
        ))}
      </div>
      <div className="panel skeleton skeleton-chart" />
    </main>
  )
}

function ErrorPage({
  message,
  onRetry,
}: {
  message: string
  onRetry: () => void
}) {
  return (
    <main className="page centered-page">
      <section className="panel error-panel">
        <p className="eyebrow">Akashavani weather</p>
        <h1>Dashboard unavailable</h1>
        <p>{message}</p>
        <button type="button" className="button primary" onClick={onRetry}>
          Try again
        </button>
      </section>
    </main>
  )
}

export default function App() {
  const dashboard = useDashboard()
  const [selected, setSelected] = useState<SelectedObservation | null>(null)
  const { station, selection, todayHistory, history, loading, refreshing, error } = dashboard

  useEffect(() => {
    if (station) {
      document.title = `${station.station.name || station.station.id} · Akashavani`
    }
  }, [station])

  const todayPoints = todayHistory?.points ?? []
  const values = station
    ? selected
      ? displayFromPoint(selected.point, todayPoints, selected.index)
      : displayFromCurrent(station.current, station.today_rain_mm)
    : null

  if (!dashboard.slug) {
    return (
      <ErrorPage
        message="No station slug in the URL. Open /dashboard/<slug>/ for a station."
        onRetry={() => window.location.reload()}
      />
    )
  }
  if (loading && !station) return <LoadingPage />
  if (!station || !values || !selection) {
    return (
      <ErrorPage
        message={error ?? 'The station could not be loaded.'}
        onRetry={dashboard.refresh}
      />
    )
  }

  const asOf = selected?.point.t ?? station.current?.date_time ?? null
  const timelineReason =
    todayPoints.length === 0
      ? 'No observations today. The timeline will enable when data arrives.'
      : undefined

  return (
    <div className="page">
      <StationHeader
        station={station.station}
        asOf={asOf}
        selected={selected !== null}
        refreshing={refreshing}
        onRefresh={dashboard.refresh}
      />

      {error ? (
        <div className="banner error-banner" role="status">
          <span>{error}</span>
          <button type="button" className="text-button" onClick={dashboard.refresh}>
            Retry
          </button>
        </div>
      ) : null}

      <WidgetGrid values={values} />

      <TimelineSlider
        points={todayPoints}
        timezone={station.station.timezone}
        selected={selected}
        onSelect={setSelected}
        disabledReason={timelineReason}
      />

      <HistorySection
        slug={dashboard.slug}
        selection={selection}
        siteDate={station.station.site_date}
        firstDate={dashboard.firstDate}
        history={history}
        error={error}
        onChange={dashboard.setSelection}
        onRetry={dashboard.refresh}
      />

      <footer className="page-footer">
        <span>
          Data from Akashavani · times shown in {station.station.timezone} · metric units
        </span>
        {station.station.latest_archive_at ? (
          <span>
            Latest archive bucket:{' '}
            {formatLocalDateTime(station.station.latest_archive_at, station.station.timezone)}
          </span>
        ) : null}
      </footer>
    </div>
  )
}
