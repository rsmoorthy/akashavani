import { render, screen } from '@testing-library/react'
import { describe, expect, it } from 'vitest'

import { displayValuesFixture } from '../test/fixtures'
import { WidgetGrid } from './WidgetGrid'

describe('WidgetGrid', () => {
  it('renders the current metric values', () => {
    render(<WidgetGrid values={displayValuesFixture()} />)
    expect(screen.getByText('27.0 °C')).toBeInTheDocument()
    expect(screen.getByText('Feels like 26.8 °C')).toBeInTheDocument()
    expect(screen.getByText('39%')).toBeInTheDocument()
    expect(screen.getByText('1.2 m/s')).toBeInTheDocument()
    expect(screen.getByText('SSW 210°')).toBeInTheDocument()
    expect(screen.getByText('1008.5 hPa')).toBeInTheDocument()
    expect(screen.getByText('Sea-level pressure')).toBeInTheDocument()
    expect(screen.getByText('4.3')).toBeInTheDocument()
    expect(screen.getByText('836 W/m²')).toBeInTheDocument()
  })

  it('renders dashes for missing values without hiding cards', () => {
    render(
      <WidgetGrid
        values={displayValuesFixture({
          out_temp_c: null,
          app_temp_c: null,
          dewpoint_c: null,
          out_humidity_pct: null,
          wind_speed_m_s: null,
          wind_gust_m_s: null,
          wind_dir_deg: null,
          pressure_hpa: null,
          barometer_hpa: null,
          rain_rate_mm_h: null,
          rain_total_mm: null,
          solar_wm2: null,
          uv_index: null,
        })}
      />,
    )
    expect(screen.getByText('Temperature')).toBeInTheDocument()
    expect(screen.getByText('Radiation')).toBeInTheDocument()
    expect(screen.getAllByText('—').length).toBeGreaterThanOrEqual(8)
  })
})
