#!/bin/bash

if [ $# -eq 0 ]
then
    echo Number of agents not specified starting with 3.
    NRAGENTS=3
else
    NRAGENTS=$1
    echo Number of Agents: $NRAGENTS
fi


POOLSZ=$2
if [ "$POOLSZ" == "" ] ; then
    POOLSZ=$NRAGENTS
fi

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
if [ $NRAGENTS -gt 1 ]; then
    for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
        port=$(( $BASE + $aid ))
        build/bin/arangod \
            -c none \
            --agency.endpoint tcp://localhost:5001 \
            --agency.activate true \
            --agency.size $NRAGENTS \
            --agency.pool-size $POOLSZ \
            --agency.supervision true \
            --agency.supervision-frequency $SFRE \
            --agency.wait-for-sync true \
            --agency.election-timeout-min $MINP \
            --agency.election-timeout-max $MAXP \
            --database.directory agency/data$port \
            --javascript.app-path ./js/apps \
            --javascript.startup-directory ./js \
            --javascript.v8-contexts 1 \
            --log.file agency/$port.log \
            --server.authentication false \
            --server.endpoint tcp://0.0.0.0:$port \
            --server.statistics false \
            --agency.compaction-step-size $COMP \
            --log.level agency=debug \
            --log.force-direct true \
            > agency/$port.stdout 2>&1 &
    done
fi

echo " done."
echo "Your agents are ready at port $BASE onward"


