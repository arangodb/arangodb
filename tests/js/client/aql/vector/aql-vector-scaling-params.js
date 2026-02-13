/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
const print = internal.print;
const {
    randomNumberGeneratorFloat,
    randomInteger,
} = require("@arangodb/testutils/seededRandom");

const dbName = "vectorScalingDB";
const collName = "coll";
const idxName = "vector_scaling_test";
const dimension = 50;
const seed = randomInteger();
const insertedDocsCount = 2000;

function VectorIndexScalingTestSuite() {
    let collection;
    let randomPoint;

    return {
        setUpAll: function() {
            print("Using seed: " + seed);
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
            } catch (_) {}
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
            // When nLists is omitted, the default scaling spec should be used
            assertEqual("autoSqrt", idx.params.nLists.strategy, "default strategy should be autoSqrt");
            assertEqual(8, idx.params.nLists.multiplier, "default multiplier should be 8");
            assertEqual(32, idx.params.nLists.minNLists, "default minNLists should be 32");
            assertEqual(3, idx.params.nLists.tiers.length, "default should have 3 tiers");

            assertEqual(100000000, idx.params.nLists.tiers[0].min_n);
            assertEqual(1048576, idx.params.nLists.tiers[0].fixed_value);
            assertEqual(10000000, idx.params.nLists.tiers[1].min_n);
            assertEqual(262144, idx.params.nLists.tiers[1].fixed_value);
            assertEqual(1000000, idx.params.nLists.tiers[2].min_n);
            assertEqual(65536, idx.params.nLists.tiers[2].fixed_value);
            assertEqual("autoSqrt", idx.params.defaultNProbe, "default defaultNProbe should be autoSqrt");
        },

        testScalingNListsWithTiersAndExplicitNProbe: function() {
            const params = {
                metric: "l2", dimension,
                nLists: {
                    strategy: "autoSqrt",
                    multiplier: 4,
                    minNLists: 2,
                    tiers: [
                        { min_n: 50000000, fixed_value: 500000 },
                        { min_n: 5000000, fixed_value: 50000 },
                        { min_n: 500000, fixed_value: 5000 },
                    ],
                },
                defaultNProbe: 10,
                trainingIterations: 25,
            };
            collection.ensureIndex({
                name: idxName,
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params,
            });
            const idx = collection.getIndexes().find(i => i.name === idxName);
            assertTrue(idx !== undefined);
            assertEqual(params, idx.params);

            // Query should work
            const results = db._query(
                `FOR d IN ${collName} SORT APPROX_NEAR_L2(d.vector, @qp, {nProbe: 10}) LIMIT 5 RETURN d`,
                { qp: randomPoint }
            ).toArray();
            assertEqual(5, results.length);
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
                        nLists: { multiplier: 0, minNLists: 1, tiers: [] },
                    },
                });
                fail();
            } catch (e) {
                assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
            }
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

        testFactoryWithScalingNListsAndPlaceholder: function() {
            collection.ensureIndex({
                name: idxName,
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2", dimension,
                    nLists: { multiplier: 4, minNLists: 2, tiers: [] },
                    factory: "IVF{nLists},Flat",
                },
            });
            const idx = collection.getIndexes().find(i => i.name === idxName);

            assertTrue(idx !== undefined);
            assertEqual(4, idx.params.nLists.multiplier, `nLists multiplier should be 4, got ${idx.params.nLists.multiplier}`);
            assertEqual("IVF{nLists},Flat", idx.params.factory);
        },

        testFactoryWithScalingNListsMissingPlaceholderFails: function() {
            try {
                collection.ensureIndex({
                    name: idxName,
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2", dimension,
                        nLists: { multiplier: 4, minNLists: 2, tiers: [] },
                        factory: "IVF10,Flat",
                    },
                });
                fail();
            } catch (e) {
                assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
            }
        },
    };
}

jsunity.run(VectorIndexScalingTestSuite);

return jsunity.done();
