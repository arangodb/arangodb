#!/bin/bash
set -e

echo
echo '$0: loading precompiled libraries'

wget -q -O - "https://www.arangodb.com/support-files/travisCI/precompiled-libraries.tar.gz" | tar xzvf - 

echo
echo '$0: setup make-system'

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

make unittests-shell-server unittests-shell-server-aql unittests-http-server SKIP_RANGES=1 || exit 1

echo
echo "$0: done"
