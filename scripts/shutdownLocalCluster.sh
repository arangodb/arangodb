#!/bin/bash

. `dirname $0`/cluster-run-common.sh

echo Number of Agents: $NRAGENTS
echo Number of DBServers: $NRDBSERVERS
echo Number of Coordinators: $NRCOORDINATORS

shutdown() {
    PORT=$1
    echo -n "$PORT "
    curl -X DELETE http://localhost:$PORT/_admin/shutdown >/dev/null 2>/dev/null
    echo
}

if [ "$SECONDARIES" == "1" ]; then
  echo "Shutting down secondaries..."
  PORTTOPSE=`expr 8729 + $NRDBSERVERS - 1` 
  for PORT in `seq 8729 $PORTTOPSE` ; do
    shutdown $PORT
  done
fi

echo Shutting down Coordinators...
PORTTOPCO=`expr 8530 + $NRCOORDINATORS - 1`
for p in `seq 8530 $PORTTOPCO` ; do
    shutdown $p
done

echo Shutting down DBServers...
PORTTOPDB=`expr 8629 + $NRDBSERVERS - 1`
for p in `seq 8629 $PORTTOPDB` ; do
    shutdown $p
done

testServerDown() {
    PORT=$1
    while true ; do
        curl -s -f -X GET "http://127.0.0.1:$PORT/_api/version" > /dev/null 2>&1
        if [ "$?" != "0" ] ; then
            echo Server on port $PORT does not answer any more.
            break
        else
            echo Server on port $PORT is still running
        fi
        sleep 1
    done
}

for p in `seq 8530 $PORTTOPCO` ; do
    testServerDown $p
done

for p in `seq 8629 $PORTTOPDB` ; do
    testServerDown $p
done

# Currently the agency does not wait for all servers to shutdown
# This causes a race condisiton where all servers wait to tell the agency
# they are shutting down
sleep 10

echo Shutting down agency ... 
for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    port=$(( 4001 + $aid ))
    shutdown $port
done

echo Done, your cluster is gone

