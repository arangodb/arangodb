#!/bin/bash -x
set -e

if python -c "import sys ; sys.exit(sys.platform != 'cygwin')"; then
    echo "can't work with cygwin python - move it away!"
    exit 1
fi

OSNAME=linux
isCygwin=0
if test "$(uname -o||true)" == "Cygwin"; then
    isCygwin=1
    OSNAME=windows
fi

SED=sed
isMac=0
if test "$(uname)" == "Darwin"; then
    isMac=1
    SED=gsed
    OSNAME=darwin
fi
# shut up shellcheck...
export isMac

#          debian          mac
for f in /usr/bin/md5sum /sbin/md5; do
    if test -e ${f}; then
        MD5=${f}
        break
    fi
done
if test -z "${f}"; then
    echo "didn't find a valid MD5SUM binary!"
    exit 1
fi

if test -f /scripts/prepare_buildenv.sh; then
    echo "Sourcing docker container environment settings"
    # shellcheck disable=SC1091
    . /scripts/prepare_buildenv.sh
fi
if test -z "${PARALLEL_BUILDS}"; then
    PARALLEL_BUILDS=1
fi
# remove local from LD_LIBRARY_PATH

if [ "$LD_LIBRARY_PATH" != "" ]; then
    LD_LIBRARY_PATH=$(echo "$LD_LIBRARY_PATH" | ${SED} -e 's/:$//');
fi

# find out if we are running on 32 or 64 bit

BITS=32
case $(uname -m) in
    x86_64)
        BITS=64
        ;;

    armv7l)
        BITS=32
        ;;

    armv6)
        BITS=32
        ;;
esac
#shut up shellcheck
export BITS

compute_relative()
{
    # http://stackoverflow.com/questions/2564634/convert-absolute-path-into-relative-path-given-a-current-directory-using-bash
    # both $1 and $2 are absolute paths beginning with /
    # returns relative path to $2/$target from $1/$source
    source=$1
    target=$2

    common_part=$source # for now
    result="" # for now

    while [[ "${target#$common_part}" == "${target}" ]]; do
        # no match, means that candidate common part is not correct
        # go up one level (reduce common part)
        common_part=$(dirname "$common_part")
        # and record that we went back, with correct / handling
        if [[ -z $result ]]; then
            result=".."
        else
            result="../$result"
        fi
    done

    if [[ $common_part == "/" ]]; then
        # special case for root (no common path)
        result="$result/"
    fi

    # since we now have identified the common part,
    # compute the non-common part
    forward_part="${target#$common_part}"

    # and now stick all parts together
    if [[ -n $result ]] && [[ -n $forward_part ]]; then
        result="$result$forward_part"
    elif [[ -n $forward_part ]]; then
        # extra slash removal
        result="${forward_part:1}"
    fi

    echo "$result"
}

# check if there are any relevant changes to the source code

LASTREV=""

if test -f last_compiled_version.sha;  then
    LASTREV=$(cat last_compiled_version.sha)
fi

COMPILE_MATTERS="3rdParty"

if test -z "${CXX}"; then
    CC=/usr/bin/gcc
    CXX=/usr/bin/g++
fi

CFLAGS="-g -fno-omit-frame-pointer"
CXXFLAGS="-g -fno-omit-frame-pointer"
if test "${isCygwin}" == 1; then
    LDFLAGS=""
else
    LDFLAGS="-g"
fi
V8_CFLAGS="-fno-omit-frame-pointer"
V8_CXXFLAGS="-fno-omit-frame-pointer"
V8_LDFLAGS=""
LIBS=""

BUILD_DIR="build"
BUILD_CONFIG=RelWithDebInfo

PAR="-j"
GENERATOR="Unix Makefiles"
MAKE=make
PACKAGE_MAKE=make
MAKE_PARAMS=()
MAKE_CMD_PREFIX=""
CONFIGURE_OPTIONS+=("$CMAKE_OPENSSL -DGENERATE_BUILD_DATE=OFF -DUSE_IRESEARCH=On")
INSTALL_PREFIX="/"
MAINTAINER_MODE="-DUSE_MAINTAINER_MODE=off"

RETRY_N_TIMES=1
TAR_SUFFIX=""
TARGET_DIR=""
CLANG36=0
CLANG=0
COVERAGE=0
CPACK=
FAILURE_TESTS=0
GCC5=0
GCC6=0
GOLD=0
SANITIZE=0
VERBOSE=0
MSVC=
ENTERPRISE_GIT_URL=

