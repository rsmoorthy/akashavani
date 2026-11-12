from __future__ import annotations

from datetime import UTC, datetime
from enum import Enum
from typing import Annotated, Any, Literal

from pydantic import (
    AwareDatetime,
    BaseModel,
    ConfigDict,
    Field,
    PlainSerializer,
    StringConstraints,
    model_validator,
)

Channel = Annotated[str, StringConstraints(pattern=r"^[a-z][a-z0-9-]{1,15}$")]


def _serialize_utc(value: datetime) -> str:
    if value.tzinfo is None:
        value = value.replace(tzinfo=UTC)
    return value.astimezone(UTC).isoformat().replace("+00:00", "Z")


# SQLite drops tzinfo, so response models force an explicit UTC representation.
UtcDateTime = Annotated[
    datetime, PlainSerializer(_serialize_utc, return_type=str, when_used="json")
]


DeviceId = Annotated[
    str,
    StringConstraints(pattern=r"^[a-z0-9][a-z0-9-]{2,31}$", min_length=3, max_length=32),
]
MessageId = Annotated[
    str,
    StringConstraints(
        pattern=r"^[A-Za-z0-9._:-]+$",
        min_length=8,
        max_length=64,
    ),
]
FirmwareVersion = Annotated[
    str,
    StringConstraints(
        pattern=r"^\d+\.\d+\.\d+(?:[-.][A-Za-z0-9.-]+)?$",
        min_length=5,
        max_length=32,
    ),
]


class ObservationSource(BaseModel):
    model_config = ConfigDict(extra="forbid")

    protocol: Literal["davis"] = "davis"
    channel: int | None = Field(default=None, ge=0, le=255)
    transmitter_id: int | None = Field(default=None, ge=1, le=8)
    rssi_dbm: int | None = Field(default=None, ge=-200, le=0)


class ObservationEnvelope(BaseModel):
    model_config = ConfigDict(extra="forbid")

    v: Literal[1]
    type: Literal["observation"] = "observation"
    device_id: DeviceId
    message_id: MessageId
    observed_at: AwareDatetime
    sequence: int | None = Field(default=None, ge=0)
    source: ObservationSource = Field(default_factory=ObservationSource)
    data: dict[str, Any] = Field(min_length=1)


class ObservationAccepted(BaseModel):
    accepted: bool = True
    duplicate: bool = False
    id: int


class ObservationOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    device_id: str
    message_id: str
    observed_at: UtcDateTime
    received_at: UtcDateTime
    sequence: int | None
    source: dict[str, Any]
    data: dict[str, Any]


class ArchiveSums(BaseModel):
    model_config = ConfigDict(extra="forbid")

    out_temp_c: float | None = None
    out_humidity_pct: float | None = None
    pressure_hpa: float | None = None
    in_temp_c: float | None = None
    in_humidity_pct: float | None = None
    wind_speed_m_s: float | None = None
    uv_index: float | None = None
    solar_wm2: float | None = None


class ArchiveCounts(BaseModel):
    model_config = ConfigDict(extra="forbid")

    out_temp: int = Field(default=0, ge=0)
    out_humidity: int = Field(default=0, ge=0)
    pressure: int = Field(default=0, ge=0)
    in_temp: int = Field(default=0, ge=0)
    in_humidity: int = Field(default=0, ge=0)
    wind_speed: int = Field(default=0, ge=0)
    wind_dir: int = Field(default=0, ge=0)
    wind_gust: int = Field(default=0, ge=0)
    rain: int = Field(default=0, ge=0)
    solar: int = Field(default=0, ge=0)
    uv: int = Field(default=0, ge=0)
    supercap: int = Field(default=0, ge=0)


class ArchiveWindDirection(BaseModel):
    model_config = ConfigDict(extra="forbid")

    sin_sum: float = 0.0
    cos_sum: float = 0.0
    speed_sum: float = 0.0


class ArchiveWindGust(BaseModel):
    model_config = ConfigDict(extra="forbid")

    max_m_s: float | None = None
    dir_deg: float | None = None


class ArchiveRain(BaseModel):
    model_config = ConfigDict(extra="forbid")

    tips: int = Field(default=0, ge=0)
    rate_raw: int | None = Field(default=None, ge=0, le=1023)
    rate_high: bool = False
    rate_valid: bool = False


class ArchiveLatest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    battery_low: bool = False
    supercap_v: float | None = None
    supercap_min_v: float | None = None


class ArchiveQuality(BaseModel):
    model_config = ConfigDict(extra="forbid")

    frames_expected: int = Field(default=0, ge=0)
    frames_valid: int = Field(default=0, ge=0)
    frames_crc_bad: int = Field(default=0, ge=0)
    resync_count: int = Field(default=0, ge=0)
    search_count: int = Field(default=0, ge=0)
    gap_slots: int = Field(default=0, ge=0)
    max_gap_slots: int = Field(default=0, ge=0)
    rssi_avg_dbm: float | None = None
    rssi_min_dbm: float | None = None
    rssi_max_dbm: float | None = None


class ArchiveEnvelope(BaseModel):
    model_config = ConfigDict(extra="forbid")

    v: Literal[1]
    type: Literal["archive"] = "archive"
    device_id: DeviceId
    message_id: MessageId
    bucket_end_epoch: int = Field(gt=0)
    interval_minutes: int = Field(default=5, ge=1, le=60)
    source: ObservationSource = Field(default_factory=ObservationSource)
    sums: ArchiveSums = Field(default_factory=ArchiveSums)
    counts: ArchiveCounts = Field(default_factory=ArchiveCounts)
    wind_dir: ArchiveWindDirection = Field(default_factory=ArchiveWindDirection)
    wind_gust: ArchiveWindGust = Field(default_factory=ArchiveWindGust)
    rain: ArchiveRain = Field(default_factory=ArchiveRain)
    latest: ArchiveLatest = Field(default_factory=ArchiveLatest)
    quality: ArchiveQuality = Field(default_factory=ArchiveQuality)


