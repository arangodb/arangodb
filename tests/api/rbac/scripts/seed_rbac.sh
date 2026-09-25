#!/bin/bash
# Seed a demo RBAC policy/role/binding into the sidecar (stored in arangod's
# _system). Grants a user read access to everything (db:Read on db:*).
# Management API lives on the SIDECAR gateway (:8108).
set -u
source "$(dirname "$0")/env.sh"
ensure_secret
MGMT="http://$SIDECAR_MGMT/_management/permissions"
SU=$(python3 "$RBAC_DIR/mkjwt.py" superuser "$JWT_DIR/-")
H=(-H "authorization: bearer $SU" -H 'content-type: application/json')

USER="${1:-alice}"
POLICY="${2:-${USER}-read}"
ROLE="${3:-${USER}-role}"
ACTIONS="${ACTIONS:-\"db:Read\"}"
RESOURCES="${RESOURCES:-\"db:*\"}"

echo "=== policy '$POLICY' (Allow actions=[$ACTIONS] resources=[$RESOURCES]) ==="
curl -s -o /dev/null -w "POST policy -> %{http_code}\n" -X POST "$MGMT/policy/$POLICY" "${H[@]}" \
  -d "{\"item\":{\"statements\":[{\"effect\":\"Allow\",\"actions\":[$ACTIONS],\"resources\":[$RESOURCES]}]}}"
echo "=== role '$ROLE' -> policy '$POLICY' ==="
curl -s -o /dev/null -w "POST role -> %{http_code}\n" -X POST "$MGMT/role/$ROLE" "${H[@]}" \
  -d "{\"item\":{\"policies\":[\"$POLICY\"]}}"
# The binding scope is a REQUIRED AND-boundary; an empty scope denies everything.
echo "=== bind '$ROLE' to '$USER' (allow-all scope) ==="
curl -s -o /dev/null -X DELETE "$MGMT/user/$USER/role/$ROLE" "${H[@]}"
curl -s -o /dev/null -w "POST binding -> %{http_code}\n" -X POST "$MGMT/user/$USER/role/$ROLE" "${H[@]}" \
  -d '{"scope":{"statements":[{"effect":"Allow","actions":["*"],"resources":["*"]}]}}'
curl -s -o /dev/null -w "POST refresh -> %{http_code}\n" -X POST "$MGMT/refresh" "${H[@]}" -d '{}'
