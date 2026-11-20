import { useState } from 'react'

import {
  canGoBack,
  canGoForward,
  rangeDates,
  selectionLabel,
  shiftSelection,
} from '../range'
import type { RangeSelection } from '../range'
import type { Period } from '../types'
import { CustomRangePanel } from './CustomRangePanel'

const PERIODS: { value: Period; label: string }[] = [
  { value: 'day', label: 'Day' },
  { value: 'week', label: 'Week' },
  { value: 'month', label: 'Month' },
  { value: 'custom', label: 'Custom' },
]

interface RangeControlsProps {
  selection: RangeSelection
  siteDate: string
  firstDate: string | null
  onChange: (selection: RangeSelection) => void
}

export function RangeControls({
  selection,
  siteDate,
  firstDate,
  onChange,
}: RangeControlsProps) {
  const [customOpen, setCustomOpen] = useState(selection.period === 'custom')
  const label = selectionLabel(selection, siteDate)
  const window = rangeDates(selection, siteDate)
  const canForward = canGoForward(selection, siteDate)
  const canBack = canGoBack(selection, firstDate, siteDate)

  function pickPeriod(period: Period) {
    if (period === 'custom') {
      setCustomOpen(true)
      return
    }
    setCustomOpen(false)
    const anchor = selection.period === 'custom' ? selection.end ?? siteDate : selection.date
    onChange({ period, date: anchor })
  }

  return (
    <section className="range-controls panel" aria-label="History range">
      <div className="range-top">
        <div className="range-nav">
          <button
            type="button"
            className="icon-button"
            aria-label="Previous range"
            disabled={customOpen || !canBack}
            onClick={() => onChange(shiftSelection(selection, -1, siteDate))}
          >
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path d="M15 5 8 12l7 7" />
            </svg>
          </button>
          <div className="range-label">
            <span className="eyebrow">History</span>
            <strong>{label}</strong>
          </div>
          <button
            type="button"
            className="icon-button"
            aria-label="Next range"
            disabled={customOpen || !canForward}
            onClick={() => onChange(shiftSelection(selection, 1, siteDate))}
          >
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path d="m9 5 7 7-7 7" />
            </svg>
          </button>
        </div>
        <div className="period-buttons" role="group" aria-label="Period">
          {PERIODS.map((period) => {
            const active =
              period.value === 'custom'
                ? customOpen || selection.period === 'custom'
                : selection.period === period.value && !customOpen
            return (
              <button
                key={period.value}
                type="button"
                className={`segment-button ${active ? 'is-active' : ''}`}
                aria-pressed={active}
                aria-expanded={period.value === 'custom' ? customOpen : undefined}
                onClick={() => pickPeriod(period.value)}
              >
                {period.label}
              </button>
            )
          })}
        </div>
      </div>
      {customOpen ? (
        <CustomRangePanel
          selection={{ ...selection, start: window.start, end: window.end }}
          siteDate={siteDate}
          firstDate={firstDate}
          onApply={(next) => {
            setCustomOpen(false)
            onChange(next)
          }}
          onCancel={() => setCustomOpen(false)}
        />
      ) : null}
    </section>
  )
}
