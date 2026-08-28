#!/usr/bin/env bash
#
# Run the public ann-benchmarks harness against ArangoDB's faiss-based IVF
# vector index and produce the standard ann-benchmarks HTML site + CSV.
#
# CI-agnostic: it downloads an arangod tarball, boots it locally with the
# experimental vector index enabled, clones a pinned ann-benchmarks fork and
# runs a fixed sweep (single nLists build, nProbe query sweep) over one L2 and
# one cosine dataset. Runnable on a laptop too - every knob is an env var.
#
# Everything is written under $ANN_WORKDIR (default: a fresh temp dir). The
# arangod instance and the extracted tarball are torn down on exit.

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration (all overridable from the environment)
# ---------------------------------------------------------------------------

# Where the arangod tarball comes from. Leave ARANGODB_PACKAGE_URL empty to
# auto-resolve the newest enterprise devel nightly for linux/x86_64 (there is
# no community nightly tarball; enterprise carries the same in-tree vector
# index and needs no license below its disk-usage limit - ~100 GiB, which
# these datasets, ~0.5 GB each, never approach).
ARANGODB_NIGHTLY_INDEX="${ARANGODB_NIGHTLY_INDEX:-https://download.arangodb.com/nightly/devel/Linux/x86_64/}"
ARANGODB_PACKAGE_URL="${ARANGODB_PACKAGE_URL:-}"

# ann-benchmarks fork, pinned. The circle-ci branch carries the CI sweep:
# arangodb-ivf = single nLists=16384 build + nProbe query sweep.
ANN_FORK_URL="${ANN_FORK_URL:-https://github.com/jbajic/ann-benchmarks.git}"
ANN_FORK_REF="${ANN_FORK_REF:-1967a1c9b6be75f7213c89d8be29d05715e2850c}"
ANN_ALGORITHM="${ANN_ALGORITHM:-arangodb-ivf}"
# ANN_DATASETS="${ANN_DATASETS:-sift-128-euclidean glove-100-angular}"
ANN_DATASETS="${ANN_DATASETS:-glove-100-angular}"
ANN_RUNS="${ANN_RUNS:-3}"

# arangod runtime knobs.
ARANGO_HOST="${ARANGO_HOST:-127.0.0.1}"
ARANGO_PORT="${ARANGO_PORT:-8529}"
# The startup flag that unlocks the experimental vector index. Kept as a
# variable because its exact spelling has changed across versions.
VECTOR_INDEX_FLAG="${VECTOR_INDEX_FLAG:---experimental-vector-index=true}"

# Working + output layout.
ANN_WORKDIR="${ANN_WORKDIR:-$(mktemp -d "${TMPDIR:-/tmp}/ann-bench.XXXXXX")}"
ANN_OUTPUT_DIR="${ANN_OUTPUT_DIR:-${ANN_WORKDIR}/output}"
# Persist datasets across runs (hundreds of MB each). Point at a durable path
# on CI so each run doesn't re-download.
ANN_DATASET_CACHE="${ANN_DATASET_CACHE:-${ANN_WORKDIR}/data}"

# Optional metrics push target. Empty => the push step is a no-op.
PUSHGATEWAY_URL="${PUSHGATEWAY_URL:-}"

ARANGO_URL="http://${ARANGO_HOST}:${ARANGO_PORT}"
ARANGOD_PID=""

