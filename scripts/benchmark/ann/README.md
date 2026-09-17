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
| `schema.js` | Creates database `bench`, collections and unique indexes; idempotent |
| `load_run.py` | Loads one `results.csv` as one run, in one transaction (stdlib only) |
| `grafana/provisioning/` | Infinity datasource + `ann-bench` dashboard |

arangod is never started by the script. Someone else provides it: you, the CircleCI service
container, or the `arangod-sut` compose service.

## Local mode

Start an arangod with `--vector-index=true` (default `127.0.0.1:8529`), then from any directory:

```bash
/path/to/scripts/benchmark/ann/run_local.sh
open ann-output/site/index.html
```

`ann-output/` also holds `results.csv`, `run.log`, `arangod.log`, `run_meta.json`, `diagnose.txt`
and `ann-benchmark-site.tar.gz`. Datasets are cached in `~/.cache/ann-benchmarks`
(`ANN_DATASET_CACHE` to change). Every `ARANGO_*`, `ANN_*`, `RESULTS_ARANGO_*` and `GRAFANA_URL`
variable in your shell is forwarded into the container:

```bash
ARANGO_PORT=8530 ANN_DATASETS="sift-128-euclidean glove-100-angular" ANN_RUNS=3 run_local.sh
```

The container uses host networking. On Docker Desktop without it, set
`ARANGO_HOST=host.docker.internal`.

## CI mode

The CircleCI job `ann-benchmark` (manual, pipeline parameter `run-ann-benchmark`) runs
`run_ann_benchmark.sh` on `cimg/python` with arangod as a service container. The results store
and Grafana are expected to be running already; their location comes from the
`ann-benchmark-store` context. Until that context exists the load step logs
`RESULTS_ARANGO_URL unset - skipping results load` and the job still publishes the site.

| Variable | Default | Meaning |
|---|---|---|
| `ARANGO_HOST` / `ARANGO_PORT` | `127.0.0.1` / `8529` | arangod under test |
| `ARANGO_USER` / `ARANGO_PASSWORD` | `root` / empty | its credentials |
| `ARANGODB_IMAGE` | empty | image that runs it; recorded in `run_meta.json` only |
| `RESULTS_ARANGO_URL` | empty | results store; unset skips loading |
| `RESULTS_ARANGO_PASSWORD` | empty | its `root` password |
| `GRAFANA_URL` | empty | printed as `<url>/d/ann-bench` in the log and `METADATA.txt` |
| `ANN_ALGORITHM` | `arangodb-ivf` | algorithm name in the fork's `config.yml` |
| `ANN_DATASETS` | `glove-100-angular` | space-separated |
| `ANN_RUNS` | `1` | best-of runs per query config |
| `ANN_FORK_URL` / `ANN_FORK_REF` | jbajic fork, pinned sha | harness source |
| `ANN_OUTPUT_DIR` / `ANN_DATASET_CACHE` / `ANN_WORKDIR` | temp dir | layout |

Apply the schema once to the shared results store:

```bash
arangosh --server.endpoint tcp://<results-host>:8529 --javascript.execute schema.js
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

Without the profile, `docker compose up -d` brings up only the store + Grafana, e.g. to load a
CSV from a CI artifact by hand:

```bash
tar -xzf ann-benchmark-site.tar.gz
./load_run.py --csv site/results.csv \
  --arangodb-version "$(jq -r .arangodb_version_full site/run_meta.json)" \
  --docker-image     "$(jq -r .image site/run_meta.json)" \
  --started-at       "$(jq -r .started_at site/run_meta.json)" \
  --ended-at         "$(jq -r .ended_at site/run_meta.json)"
```

`--dry-run` checks a CSV without touching a database.

## Grafana

The provisioned "ann-benchmarks" dashboard overlays all selected runs on recall-vs-QPS and
recall-vs-p50-latency charts and shows index build time per run as a bar chart and a table,
with `dataset` and (multi-select) `run` variables.

To add a panel: datasource `bench`, type **JSON**, source **URL**, method **POST**,
URL `http://arangodb:8529/_db/bench/_api/cursor`, body type raw / JSON, root selector `result`:

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
- **Index params** are stored opaquely as `{"descriptor": "<ann-benchmarks params string>"}`.
- **Units**: latency ms, build time s, qps 1/s.
- **CSV columns**: `load_run.py` expects `k-nn / epsilon / largeepsilon / qps / p50 / p95 / p99 /
  build`; `--dry-run` prints the real header if the fork differs.
- **Schema changes**: `docker compose up schema` re-applies additions; changing an existing index
  needs `docker compose down -v`.
- **Auth on the results store**: replace `ARANGO_NO_AUTH` with `ARANGO_ROOT_PASSWORD` in the
  compose file, set `RESULTS_ARANGO_PASSWORD`, and switch the Grafana datasource to
  `auth_method: basicAuth`.
