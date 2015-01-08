#!/bin/bash
set -e

echo
echo "$0: linting ArangoDB JS"

ulimit -c unlimited -S # enable core files
make jslint

echo
echo "$0: done"
