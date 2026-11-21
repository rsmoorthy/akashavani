from __future__ import annotations

from datetime import UTC, datetime
from typing import Any

from sqlalchemy import (
    JSON,
    BigInteger,
    Boolean,
    DateTime,
    Float,
    ForeignKey,
    Index,
    Integer,
    String,
    Text,
    UniqueConstraint,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship

from .db import Base


def utcnow() -> datetime:
    return datetime.now(UTC)


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    name: Mapped[str] = mapped_column(String(80), default="", nullable=False)
    # Stable, URL-safe identity used by the public dashboard. Renaming the
    # display name does not change it; operators change it explicitly.
    slug: Mapped[str] = mapped_column(String(64), unique=True, index=True, nullable=False)
    # IANA zone used only for operator-facing rendering; storage is always UTC.
    timezone: Mapped[str] = mapped_column(String(64), default="UTC", nullable=False)
    # Device-reported site configuration. The JSON is the complete document;
    # the scalar columns are extracted for archive derivations and admin views.
    config: Mapped[dict[str, Any] | None] = mapped_column(JSON, nullable=True)
    config_version: Mapped[int | None] = mapped_column(Integer, nullable=True)
    config_updated_at: Mapped[datetime | None] = mapped_column(
        DateTime(timezone=True), nullable=True
    )
    latitude: Mapped[float | None] = mapped_column(Float, nullable=True)
    longitude: Mapped[float | None] = mapped_column(Float, nullable=True)
    altitude_m: Mapped[float | None] = mapped_column(Float, nullable=True)
    token_hash: Mapped[str] = mapped_column(String(64), unique=True, index=True, nullable=False)
    mac_address: Mapped[str | None] = mapped_column(String(17), nullable=True)
    firmware_version: Mapped[str | None] = mapped_column(String(32), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow)
    last_seen_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    last_radio_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    uptime_seconds: Mapped[int | None] = mapped_column(Integer, nullable=True)
    queue_depth: Mapped[int | None] = mapped_column(Integer, nullable=True)
    free_heap: Mapped[int | None] = mapped_column(Integer, nullable=True)
    reset_reason: Mapped[str | None] = mapped_column(String(16), nullable=True)
    wifi_rssi: Mapped[int | None] = mapped_column(Integer, nullable=True)

    observations: Mapped[list[Observation]] = relationship(back_populates="device")
    archive_records: Mapped[list[ArchiveRecord]] = relationship(back_populates="device")
    commands: Mapped[list[Command]] = relationship(back_populates="device")


class Observation(Base):
    __tablename__ = "observations"
    __table_args__ = (
        UniqueConstraint("device_id", "message_id", name="uq_observation_device_message"),
        Index("ix_observation_device_observed", "device_id", "observed_at"),
    )

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(
        ForeignKey("devices.id", ondelete="CASCADE"), nullable=False, index=True
    )
    message_id: Mapped[str] = mapped_column(String(64), nullable=False)
    observed_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utcnow, nullable=False
    )
    sequence: Mapped[int | None] = mapped_column(BigInteger, nullable=True)
    source: Mapped[dict[str, Any]] = mapped_column(JSON, default=dict, nullable=False)
    data: Mapped[dict[str, Any]] = mapped_column(JSON, default=dict, nullable=False)

    device: Mapped[Device] = relationship(back_populates="observations")


