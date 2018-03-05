#!/bin/bash
concurrency=$1
os=$2
edition=$3
maintainer=$4

ENTERPRISE=""
type="build"

if [ "$edition" == community ]; then
    ENTERPRISE="-DUSE_ENTERPRISE=Off"
    type="${type}_community"
elif [ "$edition" == enterprise ]; then
    type="${type}_enterprise"
    ENTERPRISE="-DUSE_ENTERPRISE=On"
else
    echo "$0: unknown edition '$edition', expecting 'community' or 'enterprise'"
    exit 1
fi

MAINTAINER=""

if [ "$maintainer" == maintainer ]; then
    MAINTAINER="-DUSE_MAINTAINER_MODE=On -DUSE_FAILURE_TESTS=On"
    type="${type}_maintainer"
elif [ "$maintainer" == user ]; then
    MAINTAINER="-DUSE_MAINTAINER_MODE=Off"
    type="${type}_user"
else
    echo "$0: unknown maintainer '$maintainer', expecting 'maintainer' or 'user'"
    exit 1
fi

PACKAGING=

if [ "$os" == linux ]; then
    PACKAGING="-DPACKAGING=DEB"
    type="${type}_linux"
    load=40
elif [ "$os" == mac ]; then
    type="${type}_mac"
    load=10
else
    echo "$0: unknown os '$os', expecting 'linux' or 'mac'"
    exit 1
fi

mkdir -p build

if [ ! -f build/location ]; then
    if [ "$os" == mac ]; then
        (ls -l && echo "$os-$edition-$maintainer" && head -c 20 /dev/urandom) | md5 | awk '{print $1}' > build/location
    else
        (ls -l && echo "$os-$edition-$maintainer" && head -c 20 /dev/urandom) | md5sum | awk '{print $1}' > build/location
    fi
fi

GENPATH="/tmp/`cat build/location`"

rm -f $GENPATH
ln -s `pwd` $GENPATH
cd $GENPATH

if test -z "$CMAKE_BUILD_TYPE"; then
    CMAKE_BUILD_TYPE=RelWithDebInfo
fi

echo "CONCURRENY: $concurrency"
echo "HOST: `hostname`"
echo "PWD: `pwd`"
echo "BUILD_TYPE: $CMAKE_BUILD_TYPE"

function configureBuild {
    echo "`date +%T` configuring..."
    rm -f cmake.run

    (CXXFLAGS=-fno-omit-frame-pointer \
        cmake \
            -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
            -DCMAKE_INSTALL_PREFIX=/ \
            -DUSE_CATCH_TESTS=On \
            -DDEBUG_SYNC_REPLICATION=On \
            $ENTERPRISE \
            $MAINTAINER \
            $PACKAGING \
            .. 2>&1 || echo "FAILED") | tee cmake.run


    if test ! -e cmake.run; then
        exit 1
    elif fgrep -q "does not match the source" cmake.run; then
        rm -rf *
        configureBuild
    elif fgrep -q "FAILED" cmake.run; then
        exit 1
    fi
}

(
    cd build

    configureBuild

    echo "`date +%T` building..."
    make -j $concurrency -l $load 2>&1
) || exit 1

# the file is huge and taking lots of space and is totally useless on jenkins afterwards
rm build/arangod/libarangoserver.a || true
rm build/bin/libarangoserver.a || true


echo "`date +%T` done..."
