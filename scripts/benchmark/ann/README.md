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

The scripts here also set up the results store and Grafana locally, so the whole flow
(benchmark → store → dashboards) can be seen on one machine. To do that:

1. Run the arangod under test somewhere, with `--vector-index=true`. The local store takes
   port 8529, so use another one, e.g.:

   ```bash
   docker run -d -p 8530:8529 -e ARANGO_NO_AUTH=1 arangodb/enterprise-preview:devel-nightly --vector-index=true
   ```

2. Start the results store + Grafana (http://localhost:3000, `admin` / `admin`). With
   `AWS_ARANGODB_*` unset, Grafana reads the local store:

   ```bash
   env -u AWS_ARANGODB_URL -u AWS_ARANGODB_ID -u AWS_ARANGODB_KEY docker compose up -d
   ```

3. Set these env flags and run the benchmark:

   | Variable | Example | Meaning |
   |---|---|---|
   | `ARANGO_HOST` / `ARANGO_PORT` | `127.0.0.1` / `8530` | arangod under test |
   | `ARANGO_USER` / `ARANGO_PASSWORD` | `root` / empty | its credentials |
   | `RESULTS_ARANGO_URL` | `http://127.0.0.1:8529` | results store; unset skips loading |
   | `GRAFANA_URL` | `http://localhost:3000` | printed as the dashboard link |
   | `ANN_DATASETS` / `ANN_RUNS` | `glove-100-angular` / `1` | datasets (space-separated), runs per config |

   ```bash
   ARANGO_PORT=8530 RESULTS_ARANGO_URL=http://127.0.0.1:8529 GRAFANA_URL=http://localhost:3000 \
     ./run_local.sh                # output in ./ann-output (site/index.html, results.csv, logs)
   ```

   The run then shows up at http://localhost:3000/d/ann-bench. A run downloaded from CI is
   loaded with `./load_run.py <run-dir>` (`--dry-run` to check it first).

`docker compose --profile ci up --build harness` does all three steps in one go, like CI.

## CI

`.circleci/benchmarks.yml` is its own pipeline, triggered manually or by schedule with
`benchmark` = `ann`. `arango-image` picks the image under test; `build-arango` = `true` benchmarks
a release build of the branch instead. Either way `glove-100-angular` and `sift-128-euclidean` run
as parallel jobs. Store credentials come from the `ann-benchmark-store` context. Artifacts are
under `ann-benchmark/`.