ARCH="-DTARGET_ARCHITECTURE=nehalem"

case "$1" in
    standard)
        CFLAGS="${CFLAGS} -O3"
        CXXFLAGS="${CXXFLAGS} -O3"
        CONFIGURE_OPTIONS+=("-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}")

        echo "using standard compile configuration"
        shift
        ;;

    debug)
        BUILD_CONFIG=Debug
        MAINTAINER_MODE=''
        CFLAGS="${CFLAGS} -O0"
        CXXFLAGS="${CXXFLAGS} -O0"
        CONFIGURE_OPTIONS+=(
            '-DUSE_MAINTAINER_MODE=On'
            '-DUSE_FAILURE_TESTS=On'
            '-DOPTDBG=On'
            '-DUSE_BACKTRACE=On'
            "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}"
        )
        
        echo "using debug compile configuration"
        shift
        ;;

    maintainer)
        CFLAGS="${CFLAGS} -O3"
        CXXFLAGS="${CXXFLAGS} -O3"
        MAINTAINER_MODE="-DUSE_MAINTAINER_MODE=on"
        CONFIGURE_OPTIONS+=("-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}")

        echo "using maintainer mode"
        shift
        ;;

    scan-build)
        MAKE_CMD_PREFIX="scan-build"
        MAKE_PARAMS+=('-f' 'Makefile')

        echo "using scan-build compile configuration"
        shift
        ;;

    *)
        echo "using unknown compile configuration - supported are [standard|debug|maintainer|scan-build]"
        exit 1
        ;;
esac

CLEAN_IT=0

while [ $# -gt 0 ];  do
    case "$1" in
        --clang)
            GCC5=0
            CLANG=1
            shift
            ;;

        --clang36)
            GCC5=0
            CLANG36=1
            shift
            ;;

        --gcc5)
            GCC5=1
            shift
            ;;

        --gcc6)
            GCC5=0
            GCC6=1
            shift
            ;;

        --sanitize)
            TAR_SUFFIX="-sanitize"
            SANITIZE=1
	    USE_JEMALLOC=0
            shift
            ;;

        --noopt)
            ARCH="-DUSE_OPTIMIZE_FOR_ARCHITECTURE=Off"
            shift
            ;;

        --coverage)
            TAR_SUFFIX="-coverage"
            COVERAGE=1
            shift
            ;;

        --msvc)
             shift
             MSVC=1
             CC=""
             CXX=""
             PAR=""
             PARALLEL_BUILDS=""
             GENERATOR="Visual Studio 15 Win64"
             CONFIGURE_OPTIONS+=("-T")
             CONFIGURE_OPTIONS+=("v141,host=x64")
             MAKE="cmake --build . --config ${BUILD_CONFIG}"
             PACKAGE_MAKE="cmake --build . --config ${BUILD_CONFIG} --target"
             CONFIGURE_OPTIONS+=("-DOPENSSL_USE_STATIC_LIBS=TRUE")
             # MSVC doesn't know howto do our assembler in first place.
             ARCH="-DUSE_OPTIMIZE_FOR_ARCHITECTURE=Off"
             export _IsNativeEnvironment=true
             ;;

        --symsrv)
            shift
            SYMSRV=1
            SYMSRVDIR=$1
            shift
            ;;
        
        --gold)
            GOLD=1
            shift
            ;;

        --failure-tests)
            FAILURE_TESTS=1
            shift
            ;;

        --verbose)
            VERBOSE=1
            shift
            ;;
        --prefix)
            shift
            INSTALL_PREFIX=$1
            shift
            ;;
        --buildDir)
            shift
            BUILD_DIR=$1
            shift
            ;;

        --clientBuildDir)
            shift
            CONFIGURE_OPTIONS+=("-DCLIENT_BUILD_DIR=$1")
            shift
            ;;

        --cswgcc)
	    export CC="/opt/csw/bin/gcc"
	    export CXX="/opt/csw/bin/g++"
	    MAKE=/opt/csw/bin/gmake
            PACKAGE_MAKE=/opt/csw/bin/gmake
	    PATH=/opt/csw/bin/:${PATH}
	    shift
	    ;;

        --package)
            shift
            CPACK="$1"
            CONFIGURE_OPTIONS+=("-DPACKAGING=$1")
            shift
            ;;

        --jemalloc)
            USE_JEMALLOC=1
            shift
            ;;

        --rpath)
            CONFIGURE_OPTIONS+=(-DCMAKE_SKIP_RPATH=On)
            shift
            ;;

        --staticlibc)
            CFLAGS="${CFLAGS} -static-libgcc"
            CXXFLAGS="${CXXFLAGS} -static-libgcc -static-libstdc++"
            V8_CFLAGS="${V8_CFLAGS} -static-libgcc"
            V8_CXXFLAGS="${V8_CXXFLAGS} -static-libgcc -static-libstdc++"
            V8_LDFLAGS="${V8_LDFLAGS} -static-libgcc -static-libstdc++"
            shift
            ;;

        --snap)
            CONFIGURE_OPTIONS+=(-DUSE_SNAPCRAFT=ON -DSNAP_PORT=8529)
            shift
            ;;

        --parallel)
            shift
            PARALLEL_BUILDS=$1
            shift
            ;;

        --targetDir)
            shift
            TARGET_DIR=$1
            if test "${isCygwin}" == 1; then
                CONFIGURE_OPTIONS+=("-DPACKAGE_TARGET_DIR=$(cygpath --windows "$1")")
            else
                CONFIGURE_OPTIONS+=("-DPACKAGE_TARGET_DIR=$1")
            fi
            shift
            ;;

        --checkCleanBuild)
            CLEAN_IT=1
            shift
            ;;
        --xcArm)
            shift
            TOOL_PREFIX=$1
            XCGCC=1
            shift
            ;;

        --rpmDistro)
            shift
            CONFIGURE_OPTIONS+=("-DRPM_DISTRO=$1")
            shift
            ;;
        
        --staticOpenSSL)
            shift
            CONFIGURE_OPTIONS+=(-DOPENSSL_USE_STATIC_LIBS=TRUE)
            ;;

        --downloadStarter)
            shift
            DOWNLOAD_STARTER=1
            ;;

        --downloadSyncer)
            shift
            DOWNLOAD_SYNCER_USER=$1
            shift
            ;;

        --enterprise)
            shift
            ENTERPRISE_GIT_URL=$1
            shift
            CONFIGURE_OPTIONS+=(-DUSE_ENTERPRISE=On)
            ;;

        --maintainer)
            shift
            MAINTAINER_MODE="-DUSE_MAINTAINER_MODE=on"
            ;;

        --debugV8)
            shift
            CONFIGURE_OPTIONS+=(-DUSE_DEBUG_V8=ON)
            ;;
        --retryPackages)
            shift
            RETRY_N_TIMES=$1
            shift
            ;;
        --forceVersionNightly)
            shift
            CONFIGURE_OPTIONS+=(-DARANGODB_VERSION_REVISION=nightly)
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done


