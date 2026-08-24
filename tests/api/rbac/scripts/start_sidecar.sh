#!/bin/bash
# Start the authorization sidecar (RBAC policy store). It stores
# _policies / _roles / _user_role_bindings in the ArangoDB it points at and
# authenticates to ArangoDB as superuser via the shared JWT folder.
# Usage: start_sidecar.sh [central|central-permissive]
#
# Since this script was first written the sidecar also serves the
# authentication.v1 and authorization.v1 integration services on its own HTTP
# gateway (pkg/sidecar/register.go). arangod's
# --server.external-rbac-service should therefore point at $SIDECAR_MGMT, and a
# separate arangodb_operator_integration process is no longer part of this
# topology. Three things had to be added for the sidecar to come up outside a
# Pod; each is explained where it happens below.
set -u
source "$(dirname "$0")/env.sh"
ensure_secret
MODE="${1:-central-permissive}"

# Kill by pidfile rather than `pkill -f "arangodb_operator sidecar"`: that
# pattern also matches the command line of whatever invoked this script, so a
# caller that mentions it kills itself.
PIDFILE="$WORK/sidecar.pid"
if [ -f "$PIDFILE" ]; then
  PID=$(cat "$PIDFILE" 2>/dev/null)
  if [ -n "${PID:-}" ] && kill -0 "$PID" 2>/dev/null; then
    kill -9 -- "-$PID" 2>/dev/null || kill -9 "$PID" 2>/dev/null
  fi
  rm -f "$PIDFILE"
fi
for i in $(seq 1 20); do ss -ltn 2>/dev/null | grep -qE "$SIDECAR_GRPC" || break; sleep 0.5; done

# 1. The sidecar opens a unix socket under /var/run/sidecar (hardcoded in
#    pkg/util/constants/sidecar.go) for internal service-to-service calls, and
#    exits if it cannot create it. /var/run is not writable outside a Pod, so
#    run it in a user+mount namespace with a writable directory bind-mounted
#    over /run. Needs unprivileged user namespaces (the default on most
#    distributions); no root required.
RUNDIR="$WORK/run"
mkdir -p "$RUNDIR/sidecar"

# 2. CENTRAL_INTEGRATION_SERVICE_ADDRESS is how the authorization.v1 pool client
#    finds the pool to sync from - in this single-process topology, the
#    sidecar's own gRPC port. Without it authorization.v1 stays `degraded` and
#    every evaluation fails, which arangod reports as a blanket denial.
# 3. INTEGRATION_ARANGO_JWT_FOLDER lets that same client authenticate. The
#    connection carries no credentials otherwise, and because --sidecar.auth is
#    set the sidecar's own authenticator answers its own client with
#    `Unauthenticated: Unauthorized`.
#    Both mirror what the operator injects in pkg/deployment/resources/internal_sidecar.go.
CENTRAL_INTEGRATION_SERVICE_ADDRESS="$SIDECAR_GRPC" \
INTEGRATION_ARANGO_JWT_FOLDER="$JWT_DIR" \
setsid unshare --map-root-user --mount bash -c '
  mount --bind "$0" /run || { echo "bind mount over /run failed" >&2; exit 91; }
  shift
  exec "$@"
' "$RUNDIR" _ "$OPERATOR" sidecar \
  --arangodb.endpoint="http://${ARANGOD_RBAC_ENDPOINT#tcp://}" \
  --sidecar.auth="$JWT_DIR" \
  --sidecar.auth.mode="$MODE" \
  --sidecar.address="$SIDECAR_GRPC" \
  --sidecar.gateway.address="$SIDECAR_MGMT" \
  --sidecar.health.address="$SIDECAR_HEALTH" \
  > "$LOG_DIR/sidecar.log" 2>&1 &
echo $! > "$PIDFILE"
disown

for i in $(seq 1 60); do
  if ss -ltn 2>/dev/null | grep -qE "$SIDECAR_MGMT"; then
    # Binding the gateway is not enough: authorization.v1 needs its pool client
    # connected before a decision means anything.
    for j in $(seq 1 40); do
      if grep -q "authorization.v1=healthy" "$LOG_DIR/sidecar.log" 2>/dev/null; then
        echo "sidecar ($MODE) up: gRPC $SIDECAR_GRPC mgmt $SIDECAR_MGMT"
        exit 0
      fi
      sleep 0.5
    done
    echo "sidecar ($MODE) bound $SIDECAR_MGMT but authorization.v1 never became healthy"
    grep -E "pool client error|degraded" "$LOG_DIR/sidecar.log" | tail -5
    exit 1
  fi
  sleep 0.5
done
echo "sidecar ($MODE) FAILED to bind"; tail -15 "$LOG_DIR/sidecar.log"; exit 1
