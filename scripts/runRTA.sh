#!/bin/bash

ARANGO_SOURCE="$(pwd)"
ulimit -n 65536

STARTER_REV=$(grep -Po "STARTER_REV \"\Kv\d+\.\d+\.\d+[a-z0-9-]*" "${ARANGO_SOURCE}/VERSIONS")
RCLONE_GO=$(grep -Po "RCLONE_GO \"\K\d+\.\d+\.\d+\-[0-9a-h]+_[0-9]+" "${ARANGO_SOURCE}/VERSIONS")
RCLONE_VERSION=$(grep -Po "RCLONE_VERSION \"\K\d+\.\d+\.\d+" "${ARANGO_SOURCE}/VERSIONS")
ARANGO_MAJOR_MINOR_VERSION=$(grep -Po "\K\d+\.\d+" "${ARANGO_SOURCE}/ARANGO-VERSION")

arch=$(uname -m)
if test "$arch" == "x64" -o "$arch" == "x86_64"; then
    arch="amd64"
elif test "$arch" == "aarch64"; then
    arch="arm64"
fi

if test ! -f "${ARANGO_SOURCE}/build/bin/arangodb" ; then
    curl -s -L -o "${ARANGO_SOURCE}/build/bin/arangodb" "https://github.com/arangodb-helper/arangodb/releases/download/$STARTER_REV/arangodb-linux-$arch"
    chmod a+x "${ARANGO_SOURCE}/build/bin/arangodb"
fi
if test ! -f "${ARANGO_SOURCE}/build/bin/rclone-arangodb" ; then 
    curl -s -L -o "${ARANGO_SOURCE}/build/bin/rclone-arangodb" "https://github.com/arangodb/rclone-arangodb/releases/download/golang-${RCLONE_GO}/golang-${RCLONE_GO}_${ARANGO_MAJOR_MINOR_VERSION}_v${RCLONE_VERSION}_rclone-arangodb-linux-$arch"
    chmod a+x "${ARANGO_SOURCE}/build/bin/rclone-arangodb"
fi

if test ! -d ../release-test-automation; then
    ( \
      cd .. ; \
      git clone git@github.com/arangodb/release-test-automation.git; \
      cd release-test-automation; \
      git submodule --init; \
      git pull --all; \
      )
fi

cd ../release-test-automation
DOCKER_SUFFIX=tar-oskarnew
. ./jenkins/common/detect_podman.sh
ALLURE_DIR="$(pwd)/allure-results"
if test -n "$WORKSPACE"; then
    ALLURE_DIR="${WORKSPACE}/allure-results"
fi
if test ! -d "${ALLURE_DIR}"; then 
    mkdir -p "${ALLURE_DIR}"
fi
cat /proc/sys/kernel/core_pattern
ARCH="-$(uname -m)"

if test "${ARCH}" == "-x86_64"; then
    ARCH="-amd64"
else
    ARCH="-arm64v8"
fi

VERSION=$(cat VERSION.json)
git status
GIT_VERSION=$(git rev-parse --verify HEAD |sed ':a;N;$!ba;s/\n/ /g')
if test -z "$GIT_VERSION"; then
    GIT_VERSION=$VERSION
fi
if test -z "$OLD_VERSION"; then
    OLD_VERSION=3.11.0-nightly
fi
if test -z "$NEW_VERSION"; then
    NEW_VERSION="$(sed -e "s;-devel;;" "${ARANGO_SOURCE}/ARANGO-VERSION")-src"
fi
if test -z "${PACKAGE_CACHE}"; then
    PACKAGE_CACHE="$(pwd)/package_cache/"
fi

RTA_ARGS=()
if test -n "$FORCE" -o "$TEST_BRANCH" != 'main'; then
  RTA_ARGS+=(--force)
fi

if test -z "$UPGRADE_MATRIX"; then
    UPGRADE_MATRIX="${OLD_VERSION}:${NEW_VERSION}"
fi

if test -z "${SOURCE}"; then
    SOURCE=nightlypublic
fi
if test -n "$SOURCE"; then
    RTA_ARGS+=(--other-source "$SOURCE")
