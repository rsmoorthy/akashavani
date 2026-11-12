from __future__ import annotations

from pathlib import Path

from fastapi import APIRouter
from fastapi.responses import HTMLResponse

router = APIRouter(tags=["web"])
_UI_PATH = Path(__file__).with_name("admin_ui.html")


@router.get("/admin", response_class=HTMLResponse, include_in_schema=False)
def admin_ui() -> HTMLResponse:
    return HTMLResponse(_UI_PATH.read_text(encoding="utf-8"))
