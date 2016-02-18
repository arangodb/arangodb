#!/bin/bash
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
        echo Starting one coordinator in xterm with --console
    elif [ "$3" == "D" ] ; then
        CLUSTERDEBUGGER=1
        echo Running cluster in debugger.
    fi
fi

if [ -z "$XTERMOPTIONS" ] ; then
    XTERMOPTIONS="-fa Monospace-14 -bg white -fg black -geometry 80x43"
fi

rm -rf cluster
mkdir cluster
cd cluster
echo Starting agency...
../bin/etcd-arango > /dev/null 2>&1 &
cd ..
sleep 1

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
    bin/arangod --database.directory cluster/data$PORT \
                --cluster.agency-endpoint tcp://127.0.0.1:4001 \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
                --log.requests-file cluster/$PORT.req \
                --server.disable-statistics true \
                --server.foxx-queues false \
                --server.foxx-queues false \
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
    xterm $XTERMOPTIONS -e bin/arangod --database.directory cluster/data$PORT \
                --cluster.agency-endpoint tcp://127.0.0.1:4001 \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
                --log.requests-file cluster/$PORT.req \
                --server.disable-statistics true \
                --server.foxx-queues false \
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
    bin/arangod --database.directory cluster/data$PORT \
                --cluster.agency-endpoint tcp://127.0.0.1:4001 \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.file cluster/$PORT.log \
                --log.requests-file cluster/$PORT.req \
                --server.disable-statistics true \
                --server.foxx-queues false &
    xterm $XTERMOPTIONS -title "$TYPE $PORT" -e gdb bin/arangod -p $! &
}

PORTTOPDB=`expr 8629 + $NRDBSERVERS - 1`
for p in `seq 8629 $PORTTOPDB` ; do
    if [ "$CLUSTERDEBUGGER" == "1" ] ; then
        startDebugger dbserver $p
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
        sleep 1
        curl -s -X GET "http://127.0.0.1:$PORT/_api/version" > /dev/null 2>&1
        if [ "$?" != "0" ] ; then
            echo Server on port $PORT does not answer yet.
        else
            echo Server on port $PORT is ready for business.
            break
        fi
    done
}

for p in `seq 8530 $PORTTOPCO` ; do
    testServer $p
done

echo Bootstrapping DBServers...
curl -s -X POST "http://127.0.0.1:8530/_admin/cluster/bootstrapDbServers" \
     -d '{"isRelaunch":false}' >> cluster/DBServersUpgrade.log 2>&1

echo Running DB upgrade on cluster...
curl -s -X POST "http://127.0.0.1:8530/_admin/cluster/upgradeClusterDatabase" \
     -d '{"isRelaunch":false}' >> cluster/DBUpgrade.log 2>&1

echo Bootstrapping Coordinators...
PIDS=""
for p in `seq 8530 $PORTTOPCO` ; do
    curl -s -X POST "http://127.0.0.1:$p/_admin/cluster/bootstrapCoordinator" \
         -d '{"isRelaunch":false}' >> cluster/Coordinator.boot.log 2>&1 &
    PIDS="$PIDS $!"
done

echo Pids: $PIDS
for p in $PIDS ; do
    wait $p
    echo PID $p done
done

echo Done, your cluster is ready at
for p in `seq 8530 $PORTTOPCO` ; do
    echo "   bin/arangosh --server.endpoint tcp://127.0.0.1:$p"
done

