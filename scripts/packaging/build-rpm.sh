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

# Preflight: the strip/verify toolchain must be complete. eu-strip is not
# called by the spec itself (it strips explicitly with objcopy/strip, see
# arangodb3e.spec.in), but a build image without eu-strip is one where
# rpm's own debuginfo machinery is silently broken — that once shipped
# unstripped client tools and an empty debuginfo package. Refuse to build
# on such an image instead of relying on the machinery staying unused.
for tool in eu-strip objcopy strip file readelf rpm2cpio cpio cmp; do
  if ! command -v "${tool}" > /dev/null; then
    echo "build-rpm: required tool '${tool}' is missing from the build image" >&2
    exit 1
  fi
done

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

# ── Verify the strip policy on the built packages ────────────────────────────
# The outcome is asserted instead of trusting the toolchain: rpm's strip
# machinery has failed silently before (find-debuginfo was a no-op without
# eu-strip while brp-strip -g'ed every binary regardless of exec bits), and
# wrong packages must fail the build here, not ship and fail RTA later.
# Policy: client tools stripped with a .gnu_debuglink and their .debug in
# the debuginfo package; arangod byte-identical to the install tree
# (minimal debug info) unless PACKAGE_STRIP=All; the starter and rclone
# always byte-identical; PACKAGE_STRIP=None leaves every binary untouched.
CLIENT_TOOLS="arangobackup arangobench arangodump arangoexport arangoimport arangorestore arangosh arangovpack"

SERVER_RPM="$(ls "${PACKAGES_OUT}"/*.rpm | grep -v -e '-client-' -e '-debuginfo-')"
DEBUG_RPM="$(ls "${PACKAGES_OUT}"/*-debuginfo-*.rpm)"

VER_TREE="${WORK}/verify"
mkdir -p "${VER_TREE}/main" "${VER_TREE}/debug"
( cd "${VER_TREE}/main"  && rpm2cpio "${SERVER_RPM}" | cpio -idm --quiet )
( cd "${VER_TREE}/debug" && rpm2cpio "${DEBUG_RPM}"  | cpio -idm --quiet )

fail_verify() { echo "build-rpm: STRIP POLICY VIOLATION: $*" >&2; exit 1; }

expect_untouched() {
  if ! cmp -s "${PROJECT_DIR}/build/install/$1" "${VER_TREE}/main/$1"; then
    fail_verify "$1 differs from the install tree but must not be touched"
  fi
}

expect_stripped() {
  local f="${VER_TREE}/main/$1"
  if [ ! -f "${f}" ]; then
    fail_verify "$1 is missing from the package"
  fi
  if file -b "${f}" | grep -q ", not stripped"; then
    fail_verify "$1 must be stripped, but it is not"
  fi
  if ! readelf -S "${f}" | grep -q '\.gnu_debuglink'; then
    fail_verify "$1 lacks the .gnu_debuglink to its debug file"
  fi
  if [ ! -f "${VER_TREE}/debug/usr/lib/debug/$1.debug" ]; then
    fail_verify "the debuginfo package lacks usr/lib/debug/$1.debug"
  fi
}

expect_untouched usr/bin/arangodb
expect_untouched usr/sbin/rclone-arangodb
case "${PACKAGE_STRIP}" in
  All)
    expect_stripped usr/sbin/arangod
    ;;
  ExceptArangod)
    expect_untouched usr/sbin/arangod
    ;;
  *)
    expect_untouched usr/sbin/arangod
    ;;
esac
for tool in ${CLIENT_TOOLS}; do
  [ -f "${PROJECT_DIR}/build/install/usr/bin/${tool}" ] || continue
  if [ "${PACKAGE_STRIP}" = "All" ] || [ "${PACKAGE_STRIP}" = "ExceptArangod" ]; then
    expect_stripped "usr/bin/${tool}"
  else
    expect_untouched "usr/bin/${tool}"
  fi
done
echo "build-rpm: strip policy verified (PACKAGE_STRIP=${PACKAGE_STRIP})"
