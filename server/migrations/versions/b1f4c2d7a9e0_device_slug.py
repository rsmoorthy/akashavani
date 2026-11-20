"""device public slug

Revision ID: b1f4c2d7a9e0
Revises: 9c4e7a2b5d10
Create Date: 2026-11-20 12:00:00.000000

"""

from __future__ import annotations

import re
import unicodedata
from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "b1f4c2d7a9e0"
down_revision: str | None = "9c4e7a2b5d10"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None

MAX_LENGTH = 64
RESERVED = {"assets", "favicon.ico", "favicon.svg", "static"}


def _slugify(value: str) -> str:
    text = unicodedata.normalize("NFKD", value or "")
    text = text.encode("ascii", "ignore").decode("ascii").lower()
    text = re.sub(r"[^a-z0-9]+", "-", text).strip("-")
    text = re.sub(r"-{2,}", "-", text)
    return text[:MAX_LENGTH].rstrip("-")


def _automatic_slug(name: str, device_id: str) -> str:
    base = _slugify(name) or _slugify(device_id) or "station"
    if base in RESERVED or len(base) < 2:
        base = f"{base}-station"
    return base[:MAX_LENGTH].rstrip("-")


def upgrade() -> None:
    op.add_column("devices", sa.Column("slug", sa.String(length=64), nullable=True))
    connection = op.get_bind()
    rows = (
        connection.execute(sa.text("SELECT id, name FROM devices ORDER BY created_at, id"))
        .mappings()
        .all()
    )
    used: set[str] = set()
    for row in rows:
        base = _automatic_slug(row["name"], row["id"])
        candidate = base
        suffix = 1
        while candidate in used:
            suffix += 1
            suffix_text = f"-{suffix}"
            candidate = base[: MAX_LENGTH - len(suffix_text)].rstrip("-") + suffix_text
        used.add(candidate)
        connection.execute(
            sa.text("UPDATE devices SET slug = :slug WHERE id = :id"),
            {"slug": candidate, "id": row["id"]},
        )

    with op.batch_alter_table("devices") as batch_op:
        batch_op.alter_column("slug", existing_type=sa.String(length=64), nullable=False)
    op.create_index("ix_devices_slug", "devices", ["slug"], unique=True)


def downgrade() -> None:
    op.drop_index("ix_devices_slug", table_name="devices")
    with op.batch_alter_table("devices") as batch_op:
        batch_op.drop_column("slug")
