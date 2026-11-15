"""device configuration mirror and site fields

Revision ID: f3d18c6b2a70
Revises: e2c7b5a41f90
Create Date: 2026-11-12 18:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "f3d18c6b2a70"
down_revision: str | None = "e2c7b5a41f90"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.add_column(sa.Column("config", sa.JSON(), nullable=True))
        batch_op.add_column(sa.Column("config_version", sa.Integer(), nullable=True))
        batch_op.add_column(
            sa.Column("config_updated_at", sa.DateTime(timezone=True), nullable=True)
        )
        batch_op.add_column(sa.Column("latitude", sa.Float(), nullable=True))
        batch_op.add_column(sa.Column("longitude", sa.Float(), nullable=True))
        batch_op.add_column(sa.Column("altitude_m", sa.Float(), nullable=True))


def downgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.drop_column("altitude_m")
        batch_op.drop_column("longitude")
        batch_op.drop_column("latitude")
        batch_op.drop_column("config_updated_at")
        batch_op.drop_column("config_version")
        batch_op.drop_column("config")
