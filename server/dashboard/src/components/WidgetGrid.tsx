import {
  degreesToCompass,
  formatHumidity,
  formatPressure,
  formatRain,
  formatRainRate,
  formatSolar,
  formatTemperature,
  formatUv,
  formatWind,
  uvCategory,
} from '../format'
import type { DisplayValues } from '../types'
import { WidgetCard } from './WidgetCard'
import {
  HumidityRing,
  PressureDial,
  RainDroplet,
  SolarRing,
  TemperatureGauge,
  UvScale,
  WindCompass,
} from './widgets/gauges'

function secondary(label: string, value: string, stale: boolean): string {
  if (stale) return `${label} NA`
  return value === '—' ? label : `${label} ${value}`
}

export function WidgetGrid({
  values,
  stale = false,
}: {
  values: DisplayValues
  stale?: boolean
}) {
  const shown: DisplayValues = stale
    ? {
        ...values,
        out_temp_c: null,
        app_temp_c: null,
        dewpoint_c: null,
        out_humidity_pct: null,
        wind_speed_m_s: null,
        wind_gust_m_s: null,
        wind_dir_deg: null,
        pressure_hpa: null,
        barometer_hpa: null,
        rain_rate_mm_h: null,
        rain_total_mm: null,
        solar_wm2: null,
        uv_index: null,
      }
    : values
  const main = (text: string) => (stale ? 'NA' : text)
  const feelsLike = shown.app_temp_c ?? shown.out_temp_c
  const pressure = shown.barometer_hpa ?? shown.pressure_hpa
  const pressureLabel =
    values.barometer_hpa !== null
      ? 'Sea-level'
      : values.pressure_hpa !== null
        ? 'Station'
        : 'Pressure'
  const directionText =
    shown.wind_dir_deg === null
      ? '—'
      : `${degreesToCompass(shown.wind_dir_deg)} ${Math.round(shown.wind_dir_deg)}°`

  return (
    <section
      className="widget-grid"
      aria-label="Current conditions"
      title={
        stale
          ? 'No live data: the station has not reported for over 30 minutes.'
          : undefined
      }
    >
      <WidgetCard
        title="Temperature"
        accent="temp"
        ariaLabel={`Temperature ${main(formatTemperature(shown.out_temp_c))}`}
      >
        <TemperatureGauge value={shown.out_temp_c} />
        <p className="widget-value">{main(formatTemperature(shown.out_temp_c))}</p>
        <p className="widget-secondary">
          {secondary('Feels like', formatTemperature(feelsLike), stale)}
        </p>
      </WidgetCard>

      <WidgetCard
        title="Humidity"
        accent="humidity"
        ariaLabel={`Humidity ${main(formatHumidity(shown.out_humidity_pct))}`}
      >
        <HumidityRing value={shown.out_humidity_pct} />
        <p className="widget-value">{main(formatHumidity(shown.out_humidity_pct))}</p>
        <p className="widget-secondary">
          {secondary('Dew point', formatTemperature(shown.dewpoint_c), stale)}
        </p>
      </WidgetCard>

      <WidgetCard
        title="Wind"
        accent="wind"
        ariaLabel={`Wind ${main(formatWind(shown.wind_speed_m_s))}, ${main(directionText)}`}
      >
        <WindCompass direction={shown.wind_dir_deg} speed={shown.wind_speed_m_s} />
        <p className="widget-value">{main(formatWind(shown.wind_speed_m_s))}</p>
        <p className="widget-secondary">
          {secondary('Gust', formatWind(shown.wind_gust_m_s), stale)}
        </p>
        <p className="widget-tertiary">{main(directionText)}</p>
      </WidgetCard>

      <WidgetCard
        title="Precipitation"
        accent="rain"
        ariaLabel={`Precipitation ${main(formatRain(shown.rain_total_mm))}`}
      >
        <RainDroplet value={shown.rain_total_mm} />
        <p className="widget-value">{main(formatRain(shown.rain_total_mm))}</p>
        <p className="widget-secondary">
          {secondary('Rate', formatRainRate(shown.rain_rate_mm_h), stale)}
        </p>
      </WidgetCard>

      <WidgetCard
        title="Pressure"
        accent="pressure"
        ariaLabel={`Pressure ${main(formatPressure(pressure))}`}
      >
        <PressureDial value={pressure} />
        <p className="widget-value">{main(formatPressure(pressure))}</p>
        <p className="widget-secondary">
          {stale ? 'Pressure NA' : `${pressureLabel} pressure`}
        </p>
      </WidgetCard>

      <WidgetCard
        title="UV"
        accent="uv"
        ariaLabel={`UV index ${main(formatUv(shown.uv_index))}, ${main(uvCategory(shown.uv_index))}`}
      >
        <UvScale value={shown.uv_index} />
        <p className="widget-value">{main(formatUv(shown.uv_index))}</p>
        <p className="widget-secondary">{main(uvCategory(shown.uv_index))}</p>
      </WidgetCard>

      <WidgetCard
        title="Radiation"
        accent="solar"
        ariaLabel={`Solar radiation ${main(formatSolar(shown.solar_wm2))}`}
      >
        <SolarRing value={shown.solar_wm2} />
        <p className="widget-value">{main(formatSolar(shown.solar_wm2))}</p>
        <p className="widget-secondary">Solar radiation</p>
      </WidgetCard>
    </section>
  )
}
