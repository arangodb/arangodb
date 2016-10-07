#!/bin/bash

function help() {
  echo "USAGE: scripts/startLocalCluster.sh [options]"
  echo ""
  echo "OPTIONS:"
  echo "  -a/--nagents       # agents            (odd integer      default: 1))"
  echo "  -c/--ncoordinators # coordinators      (odd integer      default: 1))"
  echo "  -d/--ndbservers    # db servers        (odd integer      default: 2))"
  echo "  -s/--secondaries   Start secondaries   (0|1              default: 0)"
  echo "  -t/--transport     Protocol            (ssl|tcp          default: tcp)"
  echo "     --log-level-a   Log level (agency)  (INFO|DEBUG|TRACE default: INFO)"
  echo "     --log-level-c   Log level (cluster) (INFO|DEBUG|TRACE default: INFO)"
  echo "  -i/--interactive   Interactive mode    (C|D|R            default: '')"
  
  echo "  -x/--xterm         XTerm command       (default: xterm)"
  echo "  -o/--xterm-options XTerm options       (default: --geometry=80x43)"
  echo ""
  echo "EXAMPLES:"
  echo "  scripts/startLocalCluster.sh"
  echo "  scripts/startLocalCluster.sh -a 1 -c 1 -d 3 -t ssl"
  echo "  scripts/startLocalCluster.sh -a 3 -c 1 -d 2 -t tcp -i C"
  
}

# defaults
NRAGENTS=1
NRDBSERVERS=2
NRCOORDINATORS=1
POOLSZ=""
TRANSPORT="tcp"
LOG_LEVEL="INFO"
XTERM="x-terminal-emulator"
XTERMOPTIONS="--geometry=80x43"
SECONDARIES=0
BUILD="build"

while [[ ${1} ]]; do
    case "${1}" in
    -a|--agency-size)
      NRAGENTS=${2}
      shift
      ;;
    -c|--ncoordinators)
      NRCOORDINATORS=${2}
      shift
      ;;
    -d|--ndbservers)
      NRDBSERVERS=${2}
      shift
      ;;
    -s|--secondaries)
      SECONDARIES=${2}
      shift
      ;;
    -t|--transport)
      TRANSPORT=${2}
      shift
      ;;
    --log-level-agency)
      LOG_LEVEL_AGENCY=${2}
      shift
      ;;
    --log-level-cluster)
      LOG_LEVEL_CLUSTER=${2}
      shift
      ;;
    -i|--interactive)
      INTERACTIVE_MODE=${2}
      shift
      ;;
    -x|--xterm)
      XTERM=${2}
      shift
      ;;
    -o|--xterm-options)
      XTERMOPTIONS=${2}
      shift
      ;;
    -h|--help)
      help
      exit 1  
      ;;
    -B|--build)
      BUILD=${2}
      shift
      ;;
    *)
      echo "Unknown parameter: ${1}" >&2
      help
      exit 1
      ;;
  esac
  
  if ! shift; then
    echo 'Missing parameter argument.' >&2
    return 1
  fi
done

if [ "$POOLSZ" == "" ] ; then
  POOLSZ=$NRAGENTS
fi


if [ "$TRANSPORT" == "ssl" ]; then
  SSLKEYFILE="--ssl.keyfile UnitTests/server.pem"
  CURL="curl --insecure -s -f -X GET https:"
else
  SSLKEYFILE=""
  CURL="curl -s -f -X GET http:"
fi

printf "Starting agency ... \n"
printf "  # agents: %s," "$NRAGENTS"
printf " # db servers: %s," "$NRDBSERVERS"
printf " # coordinators: %s," "$NRCOORDINATORS"
printf " transport: %s\n" "$TRANSPORT"

if [ ! -d arangod ] || [ ! -d arangosh ] || [ ! -d UnitTests ] ; then
  echo Must be started in the main ArangoDB source directory.
  exit 1
fi

