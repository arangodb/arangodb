#!/bin/bash

function help() {
  echo "USAGE: scripts/startStandAloneAgency.sh [options]"
  echo ""
  echo "OPTIONS:"
  echo "  -a/--agency-size   Agency size (odd integer      default: 3))"
  echo "  -p/--pool-size     Pool size   (>= agency size   default: [agency size])"
  echo "  -t/--transport     Protocol    (ssl|tcp          default: tcp)"
  echo "  -l/--log-level     Log level   (INFO|DEBUG|TRACE default: INFO)"
  echo "  -w/--wait-for-sync Boolean     (true|false       default: true)"
  echo "  -m/--use-microtime Boolean     (true|false       default: false)"
  echo "  -s/--start-delays  Integer     (                 default: 0)"
  echo "  -r/--random-delays Integer     (true|false       default: false)"
  echo "  -g/--gossip-mode   Integer     (0: Announce first endpoint to all"
  echo "                                  1: Grow list of known endpoints for each"
  echo "                                  2: Cyclic        default: 0)"
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

while [[ ${1} ]]; do
  case "${1}" in
    -a|--agency-size)
      NRAGENTS=${2}
      shift;;
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

MINP=0.5
MAXP=2.0
SFRE=2.5
COMP=1000
BASE=5000

if [ "$GOSSIP_MODE" = "0" ]; then
   GOSSIP_PEERS=" --agency.endpoint $TRANSPORT://localhost:$BASE"
fi

rm -rf agency
mkdir -p agency
PIDS=""

aaid=(`seq 0 $(( $POOLSZ - 1 ))`)
shuffle

count=1

for aid in "${aaid[@]}"; do

  port=$(( $BASE + $aid ))
  if [ "$GOSSIP_MODE" = 2 ]; then
    nport=$(( $BASE + $(( $(( $aid + 1 )) % 3 ))))
    GOSSIP_PEERS=" --agency.endpoint $TRANSPORT://localhost:$nport"
  fi
  printf "    starting agent %s " "$aid"
  build/bin/arangod \
    -c none \
    --agency.activate true \
    $GOSSIP_PEERS \
    --agency.my-address $TRANSPORT://localhost:$port \
    --agency.compaction-step-size $COMP \
    --agency.pool-size $POOLSZ \
    --agency.size $NRAGENTS \
    --agency.supervision true \
    --agency.supervision-frequency $SFRE \
    --agency.wait-for-sync $WAIT_FOR_SYNC \
    --database.directory agency/data$port \
    --javascript.app-path ./js/apps \
    --javascript.startup-directory ./js \
    --javascript.v8-contexts 1 \
    --log.file agency/$port.log \
    --log.force-direct true \
    $LOG_LEVEL \
    --log.use-microtime $USE_MICROTIME \
    --server.authentication false \
    --server.endpoint $TRANSPORT://0.0.0.0:$port \
    --server.statistics false \
    $SSLKEYFILE \
    > agency/$port.stdout 2>&1 &
  PIDS+=$!
  PIDS+=" "
  if [ "$GOSSIP_MODE" == "1" ]; then
    GOSSIP_PEERS+=" --agency.endpoint $TRANSPORT://localhost:$port"
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
echo "Try ${CURL}localhost:5000/_api/agency/config."


