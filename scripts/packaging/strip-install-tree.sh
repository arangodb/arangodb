#!/bin/bash
# Strip an install tree the way shipped packages are stripped, optionally
# extracting each stripped binary's debug information first into a
# GDB-style usr/lib/debug/.build-id/ tree (objcopy --only-keep-debug,
# strip, --add-gnu-debuglink; the GNU build-id is left intact).
# Client tools are stripped, arangod and the starter stay unstripped;
# only PACKAGE_STRIP=All strips arangod too.
# Used by build-targz.sh and by the nightly Docker image job.
#
# Usage: strip-install-tree.sh <install-tree>
#   EDITION            enterprise | community      (default enterprise)
#   PACKAGE_STRIP      All | ExceptArangod | None  (default ExceptArangod)
#   DEBUG_SYMBOLS_DIR  optional: extract debug info of every stripped
#                      binary into this directory before stripping

set -euo pipefail

EDITION="${EDITION:-enterprise}"
PACKAGE_STRIP="${PACKAGE_STRIP:-ExceptArangod}"
DEBUG_SYMBOLS_DIR="${DEBUG_SYMBOLS_DIR:-}"

if [ $# -ne 1 ] || [ ! -d "$1" ]; then
  echo "strip-install-tree: need an existing install tree as first argument" >&2
  exit 1
fi

cd "$1"

# Extract debug info (when DEBUG_SYMBOLS_DIR is set), then strip; the
# debug file is keyed by GNU build-id, by name if the binary has none.
extract_and_strip() {
  local bin="$1"
  if [ -z "${DEBUG_SYMBOLS_DIR}" ]; then
    strip "${bin}"
    return
  fi
  local id dbgfile
  id="$(readelf -n "${bin}" 2>/dev/null | sed -n 's/.*Build ID: \([0-9a-f]\{40\}\).*/\1/p' | head -n1)"
  if [ -n "${id}" ]; then
    dbgfile="${DEBUG_SYMBOLS_DIR}/usr/lib/debug/.build-id/${id:0:2}/${id:2}.debug"
  else
    dbgfile="${DEBUG_SYMBOLS_DIR}/usr/lib/debug/$(basename "${bin}").debug"
  fi
  mkdir -p "$(dirname "${dbgfile}")"
  objcopy --only-keep-debug "${bin}" "${dbgfile}"
  chmod 644 "${dbgfile}"
  strip "${bin}"
  objcopy --add-gnu-debuglink="${dbgfile}" "${bin}"
}

case "${PACKAGE_STRIP}" in
  All|ExceptArangod)
    for tool in arangobench arangodump arangoexport arangoimport \
                arangorestore arangosh arangovpack; do
      extract_and_strip "usr/bin/${tool}"
    done
    ;;
esac

if [ "${PACKAGE_STRIP}" = "All" ]; then
  extract_and_strip usr/sbin/arangod
fi

if [ "${EDITION}" != "enterprise" ]; then
  rm -f bin/arangobackup usr/bin/arangobackup usr/sbin/arangobackup
elif [ -f usr/bin/arangobackup ] && [ "${PACKAGE_STRIP}" != "None" ]; then
  extract_and_strip usr/bin/arangobackup
fi
