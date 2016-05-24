#!/bin/sh

set -e

mkdir -p build-debian
cd build-debian
CFLAGS="-g -static-libgcc -O3 -fomit-frame-pointer" CXXFLAGS="-g -static-libgcc -static-libstdc++ -O3 -fomit-frame-pointer" V8_CFLAGS="-static-libgcc" V8_CXXFLAGS="-static-libgcc -static-libstdc++" V8_LDFLAGS="-static-libgcc -static-libstdc++" cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_SKIP_RPATH=On -DUSE_OPTIMIZE_FOR_ARCHITECTURE=Off -DETCDIR=/etc  -DCMAKE_INSTALL_PREFIX=/usr -DVARDIR=/var -DUSE_JEMALLOC=On ..
make -j12
cpack -G DEB --verbose
cd ..
