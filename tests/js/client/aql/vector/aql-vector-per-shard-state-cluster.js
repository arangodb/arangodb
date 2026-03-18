/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertFalse, assertNotEqual, print */

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
/// @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = internal.db;
const {
    randomNumberGeneratorFloat,
    randomInteger,
} = require("@arangodb/testutils/seededRandom");
const dbName = "vectorPerShardStateDB";
const dimension = 100;
const nLists = 1;
// Training threshold is max(nLists * 39, 1000) = 1000 with nLists=1.
// Use 1200 per shard to comfortably exceed the threshold.
const docsAboveThreshold = 1200;
const docsBelowThreshold = 500;

////////////////////////////////////////////////////////////////////////////////
/// Helpers
////////////////////////////////////////////////////////////////////////////////

function generateVector(gen) {
    return Array.from({length: dimension}, () => gen());
}

/// Pre-computes a pool of keys grouped by shard using SHARD_ID() in a single
/// AQL query, similar to the approach in shell-cluster-dbserver-shard-metrics.
/// Returns {shardNames: [...], keysPerShard: {shardName: [key, ...], ...}}.
function buildKeyPool(collection, keysNeeded) {
    const shardNames = Object.keys(collection.shards(true));
    const keysPerShard = {};
    for (const s of shardNames) {
        keysPerShard[s] = [];
    }

    const batchSize = 1000;
    let keyIndex = 0;
    while (Object.values(keysPerShard).some(keys => keys.length < keysNeeded)) {
        const results = db._query(`
            FOR i IN @from..@to
              LET key = CONCAT('k', i)
              RETURN {key, shard: SHARD_ID(@coll, {_key: key})}
        `, {from: keyIndex, to: keyIndex + batchSize - 1, coll: collection.name()}).toArray();

        for (const {key, shard} of results) {
            if (keysPerShard[shard] && keysPerShard[shard].length < keysNeeded) {
                keysPerShard[shard].push(key);
            }
        }
        keyIndex += batchSize;
    }

    return {shardNames, keysPerShard};
}

/// Inserts `count` docs with valid vectors using pre-computed keys for a
/// specific shard.
function insertDocsForShard(collection, keys, count, gen) {
    assertTrue(keys.length >= count,
        "Not enough keys for shard, need " + count + " have " + keys.length);
    const batchSize = 500;
    for (let i = 0; i < count; i += batchSize) {
        const size = Math.min(batchSize, count - i);
        const docs = [];
        for (let j = 0; j < size; ++j) {
            docs.push({_key: keys[i + j], vector: generateVector(gen)});
        }
        collection.insert(docs);
    }
}

/// Queries per-shard state for a vector index from the coordinator response.
/// Returns {shardName: {state: "...", error: "..."}, ...} or null.
function getPerShardStates(collection, indexName) {
    const idx = collection.indexes(true, true).find(i => i.name === indexName);
    if (!idx || !idx.shards) {
        return null;
    }
    return idx.shards;
}

