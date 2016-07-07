#!/bin/bash
set -e

if test "$#" -ne 4; then
  echo "usage: $0 <arangod2_dir> <database2_dir> <arangod3_dir> <database3_dir>"
  exit 1
fi

ARANGOD2_DIR="$1"
DATABASE2_DIR="$2"
ARANGOD3_DIR="$3"
DATABASE3_DIR="$4"

if [ ! -d "${ARANGOD2_DIR}" ]; then
  echo "${ARANGOD2_DIR} not available, no need to upgrade"
  exit 0
fi

if [ -f "${DATABASE3_DIR}/SERVER" ]; then
  echo "${ARANGOD3_DIR}/SERVER exists, upgrade already done?"
  exit 0
fi

ARANGOD2="${ARANGOD2_DIR}/sbin/arangod-2.8"
ARANGOD2_CONF="${ARANGOD2_DIR}/etc/arangodb/arangod-2.8.conf"

ARANGOSH2="${ARANGOD2_DIR}/bin/arangosh-2.8"
ARANGOSH2_CONF="${ARANGOD2_DIR}/etc/arangodb/arangosh-2.8.conf"

ARANGODUMP2="${ARANGOD2_DIR}/bin/arangodump-2.8"
ARANGODUMP2_CONF="${ARANGOD2_DIR}/etc/arangodb/arangodump-2.8.conf"

ARANGOD3="${ARANGOD3_DIR}/sbin/arangod"
ARANGOSH3="${ARANGOD3_DIR}/bin/arangosh"
ARANGORESTORE3="${ARANGOD3_DIR}/bin/arangorestore"

DUMPDIR="${DATABASE2_DIR}/DUMP"

echo "Upgrade in progress..."

echo "Waiting for ArangoDB 2 to boot..."
${ARANGOD2} -c ${ARANGOD2_CONF} --log.tty "" &
sleep 10

while ${ARANGOSH2} -c ${ARANGOSH2_CONF} --quiet --javascript.execute-string 'arango.isConnected() ? 1 : 0'; do
  sleep 10
  echo "Waiting for ArangoDB 2 to boot..."
done

VERSION=`${ARANGOSH2} -c ${ARANGOSH2_CONF} --quiet --javascript.execute-string 'require("internal").print(arango.getVersion()); 0'`

if [ "${VERSION}" != "2.8.10" ]; then
  echo "$0: failed to start ArangoDB 2.8.0, got version ${VERSION} instead"
  exit 1
fi

echo "ArangoDB 2 has started, beginning dump process..."
rm -rf ${DATABASE2_DIR}/DUMP
${ARANGODUMP2} -c ${ARANGODUMP2_CONF} --include-system-collections true --output-directory ${DUMPDIR}

echo "Dump has finished, shutting down ArangoDB 2..."
kill -SIGINT %1
wait %1

echo "Waiting for ArangoDB 3 to boot..."
${ARANGOD3} --log.foreground-tty false &
sleep 10

while ${ARANGOSH3} --server.password "" --quiet --javascript.execute-string 'arango.isConnected() ? 1 : 0'; do
  sleep 10
  echo "Waiting for ArangoDB 3 to boot..."
done

echo "ArangoDB 3 has started, beginning restore process..."
${ARANGORESTORE3} --server.password "" --input-directory ${DUMPDIR}

kill %1
wait %1