CONFIGURE_OPTIONS+=("-DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}")

if test -n "$LASTREV"; then
    lines=$(git diff "${LASTREV}:" "${COMPILE_MATTERS}" | wc -l)

    if test "${lines}" -eq 0; then
        echo "no relevant changes, no need for full recompile"
        CLEAN_IT=0
    fi
fi

if [ "$GCC5" == 1 ]; then
    CC=/usr/bin/gcc-5
    CXX=/usr/bin/g++-5
elif [ "$GCC6" == 1 ]; then
    CC=/usr/bin/gcc-6
    CXX=/usr/bin/g++-6
    CXXFLAGS="${CXXFLAGS} -std=c++11"
elif [ "$CLANG" == 1 ]; then
    CC=/usr/bin/clang
    CXX=/usr/bin/clang++
    CXXFLAGS="${CXXFLAGS} -std=c++11"
elif [ "$CLANG36" == 1 ]; then
    CC=/usr/bin/clang-3.6
    CXX=/usr/bin/clang++-3.6
    CXXFLAGS="${CXXFLAGS} -std=c++11"
elif [ "${XCGCC}" = 1 ]; then
    USE_JEMALLOC=0
    ARCH="-DUSE_OPTIMIZE_FOR_ARCHITECTURE=Off"
    BUILD_DIR="${BUILD_DIR}-$(basename "${TOOL_PREFIX}")"

    # tell cmake we're cross compiling:
    CONFIGURE_OPTIONS+=(-DCROSS_COMPILING=true -DCMAKE_SYSTEM_NAME=Linux)
    # -DCMAKE_LIBRARY_ARCHITECTURE=${TOOL_PREFIX} "
    # these options would be evaluated using TRY_RUN(), which obviously doesn't work:
    CONFIGURE_OPTIONS+=(-DHAVE_POLL_FINE_EXITCODE=0)
    CONFIGURE_OPTIONS+=(-DHAVE_GLIBC_STRERROR_R=0)
    CONFIGURE_OPTIONS+=(-DHAVE_GLIBC_STRERROR_R__TRYRUN_OUTPUT=TRUE)
    CONFIGURE_OPTIONS+=(-DHAVE_POSIX_STRERROR_R=1)
    CONFIGURE_OPTIONS+=(-DHAVE_POSIX_STRERROR_R__TRYRUN_OUTPUT=FALSE)
    
    export CXX=$TOOL_PREFIX-g++
    export AR=$TOOL_PREFIX-ar
    export RANLIB=$TOOL_PREFIX-ranlib
    export CC=$TOOL_PREFIX-gcc
    export LD=$TOOL_PREFIX-g++
    export LINK=$TOOL_PREFIX-g++
    export STRIP=$TOOL_PREFIX-strip
    export OBJCOPY=$TOOL_PREFIX-objcopy
    # we need ARM LD:
    GOLD=0;

    # V8's mksnapshot won't work - ignore it:
    MAKE_PARAMS+=(-i)
