#!/usr/bin/env bash
#
# Run the public ann-benchmarks harness against ArangoDB's faiss-based IVF
# vector index and produce the standard ann-benchmarks HTML site + CSV.
#
# arangod is always provided from the outside (a locally started server, the
# CircleCI service container, or the `arangod-sut` compose service); this script
# only waits for it. It clones a pinned ann-benchmarks fork, runs a fixed sweep,
# writes the site + CSV + logs under $ANN_OUTPUT_DIR and, when
# RESULTS_ARANGO_URL is set, loads the run into the results store (schema.js)
# that Grafana reads.
#
#   run_ann_benchmark.sh          full flow
#   run_ann_benchmark.sh setup    only clone the fork + create the venv
#                                 (used by the Dockerfile at build time)
#
# Every knob is an env var; see README.md.

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration (all overridable from the environment)
# ---------------------------------------------------------------------------

# System under test. Single server only.
ARANGO_HOST="${ARANGO_HOST:-127.0.0.1}"
ARANGO_PORT="${ARANGO_PORT:-8529}"
ARANGO_USER="${ARANGO_USER:-root}"
ARANGO_PASSWORD="${ARANGO_PASSWORD:-}"
# Recorded in run_meta.json only; whoever started arangod knows the image.
ARANGODB_IMAGE="${ARANGODB_IMAGE:-}"

# ann-benchmarks fork, pinned. The circle-ci branch carries the CI sweep:
# arangodb-ivf = single nLists=16384 build + nProbe query sweep.
ANN_FORK_URL="${ANN_FORK_URL:-https://github.com/arangodb/ann-benchmarks.git}"
ANN_FORK_REF="${ANN_FORK_REF:-1967a1c9b6be75f7213c89d8be29d05715e2850c}"
ANN_ALGORITHM="${ANN_ALGORITHM:-arangodb-ivf}"
# TODO ANN_DATASETS="${ANN_DATASETS:-sift-128-euclidean glove-100-angular}"
ANN_DATASETS="${ANN_DATASETS:-glove-100-angular}"
# TODO  ANN_RUNS="${ANN_RUNS:-3}"
ANN_RUNS="${ANN_RUNS:-1}"

# Working + output layout.
ANN_WORKDIR="${ANN_WORKDIR:-$(mktemp -d "${TMPDIR:-/tmp}/ann-bench.XXXXXX")}"
ANN_OUTPUT_DIR="${ANN_OUTPUT_DIR:-${ANN_WORKDIR}/output}"
# Persist datasets across runs (hundreds of MB each). Point at a durable path
# so each run doesn't re-download.
ANN_DATASET_CACHE="${ANN_DATASET_CACHE:-${ANN_WORKDIR}/data}"

# Results store (schema.js) + dashboard. Both optional: unset => the run is
# only written to $ANN_OUTPUT_DIR.
RESULTS_ARANGO_URL="${RESULTS_ARANGO_URL:-}"
RESULTS_ARANGO_USER="${RESULTS_ARANGO_USER:-root}"
RESULTS_ARANGO_PASSWORD="${RESULTS_ARANGO_PASSWORD:-}"
# 1 => skip TLS certificate verification of the results store.
RESULTS_ARANGO_INSECURE="${RESULTS_ARANGO_INSECURE:-}"
GRAFANA_URL="${GRAFANA_URL:-}"

ARANGO_URL="http://${ARANGO_HOST}:${ARANGO_PORT}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HARNESS_DIR="${ANN_WORKDIR}/ann-benchmarks"
VENV_DIR="${ANN_WORKDIR}/venv"

