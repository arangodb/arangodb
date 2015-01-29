#!/bin/bash
if [ ! -d arangod or ! -d arangosh or ! -d UnitTests ] ; then
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

rm -rf cluster
mkdir cluster
cd cluster
echo Starting agency...
../bin/etcd-arango 2>&1 > /dev/null &
cd ..
sleep 1
echo Initializing agency...
bin/arangosh --javascript.execute scripts/init_agency.js 2>&1 > cluster/init_agency.log
echo Starting discovery...
bin/arangosh --javascript.execute scripts/discover.js 2>&1 > cluster/discover.log &

start() {
    TYPE=$1
    PORT=$2
    mkdir cluster/data$PORT
    echo Starting $TYPE on port $PORT
    bin/arangod --database.directory cluster/data$PORT \
                --cluster.agency-endpoint tcp://localhost:4001 \
                --cluster.my-address tcp://localhost:$PORT \
                --server.endpoint tcp://localhost:$PORT \
                --cluster.my-local-info $TYPE:localhost:$PORT \
                --log.file cluster/$PORT.log 2>&1 > cluster/$PORT.stdout &
}

PORTTOPDB=`expr 8629 + $NRDBSERVERS - 1`
for p in `seq 8629 $PORTTOPDB` ; do
    start dbserver $p
done
PORTTOPCO=`expr 8530 + $NRCOORDINATORS - 1`
for p in `seq 8530 $PORTTOPCO` ; do
    start coordinator $p
done

echo Waiting for cluster to come up...

testServer() {
    PORT=$1
    while true ; do
        sleep 1
        curl -s -X GET "http://localhost:$PORT/_api/version" 2>&1 > /dev/null
        if [ "$?" != "0" ] ; then
            echo Server on port $PORT does not answer yet.
        else
            echo Server on port $PORT is ready for business.
            break
        fi
    done
}

testServer $PORTTOPCO
if [ "8530" != "$PORTTOPCO" ] ; then
    testServer 8530
fi

echo Bootstrapping DBServers...
for p in `seq 8629 $PORTTOPDB` ; do
    curl -s -X POST "http://localhost:$p/_admin/cluster/bootstrapDbServers" \
         -d '{"isRelaunch":false}' 2>&1 >> cluster/DBServer.boot.log
done

echo Running DB upgrade on cluster...
curl -s -X POST "http://localhost:8530/_admin/cluster/upgradeClusterDatabase" \
     -d '{"isRelaunch":false}' 2>&1 >> cluster/DBUpgrade.log

echo Bootstrapping Coordinators...
for p in `seq 8530 $PORTTOPCO` ; do
    curl -s -X POST "http://localhost:8530/_admin/cluster/bootstrapCoordinator" \
         -d '{"isRelaunch":false}' 2>&1 >> cluster/Coordinator.boot.log
done

echo Done, your cluster is ready at
for p in `seq 8530 $PORTTOPCO` ; do
    echo "   bin/arangosh --server.endpoint tcp://localhost:$p"
done

