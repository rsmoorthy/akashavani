import type { ReactNode } from 'react'

interface WidgetCardProps {
  title: string
  accent: string
  ariaLabel?: string
  children: ReactNode
}

export function WidgetCard({ title, accent, ariaLabel, children }: WidgetCardProps) {
  return (
    <article className={`widget-card widget-${accent}`} aria-label={ariaLabel ?? title}>
      <h2 className="widget-title">{title}</h2>
      {children}
    </article>
  )
}
