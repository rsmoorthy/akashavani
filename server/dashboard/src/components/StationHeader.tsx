import {
  formatLocalDateTime,
  formatLocalTime,
  formatRelative,
} from '../format'
import type { Station as StationType, StationListItem } from '../types'

interface StationHeaderProps {
  station: StationType
  stations: StationListItem[]
  asOf: string | null
  selected: boolean
  refreshing: boolean
  onRefresh: () => void
}

function locationLabel(station: StationType): string {
  const altitude = station.altitude_m === null ? 'N/A' : `${station.altitude_m.toFixed(0)} m`
  const latitude =
    station.latitude === null
      ? 'N/A'
      : `${Math.abs(station.latitude).toFixed(3)}° ${station.latitude >= 0 ? 'N' : 'S'}`
  const longitude =
    station.longitude === null
      ? 'N/A'
      : `${Math.abs(station.longitude).toFixed(3)}° ${station.longitude >= 0 ? 'E' : 'W'}`
  return `Elev ${altitude}, ${latitude} ${longitude}`
}

export function StationHeader({
  station,
  stations,
  asOf,
  selected,
  refreshing,
  onRefresh,
}: StationHeaderProps) {
  const zone = station.timezone
  const name = station.name || station.id
  const lastSeen = station.last_seen_at
    ? `${formatRelative(station.last_seen_at)} · ${formatLocalDateTime(station.last_seen_at, zone)}`
    : 'N/A'
  const monitoringUrl =
    station.monitoring_url && /^https?:\/\//i.test(station.monitoring_url)
      ? station.monitoring_url
      : null
  const stationOptions = stations.some((item) => item.slug === station.slug)
    ? stations
    : [{ slug: station.slug, name: station.name }, ...stations]
  const showStationSelect = stationOptions.length > 1

  return (
    <header className="station-header panel">
      <div className="station-heading">
        <div>
          <p className="eyebrow">Akashavani weather station</p>
          <h1>{name}</h1>
          <p className="location-line">{locationLabel(station)}</p>
        </div>
        <div className="station-actions">
          <span className={`status-pill ${station.online ? 'is-online' : 'is-offline'}`}>
            <span className="status-dot" />
            {station.online ? 'Connected' : 'Offline'}
          </span>
          <button
            type="button"
            className={`refresh-button ${refreshing ? 'is-refreshing' : ''}`}
            onClick={onRefresh}
            aria-label="Refresh dashboard data"
            title="Refresh"
          >
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path d="M20 11a8 8 0 1 0-2.3 6.2" />
              <path d="M20 4v7h-7" />
            </svg>
          </button>
        </div>
      </div>
      <div className="station-meta">
        <span>
          <strong>Last seen:</strong> {lastSeen}
        </span>
        <span>
          <strong>{selected ? 'Selected reading:' : 'Data as of:'}</strong>{' '}
          {asOf ? `${formatLocalTime(asOf, zone)} (${formatRelative(asOf)})` : 'No observations yet'}
        </span>
        {monitoringUrl ? (
          <a
            className="monitor-link"
            href={monitoringUrl}
            target="_blank"
            rel="noopener noreferrer"
          >
            Monitor Device
          </a>
        ) : null}
        <span className="station-id">
          <strong>Station:</strong>{' '}
          {showStationSelect ? (
            <select
              className="station-select"
              value={station.slug}
              aria-label="Select station"
              onChange={(event) => {
                const next = event.target.value
                if (next !== station.slug) {
                  window.location.assign(`/dashboard/${encodeURIComponent(next)}/`)
                }
              }}
            >
              {stationOptions.map((item) => (
                <option key={item.slug} value={item.slug}>
                  {item.slug}
                </option>
              ))}
            </select>
          ) : (
            station.slug
          )}
        </span>
      </div>
    </header>
  )
}
