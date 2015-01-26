#!/bin/bash

echo
echo '$0: setup make-system'

make setup || exit 1

echo
echo "$0: configuring ArangoDB"

./configure --enable-relative 

echo
echo "$0: compiling ArangoDB"

make -j2 || exit 1

echo
echo "$0: testing ArangoDB"

ulimit -c unlimited -S # enable core files
make jslint unittests-shell-server unittests-shell-server-aql unittests-http-server SKIP_RANGES=1 || exit 1

echo
echo "$0: done"
