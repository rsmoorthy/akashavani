from __future__ import annotations

from pathlib import Path

from fastapi import APIRouter, HTTPException, Request, status
from fastapi.responses import FileResponse, HTMLResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles
from starlette.responses import Response
from starlette.types import Scope

router = APIRouter(tags=["web"])
_UI_PATH = Path(__file__).with_name("admin_ui.html")


class DashboardStaticFiles(StaticFiles):
    """StaticFiles with long-lived caching for Vite's hashed assets."""

    async def get_response(self, path: str, scope: Scope) -> Response:
        response = await super().get_response(path, scope)
        if response.status_code == 200:
            response.headers["Cache-Control"] = "public, max-age=31536000, immutable"
        return response


def _message(title: str, body: str, status_code: int = status.HTTP_200_OK) -> HTMLResponse:
    return HTMLResponse(
        f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<style>
body{{font:16px/1.5 system-ui,sans-serif;max-width:42rem;margin:4rem auto;
padding:0 1rem;color:#24313a}}
code{{background:#eef2f4;padding:.15rem .35rem;border-radius:4px}}
a{{color:#0b7285}}
</style>
</head><body><h1>{title}</h1>{body}</body></html>""",
        status_code=status_code,
    )


def _index(request: Request) -> HTMLResponse:
    settings = request.app.state.settings
    if not settings.public_dashboard_enabled:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="not found")
    path = settings.dashboard_dir / "index.html"
    if not path.is_file():
        return _message(
            "Dashboard is not built",
            "<p>Build the frontend with <code>make dashboard-build</code>, then restart the "
            "server. The expected file is <code>"
            f"{path}</code>.</p>",
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
        )
    return HTMLResponse(
        path.read_text(encoding="utf-8"),
        headers={"Cache-Control": "no-cache"},
    )


@router.get("/admin", response_class=HTMLResponse, include_in_schema=False)
def admin_ui() -> HTMLResponse:
    return HTMLResponse(_UI_PATH.read_text(encoding="utf-8"))


@router.get("/dashboard", include_in_schema=False, response_model=None)
def dashboard_root(request: Request) -> HTMLResponse | RedirectResponse:
    settings = request.app.state.settings
    if not settings.public_dashboard_enabled:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="not found")
    if not settings.public_default_slug:
        return _message(
            "No default station",
            "<p>Set <code>AKASHAVANI_PUBLIC_DEFAULT_SLUG</code> to a device slug, or open "
            "<code>/dashboard/&lt;slug&gt;/</code> directly.</p>",
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
        )
    return RedirectResponse(f"/dashboard/{settings.public_default_slug}/", status_code=307)


@router.get("/dashboard/favicon.svg", include_in_schema=False, response_model=None)
def dashboard_favicon(request: Request) -> FileResponse:
    settings = request.app.state.settings
    if not settings.public_dashboard_enabled:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="not found")
    path = settings.dashboard_dir / "favicon.svg"
    if not path.is_file():
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="not found")
    return FileResponse(
        path,
        media_type="image/svg+xml",
        headers={"Cache-Control": "public, max-age=86400"},
    )


@router.get("/dashboard/{slug}", include_in_schema=False, response_model=None)
@router.get("/dashboard/{slug}/", include_in_schema=False, response_model=None)
def dashboard_page(slug: str, request: Request) -> HTMLResponse:
    del slug
    return _index(request)
