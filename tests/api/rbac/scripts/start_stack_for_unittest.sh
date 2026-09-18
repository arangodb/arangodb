#!/bin/bash
# usage:
#
#   tests/api/rbac/scripts/start_stack_for_unittest.sh            # start + verify
#   tests/api/rbac/scripts/start_stack_for_unittest.sh --run       # ...then run the suite
#   tests/api/rbac/scripts/start_stack_for_unittest.sh --run --test 050,400
#   tests/api/rbac/scripts/start_stack_for_unittest.sh --stop      # tear it down

set -u -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# tests/api/rbac/scripts -> rbac -> api -> tests -> arangodb
ROOT="$(cd "$HERE/../../../.." && pwd)"

DO_RUN=0
DO_STOP=0
FILTER="050,100,400,500,580,607,612"
MODE="central"

while [ $# -gt 0 ]; do
    case "$1" in
        --run)   DO_RUN=1; shift ;;
        --stop)  DO_STOP=1; shift ;;
        --test)  FILTER="${2:-}"; shift 2 ;;
        --mode)  MODE="${2:-}"; shift 2 ;;
        -h|--help) sed -n '2,40p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 64 ;;
    esac
done

# --- configuration ---------------------------------------------------------
export RBAC_WORK="${RBAC_WORK:-/tmp/rbac-unittest}"
export ARANGODB_SRC="${ARANGODB_SRC:-$ROOT}"
export ARANGOD="${ARANGOD:-$ROOT/build/bin/arangod}"
export ARANGOSH="${ARANGOSH:-$ROOT/build/bin/arangosh}"

source "$HERE/env.sh"

MGMT_URL="http://$SIDECAR_MGMT"

# --- helpers ---------------------------------------------------------------
die() { echo "error: $*" >&2; exit 1; }

# Kill by pid, but only if the process really is one of ours. Never `pkill -f`:
# that pattern matches this script's own command line and kills the caller.
kill_if_named() {
    local pid="$1" want="$2" name
    [ -n "$pid" ] || return 0
    name="$(ps -p "$pid" -o comm= 2>/dev/null || true)"
    case "$name" in
        "$want"*) kill -9 "$pid" 2>/dev/null; return 0 ;;
        "") return 0 ;;   # already gone
        *)  echo "  refusing to kill pid $pid ('$name' is not $want)" >&2; return 1 ;;
    esac
}

pid_on_port() {
    ss -ltnp 2>/dev/null | awk -v ep="$1" '$4 == ep' \
        | grep -oE 'pid=[0-9]+' | head -1 | cut -d= -f2
}

free_ports() {
    local failed=0 pid
    if [ -f "$WORK/sidecar.pid" ]; then
        kill_if_named "$(cat "$WORK/sidecar.pid" 2>/dev/null)" arangodb_operat || failed=1
        rm -f "$WORK/sidecar.pid"
    fi
    for endpoint in "$SIDECAR_MGMT" "$SIDECAR_GRPC" "$SIDECAR_HEALTH"; do
        pid="$(pid_on_port "$endpoint")"
        [ -n "$pid" ] && { kill_if_named "$pid" arangodb_operat || failed=1; }
    done
    pid="$(pid_on_port "${ARANGOD_RBAC_ENDPOINT#tcp://}")"
    [ -n "$pid" ] && { kill_if_named "$pid" arangod || failed=1; }
    [ "$failed" -eq 0 ] || die "could not free the fixed ports; stop whatever holds them by hand"
    for i in $(seq 1 40); do
        ss -ltn 2>/dev/null | grep -qE "($SIDECAR_MGMT|$SIDECAR_GRPC|$SIDECAR_HEALTH|${ARANGOD_RBAC_ENDPOINT#tcp://})" || return 0
        sleep 0.5
    done
    die "ports still occupied after 20s"
}

# --- --stop ----------------------------------------------------------------
if [ "$DO_STOP" -eq 1 ]; then
    echo "stopping the unittest RBAC stack ($WORK)"
    free_ports
    echo "stopped."
    exit 0
fi

# --- preflight -------------------------------------------------------------
[ -x "$ARANGOD" ]  || die "arangod not found or not executable: $ARANGOD"
[ -x "$ARANGOSH" ] || die "arangosh not found or not executable: $ARANGOSH"

# The sidecar binary comes from kube-arangodb.
# Look where it usually sits next to it before giving up.
if [ -z "${OPERATOR:-}" ] || [ ! -x "${OPERATOR:-}" ]; then
    for candidate in \
        "$(dirname "$ROOT")/kube-arangodb/bin/linux/amd64/arangodb_operator" \
        "$(dirname "$ROOT")/kube-arangodb/bin/linux/arm64/arangodb_operator"
    do
        [ -x "$candidate" ] && { export OPERATOR="$candidate"; break; }
    done
fi
[ -n "${OPERATOR:-}" ] && [ -x "$OPERATOR" ] || die \
"the kube-arangodb sidecar binary was not found. Set it explicitly:
  export OPERATOR=/path/to/kube-arangodb/bin/linux/amd64/arangodb_operator"

TEST_UTILS="$ROOT/js/client/modules/@arangodb/testutils/test-utils.js"
[ -f "$TEST_UTILS" ] || die "cannot find $TEST_UTILS to read the harness JWT secret from"
SECRET="$(sed -nE "s/^const testsecret = '([^']*)'.*/\1/p" "$TEST_UTILS" | head -1)"
[ -n "$SECRET" ] || die \
"could not read \`const testsecret = '...'\` from $TEST_UTILS.
 The harness JWT secret is what the sidecar has to agree with; refusing to
 guess it. Check whether that declaration was renamed."

