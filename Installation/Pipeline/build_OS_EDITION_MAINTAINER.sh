#!/bin/bash
concurrency=$1
os=$2
edition=$3
maintainer=$4
logdir=$5

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
        (ls -l  && echo "$os-$edition-$maintainer") | md5 | awk '{print $1}' > build/location
    else
        (ls -l  && echo "$os-$edition-$maintainer") | md5sum | awk '{print $1}' > build/location
    fi
fi

GENPATH="/tmp/`cat build/location`"

rm -f $GENPATH
ln -s `pwd` $GENPATH
cd $GENPATH

if [ -z "$logdir" ]; then
  logdir=log-output
  rm -rf $logdir
  mkdir -p $logdir
fi

touch $logdir/build.log

echo "CONCURRENY: $concurrency" | tee -a $logdir/build.log
echo "HOST: `hostname`" | tee -a $logdir/build.log
echo "PWD: `pwd`" | tee -a $logdir/build.log

function configureBuild {
    echo "`date +%T` configuring..."
    CXXFLAGS=-fno-omit-frame-pointer \
        cmake \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DCMAKE_INSTALL_PREFIX=/ \
            -DUSE_CATCH_TESTS=On \
            -DDEBUG_SYNC_REPLICATION=On \
            $ENTERPRISE \
            $MAINTAINER \
            $PACKAGING \
            ..  2>&1 | tee -a ../$logdir/build.log
}

(
    set -eo pipefail
    cd build

    configureBuild

    if grep -q "Re-run cmake with a different source directory" ../$logdir/build.log; then
        cd ..
        mv build/location location.$$
        rm -rf build
        mkdir build
        mv location.$$ build
        cd build
        configureBuild
    fi

    echo "`date +%T` building..."
    make -j $concurrency -l $load 2>&1 | tee -a ../$logdir/build.log
) || exit 1

echo "`date +%T` done..."
