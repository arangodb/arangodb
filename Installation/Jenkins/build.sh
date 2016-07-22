#!/bin/bash -x
set -e

#if test -f /scripts/prepare_buildenv.sh; then
#    echo "Sourcing docker container environment settings"
#    . /scripts/prepare_buildenv.sh
#fi
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

CC="/usr/bin/gcc-4.9"
CXX="/usr/bin/g++-4.9"
MAKE=make

CFLAGS="-g"
CXXFLAGS="-g"
LDFLAGS="-g"
LIBS=""

MAKE_PARAMS=""
MAKE_CMD_PREFIX=""

CONFIGURE_OPTIONS="$CMAKE_OPENSSL"
MAINTAINER_MODE="-DUSE_MAINTAINER_MODE=off"

TARGET_DIR=""
CLANG36=0
CLANG=0
COVERGAE=0
CPACK=0
FAILURE_TESTS=0
GCC5=0
GOLD=0
SANITIZE=0
VERBOSE=0

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
    
    --cpack)
      CPACK=1
      shift
      ;;

    --cswgcc)
	export CC="/opt/csw/bin/gcc"
	export CXX="/opt/csw/bin/g++"
	MAKE=/opt/csw/bin/gmake
	PATH=/opt/csw/bin/:${PATH}
	shift
	;;
    --targetDir)
        shift
        TARGET_DIR=$1
        shift
        ;;
    *)
      break
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

case "$1" in
  standard)
    CFLAGS="${CFLAGS} -O3"
    CXXFLAGS="${CXXFLAGS} -O3"

    echo "using standard compile configuration"
    ;;

  debug)
    CFLAGS="${CFLAGS} -O0"
    CXXFLAGS="${CXXFLAGS} -O0"
    CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} --enable-v8-debug"

    echo "using debug compile configuration"
    ;;

  maintainer)
    CFLAGS="${CFLAGS} -O3"
    CXXFLAGS="${CXXFLAGS} -O3"
    MAINTAINER_MODE="-DUSE_MAINTAINER_MODE=on"

    echo "using maintainer mode"
    ;;
  
  scan-build)
    MAKE_CMD_PREFIX="scan-build"
    MAKE_PARAMS="-f Makefile"

    echo "using scan-build compile configuration"

    ;;

  *)
    echo "using unknown compile configuration"
    ;;
esac

if [ "$SANITIZE" == 1 ]; then
  if [ "$GCC5" == 1 ]; then
    CFLAGS="${CFLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize=alignment -fno-sanitize=vptr -fno-omit-frame-pointer"
    CXXFLAGS="${CXXFLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize=alignment -fno-sanitize=vptr -fno-omit-frame-pointer"
    LDFLAGS="${LDFLAGS} -pthread"
    LIBS="ubsan;asan"
  else
    CFLAGS="${CFLAGS} -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer"
    CXXFLAGS="${CXXFLAGS} -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer"
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

CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DUSE_OPTIMIZE_FOR_ARCHITECTURE=Off"

echo "using compiler & linker:"
echo "  CC: $CC"
echo "  CXX: $CXX"

echo "using compiler flags:"
echo "  CFLAGS: $CFLAGS"
echo "  CXXFLAGS: $CXXFLAGS"
echo "  LDFLAGS: $LDFLAGS"
echo "  LIBS: $LIBS"
echo "  CONFIGURE_OPTIONS: ${CONFIGURE_OPTIONS} ${MAINTAINER_MODE}"

# compile everything

if test ${CLEAN_IT} -eq 1; then
  echo "found fundamental changes, rebuilding from scratch!"
  git clean -f -d -x
fi
set
test -d build || mkdir build
cd build

VERBOSE_MAKE=""

if [ "${VERBOSE}" == 1 ];  then
  CONFIGURE_OPTIONS="${CONFIGURE_OPTIONS} -DVERBOSE=ON"
  VERBOSE_MAKE="V=1 Verbose=1 VERBOSE=1"
fi

if [ ! -f Makefile ];  then
  CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}" LIBS="${LIBS}" \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DETCDIR=/etc -DVARDIR=/var ${CONFIGURE_OPTIONS} ${MAINTAINER_MODE}
fi

${MAKE_CMD_PREFIX} ${MAKE} ${VERBOSE_MAKE} -j "${PARALLEL_BUILDS}" ${MAKE_PARAMS}
if [ "$CPACK" == "1" -a -n "${TARGET_DIR}" ];  then
  cpack -G DEB
  cp *.deb ${TARGET_DIR}
fi

git rev-parse HEAD > ../last_compiled_version.sha

# and install

if test -n "${TARGET_DIR}";  then
  echo "building distribution tarball"
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
  fi

  rm files.$$

  gzip < ${TARFILE_TMP} > ${dir}/${TARFILE}
  md5sum < ${dir}/${TARFILE} > ${dir}/${TARFILE}.md5
  rm ${TARFILE_TMP}
fi
