#!/bin/bash
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

shutdown() {
    PORT=$1
    echo -n "$PORT "
    curl -X DELETE http://localhost:$PORT/_admin/shutdown >/dev/null 2>/dev/null
    echo
}

if [ -n "$SECONDARIES" ]; then
  echo "Shutting down secondaries..."
  PORTTOPSE=`expr 8729 + $NRDBSERVERS - 1` 
  for PORT in `seq 8729 $PORTTOPSE` ; do
    shutdown $PORT
  done
fi

echo Shutting down Coordiantors...
PORTTOPCO=`expr 8530 + $NRCOORDINATORS - 1`
for p in `seq 8530 $PORTTOPCO` ; do
    shutdown $p
done

echo Shutting down DBServers...
PORTTOPDB=`expr 8629 + $NRDBSERVERS - 1`
for p in `seq 8629 $PORTTOPDB` ; do
    shutdown $p
done

sleep 1

echo Shutting down agency ... 
for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    port=$(( 4001 + $aid ))
    shutdown $port
done

echo Done, your cluster is gone

