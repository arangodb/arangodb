#!/bin/bash
# Bash port of selected oskar fish-shell helpers.
# Source this file; do not execute it directly.
#
# Usage:
#   source scripts/oskar.bash
#   set_nightly_version /path/to/CMakeLists.txt   # modifies file in-place,
#                                                 # then calls find_arangodb_version
#   # All ARANGODB_* and DOCKER_TAG variables are now exported.

# ---------------------------------------------------------------------------
# find_arangodb_version [<CMakeLists.txt>]
#
# Reads version components from CMakeLists.txt, assembles every variable that
# oskar's findArangoDBVersion sets, exports them all, and prints them.
# Defaults to ../CMakeLists.txt relative to this script when no path given.
#
# Exported variables:
#   ARANGODB_VERSION                  e.g.  3.12.10-devel
#   ARANGODB_VERSION_MAJOR            e.g.  3
#   ARANGODB_VERSION_MINOR            e.g.  12
#   ARANGODB_VERSION_PATCH            e.g.  10
#   ARANGODB_VERSION_RELEASE_TYPE     e.g.  devel        (empty for stable)
#   ARANGODB_VERSION_RELEASE_NUMBER   e.g.  20260602     (empty when no number)
#   ARANGODB_PLAIN_VERSION            e.g.  3.12.10
#   ARANGODB_VERSION_MAJOR_MINOR      e.g.  3.12
#   ARANGODB_DARWIN_UPSTREAM          e.g.  3.12.10.devel
#   ARANGODB_DARWIN_REVISION          e.g.  devel
#   ARANGODB_DEBIAN_UPSTREAM          e.g.  3.12.10~~devel
#   ARANGODB_DEBIAN_REVISION          e.g.  1
#   ARANGODB_PACKAGES                 e.g.  3.12
#   ARANGODB_REPO                     e.g.  nightly      (stable for releases)
#   ARANGODB_RPM_UPSTREAM             e.g.  3.12.10
#   ARANGODB_RPM_REVISION             e.g.  0.1          (1 for stable)
#   ARANGODB_SNIPPETS                 e.g.  3.12
#   ARANGODB_SNAP_REVISION            e.g.  1
#   ARANGODB_TGZ_UPSTREAM             e.g.  3.12.10-devel
#   DOCKER_TAG                        e.g.  3.12.10-devel
# ---------------------------------------------------------------------------
find_arangodb_version() {
  local _default_cmake
  _default_cmake="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../CMakeLists.txt"
  local cmake_file="${1:-}"
  if [ -z "${cmake_file}" ]; then
    [ -f "${_default_cmake}" ] || { echo "find_arangodb_version: CMakeLists.txt not found at ${_default_cmake}" >&2; return 1; }
    cmake_file="${_default_cmake}"
  fi

  # Read raw components from CMakeLists.txt
  ARANGODB_VERSION_MAJOR="$(sed -n 's/^[[:space:]]*set(ARANGODB_VERSION_MAJOR[[:space:]]*"\([^"]*\)").*/\1/p' "${cmake_file}")"
  ARANGODB_VERSION_MINOR="$(sed -n 's/^[[:space:]]*set(ARANGODB_VERSION_MINOR[[:space:]]*"\([^"]*\)").*/\1/p' "${cmake_file}")"
  ARANGODB_VERSION_PATCH="$(sed -n 's/^[[:space:]]*set(ARANGODB_VERSION_PATCH[[:space:]]*"\([^"]*\)").*/\1/p' "${cmake_file}")"
  ARANGODB_VERSION_RELEASE_TYPE="$(sed -n 's/^[[:space:]]*set(ARANGODB_VERSION_RELEASE_TYPE[[:space:]]*"\([^"]*\)").*/\1/p' "${cmake_file}" | head -1)"
  ARANGODB_VERSION_RELEASE_NUMBER="$(sed -n 's/^[[:space:]]*set(ARANGODB_VERSION_RELEASE_NUMBER[[:space:]]*"\([^"]*\)").*/\1/p' "${cmake_file}" | head -1)"
  ARANGODB_SNAP_REVISION="$(sed -n 's/^[[:space:]]*set(ARANGODB_SNAP_REVISION[[:space:]]*"\([^"]*\)").*/\1/p' "${cmake_file}")"

  # Plain and major.minor
  ARANGODB_PLAIN_VERSION="${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.${ARANGODB_VERSION_PATCH}"
  ARANGODB_VERSION_MAJOR_MINOR="${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}"

  # Full semantic version (mirrors cmake logic)
  if [ -n "${ARANGODB_VERSION_RELEASE_TYPE}" ]; then
    if [ -n "${ARANGODB_VERSION_RELEASE_NUMBER}" ]; then
      ARANGODB_VERSION="${ARANGODB_PLAIN_VERSION}-${ARANGODB_VERSION_RELEASE_TYPE}.${ARANGODB_VERSION_RELEASE_NUMBER}"
    else
      ARANGODB_VERSION="${ARANGODB_PLAIN_VERSION}-${ARANGODB_VERSION_RELEASE_TYPE}"
    fi
  else
    ARANGODB_VERSION="${ARANGODB_PLAIN_VERSION}"
  fi

  # Darwin: separator '-' → '.'; revision = full release qualifier
  if [ -n "${ARANGODB_VERSION_RELEASE_TYPE}" ]; then
    if [ -n "${ARANGODB_VERSION_RELEASE_NUMBER}" ]; then
      ARANGODB_DARWIN_UPSTREAM="${ARANGODB_PLAIN_VERSION}.${ARANGODB_VERSION_RELEASE_TYPE}.${ARANGODB_VERSION_RELEASE_NUMBER}"
      ARANGODB_DARWIN_REVISION="${ARANGODB_VERSION_RELEASE_TYPE}.${ARANGODB_VERSION_RELEASE_NUMBER}"
    else
      ARANGODB_DARWIN_UPSTREAM="${ARANGODB_PLAIN_VERSION}.${ARANGODB_VERSION_RELEASE_TYPE}"
      ARANGODB_DARWIN_REVISION="${ARANGODB_VERSION_RELEASE_TYPE}"
    fi
  else
    ARANGODB_DARWIN_UPSTREAM="${ARANGODB_PLAIN_VERSION}"
    ARANGODB_DARWIN_REVISION=""
  fi

  # Debian: separator '-' → '~~' (double tilde for correct dpkg pre-release ordering)
  if [ -n "${ARANGODB_VERSION_RELEASE_TYPE}" ]; then
    if [ -n "${ARANGODB_VERSION_RELEASE_NUMBER}" ]; then
      ARANGODB_DEBIAN_UPSTREAM="${ARANGODB_PLAIN_VERSION}~~${ARANGODB_VERSION_RELEASE_TYPE}.${ARANGODB_VERSION_RELEASE_NUMBER}"
    else
      ARANGODB_DEBIAN_UPSTREAM="${ARANGODB_PLAIN_VERSION}~~${ARANGODB_VERSION_RELEASE_TYPE}"
    fi
  else
    ARANGODB_DEBIAN_UPSTREAM="${ARANGODB_PLAIN_VERSION}"
  fi
  ARANGODB_DEBIAN_REVISION="1"

  # RPM: upstream = plain version only (no release suffix);
  # revision = 0.1 for pre-releases so they sort before the stable -1.
  ARANGODB_RPM_UPSTREAM="${ARANGODB_PLAIN_VERSION}"
  if [ -n "${ARANGODB_VERSION_RELEASE_TYPE}" ]; then
    ARANGODB_RPM_REVISION="0.1"
  else
    ARANGODB_RPM_REVISION="1"
  fi

  # Repository, packages, snippets
  if [ -n "${ARANGODB_VERSION_RELEASE_TYPE}" ]; then
    ARANGODB_REPO="nightly"
  else
    ARANGODB_REPO="stable"
  fi
  ARANGODB_PACKAGES="${ARANGODB_VERSION_MAJOR_MINOR}"
  ARANGODB_SNIPPETS="${ARANGODB_VERSION_MAJOR_MINOR}"

  # TGZ and Docker
  ARANGODB_TGZ_UPSTREAM="${ARANGODB_VERSION}"
  DOCKER_TAG="${ARANGODB_VERSION}"

  export ARANGODB_VERSION
  export ARANGODB_VERSION_MAJOR ARANGODB_VERSION_MINOR ARANGODB_VERSION_PATCH
  export ARANGODB_VERSION_RELEASE_TYPE ARANGODB_VERSION_RELEASE_NUMBER
  export ARANGODB_PLAIN_VERSION ARANGODB_VERSION_MAJOR_MINOR
  export ARANGODB_DARWIN_UPSTREAM ARANGODB_DARWIN_REVISION
  export ARANGODB_DEBIAN_UPSTREAM ARANGODB_DEBIAN_REVISION
  export ARANGODB_PACKAGES ARANGODB_REPO
  export ARANGODB_RPM_UPSTREAM ARANGODB_RPM_REVISION
  export ARANGODB_SNIPPETS ARANGODB_SNAP_REVISION
  export ARANGODB_TGZ_UPSTREAM DOCKER_TAG

  echo "ARANGODB_VERSION:                  ${ARANGODB_VERSION}"
  echo ""
  echo "ARANGODB_VERSION_MAJOR:            ${ARANGODB_VERSION_MAJOR}"
  echo "ARANGODB_VERSION_MINOR:            ${ARANGODB_VERSION_MINOR}"
  echo "ARANGODB_VERSION_PATCH:            ${ARANGODB_VERSION_PATCH}"
  echo "ARANGODB_VERSION_RELEASE_TYPE:     ${ARANGODB_VERSION_RELEASE_TYPE}"
  echo "ARANGODB_VERSION_RELEASE_NUMBER:   ${ARANGODB_VERSION_RELEASE_NUMBER}"
  echo ""
  echo "ARANGODB_DARWIN_UPSTREAM/REVISION: ${ARANGODB_DARWIN_UPSTREAM} / ${ARANGODB_DARWIN_REVISION}"
  echo "ARANGODB_DEBIAN_UPSTREAM/REVISION: ${ARANGODB_DEBIAN_UPSTREAM} / ${ARANGODB_DEBIAN_REVISION}"
  echo "ARANGODB_PACKAGES:                 ${ARANGODB_PACKAGES}"
  echo "ARANGODB_REPO:                     ${ARANGODB_REPO}"
  echo "ARANGODB_RPM_UPSTREAM/REVISION:    ${ARANGODB_RPM_UPSTREAM} / ${ARANGODB_RPM_REVISION}"
  echo "ARANGODB_SNIPPETS:                 ${ARANGODB_SNIPPETS}"
  echo "ARANGODB_SNAP_REVISION:            ${ARANGODB_SNAP_REVISION}"
  echo "ARANGODB_TGZ_UPSTREAM:             ${ARANGODB_TGZ_UPSTREAM}"
  echo "DOCKER_TAG:                        ${DOCKER_TAG}"
}

# ---------------------------------------------------------------------------
# set_nightly_version [<CMakeLists.txt>]
#
# Modifies CMakeLists.txt in-place to turn a devel tree into a nightly build:
#   ARANGODB_VERSION_RELEASE_TYPE   "devel"  →  "nightly"
#   ARANGODB_VERSION_RELEASE_NUMBER ""       →  YYYYMMDD
#
# Defaults to ../CMakeLists.txt relative to this script when no path given.
# Calls find_arangodb_version afterwards so all ARANGODB_* variables are
# immediately available to the caller.
# ---------------------------------------------------------------------------
set_nightly_version() {
  local _default_cmake
  _default_cmake="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../CMakeLists.txt"
  local cmake_file="${1:-}"
  if [ -z "${cmake_file}" ]; then
    [ -f "${_default_cmake}" ] || { echo "set_nightly_version: CMakeLists.txt not found at ${_default_cmake}" >&2; return 1; }
    cmake_file="${_default_cmake}"
  fi

  local date_str
  date_str="$(date +%Y%m%d)"

  sed -i \
    -e "s/set(ARANGODB_VERSION_RELEASE_TYPE \"devel\")/set(ARANGODB_VERSION_RELEASE_TYPE \"nightly\")/" \
    -e "s/set(ARANGODB_VERSION_RELEASE_NUMBER \"\")/set(ARANGODB_VERSION_RELEASE_NUMBER \"${date_str}\")/" \
    "${cmake_file}"

  find_arangodb_version "${cmake_file}"
}
