#!/bin/bash
# Start the authorization sidecar (RBAC policy store). It stores
# _policies / _roles / _user_role_bindings in the ArangoDB it points at and
# authenticates to ArangoDB as superuser via the shared JWT folder.
# Usage: start_sidecar.sh [central|central-permissive]
set -u
source "$(dirname "$0")/env.sh"
ensure_secret
MODE="${1:-central-permissive}"

pkill -9 -f "arangodb_operator sidecar" 2>/dev/null
for i in $(seq 1 20); do ss -ltn 2>/dev/null | grep -qE "$SIDECAR_GRPC" || break; sleep 0.5; done

setsid "$OPERATOR" sidecar \
  --arangodb.endpoint="http://${ARANGOD_RBAC_ENDPOINT#tcp://}" \
  --sidecar.auth="$JWT_DIR" \
  --sidecar.auth.mode="$MODE" \
  --sidecar.address="$SIDECAR_GRPC" \
  --sidecar.gateway.address="$SIDECAR_MGMT" \
  --sidecar.health.address="$SIDECAR_HEALTH" \
  > "$LOG_DIR/sidecar.log" 2>&1 &
disown
for i in $(seq 1 40); do
  ss -ltn 2>/dev/null | grep -qE "$SIDECAR_MGMT" && { echo "sidecar ($MODE) up: gRPC $SIDECAR_GRPC mgmt $SIDECAR_MGMT"; exit 0; }
  sleep 0.5
done
echo "sidecar ($MODE) FAILED to bind"; tail -8 "$LOG_DIR/sidecar.log"; exit 1
