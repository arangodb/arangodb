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

        //  Same 10-point 2D dataset as testLinearVsTrainedSameResultsAllApproxNearFunctions.
        //  nLists=3: two shards get 4 vectors each (trained); one shard gets 2 (< nLists,
        //  unusable, linear scan). cAllReady adds far-away fillers only on the starved
        //  shard so every shard has enough vectors to train; top-k stays identical with
        //  nProbe=nLists=3.
        testClusterMixedShardIvfPlusLinearMatchesAllReadyIvf: function () {
            if (!isCluster) {
                return;
            }

            const clusterNlists = 3;
            const clusterDim = 2;
            const docsBelow = clusterNlists - 1;
            const nFull = 4;
            const keysNeeded = nFull;

            //  This collection will have 1 shard in unusable state
            const collOneShardUnusable = "vecOneShardUnusable";

            //  This collection will have all shards trained and in ready state
            const collAllReady = "vecAllShardsReady";

            const indexHint = "vec_l2";
            const nProbeExhaustive = clusterNlists;

            const idxDef = {
                name: indexHint,
                type: "vector",
                fields: ["vector"],
                inBackground: true,
                params: {
                    metric: "l2",
                    dimension: clusterDim,
                    nLists: clusterNlists,
                    defaultNProbe: 1,
                    trainingIterations: 25,
                },
            };

            const cOneShardUnusable = db._create(collOneShardUnusable, {numberOfShards: 3});
            const cAllReady = db._create(collAllReady, {numberOfShards: 3});

            const shardNamesForOneUnusable = Object.keys(cOneShardUnusable.shards(true)).sort();
            assertEqual(3, shardNamesForOneUnusable.length);

            const poolOne = buildKeyPool(cOneShardUnusable, keysNeeded);
            const poolAll = buildKeyPool(cAllReady, keysNeeded);

            //  Shard names for vecOneShardUnusable collection
            const starvedNameMixed = shardNamesForOneUnusable[0];
            const full1Mixed = shardNamesForOneUnusable[1];
            const full2Mixed = shardNamesForOneUnusable[2];

            function getShardOfKey(coll, key) {
                return db._query(
                    `RETURN SHARD_ID(@c, {_key: @k})`,
                    {c: coll.name(), k: key}).toArray()[0];
            }

            //  Shard names for vecAllShardsReady collection
            const starvedNameAll = getShardOfKey(cAllReady, poolOne.keysPerShard[starvedNameMixed][0]);
            const full1All = getShardOfKey(cAllReady, poolOne.keysPerShard[full1Mixed][0]);
            const full2All = getShardOfKey(cAllReady, poolOne.keysPerShard[full2Mixed][0]);

            const vs = VECTORS_2D_DATASET;
            const vectorsStarved = vs.slice(0, docsBelow);
            const vectorsFull1 = vs.slice(docsBelow, docsBelow + nFull);
            const vectorsFull2 = vs.slice(docsBelow + nFull, docsBelow + 2 * nFull);
            assertEqual(10, vectorsStarved.length + vectorsFull1.length + vectorsFull2.length);

            insertDocsForShard(
                cOneShardUnusable, poolOne.keysPerShard[starvedNameMixed], docsBelow,
                vectorsStarved);
            insertDocsForShard(
                cOneShardUnusable, poolOne.keysPerShard[full1Mixed], nFull,
                vectorsFull1);
            insertDocsForShard(
                cOneShardUnusable, poolOne.keysPerShard[full2Mixed], nFull,
                vectorsFull2);

            const keysStarvedAll = poolAll.keysPerShard[starvedNameAll];
            insertDocsForShard(
                cAllReady, keysStarvedAll, docsBelow,
                vectorsStarved);

            //  In vecAllShardsReady collection the shards are filled with
            //  2, 4 and 4 vectors, same as the other collection. This will
            //  prevent the shard from getting trained and enter into ready state.
            //  That is why we fill the unusable shard with 2 extra vectors that
            //  are far away from the search vector ensuring that they never
            //  show up in the search results.
            const filler = [];
            for (let i = docsBelow; i < keysNeeded; ++i) {
                filler.push(farVectorFrom(SEARCH_VECTOR_2D));
            }
            insertDocsForShard(
                cAllReady, keysStarvedAll.slice(docsBelow),
                keysNeeded - docsBelow,
                filler);
            insertDocsForShard(
                cAllReady, poolAll.keysPerShard[full1All], nFull,
                vectorsFull1);
            insertDocsForShard(
                cAllReady, poolAll.keysPerShard[full2All], nFull,
                vectorsFull2);

            cOneShardUnusable.ensureIndex(idxDef);
            cAllReady.ensureIndex(idxDef);

            const expectationsMixed = {};
            expectationsMixed[full1Mixed] = {trainingState: VectorIndexTrainingState.kReady, hasError: false};
            expectationsMixed[full2Mixed] = {trainingState: VectorIndexTrainingState.kReady, hasError: false};
            expectationsMixed[starvedNameMixed] = {
                trainingState: VectorIndexTrainingState.kUnusable,
                hasError: false,
            };
            assertTrue(
                waitForPerShardStates(cOneShardUnusable, indexHint, expectationsMixed, 120),
                "mixed: two shards ready, starved shard unusable");

            const mixedStates = getPerShardStates(cOneShardUnusable, indexHint);
            assertEqual(VectorIndexTrainingState.kUnusable,
                mixedStates[starvedNameMixed].trainingState);
            assertTrue(mixedStates[starvedNameMixed].error.length > 0);

            assertTrue(
                waitForVectorIndexState(cAllReady, indexHint, VectorIndexTrainingState.kReady, 120),
                "all-shard baseline index should become ready");

            const docTotalMixed = docsBelow + 2 * nFull;
            const lim = 5;
            assertTrue(lim <= docTotalMixed, "limit should not exceed mixed collection size");
            const bind = {
                qp: SEARCH_VECTOR_2D,
                lim,
                nProbe: nProbeExhaustive,
            };

            const keysMixed = vectorTopKeys(collOneShardUnusable, indexHint, bind);
            const keysAll = vectorTopKeys(collAllReady, indexHint, bind);

            assertEqual(lim, keysMixed.length);
            assertEqual(lim, keysAll.length);
            assertEqual(keysMixed, keysAll,
                "mixed (IVF + linear on starved shard) vs all-ready IVF, nProbe=nLists");
        },

    };
}

jsunity.run(VectorIndexUnusableShardLinearScanSuite);

return jsunity.done();
