"""device live radio metrics

Revision ID: e7b1c4d9a2f6
Revises: a7d3e1f9c2b4
Create Date: 2026-11-21 17:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa


revision: str = "e7b1c4d9a2f6"
down_revision: str | None = "a7d3e1f9c2b4"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.add_column(sa.Column("radio_frames_valid", sa.Integer(), nullable=True))
        batch_op.add_column(sa.Column("radio_rssi_dbm", sa.Integer(), nullable=True))


def downgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.drop_column("radio_rssi_dbm")
        batch_op.drop_column("radio_frames_valid")
