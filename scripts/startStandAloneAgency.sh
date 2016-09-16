#!/bin/bash

NRAGENTS=3
POOLSZ=""
TRANSPORT="tcp"
LOG_LEVEL="INFO"

while getopts ":a:p:t:l:" opt; do
  case $opt in
    a) NRAGENTS="$OPTARG"
    ;;
    p) POOLSZ="$OPTARG"
    ;;
    t) TRANSPORT="$OPTARG"
    ;;
    l) LOG_LEVEL="$OPTARG"
    ;;
    \?) echo "Invalid option -$OPTARG" >&2
    ;;
  esac
done

if [ "$POOLSZ" == "" ] ; then
    POOLSZ=$NRAGENTS
fi

if [ "$TRANSPORT" == "ssl" ]; then
    SSLKEYFILE="--ssl.keyfile UnitTests/server.pem"
else
    SSLKEYFILE=""
fi

printf "agency-size: %s\n" "$NRAGENTS"
printf "pool-size: %s\n" "$POOLSZ"
printf "transport: %s\n" "$TRANSPORT"
printf "log-level: %s\n" "$LOG_LEVEL"

if [ ! -d arangod ] || [ ! -d arangosh ] || [ ! -d UnitTests ] ; then
    echo Must be started in the main ArangoDB source directory.
    exit 1
fi

if [[ $(( $NRAGENTS % 2 )) == 0 ]]; then
    echo Number of agents must be odd.
    exit 1
fi

MINP=0.5
MAXP=2.0
SFRE=2.5
COMP=1000
BASE=5001

rm -rf agency
mkdir -p agency
echo -n "Starting agency ... "
for aid in `seq 0 $(( $POOLSZ - 1 ))`; do
    port=$(( $BASE + $aid ))
    build/bin/arangod \
        -c none \
        --agency.activate true \
        --agency.election-timeout-min $MINP \
        --agency.election-timeout-max $MAXP \
        --agency.endpoint $TRANSPORT://localhost:$BASE \
        --agency.my-address $TRANSPORT://localhost:$port \
        --agency.compaction-step-size $COMP \
        --agency.pool-size $POOLSZ \
        --agency.size $NRAGENTS \
        --agency.supervision true \
        --agency.supervision-frequency $SFRE \
        --agency.wait-for-sync false \
        --database.directory agency/data$port \
        --javascript.app-path ./js/apps \
        --javascript.startup-directory ./js \
        --javascript.v8-contexts 1 \
        --log.file agency/$port.log \
        --log.force-direct true \
        --log.level agency=$LOG_LEVEL \
        --server.authentication false \
        --server.endpoint $TRANSPORT://localhost:$port \
        --server.statistics false \
        $SSLKEYFILE \
        > agency/$port.stdout 2>&1 &
done

echo "done."
echo "Your agents are ready at port $BASE onward"


