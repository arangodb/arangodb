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

rm -rf agency
mkdir agency
echo -n "Starting agency ... "
if [ $NRAGENTS -gt 1 ]; then
   for aid in `seq 0 $(( $NRAGENTS - 2 ))`; do
       port=$(( 4001 + $aid ))
       build/bin/arangod \
           -c none \
           --agency.id $aid \
           --agency.size $NRAGENTS \
           --agency.supervision true \
           --agency.supervision-frequency 1 \
           --agency.wait-for-sync false \
           --database.directory agency/data$port \
           --javascript.app-path ./js/apps \
           --javascript.startup-directory ./js \
           --javascript.v8-contexts 1 \
           --log.file agency/$port.log \
           --server.authentication false \
           --server.endpoint tcp://127.0.0.1:$port \
           --server.statistics false \
           --agency.compaction-step-size 1000 \
           --log.force-direct true \
           > agency/$port.stdout 2>&1 &
   done
fi
for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    endpoints="$endpoints --agency.endpoint tcp://localhost:$(( 4001 + $aid ))"          
done
build/bin/arangod \
    -c none \
    $endpoints \
    --agency.id $(( $NRAGENTS - 1 )) \
    --agency.notify true \
    --agency.size $NRAGENTS \
    --agency.supervision true \
    --agency.supervision-frequency 1 \
    --agency.wait-for-sync false \
    --database.directory agency/data$(( 4001 + $aid )) \
    --javascript.app-path ./js/apps \
    --javascript.startup-directory ./js \
    --javascript.v8-contexts 1 \
    --log.file agency/$(( 4001 + $aid )).log \
    --server.authentication false \
    --server.endpoint tcp://127.0.0.1:$(( 4001 + $aid )) \
    --server.statistics false \
    --agency.compaction-step-size 1000 \
    --log.force-direct true \
    > agency/$(( 4001 + $aid )).stdout 2>&1 &

echo " done."
echo "Your agents are ready at port 4001 onward"


