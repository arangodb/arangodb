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

(cd build && make -j8)

echo
echo "$0: done"
