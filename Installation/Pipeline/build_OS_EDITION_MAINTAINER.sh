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

if [ "$os" == linux ]; then
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

if [ -z "$logdir" ]; then
  logdir=log-output
  rm -rf $logdir
  mkdir -p $logdir
fi

touch $logdir/build.log

echo "CONCURRENY: $concurrency" | tee -a $logdir/build.log
echo "HOST: `hostname`" | tee -a $logdir/build.log
echo "PWD: `pwd`" | tee -a $logdir/build.log

(
    set -eo pipefail
    cd build

    echo "`date +%T` configuring..."
    CXXFLAGS=-fno-omit-frame-pointer \
        cmake \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DUSE_CATCH_TESTS=On \
            -DDEBUG_SYNC_REPLICATION=On \
            $MAINTAINER \
            $ENTERPRISE \
            ..  2>&1 | tee -a ../$logdir/build.log

    echo "`date +%T` building..."
    make -j $concurrency -l $load 2>&1 | tee -a ../$logdir/build.log
) || exit 1

echo "`date +%T` done..."
