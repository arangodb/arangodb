#!/usr/bin/env bash
#
# Run the public ann-benchmarks harness against ArangoDB's faiss-based IVF
# vector index and produce the standard ann-benchmarks HTML site + CSV.
#
# CI-agnostic: it boots arangod from the nightly enterprise docker image with
# the vector index enabled, clones a pinned ann-benchmarks fork and runs a
# fixed sweep (single nLists build, nProbe query sweep) over one L2 and one
# cosine dataset. Runnable on a laptop too - every knob is an env var.
#
# Everything is written under $ANN_WORKDIR (default: a fresh temp dir). The
# arangod container (when this script starts it) is torn down on exit.

set -euo pipefail
# TODOs
# 1. Make the datasets caches since we alywas run it on same ones
# 2. Setup run params properly

# ---------------------------------------------------------------------------
# Configuration (all overridable from the environment)
# ---------------------------------------------------------------------------

# arangod comes from the official nightly enterprise docker image. It runs
# unlicensed - enterprise only restricts dataset disk usage (~100 GiB), which
# these datasets (~0.5 GB each) never approach. devel-nightly is the rolling
# latest-devel tag.
ARANGODB_IMAGE="${ARANGODB_IMAGE:-arangodb/enterprise-preview:devel-nightly}"

# How arangod is provided:
#   docker    - this script runs the image itself (default; laptops / hosts
#               with a docker daemon)
#   external  - arangod is already listening at $ARANGO_URL and the script only
#               waits for it. Used in CircleCI, where a service container in the
#               job's docker: list starts arangod (container runners have no
#               docker daemon to run it from inside the job).
ARANGO_START="${ARANGO_START:-docker}"
ARANGO_CONTAINER="${ARANGO_CONTAINER:-ann-bench-arangod}"

# ann-benchmarks fork, pinned. The circle-ci branch carries the CI sweep:
# arangodb-ivf = single nLists=16384 build + nProbe query sweep.
ANN_FORK_URL="${ANN_FORK_URL:-https://github.com/jbajic/ann-benchmarks.git}"
ANN_FORK_REF="${ANN_FORK_REF:-1967a1c9b6be75f7213c89d8be29d05715e2850c}"
ANN_ALGORITHM="${ANN_ALGORITHM:-arangodb-ivf}"
# TODO ANN_DATASETS="${ANN_DATASETS:-sift-128-euclidean glove-100-angular}"
ANN_DATASETS="${ANN_DATASETS:-glove-100-angular}"
# TODO  ANN_RUNS="${ANN_RUNS:-3}"
ANN_RUNS="${ANN_RUNS:-1}"

# This runs only on single server
ARANGO_HOST="${ARANGO_HOST:-127.0.0.1}"
ARANGO_PORT="${ARANGO_PORT:-8529}"
# The startup flag that unlocks the experimental vector index. Kept as a
# variable because its exact spelling has changed across versions.
VECTOR_INDEX_FLAG="${VECTOR_INDEX_FLAG:---vector-index=true}"

# Working + output layout.
ANN_WORKDIR="${ANN_WORKDIR:-$(mktemp -d "${TMPDIR:-/tmp}/ann-bench.XXXXXX")}"
ANN_OUTPUT_DIR="${ANN_OUTPUT_DIR:-${ANN_WORKDIR}/output}"
# Persist datasets across runs (hundreds of MB each). Point at a durable path
# on CI so each run doesn't re-download.
ANN_DATASET_CACHE="${ANN_DATASET_CACHE:-${ANN_WORKDIR}/data}"

# Optional metrics push target. Empty => the push step is a no-op.
PUSHGATEWAY_URL="${PUSHGATEWAY_URL:-}"

ARANGO_URL="http://${ARANGO_HOST}:${ARANGO_PORT}"
# Set once we start the container ourselves, so cleanup only removes our own.
ARANGOD_STARTED_BY_US=""

