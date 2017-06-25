foxx="$1"
engine="$2"
os="$3"

PORT01=`./Installation/Pipeline/port.sh`
PORTTRAP="./Installation/Pipeline/port.sh --clean $PORT01 ;"

trap "$PORTTRAP" EXIT

(
    cd resilience
    rm -f core.* build etc js

    ln -s ../build .
    ln -s ../etc .
    ln -s ../js .

    npm install
    ./build/bin/arangod --version

    if [ "$foxx" == yes ]; then
        TESTS=$(find test/* -name "*foxx*")
    else
        TESTS=$(find test/* -not -name "*foxx*")
    fi

    MIN_PORT=`expr $PORT01 + 0` \
    MAX_PORT=`expr $PORT01 + 1999` \
    PORT_OFFSET=10 \
    RESILIENCE_ARANGO_BASEPATH=. \
    ARANGO_STORAGE_ENGINE=rocksdb \
            npm run test-jenkins -- $TESTS
)