echo "source root   : $ROOT"
echo "work directory: $WORK"
echo "arangod       : $ARANGOD"
echo "sidecar       : $OPERATOR"
echo "harness secret: '$SECRET' (from test-utils.js)"
echo

echo "=== freeing the fixed ports ==="
free_ports

mkdir -p "$JWT_DIR"
if [ -s "$JWT_DIR/-" ] && [ "$(cat "$JWT_DIR/-")" != "$SECRET" ]; then
    echo "note: replacing the existing key in $JWT_DIR/- - it did not match the harness secret"
fi
printf '%s' "$SECRET" > "$JWT_DIR/-"

echo "=== starting arangod (the sidecar's policy store) ==="
"$HERE/start_arangod.sh" "$MGMT_URL" || die "start_arangod.sh failed"

echo "=== starting the sidecar ($MODE) ==="
"$HERE/start_sidecar.sh" "$MODE" || die "start_sidecar.sh failed"

# --- verify ----------------------------------------------------------------
echo
echo "=== verifying ==="
SU="$(python3 "$RBAC_DIR/mkjwt.py" superuser "$JWT_DIR/-")" \
    || die "could not mint a superuser token with mkjwt.py"

VERSION_JSON=""
for i in $(seq 1 40); do
    VERSION_JSON="$(curl -s -m 5 -H "Authorization: bearer $SU" \
        "http://${ARANGOD_RBAC_ENDPOINT#tcp://}/_api/version" || true)"
    case "$VERSION_JSON" in *'"version"'*) break ;; esac
    sleep 0.5
done
case "$VERSION_JSON" in
    *'"version"'*) echo "  arangod accepts a token signed with the harness secret" ;;
    *) die "arangod at $ARANGOD_RBAC_ENDPOINT did not accept a superuser token signed
 with the harness secret. Answer was: ${VERSION_JSON:-<nothing>}
 Check $LOG_DIR/arangod.log." ;;
esac

# The management API answers 200 on a listing.
# It answers `503 {"code": 14, "message": "service is not healthy"}`
# until its store is ready, which is later than the authorization.v1=healthy line
# start_sidecar.sh waits for.
MGMT_CODE=""
for i in $(seq 1 60); do
    MGMT_CODE="$(curl -s -m 5 -o /dev/null -w '%{http_code}' \
        "$MGMT_URL/_management/permissions/policy" || true)"
    [ "$MGMT_CODE" = "200" ] && break
    sleep 0.5
done
[ "$MGMT_CODE" = "200" ] \
    || die "the sidecar management API still answered HTTP $MGMT_CODE after 30s,
 expected 200. 503 means its store never became healthy.
 Check $LOG_DIR/sidecar.log."
echo "  sidecar management API is serving"

CMD="./scripts/unittest rta_makedata --rbac $MGMT_URL --test $FILTER"

echo
echo "stack is up and verified."
if [ "$MODE" != "central" ]; then
    echo "note: mode '$MODE' - scenarios needing 'central' will be skipped."
fi
echo
if [ "$DO_RUN" -eq 1 ]; then
    echo "=== $CMD ==="
    cd "$ROOT" || die "cannot cd to $ROOT"
    exec ./scripts/unittest rta_makedata --rbac "$MGMT_URL" --test "$FILTER"
fi
echo "run it with:"
echo "  cd $ROOT && $CMD"
echo
echo "tear down with:"
echo "  $HERE/$(basename "${BASH_SOURCE[0]}") --stop"
