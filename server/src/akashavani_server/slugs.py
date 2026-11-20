from __future__ import annotations

import re
import unicodedata

from sqlalchemy import select
from sqlalchemy.orm import Session

SLUG_MAX_LENGTH = 64
# Paths that already exist below /dashboard and must never be used as a
# station slug. Keep this list small and deployment-specific.
RESERVED_SLUGS = frozenset({"assets", "favicon.ico", "favicon.svg", "static"})
_SLUG_RE = re.compile(r"^[a-z0-9](?:[a-z0-9-]{0,62}[a-z0-9])?$")


def slugify(value: str, *, fallback: str | None = "station") -> str:
    """Return a URL-safe slug candidate.

    The result is not checked for uniqueness or reserved names; callers use
    :func:`unique_slug` and :func:`normalise_slug` for those. Pass
    ``fallback=None`` to return an empty string when ``value`` has no usable
    characters.
    """
    text = unicodedata.normalize("NFKD", value)
    text = text.encode("ascii", "ignore").decode("ascii").lower()
    text = re.sub(r"[^a-z0-9]+", "-", text).strip("-")
    text = re.sub(r"-{2,}", "-", text)
    if len(text) > SLUG_MAX_LENGTH:
        text = text[:SLUG_MAX_LENGTH].rstrip("-")
    if text:
        return text
    if fallback is None:
        return ""
    fallback_text = re.sub(r"[^a-z0-9]+", "-", fallback.lower()).strip("-")
    return (fallback_text or "station")[:SLUG_MAX_LENGTH].rstrip("-") or "station"


def normalise_slug(value: str) -> str:
    """Validate and normalise an operator-supplied slug.

    Uppercase letters, spaces and punctuation are normalised rather than
    rejected so the admin form is forgiving. Reserved path names are rejected.
    """
    slug = slugify(value, fallback=None)
    if not slug:
        raise ValueError("slug must contain at least one letter or digit")
    if len(slug) < 2:
        raise ValueError("slug must be at least 2 characters")
    if slug in RESERVED_SLUGS:
        raise ValueError(f"slug '{slug}' is reserved")
    if not _SLUG_RE.fullmatch(slug):
        raise ValueError("slug must be 2-64 characters of a-z, 0-9 and '-'")
    return slug


def automatic_slug(name: str, device_id: str) -> str:
    """Derive a slug for a device that was not given one explicitly."""
    base = slugify(name or device_id, fallback=device_id)
    if base in RESERVED_SLUGS or len(base) < 2:
        base = f"{base or 'station'}-station"
    return base[:SLUG_MAX_LENGTH].rstrip("-")


def unique_slug(db: Session, base: str, *, exclude_id: str | None = None) -> str:
    """Return ``base`` or the first free ``-2``/``-3`` suffix."""
    from .models import Device

    candidate = normalise_slug(base)
    suffix = 1
    while True:
        statement = select(Device.id).where(Device.slug == candidate)
        if exclude_id is not None:
            statement = statement.where(Device.id != exclude_id)
        if db.scalar(statement) is None:
            return candidate
        suffix += 1
        suffix_text = f"-{suffix}"
        candidate = candidate[: SLUG_MAX_LENGTH - len(suffix_text)].rstrip("-") + suffix_text
