import { useMemo } from 'react'
import type { CSSProperties } from 'react'

import {
  formatMinute,
  latestDataIndex,
  minuteOfDay,
  nearestDataIndex,
  TIMELINE_TICKS,
} from '../timeline'
import type { HistoryPoint, SelectedObservation } from '../types'

interface TimelineSliderProps {
  points: HistoryPoint[]
  timezone: string
  selected: SelectedObservation | null
  onSelect: (selection: SelectedObservation | null) => void
  disabledReason?: string
}

export function TimelineSlider({
  points,
  timezone,
  selected,
  onSelect,
  disabledReason,
}: TimelineSliderProps) {
  const latestIndex = useMemo(() => latestDataIndex(points), [points])
  const hasData = latestIndex >= 0
  const latestMinute = hasData ? minuteOfDay(points[latestIndex].t, timezone) : 0
  const value = selected
    ? minuteOfDay(selected.point.t, timezone)
    : hasData
      ? latestMinute
      : 0
  const fill = `${(value / 1440) * 100}%`
  const label = selected && hasData ? formatMinute(value) : 'Now'

  function handleChange(nextValue: number) {
    if (!hasData) return
    if (nextValue >= latestMinute) {
      onSelect(null)
      return
    }
    const index = nearestDataIndex(points, nextValue, timezone)
    if (index < 0) {
      onSelect(null)
      return
    }
    onSelect({ index, point: points[index] })
  }

  return (
    <section className="timeline panel" aria-label="Time of day">
      <div className="timeline-head">
        <div>
          <p className="eyebrow">Time of day</p>
          <p className="timeline-help">
            {disabledReason ?? 'Drag to inspect today’s readings, or return to Now.'}
          </p>
        </div>
        <div className="timeline-now">
          {selected ? (
            <button type="button" className="text-button" onClick={() => onSelect(null)}>
              Back to Now
            </button>
          ) : (
            <span className="timeline-now-label">Now</span>
          )}
        </div>
      </div>
      <div
        className={`timeline-track ${hasData ? '' : 'is-disabled'}`}
        style={{ '--timeline-fill': fill } as CSSProperties}
      >
        <div className="timeline-value">{label}</div>
        <input
          type="range"
          min={0}
          max={1440}
          step={5}
          value={value}
          disabled={!hasData}
          aria-label="Time of day"
          aria-valuetext={label}
          onChange={(event) => handleChange(Number(event.target.value))}
        />
        <div className="timeline-ticks" aria-hidden="true">
          {TIMELINE_TICKS.map((tick) => (
            <span
              key={`${tick.minute}-${tick.label}`}
              className={`timeline-tick ${tick.minute % 180 === 0 ? 'major' : ''}`}
              style={{ left: `${(tick.minute / 1440) * 100}%` }}
            >
              {tick.label}
            </span>
          ))}
        </div>
      </div>
    </section>
  )
}
