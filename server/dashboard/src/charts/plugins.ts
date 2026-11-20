import type { Plugin } from 'chart.js'

import { COLORS } from './chartTheme'

export const crosshairPlugin: Plugin<'line'> = {
  id: 'pwsCrosshair',
  afterDraw(chart) {
    const active = chart.tooltip?.getActiveElements()
    if (!active || active.length === 0) return
    const x = active[0].element.x
    const { top, bottom } = chart.chartArea
    const context = chart.ctx
    context.save()
    context.beginPath()
    context.moveTo(x, top)
    context.lineTo(x, bottom)
    context.lineWidth = 1
    context.strokeStyle = 'rgba(107, 128, 139, 0.55)'
    context.stroke()
    context.restore()
  },
}

export function alternatingBandsPlugin(size: number): Plugin<'line'> {
  return {
    id: 'pwsAlternatingBands',
    beforeDraw(chart) {
      const total = chart.data.labels?.length ?? 0
      if (size < 1 || total < 2) return
      const { ctx, chartArea } = chart
      const xScale = chart.scales.x
      if (!xScale || !chartArea) return
      for (let index = size; index < total; index += size * 2) {
        const start = xScale.getPixelForValue(index)
        const end = xScale.getPixelForValue(Math.min(index + size - 1, total - 1))
        const left = Math.max(start, chartArea.left)
        const right = Math.min(end, chartArea.right)
        if (right <= left) continue
        ctx.save()
        ctx.fillStyle = COLORS.band
        ctx.fillRect(left, chartArea.top, right - left, chartArea.bottom - chartArea.top)
        ctx.restore()
      }
    },
  }
}
