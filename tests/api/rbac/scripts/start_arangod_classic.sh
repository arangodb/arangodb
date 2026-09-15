#!/bin/bash
# Start a second arangod in CLASSIC permission mode (no RBAC) on :8530,
# sharing the JWT secret so apitester's superuser token works.
set -u
source "$(dirname "$0")/env.sh"
ensure_secret

pkill -9 -f "arangod-classic-data" 2>/dev/null
for i in $(seq 1 30); do ss -ltn 2>/dev/null | grep -qE '127.0.0.1:8530' || break; sleep 0.5; done

rm -rf "$WORK/arangod-classic-data"
mkdir -p "$WORK/arangod-classic-data" "$WORK/arangod-classic-apps"

nohup "$ARANGOD" \
  --configuration none \
  --server.rest-server true \
  --server.endpoint "$ARANGOD_CLASSIC_ENDPOINT" \
  --server.authentication true \
  --server.jwt-secret-folder "$JWT_DIR" \
  --database.directory "$WORK/arangod-classic-data" \
  --javascript.startup-directory "$ARANGODB_SRC/js" \
  --javascript.app-path "$WORK/arangod-classic-apps" \
  --log.output "file://$LOG_DIR/arangod-classic.log" \
  --log.level info \
  > "$LOG_DIR/arangod-classic.stdout.log" 2>&1 &
echo "arangod (classic) PID: $!  endpoint=$ARANGOD_CLASSIC_ENDPOINT"
