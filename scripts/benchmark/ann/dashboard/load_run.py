#!/usr/bin/env python3
"""Load one ann-benchmarks run into ArangoDB (the Grafana source of truth).

Input is the CSV produced by ann-benchmarks `data_export.py`, which already
computes the metrics we want (k-nn recall, epsilon recall, p50/p95/p99 latency,
qps, build time). Each CSV row is one (dataset, algorithm, parameters) config.

The store is denormalized on purpose - each measurement document carries its run
+ dataset + index labels - so Grafana panels are single-collection AQL with no
joins. Documents use deterministic _key values, so re-loading the same run is
idempotent (overwrite).

Collections created if missing: runs, recall_qps, recall_lat, build_time.

Usage:
  python load_run.py --csv run.csv \
      --arangodb-version 4.0.0-dirty --docker-image arangodb/enterprise-preview:devel-nightly \
      --started-at 2026-08-28T10:00:00Z --ended-at 2026-08-28T10:42:00Z \
      --url http://localhost:8529 --db bench

  # inspect the CSV without a database:
  python load_run.py --csv run.csv --arangodb-version x --docker-image y --dry-run
"""

import argparse
import csv
import hashlib
import os
import re
import sys
from datetime import datetime, timezone

# CSV column -> meaning. Adjust here if your ann-benchmarks fork names columns
# differently (print the header with --dry-run to check).
COL_DATASET = "dataset"
COL_ALGORITHM = "algorithm"
COL_PARAMETERS = "parameters"
COL_KNN = "k-nn"
COL_QPS = "qps"
COL_BUILD = "build"
# metric column -> epsilon it is stored under. k-nn is exact recall (epsilon 0).
RECALL_QPS_EPSILONS = {"k-nn": 0.0, "epsilon": 0.01, "largeepsilon": 0.10}
# metric column -> percentile it is stored under.
RECALL_LAT_PERCENTILES = {"p50": 50.0, "p95": 95.0, "p99": 99.0}

COLLECTIONS = ("runs", "recall_qps", "recall_lat", "build_time")


def parse_dataset_name(name):
    """'glove-100-angular' -> (100, 'angular'). Unknown parts stay None."""
    dim = None
    m = re.search(r"-(\d+)-", name)
    if m:
        dim = int(m.group(1))
    metric = "angular" if name.endswith("angular") else \
             "euclidean" if name.endswith("euclidean") else None
    return dim, metric


def to_float(row, col):
    val = row.get(col, "")
    if val in (None, "", "nan"):
        return None
    try:
        return float(val)
    except ValueError:
        return None


def iso(ts):
    if ts is None:
        return datetime.now(timezone.utc).isoformat()
    return datetime.fromisoformat(ts.replace("Z", "+00:00")).isoformat()


def key(*parts):
    """Deterministic, arango-safe _key from arbitrary parts."""
    return hashlib.md5("\x1f".join(str(p) for p in parts).encode()).hexdigest()


def read_rows(path):
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        return reader.fieldnames, list(reader)


def parse_args():
    p = argparse.ArgumentParser(description="Load one ann-benchmarks run into ArangoDB.")
    p.add_argument("--csv", required=True, help="CSV from ann-benchmarks data_export.py")
    p.add_argument("--arangodb-version", required=True, help="version string (may include build-id)")
    p.add_argument("--build-id", default="", help="arangod ELF build-id (sha1)")
    p.add_argument("--docker-image", required=True)
    p.add_argument("--docker-image-id", default="", help="resolved docker image id/digest")
    p.add_argument("--started-at", help="ISO-8601; default: now")
    p.add_argument("--ended-at", help="ISO-8601; default: now")
    p.add_argument("--url", default=os.environ.get("ARANGO_STORE_URL", "http://localhost:8529"))
    p.add_argument("--db", default=os.environ.get("ARANGO_STORE_DB", "bench"))
    p.add_argument("--user", default=os.environ.get("ARANGO_STORE_USER", "root"))
    p.add_argument("--password", default=os.environ.get("ARANGO_STORE_PASSWORD", ""))
    p.add_argument("--dry-run", action="store_true",
                   help="parse and print a summary, touch no database")
    return p.parse_args()


