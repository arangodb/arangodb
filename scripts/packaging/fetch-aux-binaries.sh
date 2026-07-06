#!/bin/bash
# Download the Starter and (enterprise only) rclone binaries into the install
# tree, at the same locations oskar's downloadStarter/downloadRclone use:
#   Starter → build/install/usr/bin/arangodb
#   rclone  → build/install/usr/sbin/rclone-arangodb
# Versions are pinned by the VERSIONS file. Updates sourceInfo.log when
# SOURCE_INFO_DIR is set.
#
# Run from the repository root; requires:
#   build/install/            populated install tree
#   ARCH                      x86_64 | amd64 | arm64 | aarch64 (default: uname -m)
#   EDITION                   enterprise | community           (default enterprise)

set -euo pipefail

PROJECT_DIR="$(pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EDITION="${EDITION:-enterprise}"
ARCH="${ARCH:-$(uname -m)}"

source "${SCRIPT_DIR}/../ci/helpers.bash"
if [ -z "${ARANGODB_VERSION_MAJOR_MINOR:-}" ]; then
  find_arangodb_version "${PROJECT_DIR}/CMakeLists.txt" > /dev/null
fi

case "${ARCH}" in
  x86_64|amd64)  DL_ARCH="amd64" ;;
  arm64|aarch64) DL_ARCH="arm64" ;;
  *)
    echo "fetch-aux-binaries: unknown architecture ${ARCH}" >&2
    exit 1
    ;;
esac

update_source_info() {
  if [ -n "${SOURCE_INFO_DIR:-}" ]; then
    setup_source_info "$1" "$2"
  fi
}

STARTER_REV="$(grep -Po 'STARTER_REV "\Kv[0-9.a-z-]+' "${PROJECT_DIR}/VERSIONS")"
echo "Downloading Starter ${STARTER_REV} (${DL_ARCH})"
if curl -fsSL -o "${PROJECT_DIR}/build/install/usr/bin/arangodb" \
  "https://github.com/arangodb-helper/arangodb/releases/download/${STARTER_REV}/arangodb-linux-${DL_ARCH}"; then
  chmod 755 "${PROJECT_DIR}/build/install/usr/bin/arangodb"
  update_source_info "Starter" "${STARTER_REV}"
else
  echo "ERROR - cannot download Starter" >&2
  update_source_info "Starter" "N/A"
  exit 1
fi

if [ "${EDITION}" = "enterprise" ]; then
  USE_RCLONE="$(grep -Po 'USE_RCLONE "\K[a-z]+' "${PROJECT_DIR}/VERSIONS" || echo "false")"
  if [ "${USE_RCLONE}" = "true" ]; then
    RCLONE_GO="$(grep -Po 'RCLONE_GO "\K[0-9.]+-[0-9a-h]+_[0-9]+' "${PROJECT_DIR}/VERSIONS")"
    RCLONE_VERSION="$(grep -Po 'RCLONE_VERSION "\K[0-9.]+' "${PROJECT_DIR}/VERSIONS")"
    RCLONE_RELEASE="golang-${RCLONE_GO}_${ARANGODB_VERSION_MAJOR_MINOR}_v${RCLONE_VERSION}"
    echo "Downloading rclone ${RCLONE_RELEASE} (${DL_ARCH})"
    mkdir -p "${PROJECT_DIR}/build/install/usr/sbin"
    if curl -fsSL -o "${PROJECT_DIR}/build/install/usr/sbin/rclone-arangodb" \
      "https://github.com/arangodb/rclone-arangodb/releases/download/golang-${RCLONE_GO}/${RCLONE_RELEASE}_rclone-arangodb-linux-${DL_ARCH}"; then
      chmod 755 "${PROJECT_DIR}/build/install/usr/sbin/rclone-arangodb"
      update_source_info "Rclone" "${RCLONE_RELEASE}"
    else
      echo "ERROR - cannot download Rclone" >&2
      update_source_info "Rclone" "N/A"
      exit 1
    fi
  else
    echo "USE_RCLONE is not enabled in VERSIONS, skipping rclone download"
  fi
fi
