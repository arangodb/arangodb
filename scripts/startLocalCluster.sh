#!/usr/bin/env bash
params=("$@")

rm -rf cluster
if [ -d cluster-init ];then
  echo "== creating cluster directory from existing cluster-init directory"
  cp -a cluster-init cluster
else
  echo "== creating fresh directory"
  mkdir -p cluster || { echo "failed to create cluster directory"; exit 1; }
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

if [[ -f cluster/startup_parameters ]];then
    string="$(< cluster/startup_parameters)"
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
    echo "${params[@]}" > cluster/startup_parameters
  fi
fi

parse_args "${params[@]}"

if [ "$POOLSZ" == "" ] ; then
  POOLSZ=$NRAGENTS
fi

STORAGE_ENGINE="--server.storage-engine=rocksdb"

if [ "$AUTOUPGRADE" == "1" ];then
  echo "-- Using autoupgrade procedure"
fi

if [[ $NRAGENTS -le 0 ]]; then
    echo "you need as least one agent currently you have $NRAGENTS"
    exit 1
fi

printf "== Starting agency ... \n"
printf " # agents: %s," "$NRAGENTS"
printf " # db servers: %s," "$NRDBSERVERS"
printf " # coordinators: %s," "$NRCOORDINATORS"
printf " transport: %s\n" "$TRANSPORT"

if (( $NRAGENTS % 2 == 0)) ; then
  echo "**ERROR: Number of agents must be odd! Bailing out."
  exit 1
fi

SFRE=1.0
COMP=500
KEEP=2000
if [ -z "$ONGOING_PORTS" ] ; then
  CO_BASE=$(( $PORT_OFFSET + 8530 ))
  DB_BASE=$(( $PORT_OFFSET + 8629 ))
  AG_BASE=$(( $PORT_OFFSET + 4001 ))
  SE_BASE=$(( $PORT_OFFSET + 8729 ))
else
  CO_BASE=$(( $PORT_OFFSET + 8530 ))
  DB_BASE=$(( $PORT_OFFSET + 8530 + $NRCOORDINATORS ))
  AG_BASE=$(( $PORT_OFFSET + 8530 + $NRCOORDINATORS + $NRDBSERVERS ))
  SE_BASE=$(( $PORT_OFFSET + 8530 + $NRCOORDINATORS + $NRDBSERVERS + $NRAGENTS ))
fi
NATH=$(( $NRDBSERVERS + $NRCOORDINATORS + $NRAGENTS ))
ENDPOINT=[::]
ADDRESS=${ADDRESS:-[::1]}

if [ -z "$JWT_SECRET" ];then
  AUTHENTICATION="--server.authentication false"
  AUTHORIZATION_HEADER=""
else
  AUTHENTICATION="--server.jwt-secret $JWT_SECRET"
  AUTHORIZATION_HEADER="Authorization: bearer $(jwtgen -a HS256 -s $JWT_SECRET -c 'iss=arangodb' -c 'server_id=setup')"
fi

if [ -z "$ENCRYPTION_SECRET" ];then
  ENCRYPTION=""
else
  echo -n $ENCRYPTION_SECRET > cluster/encryption-secret.txt
  ENCRYPTION="--rocksdb.encryption-keyfile cluster/encryption-secret.txt"
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
        CO_ARANGOD="$XTERM $XTERMOPTIONS ${BUILD}/bin/arangod --console"
        echo "Starting one coordinator in terminal with --console"
    elif [ "$INTERACTIVE_MODE" == "R" ] ; then
        ARANGOD="rr ${BUILD}/bin/arangod"
        CO_ARANGOD="$ARANGOD"
        echo Running cluster in rr with --console.
    fi
else
    ARANGOD="${BUILD}/bin/arangod "
    CO_ARANGOD=$ARANGOD
fi

echo == Starting agency ...
for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    [ "$INTERACTIVE_MODE" == "R" ] && sleep 1
    PORT=$(( $AG_BASE + $aid ))
    AGENCY_ENDPOINTS+="--cluster.agency-endpoint $TRANSPORT://$ADDRESS:$PORT "
    if [ "$AUTOUPGRADE" == "1" ];then
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
          --database.directory cluster/data$PORT \
          --javascript.enabled false \
          --server.endpoint $TRANSPORT://$ENDPOINT:$PORT \
          --server.statistics false \
          --log.role true \
          --log.file cluster/$PORT.log \
          --log.force-direct false \
          --log.level $LOG_LEVEL_AGENCY \
          --javascript.allow-admin-execute true \
          $STORAGE_ENGINE \
          $AUTHENTICATION \
          $SSLKEYFILE \
          $ENCRYPTION \
          --database.auto-upgrade true \
          2>&1 | tee cluster/$PORT.stdout
    fi
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
        --database.directory cluster/data$PORT \
        --javascript.enabled false \
        --server.endpoint $TRANSPORT://$ENDPOINT:$PORT \
        --server.statistics false \
        --log.role true \
        --log.file cluster/$PORT.log \
        --log.force-direct false \
        --log.level $LOG_LEVEL_AGENCY \
        --javascript.allow-admin-execute true \
        $STORAGE_ENGINE \
        $AUTHENTICATION \
        $SSLKEYFILE \
        $ENCRYPTION \
        2>&1 | tee cluster/$PORT.stdout &
