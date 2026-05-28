/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertFalse, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Koushal Kawade
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = internal.db;
const isCluster = internal.isCluster();

const {
    VectorIndexTrainingState,
    waitForVectorIndexState,
    buildKeyPool
} = require("@arangodb/testutils/vector-index-common");

const dbName = "vectorUnusableShardLinearScanDB";

//  Shared 2D dataset (10 points) for single-server linear-vs-IVF test and for
//  cluster mixed-shard vs all-ready comparison.
const VECTORS_2D_DATASET = [
    [1.1, 0.1],
    [1.0, 0.0],
    [0.9, -0.1],
    [0.5, 0.5],
    [0.0, 1.0],
    [-0.2, 0.8],
    [2.0, -0.5],
    [0.3, -0.9],
    [-1.0, 0.0],
    [0.7, 0.7],
];
const SEARCH_VECTOR_2D = [1.2, 0.2];

function insertDocsForShard(collection, keys, count, vectors) {
    assertTrue(keys.length >= count,
        "Not enough keys for shard, need " + count + " have " + keys.length);
    const batchSize = 500;
    for (let i = 0; i < count; i += batchSize) {
        const size = Math.min(batchSize, count - i);
        const docs = [];
        for (let j = 0; j < size; ++j) {
            docs.push({_key: keys[i + j], vector: vectors[i + j]});
        }
        collection.insert(docs);
    }
}

function getPerShardStates(collection, indexName) {
    const idx = collection.indexes(true, true).find(i => i.name === indexName);
    if (!idx || !idx.shards) {
        return null;
    }
    return idx.shards;
}

function waitForPerShardStates(collection, indexName, expectations, timeoutSec) {
    const sleepInterval = 0.5;
    const iterations = Math.floor(timeoutSec / sleepInterval);

    for (let iter = 0; iter < iterations; ++iter) {
        const shardStates = getPerShardStates(collection, indexName);
        assertNotEqual(shardStates, null);
        let allMatch = true;
        for (const [shard, expected] of Object.entries(expectations)) {
            const actual = shardStates[shard];
            if (actual.trainingState !== expected.trainingState) {
                allMatch = false;
                break;
            }
            if (expected.hasError && actual.error.length === 0) {
                allMatch = false;
                break;
            }
        }
        if (allMatch) {
            return true;
        }
        internal.sleep(sleepInterval);
    }
    return false;
}

function vectorTopKeys(collectionName, indexHint, bind) {
    const sortExpr = "APPROX_NEAR_L2(d.vector, @qp, {nProbe: @nProbe}) ASC";
    const q = "FOR d IN " + collectionName +
        " OPTIONS { indexHint: \"" + indexHint + "\" } " +
        "SORT " + sortExpr +
        " LIMIT @lim RETURN d._key";
    return db._query(q, bind).toArray();
}

//  Vectors placed very far from `queryPoint` so they never enter a modest top-k.
function farVectorFrom(queryPoint) {
    const v = [];
    for (let i = 0; i < queryPoint.length; ++i) {
        v.push(queryPoint[i] + 1.0e6);
    }
    return v;
}

function VectorIndexUnusableShardLinearScanSuite() {
    return {
        setUp: function () {
            db._useDatabase("_system");
            db._createDatabase(dbName);
            db._useDatabase(dbName);
        },

        tearDown: function () {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        //  Ten 2D vectors on a single-shard collection:
        //  When nLists=10 the shards train,
        //  when nLists=20 they stay unusable (linear scan).
        //  With a high value of nProbe on the trained IVF index,
        //  APPROX_NEAR_* results must match between the two indexes.
        testLinearVsTrainedSameResultsAllApproxNearFunctions: function () {
            if (isCluster)
                return;

            const collNameCompare = "vecLinearVsTrained";
            const nListsSmall = 10;
            const nListsLarge = 20;
            const c = db._create(collNameCompare, {numberOfShards: 1});

            let k = 0;
            for (const vec of VECTORS_2D_DATASET) {
                c.insert({_key: "d" + (k++), vector: vec});
            }

            const vectorSearchCases = [
                {
                    metric: "l2",
                    label: "L2",
                    sortExpr: "APPROX_NEAR_L2(d.vector, @qp, {nProbe: @nProbe}) ASC",
                },
                {
                    metric: "cosine",
                    label: "COSINE",
                    sortExpr: "APPROX_NEAR_COSINE(d.vector, @qp, {nProbe: @nProbe}) DESC",
                },
                {
                    metric: "innerProduct",
                    label: "INNER_PRODUCT",
                    sortExpr: "APPROX_NEAR_INNER_PRODUCT(d.vector, @qp, {nProbe: @nProbe}) DESC",
                },
            ];

            for (const row of vectorSearchCases) {
                //  index with small nLists value
                //  training will succeed for this index
                c.ensureIndex({
                    name: "vec_" + row.label + "_n" + nListsSmall,
                    type: "vector",
                    fields: ["vector"],
                    inBackground: true,
                    params: {
                        metric: row.metric,
                        dimension: 2,
                        nLists: nListsSmall,
                        defaultNProbe: 1,
                        trainingIterations: 25,
                    },
                });

                //  index with a large nLists value
                //  training will fail and the index will be marked as unusable
                c.ensureIndex({
                    name: "vec_" + row.label + "_n" + nListsLarge,
                    type: "vector",
                    fields: ["vector"],
                    inBackground: true,
                    params: {
                        metric: row.metric,
                        dimension: 2,
                        nLists: nListsLarge,
                        defaultNProbe: 1,
                        trainingIterations: 25,
                    },
                });
            }

            const lim = 5;
            const nProbeExhaustive = nListsSmall;
            const bindBase = {qp: SEARCH_VECTOR_2D, lim: lim, nProbe: nProbeExhaustive};

            for (const row of vectorSearchCases) {
                const hintSmall = "vec_" + row.label + "_n" + nListsSmall;
                const hintLarge = "vec_" + row.label + "_n" + nListsLarge;

                assertTrue(
                    waitForVectorIndexState(
                        c, hintSmall, VectorIndexTrainingState.kReady, 120),
                    "nLists=" + nListsSmall + " index " + hintSmall + " should become ready");

                assertTrue(
                    waitForVectorIndexState(
                        c, hintLarge, VectorIndexTrainingState.kUnusable, 120),
                    "nLists=" + nListsLarge + " index " + hintLarge +
                    " should become unusable (not enough training vectors)");

                const q = "FOR d IN " + collNameCompare +
                    " OPTIONS { indexHint: \"" + hintSmall + "\" } " +
                    "SORT " + row.sortExpr +
                    " LIMIT @lim RETURN d._key";
                const keysSmall = db._query(q, bindBase).toArray();

                const qLarge = "FOR d IN " + collNameCompare +
                    " OPTIONS { indexHint: \"" + hintLarge + "\" } " +
                    "SORT " + row.sortExpr +
                    " LIMIT @lim RETURN d._key";
                const keysLarge = db._query(qLarge, bindBase).toArray();

                assertEqual(
                    keysSmall, keysLarge,
                    row.label + ": trained IVF (exhaustive nProbe) vs linear scan top-k _key order");
            }

            db._drop(collNameCompare);
        },

    };
}

jsunity.run(VectorIndexUnusableShardLinearScanSuite);

return jsunity.done();
