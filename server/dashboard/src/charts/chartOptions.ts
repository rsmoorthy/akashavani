import type {
  ChartConfiguration,
  ChartDataset,
  ChartOptions,
  TooltipItem,
} from 'chart.js'

import { axisLabel, formatNumber, tooltipTitle } from '../format'
import type { HistoryPoint } from '../types'
import { CHART_FONT, COLORS } from './chartTheme'

export interface SeriesSpec {
  label: string
  data: (number | null)[]
  color: string
  unit: string
  digits?: number
  yAxisID?: 'y' | 'y1'
  fill?: boolean
}

export interface ChartSpec {
  points: HistoryPoint[]
  timeZone: string
  bucketSeconds: number
  series: SeriesSpec[]
  yTitle: string
  y1Title?: string
  yMin?: number
  yMax?: number
  yBeginAtZero?: boolean
  yTickStep?: number
  y1Min?: number
  y1Max?: number
  yTicks?: (value: number) => string
  y1Ticks?: (value: number) => string
}

export function makeChartConfig(spec: ChartSpec): ChartConfiguration<'line'> {
  const labels = spec.points.map((point) =>
    axisLabel(point.t, spec.timeZone, spec.bucketSeconds),
  )
  const narrow = typeof window !== 'undefined' && window.innerWidth < 640
  let tickInterval: number
  if (spec.bucketSeconds <= 300) {
    tickInterval = Math.max(narrow ? 72 : 36, Math.ceil(labels.length / (narrow ? 4 : 8)))
  } else if (spec.bucketSeconds <= 3600) {
    tickInterval =
      labels.length <= 48 ? (narrow ? 24 : 12) : narrow ? 48 : 24
  } else {
    tickInterval = Math.max(1, Math.ceil(labels.length / (narrow ? 4 : 8)))
  }
  const datasets: ChartDataset<'line', (number | null)[]>[] = spec.series.map((series) => ({
    label: series.label,
    data: series.data,
    borderColor: series.color,
    backgroundColor: series.fill ? `${series.color}22` : series.color,
    borderWidth: 2,
    fill: series.fill ?? false,
    pointRadius: 0,
    pointHoverRadius: 4,
    pointHitRadius: 14,
    pointHoverBorderWidth: 2,
    tension: 0.24,
    spanGaps: false,
    yAxisID: series.yAxisID ?? 'y',
  }))

  const options: ChartOptions<'line'> = {
    responsive: true,
    maintainAspectRatio: false,
    animation: false,
    normalized: true,
    interaction: { mode: 'index', intersect: false },
    layout: { padding: { top: 6, right: 6, bottom: 0, left: 2 } },
    elements: {
      line: { borderJoinStyle: 'round', borderCapStyle: 'round' },
    },
    scales: {
      x: {
        type: 'category',
        grid: { display: false, drawTicks: false },
        border: { display: false },
        ticks: {
          color: COLORS.text,
          maxRotation: 0,
          minRotation: 0,
          autoSkip: false,
          padding: 6,
          callback: (_value, index) =>
            index % tickInterval === 0 ? (labels[index] ?? '') : '',
        },
      },
      y: {
        type: 'linear',
        beginAtZero: spec.yBeginAtZero ?? false,
        grace: '8%',
        min: spec.yMin,
        max: spec.yMax,
        grid: { color: COLORS.grid, drawTicks: false },
        border: { display: false },
        title: {
          display: true,
          text: spec.yTitle,
          color: COLORS.text,
          font: { size: 11, family: CHART_FONT },
        },
        ticks: {
          color: COLORS.text,
          maxTicksLimit: 6,
          stepSize: spec.yTickStep,
          padding: 4,
          callback: spec.yTicks ? (value) => spec.yTicks?.(Number(value)) : undefined,
        },
      },
    },
    plugins: {
      legend: {
        position: 'bottom',
        align: 'start',
        labels: {
          color: COLORS.text,
          boxWidth: 12,
          boxHeight: 2,
          padding: 14,
          usePointStyle: true,
          pointStyle: 'line',
          font: { size: 11, family: CHART_FONT },
        },
      },
      tooltip: {
        enabled: true,
        backgroundColor: 'rgba(16, 30, 38, 0.92)',
        titleFont: { size: 12, family: CHART_FONT },
        bodyFont: { size: 12, family: CHART_FONT },
        padding: 10,
        cornerRadius: 6,
        displayColors: true,
        callbacks: {
          title: (items: TooltipItem<'line'>[]) => {
            const first = items[0]
            const point = first ? spec.points[first.dataIndex] : undefined
            return point ? tooltipTitle(point.t, spec.timeZone, spec.bucketSeconds) : ''
          },
          label: (item: TooltipItem<'line'>) => {
            const series = spec.series[item.datasetIndex]
            const value = item.parsed.y
            if (!series) return ''
            const suffix = value === null || !series.unit ? '' : ` ${series.unit}`
            return `${series.label}: ${formatNumber(value, series.digits ?? 1)}${suffix}`
          },
        },
      },
    },
  }

  if (spec.y1Title) {
    options.scales = {
      ...options.scales,
      y1: {
        type: 'linear',
        position: 'right',
        beginAtZero: false,
        grace: '8%',
        min: spec.y1Min,
        max: spec.y1Max,
        grid: { drawOnChartArea: false, drawTicks: false },
        border: { display: false },
        title: {
          display: true,
          text: spec.y1Title,
          color: COLORS.text,
          font: { size: 11, family: CHART_FONT },
        },
        ticks: {
          color: COLORS.text,
          maxTicksLimit: 5,
          padding: 4,
          callback: spec.y1Ticks ? (value) => spec.y1Ticks?.(Number(value)) : undefined,
        },
      },
    }
  }

  return {
    type: 'line',
    data: { labels, datasets },
    options,
  }
}

export function hasAnyData(values: (number | null)[]): boolean {
  return values.some((value) => value !== null && value !== undefined)
}
