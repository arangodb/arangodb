#!/bin/bash
os="$1"
engine="$2"
foxx="$3"

if [ "$engine" == mmfiles ]; then
    type="${type}_${engine}"
elif [ "$engine" == rocksdb ]; then
    type="${type}_${engine}"
else
    echo "$0: unknown engine '$engine', expecting 'mmfiles' or 'rocksdb'"
    exit 1
fi

if [ "$os" == linux ]; then
    type="${type}_${os}"
elif [ "$os" == mac ]; then
    type="${type}_${os}"
else
    echo "$0: unknown os '$os', expecting 'linux' or 'mac'"
    exit 1
fi

PORT01=`./Installation/Pipeline/port.sh`
PORTTRAP="./Installation/Pipeline/port.sh --clean $PORT01 ;"

trap "$PORTTRAP" EXIT

test -d resilience || exit 1

(
    cd resilience
    rm -f core.* build etc js

    ln -s ../build .
    ln -s ../etc .
    ln -s ../js .

    npm install > install.log

    if [ "$foxx" == "foxx" ]; then
        TESTS=$(find "test/foxx")
    elif [ "$foxx" == "cluster" ]; then
        TESTS=$(find "test/cluster")
    elif [ "$foxx" == "single" ]; then
        TESTS=$(find "test/single")
    else
        TESTS="$foxx"
    fi

    echo "TESTS: $TESTS"

    MIN_PORT=`expr $PORT01 + 0` \
    MAX_PORT=`expr $PORT01 + 1999` \
    PORT_OFFSET=10 \
    RESILIENCE_ARANGO_BASEPATH=. \
    ARANGO_STORAGE_ENGINE=$engine \
    npm run test-jenkins -- $TESTS
)
