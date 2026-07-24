#!/bin/bash
# Build RPM packages (server, client, debuginfo) from an existing install tree.
# Uses rpmbuild with the spec template from scripts/packaging/rpm/.
#
# Run from the repository root; requires:
#   build/install/            populated install tree ("make install" output,
#                             unstripped, incl. starter and rclone)
#   EDITION                   enterprise | community      (default enterprise)
#   PACKAGE_STRIP             All | ExceptArangod | None  (default ExceptArangod)
#   PACKAGES_OUT              output directory            (default ./packages)

set -euo pipefail

PROJECT_DIR="$(pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EDITION="${EDITION:-enterprise}"
PACKAGE_STRIP="${PACKAGE_STRIP:-ExceptArangod}"
PACKAGES_OUT="${PACKAGES_OUT:-${PROJECT_DIR}/packages}"

if [ ! -d "${PROJECT_DIR}/build/install" ]; then
  echo "build-rpm: ${PROJECT_DIR}/build/install does not exist" >&2
  exit 1
fi

if [ -z "${ARANGODB_RPM_UPSTREAM:-}" ]; then
  source "${SCRIPT_DIR}/../ci/helpers.bash"
  find_arangodb_version "${PROJECT_DIR}/CMakeLists.txt" > /dev/null
fi

SOURCE="${SCRIPT_DIR}/rpm"
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

if [ "${EDITION}" = "enterprise" ]; then
  SPEC_IN="${SOURCE}/arangodb3e.spec.in"
else
  SPEC_IN="${SOURCE}/arangodb3.spec.in"
fi

# transformSpec
SPEC="${WORK}/arangodb3.spec"
cp "${SPEC_IN}" "${SPEC}"
sed -i -e "s/@PACKAGE_VERSION@/${ARANGODB_RPM_UPSTREAM}/" "${SPEC}"
sed -i -e "s/@PACKAGE_REVISION@/${ARANGODB_RPM_REVISION}/" "${SPEC}"
case "${PACKAGE_STRIP}" in
  All)
    sed -i -e "s/@RPM_STRIP_ALL@//"              "${SPEC}"
    sed -i -e "s/@RPM_STRIP_EXCEPT_ARANGOD@/# /" "${SPEC}"
    sed -i -e "s/@RPM_STRIP_NONE@/# /"           "${SPEC}"
    ;;
  ExceptArangod)
    sed -i -e "s/@RPM_STRIP_ALL@/# /"            "${SPEC}"
    sed -i -e "s/@RPM_STRIP_EXCEPT_ARANGOD@//"   "${SPEC}"
    sed -i -e "s/@RPM_STRIP_NONE@/# /"           "${SPEC}"
    ;;
  *)
    sed -i -e "s/@RPM_STRIP_ALL@/# /"            "${SPEC}"
    sed -i -e "s/@RPM_STRIP_EXCEPT_ARANGOD@/# /" "${SPEC}"
    sed -i -e "s/@RPM_STRIP_NONE@//"             "${SPEC}"
    ;;
esac
sed -i -e "s~@JS_DIR@~~" "${SPEC}"

# The spec's %post copies these out of usr/share/arangodb3 at install time.
cp "${SOURCE}/arangodb3.initd" "${SOURCE}/arangodb3.service" "${SOURCE}/arangodb3.logrotate" \
  "${PROJECT_DIR}/build/install/usr/share/arangodb3/"

# The spec references the install tree as $INNERWORKDIR/ArangoDB/build/install
# (historical layout the spec was written for); provide that view via a symlink.
export INNERWORKDIR="${WORK}"
ln -s "${PROJECT_DIR}" "${WORK}/ArangoDB"

TOPDIR="${WORK}/rpmbuild"
mkdir -p "${TOPDIR}"
rpmbuild -bb -vv \
  --define "_topdir ${TOPDIR}" \
  --define "_binary_filedigest_algorithm 8" \
  "${SPEC}"

mkdir -p "${PACKAGES_OUT}"
mv "${TOPDIR}"/RPMS/*/*.rpm "${PACKAGES_OUT}/"
ls -l "${PACKAGES_OUT}"
