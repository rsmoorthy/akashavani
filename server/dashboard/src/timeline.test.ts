import { describe, expect, it } from 'vitest'

import {
  cumulativeRain,
  formatMinute,
  hasData,
  latestDataIndex,
  minuteOfDay,
  nearestDataIndex,
} from './timeline'
import { pointFixture } from './test/fixtures'

const points = [
  pointFixture('2026-11-20T00:00:00Z', { out_temp_c: 20, rain_mm: 0.5 }),
  pointFixture('2026-11-20T00:05:00Z', { out_temp_c: 21, rain_mm: 0.25 }),
  pointFixture('2026-11-20T00:10:00Z', { out_temp_c: 22, rain_mm: null }),
  pointFixture('2026-11-20T00:15:00Z'),
]

describe('timeline helpers', () => {
  it('finds the latest point that has data', () => {
    expect(latestDataIndex(points)).toBe(2)
    expect(hasData(points[3])).toBe(false)
  })

  it('finds the nearest data point to a minute of day', () => {
    expect(minuteOfDay('2026-11-20T00:00:00Z', 'Asia/Calcutta')).toBe(330)
    expect(nearestDataIndex(points, 334, 'Asia/Calcutta')).toBe(1)
    expect(nearestDataIndex(points, 339, 'Asia/Calcutta')).toBe(2)
  })

  it('accumulates rain to the selected point', () => {
    expect(cumulativeRain(points, 0)).toBe(0.5)
    expect(cumulativeRain(points, 1)).toBe(0.75)
    expect(cumulativeRain([pointFixture('2026-11-20T00:00:00Z')], 0)).toBeNull()
  })

  it('formats slider minutes', () => {
    expect(formatMinute(0)).toBe('12:00 AM')
    expect(formatMinute(735)).toBe('12:15 PM')
  })
})
