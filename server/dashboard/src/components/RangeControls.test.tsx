import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { describe, expect, it, vi } from 'vitest'

import { RangeControls } from './RangeControls'

describe('RangeControls', () => {
  it('switches periods and opens custom date dropdowns', async () => {
    const onChange = vi.fn()
    const user = userEvent.setup()
    render(
      <RangeControls
        selection={{ period: 'day', date: '2026-11-20' }}
        siteDate="2026-11-20"
        firstDate="2026-11-01"
        onChange={onChange}
      />,
    )

    await user.click(screen.getByRole('button', { name: 'Week' }))
    expect(onChange).toHaveBeenCalledWith({ period: 'week', date: '2026-11-20' })

    onChange.mockClear()
    await user.click(screen.getByRole('button', { name: 'Custom' }))
    expect(screen.getByRole('group', { name: 'Start' })).toBeInTheDocument()
    expect(screen.getByRole('group', { name: 'End' })).toBeInTheDocument()
    expect(screen.getByRole('combobox', { name: 'Start year' })).toBeInTheDocument()

    await user.click(screen.getByRole('button', { name: 'Apply range' }))
    expect(onChange).toHaveBeenCalledWith({
      period: 'custom',
      date: '2026-11-20',
      start: '2026-11-20',
      end: '2026-11-20',
    })
  })

  it('disables forward navigation at the current day', () => {
    render(
      <RangeControls
        selection={{ period: 'day', date: '2026-11-20' }}
        siteDate="2026-11-20"
        firstDate="2026-11-01"
        onChange={vi.fn()}
      />,
    )
    expect(screen.getByRole('button', { name: 'Next range' })).toBeDisabled()
  })
})