fi

if [ "${USE_JEMALLOC}" = 1 ]; then
    CONFIGURE_OPTIONS+=(-DUSE_JEMALLOC=On)
else
    CONFIGURE_OPTIONS+=(-DUSE_JEMALLOC=Off)
fi

if [ "$SANITIZE" == 1 ]; then
    if [ "$GCC5" == 1 ]; then
        CFLAGS="${CFLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize=alignment -fno-sanitize=vptr"
        CXXFLAGS="${CXXFLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize=alignment -fno-sanitize=vptr"
        LDFLAGS="${LDFLAGS} -pthread"
        LIBS="ubsan;asan"
    else
        CFLAGS="${CFLAGS} -fsanitize=address -fsanitize=undefined"
        CXXFLAGS="${CXXFLAGS} -fsanitize=address -fsanitize=undefined"
        LDFLAGS="-pthread"
    fi
fi

if [ "$COVERAGE" == 1 ]; then
    CFLAGS="${CFLAGS} -O0 -fprofile-arcs -ftest-coverage -DCOVERAGE"
    CXXFLAGS="${CXXFLAGS} -O0 -fprofile-arcs -ftest-coverage -DCOVERAGE"
    LDFLAGS="${LDFLAGS} -pthread"
    LIBS="gcov"
fi

if [ "$GOLD" == 1 ]; then
    CXXFLAGS="${CXXFLAGS} -B/usr/bin/gold"
fi

if [ "$FAILURE_TESTS" == 1 ]; then
    CONFIGURE_OPTIONS+=(-DUSE_FAILURE_TESTS=on)
fi

if [ -n "$CC" ]; then
    CONFIGURE_OPTIONS+=("-DCMAKE_C_COMPILER=${CC}")
fi

if [ -n "$CXX" ]; then
    CONFIGURE_OPTIONS+=("-DCMAKE_CXX_COMPILER=${CXX}")
fi

if [ "${MSVC}" != "1" ]; then
    # on all other system cmake tends to be sluggish on finding strip.
    # workaround by presetting it:
    if test -z "${STRIP}"; then
        STRIP=/usr/bin/strip
        if [ ! -f ${STRIP} ] ; then
            set +e
            STRIP=$(which strip)
            set -e
        fi
        export STRIP
    fi
    if test -n "${STRIP}"; then
        CONFIGURE_OPTIONS+=("-DCMAKE_STRIP=${STRIP}")
    fi

    if test -z "${OBJCOPY}"; then
        OBJCOPY=/usr/bin/objcopy
        if [ ! -f ${OBJCOPY} ] ; then
            set +e
            OBJCOPY=$(which objcopy)
            
            set -e
            if test -n "${OBJCOPY}" -a ! -x "${OBJCOPY}"; then
                OBJCOPY=""
            fi
        fi
        export OBJCOPY
    fi
    if test -n "${OBJCOPY}"; then
        CONFIGURE_OPTIONS+=("-DCMAKE_OBJCOPY=${OBJCOPY}")
    fi

fi

CONFIGURE_OPTIONS+=("${MAINTAINER_MODE}")
CONFIGURE_OPTIONS+=("${ARCH}")

if [ "${VERBOSE}" == 1 ];  then
    CONFIGURE_OPTIONS+=(-DVERBOSE=ON)
    MAKE_PARAMS+=(V=1 Verbose=1 VERBOSE=1)
fi

if [ -n "${PAR}" ]; then
     MAKE_PARAMS+=("${PAR}" "${PARALLEL_BUILDS}")
fi


