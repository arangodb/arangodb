#!/usr/bin/env bash
if [ -z "$XTERM" ] ; then
    XTERM=x-terminal-emulator
fi
if [ -z "$XTERMOPTIONS" ] ; then
    XTERMOPTIONS="--geometry=80x43"
fi

ARANGOD=/usr/sbin/arangod

NRAGENTS=$1
if [ "$NRAGENTS" == "" ] ; then
    NRAGENTS=1
fi
if [[ $(( $NRAGENTS % 2 )) == 0 ]]; then
    echo Number of agents must be odd.
    exit 1
fi
echo Number of Agents: $NRAGENTS
NRDBSERVERS=$2
if [ "$NRDBSERVERS" == "" ] ; then
    NRDBSERVERS=2
fi
echo Number of DBServers: $NRDBSERVERS
NRCOORDINATORS=$3
if [ "$NRCOORDINATORS" == "" ] ; then
    NRCOORDINATORS=1
fi
echo Number of Coordinators: $NRCOORDINATORS

if [ ! -z "$4" ] ; then
    if [ "$4" == "C" ] ; then
        COORDINATORCONSOLE=1
        echo Starting one coordinator in terminal with --console
    fi
fi

SECONDARIES="$5"

MINP=0.5
MAXP=2.5
COMP=1000
BASE=4001

rm -rf /tmp/cluster
mkdir /tmp/cluster
echo Starting agency ... 
if [ $NRAGENTS -gt 1 ]; then
   for aid in `seq 0 $(( $NRAGENTS - 2 ))`; do
       port=$(( $BASE + $aid ))
       ${ARANGOD} \
           --agency.id $aid \
           --agency.compaction-step-size $COMP \
           --agency.election-timeout-min $MINP \
           --agency.election-timeout-max $MAXP \
           --agency.size $NRAGENTS \
           --agency.supervision true \
           --agency.wait-for-sync false \
           --database.directory /tmp/cluster/data$port \
           --javascript.app-path /tmp/cluster/jsapps$port \
           --javascript.v8-contexts 1 \
           --log.file /tmp/cluster/$port.log \
           --server.authentication false \
           --server.endpoint tcp://127.0.0.1:$port \
           --server.statistics false \
           --log.force-direct true \
           > /tmp/cluster/$port.stdout 2>&1 &
   done
fi
for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    endpoints="$endpoints --agency.endpoint tcp://localhost:$(( $BASE + $aid ))"          
done
${ARANGOD} \
    $endpoints \
    --agency.id $(( $NRAGENTS - 1 )) \
    --agency.compaction-step-size $COMP \
    --agency.election-timeout-min $MINP \
    --agency.election-timeout-max $MAXP \
    --agency.notify true \
    --agency.size $NRAGENTS \
    --agency.supervision true \
    --agency.wait-for-sync false \
    --database.directory /tmp/cluster/data$(( $BASE + $aid )) \
    --javascript.app-path /tmp/cluster/jsapps$port \
    --javascript.v8-contexts 1 \
    --log.file /tmp/cluster/$(( $BASE + $aid )).log \
    --server.authentication false \
    --server.endpoint tcp://127.0.0.1:$(( $BASE + $aid )) \
    --server.statistics false \
    --log.force-direct true \
    > /tmp/cluster/$(( $BASE + $aid )).stdout 2>&1 &

start() {
    if [ "$1" == "dbserver" ]; then
      ROLE="DBSERVER"
    elif [ "$1" == "coordinator" ]; then
      ROLE="COORDINATOR"
      sleep 1
    fi
    TYPE=$1
    PORT=$2
    mkdir /tmp/cluster/data$PORT
    echo Starting $TYPE on port $PORT
    ${ARANGOD} --database.directory /tmp/cluster/data$PORT \
               --cluster.agency-endpoint tcp://127.0.0.1:$BASE \
               --cluster.my-address tcp://127.0.0.1:$PORT \
               --server.endpoint tcp://127.0.0.1:$PORT \
               --cluster.my-role $ROLE \
               --log.file /tmp/cluster/$PORT.log \
               --log.level info \
               --javascript.app-path /tmp/cluster/jsapps$port \
               --server.statistics true \
               --server.authentication false \
               --log.force-direct true \
               > /tmp/cluster/$PORT.stdout 2>&1 &
}

startTerminal() {
    if [ "$1" == "dbserver" ]; then
      ROLE="DBSERVER"
    elif [ "$1" == "coordinator" ]; then
      ROLE="COORDINATOR"
    fi
    TYPE=$1
    PORT=$2
    mkdir /tmp/cluster/data$PORT
    echo Starting $TYPE on port $PORT
    $XTERM $XTERMOPTIONS -e ${ARANGOD} \
                --database.directory /tmp/cluster/data$PORT \
                --cluster.agency-endpoint tcp://127.0.0.1:$BASE \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --javascript.app-path /tmp/cluster/jsapps$port \
                --log.file /tmp/cluster/$PORT.log \
                --log.level info \
                --server.statistics true \
                --server.authentication false \
                --console &
}

PORTTOPDB=`expr 8629 + $NRDBSERVERS - 1`
for p in `seq 8629 $PORTTOPDB` ; do
    start dbserver $p
done
PORTTOPCO=`expr 8530 + $NRCOORDINATORS - 1`
for p in `seq 8530 $PORTTOPCO` ; do
    if [ $p == "8530" -a ! -z "$COORDINATORCONSOLE" ] ; then
        startTerminal coordinator $p
    else
        start coordinator $p
    fi
done

echo Waiting for cluster to come up...

testServer() {
    PORT=$1
    while true ; do
        curl -s -f -X GET "http://127.0.0.1:$PORT/_api/version" > /dev/null 2>&1
        if [ "$?" != "0" ] ; then
            echo Server on port $PORT does not answer yet.
        else
            echo Server on port $PORT is ready for business.
            break
        fi
        sleep 1
    done
}

for p in `seq 8629 $PORTTOPDB` ; do
    testServer $p
done
for p in `seq 8530 $PORTTOPCO` ; do
    testServer $p
done

if [ -n "$SECONDARIES" ]; then
  let index=1
  PORTTOPSE=`expr 8729 + $NRDBSERVERS - 1` 
  for PORT in `seq 8729 $PORTTOPSE` ; do
    mkdir /tmp/cluster/data$PORT

    CLUSTER_ID="Secondary$index"
    
    echo Registering secondary $CLUSTER_ID for "DBServer$index"
    curl -f -X PUT --data "{\"primary\": \"DBServer$index\", \"oldSecondary\": \"none\", \"newSecondary\": \"$CLUSTER_ID\"}" -H "Content-Type: application/json" localhost:8530/_admin/cluster/replaceSecondary
    echo Starting Secondary $CLUSTER_ID on port $PORT
    ${ARANGOD} --database.directory /tmp/cluster/data$PORT \
               --cluster.agency-endpoint tcp://127.0.0.1:$BASE \
               --cluster.my-address tcp://127.0.0.1:$PORT \
               --server.endpoint tcp://127.0.0.1:$PORT \
               --cluster.my-id $CLUSTER_ID \
               --javascript.app-path /tmp/cluster/jsapps$port \
               --log.file /tmp/cluster/$PORT.log \
               --server.statistics true \
               --server.authentication false \
               > /tmp/cluster/$PORT.stdout 2>&1 &

    let index=$index+1
  done
fi

echo Done, your cluster is ready at
for p in `seq 8530 $PORTTOPCO` ; do
    echo "   arangosh --server.authentication false --server.endpoint tcp://127.0.0.1:$p"
done

