import { Chart, registerables } from 'chart.js'
import type { ChartConfiguration } from 'chart.js'
import { useEffect, useRef } from 'react'

import { CHART_FONT, COLORS } from './chartTheme'
import { alternatingBandsPlugin, crosshairPlugin } from './plugins'

Chart.register(...registerables)
Chart.defaults.font.family = CHART_FONT
Chart.defaults.color = COLORS.text

interface PwsChartProps {
  config: ChartConfiguration<'line'>
  ariaLabel: string
  bandSize: number
}

export function PwsChart({ config, ariaLabel, bandSize }: PwsChartProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const chartRef = useRef<Chart<'line'> | null>(null)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const chart = new Chart<'line'>(canvas, {
      ...config,
      plugins: [crosshairPlugin, alternatingBandsPlugin(bandSize)],
    })
    chartRef.current = chart
    return () => {
      chart.destroy()
      chartRef.current = null
    }
  }, [config, bandSize])

  return (
    <div className="chart-canvas">
      <canvas ref={canvasRef} role="img" aria-label={ariaLabel} />
    </div>
  )
}
