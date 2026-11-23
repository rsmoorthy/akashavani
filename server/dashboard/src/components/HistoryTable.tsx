import {
  WIND_DISPLAY_UNIT,
  degreesToCompass,
  formatNumber,
  formatPressure,
  formatRain,
  formatRainRate,
  formatSolar,
  formatTemperature,
  formatWind,
  tableTime,
} from '../format'
import type { History } from '../types'

function direction(value: number | null): string {
  return value === null ? '—' : `${Math.round(value)}° ${degreesToCompass(value)}`
}

export function HistoryTable({ history }: { history: History }) {
  const { points, timezone, bucket_seconds: bucketSeconds } = history
  return (
    <section className="history-table-panel panel" aria-label="History table">
      <div className="section-heading">
        <div>
          <p className="eyebrow">Table view</p>
          <h2>Archive readings</h2>
        </div>
        <span className="muted">{points.length} rows</span>
      </div>
      <div className="table-scroll history-table-scroll">
        <table className="history-table">
          <caption className="sr-only">
            Archive readings for the selected range at {bucketSeconds / 60} minute
            resolution
          </caption>
          <thead>
            <tr>
              <th scope="col">Time</th>
              <th scope="col">Temp</th>
              <th scope="col">Dew</th>
              <th scope="col">Humidity</th>
              <th scope="col">Wind ({WIND_DISPLAY_UNIT})</th>
              <th scope="col">Gust ({WIND_DISPLAY_UNIT})</th>
              <th scope="col">Direction</th>
              <th scope="col">Rain rate</th>
              <th scope="col">Rain</th>
              <th scope="col">Pressure</th>
              <th scope="col">Solar</th>
              <th scope="col">UV</th>
            </tr>
          </thead>
          <tbody>
            {points.map((point) => (
              <tr key={point.t}>
                <th scope="row">{tableTime(point.t, timezone, bucketSeconds)}</th>
                <td>{formatTemperature(point.out_temp_c)}</td>
                <td>{formatTemperature(point.dewpoint_c)}</td>
                <td>
                  {point.out_humidity_pct === null
                    ? '—'
                    : `${formatNumber(point.out_humidity_pct, 0)}%`}
                </td>
                <td>{formatWind(point.wind_speed_m_s)}</td>
                <td>{formatWind(point.wind_gust_m_s)}</td>
                <td>{direction(point.wind_dir_deg)}</td>
                <td>{formatRainRate(point.rain_rate_mm_h)}</td>
                <td>{formatRain(point.rain_mm)}</td>
                <td>{formatPressure(point.pressure_hpa)}</td>
                <td>{formatSolar(point.solar_wm2)}</td>
                <td>{formatNumber(point.uv_index, 1)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  )
}