echo "using compiler & linker:"
echo "  CC: $CC"
echo "  CXX: $CXX"

echo "using compiler flags:"
echo "  CFLAGS: $CFLAGS"
echo "  CXXFLAGS: $CXXFLAGS"
echo "  LDFLAGS: $LDFLAGS"
echo "  LIBS: $LIBS"
echo "  CONFIGURE_OPTIONS: ${CONFIGURE_OPTIONS[*]}"
echo "  MAKE: ${MAKE_CMD_PREFIX} ${MAKE} ${MAKE_PARAMS[*]}"

# compile everything

if test ${CLEAN_IT} -eq 1; then
    echo "found fundamental changes, rebuilding from scratch!"
    git clean -f -d -x
    if test -d "${BUILD_DIR}"; then
        rm -rf "${BUILD_DIR}"
    fi
fi

SRC=$(pwd)

if test -n "${ENTERPRISE_GIT_URL}" ; then
    GITSHA=$(git log -n1 --pretty='%h')
    if git describe --exact-match --tags "${GITSHA}"; then
        GITARGS=$(git describe --exact-match --tags "${GITSHA}")
        echo "I'm on tag: ${GITARGS}"
    else
        GITARGS=$(git branch --no-color| grep '^\*' | ${SED} "s;\* *;;")
        if echo "$GITARGS" |grep -q ' '; then
            GITARGS=devel
        fi
        echo "I'm on Branch: ${GITARGS}"
        export FINAL_PULL="git pull"
    fi
    # clean up if we're commanded to:
    if test -d enterprise -a "${CLEAN_IT}" -eq 1; then
        rm -rf enterprise
    fi
    if test ! -d enterprise; then
        git clone "${ENTERPRISE_GIT_URL}" enterprise
    fi
    (
        cd enterprise;
        EP_GITSHA=$(git log -n1 --pretty='%h')
        if git describe --exact-match --tags "${EP_GITSHA}"; then
            EP_GITARGS=$(git describe --exact-match --tags "${EP_GITSHA}")
            echo "I'm on tag: ${GITARGS}"
        else
            EP_GITARGS=$(git branch --no-color| grep '^\*' | ${SED} "s;\* *;;")
            if echo "$EP_GITARGS" |grep -q ' '; then
                EP_GITARGS=devel
            fi
            echo "I'm on Branch: ${GITARGS}"
        fi

        if test "${EP_GITARGS}" != "${GITARGS}"; then
            git checkout master;
        fi
        git fetch --tags;
        if git pull --all; then
            if test "${EP_GITARGS}" != "${GITARGS}"; then
                git checkout ${GITARGS};
            fi
        else
            git checkout master;
            git pull --all;
            git fetch --tags;
            git checkout ${GITARGS};
        fi
        ${FINAL_PULL}
    )
fi

