#!/usr/bin/env bash
set -euo pipefail

PORT=""
CRASH_AFTER=""
EXIT_CODE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      PORT="$2"
      shift 2
      ;;
    --crash-after)
      CRASH_AFTER="$2"
      shift 2
      ;;
    --exit-code)
      EXIT_CODE="$2"
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done

cleanup() {
  if [[ -n "${BG_PID:-}" ]]; then
    kill "$BG_PID" 2>/dev/null || true
    wait "$BG_PID" 2>/dev/null || true
  fi
}

trap cleanup EXIT

handle_sigterm() {
  echo "SIGTERM received"
  cleanup
  exit "$EXIT_CODE"
}

trap handle_sigterm TERM

echo "STARTED port=${PORT:-none} pid=$$"

if [[ -n "$CRASH_AFTER" ]]; then
  sleep "$CRASH_AFTER" &
  BG_PID=$!
  wait "$BG_PID" 2>/dev/null || true
  unset BG_PID
  exit 1
fi

while true; do
  sleep 60 &
  BG_PID=$!
  wait "$BG_PID" 2>/dev/null || true
done
