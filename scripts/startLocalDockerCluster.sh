#!/usr/bin/env bash
DOCKERIMAGE=arangodb/arangodb:3.0.0-p4
if [ -z "$XTERM" ] ; then
    XTERM=x-terminal-emulator
fi
if [ -z "$XTERMOPTIONS" ] ; then
    XTERMOPTIONS="--geometry=80x43"
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
    fi
fi

SECONDARIES="$4"

echo Starting agency...
docker run -d --net=host -e ARANGO_NO_AUTH=1 --name=agency \
  ${DOCKERIMAGE} \
  --agency.endpoint tcp://0.0.0.0:4001 \
  --agency.id 0 \
  --agency.size 1 \
  --agency.wait-for-sync false \
  --agency.supervision false \
  --agency.supervision-frequency 5 \
  --server.endpoint tcp://0.0.0.0:4001 \
  --server.statistics false 
sleep 1

start() {
    if [ "$1" == "dbserver" ]; then
      ROLE="DBSERVER"
    elif [ "$1" == "coordinator" ]; then
      ROLE="COORDINATOR"
    fi
    TYPE=$1
    PORT=$2
    echo Starting $TYPE on port $PORT
    docker run -d --net=host -e ARANGO_NO_AUTH=1 --name="$TYPE_$PORT" \
      ${DOCKERIMAGE} \
                --cluster.agency-endpoint tcp://127.0.0.1:4001 \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --log.level trace \
                --server.statistics false
}

startTerminal() {
    if [ "$1" == "dbserver" ]; then
      ROLE="DBSERVER"
    elif [ "$1" == "coordinator" ]; then
      ROLE="COORDINATOR"
    fi
    TYPE=$1
    PORT=$2
    echo Starting $TYPE on port $PORT
    $XTERM $XTERMOPTIONS -e docker run -it --net=host -e ARANGO_NO_AUTH=1 \
      --name="$TYPE_$PORT" \
      ${DOCKERIMAGE} \
                --cluster.agency-endpoint tcp://127.0.0.1:4001 \
                --cluster.my-address tcp://127.0.0.1:$PORT \
                --server.endpoint tcp://127.0.0.1:$PORT \
                --cluster.my-role $ROLE \
                --server.statistics false \
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

echo Done, your cluster is ready at
for p in `seq 8530 $PORTTOPCO` ; do
    echo "   build/bin/arangosh --server.endpoint tcp://127.0.0.1:$p"
done

