#!/usr/bin/env python3
"""Load one ann-benchmarks run into ArangoDB (schema.js).

Input is the CSV produced by ann-benchmarks `data_export.py`, which already
computes the metrics we want (k-nn recall, epsilon recall, p50/p95/p99 latency,
qps, build time) using ann-benchmarks' own metric functions. Each CSV row is one
(dataset, algorithm, parameters) configuration; we fan it out into the three
measurement collections.

Everything is written in a single stream transaction = one `run`; on any error the
transaction is aborted and nothing is left behind. Every invocation creates a new
`run` document with its own copy of the measurements; datasets and index setups are
shared across runs (UPSERT). Build time is stored once per distinct (dataset, value)
within a run, because ann-benchmarks repeats it on every query-parameter row.

Standard library only; talks to ArangoDB over its HTTP API.

Usage:
  ./load_run.py --csv run.csv \
      --arangodb-version 3.12.12-devel --docker-image arangodb:3.12 \
      --started-at 2026-08-28T10:00:00Z --ended-at 2026-08-28T10:42:00Z \
      --url http://localhost:8529

  # inspect without a database:
  ./load_run.py --csv run.csv --arangodb-version x --docker-image y --dry-run
"""

import argparse
import base64
import csv
import json
import os
import re
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone

# CSV column -> meaning. Adjust here if your ann-benchmarks fork names columns
# differently (print the header with --dry-run to check).
COL_DATASET = "dataset"
COL_ALGORITHM = "algorithm"
COL_PARAMETERS = "parameters"

# metric column -> (epsilon value we store it under). k-nn is exact (epsilon 0).
RECALL_QPS_EPSILONS = {"k-nn": 0.0, "epsilon": 0.01, "largeepsilon": 0.10}
# metric column -> (percentile we store it under). Recall is the exact k-nn recall.
RECALL_LAT_PERCENTILES = {"p50": 50.0, "p95": 95.0, "p99": 99.0}
COL_KNN = "k-nn"
COL_QPS = "qps"
COL_BUILD = "build"

WRITE_COLLECTIONS = [
    "run", "dataset", "index_setup",
    "measurement_recall_lat", "measurement_recall_qps", "measurement_build_time",
]


class ArangoError(RuntimeError):
    pass


class Arango:
    """Minimal ArangoDB HTTP client: AQL cursors inside one stream transaction."""

    def __init__(self, url, database, user, password):
        self.base = f"{url.rstrip('/')}/_db/{database}"
        self.headers = {"Content-Type": "application/json"}
        if password:
            token = base64.b64encode(f"{user}:{password}".encode()).decode()
            self.headers["Authorization"] = f"Basic {token}"
        self.trx_id = None

    def _request(self, method, path, body=None):
        data = json.dumps(body).encode() if body is not None else None
        req = urllib.request.Request(self.base + path, data=data, method=method,
                                     headers=self.headers)
        if self.trx_id:
            req.add_header("x-arango-trx-id", self.trx_id)
        try:
            with urllib.request.urlopen(req) as resp:
                return json.loads(resp.read() or b"{}")
        except urllib.error.HTTPError as e:
            try:
                msg = json.loads(e.read()).get("errorMessage", e.reason)
            except ValueError:
                msg = e.reason
            raise ArangoError(f"{method} {path} -> HTTP {e.code}: {msg}") from None
        except urllib.error.URLError as e:
            raise ArangoError(f"{method} {path}: {e.reason}") from None

    def begin(self, write_collections):
        res = self._request("POST", "/_api/transaction/begin",
                            {"collections": {"write": write_collections}})
        self.trx_id = res["result"]["id"]

    def commit(self):
        self._request("PUT", f"/_api/transaction/{self.trx_id}")
        self.trx_id = None

    def abort(self):
        if self.trx_id:
            trx, self.trx_id = self.trx_id, None
            self._request("DELETE", f"/_api/transaction/{trx}")

    def aql(self, query, bind_vars=None):
        res = self._request("POST", "/_api/cursor",
                            {"query": query, "bindVars": bind_vars or {}})
        rows = list(res["result"])
        while res.get("hasMore"):
            res = self._request("PUT", f"/_api/cursor/{res['id']}")
            rows.extend(res["result"])
        return rows

    def one(self, query, bind_vars=None):
        return self.aql(query, bind_vars)[0]


def parse_dataset_name(name):
    """Best-effort dimension/metric from an ann-benchmarks dataset name, e.g.
    'glove-100-angular' -> (100, 'angular'). Unknown parts stay None."""
    dimension = None
    metric = None
    m = re.search(r"-(\d+)-", name)
    if m:
        dimension = int(m.group(1))
    if name.endswith("angular"):
        metric = "angular"
    elif name.endswith("euclidean"):
        metric = "euclidean"
    return dimension, metric


def to_float(row, col):
    val = row.get(col, "")
    if val is None or val == "" or val == "nan":
        return None
    try:
        return float(val)
    except ValueError:
        return None


def parse_args():
    p = argparse.ArgumentParser(description="Load one ann-benchmarks run into ArangoDB.")
    p.add_argument("--csv", required=True, help="CSV from ann-benchmarks data_export.py")
    p.add_argument("--arangodb-version", required=True)
    p.add_argument("--docker-image", required=True)
    p.add_argument("--started-at", help="ISO-8601; default: now")
    p.add_argument("--ended-at", help="ISO-8601; default: now")
    p.add_argument("--url", default=os.environ.get("ARANGO_URL", "http://localhost:8529"),
                   help="ArangoDB endpoint (env ARANGO_URL)")
    p.add_argument("--db", default="bench", help="database name")
    p.add_argument("--user", default="root")
    p.add_argument("--password", default=os.environ.get("ARANGO_PASSWORD", ""),
                   help="empty = no authentication (env ARANGO_PASSWORD)")
    p.add_argument("--dry-run", action="store_true",
                   help="parse and print a summary, touch no database")
    return p.parse_args()


