#!/usr/bin/env bash
# Server that traps and ignores SIGTERM to test kill escalation.
# Background sleeps redirect stdout/stderr to /dev/null so they don't
# hold the Port pipe open after the main process is killed.
trap '' TERM  # Ignore SIGTERM
echo "STARTED port=none pid=$$"
while true; do
  sleep 60 >/dev/null 2>&1 &
  BG_PID=$!
  wait "$BG_PID" 2>/dev/null || true
done
