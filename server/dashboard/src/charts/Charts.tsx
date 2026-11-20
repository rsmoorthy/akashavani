import { useMemo } from 'react'

import { degreesToCompass } from '../format'
import type { History } from '../types'
import { COLORS, bandSizeForBucket } from './chartTheme'
import { hasAnyData, makeChartConfig } from './chartOptions'
import { PwsChart } from './PwsChart'

interface ChartSectionProps {
  title: string
  subtitle: string
  ariaLabel: string
  config: ReturnType<typeof makeChartConfig>
  hasData: boolean
  bandSize: number
}

function ChartSection({
  title,
  subtitle,
  ariaLabel,
  config,
  hasData,
  bandSize,
}: ChartSectionProps) {
  return (
    <section className="chart-section">
      <div className="chart-section-head">
        <h3>{title}</h3>
        <span>{subtitle}</span>
      </div>
      {hasData ? (
        <PwsChart config={config} ariaLabel={ariaLabel} bandSize={bandSize} />
      ) : (
        <p className="empty-state">No data for this period.</p>
      )}
    </section>
  )
}

export function Charts({ history }: { history: History }) {
  const { points, timezone, bucket_seconds: bucketSeconds } = history
  const bandSize = bandSizeForBucket(bucketSeconds)

  const temperature = useMemo(() => {
    const temperatureValues = points.map((point) => point.out_temp_c)
    const dewpointValues = points.map((point) => point.dewpoint_c)
    return {
      config: makeChartConfig({
        points,
        timeZone: timezone,
        bucketSeconds,
        yTitle: '°C',
        series: [
          {
            label: 'Temperature',
            data: temperatureValues,
            color: COLORS.temp,
            unit: '°C',
          },
          {
            label: 'Dew point',
            data: dewpointValues,
            color: COLORS.dewpoint,
            unit: '°C',
          },
        ],
      }),
      hasData: hasAnyData(temperatureValues) || hasAnyData(dewpointValues),
    }
  }, [points, timezone, bucketSeconds])

  const wind = useMemo(() => {
    const speed = points.map((point) => point.wind_speed_m_s)
    const gust = points.map((point) => point.wind_gust_m_s)
    return {
      config: makeChartConfig({
        points,
        timeZone: timezone,
        bucketSeconds,
        yTitle: 'm/s',
        yBeginAtZero: true,
        series: [
          { label: 'Wind speed', data: speed, color: COLORS.wind, unit: 'm/s' },
          { label: 'Gust', data: gust, color: COLORS.gust, unit: 'm/s' },
        ],
      }),
      hasData: hasAnyData(speed) || hasAnyData(gust),
    }
  }, [points, timezone, bucketSeconds])

  const direction = useMemo(() => {
    const values = points.map((point) => point.wind_dir_deg)
    return {
      config: makeChartConfig({
        points,
        timeZone: timezone,
        bucketSeconds,
        yTitle: 'Direction',
        yMin: 0,
        yMax: 360,
        yTickStep: 90,
        yTicks: (value) => degreesToCompass(value),
        series: [
          {
            label: 'Wind direction',
            data: values,
            color: COLORS.direction,
            unit: '°',
            digits: 0,
          },
        ],
      }),
      hasData: hasAnyData(values),
    }
  }, [points, timezone, bucketSeconds])

  const precipitation = useMemo(() => {
    const rate = points.map((point) => point.rain_rate_mm_h)
    const accumulation = points.map((point) => point.rain_mm)
    return {
      config: makeChartConfig({
        points,
        timeZone: timezone,
        bucketSeconds,
        yTitle: 'mm/h',
        yBeginAtZero: true,
        y1Title: 'mm',
        y1Min: 0,
        series: [
          {
            label: 'Precipitation rate',
            data: rate,
            color: COLORS.rain,
            unit: 'mm/h',
            digits: 2,
          },
          {
            label: 'Accumulation',
            data: accumulation,
            color: COLORS.accumulation,
            unit: 'mm',
            digits: 2,
            yAxisID: 'y1',
          },
        ],
      }),
      hasData: hasAnyData(rate) || hasAnyData(accumulation),
    }
  }, [points, timezone, bucketSeconds])

  const pressure = useMemo(() => {
    const values = points.map((point) => point.pressure_hpa)
    const seaLevel = points.some((point) => point.barometer_hpa !== null)
    return {
      config: makeChartConfig({
        points,
        timeZone: timezone,
        bucketSeconds,
        yTitle: 'hPa',
        series: [
          {
            label: 'Pressure',
            data: values,
            color: COLORS.pressure,
            unit: 'hPa',
          },
        ],
      }),
      hasData: hasAnyData(values),
      subtitle: seaLevel ? 'sea-level hPa' : 'station hPa',
    }
  }, [points, timezone, bucketSeconds])

  const solar = useMemo(() => {
    const values = points.map((point) => point.solar_wm2)
    const uv = points.map((point) => point.uv_index)
    return {
      config: makeChartConfig({
        points,
        timeZone: timezone,
        bucketSeconds,
        yTitle: 'W/m²',
        yMin: 0,
        y1Title: 'UV index',
        y1Min: 0,
        series: [
          {
            label: 'Solar radiation',
            data: values,
            color: COLORS.solar,
            unit: 'W/m²',
            digits: 0,
          },
          {
            label: 'UV index',
            data: uv,
            color: COLORS.uv,
            unit: '',
            digits: 1,
            yAxisID: 'y1',
          },
        ],
      }),
      hasData: hasAnyData(values) || hasAnyData(uv),
    }
  }, [points, timezone, bucketSeconds])

  return (
    <div className="charts-list">
      <ChartSection
        title="Temperature & dew point"
        subtitle="°C"
        ariaLabel="Temperature and dew point over time"
        config={temperature.config}
        hasData={temperature.hasData}
        bandSize={bandSize}
      />
      <ChartSection
        title="Wind speed & gust"
        subtitle="m/s"
        ariaLabel="Wind speed and gust over time"
        config={wind.config}
        hasData={wind.hasData}
        bandSize={bandSize}
      />
      <ChartSection
        title="Wind direction"
        subtitle="compass"
        ariaLabel="Wind direction over time"
        config={direction.config}
        hasData={direction.hasData}
        bandSize={bandSize}
      />
      <ChartSection
        title="Precipitation"
        subtitle="mm/h and mm"
        ariaLabel="Precipitation rate and accumulation over time"
        config={precipitation.config}
        hasData={precipitation.hasData}
        bandSize={bandSize}
      />
      <ChartSection
        title="Pressure"
        subtitle={pressure.subtitle}
        ariaLabel="Pressure over time"
        config={pressure.config}
        hasData={pressure.hasData}
        bandSize={bandSize}
      />
      <ChartSection
        title="Solar radiation & UV index"
        subtitle="W/m² and index"
        ariaLabel="Solar radiation and UV index over time"
        config={solar.config}
        hasData={solar.hasData}
        bandSize={bandSize}
      />
    </div>
  )
}
