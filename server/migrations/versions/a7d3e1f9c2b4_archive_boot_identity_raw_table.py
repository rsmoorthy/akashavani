"""archive boot identity and raw archive table

Revision ID: a7d3e1f9c2b4
Revises: b1f4c2d7a9e0
Create Date: 2026-11-21 10:00:00.000000

"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op


revision: str = "a7d3e1f9c2b4"
down_revision: str | None = "b1f4c2d7a9e0"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "raw_archive",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("device_id", sa.String(length=32), nullable=False),
        sa.Column("message_id", sa.String(length=64), nullable=False),
        sa.Column("date_time", sa.DateTime(timezone=True), nullable=False),
        sa.Column("received_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("source", sa.JSON(), nullable=False),
        sa.Column("raw", sa.JSON(), nullable=False),
        sa.ForeignKeyConstraint(["device_id"], ["devices.id"], ondelete="CASCADE"),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("device_id", "message_id", name="uq_raw_archive_device_message"),
    )
    op.create_index(op.f("ix_raw_archive_device_id"), "raw_archive", ["device_id"], unique=False)
    op.create_index(
        "ix_raw_archive_device_time",
        "raw_archive",
        ["device_id", "date_time"],
        unique=False,
    )

    # Preserve the envelopes already stored on archive rows before the column
    # is dropped.
    op.execute(
        """
        INSERT INTO raw_archive
            (device_id, message_id, date_time, received_at, source, raw)
        SELECT device_id, message_id, date_time, received_at, source, raw
        FROM archive
        """
    )

    with op.batch_alter_table("archive") as batch_op:
        batch_op.add_column(sa.Column("boot_id", sa.String(length=16), nullable=True))
        batch_op.add_column(sa.Column("uptime_ms", sa.BigInteger(), nullable=True))
        batch_op.drop_column("raw")

    op.create_index("ix_archive_device_boot", "archive", ["device_id", "boot_id"], unique=False)


def downgrade() -> None:
    op.drop_index("ix_archive_device_boot", table_name="archive")

    with op.batch_alter_table("archive") as batch_op:
        batch_op.add_column(sa.Column("raw", sa.JSON(), nullable=True))
        batch_op.drop_column("uptime_ms")
        batch_op.drop_column("boot_id")

    op.execute(
        """
        UPDATE archive
        SET raw = COALESCE(
            (
                SELECT raw_archive.raw
                FROM raw_archive
                WHERE raw_archive.device_id = archive.device_id
                  AND raw_archive.message_id = archive.message_id
            ),
            '{}'
        )
        """
    )

    with op.batch_alter_table("archive") as batch_op:
        batch_op.alter_column("raw", existing_type=sa.JSON(), nullable=False)

    op.drop_index("ix_raw_archive_device_time", table_name="raw_archive")
    op.drop_index(op.f("ix_raw_archive_device_id"), table_name="raw_archive")
    op.drop_table("raw_archive")
