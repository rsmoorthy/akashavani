# Akashavani dashboard

React + TypeScript + Vite public weather dashboard. The production bundle is
served by FastAPI at `/dashboard/<slug>/`.

From the repository root:

```bash
make dashboard-setup
make dashboard-dev       # Vite on :5173, proxies /api to :8000
make dashboard-test
make dashboard-lint
make dashboard-build
```

The backend must be running with
`AKASHAVANI_PUBLIC_DASHBOARD_ENABLED=true`; see
[the dashboard build guide](../../docs/build/dashboard.md).

The page uses only `/api/v1/public`. It reads no device credentials and has no
write controls.
