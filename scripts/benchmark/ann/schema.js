// ann-benchmarks results schema (ArangoDB). Applied through arangosh by the `schema`
// compose service on every `up`; safe to re-run, every step is guarded.
// Units: latency = milliseconds, build_time = seconds, qps = queries/second.
//
// Measurements reference run / index_setup / dataset by _key. "Which index setups
// ran in which run" is derivable from the measurements, so there is no join collection.

const DB_NAME = "bench";

const COLLECTIONS = {
  run: {
    unique: [],
    plain: [["started_at"]],
  },
  dataset: {
    unique: [["name"]],
    plain: [],
  },
  index_setup: {
    unique: [["index_type", "index_params.descriptor"]],
    plain: [],
  },
  measurement_recall_lat: {
    unique: [["run_id", "index_id", "dataset_id", "percentile"]],
    plain: [["run_id"]],
  },
  measurement_recall_qps: {
    unique: [["run_id", "index_id", "dataset_id", "epsilon"]],
    plain: [["run_id"]],
  },
  measurement_build_time: {
    unique: [["run_id", "index_id", "dataset_id"]],
    plain: [["run_id"]],
  },
};

if (!db._databases().includes(DB_NAME)) {
  db._createDatabase(DB_NAME);
}
db._useDatabase(DB_NAME);

for (const [name, spec] of Object.entries(COLLECTIONS)) {
  const coll = db._collection(name) || db._create(name);
  for (const fields of spec.unique) {
    coll.ensureIndex({ type: "persistent", fields, unique: true });
  }
  for (const fields of spec.plain) {
    coll.ensureIndex({ type: "persistent", fields });
  }
}

print(`schema ready in database '${DB_NAME}': ${Object.keys(COLLECTIONS).join(", ")}`);