if [[ $(( $NRAGENTS % 2 )) == 0 ]]; then
  echo "**ERROR: Number of agents must be odd! Bailing out."
  exit 1
fi

if [ ! -d arangod ] || [ ! -d arangosh ] || [ ! -d UnitTests ] ; then
    echo "Must be started in the main ArangoDB source directory! Bailing out."
    exit 1
fi

if [ ! -z "$INTERACTIVE_MODE" ] ; then
    if [ "$INTERACTIVE_MODE" == "C" ] ; then
        COORDINATORCONSOLE=1
        echo "Starting one coordinator in terminal with --console"
    elif [ "$INTERACTIVE_MODE" == "D" ] ; then
        CLUSTERDEBUGGER=1
        echo Running cluster in debugger.
    elif [ "$INTERACTIVE_MODE" == "R" ] ; then
        RRDEBUGGER=1
        echo Running cluster in rr with --console.
    fi
fi

MINP=0.5
MAXP=2.5
SFRE=5.0
COMP=1000
BASE=4001
NATH=$(( $NRDBSERVERS + $NRCOORDINATORS + $NRAGENTS ))

#rm -rf cluster
if [ -d cluster-init ];then
  cp -a cluster-init cluster
fi
mkdir -p cluster

echo Starting agency ... 
for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    port=$(( $BASE + $aid ))
    ${BUILD}/bin/arangod \
        -c none \
        --agency.activate true \
        --agency.compaction-step-size $COMP \
        --agency.election-timeout-min $MINP \
        --agency.election-timeout-max $MAXP \
        --agency.endpoint $TRANSPORT://localhost:$BASE \
        --agency.my-address $TRANSPORT://localhost:$port \
        --agency.pool-size $NRAGENTS \
        --agency.size $NRAGENTS \
        --agency.supervision true \
        --agency.supervision-frequency $SFRE \
        --agency.supervision-grace-period 15 \
        --agency.wait-for-sync false \
        --database.directory cluster/data$port \
        --javascript.app-path ./js/apps \
        --javascript.startup-directory ./js \
        --javascript.module-directory ./enterprise/js \
        --javascript.v8-contexts 1 \
        --server.authentication false \
        --server.endpoint $TRANSPORT://0.0.0.0:$port \
        --server.statistics false \
        --server.threads 16 \
        --log.file cluster/$port.log \
        --log.force-direct true \
        $SSLKEYFILE \
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
    ${BUILD}/bin/arangod \
       -c none \
       --database.directory cluster/data$PORT \
       --cluster.agency-endpoint $TRANSPORT://127.0.0.1:$BASE \
       --cluster.my-address $TRANSPORT://127.0.0.1:$PORT \
       --server.endpoint $TRANSPORT://127.0.0.1:$PORT \
       --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
       --cluster.my-role $ROLE \
       --log.file cluster/$PORT.log \
       --log.level info \
       --server.statistics true \
       --server.threads 5 \
       --server.authentication false \
       --javascript.startup-directory ./js \
       --javascript.module-directory ./enterprise/js \
       --javascript.app-path cluster/apps$PORT \
       --log.force-direct true \
        $SSLKEYFILE \
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
    $XTERM $XTERMOPTIONS -e ${BUILD}/bin/arangod \
        -c none \
        --database.directory cluster/data$PORT \
        --cluster.agency-endpoint $TRANSPORT://127.0.0.1:$BASE \
        --cluster.my-address $TRANSPORT://127.0.0.1:$PORT \
        --server.endpoint $TRANSPORT://127.0.0.1:$PORT \
        --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
        --cluster.my-role $ROLE \
        --log.file cluster/$PORT.log \
        --log.level info \
        --server.statistics true \
        --server.threads 5 \
        --javascript.startup-directory ./js \
        --javascript.module-directory ./enterprise/js \
        --javascript.app-path ./js/apps \
        --server.authentication false \
        $SSLKEYFILE \
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
    ${BUILD}/bin/arangod \
      -c none \
      --database.directory cluster/data$PORT \
      --cluster.agency-endpoint $TRANSPORT://127.0.0.1:$BASE \
      --cluster.my-address $TRANSPORT://127.0.0.1:$PORT \
      --server.endpoint $TRANSPORT://127.0.0.1:$PORT \
      --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
      --cluster.my-role $ROLE \
      --log.file cluster/$PORT.log \
      --log.level info \
      --server.statistics false \
      --server.threads 5 \
      --javascript.startup-directory ./js \
      --javascript.module-directory ./enterprise/js \
      --javascript.app-path ./js/apps \
      $SSLKEYFILE \
      --server.authentication false &
      $XTERM $XTERMOPTIONS -e gdb ${BUILD}/bin/arangod -p $! &
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
    $XTERM $XTERMOPTIONS -e rr ${BUILD}/bin/arangod \
        -c none \
        --database.directory cluster/data$PORT \
        --cluster.agency-endpoint $TRANSPORT://127.0.0.1:$BASE \
        --cluster.my-address $TRANSPORT://127.0.0.1:$PORT \
        --server.endpoint $TRANSPORT://127.0.0.1:$PORT \
        --cluster.my-local-info $TYPE:127.0.0.1:$PORT \
        --cluster.my-role $ROLE \
        --log.file cluster/$PORT.log \
        --log.level info \
        --server.statistics true \
        --server.threads 5 \
        --javascript.startup-directory ./js \
        --javascript.module-directory ./enterprise/js \
        --javascript.app-path ./js/apps \
        --server.authentication false \
        $SSLKEYFILE \
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
if [ "$NRCOORDINATORS" -gt 0 ]
then
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
fi

