"""archive invalid-frame RSSI statistics

Revision ID: 9c4e7a2b5d10
Revises: f3d18c6b2a70
Create Date: 2026-11-13 10:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "9c4e7a2b5d10"
down_revision: str | None = "f3d18c6b2a70"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    with op.batch_alter_table("archive") as batch_op:
        batch_op.add_column(sa.Column("crc_bad_rssi_min_dbm", sa.Float(), nullable=True))
        batch_op.add_column(sa.Column("crc_bad_rssi_avg_dbm", sa.Float(), nullable=True))
        batch_op.add_column(sa.Column("crc_bad_rssi_max_dbm", sa.Float(), nullable=True))


def downgrade() -> None:
    with op.batch_alter_table("archive") as batch_op:
        batch_op.drop_column("crc_bad_rssi_max_dbm")
        batch_op.drop_column("crc_bad_rssi_avg_dbm")
        batch_op.drop_column("crc_bad_rssi_min_dbm")
