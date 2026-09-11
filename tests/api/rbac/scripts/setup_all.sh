#!/bin/bash
# Bring up the full RBAC stack for manual testing:
#   1. shared 64-byte JWT secret (folder $JWT_DIR, active key file "-")
#   2. arangod (RBAC + --server.harden) on :8529
#   3. authorization sidecar (RBAC policy store, in arangod _system) :8108/:8109
#   4. integration service (authorization.v1, central mode)          :9192/:9092
#   5. a demo policy/role/binding granting user 'alice' read access
set -u
D="$(dirname "$0")"
source "$D/env.sh"

say(){ echo; echo "############ $* ############"; }

say "0) shared 64-byte JWT secret (active key file must be named '-')"
ensure_secret
echo "secret: $JWT_DIR/-  ($(wc -c < "$JWT_DIR/-") bytes)"

say "1) (re)start arangod (RBAC + harden, external-rbac-service -> :9192)"
bash "$D/start_arangod.sh" "http://$INTEGRATION_GATEWAY"
until [ "$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8529/_api/version --max-time 2)" = "401" ]; do
  pgrep -x arangod >/dev/null || { echo "arangod DIED"; tail -5 "$LOG_DIR/arangod.stdout.log"; exit 1; }
  sleep 2
done
echo "arangod ready on :8529"

say "2) create user 'alice' (superuser bypasses RBAC)"
SU=$(python3 "$RBAC_DIR/mkjwt.py" superuser "$JWT_DIR/-")
curl -s -o /dev/null -w "create alice -> %{http_code}\n" -X POST http://127.0.0.1:8529/_api/user \
  -H "authorization: bearer $SU" -H 'content-type: application/json' -d '{"user":"alice","passwd":"alice"}'

say "3) start the authorization sidecar (central-permissive)"
bash "$D/start_sidecar.sh" central-permissive || exit 1

say "4) start the integration service in central mode"
bash "$D/start_integration.sh" central || exit 1
sleep 3

say "5) seed demo RBAC for alice"
bash "$D/seed_rbac.sh" alice

say "6) verify"
ALICE=$(python3 "$RBAC_DIR/mkjwt.py" user "$JWT_DIR/-" alice)
for i in $(seq 1 10); do
  code=$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8529/_api/collection -H "authorization: bearer $ALICE")
  [ "$code" = "200" ] && break; sleep 3
done
echo "alice READ -> HTTP $(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8529/_api/collection -H "authorization: bearer $ALICE") (expect 200)"

cat <<EOF

############ STACK UP ############
arangod            http://127.0.0.1:8529
integration (PDP)  http://$INTEGRATION_GATEWAY
sidecar mgmt API   http://$SIDECAR_MGMT/_management/permissions/...
secret             $JWT_DIR/-
logs               $LOG_DIR/{arangod,integration,sidecar}.log
Stop:  pkill -x arangod; pkill -f arangodb_operator
#################################
EOF
