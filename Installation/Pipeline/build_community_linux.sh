#!/bin/bash

concurrency=$1
type="build_community_linux"

echo "CONCURRENY: $concurrency"
echo "HOST: `hostname`"
echo "PWD: `pwd`"

(
    mkdir -p build-jenkins
    cd build-jenkins

    CXXFLAGS=-fno-omit-frame-pointer \
        cmake \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DUSE_MAINTAINER_MODE=On \
            -DUSE_CATCH_TESTS=On \
            -DUSE_FAILURE_TESTS=On \
            -DDEBUG_SYNC_REPLICATION=On \
            ..

    rm -rf log-output/$type
    mkdir -p log-output

    make -j$concurrency > log-output/$type.log 2>&1
)

# copy binaries to preserve them
rm -rf build
mkdir build

cp -a build-jenkins/bin   build
cp -a build-jenkins/etc   build
cp -a build-jenkins/tests build
