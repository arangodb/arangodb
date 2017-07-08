concurrency=$1
edition=$2
os=$3

ENTERPRISE=""
type="build"

if [ "$edition" == community ]; then
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
    load=20
elif [ "$os" == mac ]; then
    type="${type}_mac"
    load=5
else
    echo "$0: unknown os '$os', expecting 'linux' or 'mac'"
    exit 1
fi

echo "CONCURRENY: $concurrency"
echo "HOST: `hostname`"
echo "PWD: `pwd`"

mkdir -p build-$edition

if [ ! -f build-$edition/location ]; then
    if [ "$os" == mac ]; then
        (ls -l  && echo "$edition $os") | md5 | awk '{print $1}' > build-$edition/location
    else
        (ls -l  && echo "$edition $os") | md5sum | awk '{print $1}' > build-$edition/location
    fi
fi

GENPATH="/tmp/`cat build-$edition/location`"

rm -f $GENPATH
ln -s `pwd` $GENPATH
cd $GENPATH

echo "GENPATH: `pwd`"

rm -rf log-output/$type.log
mkdir -p log-output
touch log-output/$type.log

(
    cd build-$edition

    echo "`date +%T` configuring..."
    CXXFLAGS=-fno-omit-frame-pointer \
        cmake \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DUSE_MAINTAINER_MODE=On \
            -DUSE_CATCH_TESTS=On \
            -DUSE_FAILURE_TESTS=On \
            -DDEBUG_SYNC_REPLICATION=On \
            $ENTERPRISE \
            .. >> ../log-output/$type.log 2>&1

    if [ "$?" != 0 ]; then
        if fgrep 'Re-run cmake with a different source directory' ../log-output/$type.log; then
            mv location ..
            rm -rf *
            mv ../location .
            
            CXXFLAGS=-fno-omit-frame-pointer \
                cmake \
                    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
                    -DUSE_MAINTAINER_MODE=On \
                    -DUSE_CATCH_TESTS=On \
                    -DUSE_FAILURE_TESTS=On \
                    -DDEBUG_SYNC_REPLICATION=On \
                    $ENTERPRISE \
                    .. >> ../log-output/$type.log 2>&1 || exit 1
        else
            exit 1
        fi
    fi

    echo "`date +%T` building..."
    make -j $concurrency -l $load >> ../log-output/$type.log 2>&1 || exit 1
) || exit 1

# copy binaries to preserve them
echo "`date +%T` copying..."

rm -rf build
mkdir -p build/tests

cp -a build-$edition/bin                 build
cp -a build-$edition/etc                 build
cp -a build-$edition/tests/arangodbtests build/tests

echo "`date +%T` done..."
