#!/bin/bash

. `dirname $0`/cluster-run-common.sh

if [ "$POOLSZ" == "" ] ; then
  POOLSZ=$NRAGENTS
fi

if [ -z "$USE_ROCKSDB" ] ; then
  STORAGE_ENGINE=""
else
  STORAGE_ENGINE="--server.storage-engine=rocksdb"
fi
DEFAULT_REPLICATION=""


printf "Starting agency ... \n"
printf "  # agents: %s," "$NRAGENTS"
printf " # db servers: %s," "$NRDBSERVERS"
printf " # coordinators: %s," "$NRCOORDINATORS"
printf " transport: %s\n" "$TRANSPORT"

if [[ $(( $NRAGENTS % 2 )) == 0 ]]; then
  echo "**ERROR: Number of agents must be odd! Bailing out."
  exit 1
fi

SFRE=1.0
COMP=2000
KEEP=1000
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
ADDRESS=[::1]

rm -rf cluster
if [ -d cluster-init ];then
  cp -a cluster-init cluster
fi
mkdir -p cluster

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
        CO_ARANGOD="$XTERM $XTERMOPTIONS -e ${BUILD}/bin/arangod --console "
        echo "Starting one coordinator in terminal with --console"
    elif [ "$INTERACTIVE_MODE" == "R" ] ; then
        ARANGOD="$XTERM $XTERMOPTIONS -e rr ${BUILD}/bin/arangod --console "
        CO_ARANGOD=$ARANGOD
        echo Running cluster in rr with --console.
    fi
else
    ARANGOD="${BUILD}/bin/arangod "
    CO_ARANGOD=$ARANGOD
fi

echo Starting agency ... 
for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    port=$(( $AG_BASE + $aid ))
    AGENCY_ENDPOINTS+="--cluster.agency-endpoint $TRANSPORT://$ADDRESS:$port "
    $ARANGOD \
        -c none \
        --agency.activate true \
        --agency.compaction-step-size $COMP \
        --agency.compaction-keep-size $KEEP \
        --agency.endpoint $TRANSPORT://$ENDPOINT:$AG_BASE \
        --agency.my-address $TRANSPORT://$ADDRESS:$port \
        --agency.pool-size $NRAGENTS \
        --agency.size $NRAGENTS \
        --agency.supervision true \
        --agency.supervision-frequency $SFRE \
        --agency.supervision-grace-period 5.0 \
        --agency.wait-for-sync false \
        --database.directory cluster/data$port \
        --javascript.app-path $SRC_DIR/js/apps \
        --javascript.startup-directory $SRC_DIR/js \
        --javascript.module-directory $SRC_DIR/enterprise/js \
        --javascript.v8-contexts 1 \
        --server.endpoint $TRANSPORT://$ENDPOINT:$port \
        --server.statistics false \
        --server.threads 16 \
        --log.file cluster/$port.log \
        --log.force-direct true \
        --log.level $LOG_LEVEL_AGENCY \
        $STORAGE_ENGINE \
        $AUTHENTICATION \
        $SSLKEYFILE \
        | tee cluster/$PORT.stdout 2>&1 &
done

start() {
    
    if [ "$1" == "dbserver" ]; then
        ROLE="PRIMARY"
    elif [ "$1" == "coordinator" ]; then
        ROLE="COORDINATOR"
    fi

    if [ "$1" == "coordinator" ]; then
        CMD=$CO_ARANGOD
    else
        CMD=$ARANGOD
    fi

    TYPE=$1
    PORT=$2
    mkdir cluster/data$PORT cluster/apps$PORT 
    echo Starting $TYPE on port $PORT
    $CMD \
        -c none \
        --database.directory cluster/data$PORT \
        --cluster.agency-endpoint $TRANSPORT://$ENDPOINT:$AG_BASE \
        --cluster.my-address $TRANSPORT://$ADDRESS:$PORT \
        --server.endpoint $TRANSPORT://$ENDPOINT:$PORT \
        --cluster.my-role $ROLE \
        --log.file cluster/$PORT.log \
        --log.level $LOG_LEVEL \
        --server.statistics true \
        --server.threads 5 \
        --javascript.startup-directory $SRC_DIR/js \
        --javascript.module-directory $SRC_DIR/enterprise/js \
        --javascript.app-path cluster/apps$PORT \
        --log.force-direct true \
        --log.level $LOG_LEVEL_CLUSTER \
        $STORAGE_ENGINE \
        $AUTHENTICATION \
        $SSLKEYFILE \
        | tee cluster/$PORT.stdout 2>&1 &
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

if [ "$SECONDARIES" == "1" ] ; then
    let index=1
    PORTTOPSE=`expr $SE_BASE + $NRDBSERVERS - 1` 
    for PORT in `seq $SE_BASE $PORTTOPSE` ; do
        mkdir cluster/data$PORT
        UUID=SCND-$(uuidgen)
        zfindex=$(printf "%04d" $index)
        CLUSTER_ID="Secondary$zfindex"
        
        echo Registering secondary $CLUSTER_ID for "DBServer$zfindex"
        curl -f -X PUT --data "{\"primary\": \"DBServer$zfindex\", \"oldSecondary\": \"none\", \"newSecondary\": \"$CLUSTER_ID\"}" -H "Content-Type: application/json" $ADDRESS:$CO_BASE/_admin/cluster/replaceSecondary
        echo Starting Secondary $CLUSTER_ID on port $PORT
        ${BUILD}/bin/arangod \
            -c none \
            --database.directory cluster/data$PORT \
            --cluster.agency-endpoint $TRANSPORT://$ADDRESS:$AG_BASE \
            --cluster.my-address $TRANSPORT://$ADDRESS:$PORT \
            --server.endpoint $TRANSPORT://$ENDPOINT:$PORT \
            --cluster.my-id $CLUSTER_ID \
            --log.file cluster/$PORT.log \
            --server.statistics true \
            --javascript.startup-directory $SRC_DIR/js \
            --javascript.module-directory $SRC_DIR/enterprise/js \
            $STORAGE_ENGINE \
            $AUTHENTICATION \
            $SSLKEYFILE \
            --javascript.app-path $SRC_DIR/js/apps \
            > cluster/$PORT.stdout 2>&1 &
            
            let index=$index+1
    done
fi

echo Done, your cluster is ready at
for p in `seq $CO_BASE $PORTTOPCO` ; do
    echo "   ${BUILD}/bin/arangosh --server.endpoint $TRANSPORT://[::1]:$p"
done

