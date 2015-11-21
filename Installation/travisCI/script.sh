#!/bin/bash

echo '$0: loading precompiled libraries'

wget \
  -q --show-progress --progress=dot \
  -O - \
  "https://www.arangodb.com/support-files/travisCI/precompiled-libraries-4.1.0.27.tar.gz" | tar xzvf - 

echo
echo '$0: setup make-system'

make setup

echo
echo "$0: configuring ArangoDB"

./configure --enable-relative 

echo
echo "$0: compiling ArangoDB"

make -j2

echo
echo "$0: testing ArangoDB"

ulimit -c unlimited -S # enable core files
make jslint unittests-shell-server unittests-shell-server-aql unittests-http-server SKIP_RANGES=1 || exit 1

echo
echo "$0: done"
