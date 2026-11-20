import { render, screen } from '@testing-library/react'
import { describe, expect, it } from 'vitest'

import { historyFixture, pointFixture } from '../test/fixtures'
import { HistoryTable } from './HistoryTable'

describe('HistoryTable', () => {
  it('renders archive rows with station-local times and metrics', () => {
    render(
      <HistoryTable
        history={historyFixture({
          points: [
            pointFixture('2026-11-20T03:30:00Z', {
              out_temp_c: 24,
              dewpoint_c: 14,
              out_humidity_pct: 60,
              wind_speed_m_s: 2,
              wind_gust_m_s: 4,
              wind_dir_deg: 210,
              rain_rate_mm_h: 1.25,
              rain_mm: 0.5,
              pressure_hpa: 1008,
              solar_wm2: 500,
              uv_index: 4,
            }),
          ],
        })}
      />,
    )
    expect(screen.getByRole('columnheader', { name: 'Temp' })).toBeInTheDocument()
    expect(screen.getByText('9:00 AM')).toBeInTheDocument()
    expect(screen.getByText('24.0 °C')).toBeInTheDocument()
    expect(screen.getByText('210° SSW')).toBeInTheDocument()
    expect(screen.getByText('1.25 mm/h')).toBeInTheDocument()
    expect(screen.getByText('500 W/m²')).toBeInTheDocument()
  })
})
