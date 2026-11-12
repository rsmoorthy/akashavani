"""device station id and mac address

Revision ID: 4f2a7c91b3d0
Revises: 9b208c04ef24
Create Date: 2026-10-26 18:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "4f2a7c91b3d0"
down_revision: str | None = "9b208c04ef24"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.add_column("devices", sa.Column("station_id", sa.Integer(), nullable=True))
    op.add_column("devices", sa.Column("mac_address", sa.String(length=17), nullable=True))


def downgrade() -> None:
    op.drop_column("devices", "mac_address")
    op.drop_column("devices", "station_id")
