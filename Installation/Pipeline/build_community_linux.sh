#!/bin/bash

concurrency=$1
type="build_community_linux"

echo "CONCURRENY: $concurrency"
echo "HOST: `hostname`"
echo "PWD: `pwd`"

rm -rf log-output/$type.log
mkdir -p log-output

(
    mkdir -p build-jenkins
    cd build-jenkins

    echo "Configuring..."
    CXXFLAGS=-fno-omit-frame-pointer \
        cmake \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DUSE_MAINTAINER_MODE=On \
            -DUSE_CATCH_TESTS=On \
            -DUSE_FAILURE_TESTS=On \
            -DDEBUG_SYNC_REPLICATION=On \
            .. > ../log-output/$type.log 2>&1 || exit 1

    echo "Building..."
    make -j$concurrency >> ../log-output/$type.log 2>&1 || exit 1
)

# copy binaries to preserve them
echo "Copying..."

rm -rf build
mkdir -p build/tests

cp -a build-jenkins/bin                 build
cp -a build-jenkins/etc                 build
cp -a build-jenkins/tests/arangodbtests build/tests

echo "Done..."
