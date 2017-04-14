#!/bin/bash

function help() {
  echo "USAGE: scripts/shutdownLocalCluster.sh [options]"
  echo ""
  echo "OPTIONS:"
  echo "  -a/--nagents            # agents            (odd integer      default: 1))"
  echo "  -c/--ncoordinators      # coordinators      (odd integer      default: 1))"
  echo "  -d/--ndbservers         # db servers        (odd integer      default: 2))"
  echo "  -s/--secondaries        Start secondaries   (0|1              default: 0)"
  echo "  -t/--transport          Protocol            (ssl|tcp          default: tcp)"
  echo ""
  echo "EXAMPLES:"
  echo "  scripts/shutdownLocalCluster.sh"
  echo "  scripts/shutdownLocalCluster.sh -a 1 -c 1 -d 3 -t ssl"
}

#defaults
NRAGENTS=1
NRDBSERVERS=2
NRCOORDINATORS=1
POOLSZ=""
TRANSPORT="tcp"

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
    -b|--port-offset)
      PORT_OFFSET=${2}
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

AG_BASE=$(( $PORT_OFFSET + 4001 ))
CO_BASE=$(( $PORT_OFFSET + 8530 ))
DB_BASE=$(( $PORT_OFFSET + 8629 ))
SE_BASE=$(( $PORT_OFFSET + 8729 ))

shutdown() {
    PORT=$1
    echo -n "$PORT "
    curl -X DELETE http://[::1]:$PORT/_admin/shutdown >/dev/null 2>/dev/null
}

i=0
if [ -n "$SECONDARIES" ]; then
    echo -n "Shutting down secondaries... "
    PORTTOPSE=`expr $SE_BASE + $NRDBSERVERS - 1` 
    for PORT in `seq $SE_BASE $PORTTOPSE` ; do
        spids[i]=`ps -eaf|grep data${PORT}|grep arangod|awk '{print $2}'`
        shutdown $PORT
        i=i+1
    done
fi
echo
echo -n "Shutting down Coordinators... "
i=0
PORTTOPCO=`expr $CO_BASE + $NRCOORDINATORS - 1`
for p in `seq $CO_BASE $PORTTOPCO` ; do
    pid=`ps -eaf|grep data${p}|grep arangod|awk '{print $2}'`
    if [ ! -z "$pid" ]; then
        pids[$i]=$pid
        let i=i+1
    fi
    shutdown $p
done
echo
echo -n "Shutting down DBServers... "
PORTTOPDB=`expr $DB_BASE + $NRDBSERVERS - 1`
for p in `seq $DB_BASE $PORTTOPDB` ; do
    pid=`ps -eaf|grep data${p}|grep arangod|awk '{print $2}'`
    if [ ! -z "$pid" ]; then
        pids[$i]=$pid
        let i=i+1
    fi
    shutdown $p
done

while true; do
    i=0;
    for p in "${pids[@]}"; do
	      pid=`ps -p $p`
        if [ $? == 0 ]; then
           let i=i+1
        fi
    done
    if [ $i == 0 ]; then
        break;
    fi
    sleep .1;
done
echo
echo -n "Shutting down agency ... "
i=0;

for aid in `seq 0 $(( $NRAGENTS - 1 ))`; do
    PORT=`expr $AG_BASE + $aid`
    pid=`ps -eaf|grep data${PORT}|grep arangod|awk '{print $2}'`
    if [ ! -z "$pid" ]; then
        apids[$i]=$pid
        let i=i+1
    fi
    port=$(( $AG_BASE + $aid ))
    shutdown $port
done

while true; do
    i=0;
    for p in "${apids[@]}"; do
	      pid=`ps -p $p`
        if [ $? == 0 ]; then
           let i=i+1
        fi
    done
    if [ $i == 0 ]; then
        break;
    fi
    sleep .1;
done
echo
echo Done, your cluster is gone

