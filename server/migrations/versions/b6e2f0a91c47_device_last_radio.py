"""device last radio timestamp

Revision ID: b6e2f0a91c47
Revises: 7d1c93a5e2b4
Create Date: 2026-11-12 11:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa


revision: str = "b6e2f0a91c47"
down_revision: str | None = "7d1c93a5e2b4"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.add_column(sa.Column("last_radio_at", sa.DateTime(timezone=True), nullable=True))


def downgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.drop_column("last_radio_at")
