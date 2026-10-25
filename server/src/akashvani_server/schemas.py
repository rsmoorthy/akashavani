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


class DeviceCreate(BaseModel):
    id: DeviceId
    name: str = Field(default="", max_length=80)
    token: str = Field(min_length=8, max_length=256)


class DeviceOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    name: str
    firmware_version: str | None
    created_at: UtcDateTime
    last_seen_at: UtcDateTime | None


class CommandType(str, Enum):
    REBOOT = "reboot"
    SYNC_TIME = "sync_time"
    SET_CONFIG = "set_config"
    CHECK_FOR_UPDATE = "check_for_update"
    UPLOAD_LOGS = "upload_logs"
    SELF_TEST = "self_test"


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
