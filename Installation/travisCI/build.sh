#!/bin/bash
set -e

echo
echo "$0: setup make-system"

test -d build || mkdir build

echo
echo "$0: configuring ArangoDB"

export LDFLAGS="-lrt"

echo "CC: $CC"
echo "CXX: $CXX"

(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_MAINTAINER_MODE=On -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX)

echo
echo "$0: compiling ArangoDB"

(cd build && make -j2)

echo
echo "$0: testing ArangoDB"

ulimit -c unlimited -S # enable core files
./scripts/unittest all \
  --skipRanges true \
  --skipTimeCritical true \
  --skipNondeterministic true \
  --skipSsl true \
  --skipBoost true \
  --skipGeo true

success=`cat out/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json`
if test "$success" == "false"; then
  exit 1
fi

echo
echo "$0: done"
