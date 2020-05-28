#!/usr/bin/env bash
params=("$@")

rm -rf active
if [ -d cluster-init ];then
  echo "== creating cluster directory from existing cluster-init directory"
  cp -a active-init active
else
  echo "== creating fresh directory"
  mkdir -p active || { echo "failed to create cluster directory"; exit 1; }
  #if we want to restart we should probably store the parameters line wise
fi

case $OSTYPE in
  darwin*)
    lib="$PWD/scripts/cluster-run-common.sh"
  ;;
  *)
    lib="$(dirname $(readlink -f ${BASH_SOURCE[0]}))/cluster-run-common.sh"
  ;;
esac

if [[ -f "$lib" ]]; then
  . "$lib"
else
  echo "could not source $lib"
  exit 1
fi

if [[ -f active/startup_parameters ]];then
    string="$(< active/startup_parameters)"
    if [[ -z "${params[@]}" ]]; then
        params=( $string )
    else
        if ! [[ "$*" == "$string" ]]; then
            echo "stored and given params do not match:"
            echo "given: ${params[@]}"
            echo "stored: $string"
        fi
    fi
else
    #store parmeters
    if [[ -n "${params[@]}" ]]; then
      echo "${params[@]}" > active/startup_parameters
    fi
fi

parse_args "${params[@]}"

if [ "$POOLSZ" == "" ] ; then
  POOLSZ=$NRAGENTS
fi

#default engine is RocksDB
STORAGE_ENGINE="--server.storage-engine=rocksdb"
DEFAULT_REPLICATION=""

if [[ $NRAGENTS -le 0 ]]; then
    echo "you need as least one agent currently you have $NRAGENTS"
    exit 1
fi

printf "Starting agency ... \n"
printf "  # agents: %s," "$NRAGENTS"
printf " # single servers: %s," "$NRDBSERVERS"
printf " transport: %s\n" "$TRANSPORT"

if [[ $(( $NRAGENTS % 2 )) == 0 ]]; then
  echo "**ERROR: Number of agents must be odd! Bailing out."
  exit 1
fi

SFRE=1.0
COMP=2000
KEEP=1000
NRSINGLESERVERS=$NRDBSERVERS
if [ -z "$ONGOING_PORTS" ] ; then
  SS_BASE=$(( $PORT_OFFSET + 8530 ))
  AG_BASE=$(( $PORT_OFFSET + 4001 ))
else
  SS_BASE=$(( $PORT_OFFSET + 8530 ))
  AG_BASE=$(( $PORT_OFFSET + 8530 + $NRSINGLESERVERS ))
fi
NATH=$(( $NRSINGLESERVERS + $NRAGENTS ))
ENDPOINT=[::]
ADDRESS=[::1]

rm -rf active
if [ -d active-init ];then
  cp -a active-init active
fi
mkdir -p active

if [ -z "$JWT_SECRET" ];then
  AUTHENTICATION="--server.authentication false"
  AUTHORIZATION_HEADER=""
else
  AUTHENTICATION="--server.jwt-secret $JWT_SECRET"
  AUTHORIZATION_HEADER="Authorization: bearer $(jwtgen -a HS256 -s $JWT_SECRET -c 'iss=arangodb' -c 'server_id=setup')"
fi

if [ "$TRANSPORT" == "ssl" ]; then
  SSLKEYFILE="--ssl.keyfile UnitTests/server.pem"
  CURL="curl --insecure $CURL_AUTHENTICATION -s -f -X GET https:"
else
  SSLKEYFILE=""
  CURL="curl -s -f $CURL_AUTHENTICATION -X GET http:"
fi

if [ ! -z "$INTERACTIVE_MODE" ] ; then
    if [ "$INTERACTIVE_MODE" == "C" ] ; then
        ARANGOD="${BUILD}/bin/arangod "
        echo "Starting one server in terminal with --console"
    elif [ "$INTERACTIVE_MODE" == "R" ] ; then
        ARANGOD="$XTERM $XTERMOPTIONS -e rr ${BUILD}/bin/arangod --console "
        echo "Running cluster in rr with --console."
    fi
else
    ARANGOD="${BUILD}/bin/arangod "
fi

echo Starting agency ... 
for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    PORT=$(( $AG_BASE + $aid ))
    AGENCY_ENDPOINTS+="--cluster.agency-endpoint $TRANSPORT://$ADDRESS:$PORT "
    $ARANGOD \
        -c none \
        --agency.activate true \
        --agency.compaction-step-size $COMP \
        --agency.compaction-keep-size $KEEP \
        --agency.endpoint $TRANSPORT://$ENDPOINT:$AG_BASE \
        --agency.my-address $TRANSPORT://$ADDRESS:$PORT \
        --agency.pool-size $NRAGENTS \
        --agency.size $NRAGENTS \
        --agency.supervision true \
        --agency.supervision-frequency $SFRE \
        --agency.wait-for-sync false \
        --database.directory active/data$PORT \
        --javascript.enabled false \
        --server.endpoint $TRANSPORT://$ENDPOINT:$PORT \
        --server.statistics false \
        --log.file active/$PORT.log \
        --log.level $LOG_LEVEL_AGENCY \
        $STORAGE_ENGINE \
        $AUTHENTICATION \
        $SSLKEYFILE \
        2>&1 | tee active/$PORT.stdout &
done

start() {
    ROLE="SINGLE"
    CMD=$ARANGOD

    TYPE=$1
    PORT=$2
    mkdir active/data$PORT active/apps$PORT 
    echo Starting $TYPE on port $PORT
    $CMD \
        -c none \
        --database.directory active/data$PORT \
        --cluster.agency-endpoint $TRANSPORT://$ENDPOINT:$AG_BASE \
        --cluster.my-address $TRANSPORT://$ADDRESS:$PORT \
        --server.endpoint $TRANSPORT://$ENDPOINT:$PORT \
        --cluster.my-role $ROLE \
        --replication.active-failover true \
        --log.file active/$PORT.log \
        --log.level $LOG_LEVEL \
        --server.statistics true \
        --javascript.startup-directory $SRC_DIR/js \
        --javascript.module-directory $SRC_DIR/enterprise/js \
        --javascript.app-path active/apps$PORT \
        --log.level $LOG_LEVEL_CLUSTER \
        $STORAGE_ENGINE \
        $AUTHENTICATION \
        $SSLKEYFILE \
        2>&1 | tee active/$PORT.stdout &
}

PORTTOPDB=`expr $SS_BASE + $NRSINGLESERVERS - 1`
for p in `seq $SS_BASE $PORTTOPDB` ; do
    start "single server" $p
done

testServer() {
    PORT=$1
    while true ; do
        if [ -z "$AUTHORIZATION_HEADER" ]; then
          ${CURL}//$ADDRESS:$PORT/_api/version > /dev/null 2>&1
        else
          ${CURL}//$ADDRESS:$PORT/_api/version -H "$AUTHORIZATION_HEADER" > /dev/null 2>&1
        fi
        if [ "$?" != "0" ] ; then
            echo Server on port $PORT does not answer yet.
        else
            echo Server on port $PORT is ready for business.
            break
        fi
        sleep 1
    done
}

for p in `seq $SS_BASE $PORTTOPDB` ; do
    testServer $p
done

echo Done, your active failover pair is ready at
for p in `seq $SS_BASE $PORTTOPDB` ; do
    echo "   ${BUILD}/bin/arangosh --server.endpoint $TRANSPORT://[::1]:$p"
done

