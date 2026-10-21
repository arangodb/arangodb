# ann-benchmarks → ArangoDB → Grafana

Benchmarks ArangoDB's vector index with the public [ann-benchmarks](https://github.com/erikbern/ann-benchmarks)
harness (pinned fork), produces the standard HTML site + CSV, and optionally stores each run
in a results ArangoDB that Grafana charts.

| File | Purpose |
|---|---|
| `run_ann_benchmark.sh` | The one script: wait for arangod → sweep → site/CSV/logs → validate → load into results store |
| `Dockerfile` | Harness image: pinned fork + Python deps baked in (`run_ann_benchmark.sh setup`) |
| `run_local.sh` | Local mode: build the image, run it against your arangod, output in `./ann-output` |
| `docker-compose.yml` | Results ArangoDB + one-shot `schema` job + Grafana; `--profile ci` adds a SUT arangod + the harness |
| `schema.js` | Creates database `ann-bench`, collections and unique indexes; idempotent |
| `load_run.py` | Loads one `results.csv` as one run, in one transaction (stdlib only) |
| `grafana/provisioning/` | Infinity datasource + `ann-bench` (trends) and `ann-bench-compare` dashboards |

arangod is never started by the script. Someone else provides it: you, the CircleCI service
container, or the `arangod-sut` compose service.

## Local mode

Start an arangod with `--vector-index=true` (default `127.0.0.1:8529`), then from any directory:

```bash
/path/to/scripts/benchmark/ann/run_local.sh
open ann-output/site/index.html
```

`ann-output/` also holds `results.csv`, `run.log`, `arangod.log`, `run_meta.json`
and `ann-benchmark-site.tar.gz`. Datasets are cached in `~/.cache/ann-benchmarks`
(`ANN_DATASET_CACHE` to change). Every `ARANGO_*`, `ANN_*`, `RESULTS_ARANGO_*` and `GRAFANA_URL`
variable in your shell is forwarded into the container:

```bash
ARANGO_PORT=8530 ANN_DATASETS="sift-128-euclidean glove-100-angular" ANN_RUNS=3 run_local.sh
```

The container uses host networking. On Docker Desktop without it, set
`ARANGO_HOST=host.docker.internal`.

To also load the run into Grafana, start the results store with `docker compose up -d` (see
below). It takes host port 8529, so run your arangod on another port:

```bash
ARANGO_PORT=8530 RESULTS_ARANGO_URL=http://127.0.0.1:8529 GRAFANA_URL=http://localhost:3000 \
  run_local.sh
```

Without Docker, run the script directly; it needs `python3` (with `venv`), `git`, `curl` and `jq`:

```bash
ANN_OUTPUT_DIR=./ann-output ANN_DATASET_CACHE=~/.cache/ann-benchmarks ./run_ann_benchmark.sh
```

## CI mode

