#!/bin/bash
# Bash port of selected oskar fish-shell helpers.
# Source this file; do not execute it directly.
#
# Usage:
#   source scripts/ci/helpers.bash
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
  # revision sorts pre-releases before the stable -1: devel = 0.1,
  # nightly = 0.2 (matching oskar), stable = 1.
  ARANGODB_RPM_UPSTREAM="${ARANGODB_PLAIN_VERSION}"
  if [ "${ARANGODB_VERSION_RELEASE_TYPE}" = "nightly" ]; then
    ARANGODB_RPM_REVISION="0.2"
  elif [ -n "${ARANGODB_VERSION_RELEASE_TYPE}" ]; then
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
# Modifies CMakeLists.txt in-place to turn any tree into a nightly build:
#   ARANGODB_VERSION_RELEASE_TYPE   <any>    →  "nightly"
#   ARANGODB_VERSION_RELEASE_NUMBER <any>    →  YYYYMMDD
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

  # NIGHTLY_DATE lets CI pin one date for all jobs of a workflow,
  # so builds spanning midnight still produce a single version.
  local date_str
  date_str="${NIGHTLY_DATE:-$(date +%Y%m%d)}"

  # Like oskar's setNightlyVersion: force "nightly" over ANY prior release
  # type (devel, rc, beta, or stable's "") so a nightly run of a release
  # branch can never produce stable-looking artifacts.
  sed -i \
    -e "s/set(ARANGODB_VERSION_RELEASE_TYPE .*/set(ARANGODB_VERSION_RELEASE_TYPE \"nightly\")/" \
    -e "s/set(ARANGODB_VERSION_RELEASE_NUMBER .*/set(ARANGODB_VERSION_RELEASE_NUMBER \"${date_str}\")/" \
    "${cmake_file}"

  find_arangodb_version "${cmake_file}"

  # oskar's setNightlyVersion also rewrote ARANGO-VERSION. CMake regenerates
  # it from lib/Basics/VERSION.in at configure time, but jobs that only patch
  # the version and never configure (packaging, docker) would otherwise keep
  # the stale checked-in content (e.g. 3.12.10-devel) in their checkout.
  echo "${ARANGODB_VERSION}" > "$(dirname "${cmake_file}")/ARANGO-VERSION"
}

# ---------------------------------------------------------------------------
# sourceInfo.log / sourceInfo.json helpers
#
# Bash ports of oskar's initSourceInfo / setupSourceInfo / convertSItoJSON.
# The former "oskar" field is now called "CI" and identifies the CI run that
# produced the artifacts. The log lives at
# ${SOURCE_INFO_DIR:-.}/sourceInfo.log; the JSON twin is regenerated on every
# update.
#
#   init_source_info [<ci-field-value>]
#   setup_source_info <field> <value>     # field: VERSION|Community|Starter|Enterprise|Rclone|CI
# ---------------------------------------------------------------------------
convert_source_info_to_json() {
  local dir="${SOURCE_INFO_DIR:-.}"
  local log="${dir}/sourceInfo.log"
  [ -f "${log}" ] || return 0

  local fields=""
  local var val
  while IFS= read -r line; do
    var="$(echo "${line}" | cut -f1 -d ':')"
    case "${var}" in
      CI|VERSION|Community|Starter|Enterprise|Rclone)
        val="$(echo "${line}" | cut -f2 -d ' ')"
        if [ -n "${val}" ]; then
          fields="${fields:+${fields},$'\n'}  \"${var}\":\"${val}\""
        fi
        ;;
    esac
  done < "${log}"

  if [ -n "${fields}" ]; then
    echo "convert ${log} to ${dir}/sourceInfo.json"
    printf '{\n%s\n}' "${fields}" > "${dir}/sourceInfo.json"
  fi
}

init_source_info() {
  local dir="${SOURCE_INFO_DIR:-.}"
  local ci_value="${1:-N/A}"
  mkdir -p "${dir}"
  {
    echo "CI: ${ci_value}"
    echo "VERSION: N/A"
    echo "Community: N/A"
    echo "Starter: N/A"
    echo "Enterprise: N/A"
    echo "Rclone: N/A"
  } > "${dir}/sourceInfo.log"
  convert_source_info_to_json
}

setup_source_info() {
  local field="$1"
  local value="$2"
  local dir="${SOURCE_INFO_DIR:-.}"
  sed -i -E "s|^${field}:.*\$|${field}: ${value}|g" "${dir}/sourceInfo.log"
  convert_source_info_to_json
}