log() { printf '\n=== %s ===\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

cleanup() {
  if [[ -n "${ARANGOD_PID}" ]] && kill -0 "${ARANGOD_PID}" 2>/dev/null; then
    log "Stopping arangod (pid ${ARANGOD_PID})"
    kill "${ARANGOD_PID}" 2>/dev/null || true
    wait "${ARANGOD_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
# arangod: fetch, start, wait
# ---------------------------------------------------------------------------

resolve_package_url() {
  if [[ -n "${ARANGODB_PACKAGE_URL}" ]]; then
    echo "${ARANGODB_PACKAGE_URL}"
    return
  fi
  # Pick the server tarball from the nightly index: arangodb3e-linux-...tar.gz,
  # excluding the client-only and object-files (debug) variants.
  local listing name
  listing="$(curl -fsSL "${ARANGODB_NIGHTLY_INDEX}")" \
    || die "cannot list nightly index ${ARANGODB_NIGHTLY_INDEX}"
  name="$(grep -oE 'arangodb3e-linux-[0-9][^"'\'' ]*_x86_64\.tar\.gz' <<<"${listing}" \
            | grep -vE 'client|object_files' | sort -u | tail -n1)"
  [[ -n "${name}" ]] || die "no server tarball found in ${ARANGODB_NIGHTLY_INDEX}"
  echo "${ARANGODB_NIGHTLY_INDEX%/}/${name}"
}

fetch_arangod() {
  local url tarball
  url="$(resolve_package_url)"
  log "Downloading arangod: ${url}"
  tarball="${ANN_WORKDIR}/arangod.tar.gz"
  curl -fSL -o "${tarball}" "${url}" || die "download failed"
  mkdir -p "${ANN_WORKDIR}/arangod"
  tar -xzf "${tarball}" -C "${ANN_WORKDIR}/arangod"
  rm -f "${tarball}"

  ARANGOD_BIN="$(find "${ANN_WORKDIR}/arangod" -type f -path '*/usr/sbin/arangod' | head -n1)"
  [[ -n "${ARANGOD_BIN}" ]] || die "arangod binary not found in tarball"
  # The relocatable tarball ships a config with all paths relative to the
  # binary - the supported way to run it in place.
  ARANGOD_CONF="$(find "${ANN_WORKDIR}/arangod" -type f -path '*/etc/relative/arangod.conf' | head -n1)"
  [[ -n "${ARANGOD_CONF}" ]] || die "relative arangod.conf not found in tarball"
  log "arangod: ${ARANGOD_BIN}"
  "${ARANGOD_BIN}" --version | head -n5 || true
}

start_arangod() {
  local datadir="${ANN_WORKDIR}/db" logfile="${ANN_OUTPUT_DIR}/arangod.log"
  mkdir -p "${datadir}" "${ANN_OUTPUT_DIR}"
  log "Starting arangod on ${ARANGO_URL}"
  "${ARANGOD_BIN}" \
    --configuration "${ARANGOD_CONF}" \
    --database.directory "${datadir}" \
    --server.endpoint "tcp://${ARANGO_HOST}:${ARANGO_PORT}" \
    --server.authentication false \
    "${VECTOR_INDEX_FLAG}" \
    >"${logfile}" 2>&1 &
  ARANGOD_PID=$!
}

wait_for_arangod() {
  log "Waiting for arangod to accept connections"
  for _ in $(seq 1 120); do
    if curl -fsS "${ARANGO_URL}/_api/version" >/dev/null 2>&1; then
      log "arangod is up"
      return
    fi
    kill -0 "${ARANGOD_PID}" 2>/dev/null || die "arangod exited early; see ${ANN_OUTPUT_DIR}/arangod.log"
    sleep 2
  done
  die "arangod did not come up within timeout; see ${ANN_OUTPUT_DIR}/arangod.log"
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

  # Self-describing artifact: what produced these numbers.
  {
    echo "generated: ${ANN_STAMP:-unknown}"
    echo "arangod:   $(${ARANGOD_BIN} --version | head -n1)"
    echo "fork:      ${ANN_FORK_URL} @ ${ANN_FORK_REF}"
    echo "algorithm: ${ANN_ALGORITHM}"
    echo "datasets:  ${ANN_DATASETS}"
    echo "runs:      ${ANN_RUNS}"
  } > "${ANN_OUTPUT_DIR}/site/METADATA.txt"
  cp "${ANN_OUTPUT_DIR}/results.csv" "${ANN_OUTPUT_DIR}/site/results.csv" 2>/dev/null || true
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
  fetch_arangod
  start_arangod
  wait_for_arangod
  setup_harness
  run_sweep
  export_results
  push_metrics
  log "Done. Artifacts in ${ANN_OUTPUT_DIR} (site/, results.csv, arangod.log)"
}

main "$@"
