#!/bin/bash
# Start the authorization integration service (authorization.v1 PDP).
# Usage: start_integration.sh [always|never|central|central-permissive]
set -u
source "$(dirname "$0")/env.sh"
ensure_secret
MODE="${1:-central}"

pkill -9 -f arangodb_operator_integration 2>/dev/null
for i in $(seq 1 20); do ss -ltn 2>/dev/null | grep -qE "$INTEGRATION_GATEWAY" || break; sleep 0.5; done

# central / central-permissive delegate to the sidecar, whose gRPC address is
# read from this env var (not a flag).
export CENTRAL_INTEGRATION_SERVICE_ADDRESS="$SIDECAR_GRPC"

setsid "$OPERATOR_INT" \
  --integration.authorization.v1 \
  --integration.authorization.v1.type="$MODE" \
  --integration.authentication.v1 \
  --integration.authentication.v1.path="$JWT_DIR" \
  --services.address="$INTEGRATION_GRPC" \
  --services.gateway.address="$INTEGRATION_GATEWAY" \
  > "$LOG_DIR/integration.log" 2>&1 &
disown
for i in $(seq 1 30); do
  ss -ltn 2>/dev/null | grep -qE "$INTEGRATION_GATEWAY" && { echo "integration ($MODE) bound on $INTEGRATION_GATEWAY"; exit 0; }
  sleep 0.5
done
echo "integration ($MODE) FAILED to bind"; tail -5 "$LOG_DIR/integration.log"; exit 1