if [ "$CLUSTERDEBUGGER" == "1" ] ; then
    echo Waiting for you to setup debugger windows, hit RETURN to continue!
    read
fi

echo Waiting for cluster to come up...

testServer() {
    PORT=$1
    while true ; do
        ${CURL}//127.0.0.1:$PORT/_api/version > /dev/null 2>&1
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
if [ "$NRCOORDINATORS" -gt 0 ]
then
  for p in `seq 8530 $PORTTOPCO` ; do
      testServer $p
  done
fi

if [ "$SECONDARIES" == "1" ] ; then
    let index=1
    PORTTOPSE=`expr 8729 + $NRDBSERVERS - 1` 
    for PORT in `seq 8729 $PORTTOPSE` ; do
        mkdir cluster/data$PORT
        
        CLUSTER_ID="Secondary$index"
        
        echo Registering secondary $CLUSTER_ID for "DBServer$index"
        curl -f -X PUT --data "{\"primary\": \"DBServer$index\", \"oldSecondary\": \"none\", \"newSecondary\": \"$CLUSTER_ID\"}" -H "Content-Type: application/json" localhost:8530/_admin/cluster/replaceSecondary
        echo Starting Secondary $CLUSTER_ID on port $PORT
        ${BUILD}/bin/arangod \
            -c none \
            --database.directory cluster/data$PORT \
            --cluster.agency-endpoint $TRANSPORT://127.0.0.1:$BASE \
            --cluster.my-address $TRANSPORT://127.0.0.1:$PORT \
            --server.endpoint $TRANSPORT://127.0.0.1:$PORT \
            --cluster.my-id $CLUSTER_ID \
            --log.file cluster/$PORT.log \
            --server.statistics true \
            --javascript.startup-directory ./js \
            --javascript.module-directory ./enterprise/js \
            --server.authentication false \
            $SSLKEYFILE \
            --javascript.app-path ./js/apps \
            > cluster/$PORT.stdout 2>&1 &
            
            let index=$index+1
    done
fi

echo Done, your cluster is ready at
if [ "$NRCOORDINATORS" -gt 0 ]
then
for p in `seq 8530 $PORTTOPCO` ; do
    echo "   ${BUILD}/bin/arangosh --server.endpoint $TRANSPORT://127.0.0.1:$p"
done
fi

