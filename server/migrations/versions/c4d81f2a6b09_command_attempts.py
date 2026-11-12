"""command attempts and ack timeout support

Revision ID: c4d81f2a6b09
Revises: b6e2f0a91c47
Create Date: 2026-11-12 13:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa


revision: str = "c4d81f2a6b09"
down_revision: str | None = "b6e2f0a91c47"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    with op.batch_alter_table("commands") as batch_op:
        batch_op.add_column(sa.Column("attempts", sa.Integer(), nullable=False, server_default="0"))
        batch_op.add_column(sa.Column("last_attempt_at", sa.DateTime(timezone=True), nullable=True))


def downgrade() -> None:
    with op.batch_alter_table("commands") as batch_op:
        batch_op.drop_column("last_attempt_at")
        batch_op.drop_column("attempts")
