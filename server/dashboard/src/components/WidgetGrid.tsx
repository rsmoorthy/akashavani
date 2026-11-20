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

function secondary(label: string, value: string): string {
  return value === '—' ? label : `${label} ${value}`
}

export function WidgetGrid({ values }: { values: DisplayValues }) {
  const feelsLike = values.app_temp_c ?? values.out_temp_c
  const pressure = values.barometer_hpa ?? values.pressure_hpa
  const pressureLabel =
    values.barometer_hpa !== null ? 'Sea-level' : values.pressure_hpa !== null ? 'Station' : 'Pressure'
  const directionText =
    values.wind_dir_deg === null
      ? '—'
      : `${degreesToCompass(values.wind_dir_deg)} ${Math.round(values.wind_dir_deg)}°`

  return (
    <section className="widget-grid" aria-label="Current conditions">
      <WidgetCard
        title="Temperature"
        accent="temp"
        ariaLabel={`Temperature ${formatTemperature(values.out_temp_c)}`}
      >
        <TemperatureGauge value={values.out_temp_c} />
        <p className="widget-value">{formatTemperature(values.out_temp_c)}</p>
        <p className="widget-secondary">Feels like {formatTemperature(feelsLike)}</p>
      </WidgetCard>

      <WidgetCard
        title="Humidity"
        accent="humidity"
        ariaLabel={`Humidity ${formatHumidity(values.out_humidity_pct)}`}
      >
        <HumidityRing value={values.out_humidity_pct} />
        <p className="widget-value">{formatHumidity(values.out_humidity_pct)}</p>
        <p className="widget-secondary">
          {secondary('Dew point', formatTemperature(values.dewpoint_c))}
        </p>
      </WidgetCard>

      <WidgetCard
        title="Wind"
        accent="wind"
        ariaLabel={`Wind ${formatWind(values.wind_speed_m_s)}, ${directionText}`}
      >
        <WindCompass direction={values.wind_dir_deg} speed={values.wind_speed_m_s} />
        <p className="widget-value">{formatWind(values.wind_speed_m_s)}</p>
        <p className="widget-secondary">
          {secondary('Gust', formatWind(values.wind_gust_m_s))}
        </p>
        <p className="widget-tertiary">{directionText}</p>
      </WidgetCard>

      <WidgetCard
        title="Precipitation"
        accent="rain"
        ariaLabel={`Precipitation ${formatRain(values.rain_total_mm)}`}
      >
        <RainDroplet value={values.rain_total_mm} />
        <p className="widget-value">{formatRain(values.rain_total_mm)}</p>
        <p className="widget-secondary">
          {secondary('Rate', formatRainRate(values.rain_rate_mm_h))}
        </p>
      </WidgetCard>

      <WidgetCard
        title="Pressure"
        accent="pressure"
        ariaLabel={`Pressure ${formatPressure(pressure)}`}
      >
        <PressureDial value={pressure} />
        <p className="widget-value">{formatPressure(pressure)}</p>
        <p className="widget-secondary">{pressureLabel} pressure</p>
      </WidgetCard>

      <WidgetCard
        title="UV"
        accent="uv"
        ariaLabel={`UV index ${formatUv(values.uv_index)}, ${uvCategory(values.uv_index)}`}
      >
        <UvScale value={values.uv_index} />
        <p className="widget-value">{formatUv(values.uv_index)}</p>
        <p className="widget-secondary">{uvCategory(values.uv_index)}</p>
      </WidgetCard>

      <WidgetCard
        title="Radiation"
        accent="solar"
        ariaLabel={`Solar radiation ${formatSolar(values.solar_wm2)}`}
      >
        <SolarRing value={values.solar_wm2} />
        <p className="widget-value">{formatSolar(values.solar_wm2)}</p>
        <p className="widget-secondary">Solar radiation</p>
      </WidgetCard>
    </section>
  )
}
