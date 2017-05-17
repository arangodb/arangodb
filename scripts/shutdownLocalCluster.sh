#!/bin/bash

. `dirname $0`/cluster-run-common.sh

echo Number of Agents: $NRAGENTS
echo Number of DBServers: $NRDBSERVERS
echo Number of Coordinators: $NRCOORDINATORS

AG_BASE=$(( $PORT_OFFSET + 4001 ))
CO_BASE=$(( $PORT_OFFSET + 8530 ))
DB_BASE=$(( $PORT_OFFSET + 8629 ))
SE_BASE=$(( $PORT_OFFSET + 8729 ))

LOCALHOST="[::1]"
ANY="[::]"

shutdown() {
  PORT=$1
  echo -n "$PORT "
  curl -X DELETE http://$LOCALHOST:$PORT/_admin/shutdown >/dev/null 2>/dev/null
}

if [ "$SECONDARIES" == "1" ]; then
  echo Shutting down secondaries...
  PORTHISE=$(expr $SE_BASE + $NRDBSERVERS - 1)
  for p in $(seq $SE_BASE $PORTHISE) ; do
    shutdown $PORT
  done
fi

echo -n "  Shutting down Coordinators... "
PORTHICO=$(expr $CO_BASE + $NRCOORDINATORS - 1)
for p in $(seq $CO_BASE $PORTHICO) ; do
  shutdown $p
done

echo
echo -n "  Shutting down DBServers... "
PORTHIDB=$(expr $DB_BASE + $NRDBSERVERS - 1)
for p in $(seq $DB_BASE $PORTHIDB) ; do
  shutdown $p
done

testServerDown() {
  PORT=$1
  while true ; do
    curl -s -f -X GET "http://$LOCALHOST:$PORT/_api/version" > /dev/null 2>&1
    if [ "$?" != "0" ] ; then
      pid=$(ps -eaf|grep data$PORT|grep -v grep|awk '{print $2}')
      if [ -z $pid ]; then
        break
      fi
    fi
    sleep 1
  done
}

for p in $(seq $CO_BASE $PORTHICO) ; do
    testServerDown $p
done

for p in $(seq $DB_BASE $PORTHIDB) ; do
    testServerDown $p
done

# Currently the agency does not wait for all servers to shutdown
# This causes a race condisiton where all servers wait to tell the agency
# they are shutting down

echo
echo -n "  Shutting down agency ... "

PORTHIAG=$(expr $AG_BASE + $NRAGENTS - 1)
for p in $(seq $AG_BASE $PORTHIAG) ; do
  shutdown $p
done

for p in $(seq $AG_BASE $PORTHIAG) ; do
    testServerDown $p
done

echo
echo Done, your cluster is gone

