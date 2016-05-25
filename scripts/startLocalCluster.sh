#!/bin/bash
if [ -z "$XTERM" ] ; then
    XTERM=x-terminal-emulator
fi
if [ -z "$XTERMOPTIONS" ] ; then
    XTERMOPTIONS="--geometry=80x43"
fi

if [ ! -d arangod ] || [ ! -d arangosh ] || [ ! -d UnitTests ] ; then
    echo Must be started in the main ArangoDB source directory.
    exit 1
fi

NRDBSERVERS=$1
if [ "$NRDBSERVERS" == "" ] ; then
    NRDBSERVERS=2
fi
echo Number of DBServers: $NRDBSERVERS
NRCOORDINATORS=$2
if [ "$NRCOORDINATORS" == "" ] ; then
    NRCOORDINATORS=1
fi
echo Number of Coordinators: $NRCOORDINATORS

if [ ! -z "$3" ] ; then
    if [ "$3" == "C" ] ; then
        COORDINATORCONSOLE=1
        echo Starting one coordinator in terminal with --console
    elif [ "$3" == "D" ] ; then
        CLUSTERDEBUGGER=1
        echo Running cluster in debugger.
    elif [ "$3" == "R" ] ; then
        RRDEBUGGER=1
        echo Running cluster in rr with --console.
    fi
fi

SECONDARIES="$4"

rm -rf cluster
mkdir cluster
echo Starting agency...
build/bin/arangod \
  -c none \
  --agency.endpoint tcp://127.0.0.1:4001 \
  --agency.id 0 \
  --agency.size 1 \
  --agency.wait-for-sync false \
  --agency.supervision true \
  --agency.supervision-frequency 5 \
  --database.directory cluster/data4001 \
  --javascript.app-path ./js/apps \
  --javascript.startup-directory ./js \
  --javascript.v8-contexts 1 \
  --log.file cluster/4001.log \
  --server.authentication false \
  --server.endpoint tcp://127.0.0.1:4001 \
  --server.statistics false \
  --agency.compaction-step-size 100 \
  --log.force-direct true \
  > cluster/4001.stdout 2>&1 &
sleep 1

start() {
    if [ "$1" == "dbserver" ]; then
      ROLE="PRIMARY"
    elif [ "$1" == "coordinator" ]; then
      ROLE="COORDINATOR"
      sleep 1
    fi
    TYPE=$1
    PORT=$2
    mkdir cluster/data$PORT
    echo Starting $TYPE on port $PORT
    build/bin/arangod -c none \
                --database.directory cluster/data$PORT \
                --cluster.agency-endpoint tcp://127.0.0.1:4001 \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
                --log.level info \
                --server.statistics false \
                --server.threads 5 \
                --javascript.startup-directory ./js \
                --server.authentication false \
                --javascript.app-path ./js/apps \
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
                --cluster.agency-endpoint tcp://127.0.0.1:4001 \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
                --server.statistics false \
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
                --cluster.agency-endpoint tcp://127.0.0.1:4001 \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
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
                --cluster.agency-endpoint tcp://127.0.0.1:4001 \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
                --server.statistics false \
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
                --cluster.agency-endpoint tcp://127.0.0.1:4001 \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-id $CLUSTER_ID \
                --log.file cluster/$PORT.log \
                --server.statistics false \
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

