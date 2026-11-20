import { useId } from 'react'
import type { CSSProperties } from 'react'

function clamp(value: number, minimum: number, maximum: number): number {
  return Math.min(maximum, Math.max(minimum, value))
}

function arcPoint(value: number, minimum: number, maximum: number) {
  const ratio = clamp((value - minimum) / (maximum - minimum), 0, 1)
  const angle = Math.PI - ratio * Math.PI
  return {
    x: 100 + 78 * Math.cos(angle),
    y: 90 - 78 * Math.sin(angle),
  }
}

export function TemperatureGauge({ value }: { value: number | null }) {
  const rawId = useId()
  const gradientId = `temp-${rawId.replace(/[^a-zA-Z0-9_-]/g, '')}`
  const marker = value === null ? null : arcPoint(value, -10, 50)
  return (
    <svg className="gauge gauge-temperature" viewBox="0 0 200 106" aria-hidden="true">
      <defs>
        <linearGradient id={gradientId} x1="0" y1="0" x2="1" y2="0">
          <stop offset="0%" stopColor="#8155a6" />
          <stop offset="25%" stopColor="#3d7fb8" />
          <stop offset="50%" stopColor="#5aa06b" />
          <stop offset="75%" stopColor="#efa53f" />
          <stop offset="100%" stopColor="#c94545" />
        </linearGradient>
      </defs>
      <path
        d="M 22 90 A 78 78 0 0 1 178 90"
        fill="none"
        stroke="var(--track)"
        strokeWidth="12"
        strokeLinecap="round"
      />
      <path
        d="M 22 90 A 78 78 0 0 1 178 90"
        fill="none"
        stroke={`url(#${gradientId})`}
        strokeWidth="12"
        strokeLinecap="round"
        opacity="0.9"
      />
      {marker ? (
        <circle
          cx={marker.x}
          cy={marker.y}
          r="6.5"
          fill="var(--panel)"
          stroke="var(--text)"
          strokeWidth="2.5"
        />
      ) : null}
    </svg>
  )
}

export function HumidityRing({ value }: { value: number | null }) {
  const ratio = value === null ? 0 : clamp(value, 0, 100)
  const style = {
    '--humidity-fill': `${ratio * 3.6}deg`,
  } as CSSProperties
  return (
    <div className="humidity-ring" style={style} aria-hidden="true">
      <div className="humidity-ring-inner" />
    </div>
  )
}

export function WindCompass({
  direction,
  speed,
}: {
  direction: number | null
  speed: number | null
}) {
  const active = direction !== null
  return (
    <div className={`compass ${active && (speed ?? 0) > 0 ? 'compass-active' : ''}`} aria-hidden="true">
      <span className="compass-letter compass-n">N</span>
      <span className="compass-letter compass-e">E</span>
      <span className="compass-letter compass-s">S</span>
      <span className="compass-letter compass-w">W</span>
      <span className="compass-line compass-line-n" />
      <span className="compass-line compass-line-e" />
      <span className="compass-line compass-line-s" />
      <span className="compass-line compass-line-w" />
      <svg
        className="compass-arrow"
        viewBox="0 0 24 24"
        style={{ transform: `rotate(${direction ?? 0}deg)` }}
      >
        <path d="M12 3 18 21 12 17.5 6 21Z" />
      </svg>
      <span className="compass-hub" />
    </div>
  )
}

export function PressureDial({ value }: { value: number | null }) {
  const ratio = value === null ? null : clamp((value - 850) / 200, 0, 1)
  const angle = ratio === null ? null : Math.PI - ratio * Math.PI
  const end =
    angle === null ? null : { x: 100 + 62 * Math.cos(angle), y: 90 - 62 * Math.sin(angle) }
  return (
    <svg className="gauge gauge-pressure" viewBox="0 0 200 106" aria-hidden="true">
      <path
        d="M 22 90 A 78 78 0 0 1 178 90"
        fill="none"
        stroke="var(--track)"
        strokeWidth="10"
        strokeLinecap="round"
      />
      <path
        d="M 34 90 A 66 66 0 0 1 166 90"
        fill="none"
        stroke="var(--pressure-accent)"
        strokeWidth="2"
        strokeDasharray="1 8"
        opacity="0.55"
      />
      <text x="20" y="104" className="gauge-label">
        850
      </text>
      <text x="158" y="104" className="gauge-label">
        1050
      </text>
      {end ? (
        <>
          <line
            x1="100"
            y1="90"
            x2={end.x}
            y2={end.y}
            stroke="var(--text)"
            strokeWidth="3"
            strokeLinecap="round"
          />
          <circle cx="100" cy="90" r="5" fill="var(--text)" />
        </>
      ) : null}
    </svg>
  )
}

export function UvScale({ value }: { value: number | null }) {
  const ratio = value === null ? 0 : clamp(value / 12, 0, 1)
  return (
    <div className="uv-scale" aria-hidden="true">
      <div className="uv-segments">
        <span className="uv-segment uv-low" />
        <span className="uv-segment uv-moderate" />
        <span className="uv-segment uv-high" />
        <span className="uv-segment uv-very-high" />
        <span className="uv-segment uv-extreme" />
      </div>
      <span className="uv-marker" style={{ left: `${ratio * 100}%` }} />
    </div>
  )
}

export function SolarRing({ value }: { value: number | null }) {
  const ratio = value === null ? 0 : clamp(value / 1000, 0, 1)
  const circumference = 2 * Math.PI * 28
  return (
    <svg className="solar-ring" viewBox="0 0 72 72" aria-hidden="true">
      <circle cx="36" cy="36" r="28" fill="none" stroke="var(--track)" strokeWidth="7" />
      <circle
        cx="36"
        cy="36"
        r="28"
        fill="none"
        stroke="var(--solar-accent)"
        strokeWidth="7"
        strokeLinecap="round"
        strokeDasharray={`${circumference * ratio} ${circumference}`}
        transform="rotate(-90 36 36)"
      />
      <circle cx="36" cy="36" r="9" fill="var(--solar-accent)" opacity="0.35" />
    </svg>
  )
}

export function RainDroplet({ value }: { value: number | null }) {
  const fill = value === null ? 0 : clamp(value / 10, 0, 1)
  return (
    <div className="rain-visual" aria-hidden="true">
      <svg viewBox="0 0 60 72" className="rain-drop">
        <path d="M30 5C24 18 12 28 12 44a18 18 0 0 0 36 0C48 28 36 18 30 5Z" />
      </svg>
      <span className="rain-level" style={{ height: `${12 + fill * 34}px` }} />
    </div>
  )
}
