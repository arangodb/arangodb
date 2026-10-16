#!/bin/bash
# Build the server and client TAR.GZ bundles from an existing install tree.
# Uses the launcher wrappers from scripts/packaging/binForTarGz/.
#
# Run from the repository root; requires:
#   build/install/            populated install tree ("make install" output,
#                             unstripped, incl. starter and rclone)
#   ARCH                      x86_64 | arm64              (default: uname -m)
#   EDITION                   enterprise | community      (default enterprise)
#   PACKAGE_STRIP             All | ExceptArangod | None  (default ExceptArangod)
#   PACKAGES_OUT              output directory            (default ./packages)

set -euo pipefail

PROJECT_DIR="$(pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EDITION="${EDITION:-enterprise}"
PACKAGE_STRIP="${PACKAGE_STRIP:-ExceptArangod}"
PACKAGES_OUT="${PACKAGES_OUT:-${PROJECT_DIR}/packages}"
ARCH="${ARCH:-$(uname -m)}"

if [ ! -d "${PROJECT_DIR}/build/install" ]; then
  echo "build-targz: ${PROJECT_DIR}/build/install does not exist" >&2
  exit 1
fi

if [ -z "${ARANGODB_TGZ_UPSTREAM:-}" ]; then
  source "${SCRIPT_DIR}/../ci/helpers.bash"
  find_arangodb_version "${PROJECT_DIR}/CMakeLists.txt" > /dev/null
fi

case "${ARCH}" in
  x86_64|amd64)   ARCH_SUFFIX="_x86_64" ;;
  arm64|aarch64)  ARCH_SUFFIX="_arm64" ;;
  *)
    echo "build-targz: fatal, unknown architecture ${ARCH} for TGZ" >&2
    exit 1
    ;;
esac

if [ "${EDITION}" = "enterprise" ]; then
  NAME=arangodb3e
else
  NAME=arangodb3
fi

V="${ARANGODB_TGZ_UPSTREAM}"
OS=linux

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

rm -rf "${WORK}/targz"
mkdir -p "${WORK}/targz"
cp -a "${PROJECT_DIR}/build/install/." "${WORK}/targz/"

cd "${WORK}/targz"
rm -rf bin
cp -a "${SCRIPT_DIR}/binForTarGz" bin
find bin \( -name "*.bak" -o -name "*~" \) -delete
cp "bin/README.${OS}.server" ./README
sed -i -E "s/@ARANGODB_PACKAGE_NAME@/${NAME}-${OS}-${V}${ARCH_SUFFIX}/g" README
# prepareInstall: strip client tools per policy; arangod stays unstripped.
"${SCRIPT_DIR}/strip-install-tree.sh" "${WORK}/targz"

mkdir -p "${PACKAGES_OUT}"

# Server bundle
cd "${WORK}"
cp -a targz "${NAME}-${OS}-${V}${ARCH_SUFFIX}"
tar czf "${PACKAGES_OUT}/${NAME}-${OS}-${V}${ARCH_SUFFIX}.tar.gz" \
  --exclude "usr/local" \
  --exclude "etc" \
  --exclude "bin/README*" \
  --exclude "var" \
  "${NAME}-${OS}-${V}${ARCH_SUFFIX}"
rm -rf "${NAME}-${OS}-${V}${ARCH_SUFFIX}"

# Client bundle
CLIENT="${NAME}-client-${OS}-${V}${ARCH_SUFFIX}"
cp -a targz "${CLIENT}"
cp "${CLIENT}/bin/README.${OS}.client" "${CLIENT}/README"
sed -i -E "s/@ARANGODB_PACKAGE_NAME@/${CLIENT}/g" "${CLIENT}/README"
tar czf "${PACKAGES_OUT}/${CLIENT}.tar.gz" \
  --exclude "usr/local" \
  --exclude "bin/README*" \
  --exclude "etc" \
  --exclude "var" \
  --exclude "*.initd" \
  --exclude "*.services" \
  --exclude "*.logrotate" \
  --exclude "arangodb.8" \
  --exclude "arangod.8" \
  --exclude "arango-dfdb.8" \
  --exclude "rcarangod.8" \
  --exclude "${CLIENT}/sbin" \
  --exclude "${CLIENT}/bin/arangod" \
  --exclude "${CLIENT}/bin/arangodb" \
  --exclude "${CLIENT}/usr/sbin" \
  --exclude "${CLIENT}/usr/bin/arangodb" \
  --exclude "${CLIENT}/usr/share/arangodb3/arangodb-update-db" \
  --exclude "${CLIENT}/usr/share/arangodb3/js/server" \
  "${CLIENT}"
rm -rf "${CLIENT}"

ls -l "${PACKAGES_OUT}"
