#!/bin/bash
set -u
ferr(){
    echo "$@"
    exit 1
}


CXX_STANDARD=${CXX_STANDARD:-17}
echo "requesting c++ standard ${CXX_STANDARD}"
BUILD_TYPE=${BUILD_TYPE:-Release}
HASH_TYPE=${HASH_TYPE:-xxhash}

threads=2
if [[ $TRAVIS_OS_NAME == "linux" ]]; then
    threads=$(nproc)
fi

echo "Building with C++ Standard $CXX_STANDARD"

mkdir -p build && cd build || ferr "could not create build dir"

cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DHashType=${HASH_TYPE} \
      -DBuildTests=ON -DBuildLargeTests=OFF -DBuildVelocyPackExamples=ON \
      -DBuildTools=ON -DEnableSSE=OFF \
      -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
      .. || ferr "failed to configure"

make -j $threads || ferr "failed to build"
echo Done.
