#!/bin/bash -x
set -e

if python -c "import sys ; sys.exit(sys.platform != 'cygwin')"; then
    echo "can't work with cygwin python - move it away!"
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
    LD_LIBRARY_PATH=`echo $LD_LIBRARY_PATH | sed -e 's/:$//'`;
fi

# find out if we are running on 32 or 64 bit

BITS=32
case `uname -m` in
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

# check if there are any relevant changes to the source code

LASTREV=""

if test -f last_compiled_version.sha;  then
    LASTREV=`cat last_compiled_version.sha`
fi

COMPILE_MATTERS="3rdParty"
CLEAN_IT=1

if test -n "$LASTREV"; then
    lines=`git diff ${LASTREV}: ${COMPILE_MATTERS} | wc -l`

    if test $lines -eq 0; then
        echo "no relevant changes, no need for full recompile"
        CLEAN_IT=0
    fi
fi

# setup make options
if test -z "${CXX}"; then
    CC="/usr/bin/gcc-4.9"
    CXX="/usr/bin/g++-4.9"
fi

CFLAGS="-g -fno-omit-frame-pointer"
CXXFLAGS="-g -fno-omit-frame-pointer"
LDFLAGS="-g"
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
MAKE_PARAMS=""
MAKE_CMD_PREFIX=""
CONFIGURE_OPTIONS="-DCMAKE_INSTALL_PREFIX=/ -DCMAKE_INSTALL_LOCALSTATEDIR=/var$CMAKE_OPENSSL"
MAINTAINER_MODE="-DUSE_MAINTAINER_MODE=off"

TARGET_DIR=""
CLANG36=0
CLANG=0
COVERGAE=0
CPACK=
FAILURE_TESTS=0
GCC5=0
GOLD=0
SANITIZE=0
VERBOSE=0
MSVC=

case "$1" in
    standard)
        CFLAGS="${CFLAGS} -O3"
        CXXFLAGS="${CXXFLAGS} -O3"
        CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DCMAKE_BUILD_TYPE=${BUILD_CONFIG}"
        
        echo "using standard compile configuration"
        shift
        ;;

    debug)
        CFLAGS="${CFLAGS} -O0"
        CXXFLAGS="${CXXFLAGS} -O0"
        CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} --enable-v8-debug"

        echo "using debug compile configuration"
        shift
        ;;

    maintainer)
        CFLAGS="${CFLAGS} -O3"
        CXXFLAGS="${CXXFLAGS} -O3"
        MAINTAINER_MODE="-DUSE_MAINTAINER_MODE=on"

        echo "using maintainer mode"
        shift
        ;;
    
    scan-build)
        MAKE_CMD_PREFIX="scan-build"
        MAKE_PARAMS="-f Makefile"

        echo "using scan-build compile configuration"
        shift
        ;;

    *)
        echo "using unknown compile configuration"
        exit 1
        ;;
esac


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

        --sanitize)
            SANITIZE=1
            shift
            ;;

        --coverage)
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
             CPACK="ZIP NSIS"
             MAKE='cmake --build . --config RelWithDebInfo'
             PACKAGE_MAKE='cmake --build . --config RelWithDebInfo --target'
             CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DV8_TARGET_ARCHS=Release"
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
        
        --builddir)
            shift
            BUILD_DIR=$1
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
            CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DPACKAGING=$1"
            shift
            ;;

        --jemalloc)
            CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DUSE_JEMALLOC=On"
            shift
            ;;
        
        --rpath)
            CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DCMAKE_SKIP_RPATH=On"
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
        
        --targetDir)
            shift
            TARGET_DIR=$1
            shift
            ;;
                
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

if [ "$GCC5" == 1 ]; then
    CC=/usr/bin/gcc-5
    CXX=/usr/bin/g++-5
elif [ "$CLANG" == 1 ]; then
    CC=/usr/bin/clang
    CXX=/usr/bin/clang++
    CXXFLAGS="${CXXFLAGS} -std=c++11"
elif [ "$CLANG36" == 1 ]; then
    CC=/usr/bin/clang-3.6
    CXX=/usr/bin/clang++-3.6
    CXXFLAGS="${CXXFLAGS} -std=c++11"
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
    CXXFLAG="${CXXFLAGS} -B/usr/bin/gold"
fi

if [ "$FAILURE_TESTS" == 1 ]; then
    CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DUSE_FAILURE_TESTS=on";
