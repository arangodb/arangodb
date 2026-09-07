#!/usr/bin/env bash
#
# End-to-end local flow: run the ANN benchmark, then load its results into the
# ArangoDB store that Grafana reads. Bring the stack up first:
#   docker compose up -d
#
# Then:
#   ./run_and_load.sh
#
# Configure via the same env vars as run_ann_benchmark.sh (ANN_DATASETS,
# ANN_RUNS, ARANGODB_IMAGE, ...), plus the store connection below.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BENCH_SCRIPT="${BENCH_SCRIPT:-${HERE}/../run_ann_benchmark.sh}"

ARANGODB_IMAGE="${ARANGODB_IMAGE:-arangodb/enterprise-preview:devel-nightly}"
ANN_OUTPUT_DIR="${ANN_OUTPUT_DIR:-${HERE}/bench-out}"
export ANN_OUTPUT_DIR ARANGODB_IMAGE

# Store (docker-compose maps arangodb to 8530 on the host).
STORE_URL="${ARANGO_STORE_URL:-http://localhost:8530}"
STORE_DB="${ARANGO_STORE_DB:-bench}"

STARTED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
"${BENCH_SCRIPT}"
ENDED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

# Version / build-id / image id captured by run_ann_benchmark.sh.
META="${ANN_OUTPUT_DIR}/run_meta.json"
VERSION_FULL="$(jq -r '.arangodb_version_full // ""' "${META}")"
BUILD_ID="$(jq -r '.build_id // ""' "${META}")"
IMAGE_META="$(jq -r '.image // ""' "${META}")"
IMAGE_ID="$(jq -r '.image_id // ""' "${META}")"

python3 "${HERE}/load_run.py" \
  --csv "${ANN_OUTPUT_DIR}/results.csv" \
  --arangodb-version "${VERSION_FULL:-${ARANGODB_IMAGE##*:}}" \
  --build-id "${BUILD_ID}" \
  --docker-image "${IMAGE_META:-${ARANGODB_IMAGE}}" \
  --docker-image-id "${IMAGE_ID}" \
  --started-at "${STARTED_AT}" \
  --ended-at "${ENDED_AT}" \
  --url "${STORE_URL}" \
  --db "${STORE_DB}"

echo ">> loaded into ${STORE_URL} db=${STORE_DB}. Grafana: http://localhost:3000"
