#!/bin/bash
set -e

echo
echo "$0: loading precompiled libraries"

V8_VERSION=`/bin/ls 3rdParty/V8/|grep V8 |sed "s;V8-;;"`

wget \
  -O 3rdParty.tar.gz \
  "https://www.arangodb.com/support-files/travisCI/precompiled-libraries-${V8_VERSION}.tar.gz"

tar xzf 3rdParty.tar.gz

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