log() { printf '\n=== %s ===\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

cleanup() {
  if [[ -n "${ARANGOD_STARTED_BY_US}" ]]; then
    log "Removing arangod container (${ARANGO_CONTAINER})"
    docker rm -f "${ARANGO_CONTAINER}" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
# arangod: start, wait
# ---------------------------------------------------------------------------

start_arangod() {
  if [[ "${ARANGO_START}" == "external" ]]; then
    log "Using external arangod at ${ARANGO_URL}"
    return
  fi
  log "Starting arangod container from ${ARANGODB_IMAGE}"
  docker rm -f "${ARANGO_CONTAINER}" >/dev/null 2>&1 || true
  # The image entrypoint prepends `arangod` to leading-dash args, so the vector
  # flag is passed straight through. No auth keeps the harness connection simple.
  docker run -d --name "${ARANGO_CONTAINER}" \
    -p "${ARANGO_PORT}:8529" \
    -e ARANGO_NO_AUTH=1 \
    "${ARANGODB_IMAGE}" "${VECTOR_INDEX_FLAG}" >/dev/null \
    || die "failed to start arangod container"
  ARANGOD_STARTED_BY_US=1
}

wait_for_arangod() {
  log "Waiting for arangod at ${ARANGO_URL}"
  for _ in $(seq 1 150); do
    if curl -fsS "${ARANGO_URL}/_api/version" >/dev/null 2>&1; then
      log "arangod is up"
      return
    fi
    # If we own the container and it has died, fail fast with its logs.
    if [[ -n "${ARANGOD_STARTED_BY_US}" ]] \
       && [[ -z "$(docker ps -q -f "name=^${ARANGO_CONTAINER}$" 2>/dev/null)" ]]; then
      docker logs "${ARANGO_CONTAINER}" 2>&1 | tail -n 30 || true
      die "arangod container exited early"
    fi
    sleep 2
  done
  die "arangod did not come up within timeout"
}

collect_meta() {
  log "Collecting server metadata (build-id, version)"
  # build-id (ELF GNU sha1) + full version identify the exact binary benchmarked.
  local ver_json image_id="" bid ver lic ver_full
  ver_json="$(curl -fsS "${ARANGO_URL}/_api/version?details=true" 2>/dev/null || echo '{}')"
  # docker image id is only reachable when we run the container ourselves.
  if [[ -n "${ARANGOD_STARTED_BY_US}" ]] && command -v docker >/dev/null 2>&1; then
    image_id="$(docker image inspect --format '{{.Id}}' "${ARANGODB_IMAGE}" 2>/dev/null || true)"
  fi
  bid="$(jq -r '.details["build-id"] // ""' <<<"${ver_json}")"
  ver="$(jq -r '.version // ""' <<<"${ver_json}")"
  lic="$(jq -r '.license // ""' <<<"${ver_json}")"
  ver_full="${ver}"
  [[ -n "${bid}" ]] && ver_full="${ver}, build-id: ${bid}"
  jq -n \
    --arg version "${ver}" --arg version_full "${ver_full}" \
    --arg build_id "${bid}" --arg license "${lic}" \
    --arg image "${ARANGODB_IMAGE}" --arg image_id "${image_id}" \
    --arg started_at "${ANN_STARTED_AT:-}" \
    '{arangodb_version: $version, arangodb_version_full: $version_full,
      build_id: $build_id, license: $license, image: $image, image_id: $image_id,
      started_at: $started_at, ended_at: ""}' \
    > "${ANN_OUTPUT_DIR}/run_meta.json"
  printf '  build-id: %s\n  version:  %s\n' "${bid:-(none)}" "${ver_full}"
}

# ---------------------------------------------------------------------------
# ann-benchmarks: env, run, export, site
# ---------------------------------------------------------------------------

setup_harness() {
  log "Cloning ann-benchmarks (${ANN_FORK_REF})"
  git clone --quiet "${ANN_FORK_URL}" "${ANN_WORKDIR}/ann-benchmarks"
  git -C "${ANN_WORKDIR}/ann-benchmarks" checkout --quiet "${ANN_FORK_REF}"

  # Persist downloaded datasets outside the (ephemeral) checkout.
  mkdir -p "${ANN_DATASET_CACHE}"
  ln -sfn "${ANN_DATASET_CACHE}" "${ANN_WORKDIR}/ann-benchmarks/data"

  log "Creating Python venv"
  python3 -m venv "${ANN_WORKDIR}/venv"
  # The activate script references unset vars; don't let set -u abort here.
  set +u
  # shellcheck disable=SC1091
  source "${ANN_WORKDIR}/venv/bin/activate"
  set -u
  pip install --quiet --upgrade pip
  pip install --quiet -r "${ANN_WORKDIR}/ann-benchmarks/requirements.txt"
  pip install --quiet python-arango
}

run_sweep() {
  cd "${ANN_WORKDIR}/ann-benchmarks"
  export ANN_BENCHMARKS_ARANGO_HOST="${ARANGO_HOST}"
  export ANN_BENCHMARKS_ARANGO_PORT="${ARANGO_PORT}"
  for ds in ${ANN_DATASETS}; do
    log "Benchmarking ${ANN_ALGORITHM} on ${ds}"
    python run.py --local --algorithm "${ANN_ALGORITHM}" \
      --dataset "${ds}" --runs "${ANN_RUNS}" --force
  done
}

export_results() {
  cd "${ANN_WORKDIR}/ann-benchmarks"
  mkdir -p "${ANN_OUTPUT_DIR}/site"
  log "Exporting CSV"
  python data_export.py --out "${ANN_OUTPUT_DIR}/results.csv"
  log "Generating website"
  python create_website.py --outputdir "${ANN_OUTPUT_DIR}/site" --scatter --latex

  # Preserve the server log when we own the container.
  if [[ -n "${ARANGOD_STARTED_BY_US}" ]]; then
    docker logs "${ARANGO_CONTAINER}" > "${ANN_OUTPUT_DIR}/arangod.log" 2>&1 || true
  fi

  # Self-describing artifact: what produced these numbers.
  {
    echo "generated: ${ANN_STAMP:-unknown}"
    echo "fork:      ${ANN_FORK_URL} @ ${ANN_FORK_REF}"
    echo "algorithm: ${ANN_ALGORITHM}"
    echo "datasets:  ${ANN_DATASETS}"
    echo "runs:      ${ANN_RUNS}"
    echo "server:"
    sed 's/^/  /' "${ANN_OUTPUT_DIR}/run_meta.json" 2>/dev/null || echo "  (no metadata)"
  } > "${ANN_OUTPUT_DIR}/site/METADATA.txt"
  cp "${ANN_OUTPUT_DIR}/results.csv" "${ANN_OUTPUT_DIR}/site/results.csv" 2>/dev/null || true
  cp "${ANN_OUTPUT_DIR}/run_meta.json" "${ANN_OUTPUT_DIR}/site/run_meta.json" 2>/dev/null || true

  # Bundle the whole site into one archive - a single download from CI artifacts.
  log "Archiving site"
  tar -czf "${ANN_OUTPUT_DIR}/ann-benchmark-site.tar.gz" -C "${ANN_OUTPUT_DIR}" site
}

push_metrics() {
  if [[ -z "${PUSHGATEWAY_URL}" ]]; then
    log "PUSHGATEWAY_URL unset - skipping metrics push"
    return
  fi
  # Placeholder: no metrics backend is provisioned yet. When one exists, parse
  # results.csv and push gauges labelled by dataset/nprobe/git_sha here, à la
  # lib/iresearch/scripts/Prometheus/PythonBenchmark.py.
  log "PUSHGATEWAY_URL set (${PUSHGATEWAY_URL}) - metrics push not yet implemented"
}

# ---------------------------------------------------------------------------
main() {
  log "Working directory: ${ANN_WORKDIR}"
  mkdir -p "${ANN_OUTPUT_DIR}"
  ANN_STARTED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  start_arangod
  wait_for_arangod
  collect_meta
  setup_harness
  run_sweep
  export_results
  push_metrics
  log "Done. Artifacts in ${ANN_OUTPUT_DIR} (site/, results.csv, arangod.log)"
}

main "$@"
