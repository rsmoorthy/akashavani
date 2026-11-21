#!/usr/bin/env bash
set -euo pipefail

PORT="${AKASHAVANI_PORT:-8000}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if ! command -v ss >/dev/null 2>&1; then
  echo "ss is required to find the process listening on port $PORT." >&2
  exit 1
fi

mapfile -t listening < <(
  ss -ltnp "sport = :$PORT" 2>/dev/null \
    | grep -oE 'pid=[0-9]+' \
    | cut -d= -f2 \
    | sort -u \
    || true
)

if [[ "${#listening[@]}" -eq 0 ]]; then
  echo "No process is listening on port $PORT."
  exit 0
fi

declare -A targets=()
for pid in "${listening[@]}"; do
  targets["$pid"]=1
  parent="$(ps -o ppid= -p "$pid" | tr -d ' ')"
  if [[ -n "$parent" && "$parent" != "1" ]]; then
    parent_comm="$(ps -o comm= -p "$parent" 2>/dev/null | tr -d ' ' || true)"
    parent_cmd="$(ps -o cmd= -p "$parent" 2>/dev/null || true)"
    # uvicorn --reload keeps a supervisor that respawns the worker which owns
    # the listening socket. Kill the supervisor as well as the worker, but do
    # not kill a bash/make wrapper that merely mentions uvicorn in its command.
    if [[ "$parent_comm" == "uvicorn" || "$parent_comm" == python* ]] \
      && [[ "$parent_cmd" == *"uvicorn"* ]]; then
      targets["$parent"]=1
    fi
  fi
done

echo "Stopping process(es) on port $PORT:"
for pid in "${!targets[@]}"; do
  ps -o pid=,ppid=,cmd= -p "$pid" 2>/dev/null || true
done

kill -TERM "${!targets[@]}" 2>/dev/null || true
for _ in $(seq 1 25); do
  if ! ss -ltn "sport = :$PORT" 2>/dev/null | grep -q LISTEN; then
    echo "Port $PORT is free."
    exit 0
  fi
  sleep 0.2
done

echo "Port $PORT did not free after SIGTERM; sending SIGKILL." >&2
kill -KILL "${!targets[@]}" 2>/dev/null || true
sleep 0.5
if ss -ltn "sport = :$PORT" 2>/dev/null | grep -q LISTEN; then
  echo "Port $PORT is still in use." >&2
  exit 1
fi
echo "Port $PORT is free."
