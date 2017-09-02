concurrency=$1
edition=$2
os=$3

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

echo "CONCURRENY: $concurrency"
echo "HOST: `hostname`"
echo "PWD: `pwd`"

mkdir -p build

rm -rf log-output
mkdir -p log-output
touch log-output/build.log

(
    set -eo pipefail
    cd build

    echo "`date +%T` configuring..."
    CXXFLAGS=-fno-omit-frame-pointer \
        cmake \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DUSE_MAINTAINER_MODE=On \
            -DUSE_CATCH_TESTS=On \
            -DUSE_FAILURE_TESTS=On \
            -DDEBUG_SYNC_REPLICATION=On \
            $ENTERPRISE \
            ..  2>&1 | tee ../log-output/build.log

    echo "`date +%T` building..."
    make -j $concurrency -l $load 2>&1 | tee -a ../log-output/build.log
) || exit 1

echo "`date +%T` done..."