class ArchiveRecord(Base):
    """One 5-minute SI archive record.

    The receiver sends sums and counts; the server stores means plus the sample
    counts. Rain is stored in tips, millimetres and inches, and both the raw
    Davis 0x5 rate and the Weewx-style 15-minute trailing rate are kept.
    """

    __tablename__ = "archive"
    __table_args__ = (
        UniqueConstraint("device_id", "message_id", name="uq_archive_device_message"),
        UniqueConstraint("device_id", "date_time", name="uq_archive_device_time"),
        Index("ix_archive_device_time", "device_id", "date_time"),
        Index("ix_archive_device_boot", "device_id", "boot_id"),
    )

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(
        ForeignKey("devices.id", ondelete="CASCADE"), nullable=False, index=True
    )
    message_id: Mapped[str] = mapped_column(String(64), nullable=False)
    date_time: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utcnow, nullable=False
    )
    interval_minutes: Mapped[int] = mapped_column(Integer, default=5, nullable=False)
    boot_id: Mapped[str | None] = mapped_column(String(16), nullable=True)
    uptime_ms: Mapped[int | None] = mapped_column(BigInteger, nullable=True)
    units: Mapped[str] = mapped_column(String(8), default="SI", nullable=False)
    source: Mapped[dict[str, Any]] = mapped_column(JSON, default=dict, nullable=False)

    out_temp_c: Mapped[float | None] = mapped_column(Float, nullable=True)
    out_temp_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    out_humidity_pct: Mapped[float | None] = mapped_column(Float, nullable=True)
    out_humidity_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    pressure_hpa: Mapped[float | None] = mapped_column(Float, nullable=True)
    pressure_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    barometer_hpa: Mapped[float | None] = mapped_column(Float, nullable=True)
    altimeter_hpa: Mapped[float | None] = mapped_column(Float, nullable=True)

    wind_speed_m_s: Mapped[float | None] = mapped_column(Float, nullable=True)
    wind_speed_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    wind_dir_deg: Mapped[float | None] = mapped_column(Float, nullable=True)
    wind_dir_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    wind_gust_m_s: Mapped[float | None] = mapped_column(Float, nullable=True)
    wind_gust_dir_deg: Mapped[float | None] = mapped_column(Float, nullable=True)
    wind_gust_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)

    rain_tips: Mapped[int | None] = mapped_column(Integer, nullable=True)
    rain_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    rain_mm: Mapped[float | None] = mapped_column(Float, nullable=True)
    rain_in: Mapped[float | None] = mapped_column(Float, nullable=True)
    rain_rate_instant_mm_h: Mapped[float | None] = mapped_column(Float, nullable=True)
    rain_rate_instant_in_h: Mapped[float | None] = mapped_column(Float, nullable=True)
    rain_rate_15m_mm_h: Mapped[float | None] = mapped_column(Float, nullable=True)
    rain_rate_15m_in_h: Mapped[float | None] = mapped_column(Float, nullable=True)

    solar_wm2: Mapped[float | None] = mapped_column(Float, nullable=True)
    solar_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    uv_index: Mapped[float | None] = mapped_column(Float, nullable=True)
    uv_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)

    in_temp_c: Mapped[float | None] = mapped_column(Float, nullable=True)
    in_temp_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    in_humidity_pct: Mapped[float | None] = mapped_column(Float, nullable=True)
    in_humidity_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    in_dewpoint_c: Mapped[float | None] = mapped_column(Float, nullable=True)

    dewpoint_c: Mapped[float | None] = mapped_column(Float, nullable=True)
    windchill_c: Mapped[float | None] = mapped_column(Float, nullable=True)
    heatindex_c: Mapped[float | None] = mapped_column(Float, nullable=True)
    humidex_c: Mapped[float | None] = mapped_column(Float, nullable=True)
    app_temp_c: Mapped[float | None] = mapped_column(Float, nullable=True)
    windrun_km: Mapped[float | None] = mapped_column(Float, nullable=True)
    cloudbase_m: Mapped[float | None] = mapped_column(Float, nullable=True)
    max_solar_wm2: Mapped[float | None] = mapped_column(Float, nullable=True)
    et_mm: Mapped[float | None] = mapped_column(Float, nullable=True)

    battery_low: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    supercap_v: Mapped[float | None] = mapped_column(Float, nullable=True)
    supercap_min_v: Mapped[float | None] = mapped_column(Float, nullable=True)
    supercap_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)

    rx_check_percent: Mapped[float | None] = mapped_column(Float, nullable=True)
    frames_expected: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    frames_valid: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    frames_crc_bad: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    crc_bad_rssi_min_dbm: Mapped[float | None] = mapped_column(Float, nullable=True)
    crc_bad_rssi_avg_dbm: Mapped[float | None] = mapped_column(Float, nullable=True)
    crc_bad_rssi_max_dbm: Mapped[float | None] = mapped_column(Float, nullable=True)
    resync_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    search_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    gap_slots: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    max_gap_slots: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    rssi_avg_dbm: Mapped[float | None] = mapped_column(Float, nullable=True)
    rssi_min_dbm: Mapped[float | None] = mapped_column(Float, nullable=True)
    rssi_max_dbm: Mapped[float | None] = mapped_column(Float, nullable=True)

    quality_flags: Mapped[dict[str, Any]] = mapped_column(JSON, default=dict, nullable=False)
    derived_version: Mapped[str] = mapped_column(String(16), default="1", nullable=False)

    device: Mapped[Device] = relationship(back_populates="archive_records")


class RawArchiveRecord(Base):
    """Original archive envelope, kept separately from the derived row.

    The derived ``archive`` row is queried constantly; the full uploaded
    document is only needed for audit and re-derivation, so it lives here and
    is joined by ``device_id``/``message_id`` when required.
    """

    __tablename__ = "raw_archive"
    __table_args__ = (
        UniqueConstraint("device_id", "message_id", name="uq_raw_archive_device_message"),
        Index("ix_raw_archive_device_time", "device_id", "date_time"),
    )

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(
        ForeignKey("devices.id", ondelete="CASCADE"), nullable=False, index=True
    )
    message_id: Mapped[str] = mapped_column(String(64), nullable=False)
    date_time: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utcnow, nullable=False
    )
    source: Mapped[dict[str, Any]] = mapped_column(JSON, default=dict, nullable=False)
    raw: Mapped[dict[str, Any]] = mapped_column(JSON, default=dict, nullable=False)


class Command(Base):
    __tablename__ = "commands"
    __table_args__ = (Index("ix_command_device_status", "device_id", "status"),)

    id: Mapped[str] = mapped_column(String(36), primary_key=True)
    device_id: Mapped[str] = mapped_column(
        ForeignKey("devices.id", ondelete="CASCADE"), nullable=False, index=True
    )
    type: Mapped[str] = mapped_column(String(32), nullable=False)
    payload: Mapped[dict[str, Any]] = mapped_column(JSON, default=dict, nullable=False)
    status: Mapped[str] = mapped_column(String(16), default="pending", nullable=False)
    result: Mapped[dict[str, Any] | None] = mapped_column(JSON, nullable=True)
    error: Mapped[str | None] = mapped_column(Text, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utcnow, nullable=False
    )
    delivered_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    acked_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    expires_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    attempts: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    last_attempt_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    device: Mapped[Device] = relationship(back_populates="commands")


class FirmwareRelease(Base):
    __tablename__ = "firmware_releases"

    version: Mapped[str] = mapped_column(String(32), primary_key=True)
    channel: Mapped[str] = mapped_column(String(16), default="stable", index=True)
    filename: Mapped[str] = mapped_column(String(160), nullable=False)
    sha256: Mapped[str] = mapped_column(String(64), nullable=False)
    size_bytes: Mapped[int] = mapped_column(BigInteger, nullable=False)
    notes: Mapped[str | None] = mapped_column(Text, nullable=True)
    mandatory: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utcnow, nullable=False
    )
