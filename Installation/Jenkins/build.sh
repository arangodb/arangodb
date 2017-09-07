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

# setup make options
if test -z "${CXX}"; then
    CC="/usr/bin/gcc-4.9"
    CXX="/usr/bin/g++-4.9"
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
CONFIGURE_OPTIONS+=("$CMAKE_OPENSSL")
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
        CFLAGS="${CFLAGS} -O0"
        CXXFLAGS="${CXXFLAGS} -O0"
        CONFIGURE_OPTIONS+=('-DV8_TARGET_ARCHS=Debug' "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}")

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
            CLANG=1
            shift
            ;;

        --clang36)
            CLANG36=1
            shift
            ;;

        --gcc5)
            GCC5=1
            shift
            ;;

        --gcc6)
            GCC6=1
            shift
            ;;

        --sanitize)
            TAR_SUFFIX="-sanitize"
            SANITIZE=1
            shift
            ;;

        --noopt)
            CONFIGURE_OPTIONS+=(-DUSE_OPTIMIZE_FOR_ARCHITECTURE=Off)
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
             GENERATOR="Visual Studio 14 Win64"
             MAKE="cmake --build . --config ${BUILD_CONFIG}"
             PACKAGE_MAKE="cmake --build . --config ${BUILD_CONFIG} --target"
             CONFIGURE_OPTIONS+=(-DV8_TARGET_ARCHS=Release)
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

        --enterprise)
            shift
            ENTERPRISE_GIT_URL=$1
            shift
            CONFIGURE_OPTIONS+=(-DUSE_ENTERPRISE=On)
            ;;
        --retryPackages)
            shift
            RETRY_N_TIMES=$1
            shift
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

if [ -z "${MSVC}" ]; then
    # MSVC doesn't know howto do assembler in first place.
    CONFIGURE_OPTIONS+=(-DUSE_OPTIMIZE_FOR_ARCHITECTURE=Off)
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

if test "${DOWNLOAD_STARTER}" == 1; then
    if test -f ${SRC}/STARTER_REV; then
        STARTER_REV=$(cat ${SRC}/STARTER_REV)
    else
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

        echo $FN
        if ! test -f "${BUILD_DIR}/${FN}-${STARTER_REV}"; then
            curl -LO "${STARTER_URL}"
            cp "${FN}" "${BUILD_DIR}/${TN}"
            touch "${BUILD_DIR}/${FN}-${STARTER_REV}"
            chmod a+x "${BUILD_DIR}/${TN}"
            echo "downloaded ${BUILD_DIR}/${FN}-${STARTER_REV} MD5: $(${MD5} < "${BUILD_DIR}/${TN}")"
        else
            echo "using already downloaded ${BUILD_DIR}/${FN}-${STARTER_REV} MD5: $(${MD5} < "${BUILD_DIR}/${TN}")"
        fi
    fi
    CONFIGURE_OPTIONS+=("-DTHIRDPARTY_BIN=${BUILD_DIR}/${TN}")
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
    CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}" LIBS="${LIBS}" \
          cmake "${SOURCE_DIR}" ${CONFIGURE_OPTIONS[@]} -G "${GENERATOR}" || exit 1
fi

if [ -n "$CPACK" ] && [ -n "${TARGET_DIR}" ] && [ -z "${MSVC}" ];  then
    if ! grep -q CMAKE_STRIP CMakeCache.txt; then
        echo "cmake failed to detect strip; refusing to build unstripped packages!"
        exit 1
    fi
fi

TRIES=0;
set +e
while /bin/true; do
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
            SSLDIR=$(grep FIND_PACKAGE_MESSAGE_DETAILS_OpenSSL CMakeCache.txt | \
                            ${SED} 's/\r//' | \
                            ${SED} -e "s/.*optimized;//"  -e "s/;.*//" -e "s;/lib.*lib;;"  -e "s;\([a-zA-Z]*\):;/cygdrive/\1;"
                  )
            DLLS=$(find "${SSLDIR}" -name \*.dll |grep -i release)
            cp ${DLLS} "bin/${BUILD_CONFIG}"
            cp "bin/${BUILD_CONFIG}/"* bin/
            cp "tests/${BUILD_CONFIG}/"*exe bin/
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
