#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/firmware/tests/native/build"

if [[ -x "$ROOT/.venv/bin/cmake" ]]; then
  CMAKE="$ROOT/.venv/bin/cmake"
  CTEST="$ROOT/.venv/bin/ctest"
elif command -v cmake >/dev/null 2>&1; then
  CMAKE="cmake"
  CTEST="ctest"
else
  echo "cmake is required. Run 'make server-setup' (installs it in .venv) or install cmake." >&2
  exit 1
fi

"$CMAKE" -S "$ROOT/firmware/tests/native" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug
# Default to one job: build hosts can be memory-constrained.
# Override with AK_BUILD_JOBS=N only when there is memory to spare.
"$CMAKE" --build "$BUILD" --parallel "${AK_BUILD_JOBS:-1}"
"$CTEST" --test-dir "$BUILD" --output-on-failure
