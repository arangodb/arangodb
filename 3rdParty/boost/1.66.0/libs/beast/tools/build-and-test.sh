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

if [[ "${TRAVIS}" == "true" ]]; then
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
  if [[ $VARIANT == "coverage" ]] || \
     [[ $VARIANT == "valgrind" ]] || \
     [[ $VARIANT == "ubasan" ]]; then
    b2 \
      cxxflags=-std=c++11 \
      libs/beast/test/beast/core//fat-tests \
      libs/beast/test/beast/http//fat-tests \
      libs/beast/test/beast/websocket//fat-tests \
      libs/beast/test/beast/zlib//fat-tests \
      toolset=$TOOLSET \
      variant=$VARIANT \
      -j${JOBS}
  else
    b2 \
      cxxflags=-std=c++11 \
      libs/beast/test//fat-tests \
      libs/beast/example \
      toolset=$TOOLSET \
      variant=$VARIANT \
      -j${JOBS}
  fi
}

build_bjam

if [[ $VARIANT == "coverage" ]]; then
  # for lcov to work effectively, the paths and includes
  # passed to the compiler should not contain "." or "..".
  # (this runs in $BOOST_ROOT)
  lcov --version
  find "$BOOST_ROOT" -name "*.gcda" | xargs rm -f
  rm -f "$BOOST_ROOT/*.info"
  lcov --no-external -c -i -d "$BOOST_ROOT" -o baseline.info > /dev/null
  run_tests "$BIN_DIR" fat-tests
  # https://bugs.launchpad.net/ubuntu/+source/lcov/+bug/1163758
  lcov --no-external -c -d "$BOOST_ROOT"  -o testrun.info > /dev/null 2>&1
  lcov -a baseline.info -a testrun.info -o lcov-all.info > /dev/null
  lcov -e "lcov-all.info" "$INC_DIR/*" -o lcov.info > /dev/null
  ~/.local/bin/codecov -X gcov -f lcov.info
  find "$BOOST_ROOT" -name "*.gcda" | xargs rm -f

elif [[ $VARIANT == "valgrind" ]]; then
  run_tests_with_valgrind "$BIN_DIR" fat-tests

else
  run_tests_with_debugger "$BIN_DIR" fat-tests

fi
