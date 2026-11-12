"""drop device station id

Revision ID: 7d1c93a5e2b4
Revises: cb83667013d6
Create Date: 2026-11-12 09:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa


revision: str = "7d1c93a5e2b4"
down_revision: str | None = "cb83667013d6"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    # Identity is the device token plus the MAC-derived `ak-xxxxxx`; a station
    # number is redundant and made the shared-token fallback ambiguous.
    with op.batch_alter_table("devices") as batch_op:
        batch_op.drop_column("station_id")


def downgrade() -> None:
    with op.batch_alter_table("devices") as batch_op:
        batch_op.add_column(sa.Column("station_id", sa.Integer(), nullable=True))