fi

if [ -n "$CC" ]; then
    CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DCMAKE_C_COMPILER=${CC}"
fi

if [ -n "$CXX" ]; then
    CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DCMAKE_CXX_COMPILER=${CXX}"
fi

if [ -z "${MSVC}" ]; then
    # MSVC doesn't know howto do assembler in first place.
    CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DUSE_OPTIMIZE_FOR_ARCHITECTURE=Off"
fi

CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} ${MAINTAINER_MODE}"

if [ "${VERBOSE}" == 1 ];  then
    CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DVERBOSE=ON"
    MAKE_PARAMS="${MAKE_PARAMS} V=1 Verbose=1 VERBOSE=1"
fi

if [ -n "${PAR}" ]; then 
     MAKE_PARAMS="${MAKE_PARAMS} ${PAR} ${PARALLEL_BUILDS}"
fi


echo "using compiler & linker:"
echo "  CC: $CC"
echo "  CXX: $CXX"

echo "using compiler flags:"
echo "  CFLAGS: $CFLAGS"
echo "  CXXFLAGS: $CXXFLAGS"
echo "  LDFLAGS: $LDFLAGS"
echo "  LIBS: $LIBS"
echo "  CONFIGURE_OPTIONS: ${CONFIGURE_OPTIONS}"
echo "  MAKE: ${MAKE_CMD_PREFIX} ${MAKE} ${MAKE_PARAMS}"

# compile everything

if test ${CLEAN_IT} -eq 1; then
    echo "found fundamental changes, rebuilding from scratch!"
    git clean -f -d -x
fi

test -d ${BUILD_DIR} || mkdir ${BUILD_DIR}
cd ${BUILD_DIR}

if [ ! -f Makefile -o ! -f CMakeCache.txt ];  then
    CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}" LIBS="${LIBS}" \
          cmake .. ${CONFIGURE_OPTIONS} -G "${GENERATOR}"
fi

${MAKE_CMD_PREFIX} ${MAKE} ${MAKE_PARAMS}
git rev-parse HEAD > ../last_compiled_version.sha

if [ -n "$CPACK"  -a -n "${TARGET_DIR}" ];  then
    ${PACKAGE_MAKE} packages
fi
#if [ -n "$CPACK"  -a -n "${TARGET_DIR}" ];  then
#    for PACK in ${CPACK}; do 
##        if [ "$PACK" == "DEB" ]; then
##            make prepare_debian
##        fi
#        
#        cpack -G "$PACK"
#
#        EXT=`echo $PACK|tr '[:upper:]' '[:lower:]'`
#        if [ "$PACK" == "Bundle" ]; then
#            EXT = 'dmg'
#        fi
#        if [ "$PACK" == "NSIS" ]; then
#            true
#        else
#            cp *.${EXT} ${TARGET_DIR}
#        fi
#    done
#fi


# and install

if test -n "${TARGET_DIR}";  then
    echo "building distribution tarball"
    mkdir -p "${TARGET_DIR}"
    dir="${TARGET_DIR}"
    TARFILE=arangodb.tar.gz
    TARFILE_TMP=`pwd`/arangodb.tar.$$

    mkdir -p ${dir}
    trap "rm -rf ${TARFILE_TMP}" EXIT

    (cd ..

     touch 3rdParty/.keepme
     touch arangod/.keepme
     touch arangosh/.keepme

     tar -c -f ${TARFILE_TMP} \
         VERSION utils scripts etc/relative UnitTests Documentation js \
         lib/Basics/errors.dat \
         3rdParty/.keepme \
         arangod/.keepme \
         arangosh/.keepme
    )
    
    tar -u -f ${TARFILE_TMP} \
        bin etc tests

    find . -name *.gcno > files.$$

    if [ -s files.$$ ]; then
        tar -u -f ${TARFILE_TMP} \
            --files-from files.$$

        (cd ..

         find . \
              \( -name *.cpp -o -name *.h -o -name *.c -o -name *.hpp -o -name *.ll -o -name *.y \) > files.$$

         tar -u -f ${TARFILE_TMP} \
             --files-from files.$$

         rm files.$$
        )

        rm files.$$
    fi

    gzip < ${TARFILE_TMP} > ${dir}/${TARFILE}
    md5sum < ${dir}/${TARFILE} > ${dir}/${TARFILE}.md5
fi
