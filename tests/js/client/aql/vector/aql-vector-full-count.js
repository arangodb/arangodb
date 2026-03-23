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
const aql = arangodb.aql;
const db = internal.db;
const {
    randomNumberGeneratorFloat,
} = require("@arangodb/testutils/seededRandom");
const {
    insertDocsAndEnsureIndex,
    waitForAllVectorIndexesState,
    VectorIndexTrainingState,
} = require("@arangodb/testutils/vector-index-common");
const isCluster = require("internal").isCluster();

const dbName = "vectorDB";
const collName = "vectorColl";
const numberOfShards = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexFullCountTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocsFactor = isCluster ? numberOfShards : 1;
    const numberOfDocs = 1500 * numberOfDocsFactor;
    const seed = randomInteger();
    const nLists = 10;

    return {
        setUpAll: function() {
            db._useDatabase("_system");
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards
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
            insertDocsAndEnsureIndex({
                collection, docs, seed,
                ensureIndex: () => collection.ensureIndex({
                    name: "vector_l2",
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2",
                        dimension: dimension,
                        nLists: nLists,
                        trainingIterations: 10,
                        defaultNProbe: nLists,
                    },
                }),
            });

            assertTrue(
                waitForAllVectorIndexesState(collection, VectorIndexTrainingState.kReady, 60),
                "Expected index to become ready with " + numberOfDocs + " docs"
            );
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2WithFullCount: function() {
            const query = aql`
              FOR d IN ${collection}
              SORT APPROX_NEAR_L2(${randomPoint}, d.vector)
              LIMIT 3 RETURN {k: d._key}`;

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
            assertEqual(results.length, 3);

            const stats = queryResults.getExtra().stats;
            assertEqual(stats.fullCount, numberOfDocs);
        },

        testApproxL2SkippingWithFullCount: function() {
            const queryWithSkip = aql`
              FOR d IN ${collection}
              SORT APPROX_NEAR_L2(${randomPoint}, d.vector)
              LIMIT 3, 5 RETURN {k: d._key}`;
            const queryWithoutSkip = aql`
              FOR d IN ${collection}
              SORT APPROX_NEAR_L2(d.vector, ${randomPoint})
              LIMIT 8 RETURN {k: d._key}`;

            const options = {
                fullCount: true,
            };

            const planSkipped = db
                ._createStatement(queryWithSkip, {}, options)
                .explain().plan;
            const indexNodes = planSkipped.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            const queryResultsWithSkip = db._query(queryWithSkip, {}, options);
            const resultsWithSkip = queryResultsWithSkip.toArray();
            const statsWithSkip = queryResultsWithSkip.getExtra().stats;

            const queryResultsWithoutSkip = db._query(queryWithoutSkip, {}, options);
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
// Check more details in EnumerateNearVectorExecutor file
function VectorIndexFullCountWithNotEnoughNListsTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocsFactor = isCluster ? numberOfShards : 1;
    const numberOfDocs = 1500 * numberOfDocsFactor;
    const seed = randomInteger();

    return {
        setUpAll: function() {
            db._useDatabase("_system");
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards
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
            const batchSize = 100;
            const numBatches = Math.ceil(docs.length / batchSize);
            const ensureIndexSlot = Math.abs(seed) % (numBatches + 1);

            const ensureIndex = () => collection.ensureIndex({
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

            for (let i = 0; i < numBatches; i++) {
                if (i === ensureIndexSlot) {
                    ensureIndex();
                }
                const start = i * batchSize;
                const end = Math.min(start + batchSize, docs.length);
                collection.insert(docs.slice(start, end));
            }
            if (ensureIndexSlot === numBatches) {
                ensureIndex();
            }

            assertTrue(
                waitForAllVectorIndexesState(collection, VectorIndexTrainingState.kReady, 60),
                "Expected index to become ready with " + numberOfDocs + " docs"
            );
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2FullCountDoubleLoopMultipleNlists: function() {
            const query = aql`
                FOR i in 0..3
                    FOR d IN ${collection}
                    SORT APPROX_NEAR_L2(${randomPoint}, d.vector)
                LIMIT 10
                RETURN {k: d._key}
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
            assertEqual(results.length, 10);

            const stats = queryResults.getExtra().stats;
            // 4 outer iterations (0..3) * numberOfDocs inner docs
            assertEqual(stats.fullCount, numberOfDocs * 4);
        },
    };
}

function VectorIndexFullCountCollectionWithSmallAmountOfDocs() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocsFactor = isCluster ? numberOfShards : 1;
    const numberOfDocs = 1500 * numberOfDocsFactor;
    const seed = randomInteger();

    return {
        setUpAll: function() {
            db._useDatabase("_system");
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards
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
            const batchSize = 100;
            const numBatches = Math.ceil(docs.length / batchSize);
            const ensureIndexSlot = Math.abs(seed) % (numBatches + 1);

            const ensureIndex = () => collection.ensureIndex({
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

            for (let i = 0; i < numBatches; i++) {
                if (i === ensureIndexSlot) {
                    ensureIndex();
                }
                const start = i * batchSize;
                const end = Math.min(start + batchSize, docs.length);
                collection.insert(docs.slice(start, end));
            }
            if (ensureIndexSlot === numBatches) {
                ensureIndex();
            }

            assertTrue(
                waitForAllVectorIndexesState(collection, VectorIndexTrainingState.kReady, 60),
                "Expected index to become ready with " + numberOfDocs + " docs"
            );
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2FullCountDoubleLoopSingleNList: function() {
            const query = aql`
                FOR i in 0..4
                    FOR d IN ${collection}
                    SORT APPROX_NEAR_L2(${randomPoint}, d.vector)
                LIMIT 10
                RETURN {k: d._key}
            `;
            // 5 outer iterations (0..4) * numberOfDocs inner docs
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
            assertEqual(results.length, 10);

            const stats = queryResults.getExtra().stats;
            assertEqual(stats.fullCount, numberOfDocs * 5);
        },
    };
}

// COR-128 Fetching more documents then the internal batching limit
function VectorIndexLargeLimitTestSuite() {
    let collection;
    let randomPoint;
    const largeLimitDimension = 128;
    const largeLimitNumberOfDocs = 4500;
    const nLists = 32;
    const seed = randomInteger();

    return {
        setUpAll: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards
            });

            let gen = randomNumberGeneratorFloat(seed);
            const batchSize = 1000;
            for (let batch = 0; batch < largeLimitNumberOfDocs; batch += batchSize) {
                let docs = [];
                const end = Math.min(batch + batchSize, largeLimitNumberOfDocs);
                for (let i = batch; i < end; ++i) {
                    const vector = Array.from({
                        length: largeLimitDimension
                    }, () => gen());
                    if (i === 0) {
                        randomPoint = vector;
                    }
                    docs.push({
                        vector
                    });
                }
                collection.insert(docs);
            }

            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: largeLimitDimension,
                    nLists: nLists,
                },
            });

            assertTrue(
                waitForAllVectorIndexesState(collection, VectorIndexTrainingState.kReady, 120),
                "Expected index to become ready with " + largeLimitNumberOfDocs + " docs"
            );
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testFetchLargeNumberOfDocsWithMaxNProbe: function() {
            const limits = [1500, 3000, 4000];

            for (const limit of limits) {
                const query = aql`
                  FOR d IN ${collection}
                  SORT APPROX_NEAR_L2(d.vector, ${randomPoint},
                    {nProbe: ${nLists}})
                  LIMIT ${limit} RETURN d._key`;

                const queryResults = db._query(query, {count: true}, {fullCount: true});
                const results = queryResults.toArray();
                assertEqual(limit, results.length,
                    "Expected " + limit + " results");

                const uniqueResults = new Set(results);
                assertEqual(limit, uniqueResults.size,
                    "All " + limit + " returned documents should be unique");

                assertEqual(queryResults.count(), limit);

                const stats = queryResults.getExtra().stats;
                assertEqual(stats.fullCount, largeLimitNumberOfDocs);
            }
        },
    };
}

jsunity.run(VectorIndexFullCountTestSuite);
jsunity.run(VectorIndexFullCountWithNotEnoughNListsTestSuite);
jsunity.run(VectorIndexFullCountCollectionWithSmallAmountOfDocs);
jsunity.run(VectorIndexLargeLimitTestSuite);

return jsunity.done();
