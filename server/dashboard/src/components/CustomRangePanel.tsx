import { useMemo, useState } from 'react'

import { daysInMonth, parseISODate } from '../format'
import {
  customSelection,
  isoFromParts,
  rangeDates,
  validateCustomRange,
  yearOptions,
} from '../range'
import type { RangeSelection } from '../range'

interface DateDropdownsProps {
  label: string
  value: string
  years: number[]
  onChange: (value: string) => void
}

function DateDropdowns({ label, value, years, onChange }: DateDropdownsProps) {
  const parts = parseISODate(value) ?? { year: years[0] ?? 2026, month: 1, day: 1 }
  const monthDays = daysInMonth(parts.year, parts.month)

  function update(nextYear: number, nextMonth: number, nextDay: number) {
    onChange(isoFromParts(nextYear, nextMonth, nextDay))
  }

  return (
    <fieldset className="date-fieldset">
      <legend>{label}</legend>
      <div className="date-selects">
        <label>
          <span className="sr-only">{label} year</span>
          <select
            aria-label={`${label} year`}
            value={parts.year}
            onChange={(event) => update(Number(event.target.value), parts.month, parts.day)}
          >
            {years.map((year) => (
              <option key={year} value={year}>
                {year}
              </option>
            ))}
          </select>
        </label>
        <label>
          <span className="sr-only">{label} month</span>
          <select
            aria-label={`${label} month`}
            value={parts.month}
            onChange={(event) => update(parts.year, Number(event.target.value), parts.day)}
          >
            {Array.from({ length: 12 }, (_, index) => index + 1).map((month) => (
              <option key={month} value={month}>
                {new Intl.DateTimeFormat('en-US', { month: 'short', timeZone: 'UTC' }).format(
                  new Date(Date.UTC(2020, month - 1, 1)),
                )}
              </option>
            ))}
          </select>
        </label>
        <label>
          <span className="sr-only">{label} day</span>
          <select
            aria-label={`${label} day`}
            value={Math.min(parts.day, monthDays)}
            onChange={(event) => update(parts.year, parts.month, Number(event.target.value))}
          >
            {Array.from({ length: monthDays }, (_, index) => index + 1).map((day) => (
              <option key={day} value={day}>
                {day}
              </option>
            ))}
          </select>
        </label>
      </div>
    </fieldset>
  )
}

interface CustomRangePanelProps {
  selection: RangeSelection
  siteDate: string
  firstDate: string | null
  onApply: (selection: RangeSelection) => void
  onCancel: () => void
}

export function CustomRangePanel({
  selection,
  siteDate,
  firstDate,
  onApply,
  onCancel,
}: CustomRangePanelProps) {
  const initial = rangeDates(selection, siteDate)
  const [start, setStart] = useState(initial.start)
  const [end, setEnd] = useState(initial.end)
  const years = useMemo(() => yearOptions(firstDate, siteDate), [firstDate, siteDate])
  const error = validateCustomRange(start, end, siteDate)

  return (
    <div className="custom-range">
      <div className="custom-range-fields">
        <DateDropdowns label="Start" value={start} years={years} onChange={setStart} />
        <DateDropdowns label="End" value={end} years={years} onChange={setEnd} />
      </div>
      <div className="custom-range-actions">
        <p className={`field-message ${error ? 'is-error' : ''}`}>{error ?? ' '}</p>
        <div className="custom-range-buttons">
          <button type="button" className="button secondary" onClick={onCancel}>
            Cancel
          </button>
          <button
            type="button"
            className="button primary"
            disabled={error !== null}
            onClick={() => {
              if (!error) onApply(customSelection(start, end, siteDate))
            }}
          >
            Apply range
          </button>
        </div>
      </div>
    </div>
  )
}
