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

const dbName = "vectorScalingDB";
const collName = "coll";
const idxName = "vector_scaling_test";
const dimension = 128;
const insertedDocsCount = 100;

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

        testSmallTiersGetTriggered: function() {
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
                        ],
                    },
                },
            });
            const idx = collection.getIndexes().find(i => i.name === idxName);
            assertTrue(idx !== undefined);
            assertEqual(2, idx.params.nLists.tiers.length);
            assertEqual(50, idx.params.nLists.tiers[0].threshold);
            assertEqual(2, idx.params.nLists.tiers[0].fixedValue);

            assertVectorIndexUsable(randomPoint);
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

function VectorIndexScalingEmptyCollectionTestSuite() {
    let collection;

    return {
        setUpAll: function() {
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
jsunity.run(VectorIndexScalingEmptyCollectionTestSuite);

return jsunity.done();
