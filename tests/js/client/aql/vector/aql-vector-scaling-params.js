/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue */

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

const internal = require("internal");
const jsunity = require("jsunity");
const errors = internal.errors;
const db = require("internal").db;
const {
    randomNumberGeneratorFloat,
    generateSeed
} = require("@arangodb/testutils/seededRandom");

const isCluster = internal.isCluster();
const dbName = "vectorScalingDB";
const collName = "coll";
const idxName = "vector_scaling_test";
const dimension = 128;
const insertedDocsCount = 100;

function getResolvedNLists(collection) {
    if (isCluster) {
        const idx = collection.getIndexes(true, true).find(i => i.name === idxName);
        assertTrue(idx !== undefined);
        const shardKeys = Object.keys(idx.shards);
        assertEqual(1, shardKeys.length, "tier tests require numberOfShards: 1");
        return idx.shards[shardKeys[0]].resolvedNLists;
    }
    const idx = collection.getIndexes().find(i => i.name === idxName);
    assertTrue(idx !== undefined);
    return idx.resolvedNLists;
}

function assertVectorIndexUsable(queryPoint, limit = 5) {
    const query = `FOR d IN ${collName}
        SORT APPROX_NEAR_L2(d.vector, @qp)
        LIMIT @limit
        RETURN d`;
    const result = db._query(query, { qp: queryPoint, limit }).toArray();
    assertEqual(limit, result.length);
}

function VectorIndexScalingTestSuite() {
    let collection;
    let randomPoint;
    const seed = generateSeed();

    return {
        setUpAll: function() {
            db._useDatabase("_system");
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, { numberOfShards: 3 });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < insertedDocsCount; ++i) {
                const vector = Array.from({ length: dimension }, () => gen());
                if (i === Math.floor(insertedDocsCount / 2)) {
                    randomPoint = vector;
                }
                docs.push({ vector });
            }
            collection.insert(docs);
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        tearDown: function() {
            try {
                collection.dropIndex(idxName);
            } catch(e) {}
        },

        testDefaultNListsScalingSpec: function() {
            collection.ensureIndex({
                name: idxName,
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: { metric: "l2", dimension },
            });
            const idx = collection.getIndexes().find(i => i.name === idxName);
            assertTrue(idx !== undefined);
            // When nLists is omitted, the default scaling should be used
            assertEqual("autoSqrt", idx.params.nLists.strategy, "default strategy should be autoSqrt");
            assertEqual(4, idx.params.nLists.multiplier, "default multiplier should be 4");
            assertEqual(2, idx.params.nLists.minNLists, "default minNLists should be 2");
            assertEqual(3, idx.params.nLists.tiers.length, "default should have 3 tiers");

            assertEqual(1000000, idx.params.nLists.tiers[0].threshold);
            assertEqual(16384, idx.params.nLists.tiers[0].fixedValue);
            assertEqual(10000000, idx.params.nLists.tiers[1].threshold);
            assertEqual(65536, idx.params.nLists.tiers[1].fixedValue);
            assertEqual(300000000, idx.params.nLists.tiers[2].threshold);
            assertEqual(131072, idx.params.nLists.tiers[2].fixedValue);

            // No tiers match for 100 docs, so autoSqrt applies: max(2, 4*sqrt(N))
            if (!isCluster) {
                assertTrue(idx.resolvedNLists > 0, "resolvedNLists should be set after training");
            }
        },

        testZeroMultiplierFails: function() {
            try {
                collection.ensureIndex({
                    name: idxName,
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2", dimension,
                        nLists: { multiplier: 0 },
                    },
                });
                fail();
            } catch (e) {
                assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
            }
        },

        testMinNListsValue: function() {
            collection.ensureIndex({
                name: idxName,
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2", dimension,
                    nLists: {
                        strategy: "autoSqrt",
                        multiplier: 1,
                        minNLists: 15,
                        tiers: [],
                    },
                },
            });
            const idx = collection.getIndexes().find(i => i.name === idxName);
            assertTrue(idx !== undefined);
            assertEqual(15, idx.params.nLists.minNLists);
            // multiplier=1, sqrt(N) < 15 for any per-shard count, so minNLists dominates
            if (!isCluster) {
                assertEqual(15, idx.resolvedNLists);
            }

            assertVectorIndexUsable(randomPoint);
        },

        testInvalidMinNListsFails: function() {
            try {
                collection.ensureIndex({
                    name: idxName,
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2", dimension,
                        nLists: { multiplier: 4, minNLists: 0, tiers: [] },
                    },
                });
                fail();
            } catch (e) {
                assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
            }
        },
    };
}