THIRDPARTY_BIN=""
THIRDPARTY_SBIN=""
if test -n "${DOWNLOAD_SYNCER_USER}"; then
    count=0
    SEQNO="$$"
    while test -z "${OAUTH_REPLY}"; do
        OAUTH_REPLY=$(
            curl -s "https://$DOWNLOAD_SYNCER_USER@api.github.com/authorizations" \
                 --data "{\"scopes\":[\"repo\", \"repo_deployment\"],\"note\":\"Release${SEQNO}-${OSNAME}\"}"
                   )
        if test -n "$(echo "${OAUTH_REPLY}" |grep already_exists)"; then
            # retry with another number... 
            OAUTH_REPLY=""
            SEQNO=$((SEQNO + 1))
            count=$((count + 1))
            if test "${count}" -gt 20; then
                echo "failed to login to github! Giving up."
                exit 1
            fi
        fi
    done
    OAUTH_TOKEN=$(echo "$OAUTH_REPLY" | \
                         grep '"token"'  |\
                         sed -e 's;.*": *";;' -e 's;".*;;'
                  )
    OAUTH_ID=$(echo "$OAUTH_REPLY" | \
                      grep '"id"'  |\
                      sed -e 's;.*": *;;' -e 's;,;;'
            )

    # shellcheck disable=SC2064
    trap "curl -s -X DELETE \"https://$DOWNLOAD_SYNCER_USER@api.github.com/authorizations/${OAUTH_ID}\"" EXIT

    SYNCER_REV=$(grep "SYNCER_REV" "${SRC}/VERSIONS" |sed 's;.*"\([0-9a-zA-Z.]*\)".*;\1;')
    if test "${SYNCER_REV}" == "latest"; then
        SYNCER_REV=$(curl -s "https://api.github.com/repos/arangodb/arangosync/releases?access_token=${OAUTH_TOKEN}" | \
                             grep tag_name | \
                             head -n 1 | \
                             ${SED} -e "s;.*: ;;" -e 's;";;g' -e 's;,;;'
                   )
    fi
    SYNCER_REPLY=$(curl -s "https://api.github.com/repos/arangodb/arangosync/releases/tags/${SYNCER_REV}?access_token=${OAUTH_TOKEN}")
    DOWNLOAD_URLS=$(echo "${SYNCER_REPLY}" |grep 'browser_download_url
"url".*asset.*' |
                           ${SED} -e "s;.*: ;;" -e 's;";;g'
                 )
    LINE_NO=$(echo "${DOWNLOAD_URLS}" | grep -n "${OSNAME}" | ${SED} -e 's;:http.*;;g')
    LINE_NO=$((LINE_NO - 1))
    SYNCER_URL=$(echo "${DOWNLOAD_URLS}" | head -n "${LINE_NO}" |tail -n 1)
    
    if test -n "${SYNCER_URL}"; then
        mkdir -p "${BUILD_DIR}"
        if test "${isCygwin}" == 1; then
            TN=arangosync.exe
        else
            TN=arangosync
        fi
        if test -f "${TN}"; then
            rm -f "${TN}"
        fi

        FN=$(echo "${DOWNLOAD_URLS}" | grep "${OSNAME}" | ${SED} -e 's;.*/;;g')
        echo "$FN"
        if test -f "${BUILD_DIR}/${FN}-${SYNCER_REV}"; then
            CURRENT_MD5=$(${MD5} < "${BUILD_DIR}/${TN}" | ${SED} "s; .*;;")
            OLD_MD5=$(cat "${BUILD_DIR}/${FN}-${SYNCER_REV}")
            if test "${CURRENT_MD5}" != "${OLD_MD5}"; then
                rm -f "${BUILD_DIR}/${FN}-${SYNCER_REV}"
            fi
        fi
        if ! test -f "${BUILD_DIR}/${FN}-${SYNCER_REV}"; then
            rm -f "${FN}"
            curl -LJO# -H 'Accept: application/octet-stream' "${SYNCER_URL}?access_token=${OAUTH_TOKEN}" || \
                "${SRC}/Installation/Jenkins/curl_time_machine.sh" "${SYNCER_URL}?access_token=${OAUTH_TOKEN}" "${FN}"
            if ! test -s "${FN}" ; then
                echo "failed to download syncer binary - aborting!"
                exit 1
            fi
            mv "${FN}" "${BUILD_DIR}/${TN}"
            ${MD5} < "${BUILD_DIR}/${TN}"  | ${SED} "s; .*;;" > "${BUILD_DIR}/${FN}-${SYNCER_REV}"
            OLD_MD5=$(cat "${BUILD_DIR}/${FN}-${SYNCER_REV}")
            chmod a+x "${BUILD_DIR}/${TN}"
            echo "downloaded ${BUILD_DIR}/${FN}-${SYNCER_REV} MD5: ${OLD_MD5}"
        else
            echo "using already downloaded ${BUILD_DIR}/${FN}-${SYNCER_REV} MD5: ${OLD_MD5}"
        fi
    else
        echo "failed to find download URL for arangosync - aborting!"
        exit 1
    fi
    # Log out again:

    THIRDPARTY_SBIN=("${THIRDPARTY_SBIN}${BUILD_DIR}/${TN}")
fi