class ArchiveAccepted(BaseModel):
    accepted: bool = True
    duplicate: bool = False
    id: int
    date_time: UtcDateTime


class ArchiveBatchItemOut(BaseModel):
    message_id: str | None = None
    status: Literal["accepted", "duplicate", "rejected"]
    id: int | None = None
    date_time: UtcDateTime | None = None
    error: str | None = None


class ArchiveBatchAccepted(BaseModel):
    results: list[ArchiveBatchItemOut]
    accepted: int
    duplicates: int
    rejected: int


class ArchiveOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    device_id: str
    message_id: str
    date_time: UtcDateTime
    received_at: UtcDateTime
    interval_minutes: int
    units: str
    source: dict[str, Any]
    out_temp_c: float | None
    out_temp_count: int
    out_humidity_pct: float | None
    out_humidity_count: int
    pressure_hpa: float | None
    pressure_count: int
    barometer_hpa: float | None
    altimeter_hpa: float | None
    wind_speed_m_s: float | None
    wind_speed_count: int
    wind_dir_deg: float | None
    wind_dir_count: int
    wind_gust_m_s: float | None
    wind_gust_dir_deg: float | None
    wind_gust_count: int
    rain_tips: int | None
    rain_count: int
    rain_mm: float | None
    rain_in: float | None
    rain_rate_instant_mm_h: float | None
    rain_rate_instant_in_h: float | None
    rain_rate_15m_mm_h: float | None
    rain_rate_15m_in_h: float | None
    solar_wm2: float | None
    solar_count: int
    uv_index: float | None
    uv_count: int
    in_temp_c: float | None
    in_temp_count: int
    in_humidity_pct: float | None
    in_humidity_count: int
    in_dewpoint_c: float | None
    dewpoint_c: float | None
    windchill_c: float | None
    heatindex_c: float | None
    humidex_c: float | None
    app_temp_c: float | None
    windrun_km: float | None
    cloudbase_m: float | None
    max_solar_wm2: float | None
    et_mm: float | None
    battery_low: bool
    supercap_v: float | None
    supercap_min_v: float | None
    supercap_count: int
    rx_check_percent: float | None
    frames_expected: int
    frames_valid: int
    frames_crc_bad: int
    resync_count: int
    search_count: int
    gap_slots: int
    max_gap_slots: int
    rssi_avg_dbm: float | None
    rssi_min_dbm: float | None
    rssi_max_dbm: float | None
    quality_flags: dict[str, Any]
    derived_version: str


class DeviceCreate(BaseModel):
    id: DeviceId
    name: str = Field(default="", max_length=80)
    token: str = Field(min_length=8, max_length=256)


class DeviceOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    name: str
    mac_address: str | None = None
    firmware_version: str | None
    created_at: UtcDateTime
    last_seen_at: UtcDateTime | None
    last_radio_at: UtcDateTime | None = None


class DeviceAdminOut(DeviceOut):
    online: bool
    latest_archive_at: UtcDateTime | None = None
    frames_valid: int | None = None
    frames_expected: int | None = None
    frames_crc_bad: int | None = None
    rssi_avg_dbm: float | None = None


class CommandType(str, Enum):
    REBOOT = "reboot"
    SYNC_TIME = "sync_time"
    SET_CONFIG = "set_config"
    CHECK_FOR_UPDATE = "check_for_update"
    UPLOAD_LOGS = "upload_logs"
    SELF_TEST = "self_test"
    GET_FW_VERSION = "get_fw_version"
    DOWNLOAD_NEW_FW = "download_new_fw"
    BOOT_WITH_NEW_FW = "boot_with_new_fw"
    GET_PENDING_COMMANDS = "get_pending_commands"
    GET_DATA_AGAIN = "get_data_again"
    DOWNLOAD_RAW_DATA = "download_raw_data"


class CommandCreate(BaseModel):
    type: CommandType
    payload: dict[str, Any] = Field(default_factory=dict)
    expires_in_seconds: int | None = Field(default=None, ge=60, le=86_400)


class CommandOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    type: str
    payload: dict[str, Any]
    status: str
    result: dict[str, Any] | None = None
    error: str | None = None
    created_at: UtcDateTime
    delivered_at: UtcDateTime | None = None
    acked_at: UtcDateTime | None = None
    expires_at: UtcDateTime | None = None
    attempts: int = 0
    last_attempt_at: UtcDateTime | None = None


class CommandAck(BaseModel):
    status: Literal["ok", "error"]
    result: dict[str, Any] | None = None
    error: str | None = Field(default=None, max_length=500)

    @model_validator(mode="after")
    def require_error_for_failure(self) -> CommandAck:
        if self.status == "error" and not self.error:
            raise ValueError("error is required when status is 'error'")
        return self


class FirmwareRegister(BaseModel):
    version: FirmwareVersion
    channel: Channel = "stable"
    filename: str = Field(min_length=1, max_length=160)
    notes: str | None = Field(default=None, max_length=2000)
    mandatory: bool = False


class FirmwareReleaseOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    version: str
    channel: str
    filename: str
    sha256: str
    size_bytes: int
    notes: str | None
    mandatory: bool
    created_at: UtcDateTime


class FirmwareCheckResponse(BaseModel):
    update_available: bool
    version: str
    download_url: str | None = None
    sha256: str | None = None
    size_bytes: int | None = None
    mandatory: bool = False
