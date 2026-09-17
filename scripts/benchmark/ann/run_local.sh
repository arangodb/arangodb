#!/usr/bin/env bash
#
# Local mode: build the harness image and benchmark an arangod you started
# yourself (default 127.0.0.1:8529, needs --vector-index=true). The site and
# logs land in ./ann-output of the current directory.
#
# Any ARANGO_*, ANN_*, RESULTS_ARANGO_* or GRAFANA_URL variable set in the
# calling shell is forwarded into the container, e.g.
#   ANN_DATASETS="sift-128-euclidean" ARANGO_PORT=8530 ./run_local.sh

set -euo pipefail

IMAGE="${ANN_HARNESS_IMAGE:-ann-bench-harness}"
OUT_DIR="${PWD}/ann-output"
DATASET_CACHE="${ANN_DATASET_CACHE:-${HOME}/.cache/ann-benchmarks}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

docker build --quiet -t "${IMAGE}" "${SCRIPT_DIR}"

mkdir -p "${OUT_DIR}" "${DATASET_CACHE}"

# Forward the caller's knobs; paths inside the container are fixed by the image.
env_args=()
while IFS= read -r name; do
  case "${name}" in
    ANN_WORKDIR|ANN_OUTPUT_DIR|ANN_DATASET_CACHE|ANN_HARNESS_IMAGE) ;;
    *) env_args+=(-e "${name}") ;;
  esac
done < <(env | grep -oE '^(ARANGO|ANN|RESULTS_ARANGO)_[A-Z_]+|^GRAFANA_URL' || true)

# --network host so 127.0.0.1 inside the container is the host's arangod. On
# Docker Desktop without host networking use ARANGO_HOST=host.docker.internal.
docker run --rm --network host \
  -v "${OUT_DIR}:/out" \
  -v "${DATASET_CACHE}:/data" \
  -e ANN_STAMP="${ANN_STAMP:-local $(date -u +%Y-%m-%dT%H:%M:%SZ)}" \
  "${env_args[@]}" \
  "${IMAGE}"

printf '\nSite: %s\n' "${OUT_DIR}/site/index.html"
