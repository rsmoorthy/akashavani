"""device health fields

Revision ID: d5e9a1c3f720
Revises: c4d81f2a6b09
Create Date: 2026-11-12 15:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa


revision: str = "d5e9a1c3f720"
down_revision: str | None = "c4d81f2a6b09"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.add_column(sa.Column("uptime_seconds", sa.Integer(), nullable=True))
        batch_op.add_column(sa.Column("queue_depth", sa.Integer(), nullable=True))
        batch_op.add_column(sa.Column("free_heap", sa.Integer(), nullable=True))
        batch_op.add_column(sa.Column("reset_reason", sa.String(length=16), nullable=True))
        batch_op.add_column(sa.Column("wifi_rssi", sa.Integer(), nullable=True))


def downgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.drop_column("wifi_rssi")
        batch_op.drop_column("reset_reason")
        batch_op.drop_column("free_heap")
        batch_op.drop_column("queue_depth")
        batch_op.drop_column("uptime_seconds")
