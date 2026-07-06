#!/bin/bash
# Build Debian packages (server, -client, -dbg) from an existing install tree.
# Bash port of oskar's buildDebianPackage (helper.linux.fish) +
# scripts/buildDebianPackage.fish, using the templates vendored in
# scripts/packaging/debian/.
#
# Run from the repository root; requires:
#   build/install/            populated install tree ("make install" output,
#                             unstripped, incl. starter and rclone)
#   EDITION                   enterprise | community      (default enterprise)
#   PACKAGE_STRIP             All | ExceptArangod | None  (default ExceptArangod)
#   PACKAGES_OUT              output directory            (default ./packages)
# Version variables (ARANGODB_DEBIAN_UPSTREAM/-_REVISION) are derived from
# CMakeLists.txt via scripts/ci/helpers.bash when not already exported.

set -euo pipefail

PROJECT_DIR="$(pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EDITION="${EDITION:-enterprise}"
PACKAGE_STRIP="${PACKAGE_STRIP:-ExceptArangod}"
PACKAGES_OUT="${PACKAGES_OUT:-${PROJECT_DIR}/packages}"

if [ ! -d "${PROJECT_DIR}/build/install" ]; then
  echo "build-deb: ${PROJECT_DIR}/build/install does not exist" >&2
  exit 1
fi

if [ -z "${ARANGODB_DEBIAN_UPSTREAM:-}" ]; then
  source "${SCRIPT_DIR}/../ci/helpers.bash"
  find_arangodb_version "${PROJECT_DIR}/CMakeLists.txt" > /dev/null
fi

SOURCE="${SCRIPT_DIR}/debian"
TARGET="${PROJECT_DIR}/debian"
V="${ARANGODB_DEBIAN_UPSTREAM}-${ARANGODB_DEBIAN_REVISION}"
ARCH="$(dpkg --print-architecture)"

if [ "${EDITION}" = "enterprise" ]; then
  echo "Building enterprise edition debian package..."
  PKG_NAME=arangodb3e
  EDITIONFOLDER="${SOURCE}/enterprise"
else
  echo "Building community edition debian package..."
  PKG_NAME=arangodb3
  EDITIONFOLDER="${SOURCE}/community"
fi

rm -rf "${TARGET}"
cp -a "${EDITIONFOLDER}" "${TARGET}"
cp -a "${SOURCE}/common/source" "${TARGET}"

for f in arangodb3.init arangodb3.service compat config templates preinst prerm postinst postrm rules; do
  cp "${SOURCE}/common/${f}" "${TARGET}/${f}"
  sed -i -e "s/@EDITION@/${PKG_NAME}/g" "${TARGET}/${f}"
  case "${PACKAGE_STRIP}" in
    All)
      sed -i -e "s/@DEBIAN_STRIP_ALL@//"                 "${TARGET}/${f}"
      sed -i -e "s/@DEBIAN_STRIP_EXCEPT_ARANGOD@/echo /" "${TARGET}/${f}"
      sed -i -e "s/@DEBIAN_STRIP_NONE@/echo /"           "${TARGET}/${f}"
      ;;
    ExceptArangod)
      sed -i -e "s/@DEBIAN_STRIP_ALL@/echo /"            "${TARGET}/${f}"
      sed -i -e "s/@DEBIAN_STRIP_EXCEPT_ARANGOD@//"      "${TARGET}/${f}"
      sed -i -e "s/@DEBIAN_STRIP_NONE@/echo /"           "${TARGET}/${f}"
      ;;
    *)
      sed -i -e "s/@DEBIAN_STRIP_ALL@/echo /"            "${TARGET}/${f}"
      sed -i -e "s/@DEBIAN_STRIP_EXCEPT_ARANGOD@/echo /" "${TARGET}/${f}"
      sed -i -e "s/@DEBIAN_STRIP_NONE@//"                "${TARGET}/${f}"
      ;;
  esac
done

{
  echo "${PKG_NAME} (${V}) UNRELEASED; urgency=medium"
  echo
  echo "  * New version."
  echo
  echo -n " -- ArangoDB <hackers@arangodb.com>  "
  date -R
} > "${TARGET}/changelog"

sed -i "s/@ARCHITECTURE@/${ARCH}/g" "${TARGET}/control"

# debian/rules binary writes the .deb files into the parent directory.
unset LC_ALL LC_CTYPE LANG LANGUAGE || true
cd "${PROJECT_DIR}"
debian/rules binary

mkdir -p "${PACKAGES_OUT}"
mv "${PROJECT_DIR}"/../${PKG_NAME}*_"${V}"_*.deb "${PACKAGES_OUT}/"
ls -l "${PACKAGES_OUT}"
