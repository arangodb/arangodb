#!/usr/bin/env bash

ulimit -H -n 131072 || true
ulimit -S -n 131072 || true

function help() {
  echo "USAGE: scripts/startStandAloneAgency.sh [options]"
  echo ""
  echo "OPTIONS:"
  echo "  -a/--agency-size     Agency size (odd integer      default: 3))"
  echo "  -p/--pool-size       Pool size   (>= agency size   default: [agency size])"
  echo "  -t/--transport       Protocol    (ssl|tcp          default: tcp)"
  echo "  -l/--log-level       Log level   (INFO|DEBUG|TRACE default: INFO)"
  echo "  -w/--wait-for-sync   Boolean     (true|false       default: true)"
  echo "  -m/--use-microtime   Boolean     (true|false       default: false)"
  echo "  -s/--start-delays    Integer     (                 default: 0)"
  echo "  -r/--random-delays   Integer     (true|false       default: false)"
  echo "  -g/--gossip-mode     Integer     (0: Announce first endpoint to all"
  echo "                                    1: Grow list of known endpoints for each"
  echo "                                    2: Cyclic        default: 0)"
  echo "  -b/--offset-ports    Offsetports (default: 0, i.e. A:5001)"
  echo "  -u/--use-persistence Boolean     (true|false       default: false)"
  echo ""
  echo "EXAMPLES:"
  echo "  scripts/startStandaloneAgency.sh"
  echo "  scripts/startStandaloneAgency.sh -a 5 -p 10 -t ssl"
  echo "  scripts/startStandaloneAgency.sh --agency-size 3 --pool-size 5"
  
}

function shuffle() {
  local i tmp size max rand

  size=${#aaid[*]}
  max=$(( 32768 / size * size ))

  for ((i=size-1; i>0; i--)); do
    while (( (rand=$RANDOM) >= max )); do :; done
    rand=$(( rand % (i+1) ))
    tmp=${aaid[i]} aaid[i]=${aaid[rand]} aaid[rand]=$tmp
  done
}

function isuint () {
  re='^[0-9]+$'
  if ! [[ $1 =~ $re ]] ; then
      return 1;
  else
      return 0
  fi
}

NRAGENTS=3
POOLSZ=""
TRANSPORT="tcp"
LOG_LEVEL=""
WAIT_FOR_SYNC="true"
USE_MICROTIME="false"
GOSSIP_MODE=0
START_DELAYS=0
INTERACTIVE_MODE=""
XTERM="xterm"
XTERMOPTIONS=""
BUILD="build"

while [[ ${1} ]]; do
  case "${1}" in
    -a|--agency-size)
      NRAGENTS=${2}
      shift;;
    -i|--interactive)
      INTERACTIVE_MODE=${2}
      shift
      ;;
    -p|--pool-size)
      POOLSZ=${2}
      shift;;
    -t|--transport)
      TRANSPORT=${2}
      shift;;
    -l|--log-level)
      case ${2} in
        =*)
          LOG_LEVEL="$LOG_LEVEL --log.level ${2}"
          ;;
        *=*)
          LOG_LEVEL="$LOG_LEVEL --log.level ${2}"
          ;;
        *)
          LOG_LEVEL="$LOG_LEVEL --log.level agency=${2}"
          ;;
      esac
        
      shift
      ;;
    -w|--wait-for-sync)
      WAIT_FOR_SYNC=${2}
      shift;;
    -m|--use-microtime)
      USE_MICROTIME=${2}
      shift;;
    -g|--gossip-mode)
      GOSSIP_MODE=${2}
      shift;;
    -r|--random-delays)
      RANDOM_DELAYS=${2}
      shift;;
    -s|--start-delays)
      START_DELAYS=${2}
      shift;;
    -b|--port-offset)
      PORT_OFFSET=${2}
      shift
      ;;
    -u|--use-persistence)
      if [ "${2}" == "true" ] ; then
          USE_PERSISTENCE=true
      fi
      shift
      ;;
    -h|--help)
      help; exit 1  
      ;;
    *)
      echo "Unknown parameter: ${1}" >&2
      help; exit 1
      ;;
  esac
  
  if ! shift; then
    echo 'Missing parameter argument.' >&2
    exit 1
  fi
done

if [ "$LOG_LEVEL" == "" ];  then
  LOG_LEVEL="--log.level agency=info"
fi

if [ "$POOLSZ" == "" ]; then
  POOLSZ=$NRAGENTS
fi

if [ "$TRANSPORT" == "ssl" ]; then
  SSLKEYFILE="--ssl.keyfile UnitTests/server.pem"
  CURL="curl --insecure -ks https://"
else
  SSLKEYFILE=""
  CURL="curl -s http://"
fi

