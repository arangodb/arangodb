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
const {
    randomNumberGeneratorFloat,
} = require("@arangodb/testutils/seededRandom");

const dbName = "vectorScalingDB";
const collName = "coll";
const dimension = 50;
const seed = 98765432;
const insertedDocsCount = 200;

function VectorIndexScalingTestSuite() {
    let collection;
    let randomPoint;

    return {
        setUpAll: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, { numberOfShards: 3 });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < insertedDocsCount; ++i) {
                const vector = Array.from({ length: dimension }, () => gen());
                if (i === 50) {
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

        testFixedNListsStillWorks: function() {
            collection.ensureIndex({
                name: "vector_fixed",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: { metric: "l2", dimension, nLists: 5 },
            });
            const idx = collection.getIndexes().find(i => i.name === "vector_fixed");
            assertTrue(idx !== undefined);
            assertEqual(5, idx.params.nLists);
            collection.dropIndex("vector_fixed");
        },

        testScalingNListsSqrt: function() {
            collection.ensureIndex({
                name: "vector_sqrt",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: { metric: "l2", dimension, nLists: { factor: 1, function: "sqrt" } },
            });
            const idx = collection.getIndexes().find(i => i.name === "vector_sqrt");
            assertTrue(idx !== undefined);
            assertEqual(1, idx.params.nLists.factor);
            assertEqual("sqrt", idx.params.nLists.function);

            // Query should work
            const results = db._query(
                `FOR d IN ${collName} SORT APPROX_NEAR_L2(d.vector, @qp, {nProbe: 10}) LIMIT 5 RETURN d`,
                { qp: randomPoint }
            ).toArray();
            assertEqual(5, results.length);
            collection.dropIndex("vector_sqrt");
        },

        testScalingDefaultNProbe: function() {
            collection.ensureIndex({
                name: "vector_nprobe",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2", dimension, nLists: 5,
                    defaultNProbe: { factor: 0.5, function: "sqrt" },
                },
            });
            const idx = collection.getIndexes().find(i => i.name === "vector_nprobe");
            assertTrue(idx !== undefined);
            assertEqual(0.5, idx.params.defaultNProbe.factor);
            assertEqual("sqrt", idx.params.defaultNProbe.function);
            collection.dropIndex("vector_nprobe");
        },

        testBothScaling: function() {
            collection.ensureIndex({
                name: "vector_both",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2", dimension,
                    nLists: { factor: 1, function: "sqrt" },
                    defaultNProbe: { factor: 0.1, function: "sqrt" },
                },
            });
            const idx = collection.getIndexes().find(i => i.name === "vector_both");
            assertTrue(idx !== undefined);
            assertEqual("sqrt", idx.params.nLists.function);
            assertEqual("sqrt", idx.params.defaultNProbe.function);
            collection.dropIndex("vector_both");
        },

        testInvalidFunctionNameFails: function() {
            try {
                collection.ensureIndex({
                    name: "vector_bad",
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: { metric: "l2", dimension, nLists: { factor: 1, function: "bad" } },
                });
                fail();
            } catch (e) {
                assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
            }
        },

        testZeroFactorFails: function() {
            try {
                collection.ensureIndex({
                    name: "vector_zero",
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: { metric: "l2", dimension, nLists: { factor: 0, function: "sqrt" } },
                });
                fail();
            } catch (e) {
                assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
            }
        },

        testFactoryWithScalingNListsAndPlaceholder: function() {
            collection.ensureIndex({
                name: "vector_factory_scaling",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2", dimension,
                    nLists: { factor: 1, function: "sqrt" },
                    factory: "IVF{nLists},Flat",
                },
            });
            const idx = collection.getIndexes().find(i => i.name === "vector_factory_scaling");
            assertTrue(idx !== undefined);
            assertEqual("IVF{nLists},Flat", idx.params.factory);
            collection.dropIndex("vector_factory_scaling");
        },

        testFactoryWithScalingNListsMissingPlaceholderFails: function() {
            try {
                collection.ensureIndex({
                    name: "vector_factory_no_ph",
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2", dimension,
                        nLists: { factor: 1, function: "sqrt" },
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
