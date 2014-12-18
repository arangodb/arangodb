#!/bin/bash

echo
echo "$0: linting ArangoDB JS"

ulimit -c unlimited -S # enable core files
make jslint || exit 1

echo
echo "$0: done"
