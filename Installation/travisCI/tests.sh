#!/bin/bash

echo
echo "$0: testing ArangoDB"

ulimit -c unlimited -S # enable core files
make unittests-shell-server unittests-shell-server-aql unittests-http-server SKIP_RANGES=1 || exit 1

echo
echo "$0: done"
