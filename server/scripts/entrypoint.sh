#!/bin/sh
set -eu

# Migrations are the only schema mechanism in production.
alembic upgrade head

exec "$@"
