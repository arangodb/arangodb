# ANN benchmark dashboard (ArangoDB → Grafana)

Store ANN benchmark runs in **ArangoDB** (dogfood) and view them in Grafana.
Grafana has no native ArangoDB datasource, so the [Infinity
datasource](https://github.com/grafana/grafana-infinity-datasource) queries
ArangoDB's HTTP API (`POST /_api/cursor` with AQL) and reads the `result` array.

## Files

- `docker-compose.yml` — ArangoDB store (host `:8530`) + Grafana (`:3000`, Infinity
  plugin auto-installed, datasource + dashboard provisioned).
- `load_run.py` — loads one `data_export.py` CSV into ArangoDB collections
  (`runs`, `recall_qps`, `recall_lat`, `build_time`), denormalized and idempotent.
- `run_and_load.sh` — end-to-end: run `../run_ann_benchmark.sh`, then load.
- `grafana/provisioning/` — Infinity datasource (`uid: arango-infinity`) + dashboard provider.
- `grafana/dashboards/ann-benchmark.json` — the dashboard.

## Data model (denormalized for join-free AQL)

Each measurement doc carries its run + dataset + index labels (incl. the
identity fields), so panels group by run without a join:

- `runs` — `{run_id, arangodb_version, build_id, docker_image, docker_image_id, started_at, ended_at}`
- `recall_qps` — `{..., build_id, dataset, index_type, index_params, epsilon, recall, qps}`
- `recall_lat` — `{..., build_id, percentile, recall, latency}`
- `build_time` — `{..., build_id, build_time}`

`build_id` is the arangod ELF GNU build-id (sha1) read from
`/_api/version?details=true` — it identifies the exact binary benchmarked, so
two runs of the same version but different builds stay distinct.

## One-time setup

```bash
docker compose up -d          # ArangoDB (:8530) + Grafana (:3000)
pip install python-arango
```

ArangoDB: `http://localhost:8530` (no auth) · Grafana: http://localhost:3000
(anonymous view; admin/admin to edit).

## Run + load

```bash
# whole flow (benchmark then load)
./run_and_load.sh

# or load an existing results.csv only
./load_run.py --csv ../bench-out/results.csv \
  --arangodb-version devel-nightly \
  --docker-image arangodb/enterprise-preview:devel-nightly \
  --url http://localhost:8530 --db bench

# inspect a CSV's columns without a DB
./load_run.py --csv results.csv --arangodb-version x --docker-image y --dry-run
```

## Dashboard

Provisioned automatically (folder → *ANN benchmark — ArangoDB vector index*).
**Run-by-run** (not time-series), filtered by the `$dataset` variable:

1. **Runs** — table of every run: `started`, version, **build_id**, **docker_image**,
   **docker_image_id**, run_id.
2. **Recall vs QPS by run** — XY chart, one series per run (labelled by short build-id).
3. **Recall vs p95 latency by run** — XY chart, one series per run.
4. **Build time by run** — bar chart, one bar per run.

Runs are keyed by build-id, so each distinct binary is its own series/bar.

## Notes / likely tweaks

- **Infinity is version-sensitive.** The XY-chart field mapping (`x=recall`,
  `y=qps`) and the Infinity POST body may need a click in the UI on your Grafana
  version. Open a panel → *Edit* to confirm the query returns rows.
- **QPS is machine-noisy**; recall and build-time are the trustworthy signals.
- **Auth**: the store runs with `ARANGO_NO_AUTH`. If you enable auth, set
  `ARANGO_STORE_USER`/`ARANGO_STORE_PASSWORD` for the loader and add basic-auth
  to the Infinity datasource.