The CircleCI job `ann-benchmark` (pipeline parameter `run-ann-bench`) runs
`run_ann_benchmark.sh` on `cimg/python` with arangod from the image `ann-bench-arango-image` as a service
container, then loads the run into the shared results store at `AWS_FIGURES_URL` as
`AWS_ARANGODB_ID` / `AWS_ARANGODB_KEY`, all from the `ann-benchmark-store` context (see
[Shared results store](#shared-results-store)). The image is the system under test; the branch
only provides the harness scripts.

With `ann-bench-build-arango` = `true` as well, the branch itself is benchmarked instead:
`build-x64-for-ann-benchmark` compiles it with the `ann-benchmark-x64` preset (the nightly release
build `nightly-package-x64`, built in `build/`), and `ann-benchmark-build` installs that build on
`cimg/python`, starts arangod with `--vector-index true` and runs the same benchmark.

| Nightly | `ann-bench-arango-image` |
|---|---|
| `devel` | `arangodb/enterprise-preview:devel-nightly` (default) |
| `4.0` | `arangodb/core-preview:4.0-nightly` |

Both nightlies publish around 02:00 UTC. The nightly benchmarks are two CircleCI pipeline schedules
(Project Settings → Triggers) on the respective branch, started after that, each with
`run-ann-bench` = `true` and its `ann-bench-arango-image`. They are independent of the nightly
pipeline: if a nightly fails to publish, the benchmark runs the previous image, and the run records
that image's version (with its nightly date).

To start it manually, open the branch in CircleCI, choose **Trigger Pipeline** and add the boolean
parameter `run-ann-bench` = `true` (and `ann-bench-arango-image` for another image, or
`ann-bench-build-arango` = `true` to benchmark the branch). Or through the API, with a personal API token:

```bash
curl -X POST https://circleci.com/api/v2/project/gh/arangodb/arangodb/pipeline \
  -H "Circle-Token: ${CIRCLECI_TOKEN}" -H "Content-Type: application/json" \
  -d '{"branch": "<branch>", "parameters": {"run-ann-bench": true}}'
```

The normal test workflows are replaced by the benchmark for that run. Artifacts are under
`ann-benchmark/`: `ann-benchmark-site.tar.gz` (site + CSV + logs) and the unpacked `site/`; builds of
the branch also have `arangod-server.log`.

| Variable | Default | Meaning |
|---|---|---|
| `ARANGO_HOST` / `ARANGO_PORT` | `127.0.0.1` / `8529` | arangod under test |
| `ARANGO_USER` / `ARANGO_PASSWORD` | `root` / empty | its credentials |
| `ARANGODB_IMAGE` | empty | image that runs it; recorded in `run_meta.json` only |
| `RESULTS_ARANGO_URL` | empty | results store; unset skips loading |
| `RESULTS_ARANGO_USER` / `RESULTS_ARANGO_PASSWORD` | `root` / empty | its credentials |
| `RESULTS_ARANGO_INSECURE` | empty | `1` skips TLS certificate verification of the store |
| `GRAFANA_URL` | empty | printed as `<url>/d/ann-bench` in the log and `METADATA.txt` |
| `ANN_ALGORITHM` | `arangodb-ivf` | algorithm name in the fork's `config.yml` |
| `ANN_DATASETS` | `glove-100-angular` | space-separated |
| `ANN_RUNS` | `1` | best-of runs per query config |
| `ANN_FORK_URL` / `ANN_FORK_REF` | arangodb fork, pinned sha | harness source |
| `ANN_OUTPUT_DIR` / `ANN_DATASET_CACHE` / `ANN_WORKDIR` | temp dir | layout |
| `ANN_STAMP` | `unknown` | label for the run, written to `site/METADATA.txt` |
| `ANN_HARNESS_IMAGE` | `ann-bench-harness` | image tag `run_local.sh` builds and runs |

## Shared results store

One-time setup of the store at `AWS_FIGURES_URL` (an `https://` URL) for CI. The database
`ann-bench` already exists there and `AWS_ARANGODB_ID` has read/write on it, which is all that is
needed:

1. Create the collections and indexes. `schema.js` is idempotent, so re-run it after schema
   changes:

   ```bash
   arangosh --server.endpoint "ssl://${AWS_FIGURES_URL#https://}:443" --server.database ann-bench \
     --server.username "$AWS_ARANGODB_ID" --server.password "$AWS_ARANGODB_KEY" \
     --javascript.execute schema.js
   ```

2. Add `AWS_FIGURES_URL`, `AWS_ARANGODB_ID` and `AWS_ARANGODB_KEY` to the CircleCI context `ann-benchmark-store`
   (Organization Settings → Contexts).

3. Check by loading a downloaded run by hand:

   ```bash
   ./load_run.py <run-dir> --url "$AWS_FIGURES_URL" --insecure \
     --user "$AWS_ARANGODB_ID" --password "$AWS_ARANGODB_KEY"
   ```

## Reproduce CI locally

```bash
cd scripts/benchmark/ann
docker compose --profile ci up --build harness      # SUT + store + Grafana + harness
```

The harness benchmarks `arangod-sut` (host port 8530), loads the run into `arangodb`
(host port 8529) and exits; the site is in `./ann-output/site`, the run at
http://localhost:3000/d/ann-bench. `ARANGODB_IMAGE=<tag>` swaps the SUT image, `ANN_DATASETS` /
`ANN_RUNS` pass through.

Without the profile, `docker compose up -d` brings up only the store + Grafana, e.g. to load
CI runs by hand. Each run is a directory with `results.csv` and `run_meta.json` (the
artifact's `site/`); runs already in the store are skipped:

```bash
for r in ~/Downloads/run*; do ./load_run.py "$r"; done
```

`--dry-run` checks a run without touching a database.

## Grafana

Two dashboards are provisioned:

- **ann-benchmarks** (`/d/ann-bench`): QPS, recall, p50 and p99 latency over runs, one line per
  image (`image` variable, i.e. per nightly branch) and nProbe (`nprobe` variable, default
  16/64/256), plus index build time per run as a line chart and a table.
- **ann-benchmarks compare runs** (`/d/ann-bench-compare`): recall-vs-QPS and recall-vs-latency
  (p50, p95, p99, p99.9) curves of a `candidate` run (default: the latest) against a `baseline`
  run (default: the previous run of the same image).

To add a panel: datasource `bench`, type **JSON**, source **URL**, method **POST**,
URL `http://arangodb:8529/_db/ann-bench/_api/cursor`, body type raw / JSON, root selector `result`:

```json
{"query": "FOR m IN measurement_recall_qps FILTER m.run_id == '$run' AND m.epsilon == 0 LET d = DOCUMENT('dataset', m.dataset_id) FILTER d.name == 'glove-100-angular' LET ix = DOCUMENT('index_setup', m.index_id) SORT m.recall RETURN {recall: m.recall, qps: m.qps, params: ix.index_params.descriptor}"}
```

Build-time regression across versions:

```aql
FOR m IN measurement_build_time
  LET r = DOCUMENT('run', m.run_id)
  LET d = DOCUMENT('dataset', m.dataset_id)
  FILTER d.name == 'glove-100-angular'
  SORT r.started_at
  RETURN {time: r.started_at, arangodb_version: r.arangodb_version, build_time: m.build_time}
```

## Notes

- **Build time**: ann-benchmarks repeats it on every query-parameter row; the loader stores it
  once per run and dataset, and the build panels collapse identical pairs.
- **Index params** are stored as `{"descriptor": "<ann-benchmarks params string>", ...}` plus the
  parsed params, e.g. `metric: "cosine", factory: "IVF,Flat", nLists: 16384, nProbe: 16` (numbers
  as numbers). The descriptor identifies the index setup; loading a run fills in the parsed params
  of setups stored before they existed.
- **Units**: latency ms, build time s, qps 1/s.
- **Machine**: each `run` stores `machine: {cpu, cores, memory_bytes, os}`. Cores and memory are
  what arangod reports; the CPU model and `os` (kernel, e.g. `Linux 6.8.0-147-generic`) come from
  the harness host, which arangod shares. Runs loaded without it have `machine: null`.
- **CSV columns**: `load_run.py` expects `k-nn / epsilon / largeepsilon / qps / p50 / p95 / p99 /
  p999 / build`; `--dry-run` prints the real header if the fork differs.
- **Schema changes**: `docker compose up schema` re-applies additions; changing an existing index
  needs `docker compose down -v`.
- **Auth on the results store**: replace `ARANGO_NO_AUTH` with `ARANGO_ROOT_PASSWORD` in the
  compose file, set `RESULTS_ARANGO_PASSWORD`, and switch the Grafana datasource to
  `auth_method: basicAuth`.
