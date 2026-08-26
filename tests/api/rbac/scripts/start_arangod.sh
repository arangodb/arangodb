#!/bin/bash
# Start the RBAC arangod (external-rbac-service -> integration gateway) on :8529.
set -u
source "$(dirname "$0")/env.sh"
ensure_secret

RBAC_ENDPOINT="${1:-http://$INTEGRATION_GATEWAY}"

# stop any previous instance and wait for :8529 to free up before wiping data
pkill -9 -x arangod 2>/dev/null
for i in $(seq 1 30); do ss -ltn 2>/dev/null | grep -qE '127.0.0.1:8529' || break; sleep 0.5; done

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
echo "arangod (RBAC) PID: $!  endpoint=$ARANGOD_RBAC_ENDPOINT  rbac-service=$RBAC_ENDPOINT"