def read_rows(path):
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        return reader.fieldnames, list(reader)


def validate(fieldnames, rows):
    """Reject CSVs the loader cannot map before touching the database."""
    required = (COL_DATASET, COL_ALGORITHM, COL_PARAMETERS)
    missing = [c for c in required if c not in (fieldnames or [])]
    if missing:
        raise ValueError(f"CSV lacks required columns {missing}; header is {fieldnames}")
    for n, row in enumerate(rows, start=2):
        empty = [c for c in required if not row.get(c)]
        if empty:
            raise ValueError(f"CSV line {n}: empty value for {empty}")


def iso(ts):
    if ts is None:
        return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    return datetime.fromisoformat(ts.replace("Z", "+00:00")).astimezone(timezone.utc) \
        .strftime("%Y-%m-%dT%H:%M:%SZ")


def load(db, args, rows):
    run_id = db.one(
        "INSERT {arangodb_version: @version, docker_image_version: @image, "
        "started_at: @started, ended_at: @ended} INTO run RETURN NEW._key",
        {"version": args.arangodb_version, "image": args.docker_image,
         "started": iso(args.started_at), "ended": iso(args.ended_at)},
    )

    dataset_ids = {}
    index_ids = {}

    def dataset_id(name):
        if name not in dataset_ids:
            dim, metric = parse_dataset_name(name)
            dataset_ids[name] = db.one(
                "UPSERT {name: @name} "
                "INSERT {name: @name, dimension: @dim, metric: @metric} UPDATE {} "
                "IN dataset RETURN NEW._key",
                {"name": name, "dim": dim, "metric": metric},
            )
        return dataset_ids[name]

    def index_id(index_type, params):
        key = (index_type, params)
        if key not in index_ids:
            index_ids[key] = db.one(
                "UPSERT {index_type: @type, index_params: {descriptor: @params}} "
                "INSERT {index_type: @type, index_params: {descriptor: @params}} UPDATE {} "
                "IN index_setup RETURN NEW._key",
                {"type": index_type, "params": params},
            )
        return index_ids[key]

    def upsert_measurement(collection, key, values):
        # Equivalent of SQL "ON CONFLICT DO NOTHING": 1 if inserted, 0 if it existed.
        return db.one(
            f"UPSERT @key INSERT MERGE(@key, @values) UPDATE {{}} IN {collection} "
            "RETURN OLD ? 0 : 1",
            {"key": key, "values": values},
        )

    # ann-benchmarks repeats one index's build time on every query-parameter row
    # (e.g. each nProbe), so store each distinct (dataset, build time) once per run.
    seen_builds = set()

    n_lat = n_qps = n_build = 0
    for row in rows:
        ref = {"run_id": run_id,
               "index_id": index_id(row[COL_ALGORITHM], row[COL_PARAMETERS]),
               "dataset_id": dataset_id(row[COL_DATASET])}

        knn = to_float(row, COL_KNN)
        qps = to_float(row, COL_QPS)

        for col, pct in RECALL_LAT_PERCENTILES.items():
            lat = to_float(row, col)
            if lat is None or knn is None:
                continue
            n_lat += upsert_measurement("measurement_recall_lat",
                                        {**ref, "percentile": pct},
                                        {"recall": knn, "latency": lat})

        if qps is not None:
            for col, eps in RECALL_QPS_EPSILONS.items():
                recall = to_float(row, col)
                if recall is None:
                    continue
                n_qps += upsert_measurement("measurement_recall_qps",
                                            {**ref, "epsilon": eps},
                                            {"qps": qps, "recall": recall})

        build = to_float(row, COL_BUILD)
        if build is not None and (ref["dataset_id"], build) not in seen_builds:
            seen_builds.add((ref["dataset_id"], build))
            n_build += upsert_measurement("measurement_build_time", ref,
                                          {"build_time": build})

    print(f"run _key={run_id}: {len(index_ids)} index setups, {len(dataset_ids)} datasets, "
          f"{n_lat} recall/lat, {n_qps} recall/qps, {n_build} build-time docs")


def dry_run(fieldnames, rows):
    print(f"columns: {fieldnames}")
    print(f"configs (CSV rows): {len(rows)}")
    datasets = sorted({r[COL_DATASET] for r in rows})
    algos = sorted({r[COL_ALGORITHM] for r in rows})
    print(f"datasets: {datasets}")
    print(f"algorithms: {algos}")
    missing = [c for c in (COL_KNN, COL_QPS, COL_BUILD, *RECALL_LAT_PERCENTILES)
               if fieldnames and c not in fieldnames]
    if missing:
        print(f"WARNING: expected columns not found: {missing}", file=sys.stderr)
    if rows:
        print("sample row:", {k: rows[0].get(k) for k in
                              (COL_DATASET, COL_ALGORITHM, COL_KNN, COL_QPS, COL_BUILD)})


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
        validate(fieldnames, rows)
    except ValueError as e:
        print(f"invalid CSV: {e}", file=sys.stderr)
        return 1

    db = Arango(args.url, args.db, args.user, args.password)
    try:
        db.begin(WRITE_COLLECTIONS)
        load(db, args, rows)
        db.commit()
    except Exception as e:
        db.abort()
        print(f"aborted, nothing written: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
