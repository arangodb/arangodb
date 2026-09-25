#!/bin/bash
# Start the RBAC arangod (external-rbac-service -> integration gateway) on :8529.
set -u
source "$(dirname "$0")/env.sh"
ensure_secret

RBAC_ENDPOINT="${1:-http://$INTEGRATION_GATEWAY}"

# Stop the instance this script started last time, by pidfile, and wait for
# :8529 to free up before wiping its data.
ARANGOD_PIDFILE="$WORK/arangod.pid"
if [ -f "$ARANGOD_PIDFILE" ]; then
  OLD_PID="$(cat "$ARANGOD_PIDFILE" 2>/dev/null)"
  if [ -n "${OLD_PID:-}" ] && kill -0 "$OLD_PID" 2>/dev/null; then
    kill -9 "$OLD_PID" 2>/dev/null
  fi
  rm -f "$ARANGOD_PIDFILE"
elif command -v pkill >/dev/null 2>&1; then
  pkill -9 -x arangod 2>/dev/null
fi
for i in $(seq 1 30); do port_open 127.0.0.1:8529 || break; sleep 0.5; done

rm -rf "$WORK/arangod-data"
mkdir -p "$WORK/arangod-data" "$WORK/arangod-apps"

EE_MODULES=()
if [ -d "$ARANGODB_SRC/enterprise/js" ]; then
  EE_MODULES=(--javascript.module-directory "$ARANGODB_SRC/enterprise/js")
fi

nohup "$ARANGOD" \
  --configuration none \
  --server.rest-server true \
  --server.endpoint "$ARANGOD_RBAC_ENDPOINT" \
  --server.authentication true \
  --server.harden true \
  --server.jwt-secret-folder "$JWT_DIR" \
  --server.external-rbac-service="$RBAC_ENDPOINT" \
  --database.directory "$WORK/arangod-data" \
  --javascript.startup-directory "$ARANGODB_SRC/js" \
  "${EE_MODULES[@]+"${EE_MODULES[@]}"}" \
  --javascript.app-path "$WORK/arangod-apps" \
  --log.output "file://$LOG_DIR/arangod.log" \
  --log.level info \
  --log.level authorization=debug \
  > "$LOG_DIR/arangod.stdout.log" 2>&1 &
ARANGOD_PID=$!
echo "$ARANGOD_PID" > "$ARANGOD_PIDFILE"
echo "arangod (RBAC) PID: $ARANGOD_PID  endpoint=$ARANGOD_RBAC_ENDPOINT  rbac-service=$RBAC_ENDPOINT"
