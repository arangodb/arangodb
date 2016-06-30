#!/bin/bash

if [ $# -eq 0 ]
  then
      echo Number of agents not specified starting with 3.
      NRAGENTS=3
else
    NRAGENTS=$1
    echo Number of Agents: $NRAGENTS
fi

if [ ! -d arangod ] || [ ! -d arangosh ] || [ ! -d UnitTests ] ; then
    echo Must be started in the main ArangoDB source directory.
    exit 1
fi

if [[ $(( $NRAGENTS % 2 )) == 0 ]]; then
    echo Number of agents must be odd.
    exit 1
fi

MINP=0.25
MAXP=2.5
SFRE=2.5
COMP=1000
BASE=4001

rm -rf agency
mkdir agency
echo -n "Starting agency ... "
if [ $NRAGENTS -gt 1 ]; then
   for aid in `seq 0 $(( $NRAGENTS - 2 ))`; do
       port=$(( $BASE + $aid ))
       build/bin/arangod \
           -c none \
           --agency.id $aid \
           --agency.size $NRAGENTS \
           --agency.supervision true \
           --agency.supervision-frequency $SFRE \
           --agency.wait-for-sync false \
           --agency.election-timeout-min $MINP \
           --agency.election-timeout-max $MAXP \
           --database.directory agency/data$port \
           --javascript.app-path ./js/apps \
           --javascript.startup-directory ./js \
           --javascript.v8-contexts 1 \
           --log.file agency/$port.log \
           --server.authentication false \
           --server.endpoint tcp://127.0.0.1:$port \
           --server.statistics false \
           --agency.compaction-step-size $COMP \
           --log.force-direct true \
           > agency/$port.stdout 2>&1 &
       sleep 0
   done
fi
for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    endpoints="$endpoints --agency.endpoint tcp://localhost:$(( $BASE + $aid ))"          
done
build/bin/arangod \
    -c none \
    $endpoints \
    --agency.id $(( $NRAGENTS - 1 )) \
    --agency.notify true \
    --agency.size $NRAGENTS \
    --agency.supervision true \
    --agency.supervision-frequency $SFRE \
    --agency.wait-for-sync false \
    --agency.election-timeout-min $MINP \
    --agency.election-timeout-max $MAXP \
    --database.directory agency/data$(( $BASE + $aid )) \
    --javascript.app-path ./js/apps \
    --javascript.startup-directory ./js \
    --javascript.v8-contexts 1 \
    --log.file agency/$(( $BASE + $aid )).log \
    --server.authentication false \
    --server.endpoint tcp://127.0.0.1:$(( $BASE + $aid )) \
    --server.statistics false \
    --agency.compaction-step-size $COMP \
    --log.force-direct true \
    > agency/$(( $BASE + $aid )).stdout 2>&1 &

echo " done."
echo "Your agents are ready at port $BASE onward"


