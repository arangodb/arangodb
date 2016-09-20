#!/bin/bash
if [ -z "$XTERM" ] ; then
    XTERM=xterm
fi

if [ ! -d arangod ] || [ ! -d arangosh ] || [ ! -d UnitTests ] ; then
    echo Must be started in the main ArangoDB source directory.
    exit 1
fi

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
    elif [ "$4" == "D" ] ; then
        CLUSTERDEBUGGER=1
        echo Running cluster in debugger.
    elif [ "$4" == "R" ] ; then
        RRDEBUGGER=1
        echo Running cluster in rr with --console.
    fi
fi

SECONDARIES="$5"

MINP=0.5
MAXP=2.5
SFRE=5.0
COMP=1000
BASE=4001
NATH=$(( $NRDBSERVERS + $NRCOORDINATORS + $NRAGENTS ))

rm -rf cluster
if [ -d cluster-init ];then
  cp -a cluster-init cluster
fi
mkdir -p cluster
echo Starting agency ... 
for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    port=$(( $BASE + $aid ))
    build/bin/arangod \
        -c none \
        --agency.activate true \
        --agency.compaction-step-size $COMP \
        --agency.election-timeout-min $MINP \
        --agency.election-timeout-max $MAXP \
        --agency.endpoint tcp://localhost:$BASE \
        --agency.pool-size $NRAGENTS \
        --agency.size $NRAGENTS \
        --agency.supervision true \
        --agency.supervision-frequency $SFRE \
        --agency.wait-for-sync false \
        --database.directory cluster/data$port \
        --javascript.app-path ./js/apps \
        --javascript.startup-directory ./js \
        --javascript.v8-contexts 1 \
        --server.authentication false \
        --server.endpoint tcp://0.0.0.0:$port \
        --server.statistics false \
        --server.threads 16 \
        --log.file cluster/$port.log \
        --log.force-direct true \
        > cluster/$port.stdout 2>&1 &
done

start() {
    if [ "$1" == "dbserver" ]; then
      ROLE="PRIMARY"
    elif [ "$1" == "coordinator" ]; then
      ROLE="COORDINATOR"
    fi
    TYPE=$1
    PORT=$2
    mkdir cluster/data$PORT
    echo Starting $TYPE on port $PORT
    mkdir -p cluster/apps$PORT 
    build/bin/arangod -c none \
                --database.directory cluster/data$PORT \
                --cluster.agency-endpoint tcp://127.0.0.1:$BASE \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
                --log.level info \
                --server.statistics true \
                --server.threads 5 \
                --server.authentication false \
                --javascript.startup-directory ./js \
                --javascript.app-path cluster/apps$PORT \
                --log.force-direct true \
                > cluster/$PORT.stdout 2>&1 &
}

startTerminal() {
    if [ "$1" == "dbserver" ]; then
      ROLE="PRIMARY"
    elif [ "$1" == "coordinator" ]; then
      ROLE="COORDINATOR"
    fi
    TYPE=$1
    PORT=$2
    mkdir cluster/data$PORT
    echo Starting $TYPE on port $PORT
    $XTERM $XTERMOPTIONS -e build/bin/arangod -c none \
                --database.directory cluster/data$PORT \
                --cluster.agency-endpoint tcp://127.0.0.1:$BASE \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
                --log.level info \
                --server.statistics true \
                --server.threads 5 \
                --javascript.startup-directory ./js \
                --javascript.app-path ./js/apps \
                --server.authentication false \
                --console &
}

startDebugger() {
    if [ "$1" == "dbserver" ]; then
      ROLE="PRIMARY"
    elif [ "$1" == "coordinator" ]; then
      ROLE="COORDINATOR"
    fi
    TYPE=$1
    PORT=$2
    mkdir cluster/data$PORT
    echo Starting $TYPE on port $PORT with debugger
    build/bin/arangod -c none \
                --database.directory cluster/data$PORT \
                --cluster.agency-endpoint tcp://127.0.0.1:$BASE \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
                --log.level info \
                --server.statistics false \
                --server.threads 5 \
                --javascript.startup-directory ./js \
                --javascript.app-path ./js/apps \
                --server.authentication false &
    $XTERM $XTERMOPTIONS -e gdb build/bin/arangod -p $! &
}

startRR() {
    if [ "$1" == "dbserver" ]; then
      ROLE="PRIMARY"
    elif [ "$1" == "coordinator" ]; then
      ROLE="COORDINATOR"
    fi
    TYPE=$1
    PORT=$2
    mkdir cluster/data$PORT
    echo Starting $TYPE on port $PORT with rr tracer
    $XTERM $XTERMOPTIONS -e rr build/bin/arangod \
                -c none \
                --database.directory cluster/data$PORT \
                --cluster.agency-endpoint tcp://127.0.0.1:$BASE \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
                --log.level info \
                --server.statistics true \
                --server.threads 5 \
                --javascript.startup-directory ./js \
                --javascript.app-path ./js/apps \
                --server.authentication false \
                --console &
}

PORTTOPDB=`expr 8629 + $NRDBSERVERS - 1`
for p in `seq 8629 $PORTTOPDB` ; do
    if [ "$CLUSTERDEBUGGER" == "1" ] ; then
        startDebugger dbserver $p
    elif [ "$RRDEBUGGER" == "1" ] ; then
        startRR dbserver $p
    else
        start dbserver $p
    fi
done
PORTTOPCO=`expr 8530 + $NRCOORDINATORS - 1`
for p in `seq 8530 $PORTTOPCO` ; do
    if [ "$CLUSTERDEBUGGER" == "1" ] ; then
        startDebugger coordinator $p
    elif [ $p == "8530" -a ! -z "$COORDINATORCONSOLE" ] ; then
        startTerminal coordinator $p
    elif [ "$RRDEBUGGER" == "1" ] ; then
        startRR coordinator $p
    else
        start coordinator $p
    fi
done

if [ "$CLUSTERDEBUGGER" == "1" ] ; then
    echo Waiting for you to setup debugger windows, hit RETURN to continue!
    read
fi

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
    mkdir cluster/data$PORT

    CLUSTER_ID="Secondary$index"
    
    echo Registering secondary $CLUSTER_ID for "DBServer$index"
    curl -f -X PUT --data "{\"primary\": \"DBServer$index\", \"oldSecondary\": \"none\", \"newSecondary\": \"$CLUSTER_ID\"}" -H "Content-Type: application/json" localhost:8530/_admin/cluster/replaceSecondary
    echo Starting Secondary $CLUSTER_ID on port $PORT
    build/bin/arangod -c none \
                --database.directory cluster/data$PORT \
                --cluster.agency-endpoint tcp://127.0.0.1:$BASE \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-id $CLUSTER_ID \
                --log.file cluster/$PORT.log \
                --server.statistics true \
                --javascript.startup-directory ./js \
                --server.authentication false \
                --javascript.app-path ./js/apps \
                > cluster/$PORT.stdout 2>&1 &

    let index=$index+1
  done
fi

echo Done, your cluster is ready at
for p in `seq 8530 $PORTTOPCO` ; do
    echo "   build/bin/arangosh --server.endpoint tcp://127.0.0.1:$p"
done

