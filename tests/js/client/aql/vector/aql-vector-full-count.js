/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

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
const arangodb = require("@arangodb");
const helper = require("@arangodb/aql-helper");
const aql = arangodb.aql;
const getQueryResults = helper.getQueryResults;
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;
const db = internal.db;
const {
    randomNumberGeneratorFloat,
} = require("@arangodb/testutils/seededRandom");

const isCluster = require("internal").isCluster();
const dbName = "vectorDB";
const collName = "vectorColl";
const indexName = "vectorIndex";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexFullCountTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocs = 100;
    const seed = 12132390894;

    return {
        setUpAll: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 1
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < numberOfDocs; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                if (i === numberOfDocs / 2) {
                    randomPoint = vector;
                }
                docs.push({
                    vector,
                });
            }
            collection.insert(docs);

            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: 10,
                    trainingIterations: 10,
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2WithFullCount: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(@qp, d.vector) LIMIT 3 RETURN {k: d._key}";

            const bindVars = {
                qp: randomPoint
            };
            const options = {
                fullCount: true,
            };

            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                    options
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            const queryResults = db._query(query, bindVars, options);
            const results = queryResults.toArray();
            assertEqual(results.length, 3);

            const stats = queryResults.getExtra().stats;
            assertEqual(stats.fullCount, numberOfDocs);
        },

        testApproxL2SkippingWithFullCount: function() {
            const queryWithSkip =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(@qp, d.vector) LIMIT 3, 5 RETURN {k: d._key}";
            const queryWithoutSkip =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 8 RETURN {k: d._key}";

            const bindVars = {
                qp: randomPoint
            };
            const options = {
                fullCount: true,
            };

            const planSkipped = db
                ._createStatement({
                    query: queryWithSkip,
                    bindVars,
                    options,
                })
                .explain().plan;
            const indexNodes = planSkipped.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            const queryResultsWithSkip = db._query(queryWithSkip, bindVars, options);
            const resultsWithSkip = queryResultsWithSkip.toArray();
            const statsWithSkip = queryResultsWithSkip.getExtra().stats;

            const queryResultsWithoutSkip = db._query(queryWithoutSkip, bindVars, options);
            const resultsWithoutSkip = queryResultsWithoutSkip.toArray();
            const statsWithoutSkip = queryResultsWithoutSkip.getExtra().stats;

            assertEqual(resultsWithSkip.length, 5);
            assertEqual(resultsWithoutSkip.length, 8);

            assertEqual(statsWithSkip.fullCount, numberOfDocs);
            assertEqual(statsWithoutSkip.fullCount, numberOfDocs);

            assertEqual(resultsWithSkip, resultsWithoutSkip.slice(3, resultsWithoutSkip.length));
        },

        testApproxL2DoubleLoopWithFullCount: function() {
            const query = aql`
              FOR docOuter IN ${collection}
              FOR docInner IN ${collection}
              SORT APPROX_NEAR_L2(docInner.vector, docOuter.vector)
              LIMIT 5 RETURN docInner
              `;

            const options = {
                fullCount: true,
            };

            const plan = db
                ._createStatement(query, {}, options)
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            const queryResults = db._query(query, {}, options);
            const results = queryResults.toArray();
            assertEqual(results.length, 5);

            const stats = queryResults.getExtra().stats;
            // If we were to continue we should get a product
            assertEqual(stats.fullCount, numberOfDocs * numberOfDocs);
        },

        testApproxL2DoubleLoopWithFullCountAndSkipping: function() {
            const query = aql`
              FOR docOuter IN ${collection}
              FOR docInner IN ${collection}
              SORT APPROX_NEAR_L2(docInner.vector, docOuter.vector)
              LIMIT 5, 5 RETURN docInner
              `;

            const options = {
                fullCount: true,
            };

            const plan = db
                ._createStatement(query, {}, options)
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            const queryResults = db._query(query, {}, options);
            const results = queryResults.toArray();
            assertEqual(results.length, 5);

            const stats = queryResults.getExtra().stats;
            // If we were to continue we should get a product
            assertEqual(stats.fullCount, numberOfDocs * numberOfDocs);
        },
    };
}

/// The test suite with vector index not having enough
// documents in single nList will not return true full count in collection but how much
// it actually produced.
// Check more details in EnumerateNearVectorExucutor file
function VectorIndexFullCountWithNotEnoughNListsTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocs = 10;
    const seed = 12132390894;

    return {
        setUpAll: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 1
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < numberOfDocs; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                if (i === numberOfDocs / 2) {
                    randomPoint = vector;
                }
                docs.push({
                    vector
                });
            }
            collection.insert(docs);

            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: 10,
                    trainingIterations: 10,
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2FullCountDoubleLoopMultipleNlists: function() {
            const query = `
                FOR i in 0..3
                    FOR d IN ${collection.name()}
                    SORT APPROX_NEAR_L2(@qp, d.vector)
                LIMIT 10
                RETURN {k: d._key}
            `;
            const bindVars = {
                qp: randomPoint,
            };
            const options = {
                fullCount: true,
            };

            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                    options
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            const queryResults = db._query(query, bindVars, options);
            const results = queryResults.toArray();
            assertEqual(results.length, 4);

            const stats = queryResults.getExtra().stats;
            assertEqual(stats.fullCount, 4);
        },
    };
}

function VectorIndexFullCountCollectionWithSmallAmountOfDocs() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocs = 3;
    const seed = 12132390894;

    return {
        setUpAll: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 1
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < numberOfDocs; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                if (i === 1) {
                    randomPoint = vector;
                }
                docs.push({
                    vector
                });
            }
            collection.insert(docs);

            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: 1,
                    trainingIterations: 10,
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2FullCountDoubleLoopSingleNList: function() {
            const query = `
                FOR i in 0..4
                    FOR d IN ${collection.name()}
                    SORT APPROX_NEAR_L2(@qp, d.vector)
                LIMIT 10
                RETURN {k: d._key}
            `;
            // i=0    i=1    i=2    i=3    i=4
            // 1,2,3, 1,2,3, 1,2,3, 1|,2,3 1,2,3
            //                       ^ LIMIT 10
            const bindVars = {
                qp: randomPoint,
            };
            const options = {
                fullCount: true,
            };

            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                    options
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            const queryResults = db._query(query, bindVars, options);
            const results = queryResults.toArray();
            assertEqual(results.length, 10);

            const stats = queryResults.getExtra().stats;
            assertEqual(stats.fullCount, 15);
        },
    };
}

jsunity.run(VectorIndexFullCountTestSuite);
jsunity.run(VectorIndexFullCountWithNotEnoughNListsTestSuite);
jsunity.run(VectorIndexFullCountCollectionWithSmallAmountOfDocs);

return jsunity.done();