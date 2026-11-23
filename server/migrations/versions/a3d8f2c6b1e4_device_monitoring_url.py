"""device monitoring url

Revision ID: a3d8f2c6b1e4
Revises: e7b1c4d9a2f6
Create Date: 2026-11-23 10:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa


revision: str = "a3d8f2c6b1e4"
down_revision: str | None = "e7b1c4d9a2f6"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.add_column(sa.Column("monitoring_url", sa.String(length=500), nullable=True))


def downgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.drop_column("monitoring_url")