def dry_run(fieldnames, rows):
    print(f"columns: {fieldnames}")
    print(f"configs (CSV rows): {len(rows)}")
    print(f"datasets: {sorted({r[COL_DATASET] for r in rows})}")
    print(f"algorithms: {sorted({r[COL_ALGORITHM] for r in rows})}")
    expected = (COL_KNN, COL_QPS, COL_BUILD, *RECALL_LAT_PERCENTILES)
    missing = [c for c in expected if fieldnames and c not in fieldnames]
    if missing:
        print(f"WARNING: expected columns not found: {missing}", file=sys.stderr)
    if rows:
        print("sample:", {k: rows[0].get(k) for k in
                          (COL_DATASET, COL_ALGORITHM, COL_KNN, COL_QPS, COL_BUILD)})


def get_db(args):
    from arango import ArangoClient
    client = ArangoClient(hosts=args.url)
    sys_db = client.db("_system", username=args.user, password=args.password)
    if not sys_db.has_database(args.db):
        sys_db.create_database(args.db)
    db = client.db(args.db, username=args.user, password=args.password)
    for name in COLLECTIONS:
        if not db.has_collection(name):
            db.create_collection(name)
    return db


def load(db, args, rows):
    started, ended = iso(args.started_at), iso(args.ended_at)
    # build-id distinguishes runs of different binaries at the same version.
    run_id = key(args.arangodb_version, args.build_id, args.docker_image, started)
    common = {
        "run_id": run_id,
        "arangodb_version": args.arangodb_version,
        "build_id": args.build_id,
        "docker_image": args.docker_image,
        "docker_image_id": args.docker_image_id,
        "started_at": started,
    }
    db.collection("runs").insert(
        {"_key": run_id, "ended_at": ended, **common}, overwrite=True)

    docs = {"recall_qps": [], "recall_lat": [], "build_time": []}
    for row in rows:
        ds = row[COL_DATASET]
        dim, metric = parse_dataset_name(ds)
        idx_type = row[COL_ALGORITHM]
        idx_params = row[COL_PARAMETERS]
        labels = {**common, "dataset": ds, "dimension": dim, "metric": metric,
                  "index_type": idx_type, "index_params": idx_params}
        idx_key = key(idx_type, idx_params)

        qps = to_float(row, COL_QPS)
        if qps is not None:
            for col, eps in RECALL_QPS_EPSILONS.items():
                recall = to_float(row, col)
                if recall is None:
                    continue
                docs["recall_qps"].append({
                    "_key": key(run_id, ds, idx_key, "qps", eps),
                    "epsilon": eps, "recall": recall, "qps": qps, **labels})

        knn = to_float(row, COL_KNN)
        if knn is not None:
            for col, pct in RECALL_LAT_PERCENTILES.items():
                lat = to_float(row, col)
                if lat is None:
                    continue
                docs["recall_lat"].append({
                    "_key": key(run_id, ds, idx_key, "lat", pct),
                    "percentile": pct, "recall": knn, "latency": lat, **labels})

        build = to_float(row, COL_BUILD)
        if build is not None:
            docs["build_time"].append({
                "_key": key(run_id, ds, idx_key), "build_time": build, **labels})

    for name, batch in docs.items():
        if batch:
            db.collection(name).insert_many(batch, overwrite=True)
    print(f"run {run_id}: "
          f"{len(docs['recall_qps'])} recall/qps, "
          f"{len(docs['recall_lat'])} recall/lat, "
          f"{len(docs['build_time'])} build-time docs")


def main():
    args = parse_args()
    fieldnames, rows = read_rows(args.csv)
    if not rows:
        print("no rows in CSV", file=sys.stderr)
        return 1
    if args.dry_run:
        dry_run(fieldnames, rows)
        return 0
    try:
        import arango  # noqa: F401
    except ImportError:
        print("python-arango not installed (pip install python-arango), or use --dry-run",
              file=sys.stderr)
        return 1
    load(get_db(args), args, rows)
    return 0


if __name__ == "__main__":
    sys.exit(main())