if test "${DOWNLOAD_STARTER}" == 1; then
    STARTER_REV=$(grep "STARTER_REV"  "${SRC}/VERSIONS" |sed 's;.*"\([0-9a-zA-Z.]*\)".*;\1;')
    if test "${STARTER_REV}" == "latest"; then
        # we utilize https://developer.github.com/v3/repos/ to get the newest release:
        STARTER_REV=$(curl -s https://api.github.com/repos/arangodb-helper/arangodb/releases | \
                             grep tag_name | \
                             head -n 1 | \
                             ${SED} -e "s;.*: ;;" -e 's;";;g' -e 's;,;;'
                   )
    fi
    STARTER_URL=$(curl -s "https://api.github.com/repos/arangodb-helper/arangodb/releases/tags/${STARTER_REV}" | \
                         grep browser_download_url | \
                         grep "${OSNAME}" | \
                         ${SED} -e "s;.*: ;;" -e 's;";;g' -e 's;,;;'
               )
    if test -n "${STARTER_URL}"; then
        mkdir -p "${BUILD_DIR}"
        if test "${isCygwin}" == 1; then
            TN=arangodb.exe
        else
            TN=arangodb
        fi
        if test -f "${TN}"; then
            rm -f "${TN}"
        fi

        FN=$(echo "${STARTER_URL}" |${SED} "s;.*/;;")
        echo "$FN"
        if test -f "${BUILD_DIR}/${FN}-${STARTER_REV}"; then
            CURRENT_MD5=$(${MD5} < "${BUILD_DIR}/${TN}" | ${SED} "s; .*;;")
            OLD_MD5=$(cat "${BUILD_DIR}/${FN}-${STARTER_REV}")
            if test "${CURRENT_MD5}" != "${OLD_MD5}"; then
                rm -f "${BUILD_DIR}/${FN}-${STARTER_REV}"
            fi
        fi

        if ! test -f "${BUILD_DIR}/${FN}-${STARTER_REV}"; then
            rm -f "${FN}"
            curl -LO "${STARTER_URL}"
            if ! test -s "${FN}" ; then
                echo "failed to download starter binary - aborting!"
                exit 1
            fi
            mv "${FN}" "${BUILD_DIR}/${TN}"
            ${MD5} < "${BUILD_DIR}/${TN}" | ${SED} "s; .*;;" > "${BUILD_DIR}/${FN}-${STARTER_REV}"
            chmod a+x "${BUILD_DIR}/${TN}"
            OLD_MD5=$(cat "${BUILD_DIR}/${FN}-${STARTER_REV}")
            echo "downloaded ${BUILD_DIR}/${FN}-${STARTER_REV} MD5: ${OLD_MD5}"
        else
            echo "using already downloaded ${BUILD_DIR}/${FN}-${STARTER_REV} MD5: ${OLD_MD5}"
        fi
    else
        echo "failed to find download URL for arangodb starter - aborting!"
        exit 1
    fi
    THIRDPARTY_BIN=("${THIRDPARTY_BIN}${BUILD_DIR}/${TN}")
fi

if test -n "${THIRDPARTY_BIN}"; then 
    CONFIGURE_OPTIONS+=("-DTHIRDPARTY_BIN=${THIRDPARTY_BIN}")
fi

if test -n "${THIRDPARTY_SBIN}"; then 
    CONFIGURE_OPTIONS+=("-DTHIRDPARTY_SBIN=${THIRDPARTY_SBIN}")
fi

test -d "${BUILD_DIR}" || mkdir "${BUILD_DIR}"
cd "${BUILD_DIR}"

DST=$(pwd)
SOURCE_DIR=$(compute_relative "${DST}/" "${SRC}/")

set +e
if test "${isCygwin}" == 0; then
    test ! -f Makefile -o ! -f CMakeCache.txt
else
    test ! -f ALL_BUILD.vcxproj -o ! -f CMakeCache.txt
fi
PARTIAL_STATE=$?
set -e

if test "${PARTIAL_STATE}" == 0; then
    rm -rf CMakeFiles CMakeCache.txt CMakeCPackOptions.cmake cmake_install.cmake CPackConfig.cmake CPackSourceConfig.cmake
    # shellcheck disable=SC2068
    CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}" LIBS="${LIBS}" \
          cmake "${SOURCE_DIR}" ${CONFIGURE_OPTIONS[@]} -G "${GENERATOR}" || exit 1
fi

if [ -n "$CPACK" ] && [ -n "${TARGET_DIR}" ] && [ -z "${MSVC}" ];  then
    if ! grep -q CMAKE_STRIP CMakeCache.txt; then
        echo "cmake failed to detect strip; refusing to build unstripped packages!"
        exit 1
    fi
fi

mkdir -p "${DST}/lib/Basics/"
sed "s;@ARANGODB_BUILD_DATE@;$(date "+%Y-%m-%d %H:%M:%S");" "${SOURCE_DIR}/lib/Basics/build-date.h.in" > "${DST}/lib/Basics/build-date.h"
TRIES=0;
set +e
while /bin/true; do
    # shellcheck disable=SC2068
    ${MAKE_CMD_PREFIX} ${MAKE} ${MAKE_PARAMS[@]}
    RC=$?
    if test "${isCygwin}" == 1 -a "${RC}" != 0 -a "${TRIES}" == 0; then
        # sometimes windows will fail on a messed up working copy,
        # but succed on second try. So we retry once.
        TRIES=1
        continue
    fi
    if test "${RC}" != 0; then
        exit ${RC}
    fi
    break