done

start() {
    if [ "$NRDBSERVERS" == "1" ]; then
        SYSTEM_REPLICATION_FACTOR="--cluster.system-replication-factor=1"
    else
        SYSTEM_REPLICATION_FACTOR=""
    fi

    if [ "$1" == "dbserver" ]; then
        ROLE="DBSERVER"
    elif [ "$1" == "coordinator" ]; then
        ROLE="COORDINATOR"
    fi

    if [ "$1" == "coordinator" ]; then
        CMD=$CO_ARANGOD
    else
        CMD=$ARANGOD
    fi

    if [ "$USE_RR" = "true" ]; then
        if ! which rr > /dev/null; then
            echo 'rr binary not found in PATH!' >&2
            exit 1
        fi
        CMD="rr $CMD"
    fi

    TYPE=$1
    PORT=$2
    mkdir -p cluster/data$PORT cluster/apps$PORT
    echo == Starting $TYPE on port $PORT
    [ "$INTERACTIVE_MODE" == "R" ] && sleep 1
    if [ "$AUTOUPGRADE" == "1" ];then
      $CMD \
          -c none \
          --database.directory cluster/data$PORT \
          --cluster.agency-endpoint $TRANSPORT://$ENDPOINT:$AG_BASE \
          --cluster.my-address $TRANSPORT://$ADDRESS:$PORT \
          --server.endpoint $TRANSPORT://$ENDPOINT:$PORT \
          --cluster.my-role $ROLE \
          --log.role true \
          --log.file cluster/$PORT.log \
          --log.level $LOG_LEVEL \
          --server.statistics true \
          --javascript.startup-directory $SRC_DIR/js \
          --javascript.module-directory $SRC_DIR/enterprise/js \
          --javascript.app-path cluster/apps$PORT \
          --log.force-direct false \
          --log.level $LOG_LEVEL_CLUSTER \
          --javascript.allow-admin-execute true \
          $SYSTEM_REPLICATION_FACTOR \
          $STORAGE_ENGINE \
          $AUTHENTICATION \
          $SSLKEYFILE \
          $ENCRYPTION \
          --database.auto-upgrade true \
          2>&1 | tee cluster/$PORT.stdout
    fi
    $CMD \
        -c none \
        --database.directory cluster/data$PORT \
        --cluster.agency-endpoint $TRANSPORT://$ENDPOINT:$AG_BASE \
        --cluster.my-address $TRANSPORT://$ADDRESS:$PORT \
        --server.endpoint $TRANSPORT://$ENDPOINT:$PORT \
        --cluster.my-role $ROLE \
        --log.role true \
        --log.file cluster/$PORT.log \
        --log.level $LOG_LEVEL \
        --server.statistics true \
        --javascript.startup-directory $SRC_DIR/js \
        --javascript.module-directory $SRC_DIR/enterprise/js \
        --javascript.app-path cluster/apps$PORT \
        --log.force-direct false \
        --log.thread true \
        --log.level $LOG_LEVEL_CLUSTER \
        --javascript.allow-admin-execute true \
        $SYSTEM_REPLICATION_FACTOR \
        $STORAGE_ENGINE \
        $AUTHENTICATION \
        $SSLKEYFILE \
        $ENCRYPTION \
        2>&1 | tee cluster/$PORT.stdout &
}

PORTTOPDB=`expr $DB_BASE + $NRDBSERVERS - 1`
for p in `seq $DB_BASE $PORTTOPDB` ; do
    start dbserver $p
done

PORTTOPCO=`expr $CO_BASE + $NRCOORDINATORS - 1`
for p in `seq $CO_BASE $PORTTOPCO` ; do
    start coordinator $p
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

for p in `seq $DB_BASE $PORTTOPDB` ; do
    testServer $p
done
for p in `seq $CO_BASE $PORTTOPCO` ; do
    testServer $p
done

echo == Done, your cluster is ready at
for p in `seq $CO_BASE $PORTTOPCO` ; do
    echo "   ${BUILD}/bin/arangosh --server.endpoint $TRANSPORT://[::1]:$p"
done
