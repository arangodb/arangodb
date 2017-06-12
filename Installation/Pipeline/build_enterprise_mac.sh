#!/bin/bash

concurrency=$1

(
    mkdir -p build-jenkins
    cd build-jenkins

    CXXFLAGS=-fno-omit-frame-pointer cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUSE_MAINTAINER_MODE=On -DUSE_CATCH_TESTS=On -DUSE_FAILURE_TESTS=On -DDEBUG_SYNC_REPLICATION=On -DUSE_ENTERPRISE=On ..

    make -j$concurrency
)

# copy binaries to preserve them
rm -rf build
mkdir build

cp -a build-jenkins/bin   build
cp -a build-jenkins/etc   build
cp -a build-jenkins/tests build