printf "Starting agency ... \n"
printf "    agency-size: %s," "$NRAGENTS"
printf " pool-size: %s," "$POOLSZ"
printf " transport: %s," "$TRANSPORT"
printf " log-level: %s," "$LOG_LEVEL"
printf "\n"
printf "    use-microtime: %s," "$USE_MICROTIME"
printf " wait-for-sync: %s," "$WAIT_FOR_SYNC"
printf " start-delays: %s," "$START_DELAYS"
printf " random-delays: %s," "$RANDOM_DELAYS"
printf " gossip-mode: %s\n" "$GOSSIP_MODE"

if [ ! -d arangod ] || [ ! -d arangosh ] || [ ! -d UnitTests ] ; then
  echo Must be started in the main ArangoDB source directory.
  exit 1
fi

if [[ $(( $NRAGENTS % 2 )) == 0 ]]; then
  echo Number of agents must be odd.
  exit 1
fi


if [ ! -z "$INTERACTIVE_MODE" ] ; then
    if [ "$INTERACTIVE_MODE" == "C" ] ; then
        ARANGOD="${BUILD}/bin/arangod "
        CO_ARANGOD="$XTERM $XTERMOPTIONS -e ${BUILD}/bin/arangod --console "
        echo "Starting one coordinator in terminal with --console"
    elif [ "$INTERACTIVE_MODE" == "R" ] ; then
        ARANGOD="rr ${BUILD}/bin/arangod "
        CO_ARANGOD=$ARANGOD
        echo Running cluster in rr with --console.
    fi
else
    ARANGOD="${BUILD}/bin/arangod "
    CO_ARANGOD=$ARANGOD
fi

SFRE=2.5
COMP=1000
KEEP=50000
BASE=$(( $PORT_OFFSET + 5000 ))

if [ "$GOSSIP_MODE" = "0" ]; then
  GOSSIP_PEERS=" --agency.endpoint $TRANSPORT://[::1]:$BASE"
fi

if [ -z "$USE_PERSISTENCE" ]; then
  rm -rf agency
fi

mkdir -p agency
PIDS=""

aaid=(`seq 0 $(( $POOLSZ - 1 ))`)
#shuffle

count=1


for aid in "${aaid[@]}"; do

  port=$(( $BASE + $aid ))

  if [ "$GOSSIP_MODE" = "2" ]; then
    nport=$(( $BASE + $(( $(( $aid + 1 )) % 3 ))))
    GOSSIP_PEERS=" --agency.endpoint $TRANSPORT://[::1]:$nport"
  fi

  if [ "$GOSSIP_MODE" = "3" ]; then
    GOSSIP_PEERS=""
    for id in "${aaid[@]}"; do
      if [ ! "$id" = "$aid" ]; then
        nport=$(( $BASE + $(( $id )) ))
        GOSSIP_PEERS+=" --agency.endpoint $TRANSPORT://[::1]:$nport"
      fi
    done
  fi
  
  printf "    starting agent %s " "$aid"
  $ARANGOD \
    -c none \
    --agency.activate true \
    $GOSSIP_PEERS \
    --agency.my-address $TRANSPORT://[::1]:$port \
    --agency.compaction-step-size $COMP \
    --agency.compaction-keep-size $KEEP \
    --agency.pool-size $POOLSZ \
    --agency.size $NRAGENTS \
    --agency.supervision true \
    --agency.supervision-frequency $SFRE \
    --agency.wait-for-sync $WAIT_FOR_SYNC \
    --database.directory agency/data$port \
    --javascript.enabled false \
    --log.file agency/$port.log \
    --log.force-direct false \
    $LOG_LEVEL \
    --log.use-microtime $USE_MICROTIME \
    --server.descriptors-minimum 0 \
    --server.authentication false \
    --server.endpoint $TRANSPORT://[::]:$port \
    --server.statistics false \
    $SSLKEYFILE \
    | tee agency/$PORT.stdout 2>&1 &
  PIDS+=$!
  PIDS+=" "
  if [ "$GOSSIP_MODE" == "1" ]; then
    GOSSIP_PEERS+=" --agency.endpoint $TRANSPORT://[::1]:$port"
  fi
  if [ $count -lt $POOLSZ ]; then
    if isuint $START_DELAYS; then
      printf "fixed delay %02ds " "$START_DELAYS"
      sleep $START_DELAYS
    fi
    if [ "$RANDOM_DELAYS" == "true" ] ; then
      delay=$(( RANDOM % 16 ))
      printf "random delay %02ds" "$delay"
      sleep $delay
    fi
    ((count+=1))
  fi
  echo
done

echo "  done. Your agents are ready at port $BASE onward."
#echo "Process ids: $PIDS"
echo "Try ${CURL}[::1]:5000/_api/agency/config."


