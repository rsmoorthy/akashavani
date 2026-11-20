import { degreesToCompass, formatNumber } from '../format'
import type { History, SummaryMetric } from '../types'

interface Row {
  key: string
  label: string
  metric: SummaryMetric
  digits: number
  direction?: boolean
}

function metricRows(history: History): Row[] {
  const summary = history.summary
  const seaLevel = history.points.some((point) => point.barometer_hpa !== null)
  return [
    { key: 'temperature', label: 'Temperature (°C)', metric: summary.temperature, digits: 1 },
    { key: 'dew_point', label: 'Dew point (°C)', metric: summary.dew_point, digits: 1 },
    { key: 'humidity', label: 'Humidity (%)', metric: summary.humidity, digits: 0 },
    {
      key: 'precipitation_rate',
      label: 'Precipitation rate (mm/h)',
      metric: summary.precipitation_rate,
      digits: 2,
    },
    {
      key: 'precipitation_accumulation',
      label: 'Precipitation accumulation (mm)',
      metric: summary.precipitation_accumulation,
      digits: 2,
    },
    { key: 'wind_speed', label: 'Wind speed (m/s)', metric: summary.wind_speed, digits: 1 },
    { key: 'wind_gust', label: 'Wind gust (m/s)', metric: summary.wind_gust, digits: 1 },
    {
      key: 'wind_direction',
      label: 'Wind direction',
      metric: summary.wind_direction,
      digits: 0,
      direction: true,
    },
    {
      key: 'pressure',
      label: `Pressure (hPa, ${seaLevel ? 'sea-level' : 'station'})`,
      metric: summary.pressure,
      digits: 1,
    },
  ]
}

function valueOrDash(value: number | null, digits: number): string {
  return value === null ? '—' : formatNumber(value, digits)
}

function directionValue(value: number | null): string {
  if (value === null) return '—'
  return `${Math.round(value)}° ${degreesToCompass(value)}`
}

export function SummaryTable({ history }: { history: History }) {
  const rows = metricRows(history)
  const partial =
    history.coverage.includes_today ||
    history.coverage.days_with_data < history.coverage.days_in_range
  const note = history.coverage.includes_today
    ? 'Today is still in progress, so totals may be incomplete.'
    : history.coverage.days_with_data < history.coverage.days_in_range
      ? 'Some days in this range have no data, so totals may be understated.'
      : null

  return (
    <section className="summary panel" aria-label="Summary">
      <div className="section-heading">
        <div>
          <p className="eyebrow">Summary</p>
          <h2>Range overview</h2>
        </div>
        <span className="muted">
          {history.sample_count} archive {history.sample_count === 1 ? 'record' : 'records'}
        </span>
      </div>
      <div className="table-scroll">
        <table className="summary-table">
          <caption className="sr-only">
            High, low, average and total values for the selected range
          </caption>
          <thead>
            <tr>
              <th scope="col">Summary</th>
              <th scope="col">High</th>
              <th scope="col">Low</th>
              <th scope="col">Average</th>
              <th scope="col">Total</th>
            </tr>
          </thead>
          <tbody>
            {rows.map((row) => (
              <tr key={row.key}>
                <th scope="row">{row.label}</th>
                <td data-label="High">
                  {row.direction ? '—' : valueOrDash(row.metric.high, row.digits)}
                </td>
                <td data-label="Low">
                  {row.direction ? '—' : valueOrDash(row.metric.low, row.digits)}
                </td>
                <td data-label="Average">
                  {row.direction
                    ? directionValue(row.metric.average)
                    : valueOrDash(row.metric.average, row.digits)}
                </td>
                <td data-label="Total">
                  {valueOrDash(row.metric.total, row.digits)}
                  {row.key === 'precipitation_accumulation' && partial && row.metric.total !== null ? (
                    <span className="partial-marker" title={note ?? undefined}>
                      *
                    </span>
                  ) : null}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      {note ? <p className="table-note">{note}</p> : null}
    </section>
  )
}