/// Waits until per-shard vector index states match expectations.
/// `expectations` is {shardName: {state: "ready"}, ...}.
/// A shard matches if its `state` equals the expected state.
/// If `expectError` is provided, also checks that the error field is non-empty.
/// Returns true on success, false on timeout.
function waitForPerShardStates(collection, indexName, expectations, timeoutSec) {
    const sleepInterval = 0.5;
    const iterations = Math.floor(timeoutSec / sleepInterval);

    for (let iter = 0; iter < iterations; ++iter) {
        const shardStates = getPerShardStates(collection, indexName);
        assertNotEqual(shardStates, null);
        let allMatch = true;
        for (const [shard, expected] of Object.entries(expectations)) {
            const actual = shardStates[shard];
            print(`Acutal state: ${JSON.stringify(actual)}, expected: ${JSON.stringify(expected)}`);
            if(actual.state !== expected.state) {
                allMatch = false;
                break;
            }
            if(expected.hasError && actual.error.length === 0){
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

/// Asserts per-shard document counts.
/// `expectedCounts` is {shardName: expectedCount, ...}.
function assertPerShardCounts(collection, expectedCounts) {
    const counts = collection.count(true);
    for (const [shard, expected] of Object.entries(expectedCounts)) {
        assertEqual(expected, counts[shard],
            "Shard " + shard + " should have " + expected +
            " docs, got " + counts[shard]);
    }
}

/// Creates the vector index on the collection.
function createVectorIndex(collection, sparse) {
    collection.ensureIndex({
        name: "vec_l2",
        type: "vector",
        fields: ["vector"],
        inBackground: false,
        sparse: sparse || false,
        params: {metric: "l2", dimension, nLists},
    });
}

////////////////////////////////////////////////////////////////////////////////
/// Test suite — cluster only
////////////////////////////////////////////////////////////////////////////////

function VectorIndexPerShardStateSuite() {
    const seed = randomInteger();
    const collName = "vectorColl";
    let gen;
    let collection;

    return {
        setUp: function () {
            gen = randomNumberGeneratorFloat(seed);
            print("Using seed: " + seed);
            db._useDatabase("_system");
            db._createDatabase(dbName);
            db._useDatabase(dbName);
            collection = db._create(collName, {numberOfShards: 3});
        },

        tearDown: function () {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        // Scenario 5: Happy path — all 3 shards have enough valid data.
        // All shards should reach "ready" with no errors.
        testAllShardsReachReady: function () {
            const {shardNames, keysPerShard} = buildKeyPool(collection, docsAboveThreshold);
            assertEqual(3, shardNames.length);

            for (const shard of shardNames) {
                insertDocsForShard(collection, keysPerShard[shard], docsAboveThreshold, gen);
            }

            const expectedCounts = {};
            for (const s of shardNames) {
                expectedCounts[s] = docsAboveThreshold;
            }
            assertPerShardCounts(collection, expectedCounts);

            createVectorIndex(collection);

            const expectations = {};
            for (const s of shardNames) {
                expectations[s] = {state: "ready", hasError: false};
            }

            assertTrue(
                waitForPerShardStates(collection, "vec_l2", expectations, 120),
                "All shards should reach 'ready' state"
            );

            const shardStates = getPerShardStates(collection, "vec_l2");
            for (const s of shardNames) {
                assertEqual("ready", shardStates[s].state,
                    "Shard " + s + " should be ready");
                assertEqual("", shardStates[s].error,
                    "Shard " + s + " should have no error");
            }
        },

        // Scenario 1: 3 shards, 2 have enough data, 1 does not.
        // The 2 full shards should become "ready"; the starved shard stays
        // "unusable" with no error.
        testOneShardStarvedRemainsUntrained: function () {
            const {shardNames, keysPerShard} = buildKeyPool(collection, docsAboveThreshold);
            assertEqual(3, shardNames.length);

            // Fill 2 shards above threshold, 1 shard below threshold.
            const starvedShard = shardNames[0];
            const fullShards = shardNames.slice(1);
            insertDocsForShard(collection, keysPerShard[starvedShard], docsBelowThreshold, gen);
            for (const shard of fullShards) {
                insertDocsForShard(collection, keysPerShard[shard], docsAboveThreshold, gen);
            }

            const expectedCounts = {[starvedShard]: docsBelowThreshold};
            for (const s of fullShards) {
                expectedCounts[s] = docsAboveThreshold;
            }
            assertPerShardCounts(collection, expectedCounts);

            createVectorIndex(collection);

            const expectations = {};
            for (const s of fullShards) {
                expectations[s] = {state: "ready", hasError: false};
            }
            expectations[starvedShard] = {state: "unusable", hasError: false};

            assertTrue(
                waitForPerShardStates(collection, "vec_l2", expectations, 120),
                "2 shards should be ready, starved shard should be unusable"
            );

            const shardStates = getPerShardStates(collection, "vec_l2");
            assertEqual("unusable", shardStates[starvedShard].state,
                "Starved shard should remain unusable");
            assertEqual("", shardStates[starvedShard].error,
                "Starved shard should have no error");
            for (const s of fullShards) {
                assertEqual("ready", shardStates[s].state,
                    "Full shard " + s + " should be ready");
                assertEqual("", shardStates[s].error,
                    "Full shard " + s + " should have no error");
            }
        },

        // Scenario 2: All 3 shards have enough data, but one shard has a
        // document with wrong vector dimension. That shard should fail with
        // an error; the other 2 should become "ready".
        testWrongDimensionFailsShard: function () {
            const {shardNames, keysPerShard} = buildKeyPool(collection, docsAboveThreshold + 1);
            assertEqual(3, shardNames.length);

            const badShard = shardNames[0];
            const goodShards = shardNames.slice(1);

            // Fill all shards with valid docs.
            for (const shard of shardNames) {
                insertDocsForShard(collection, keysPerShard[shard], docsAboveThreshold, gen);
            }

            // Insert a document with wrong dimension into the bad shard.
            const badKey = keysPerShard[badShard][docsAboveThreshold];
            const wrongDimVector = Array.from({length: dimension / 2}, () => gen());
            collection.insert({_key: badKey, vector: wrongDimVector});

            const expectedCounts = {[badShard]: docsAboveThreshold + 1};
            for (const s of goodShards) {
                expectedCounts[s] = docsAboveThreshold;
            }
            assertPerShardCounts(collection, expectedCounts);

            createVectorIndex(collection);

            const expectations = {};
            for (const s of goodShards) {
                expectations[s] = {state: "ready", hasError: false};
            }
            expectations[badShard] = {state: "unusable", hasError: true};

            assertTrue(
                waitForPerShardStates(collection, "vec_l2", expectations, 120),
                "Good shards should be ready, bad shard should have error"
            );

            const shardStates = getPerShardStates(collection, "vec_l2");
            assertEqual("unusable", shardStates[badShard].state,
                "Shard with wrong dimension should be unusable");
            assertTrue(shardStates[badShard].error.length > 0,
                "Shard with wrong dimension should have an error message");
            assertTrue(
                shardStates[badShard].error.indexOf("dimension") !== -1,
                "Error should mention 'dimension', got: " +
                shardStates[badShard].error
            );
            for (const s of goodShards) {
                assertEqual("ready", shardStates[s].state);
                assertEqual("", shardStates[s].error);
            }
        },

        // Scenario 3: All 3 shards have enough data, but one shard has a
        // document with wrong vector datatype (strings instead of numbers).
        // That shard should fail; the other 2 should become "ready".
        testWrongDatatypeFailsShard: function () {
            const {shardNames, keysPerShard} = buildKeyPool(collection, docsAboveThreshold + 1);
            assertEqual(3, shardNames.length);

            const badShard = shardNames[0];
            const goodShards = shardNames.slice(1);

            for (const shard of shardNames) {
                insertDocsForShard(collection, keysPerShard[shard], docsAboveThreshold, gen);
            }

            // Insert a document with non-numeric vector elements.
            const badKey = keysPerShard[badShard][docsAboveThreshold];
            const badVector = Array.from({length: dimension}, () => "not_a_number");
            collection.insert({_key: badKey, vector: badVector});

            const expectedCounts = {[badShard]: docsAboveThreshold + 1};
            for (const s of goodShards) {
                expectedCounts[s] = docsAboveThreshold;
            }
            assertPerShardCounts(collection, expectedCounts);

            createVectorIndex(collection);

            const expectations = {};
            for (const s of goodShards) {
                expectations[s] = {state: "ready", hasError: false};
            }
            expectations[badShard] = {state: "unusable", hasError: true};

            assertTrue(
                waitForPerShardStates(collection, "vec_l2", expectations, 120),
                "Good shards should be ready, bad shard should have error"
            );

            const shardStates = getPerShardStates(collection, "vec_l2");
            assertEqual("unusable", shardStates[badShard].state,
                "Shard with wrong datatype should be unusable");
            assertTrue(shardStates[badShard].error.length > 0,
                "Shard with wrong datatype should have an error message");
            assertTrue(
                shardStates[badShard].error.indexOf("not representable as double") !== -1,
                "Error should mention datatype issue, got: " +
                shardStates[badShard].error
            );
            for (const s of goodShards) {
                assertEqual("ready", shardStates[s].state);
                assertEqual("", shardStates[s].error);
            }
        },

        // Scenario 4: All 3 shards have enough data, but one shard has a
        // document without the vector field and the index is NOT sparse.
        // That shard should fail; the other 2 should become "ready".
        testMissingVectorFieldNonSparseFailsShard: function () {
            const {shardNames, keysPerShard} = buildKeyPool(collection, docsAboveThreshold + 1);
            assertEqual(3, shardNames.length);

            const badShard = shardNames[0];
            const goodShards = shardNames.slice(1);

            for (const shard of shardNames) {
                insertDocsForShard(collection, keysPerShard[shard], docsAboveThreshold, gen);
            }

            // Insert a document without the vector field.
            const badKey = keysPerShard[badShard][docsAboveThreshold];
            collection.insert({_key: badKey, someOtherField: "no vector here"});

            const expectedCounts = {[badShard]: docsAboveThreshold + 1};
            for (const s of goodShards) {
                expectedCounts[s] = docsAboveThreshold;
            }
            assertPerShardCounts(collection, expectedCounts);

            createVectorIndex(collection, /*sparse*/ false);

            const expectations = {};
            for (const s of goodShards) {
                expectations[s] = {state: "ready", hasError: false};
            }
            expectations[badShard] = {state: "unusable", hasError: true};

            assertTrue(
                waitForPerShardStates(collection, "vec_l2", expectations, 120),
                "Good shards should be ready, bad shard should have error"
            );

            const shardStates = getPerShardStates(collection, "vec_l2");
            assertEqual("unusable", shardStates[badShard].state,
                "Shard with missing vector should be unusable");
            assertTrue(shardStates[badShard].error.length > 0,
                "Shard with missing vector should have an error message");
            assertTrue(
                shardStates[badShard].error.indexOf("not present") !== -1 ||
                shardStates[badShard].error.indexOf("not sparse") !== -1,
                "Error should mention missing field or non-sparse, got: " +
                shardStates[badShard].error
            );
            for (const s of goodShards) {
                assertEqual("ready", shardStates[s].state);
                assertEqual("", shardStates[s].error);
            }
        },

        // Scenario 6: All 3 shards have enough data, one shard has a
        // document without the vector field, but the index IS sparse.
        // All shards should reach "ready" since sparse indexes tolerate
        // missing fields.
        testMissingVectorFieldSparseSucceeds: function () {
            const {shardNames, keysPerShard} = buildKeyPool(collection, docsAboveThreshold + 1);
            assertEqual(3, shardNames.length);

            for (const shard of shardNames) {
                insertDocsForShard(collection, keysPerShard[shard], docsAboveThreshold, gen);
            }

            // Insert a document without the vector field into one shard.
            const targetShard = shardNames[0];
            const key = keysPerShard[targetShard][docsAboveThreshold];
            collection.insert({_key: key, someOtherField: "no vector"});

            const expectedCounts = {};
            for (const s of shardNames) {
                expectedCounts[s] = (s === targetShard) ? docsAboveThreshold + 1 : docsAboveThreshold;
            }
            assertPerShardCounts(collection, expectedCounts);

            createVectorIndex(collection, /*sparse*/ true);

            const expectations = {};
            for (const s of shardNames) {
                expectations[s] = {state: "ready", hasError: false};
            }

            assertTrue(
                waitForPerShardStates(collection, "vec_l2", expectations, 120),
                "All shards should reach 'ready' with sparse index"
            );

            const shardStates = getPerShardStates(collection, "vec_l2");
            for (const s of shardNames) {
                assertEqual("ready", shardStates[s].state,
                    "Shard " + s + " should be ready (sparse tolerates missing)");
                assertEqual("", shardStates[s].error,
                    "Shard " + s + " should have no error");
            }
        },

        // Scenario 7: All 3 shards have bad data (wrong dimension).
        // All shards should fail with errors.
        testAllShardsFailWithBadData: function () {
            const {shardNames, keysPerShard} = buildKeyPool(collection, docsAboveThreshold + 1);
            assertEqual(3, shardNames.length);

            // Fill all shards with valid docs, then add one bad doc per shard.
            for (const shard of shardNames) {
                insertDocsForShard(collection, keysPerShard[shard], docsAboveThreshold, gen);
                const badKey = keysPerShard[shard][docsAboveThreshold];
                const wrongDimVector = Array.from(
                    {length: dimension / 2}, () => gen()
                );
                collection.insert({_key: badKey, vector: wrongDimVector});
            }

            const expectedCounts = {};
            for (const s of shardNames) {
                expectedCounts[s] = docsAboveThreshold + 1;
            }
            assertPerShardCounts(collection, expectedCounts);

            createVectorIndex(collection);

            const expectations = {};
            for (const s of shardNames) {
                expectations[s] = {state: "unusable", hasError: true};
            }

            assertTrue(
                waitForPerShardStates(collection, "vec_l2", expectations, 120),
                "All shards should fail with errors"
            );

            const shardStates = getPerShardStates(collection, "vec_l2");
            for (const s of shardNames) {
                assertEqual("unusable", shardStates[s].state,
                    "Shard " + s + " should be unusable");
                assertTrue(shardStates[s].error.length > 0,
                    "Shard " + s + " should have an error message");
                assertTrue(
                    shardStates[s].error.indexOf("dimension") !== -1,
                    "Error should mention dimension for shard " + s +
                    ", got: " + shardStates[s].error
                );
            }
        },
    };
}

jsunity.run(VectorIndexPerShardStateSuite);

return jsunity.done();
