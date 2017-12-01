#!/bin/bash
set -e

echo
echo "$0: loading precompiled libraries"

wget \
  -O 3rdParty.tar.gz \
  "https://docs.arangodb.com/support-files/travisCI/precompiled-libraries-4.3.61.tar.gz"

tar xzf 3rdParty.tar.gz

echo
echo "$0: setup make-system"

make setup || exit 1

echo
echo "$0: configuring ArangoDB"
./configure --enable-relative

echo
echo "$0: compiling ArangoDB"

make -j1 || exit 1

echo
echo "$0: linting ArangoDB JS"

ulimit -c unlimited -S # enable core files
make jslint || exit 1

echo
echo "$0: testing ArangoDB"

./scripts/unittest all --skipRanges true --skipTimeCritical true --skipSsl true --skipBoost true --skipGeo true || exit 1
success=`cat out/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json`
if test "$success" == "false"; then
  exit 1
fi

echo
echo "$0: done"
