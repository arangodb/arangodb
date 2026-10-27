#!/bin/bash
# Strip the client tools in the debian/<pkg>{,-client} trees, extracting
# their debug info into the -dbg package first: /usr/lib/debug/<path>.debug
# (debuglink layout) plus a .build-id/<xx>/<rest>.debug symlink for
# build-id-only consumers (elfutils, perf). The GNU build-id is left intact.
# Whitelist only: arangod is stripped solely in All mode; the starter and
# rclone are never touched; symlinked tool names are skipped.
#
# Usage: strip-debian-packages.sh <edition-pkg-name> <All|ExceptArangod>

set -euo pipefail

EDITION_PKG="$1"
MODE="$2"

TOOLS="usr/bin/arangobackup usr/bin/arangobench usr/bin/arangodump
       usr/bin/arangoexport usr/bin/arangoimport usr/bin/arangorestore
       usr/bin/arangosh usr/bin/arangovpack"
if [ "${MODE}" = "All" ]; then
  TOOLS="${TOOLS} usr/sbin/arangod"
fi

for pkg in "${EDITION_PKG}" "${EDITION_PKG}-client"; do
  for rel in ${TOOLS}; do
    bin="debian/${pkg}/${rel}"
    { [ -f "${bin}" ] && [ ! -L "${bin}" ]; } || continue
    dbg="debian/${EDITION_PKG}-dbg/usr/lib/debug/${rel}.debug"
    mkdir -p "$(dirname "${dbg}")"
    objcopy --only-keep-debug "${bin}" "${dbg}"
    strip --remove-section=.comment --remove-section=.note "${bin}"
    objcopy --add-gnu-debuglink="${dbg}" "${bin}"
    id="$(readelf -n "${bin}" 2>/dev/null | sed -n 's/.*Build ID: \([0-9a-f]\{40\}\).*/\1/p' | head -n1)"
    if [ -n "${id}" ]; then
      link="debian/${EDITION_PKG}-dbg/usr/lib/debug/.build-id/${id:0:2}/${id:2}.debug"
      mkdir -p "$(dirname "${link}")"
      ln -sf "../../${rel}.debug" "${link}"
    fi
  done
done
