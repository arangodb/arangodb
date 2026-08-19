#!/bin/bash
# Pack the object files of a static enterprise build into a tar.gz so users
# can relink the static executables against a newer glibc (BUSL/static
# linking compliance). Runs right after "make install", inside the build
# image (it needs the
# static OpenSSL libraries from /opt).
#
# Archive layout: everything below the build directory
# appears under "build/" (link_executables.sh and README.static-linking
# rely on that name), plus lib/BuildId/BuildId.ld and the two helper files.
#
# Run from the repository root; requires:
#   BUILD_DIR        build directory of the preset (e.g. build-presets/nightly-package-x64)
#   BUILDMODE        CMake build type used            (default RelWithDebInfo)
#   PACKAGES_OUT     output directory                 (default ./packages)
# ARANGODB_VERSION is derived from CMakeLists.txt when not already exported.

set -euo pipefail

PROJECT_DIR="$(pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUILD_DIR="${BUILD_DIR:?BUILD_DIR must point to the CMake build directory}"
BUILDMODE="${BUILDMODE:-RelWithDebInfo}"
PACKAGES_OUT="${PACKAGES_OUT:-${PROJECT_DIR}/packages}"
ARCH="$(uname -m)"

if [ ! -d "${PROJECT_DIR}/${BUILD_DIR}" ]; then
  echo "pack-object-files: ${PROJECT_DIR}/${BUILD_DIR} does not exist" >&2
  exit 1
fi

if [ -z "${ARANGODB_VERSION:-}" ]; then
  source "${SCRIPT_DIR}/../ci/helpers.bash"
  find_arangodb_version "${PROJECT_DIR}/CMakeLists.txt" > /dev/null
fi

# 4.0 has no V8 and therefore no thin archives to rewrite (3.12 rewrites
# V8's thin archives at this point); every .a in the build tree is already
# self-contained.

# The linking scripts reference ../../libssl.a / ../../libcrypto.a relative
# to the build directory.
cp -a "$(find /opt -name libssl.a | head -1)" "${BUILD_DIR}/"
cp -a "$(find /opt -name libcrypto.a | head -1)" "${BUILD_DIR}/"

INCLUSION_LIST="$(mktemp)"
trap 'rm -f "${INCLUSION_LIST}"' EXIT

find "${BUILD_DIR}" -name "*.a" > "${INCLUSION_LIST}"
for obj in arangovpack arangobackup arangosh arangodump \
           arangoexport arangorestore arangoimport arangod; do
  find "${BUILD_DIR}" -name "${obj}.cpp.o" >> "${INCLUSION_LIST}"
done
find "${BUILD_DIR}/client-tools" -name "*.cpp.o" >> "${INCLUSION_LIST}"
echo "lib/BuildId/BuildId.ld" >> "${INCLUSION_LIST}"

# ── Generate scripts/link_executables.sh from this very build ───────────────────
# The relink script shipped in the archive is generated from the link
# command lines of the build being packed (a hand-maintained copy proved
# to rot silently) - see generate_link_executables.py for the details.
# The clang major version stamped into the script must match the build
# toolchain; fail clearly if it cannot be determined from VERSIONS.
CLANG_MAJOR="$(grep -Po 'CLANG_LINUX "\K[0-9]+' "${PROJECT_DIR}/VERSIONS" || true)"
if [ -z "${CLANG_MAJOR}" ]; then
  echo "pack-object-files: cannot determine the clang major version (CLANG_LINUX) from ${PROJECT_DIR}/VERSIONS" >&2
  exit 1
fi
python3 "${SCRIPT_DIR}/generate_link_executables.py" "${PROJECT_DIR}" "${BUILD_DIR}" "${CLANG_MAJOR}" > scripts/link_executables.sh
chmod +x scripts/link_executables.sh

# The README's toolchain instructions follow the build's clang, like the
# generated script's.
sed -E "s/clang-[0-9]+/clang-${CLANG_MAJOR}/g; s/lld-[0-9]+/lld-${CLANG_MAJOR}/g" \
  "${SCRIPT_DIR}/README.static-linking" > README.static-linking
echo "scripts/link_executables.sh" >> "${INCLUSION_LIST}"
echo "README.static-linking" >> "${INCLUSION_LIST}"

mkdir -p "${PACKAGES_OUT}"
ARCHIVE="${PACKAGES_OUT}/arangodb4e-linux-object_files_${BUILDMODE}-${ARANGODB_VERSION}_${ARCH}.tar.gz"
rm -f "${ARCHIVE}"
tar -czf "${ARCHIVE}" \
  --transform "s|^${BUILD_DIR}|build|" \
  --files-from="${INCLUSION_LIST}"
ls -l "${ARCHIVE}"
