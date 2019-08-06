#!/usr/bin/env bash
params=("$@")

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
    if [[ -z "${params[@]}" ]]; then
        string="$(< cluster/startup_parameters)"
        params=( $string )
    fi
fi

parse_args "${params[@]}"

echo Number of Agents: $NRAGENTS
echo Number of DBServers: $NRDBSERVERS
echo Number of Coordinators: $NRCOORDINATORS

if [ -z "$ONGOING_PORTS" ] ; then
  CO_BASE=$(( PORT_OFFSET + 8530 ))
  DB_BASE=$(( PORT_OFFSET + 8629 ))
  AG_BASE=$(( PORT_OFFSET + 4001 ))
  SE_BASE=$(( PORT_OFFSET + 8729 ))
else
  CO_BASE=$(( PORT_OFFSET + 8530 ))
  DB_BASE=$(( PORT_OFFSET + 8530 + NRCOORDINATORS ))
  AG_BASE=$(( PORT_OFFSET + 8530 + NRCOORDINATORS + NRDBSERVERS ))
  SE_BASE=$(( PORT_OFFSET + 8530 + NRCOORDINATORS + NRDBSERVERS + NRAGENTS ))
fi

LOCALHOST="[::1]"
ANY="[::]"

if [ "$TRANSPORT" == "ssl" ]; then
  CURL="curl -skfX"
  PROT=https
else
  CURL="curl -sfX"
  PROT=http  
fi

if [ -z "$JWT_SECRET" ];then
  AUTHENTICATION="--server.authentication false"
  AUTHORIZATION_HEADER=""
else
  AUTHENTICATION="--server.jwt-secret $JWT_SECRET"
  AUTHORIZATION_HEADER="Authorization: bearer $(jwtgen -a HS256 -s $JWT_SECRET -c 'iss=arangodb' -c 'server_id=setup')"
fi

echo $JWT_SECRET

shutdown() {
  PORT=$1
  echo -n "$PORT "
  $CURL DELETE $PROT://$LOCALHOST:$PORT/_admin/shutdown  -H "$AUTHORIZATION_HEADER" >/dev/null 2>/dev/null
}

if [ "$SECONDARIES" == "1" ]; then
  echo Shutting down secondaries...
  PORTHISE=$(expr $SE_BASE + $NRDBSERVERS - 1)
  for p in $(seq $SE_BASE $PORTHISE) ; do
    shutdown $PORT
  done
fi

echo -n "  Shutting down Coordinators... "
PORTHICO=$(expr $CO_BASE + $NRCOORDINATORS - 1)
for p in $(seq $CO_BASE $PORTHICO) ; do
  shutdown $p
done

echo
echo -n "  Shutting down DBServers... "
PORTHIDB=$(expr $DB_BASE + $NRDBSERVERS - 1)
for p in $(seq $DB_BASE $PORTHIDB) ; do
  shutdown $p
done

testServerDown() {
  PORT=$1
  while true ; do
    $CURL GET $PROT://$LOCALHOST:$PORT/_api/version >/dev/null 2>/dev/null
    if [ "$?" != "0" ] ; then
      pid=$(ps -eaf|grep data$PORT|grep -v grep|awk '{print $2}')
      if [ -z "$pid" ]; then
        break
      fi
    fi
    sleep 1
  done
}

for p in $(seq $CO_BASE $PORTHICO) ; do
    testServerDown $p
done

for p in $(seq $DB_BASE $PORTHIDB) ; do
    testServerDown $p
done

# Currently the agency does not wait for all servers to shutdown
# This causes a race condisiton where all servers wait to tell the agency
# they are shutting down

echo
echo -n "  Shutting down agency ... "

PORTHIAG=$(expr $AG_BASE + $NRAGENTS - 1)
for p in $(seq $AG_BASE $PORTHIAG) ; do
  shutdown $p
done

for p in $(seq $AG_BASE $PORTHIAG) ; do
    testServerDown $p
done

echo
echo Done, your cluster is gone

