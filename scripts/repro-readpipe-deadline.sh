#!/usr/bin/env bash
# //////////////////////////////////////////////////////////////////////////////
# Watchdog wrapper around scripts/repro-readpipe-deadline.js.
#
# fs.readPipe() blocks in TRI_ReadPointer() (lib/Basics/files.cpp:757) until
# 1023 bytes have been collected or the pipe reaches EOF. On a build without
# the interruptible read, scenarios A, B and E hang forever - so every
# scenario has to run in its own arangosh under an external timeout. Once the
# deadline has fired the whole shell session is poisoned anyway, which is the
# second reason for one process per scenario.
#
# usage:
#   scripts/repro-readpipe-deadline.sh                 # all scenarios
#   scripts/repro-readpipe-deadline.sh A E             # selected ones
#
# environment:
#   ARANGOSH       path to the arangosh binary (default: ./build/bin/arangosh)
#   ARANGOSH_ARGS  extra arguments, e.g. --javascript.startup-directory ./js
#   BUDGET         watchdog timeout per scenario in seconds (default: 25)
#
# To see the problem, run this against a build of origin/devel; to see it
# fixed, run it against a build with the interruptible TRI_ReadPipe(). Only
# lib/Basics/process-utils.* and lib/V8/v8-utils.cpp differ for the purposes of
# this repro, so make sure both translation units were actually recompiled
# between the two runs. To check what a given binary contains:
#
#   strings build/bin/arangosh | grep 'cannot poll pipe of pid'
#
# Note: whenever a scenario has to be killed by the watchdog its stalling
# children are orphaned and linger for up to STALL seconds (120s, see the .js).
# Scenario B additionally leaves its grandchild behind by design - that
# grandchild is what keeps the pipe from ever reaching EOF.
# //////////////////////////////////////////////////////////////////////////////
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JS="${HERE}/repro-readpipe-deadline.js"

if [ ! -f "${JS}" ]; then
  echo "cannot find ${JS}" >&2
  exit 2
fi

# //////////////////////////////////////////////////////////////////////////////
# locate arangosh
# //////////////////////////////////////////////////////////////////////////////
ARANGOSH="${ARANGOSH:-}"
if [ -z "${ARANGOSH}" ]; then
  for candidate in "${HERE}/../build/bin/arangosh" "${HERE}/../bin/arangosh"; do
    if [ -x "${candidate}" ]; then
      ARANGOSH="${candidate}"
      break
    fi
  done
fi

if [ -z "${ARANGOSH}" ] || [ ! -x "${ARANGOSH}" ]; then
  echo "arangosh not found - build it or set ARANGOSH=/path/to/arangosh" >&2
  exit 2
fi

# //////////////////////////////////////////////////////////////////////////////
# locate a timeout implementation - without it we cannot tell a hang from a
# failure, which is the whole point of this script
# //////////////////////////////////////////////////////////////////////////////
TIMEOUT=""
for candidate in timeout gtimeout; do
  if command -v "${candidate}" > /dev/null 2>&1; then
    TIMEOUT="${candidate}"
    break
  fi
done

if [ -z "${TIMEOUT}" ]; then
  echo "neither 'timeout' nor 'gtimeout' found; install GNU coreutils" >&2
  exit 2
fi

BUDGET="${BUDGET:-25}"
ARANGOSH_ARGS="${ARANGOSH_ARGS:-}"

SCENARIOS="$*"
if [ -z "${SCENARIOS}" ]; then
  SCENARIOS="A B C D E"
fi

echo "arangosh: ${ARANGOSH}"
if strings "${ARANGOSH}" 2>/dev/null | grep -q 'cannot poll pipe of pid'; then
  echo "          contains the interruptible TRI_ReadPipe()"
else
  echo "          does NOT contain the interruptible TRI_ReadPipe()"
fi
echo "watchdog: ${TIMEOUT} ${BUDGET}s per scenario"
echo

HUNG=""
FAILED=""
PASSED=""

for scenario in ${SCENARIOS}; do
  echo "============================ scenario ${scenario} ========================="

  # shellcheck disable=SC2086
  "${TIMEOUT}" --signal=KILL "${BUDGET}" \
    "${ARANGOSH}" \
    --server.endpoint none \
    --javascript.allow-external-process-control true \
    --log.level warning \
    --javascript.execute "${JS}" \
    ${ARANGOSH_ARGS} \
    -- "${scenario}" 2>&1 | grep -v 'Killed *"\${TIMEOUT}"'
  rc=${PIPESTATUS[0]}

  case ${rc} in
    0)
      echo "--> scenario ${scenario}: PASSED"
      PASSED="${PASSED} ${scenario}"
      ;;
    124 | 137)
      echo "--> scenario ${scenario}: HUNG - watchdog killed arangosh after ${BUDGET}s."
      echo "    arangosh is parked in read() on the child's stdout pipe; the"
      echo "    execution deadline cannot be observed because no JS runs."
      echo "    Confirm with: cat /proc/<arangosh-pid>/wchan  ->  pipe_read"
      HUNG="${HUNG} ${scenario}"
      ;;
    *)
      echo "--> scenario ${scenario}: FAILED with exit code ${rc}"
      FAILED="${FAILED} ${scenario}"
      ;;
  esac
  echo
done

# //////////////////////////////////////////////////////////////////////////////
# verdict
# //////////////////////////////////////////////////////////////////////////////
echo "============================ summary ==============================="
echo "passed:${PASSED:- -}"
echo "hung:  ${HUNG:- -}"
echo "failed:${FAILED:- -}"
echo

if [ -n "${HUNG}" ]; then
  echo "VERDICT: the blocking readPipe() is reproduced."
  echo "  Scenarios${HUNG} could only be stopped by the watchdog. On such a"
  echo "  build the IsDeadlineReached() calls in the pipe loops of the js.js"
  echo "  and go.js testsuites can never fire while a child is silent, so"
  echo "  --oneTestTimeout is not enforceable for js_driver and go_driver."
  exit 1
fi

if [ -n "${FAILED}" ]; then
  echo "VERDICT: no hang, but scenarios${FAILED} did not behave as expected."
  echo "  See the RESULT lines above."
  exit 1
fi

echo "VERDICT: reads are interruptible, EOF and chunking semantics are intact."
exit 0

