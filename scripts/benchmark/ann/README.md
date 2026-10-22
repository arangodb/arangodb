# ann-benchmarks → ArangoDB → Grafana

Benchmarks ArangoDB's vector index with [ann-benchmarks](https://github.com/arangodb/ann-benchmarks)
and optionally loads each run into a results ArangoDB that Grafana charts.

## Components

- **Benchmark runner** (`run_ann_benchmark.sh`, `Dockerfile`): runs the arangodb fork of ann-benchmarks
  (pinned sha) on Python 3.11 against an existing arangod (`--vector-index=true`) and writes the
  HTML site, `results.csv` and logs. Needs `git`, `curl`, `jq` when run without Docker.
- **Results store** (`arangodb:latest`, database `ann-bench`): `schema.js` creates the collections
  idempotently; `load_run.py` (stdlib only) loads one run in a single transaction.
- **Grafana** (`grafana/grafana:12.4.10`): reads the store through the
  [Infinity](https://github.com/grafana/grafana-infinity-datasource) datasource plugin 4.0.0, which
  needs Grafana >= 12.2.5. Panels POST AQL to `/_db/ann-bench/_api/cursor`. Provisions the
  `ann-bench` (trends over runs) and `ann-bench-compare` (baseline vs candidate run) dashboards.

## Local

Start an arangod with `--vector-index=true`, then:

```bash
./run_local.sh                    # output in ./ann-output (site/index.html, results.csv, logs)
```

Results store + Grafana (http://localhost:3000, `admin` / `admin`):

```bash
docker compose up -d
./load_run.py <run-dir>           # dir with results.csv + run_meta.json; --dry-run to check
```

Full CI flow locally: `docker compose --profile ci up --build harness`.

## CI

Trigger a pipeline with `run-ann-bench` = `true`; `ann-bench-arango-image` picks the image under
test, `ann-bench-build-arango` = `true` benchmarks the branch instead, running `glove-100-angular`
and `sift-128-euclidean` as parallel jobs. Store credentials come from
the `ann-benchmark-store` context. Artifacts are under `ann-benchmark/`.
