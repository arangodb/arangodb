#!/bin/bash

function help() {
  echo "USAGE: $0 [options]"
  echo ""
  echo "OPTIONS:"
  echo "  -a/--nagents            # agents            (odd integer      default: 1))"
  echo "  -c/--ncoordinators      # coordinators      (odd integer      default: 1))"
  echo "  -d/--ndbservers         # db servers        (odd integer      default: 2))"
  echo "  -s/--secondaries        Start secondaries   (0|1              default: 0)"
  echo "  -t/--transport          Protocol            (ssl|tcp          default: tcp)"
  echo "  -j/--jwt-secret         JWT-Secret          (string           default: )"
  echo "     --log-level-agency   Log level (agency)  (string           default: )"
  echo "     --log-level-cluster  Log level (cluster) (string           default: )"
  echo "  -i/--interactive        Interactive mode    (C|D|R            default: '')"
  echo "  -x/--xterm              XTerm command       (default: xterm)"
  echo "  -o/--xterm-options      XTerm options       (default: --geometry=80x43)"
  echo "  -b/--offset-ports       Offset ports        (default: 0, i.e. A:4001, C:8530, D:8629)"
  echo "  -r/--rocksdb-engine     Use RocksDB engine  (default: false)"
  echo "  -q/--source-dir         ArangoDB source dir (default: .)"
  echo "  -B/--bin-dir            ArangoDB binary dir (default: ./build)"
  echo "  -O/--ongoing-ports      Ongoing ports       (default: false)"
  echo ""
  echo "EXAMPLES:"
  echo "  $0"
  echo "  $0 -a 1 -c 1 -d 3 -t ssl"
  echo "  $0 -a 3 -c 1 -d 2 -t tcp -i C"
  
}

# defaults
NRAGENTS=1
NRDBSERVERS=2
NRCOORDINATORS=1
POOLSZ=""
TRANSPORT="tcp"
LOG_LEVEL="INFO"
LOG_LEVEL_AGENCY="INFO"
LOG_LEVEL_CLUSTER="INFO"
if [ -z "$XTERM" ] ; then
    XTERM="x-terminal-emulator"
fi
if [ -z "$XTERMOPTIONS" ] ; then
    XTERMOPTIONS="--geometry=80x43"
fi
SECONDARIES=0
BUILD="./build"
JWT_SECRET=""
PORT_OFFSET=0
SRC_DIR="."

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
    -r|--rocksdb-engine)
      USE_ROCKSDB=${2}
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
    --log-level-agency)
      LOG_LEVEL_AGENCY=${2}
      shift
      ;;
    --log-level-cluster)
      LOG_LEVEL_CLUSTER=${2}
      shift
      ;;
    -i|--interactive)
      INTERACTIVE_MODE=${2}
      shift
      ;;
    -j|--jwt-secret)
      JWT_SECRET=${2}
      shift
      ;;
    -q|--src-dir)
      SRC_DIR=${2}
      shift
      ;;
    -x|--xterm)
      XTERM=${2}
      shift
      ;;
    -o|--xterm-options)
      XTERMOPTIONS=${2}
      shift
      ;;
    -b|--port-offset)
      PORT_OFFSET=${2}
      shift
      ;;
    -h|--help)
      help
      exit 1  
      ;;
    -B|--build)
      BUILD=${2}
      shift
      ;;
    -O|--ongoing-ports)
      ONGOING_PORTS=${2}
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
