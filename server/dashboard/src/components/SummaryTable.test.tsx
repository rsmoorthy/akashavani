import { render, screen } from '@testing-library/react'
import { describe, expect, it } from 'vitest'

import { historyFixture } from '../test/fixtures'
import { SummaryTable } from './SummaryTable'

describe('SummaryTable', () => {
  it('renders high, low, average and total values', () => {
    render(<SummaryTable history={historyFixture()} />)
    expect(screen.getByText('Temperature (°C)')).toBeInTheDocument()
    expect(screen.getAllByText('24.0').length).toBeGreaterThan(0)
    expect(screen.getByText('210° SSW')).toBeInTheDocument()
    expect(screen.getByText('1.50')).toBeInTheDocument()
  })

  it('marks a partial accumulation total', () => {
    render(<SummaryTable history={historyFixture()} />)
    expect(screen.getByText('Today is still in progress, so totals may be incomplete.')).toBeInTheDocument()
    expect(screen.getByText('*')).toBeInTheDocument()
  })

  it('renders dashes when a range has no data', () => {
    const empty = historyFixture({
      sample_count: 0,
      points: [],
      coverage: { days_in_range: 1, days_with_data: 0, includes_today: false },
      summary: {
        temperature: { high: null, low: null, average: null, total: null },
        dew_point: { high: null, low: null, average: null, total: null },
        humidity: { high: null, low: null, average: null, total: null },
        precipitation_rate: { high: null, low: null, average: null, total: null },
        precipitation_accumulation: { high: null, low: null, average: null, total: null },
        wind_speed: { high: null, low: null, average: null, total: null },
        wind_gust: { high: null, low: null, average: null, total: null },
        wind_direction: { high: null, low: null, average: null, total: null },
        pressure: { high: null, low: null, average: null, total: null },
      },
    })
    render(<SummaryTable history={empty} />)
    expect(screen.getAllByText('—').length).toBeGreaterThan(20)
  })
})