done
set -e

(cd "${SOURCE_DIR}"; git rev-parse HEAD > last_compiled_version.sha)

if [ -n "${CPACK}" ] && [ -n "${TARGET_DIR}" ];  then
    ${PACKAGE_MAKE} clean_packages || exit 1
    
    set +e
    WAIT_ON_FAIL=$((RETRY_N_TIMES - 1))
    while test "${RETRY_N_TIMES}" -gt 0; do
        ${PACKAGE_MAKE} packages && break
        RETRY_N_TIMES=$((RETRY_N_TIMES - 1))
        if test "${WAIT_ON_FAIL}" -gt 0; then
            echo "failed to build packages - waiting 5 mins maybe the situation gets better?"
            sleep 600
        fi
    done
    if test "${RETRY_N_TIMES}" -eq 0; then
        echo "building packages failed terminally"
        exit 1
    fi
    set -e
fi
# and install


if test -n "${TARGET_DIR}";  then
    echo "building distribution tarball"
    mkdir -p "${TARGET_DIR}"
    dir="${TARGET_DIR}"
    if [ -n "$CPACK" ] && [ -n "${TARGET_DIR}" ];  then
        ${PACKAGE_MAKE} copy_packages || exit 1
        ${PACKAGE_MAKE} clean_packages || exit 1
        if test "${SYMSRV}" = "1"; then
            echo "Storing symbols:"
            LIST="$(pwd)/pdbfiles_list.txt"
            export LIST
            find "$(pwd)/bin/" -name "*pdb" | \
                grep -v Release | \
                grep -v Debug | \
                grep -v 3rdParty | \
                grep -v vc120.pdb | \
                cygpath -f - --windows > "${LIST}"
            
            symstore.exe add /f "@$(cygpath --windows "${LIST}")"  /s "$(cygpath --windows "${SYMSRVDIR}")" /t ArangoDB /compress
        fi
    else
        # we re-use a generic cpack tarball:
        ${PACKAGE_MAKE} TGZ_package
        PKG_NAME=$(grep CPACK_PACKAGE_FILE_NAME CPackConfig.cmake | ${SED} 's/\r//' | ${SED} -e 's;".$;;' -e 's;.*";;')

        
        TARFILE="arangodb-$(uname)${TAR_SUFFIX}.tar.gz"
        TARFILE_TMP=$(pwd)/arangodb.tar.$$
        trap 'rm -rf ${TARFILE_TMP}' EXIT

        mkdir -p "${dir}"

        (
            cd _CPack_Packages/*"/TGZ/${PKG_NAME}/"
            rm -rf "${dir}/share/arangodb3/js"
            /bin/ls | tar -c -f "${TARFILE_TMP}" -T -
        )
        
        (cd "${SOURCE_DIR}"

         touch 3rdParty/.keepme
         touch arangod/.keepme
         touch arangosh/.keepme
                               
         tar -u -f "${TARFILE_TMP}" \
             tests/js \
             tests/rb \
             tests/arangodbRspecLib \
             VERSION \
             utils \
             scripts \
             etc/relative \
             etc/testing \
             UnitTests \
             Documentation \
             js \
             lib/Basics/errors.dat \
             3rdParty/.keepme \
             arangod/.keepme \
             arangosh/.keepme
        )

        if test -n "${ENTERPRISE_GIT_URL}" ; then
            (cd "${SOURCE_DIR}/enterprise"; tar -u -f "${TARFILE_TMP}" js)
        fi

        if test "${isCygwin}" == 1; then
            cp "bin/${BUILD_CONFIG}/"* bin/
        fi
        tar -u -f "${TARFILE_TMP}" \
            bin etc tests

        find . -name "*.gcno" > files.$$

        if [ -s files.$$ ]; then
            tar -u -f "${TARFILE_TMP}" \
                --files-from files.$$

            (cd "${SOURCE_DIR}"

             find . \
                  \( \
                     -name "*.cpp" \
                  -o -name "*.h" \
                  -o -name "*.c" \
                  -o -name "*.hpp" \
                  -o -name "*.ll" \
                  -o -name "*.y" \
                  \) \
                  > files.$$

             tar -u -f "${TARFILE_TMP}" \
                 --files-from files.$$

             rm files.$$
            )

            rm files.$$
        fi

        gzip < "${TARFILE_TMP}" > "${dir}/${TARFILE}"
        ${MD5} < "${dir}/${TARFILE}" | ${SED} "s; .*;;" > "${dir}/${TARFILE}.md5"
    fi
fi
