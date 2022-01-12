#!/usr/bin/env bash

# add 'x' for command tracing
set -eu

#-------------------------------------------------------------------------------
#
# Utilities
#

# For builds not triggered by a pull request TRAVIS_BRANCH is the name of the
# branch currently being built; whereas for builds triggered by a pull request
# it is the name of the branch targeted by the pull request (in many cases this
# will be master).
MAIN_BRANCH="0"
if [[ $TRAVIS_BRANCH == "master" || $TRAVIS_BRANCH == "develop" ]]; then
    MAIN_BRANCH="1"
fi

if [[ "${BEAST_RETRY}" == "true" ]]; then
  JOBS=1
elif [[ "${TRAVIS}" == "true" ]]; then
  JOBS="2"
elif [[ $(uname -s) == "Linux" ]]; then
  # Physical cores
  JOBS=$(lscpu -p | grep -v '^#' | sort -u -t, -k 2,4 | wc -l)
elif [[ $(uname) == "Darwin" ]]; then
  # Physical cores
  JOBS=$(sysctl -n hw.physicalcpu)
else
  JOBS=1
fi

# run with a debugger
function debug_run ()
{
  if [[ $TRAVIS_OS_NAME == "osx" ]]; then
    # -o runs after loading the binary
    # -k runs after any crash
    # We use a ghetto appromixation of --return-child-result, exiting with
    # 1 on a crash
    lldb \
      --batch \
      -o 'run' \
      -k 'thread backtrace all' \
      -k 'script import os; os._exit(1)' \
      $@
  else
    gdb \
      --silent \
      --batch \
      --return-child-result \
      -ex="set print thread-events off" \
      -ex=run \
      -ex="thread apply all bt full" \
      --args $@
  fi
}

function valgrind_run ()
{
  valgrind \
    --track-origins=yes \
    --max-stackframe=16000000 \
    --suppressions=$BOOST_ROOT/libs/beast/tools/valgrind.supp \
    --error-exitcode=1 \
    $@
}

function run_tests_with_debugger ()
{
  find "$1" -name "$2" -print0 | while read -d $'\0' f
  do
    debug_run "$f"
  done
}

function run_tests_with_valgrind ()
{
  find "$1" -name "$2" -print0 | while read -d $'\0' f
  do
    valgrind_run "$f"
  done
}

function run_tests ()
{
  find "$1" -name "$2" -print0 | while read -d $'\0' f
  do
    "$f"
  done
}

#-------------------------------------------------------------------------------

BIN_DIR="$BOOST_ROOT/bin.v2/libs/beast/test"
LIB_DIR="$BOOST_ROOT/libs/beast"
INC_DIR="$BOOST_ROOT/boost/beast"

function build_bjam ()
{
  if [[ $VARIANT == "beast_coverage" ]] || \
     [[ $VARIANT == "beast_valgrind" ]] || \
     [[ $VARIANT == "beast_ubasan" ]]; then
    b2 \
      define=BOOST_COROUTINES_NO_DEPRECATION_WARNING=1 \
      cxxstd=$CXXSTD \
      libs/beast/test/beast/core//fat-tests \
      libs/beast/test/beast/http//fat-tests \
      libs/beast/test/beast/websocket//fat-tests \
      libs/beast/test/beast/zlib//fat-tests \
      toolset=$TOOLSET \
      variant=$VARIANT \
      link=static \
      -j${JOBS}
  elif [[ $VARIANT == "debug" ]]; then
    b2 \
      define=BOOST_COROUTINES_NO_DEPRECATION_WARNING=1 \
      cxxstd=$CXXSTD \
      libs/beast/test//fat-tests \
      libs/beast/example \
      toolset=$TOOLSET \
      variant=$VARIANT \
      -j${JOBS}
  else
    b2 \
      define=BOOST_COROUTINES_NO_DEPRECATION_WARNING=1 \
      cxxstd=$CXXSTD \
      libs/beast/test//fat-tests \
      toolset=$TOOLSET \
      variant=$VARIANT \
      -j${JOBS}
  fi
}

build_bjam

if [[ $VARIANT == "beast_coverage" ]]; then
  GCOV=${GCOV:-gcov}
  # for lcov to work effectively, the paths and includes
  # passed to the compiler should not contain "." or "..".
  # (this runs in $BOOST_ROOT)
  lcov --version
  find "$BOOST_ROOT" -name "*.gcda" | xargs rm -f
  rm -f "$BOOST_ROOT/*.info"
  lcov --gcov-tool $GCOV --no-external -c -i -d "$BOOST_ROOT" -o baseline.info > /dev/null
  run_tests "$BIN_DIR" fat-tests
  # https://bugs.launchpad.net/ubuntu/+source/lcov/+bug/1163758
  lcov --gcov-tool $GCOV --no-external -c -d "$BOOST_ROOT"  -o testrun-all.info > /dev/null 2>&1
  lcov --gcov-tool $GCOV -a baseline.info -a testrun-all.info -o lcov-diff.info > /dev/null
  lcov --gcov-tool $GCOV -e "lcov-diff.info" "$INC_DIR/*" -o lcov.info > /dev/null
  lcov --gcov-tool $GCOV --remove "lcov.info" "$INC_DIR/_experimental/*" -o lcov.info > /dev/null
  echo "Change working directory for codecov:"
  pwd
  pushd .
  cd libs/beast
  ~/.local/bin/codecov -X gcov -f ../../lcov.info
  popd
  find "$BOOST_ROOT" -name "*.gcda" | xargs rm -f

elif [[ $VARIANT == "beast_valgrind" ]]; then
  run_tests_with_valgrind "$BIN_DIR" fat-tests

else
  #run_tests_with_debugger "$BIN_DIR" fat-tests
  run_tests "$BIN_DIR" fat-tests

fi
