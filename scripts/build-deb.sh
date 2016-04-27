#!/bin/sh

set -e

mkdir -p build-debian
cd build-debian
cmake -DASM_OPTIMIZATIONS=Off -DETCDIR=/etc  -DCMAKE_INSTALL_PREFIX=/usr -DVARDIR=/var ..
make -j12
cpack -G DEB --verbose
cd ..