log() { printf '\n=== %s ===\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }
arango_curl() { curl -fsS -u "${ARANGO_USER}:${ARANGO_PASSWORD}" "$@"; }

# ---------------------------------------------------------------------------
# arangod: wait, metadata, log
# ---------------------------------------------------------------------------

wait_for_arangod() {
  log "Waiting for arangod at ${ARANGO_URL}"
  for _ in $(seq 1 150); do
    if arango_curl "${ARANGO_URL}/_api/version" >/dev/null 2>&1; then
      log "arangod is up"
      return
    fi
    sleep 2
  done
  die "arangod did not come up within timeout"
}

collect_meta() {
  log "Collecting server metadata (build-id, version)"
  # build-id (ELF GNU sha1) + full version identify the exact binary benchmarked.
  local ver_json bid ver lic ver_full
  ver_json="$(arango_curl "${ARANGO_URL}/_api/version?details=true" 2>/dev/null || echo '{}')"
  bid="$(jq -r '.details["build-id"] // ""' <<<"${ver_json}")"
  ver="$(jq -r '.version // ""' <<<"${ver_json}")"
  lic="$(jq -r '.license // ""' <<<"${ver_json}")"
  ver_full="${ver}"
  [[ -n "${bid}" ]] && ver_full="${ver}, build-id: ${bid}"
  # Cores and memory as arangod sees them; CPU model and kernel are shared with
  # this host (the OS distribution follows from the image).
  local metrics cores mem cpu os
  metrics="$(arango_curl "${ARANGO_URL}/_admin/metrics/v2" 2>/dev/null || true)"
  cores="$(awk '/^arangodb_server_statistics_cpu_cores[{ ]/ {print $2; exit}' <<<"${metrics}")"
  mem="$(awk '/^arangodb_server_statistics_physical_memory[{ ]/ {print $2; exit}' <<<"${metrics}")"
  cpu="$(awk -F': ' '/^model name/ {print $2; exit}' /proc/cpuinfo 2>/dev/null || true)"
  os="$(uname -sr)"
  jq -n \
    --arg version "${ver}" --arg version_full "${ver_full}" \
    --arg build_id "${bid}" --arg license "${lic}" \
    --arg image "${ARANGODB_IMAGE}" \
    --arg started_at "${ANN_STARTED_AT:-}" \
    --arg cpu "${cpu}" --arg cores "${cores}" --arg mem "${mem}" --arg os "${os}" \
    '{arangodb_version: $version, arangodb_version_full: $version_full,
      build_id: $build_id, license: $license, image: $image,
      machine: {cpu: $cpu, cores: ($cores | tonumber? // null),
                memory_bytes: ($mem | tonumber? // null), os: $os},
      started_at: $started_at, ended_at: ""}' \
    > "${ANN_OUTPUT_DIR}/run_meta.json"
  printf '  build-id: %s\n  version:  %s\n' "${bid:-(none)}" "${ver_full}"
  printf '  machine:  %s, %s cores, %s bytes, %s\n' "${cpu:-?}" "${cores:-?}" "${mem:-?}" "${os}"
}

collect_arango_log() {
  log "Fetching arangod log"
  # Via the REST API, since arangod may run where no docker daemon is reachable.
  local raw="${ANN_OUTPUT_DIR}/arangod-log.json"
  if arango_curl "${ARANGO_URL}/_admin/log/entries?upto=info&size=5000" -o "${raw}" 2>/dev/null; then
    jq -r '.messages[]? | "\(.date) [\(.level)] \(.topic): \(.message)"' "${raw}" \
      > "${ANN_OUTPUT_DIR}/arangod.log" 2>/dev/null || cp "${raw}" "${ANN_OUTPUT_DIR}/arangod.log"
  else
    log "  (could not fetch server log)"
  fi
}

# ---------------------------------------------------------------------------
# ann-benchmarks: env, run, export, site
# ---------------------------------------------------------------------------

# Idempotent: a checkout / venv left by `setup` (the Docker image) is reused.
setup_harness() {
  if [[ -d "${HARNESS_DIR}" ]]; then
    log "Reusing ann-benchmarks checkout at ${HARNESS_DIR}"
  else
    log "Cloning ann-benchmarks (${ANN_FORK_REF})"
    git clone --quiet "${ANN_FORK_URL}" "${HARNESS_DIR}"
    git -C "${HARNESS_DIR}" checkout --quiet "${ANN_FORK_REF}"
  fi

  # Persist downloaded datasets outside the (ephemeral) checkout.
  mkdir -p "${ANN_DATASET_CACHE}"
  ln -sfn "${ANN_DATASET_CACHE}" "${HARNESS_DIR}/data"

  local install=1
  if [[ -d "${VENV_DIR}" ]]; then
    log "Reusing Python venv at ${VENV_DIR}"
    install=0
  else
    log "Creating Python venv"
    python3 -m venv "${VENV_DIR}"
  fi
  # The activate script references unset vars; don't let set -u abort here.
  set +u
  # shellcheck disable=SC1091
  source "${VENV_DIR}/bin/activate"
  set -u
  if (( install )); then
    pip install --quiet --upgrade pip
    pip install --quiet -r "${HARNESS_DIR}/requirements.txt"
    pip install --quiet python-arango
  fi
}

run_sweep() {
  cd "${HARNESS_DIR}"
  export ANN_BENCHMARKS_ARANGO_HOST="${ARANGO_HOST}"
  export ANN_BENCHMARKS_ARANGO_PORT="${ARANGO_PORT}"
  export ANN_BENCHMARKS_ARANGO_USER="${ARANGO_USER}"
  export ANN_BENCHMARKS_ARANGO_PASSWORD="${ARANGO_PASSWORD}"
  # Tee the run output; validate_results parses it for how many query-arg groups
  # ann-benchmarks intended to run, to detect silently-dropped configs.
  : > "${ANN_OUTPUT_DIR}/run.log"
  for ds in ${ANN_DATASETS}; do
    log "Benchmarking ${ANN_ALGORITHM} on ${ds}"
    python run.py --local --algorithm "${ANN_ALGORITHM}" \
      --dataset "${ds}" --runs "${ANN_RUNS}" --force \
      2>&1 | tee -a "${ANN_OUTPUT_DIR}/run.log"
  done
}

# Fail the job if any dataset produced fewer result rows than the number of
# query-argument groups ann-benchmarks reported - i.e. some config silently
# produced no results (run.py exits 0 even then).
validate_results() {
  local runlog="${ANN_OUTPUT_DIR}/run.log" csv="${ANN_OUTPUT_DIR}/results.csv"
  local expected
  expected="$(grep -oE 'query argument group [0-9]+ of [0-9]+' "${runlog}" 2>/dev/null \
                | grep -oE '[0-9]+$' | sort -rn | head -n1)"
  if [[ -z "${expected}" ]]; then
    log "WARNING: could not determine expected config count; skipping validation"
    return 0
  fi
  local failed=0 ds actual
  for ds in ${ANN_DATASETS}; do
    # dataset is the last CSV column (no commas); strip the CRLF that Python's
    # csv writer leaves before comparing.
    actual="$(awk -F, -v d="${ds}" \
      'NR>1 {last=$NF; sub(/\r$/,"",last); if (last==d) c++} END {print c+0}' \
      "${csv}" 2>/dev/null)"
    if (( actual < expected )); then
      log "MISSING RESULTS: ${ds} produced ${actual}/${expected} configs"
      failed=1
    else
      log "OK: ${ds} produced ${actual}/${expected} configs"
    fi
  done
  (( failed == 0 )) || die "incomplete benchmark: some query configs produced no results (see run.log / arangod.log)"
}

export_results() {
  cd "${HARNESS_DIR}"
  mkdir -p "${ANN_OUTPUT_DIR}/site"
  log "Exporting CSV"
  python data_export.py --out "${ANN_OUTPUT_DIR}/results.csv"
  log "Generating website"
  python create_website.py --outputdir "${ANN_OUTPUT_DIR}/site" --scatter --latex

  # Stamp the run's end time into the metadata.
  local endtmp; endtmp="$(mktemp)"
  jq --arg ended "$(date -u +%Y-%m-%dT%H:%M:%SZ)" '.ended_at = $ended' \
     "${ANN_OUTPUT_DIR}/run_meta.json" > "${endtmp}" \
     && mv "${endtmp}" "${ANN_OUTPUT_DIR}/run_meta.json"

  # Self-describing artifact: what produced these numbers.
  {
    echo "generated: ${ANN_STAMP:-unknown}"
    echo "fork:      ${ANN_FORK_URL} @ ${ANN_FORK_REF}"
    echo "algorithm: ${ANN_ALGORITHM}"
    echo "datasets:  ${ANN_DATASETS}"
    echo "runs:      ${ANN_RUNS}"
    [[ -n "${RESULTS_ARANGO_URL}" ]] && echo "results:   ${RESULTS_ARANGO_URL}"
    [[ -n "${GRAFANA_URL}" ]] && echo "dashboard: ${GRAFANA_URL}/d/ann-bench"
    echo "server:"
    sed 's/^/  /' "${ANN_OUTPUT_DIR}/run_meta.json" 2>/dev/null || echo "  (no metadata)"
  } > "${ANN_OUTPUT_DIR}/site/METADATA.txt"
  # Gather the artifacts (results + logs) into the site bundle.
  for f in results.csv run_meta.json arangod.log arangod-log.json run.log; do
    cp "${ANN_OUTPUT_DIR}/${f}" "${ANN_OUTPUT_DIR}/site/${f}" 2>/dev/null || true
  done

  # Bundle the whole site into one archive - a single download from CI artifacts.
  log "Archiving site"
  tar -czf "${ANN_OUTPUT_DIR}/ann-benchmark-site.tar.gz" -C "${ANN_OUTPUT_DIR}" site
}

load_results() {
  if [[ -z "${RESULTS_ARANGO_URL}" ]]; then
    log "RESULTS_ARANGO_URL unset - skipping results load"
    return
  fi
  log "Loading run into results store at ${RESULTS_ARANGO_URL}"
  local insecure=()
  [[ "${RESULTS_ARANGO_INSECURE}" == 1 ]] && insecure=(--insecure)
  python3 "${SCRIPT_DIR}/load_run.py" "${ANN_OUTPUT_DIR}" \
    --url "${RESULTS_ARANGO_URL}" \
    --user "${RESULTS_ARANGO_USER}" --password "${RESULTS_ARANGO_PASSWORD}" \
    "${insecure[@]}"
  if [[ -n "${GRAFANA_URL}" ]]; then
    log "Dashboard: ${GRAFANA_URL}/d/ann-bench"
  fi
}

# ---------------------------------------------------------------------------
main() {
  log "Working directory: ${ANN_WORKDIR}"
  mkdir -p "${ANN_OUTPUT_DIR}"
  ANN_STARTED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  wait_for_arangod
  collect_meta
  setup_harness
  run_sweep
  collect_arango_log   # while arangod is still up
  export_results       # writes artifacts (incl. logs) BEFORE validation can fail
  validate_results     # fails the job if any config produced no results
  load_results         # only complete runs reach the results store
  log "Done. Artifacts in ${ANN_OUTPUT_DIR} (site/, results.csv, arangod.log)"
}

case "${1:-}" in
  setup) setup_harness ;;
  "")    main ;;
  *)     die "unknown command '$1' (expected: setup, or no argument)" ;;
esac
