export const COLORS = {
  temp: '#d1495b',
  dewpoint: '#2a9d8f',
  wind: '#3d6f9e',
  gust: '#e07a3f',
  direction: '#7b5ea7',
  rain: '#277da1',
  accumulation: '#43aa8b',
  pressure: '#6d597a',
  solar: '#e9a23b',
  uv: '#9b5de5',
  grid: 'rgba(107, 128, 139, 0.18)',
  text: '#61747f',
  band: 'rgba(107, 128, 139, 0.07)',
}

export const CHART_FONT =
  'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'

export function bandSizeForBucket(bucketSeconds: number): number {
  if (bucketSeconds <= 300) return 144
  if (bucketSeconds <= 3600) return 24
  return 1
}
