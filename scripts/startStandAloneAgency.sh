#!/bin/bash

function help() {
  echo "USAGE: scripts/startStandAloneAgency.sh [options]"
  echo ""
  echo "OPTIONS:"
  echo "  -a/--agency-size Agency size (odd integer      default: 3))"
  echo "  -p/--pool-size   Pool size   (>= agency size   default: [agency size])"
  echo "  -t/--transport   Protocol    (ssl|tcp          default: tcp)"
  echo "  -l/--log-level   Log level   (INFO|DEBUG|TRACE default: INFO)"
  echo ""
  echo "EXAMPLES:"
  echo "  scripts/startStandaloneAgency.sh"
  echo "  scripts/startStandaloneAgency.sh -a 5 -p 10 -t ssl"
  echo "  scripts/startStandaloneAgency.sh --agency-size 3 --pool-size 5"
  
}

NRAGENTS=3
POOLSZ=""
TRANSPORT="tcp"
LOG_LEVEL="INFO"

while [[ ${1} ]]; do
  case "${1}" in
    -a|--agency-size)
      NRAGENTS=${2}
      shift
      ;;
    -p|--pool-size)
      POOLSZ=${2}
      shift
      ;;
    -t|--transport)
      TRANSPORT=${2}
      shift
      ;;
    -l|--log-level)
      LOG_LEVEL=${2}
      shift
      ;;
    -h|--help)
      help
      exit 1  
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
  CURL="curl --insecure -ks https://"
else
  SSLKEYFILE=""
  CURL="curl -s http://"
fi

printf "Starting agency ... \n"
printf "  agency-size: %s," "$NRAGENTS"
printf  " pool-size: %s," "$POOLSZ"
printf  " transport: %s," "$TRANSPORT"
printf  " log-level: %s\n" "$LOG_LEVEL"

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

rm -rf agency
mkdir -p agency
PIDS=""
for aid in `seq 0 $(( $POOLSZ - 1 ))`; do
  port=$(( $BASE + $aid ))
  build/bin/arangod \
    -c none \
    --agency.activate true \
    --agency.election-timeout-min $MINP \
    --agency.election-timeout-max $MAXP \
    --agency.endpoint $TRANSPORT://localhost:$BASE \
    --agency.my-address $TRANSPORT://localhost:$port \
    --agency.compaction-step-size $COMP \
    --agency.pool-size $POOLSZ \
    --agency.size $NRAGENTS \
    --agency.supervision true \
    --agency.supervision-frequency $SFRE \
    --agency.wait-for-sync false \
    --database.directory agency/data$port \
    --javascript.app-path ./js/apps \
    --javascript.startup-directory ./js \
    --javascript.v8-contexts 1 \
    --log.file agency/$port.log \
    --log.force-direct true \
    --log.level agency=$LOG_LEVEL \
    --server.authentication false \
    --server.endpoint $TRANSPORT://localhost:$port \
    --server.statistics false \
    $SSLKEYFILE \
    > agency/$port.stdout 2>&1 &
  PIDS+=$!
  PIDS+=" "
done

echo "  done. Your agents are ready at port $BASE onward."
#echo "Process ids: $PIDS"
echo "Try ${CURL}localhost:5000/_api/agency/config."


