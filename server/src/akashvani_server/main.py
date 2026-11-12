from __future__ import annotations

import logging
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import RedirectResponse

from .api import admin, archive, commands, firmware, health, observations
from .config import Settings, get_settings
from .db import Base, create_engine_for, create_session_factory
from .logging import configure_logging
from .security import seed_devices
from .web import router as web_router

logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    settings: Settings = app.state.settings
    configure_logging(settings.log_level)
    settings.data_dir.mkdir(parents=True, exist_ok=True)
    settings.firmware_dir.mkdir(parents=True, exist_ok=True)

    if settings.environment == "test":
        Base.metadata.create_all(app.state.engine)
    seed_devices(app.state.engine, settings)

    logger.info(
        "starting %s environment=%s database=%s",
        settings.app_name,
        settings.environment,
        settings.database_url.split("@")[-1],
    )
    yield
    app.state.engine.dispose()
    logger.info("stopped")


def create_app(settings: Settings | None = None) -> FastAPI:
    settings = settings or get_settings()
    app = FastAPI(
        title=settings.app_name,
        version="0.1.0",
        description="Akashvani weather station ingest, command and OTA API",
        lifespan=lifespan,
    )
    app.state.settings = settings
    app.state.engine = create_engine_for(settings)
    app.state.session_factory = create_session_factory(app.state.engine)

    if settings.cors_origin_list:
        app.add_middleware(
            CORSMiddleware,
            allow_origins=settings.cors_origin_list,
            allow_methods=["GET", "POST"],
            allow_headers=["*"],
        )

    app.include_router(health.router)
    app.include_router(observations.router)
    app.include_router(archive.router)
    app.include_router(commands.router)
    app.include_router(firmware.router)
    app.include_router(admin.router)
    app.include_router(web_router)

    @app.get("/", include_in_schema=False)
    def root() -> RedirectResponse:
        return RedirectResponse("/docs")

    return app


app = create_app()
