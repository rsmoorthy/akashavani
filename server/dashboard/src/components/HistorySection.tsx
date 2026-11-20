import { useState } from 'react'

import { exportUrl } from '../api'
import { historyQuery } from '../range'
import type { RangeSelection } from '../range'
import type { History, ViewMode } from '../types'
import { Charts } from '../charts/Charts'
import { HistoryTable } from './HistoryTable'
import { RangeControls } from './RangeControls'
import { SummaryTable } from './SummaryTable'

interface HistorySectionProps {
  slug: string
  selection: RangeSelection
  siteDate: string
  firstDate: string | null
  history: History | null
  error: string | null
  onChange: (selection: RangeSelection) => void
  onRetry: () => void
}

export function HistorySection({
  slug,
  selection,
  siteDate,
  firstDate,
  history,
  error,
  onChange,
  onRetry,
}: HistorySectionProps) {
  const [view, setView] = useState<ViewMode>('graph')
  const download = exportUrl(slug, historyQuery(selection, siteDate))

  return (
    <div className="history-section">
      <RangeControls
        selection={selection}
        siteDate={siteDate}
        firstDate={firstDate}
        onChange={onChange}
      />

      {history ? (
        <>
          <SummaryTable history={history} />
          <section className="history panel" aria-label="Graphs and table">
            <div className="history-toolbar">
              <div className="view-toggle" role="group" aria-label="Display mode">
                <button
                  type="button"
                  className={`segment-button ${view === 'graph' ? 'is-active' : ''}`}
                  aria-pressed={view === 'graph'}
                  onClick={() => setView('graph')}
                >
                  Graph
                </button>
                <button
                  type="button"
                  className={`segment-button ${view === 'table' ? 'is-active' : ''}`}
                  aria-pressed={view === 'table'}
                  onClick={() => setView('table')}
                >
                  Table
                </button>
              </div>
              <a className="button primary download-button" href={download} download>
                <svg viewBox="0 0 24 24" aria-hidden="true">
                  <path d="M12 3v12m0 0 5-5m-5 5-5-5M4 20h16" />
                </svg>
                Download Data
              </a>
            </div>
            {history.sample_count === 0 ? (
              <p className="empty-state">No archive data for this period.</p>
            ) : view === 'graph' ? (
              <Charts history={history} />
            ) : (
              <HistoryTable history={history} />
            )}
          </section>
        </>
      ) : (
        <section className="history panel" aria-label="Loading history">
          {error ? (
            <div className="history-error">
              <p>{error}</p>
              <button type="button" className="button primary" onClick={onRetry}>
                Retry
              </button>
            </div>
          ) : (
            <div className="history-loading">
              <div className="skeleton skeleton-line" />
              <div className="skeleton skeleton-chart" />
              <p className="muted">Loading archive data…</p>
            </div>
          )}
        </section>
      )}
    </div>
  )
}
