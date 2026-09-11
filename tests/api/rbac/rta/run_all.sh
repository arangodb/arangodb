#!/bin/bash
# Run every test layer in this directory, cheapest first.
#
#   offline      catalog self-check, listings and dry runs. No server, seconds.
#   rbac         arangod + sidecar, the RBAC scenario matrix.
#   role-modelling  what the documented role set alone can do (needs the rbac stack).
#   classic      arangod WITHOUT --server.external-rbac-service, the classic
#                grant matrix. A supported configuration in its own right.
#
# The whole set takes roughly 40 minutes, most of it in the two matrices: a step
# that is *meant* to be denied still costs ~2 minutes, because rta-makedata's
# createSafe() retries a failing create 50 times before giving up.
#
# Layers 'rbac' and 'role-modelling' need the kube-arangodb sidecar binary:
#   export OPERATOR=/path/to/kube-arangodb/bin/linux/amd64/arangodb_operator
#
# Usage:
#   run_all.sh                          every layer
#   run_all.sh --layers offline         just the offline checks
#   run_all.sh --layers rbac,classic    just the two live configurations
#   run_all.sh --test 050,400           override the RBAC suite filter
set -u -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS="$(cd "$HERE/.." && pwd)/scripts"
RUNNER="$HERE/run_scenarios.py"
# tests/api/rbac/rta -> rbac -> api -> tests -> source root. Four levels, not
# three: run_scenarios.py counts from tests/api/rbac, this file from one deeper.
ROOT="$(cd "$HERE/../../../.." && pwd)"

LAYERS="offline,rbac,role-modelling,classic"
# Both empty means "use the runner's default" for both layers, so the two
# configurations cover exactly the same suites. Override either independently
# with --test / --classic-test.
RBAC_FILTER=""
CLASSIC_FILTER=""
CLASSIC_ENDPOINT="tcp://127.0.0.1:8530"

while [ $# -gt 0 ]; do
    case "$1" in
        --layers) LAYERS="$2"; shift 2;;
        --test) RBAC_FILTER="$2"; shift 2;;
        --classic-test) CLASSIC_FILTER="$2"; shift 2;;
        -h|--help) sed -n '2,/^set -u/p' "$0" | sed -e '/^set -u/d' -e 's/^# \{0,1\}//'; exit 0;;
        *) echo "unknown option: $1" >&2; exit 2;;
    esac
done

wants() { case ",$LAYERS," in *",$1,"*) return 0;; *) return 1;; esac; }

RESULTS=()
FAILED=0

record() {
    local name="$1" code="$2"
    if [ "$code" -eq 0 ]; then
        RESULTS+=("  PASS  $name")
    else
        RESULTS+=("  FAIL  $name (exit $code)")
        FAILED=1
    fi
}

banner() { echo; echo "################ $* ################"; }

# --------------------------------------------------------------------------
# offline - no server needed, so this is what to run in a plain CI job
# --------------------------------------------------------------------------
if wants offline; then
    banner "offline: catalog self-check"
    python3 "$RUNNER" --self-check; record "offline: self-check" $?

    banner "offline: catalog listings"
    python3 "$RUNNER" --list > /dev/null && \
    python3 "$RUNNER" --auth-mode classic --list > /dev/null
    record "offline: listings (rbac + classic)" $?

    banner "offline: dry runs"
    python3 "$RUNNER" --dry-run > /dev/null && \
    python3 "$RUNNER" --auth-mode classic --dry-run > /dev/null
    record "offline: dry runs (rbac + classic)" $?
fi

# --------------------------------------------------------------------------
# rbac - brings up arangod (:8529) plus the sidecar (:8108)
# --------------------------------------------------------------------------
if wants rbac || wants role-modelling; then
    if [ -z "${OPERATOR:-}" ]; then
        echo "ERROR: the rbac layers need the sidecar binary." >&2
        echo "  export OPERATOR=/path/to/kube-arangodb/bin/linux/amd64/arangodb_operator" >&2
        exit 2
    fi
    export ARANGOD="${ARANGOD:-$ROOT/build/bin/arangod}"
    export ARANGOSH="${ARANGOSH:-$ROOT/build/bin/arangosh}"
    export ARANGODB_SRC="${ARANGODB_SRC:-$ROOT}"
fi

if wants rbac; then
    banner "rbac: bringing up the stack and running the matrix"
    args=(--setup)
    [ -n "$RBAC_FILTER" ] && args+=(--test "$RBAC_FILTER")
    python3 "$RUNNER" "${args[@]}"; record "rbac: scenario matrix" $?
fi

if wants role-modelling; then
    banner "role-modelling: the documented coredb-admin set on its own"
    # Shows the consequence of the documented role catalog rather than a
    # pass/deny decision, so it is opt-in and outside the default matrix.
    python3 "$RUNNER" --group role-modelling; record "role-modelling: documented set only" $?
fi

# --------------------------------------------------------------------------
# classic - a second arangod on :8530 with no RBAC service at all
# --------------------------------------------------------------------------
if wants classic; then
    banner "classic: arangod without --server.external-rbac-service"
    export ARANGOD="${ARANGOD:-$ROOT/build/bin/arangod}"
    export ARANGOSH="${ARANGOSH:-$ROOT/build/bin/arangosh}"
    export ARANGODB_SRC="${ARANGODB_SRC:-$ROOT}"
    bash "$SCRIPTS/start_arangod_classic.sh"
    for _ in $(seq 1 90); do
        (exec 3<>/dev/tcp/127.0.0.1/8530) 2>/dev/null && { exec 3>&-; break; }
        sleep 1
    done
    # The catalog is chosen by probing, so --auth-mode is not passed here: a
    # wrong verdict should surface as a failure rather than be papered over.
    cargs=(--endpoint "$CLASSIC_ENDPOINT")
    [ -n "$CLASSIC_FILTER" ] && cargs+=(--test "$CLASSIC_FILTER")
    python3 "$RUNNER" "${cargs[@]}"
    record "classic: grant matrix" $?
fi

# --------------------------------------------------------------------------
echo
echo "=============================== summary ==============================="
for line in "${RESULTS[@]+"${RESULTS[@]}"}"; do echo "$line"; done
echo "======================================================================="
if [ "$FAILED" -eq 0 ]; then
    echo "all selected layers passed"
else
    echo "at least one layer failed"
fi
exit "$FAILED"
