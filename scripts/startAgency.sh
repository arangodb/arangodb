#!/bin/bash
if [ ! -d arangod ] || [ ! -d arangosh ] || [ ! -d UnitTests ] ; then
    echo Must be started in the main ArangoDB source directory.
    exit 1
fi

rm -rf cluster
mkdir cluster
echo Starting agency...
build/bin/arangod -c etc/relative/arangod.conf \
  --agency.size 1 \
  --server.endpoint tcp://127.0.0.1:4001 \
  --agency.wait-for-sync false \
  --database.directory cluster/data4001 \
  --agency.id 0 \
  --log.file cluster/4001.log \
  --server.statistics false \
  --server.authentication false \
  --javascript.app-path ./js/apps \
  --javascript.startup-directory ./js \
  > cluster/4001.stdout 2>&1 &

testServer() {
    PORT=$1
    while true ; do
        sleep 1
        curl -s -f -X GET "http://127.0.0.1:$PORT/_api/version" > /dev/null 2>&1
        if [ "$?" != "0" ] ; then
            echo Server on port $PORT does not answer yet.
        else
            echo Server on port $PORT is ready for business.
            break
        fi
    done
}

testServer 4001

echo Done, your agency is ready at
echo "   build/bin/arangosh --server.endpoint tcp://127.0.0.1:4001"

