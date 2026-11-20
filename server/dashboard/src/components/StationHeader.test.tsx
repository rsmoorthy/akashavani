import { render, screen } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'

import { stationFixture } from '../test/fixtures'
import { StationHeader } from './StationHeader'

describe('StationHeader', () => {
  it('shows N/A for missing site fields', () => {
    render(
      <StationHeader
        station={stationFixture({
          online: false,
          latitude: null,
          longitude: null,
          altitude_m: null,
          last_seen_at: null,
        })}
        asOf={null}
        selected={false}
        refreshing={false}
        onRefresh={vi.fn()}
      />,
    )
    expect(screen.getByText('Roof receiver')).toBeInTheDocument()
    expect(screen.getByText(/Elev N\/A, N\/A N\/A/)).toBeInTheDocument()
    expect(screen.getByText('Offline')).toBeInTheDocument()
    expect(screen.getByText('No observations yet')).toBeInTheDocument()
  })

  it('shows connected status and selected reading state', () => {
    render(
      <StationHeader
        station={stationFixture()}
        asOf="2026-11-20T05:20:00Z"
        selected
        refreshing
        onRefresh={vi.fn()}
      />,
    )
    expect(screen.getByText('Connected')).toBeInTheDocument()
    expect(screen.getByText('Selected reading:')).toBeInTheDocument()
  })
})
