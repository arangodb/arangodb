#!/bin/bash
# Shared configuration for the RBAC apitest scripts.
# Source this from every script:  source "$(dirname "$0")/env.sh"
#
# All paths can be overridden via environment variables.

# Directory containing the rbac test files (tests/api/rbac).
RBAC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Runtime working directory: JWT secret, server data dirs and logs live here.
# Kept OUT of the source tree by default.
WORK="${RBAC_WORK:-/tmp/arangodb-rbac-test}"
JWT_DIR="$WORK/jwt"
LOG_DIR="$WORK/log"
mkdir -p "$WORK" "$JWT_DIR" "$LOG_DIR"

# Binaries (override as needed for your checkout / platform).
ARANGOD="${ARANGOD:-$HOME/ArangoDB/arangodb/cmake-build-debug/bin/arangod}"
ARANGOSH="${ARANGOSH:-$HOME/ArangoDB/arangodb/cmake-build-debug/bin/arangosh}"
ARANGODB_SRC="${ARANGODB_SRC:-$HOME/ArangoDB/arangodb}"
OPERATOR="${OPERATOR:-$HOME/ArangoDB/kube-arangodb/bin/linux/arm64/arangodb_operator}"
OPERATOR_INT="${OPERATOR_INT:-$HOME/ArangoDB/kube-arangodb/bin/linux/arm64/arangodb_operator_integration}"

# Endpoints / ports.
ARANGOD_RBAC_ENDPOINT="tcp://127.0.0.1:8529"     # RBAC server under test
ARANGOD_CLASSIC_ENDPOINT="tcp://127.0.0.1:8530"  # classic baseline server
INTEGRATION_GATEWAY="127.0.0.1:9192"             # authorization.v1 HTTP gateway
INTEGRATION_GRPC="127.0.0.1:9092"
SIDECAR_GRPC="127.0.0.1:8109"
SIDECAR_MGMT="127.0.0.1:8108"
SIDECAR_HEALTH="127.0.0.1:8107"

# The active JWT signing secret MUST live in a file named "-" (the operator's
# ActiveJWTKey convention) and be a full 64-byte key so arangod (raw bytes) and
# the operator (padded to DefaultTokenSecretSize=64) compute the same HMAC key.
ensure_secret() {
  if [ ! -s "$JWT_DIR/-" ]; then
    python3 -c "import secrets; print(secrets.token_hex(32), end='')" > "$JWT_DIR/-"
  fi
}
