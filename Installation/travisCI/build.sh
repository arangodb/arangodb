#!/bin/bash
set -e

echo
echo "$0: loading precompiled libraries"

wget \
  -O 3rdParty.tar.gz \
  "https://www.arangodb.com/support-files/travisCI/precompiled-libraries-4.3.61.tar.gz"

tar xzf 3rdParty.tar.gz

echo
echo "$0: setup make-system"

mkdir build
ln -s build/bin bin

echo
echo "$0: configuring ArangoDB"

export LDFLAGS="-lrt"

(cd build && cmake .. -DUSE_RELATIVE=ON -DUSE_PRECOMPILED_V8=ON)

echo
echo "$0: compiling ArangoDB"

(cd build && make -j1)

echo
echo "$0: linting ArangoDB JS"

./utils/jslint.sh

echo
echo "$0: testing ArangoDB"

ulimit -c unlimited -S # enable core files
./scripts/unittest all \
  --skipRanges true \
  --skipTimeCritical true \
  --skipNondeterministic true \
  --skipSsl true \
  --skipBoost true \
  --skipGeo true \
  --skipShebang true

success=`cat out/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json`
if test "$success" == "false"; then
  exit 1
fi

echo
echo "$0: done"