else
    RTA_ARGS+=(--remote-host 172.17.4.0)
fi
if test "RUN_TEST"; then
    RTA_ARGS+=(--run-test)
else    
    RTA_ARGS+=(--no-run-test)
fi
if test "RUN_UPGRADE"; then
    RTA_ARGS+=(--run-upgrade)
else    
    RTA_ARGS+=(--no-run-upgrade)
fi
if test -z "${RTA_EDITION}"; then
    RTA_EDITION='C'
fi
#IFS=',' read -r -a EDITION_ARR <<< "${RTA_EDITION}"
#for one_edition in "${EDITION_ARR[@]}"; do
#    RTA_ARGS+=(--edition "${one_edition}")
RTA_ARGS+=(--edition "EP")
#done


. ./jenkins/common/setup_docker.sh

. ./jenkins/common/set_max_map_count.sh

. ./jenkins/common/setup_selenium.sh
# . ./jenkins/common/evaluate_force.sh
. ./jenkins/common/load_git_submodules.sh

. ./jenkins/common/launch_minio.sh

. ./jenkins/common/register_cleanup_trap.sh

# scoop all but the docker bridge and localhost, pick the first...
PUBLIC_IP=$(ip address |grep 'inet ' |grep -v 'br-' |grep -v '127.0.0'  |head -n 1|sed -e "s;.*inet ;;" -e "s;/.*;;")

DOCKER_ARGS+=(
       -e HOST_SYMBOLIZER_URL="http://${PUBLIC_IP}:43210"
       -v "${ARANGO_SOURCE}/:/work"
       -v "$(pwd):/work/release-test-automation"
       -v "${ARANGO_SOURCE}/utils:/utils"
       --env=BASE_DIR="/work/"
)

if test -n "${COVERAGE}"; then
  DOCKER_ARGS+=(-e "COVERAGE=${COVERAGE}")
fi
if test -n "${SAN}"; then
    DOCKER_ARGS+=(
        -e "SAN=${SAN}"
        -e "SAN_MODE=${SAN_MODE}"
    )
fi
# we need --init since our upgrade leans on zombies not happening:
$DOCKER run \
        --ulimit nofile=65536 \
       "${DOCKER_ARGS[@]}" \
       --env="ASAN_OPTIONS=${ASAN_OPTIONS}" \
       --env="LSAN_OPTIONS=${LSAN_OPTIONS}" \
       --env="UBSAN_OPTIONS=${UBSAN_OPTIONS}" \
       --env="TSAN_OPTIONS=${TSAN_OPTIONS}" \
       --env="OPENBLAS_NUM_THREADS=${OPENBLAS_NUM_THREADS}" \
       --env="OMP_TOOL_LIBRARIES=${OMP_TOOL_LIBRARIES}" \
       --env="ARCHER_OPTIONS=${ARCHER_OPTIONS}" \
       \
       --env="LLVM_PROFILE_FILE=${LLVM_PROFILE_FILE}" \
       --env="LCOV_PREFIX_STRIP=${LCOV_PREFIX_STRIP}" \
       --env="CLANG_VERSION=${CLANG_VERSION}" \
       \
       --pid=host \
       \
       "${DOCKER_NAMESPACE}${DOCKER_TAG}" \
       \
       /home/release-test-automation/release_tester/mixed_download_upgrade_test.py \
       --upgrade-matrix "${UPGRADE_MATRIX}" \
       --new-version "${NEW_VERSION}" \
       --do-not-run-test-suites \
       "${RTA_ARGS[@]}" \
       "${@}"
result=$?

# don't need docker stop $DOCKER_TAR_NAME
CLEANUP_DOCKER_ARGS=(
    -v "$(pwd)/../../:/oskar"
)
CLEANUP_PARAMS=(
    /oskar/work/
)
. ./jenkins/common/cleanup_ownership.sh
. ./jenkins/common/gather_coredumps.sh
. ./jenkins/common/gather_sanfiles.sh

if test "${result}" -eq "0"; then
    echo "OK"
else
    echo "FAILED ${DOCKER_SUFFIX}!"
    exit 1
fi
