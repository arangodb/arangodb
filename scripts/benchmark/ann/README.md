# ANN benchmark (vector index)

Benchmarks ArangoDB's faiss-based **IVF** vector index with the public
[ann-benchmarks](https://github.com/erikbern/ann-benchmarks) harness and
produces its standard HTML site + a `results.csv`.

The heavy lifting lives in [`run_ann_benchmark.sh`](run_ann_benchmark.sh),
which is CI-agnostic: it downloads an `arangod` tarball, boots it locally with
the experimental vector index enabled, clones a pinned ann-benchmarks fork and
runs a fixed sweep over one L2 and one cosine dataset. It runs the same on a
laptop and in CircleCI.

## What it runs

- **arangod**: newest enterprise `devel` nightly for linux/x86_64 from
  `download.arangodb.com/nightly` (no community nightly tarball exists; the
  vector index is the same in-tree code). No license is needed: unlicensed
  enterprise only restricts dataset disk usage (~100 GiB), which these
  datasets (~0.5 GB each) never approach.
- **Harness**: `jbajic/ann-benchmarks`, branch `circle-ci`, pinned by SHA.
- **Index**: `arangodb-ivf` — a single `nLists=16384` IVF-flat build with an
  `nProbe` query sweep (single-query, `--runs 3`).
- **Datasets**: `sift-128-euclidean` (L2) and `glove-100-angular` (cosine).

## Run locally

```bash
scripts/benchmark/ann/run_ann_benchmark.sh
```

Artifacts land under a temp `output/` dir the script prints on exit:
`site/` (open `index.html`), `results.csv`, and `arangod.log`.

Cache datasets between runs so you don't re-download hundreds of MB:

```bash
export ANN_DATASET_CACHE="$HOME/.cache/ann-benchmarks-data"
```

## Knobs (environment variables)

| Variable | Default | Purpose |
|---|---|---|
| `ARANGODB_PACKAGE_URL` | auto-resolve | Explicit arangod tarball URL; overrides nightly resolution |
| `ARANGODB_NIGHTLY_INDEX` | devel/Linux/x86_64 nightly | Directory the tarball is resolved from |
| `ANN_FORK_URL` / `ANN_FORK_REF` | fork @ pinned SHA | ann-benchmarks source |
| `ANN_ALGORITHM` | `arangodb-ivf` | Config entry to run |
| `ANN_DATASETS` | `sift-128-euclidean glove-100-angular` | Space-separated dataset list |
| `ANN_RUNS` | `3` | Repeats per config (median reported) |
| `VECTOR_INDEX_FLAG` | `--experimental-vector-index=true` | Startup flag that unlocks the index |
| `ANN_DATASET_CACHE` | under workdir | Durable dataset cache dir |
| `ANN_OUTPUT_DIR` | under workdir | Where `site/` + `results.csv` are written |
| `PUSHGATEWAY_URL` | — | If set, metrics push runs (not yet implemented) |

## In CircleCI

Manual-only. Trigger a pipeline with the `run-ann-benchmark` parameter set to
`true` (UI "Trigger Pipeline", or the API). The `ann-benchmark` job runs on the
self-hosted `arangodb/large-amd64` runner and stores `site/` + `results.csv` as
build artifacts. It never runs in normal PR or nightly pipelines.

Notes:
- The runner is shared, so **QPS is a noisy, trend-only signal — recall is the
  reliable regression indicator.** The run holds the runner for a couple of
  hours; trigger it off-hours.
- No license or secret is required (see above).

## Metrics / history

No metrics database is provisioned yet, so results are published only as build
artifacts. `push_metrics` in the script is a stub gated on `PUSHGATEWAY_URL`;
when a Prometheus Pushgateway (or similar) exists, wire it there following the
`lib/iresearch/scripts/Prometheus/` precedent.
