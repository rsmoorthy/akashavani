"""per-device time zone

Revision ID: e2c7b5a41f90
Revises: d5e9a1c3f720
Create Date: 2026-11-12 15:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "e2c7b5a41f90"
down_revision: str | None = "d5e9a1c3f720"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    # Storage and transport stay UTC; the zone only drives operator-facing
    # rendering. Existing devices fall back to UTC until they report their own
    # setting from the AP configuration page.
    with op.batch_alter_table("devices") as batch_op:
        batch_op.add_column(
            sa.Column("timezone", sa.String(length=64), nullable=False, server_default="UTC")
        )


def downgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.drop_column("timezone")
