#!/bin/bash
# Strip an install tree the way shipped packages are stripped.
# Client tools are stripped, arangod and the starter stay unstripped
# (arangod was only ever stripped for <= 3.10).
# Used by build-targz.sh and by the nightly Docker image job.
#
# Usage: strip-install-tree.sh <install-tree>
#   EDITION        enterprise | community      (default enterprise)
#   PACKAGE_STRIP  All | ExceptArangod | None  (default ExceptArangod)

set -euo pipefail

EDITION="${EDITION:-enterprise}"
PACKAGE_STRIP="${PACKAGE_STRIP:-ExceptArangod}"

if [ $# -ne 1 ] || [ ! -d "$1" ]; then
  echo "strip-install-tree: need an existing install tree as first argument" >&2
  exit 1
fi

cd "$1"

case "${PACKAGE_STRIP}" in
  All|ExceptArangod)
    strip usr/bin/arangodump usr/bin/arangoexport usr/bin/arangoimport \
          usr/bin/arangorestore usr/bin/arangosh usr/bin/arangovpack \
          usr/bin/arangobench
    ;;
esac

if [ "${EDITION}" != "enterprise" ]; then
  rm -f bin/arangobackup usr/bin/arangobackup usr/sbin/arangobackup
elif [ -f usr/bin/arangobackup ] && [ "${PACKAGE_STRIP}" != "None" ]; then
  strip usr/bin/arangobackup
fi
