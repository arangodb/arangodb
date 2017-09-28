foxx="$1"
engine="$2"
os="$3"

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
    echo "$0: unknown engine '$os', expecting 'linux' or 'mac'"
    exit 1
fi

. ./Installation/Pipeline/include/test_ulimits.inc
. ./Installation/Pipeline/include/test_log_info.inc
. ./Installation/Pipeline/include/test_setup_tmp.inc

PORT01=`./Installation/Pipeline/port.sh`
PORTTRAP="./Installation/Pipeline/port.sh --clean $PORT01 ;"

trap "$PORTTRAP" EXIT

(
    cd resilience
    rm -f core.* build etc js

    ln -s ../build .
    ln -s ../etc .
    ln -s ../js .

    npm install > ../log-output/npm-install.log 2>&1

    if [ "$foxx" == yes ]; then
        TESTS=$(find test/* -name "*foxx*")
    elif [ "$foxx" == no ]; then
        TESTS=$(find test/* -not -name "*foxx*")
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
