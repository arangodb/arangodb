#!/usr/bin/env bash

################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

set -euo pipefail

PORT=""
CRASH_AFTER=""
EXIT_CODE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      PORT="$2"
      shift 2
      ;;
    --crash-after)
      CRASH_AFTER="$2"
      shift 2
      ;;
    --exit-code)
      EXIT_CODE="$2"
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done

cleanup() {
  if [[ -n "${BG_PID:-}" ]]; then
    kill "$BG_PID" 2>/dev/null || true
    wait "$BG_PID" 2>/dev/null || true
  fi
}

trap cleanup EXIT

handle_sigterm() {
  echo "SIGTERM received"
  cleanup
  exit "$EXIT_CODE"
}

trap handle_sigterm TERM

echo "STARTED port=${PORT:-none} pid=$$"

if [[ -n "$CRASH_AFTER" ]]; then
  sleep "$CRASH_AFTER" &
  BG_PID=$!
  wait "$BG_PID" 2>/dev/null || true
  unset BG_PID
  exit 1
fi

while true; do
  sleep 60 &
  BG_PID=$!
  wait "$BG_PID" 2>/dev/null || true
done
