#!/bin/sh

set -e

mkdir -p build-debian
cd build-debian
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_OPTIMIZE_FOR_ARCHITECTURE=Off -DETCDIR=/etc  -DCMAKE_INSTALL_PREFIX=/usr -DVARDIR=/var ..
make -j12
cpack -G DEB --verbose
cd ..