function VectorIndexScalingTiersTestSuite() {
    // Uses numberOfShards: 1 so that all documents land in a single shard,
    // making tier threshold comparisons deterministic.
    let collection;
    let randomPoint;
    const seed = generateSeed();

    return {
        setUpAll: function() {
            db._useDatabase("_system");
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, { numberOfShards: 1 });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < insertedDocsCount; ++i) {
                const vector = Array.from({ length: dimension }, () => gen());
                if (i === Math.floor(insertedDocsCount / 2)) {
                    randomPoint = vector;
                }
                docs.push({ vector });
            }
            collection.insert(docs);
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        tearDown: function() {
            try {
                collection.dropIndex(idxName);
            } catch(e) {}
        },

        testFirstTierTriggered: function() {
            // 100 docs: only threshold 50 <= 100, so first tier wins
            collection.ensureIndex({
                name: idxName,
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2", dimension,
                    nLists: {
                        strategy: "autoSqrt",
                        multiplier: 4,
                        minNLists: 2,
                        tiers: [
                            { threshold: 50, fixedValue: 2 },
                            { threshold: 200, fixedValue: 4 },
                            { threshold: 300, fixedValue: 6 },
                        ],
                    },
                },
            });
            assertEqual(2, getResolvedNLists(collection));
            assertVectorIndexUsable(randomPoint);
        },

        testSecondTierTriggered: function() {
            // 100 docs: thresholds 10 and 80 <= 100, so second tier wins
            collection.ensureIndex({
                name: idxName,
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2", dimension,
                    nLists: {
                        strategy: "autoSqrt",
                        multiplier: 4,
                        minNLists: 2,
                        tiers: [
                            { threshold: 10, fixedValue: 1 },
                            { threshold: 80, fixedValue: 2 },
                            { threshold: 300, fixedValue: 6 },
                        ],
                    },
                },
            });
            assertEqual(2, getResolvedNLists(collection));
            assertVectorIndexUsable(randomPoint);
        },

        testThirdTierTriggered: function() {
            // 100 docs: all thresholds <= 100, so third tier wins
            collection.ensureIndex({
                name: idxName,
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2", dimension,
                    nLists: {
                        strategy: "autoSqrt",
                        multiplier: 4,
                        minNLists: 2,
                        tiers: [
                            { threshold: 10, fixedValue: 1 },
                            { threshold: 30, fixedValue: 1 },
                            { threshold: 60, fixedValue: 2 },
                        ],
                    },
                },
            });
            assertEqual(2, getResolvedNLists(collection));
            assertVectorIndexUsable(randomPoint);
        },
    };
}

function VectorIndexScalingEmptyCollectionTestSuite() {
    let collection;

    return {
        setUpAll: function() {
            db._useDatabase("_system");
            db._createDatabase(dbName);
            db._useDatabase(dbName);
            collection = db._create(collName, { numberOfShards: 3 });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testScaledNListsOnEmptyCollectionRejected: function() {
            try {
                collection.ensureIndex({
                    name: idxName,
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2", dimension,
                        nLists: {
                            strategy: "autoSqrt",
                            multiplier: 4,
                            minNLists: 2,
                            tiers: [],
                        },
                    },
                });
                fail();
            } catch (e) {
                assertEqual(errors.ERROR_QUERY_VECTOR_INDEX_NOT_READY.code, e.errorNum);
            }
        },
    };
}

jsunity.run(VectorIndexScalingTestSuite);
jsunity.run(VectorIndexScalingTiersTestSuite);
jsunity.run(VectorIndexScalingEmptyCollectionTestSuite);

return jsunity.done();
