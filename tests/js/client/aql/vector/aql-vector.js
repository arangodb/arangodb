/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, print */

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
    randomInteger,
} = require("@arangodb/testutils/seededRandom");
const {
    versionHas
} = require("@arangodb/test-helper");
const isCluster = require("internal").isCluster();
const dbName = "vectorDb";
const collName = "vectorColl";
const indexName = "vectorIndex";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexL2TestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocs = 500;
    const seed = randomInteger();

    return {
        setUpAll: function() {
            print("Using seed: " + seed);
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < numberOfDocs; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                if (i === (numberOfDocs / 2)) {
                    randomPoint = vector;
                }
                docs.push({
                    vector,
                    nonVector: i,
                    unIndexedVector: vector
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

        testApproxL2OnUnindexedField: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.unIndexedVector, @qp) LIMIT 5 RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint
            };
            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query,
                bindVars,
            );
        },

        testApproxL2OnNonVectorField: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.nonVector, @qp) LIMIT 5 RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint
            };
            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query,
                bindVars,
            );
        },

        testApproxL2WrongInputDimension: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN {key: d._key}";

            let changedRandomPoints = randomPoint.slice();
            changedRandomPoints.pop();
            const bindVars = {
                qp: changedRandomPoints
            };
            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            assertQueryError(
                errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code,
                query,
                bindVars,
            );
        },

        testApproxL2WrongInput: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN {key: d._key}";

            const bindVars = {
                qp: "random"
            };
            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            assertQueryError(
                errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code,
                query,
                bindVars,
            );
        },

        testApproxL2WithoutLimit: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint
            };
            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query,
                bindVars,
            );
        },

        testApproxL2DescendingOrder: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) DESC LIMIT 5" +
                " RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint
            };
            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query,
                bindVars,
            );
        },

        testApproxL2MutipleSortAttributes: function() {
            const query = aql`
                FOR d IN ${collection}
                SORT APPROX_NEAR_L2(d.vector, ${randomPoint}), d.nonVector LIMIT 5
                RETURN {key: d._key}`;

            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query
            );
        },

        testApproxL2MultipleTopK: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " LET dist = APPROX_NEAR_L2(d.vector, @qp) " +
                "SORT dist LIMIT @topK " +
                "RETURN {key: d._key, dist}";

            const topKs = [1, 5, 10, 15, 50, 100];
            let previousResult = [];
            for (let i = 0; i < topKs.length; ++i) {
                const bindVars = {
                    qp: randomPoint,
                    topK: topKs[i]
                };
                const plan = db
                    ._createStatement({
                        query,
                        bindVars,
                    })
                    .explain().plan;
                const indexNodes = plan.nodes.filter(function(n) {
                    return n.type === "EnumerateNearVectorNode";
                });
                assertEqual(1, indexNodes.length);

                // Assert gather node is sorted
                if (isCluster) {
                    const gatherNodes = plan.nodes.filter(function(n) {
                        return n.type === "GatherNode";
                    });
                    assertEqual(1, gatherNodes.length);

                    let gatherNode = gatherNodes[0];
                    assertEqual(1, gatherNode.elements.length);
                    assertTrue(gatherNode.elements[0].ascending);
                }

                const results = db._query(query, bindVars).toArray();
                // Assert that results are deterministic
                if (i !== 0) {
                    for (let j = 0; j < previousResult.length; ++j) {
                        assertEqual(previousResult[j].key, results[j].key, "Results are not deterministic: " + JSON.stringify(results));
                    }
                }

                // For l2 metric the results must be ordered in descending order
                for (let j = 1; j < results.length; ++j) {
                    assertTrue(results[j - 1].dist <= results[j].dist, "Results are not in ascending order: " + JSON.stringify(results));
                }
            }
        },

        testApproxL2SeachWithMixedComponentsVector: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " LET dist = APPROX_NEAR_L2(d.vector, @qp) " +
                "SORT dist LIMIT 3 " +
                "RETURN {key: d._key, dist}";

            let changedRandomPoint = JSON.parse(JSON.stringify(randomPoint));
            changedRandomPoint[0] = 0;
            changedRandomPoint[1] = 1;
            changedRandomPoint[2] = -1;

            const bindVars = {
                qp: changedRandomPoint
            };
            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });

            assertEqual(1, indexNodes.length);

            const results = db._query(query, bindVars).toArray();
            assertEqual(3, results.length);
        },

        testApproxL2Commutation: function() {
            const queryFirst =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(@qp, d.vector) LIMIT 5 RETURN {key: d._key}";
            const querySecond =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint
            };

            const planFirst = db
                ._createStatement({
                    query: queryFirst,
                    bindVars,
                })
                .explain().plan;
            const planSecond = db
                ._createStatement({
                    query: querySecond,
                    bindVars,
                })
                .explain().plan;
            assertEqual(planFirst, planSecond);

            const resultsFirst = db._query(queryFirst, bindVars).toArray();
            const resultsSecond = db._query(querySecond, bindVars).toArray();
            assertEqual(resultsFirst, resultsSecond);
        },

        testApproxL2Distance: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " LET dist = APPROX_NEAR_L2(@qp, d.vector)" +
                " SORT dist LIMIT 5 RETURN dist";

            const bindVars = {
                qp: randomPoint
            };
            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);
        },

        testApproxL2WithDoubleLoop: function() {
            const query = aql`
              FOR docOuter IN ${collection}
              FOR docInner IN ${collection}
              SORT APPROX_NEAR_L2(docInner.vector, docOuter.vector)
              LIMIT 5 RETURN docInner
              `;

            const plan = db
                ._createStatement(query)
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            const results = db._query(query).toArray();
            assertEqual(5, results.length);
        },

        testApproxL2Skipping: function() {
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

            const resultsWithSkip = db._query(queryWithSkip, bindVars).toArray();
            const resultsWithoutSkip = db._query(queryWithoutSkip, bindVars).toArray();

            assertEqual(resultsWithSkip.length, 5);
            assertEqual(resultsWithoutSkip.length, 8);

            // Check that skip results are contained within without skip results
            const skipKeys = new Set(resultsWithSkip.map(r => r.k));
            const withoutSkipKeys = new Set(resultsWithoutSkip.map(r => r.k));

            assertTrue([...skipKeys].every(key => withoutSkipKeys.has(key)), "Skip results are not contained within without skip results: " + JSON.stringify(resultsWithSkip) + " " + JSON.stringify(resultsWithoutSkip));
        },

        testApproxL2Subquery: function() {
            const queries = [aql`
        FOR docOuter IN ${collection}
        FILTER docOuter.nonVector < 10 
        LET neighbours = (FOR docInner IN ${collection}
          LET dist = APPROX_NEAR_L2(docInner.vector, docOuter.vector)
          SORT dist 
          LIMIT 5 
          RETURN { key: docInner._key, dist })
        RETURN { key: docOuter._key, neighbours }
        `, aql`
        FOR docOuter IN ${collection}
        FILTER docOuter.nonVector < 10 
        LET neighbours = (FOR docInner IN ${collection}
          LET dist = APPROX_NEAR_L2(docOuter.vector, docInner.vector)
          SORT dist 
          LIMIT 5 
          RETURN { key: docInner._key, dist })
        RETURN { key: docOuter._key, neighbours }
        `];

            for (let i = 0; i < queries.length; ++i) {
                let query = queries[i];
                const plan = db._createStatement(query).explain().plan;
                const indexNodes = plan.nodes.filter(function(n) {
                    return n.type === "EnumerateNearVectorNode";
                });
                assertEqual(1, indexNodes.length);

                const results = db._query(query).toArray();
                assertEqual(10, results.length);

                results.forEach((element) => {
                    assertEqual(5, element.neighbours.length);
                });
            }
        },

        testApproxL2SubqueryWithSkipping: function() {
            const queryWithSkip = aql`
            FOR docOuter IN ${collection}
            FILTER docOuter.nonVector < 10 
            SORT docOuter.nonVector
            LET neighbours = (FOR docInner IN ${collection}
              LET dist = APPROX_NEAR_L2(docInner.vector, docOuter.vector)
              SORT dist
              LIMIT 3, 5
              RETURN { key: docInner._key, dist })
            RETURN { key: docOuter._key, neighbours }`;

            const queryWithoutSkip = aql`
              FOR docOuter IN ${collection}
              FILTER docOuter.nonVector < 10 
              SORT docOuter.nonVector
              LET neighbours = (FOR docInner IN ${collection}
                LET dist = APPROX_NEAR_L2(docInner.vector, docOuter.vector)
                SORT dist
                LIMIT 8
                RETURN { key: docInner._key, dist })
              RETURN { key: docOuter._key, neighbours }`;

            const planSkipped = db
                ._createStatement(queryWithSkip)
                .explain().plan;
            const indexNodes = planSkipped.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);

            const resultsWithSkip = db._query(queryWithSkip).toArray();
            const resultsWithoutSkip = db._query(queryWithoutSkip).toArray();
            assertEqual(resultsWithSkip.length, resultsWithoutSkip.length);

            for (let i = 0; i < resultsWithSkip.length; ++i) {
                const skipNeighbours = resultsWithSkip[i].neighbours;
                const nonSkipNeighbours = resultsWithoutSkip[i].neighbours;
                assertEqual(skipNeighbours.length, 5);
                assertEqual(nonSkipNeighbours.length, 8);

                // Compare neighbours as sets by extracting keys
                const skipNeighbourKeys = new Set(skipNeighbours.map(n => n.key));
                const nonSkipNeighbourKeys = new Set(nonSkipNeighbours.map(n => n.key));
                const expectedNeighbourKeys = new Set(nonSkipNeighbours.slice(3).map(n => n.key));

                assertTrue([...skipNeighbourKeys].every(key => nonSkipNeighbourKeys.has(key)), "Skip results are not contained within without skip results: " + JSON.stringify(resultsWithSkip) + " " + JSON.stringify(resultsWithoutSkip));
                assertEqual(skipNeighbourKeys.size, expectedNeighbourKeys.size);
                assertTrue([...skipNeighbourKeys].every(key => expectedNeighbourKeys.has(key)));
            }
        },
    };
}

// When cosine similarity scores are too close to each other,  
// vector search results can become non-deterministic due to floating-point 
// precision issues. This causes test failures where document ordering 
// changes between test runs. We ensure minimum distance separation between 
// vectors' cosine distances from the query point to guarantee deterministic results.
function VectorIndexCosineTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocs = 1000;
    const seed = randomInteger();
    // ~1.19 × 10^−7
    const floatEpsilon = 0.0000001;

    // Helper function to calculate cosine similarity between two vectors
    function cosineSimilarity(vec1, vec2) {
        if (vec1.length !== vec2.length) {
            throw new Error("Vectors must have the same length");
        }
        
        let dotProduct = 0;
        let norm1 = 0;
        let norm2 = 0;
        
        for (let i = 0; i < vec1.length; i++) {
            dotProduct += vec1[i] * vec2[i];
            norm1 += vec1[i] * vec1[i];
            norm2 += vec2[i] * vec2[i];
        }
        
        norm1 = Math.sqrt(norm1);
        norm2 = Math.sqrt(norm2);
        
        if (norm1 === 0 || norm2 === 0) {
            return 0;
        }
        
        return dotProduct / (norm1 * norm2);
    }

    // Helper function to calculate cosine distance (1 - cosine similarity)
    function cosineDistance(vec1, vec2) {
        return 1 - cosineSimilarity(vec1, vec2);
    }


    return {
        setUpAll: function() {
            print("Using seed: " + seed);
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });

            const docs = [];
            const gen = randomNumberGeneratorFloat(seed); // Assumed to be provided externally
            const maxAttemptsPerDoc = 100; // Limit attempts for each vector generation
        
            // This array will store the cosine distances of generated vectors from 'randomPoint'.
            // It will be kept sorted to enable faster proximity checks.
            const distancesFromRandomPoint = [];

            // Helper function to find if a targetDistance is too close to any existing distance
            // in the sorted distancesFromRandomPoint array.
            function findProximity(targetDistance) {
                let low = 0;
                let high = distancesFromRandomPoint.length - 1;

                // Binary search to find a starting point for linear scan
                let startIndex = 0; // Default to start of array
                while (low <= high) {
                    const mid = Math.floor((low + high) / 2);
                    if (distancesFromRandomPoint[mid] < targetDistance - floatEpsilon) {
                        low = mid + 1;
                    } else {
                        startIndex = mid;
                        high = mid - 1;
                    }
                }

                // Linearly scan from the determined startIndex to find a close match.
                // We only need to check values that could possibly be within the floatEpsilon range.
                for (let k = startIndex; k < distancesFromRandomPoint.length; k++) {
                    const existingDistance = distancesFromRandomPoint[k];
                    // If the current existingDistance is already too far, we can stop early
                    if (existingDistance > targetDistance + floatEpsilon) {
                        break;
                    }
                    // Check if it's within the epsilon range
                    if (Math.abs(existingDistance - targetDistance) < floatEpsilon) {
                        return true; // Found a close distance
                    }
                }
                return false; // No close distance found
            }

            // Helper function to insert a value into the sorted distancesFromRandomPoint array
            function insertSorted(value) {
                let low = 0;
                let high = distancesFromRandomPoint.length;
                // Binary search to find the correct insertion point
                while (low < high) {
                    const mid = Math.floor((low + high) / 2);
                    if (distancesFromRandomPoint[mid] < value) {
                        low = mid + 1;
                    } else {
                        high = mid;
                    }
                }
                // Insert the value at the found position, maintaining sort order
                distancesFromRandomPoint.splice(low, 0, value);
            }
        
            for (let i = 0; i < numberOfDocs; i++) {
                let attempts = 0;
                let vector = null;
                let isTooClose;
        
                while (attempts < maxAttemptsPerDoc) {
                    vector = Array.from({ length: dimension }, () => gen());
                    isTooClose = false;
        
                    // Set the randomPoint when we reach the middle index
                    if (i === 0) {
                        randomPoint = vector;
                        // Once randomPoint is set, calculate distances for all previously generated
                        // vectors and add them to distancesFromRandomPoint.
                        // This ensures all relevant distances are tracked.
                        for (let j = 0; j < docs.length; j++) {
                            insertSorted(cosineDistance(docs[j].vector, randomPoint));
                        }
                        break; // randomPoint found, no proximity check needed for itself
                    }
        
                    // Perform proximity check only if randomPoint is set and we are generating
                    // vectors *after* the randomPoint.
                    if (randomPoint && i > Math.floor(numberOfDocs / 2)) {
                        const currentDistance = cosineDistance(vector, randomPoint);
                        isTooClose = findProximity(currentDistance);
                    }
        
                    if (!isTooClose) {
                        // If the vector is suitable, and randomPoint is set and active,
                        // add its distance to our sorted array for future checks.
                        if (randomPoint && i > Math.floor(numberOfDocs / 2)) {
                            insertSorted(cosineDistance(vector, randomPoint));
                        }
                        break; // Found a suitable vector, exit inner loop
                    }
                    attempts++;
                }
        
                if (attempts === maxAttemptsPerDoc) {
                    console.warn(`Warning: Could not generate a sufficiently unique vector for index ${i} after ${maxAttemptsPerDoc} attempts. Consider adjusting parameters.`);
                }
        
                docs.push({
                    vector: vector,
                    nonVector: i, // Use the loop index directly for simplicity
                    unIndexedVector: vector // Redundant if 'vector' is the stored value, but kept for original intent
                });
            }
            
            if (docs.length < numberOfDocs) {
                throw new Error(`Could not generate ${numberOfDocs} vectors with sufficient distance separation after ${maxAttempts} attempts. Generated ${docs.length} vectors.`);
            }
            
            collection.insert(docs);

            collection.ensureIndex({
                name: "vector_cosine",
                type: "vector",
                fields: ["vector"],
                params: {
                    metric: "cosine",
                    dimension: dimension,
                    nLists: 10
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxCosineOnUnindexedField: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_COSINE(d.unIndexedVector, @qp) DESC LIMIT 5 RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint
            };
            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query,
                bindVars,
            );
        },

        testApproxCosineMultipleTopK: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " LET sim = APPROX_NEAR_COSINE(d.vector, @qp) " +
                "SORT sim DESC LIMIT @topK " +
                "RETURN {key: d._key, sim}";

            const topKs = [1, 5, 10, 15, 50, 100];
            let previousResult = [];
            for (let i = 0; i < topKs.length; ++i) {
                const bindVars = {
                    qp: randomPoint,
                    topK: topKs[i]
                };
                const plan = db
                    ._createStatement({
                        query,
                        bindVars,
                    })
                    .explain().plan;
                const indexNodes = plan.nodes.filter(function(n) {
                    return n.type === "EnumerateNearVectorNode";
                });
                assertEqual(1, indexNodes.length);

                // Assert gather node is sorted
                if (isCluster) {
                    const gatherNodes = plan.nodes.filter(function(n) {
                        return n.type === "GatherNode";
                    });
                    assertEqual(1, gatherNodes.length);

                    let gatherNode = gatherNodes[0];
                    assertEqual(1, gatherNode.elements.length);
                    assertFalse(gatherNode.elements[0].ascending);
                }

                const results = db._query(query, bindVars).toArray();

                // Assert that results are deterministic
                if (i !== 0) {
                    for (let j = 0; j < previousResult.length; ++j) {
                        assertEqual(previousResult[j].key, results[j].key);
                    }
                }

                // For cosine similarity the results must be ordered in descending order
                for (let j = 1; j < results.length; ++j) {
                    assertTrue(results[j - 1].sim >= results[j].sim, "Results are not in descending order: " + JSON.stringify(results));
                }
                // Assert that distances are in [-1, 1] range
                for (let j = 0; j < results.length; ++j) {
                    assertTrue(Math.abs(results[j].sim) <= 1.01, "Cosine similarity is not in [-1, 1] range: " + JSON.stringify(results));
                }
            }
        },

        testApproxCosineMultipleTopKWrongOrder: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_COSINE(d.vector, @qp) ASC LIMIT 5 " +
                " RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint,
            };
            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(0, indexNodes.length);
        },

        testApproxCosineSkipping: function() {
            const queryWithSkip =
                "FOR d IN " +
                collection.name() +
                " LET sim = APPROX_NEAR_COSINE(@qp, d.vector) " + 
                " SORT sim DESC LIMIT 3, 5 RETURN {k: d._key, sim}";
            const queryWithoutSkip =
                "FOR d IN " +
                collection.name() +
                " LET sim = APPROX_NEAR_COSINE(d.vector, @qp)" + 
                " SORT sim DESC LIMIT 8 RETURN {k: d._key, sim}";

            const bindVars = {
                qp: randomPoint
            };

            const resultsWithSkip = db._query(queryWithSkip, bindVars).toArray();
            const resultsWithoutSkip = db._query(queryWithoutSkip, bindVars).toArray();

            assertTrue(resultsWithSkip.length === 5);
            assertTrue(resultsWithoutSkip.length === 8);

            const skipKeys = new Set(resultsWithSkip.map(r => r.k));
            const withoutSkipKeys = new Set(resultsWithoutSkip.map(r => r.k));

            assertTrue([...skipKeys].every(key => withoutSkipKeys.has(key)), "Skipping not deterministic with not skipping: " + JSON.stringify(resultsWithSkip) + " " + JSON.stringify(resultsWithoutSkip));
        },
    };
}

function VectorIndexInnerProductTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const seed = randomInteger();

    return {
        setUpAll: function() {
            print("Using seed: " + seed);
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < 1000; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                if (i === 250) {
                    randomPoint = vector;
                }
                docs.push({
                    vector,
                    nonVector: i,
                    unIndexedVector: vector
                });
            }
            collection.insert(docs);

            collection.ensureIndex({
                name: "vector_inner_product",
                type: "vector",
                fields: ["vector"],
                params: {
                    metric: "innerProduct",
                    dimension: dimension,
                    nLists: 10
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxInnerProductMultipleTopK: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(d.vector, @qp) " +
                "SORT sim DESC LIMIT @topK " +
                "RETURN {key: d._key, sim}";

            const topKs = [1, 5, 10, 15, 50, 100];
            let previousResult = [];
            for (let i = 0; i < topKs.length; ++i) {
                const bindVars = {
                    qp: randomPoint,
                    topK: topKs[i]
                };
                const plan = db
                    ._createStatement({
                        query,
                        bindVars,
                    })
                    .explain().plan;
                const indexNodes = plan.nodes.filter(function(n) {
                    return n.type === "EnumerateNearVectorNode";
                });
                assertEqual(1, indexNodes.length);

                // Assert gather node is sorted
                if (isCluster) {
                    const gatherNodes = plan.nodes.filter(function(n) {
                        return n.type === "GatherNode";
                    });
                    assertEqual(1, gatherNodes.length);

                    let gatherNode = gatherNodes[0];
                    assertEqual(1, gatherNode.elements.length);
                    assertFalse(gatherNode.elements[0].ascending);
                }

                const results = db._query(query, bindVars).toArray();

                // Assert that results are deterministic
                if (i !== 0) {
                    for (let j = 0; j < previousResult.length; ++j) {
                        assertEqual(previousResult[j].key, results[j].key);
                    }
                }

                // For inner product metric the results must be ordered in descending order
                for (let j = 1; j < results.length; ++j) {
                    assertTrue(results[j - 1].sim >= results[j].sim, "Results are not in descending order: " + JSON.stringify(results));
                }
            }
        },

        testApproxInnerProductMultipleTopKWrongOrder: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_INNER_PRODUCT(d.vector, @qp) ASC LIMIT 5 " +
                " RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint,
            };
            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(0, indexNodes.length);
        },

        testApproxInnerProductSkipping: function() {
            const queryWithSkip =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) DESC LIMIT 3, 5 RETURN {k: d._key}";
            const queryWithoutSkip =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_INNER_PRODUCT(d.vector, @qp) DESC LIMIT 8 RETURN {k: d._key}";

            const bindVars = {
                qp: randomPoint
            };

            const resultsWithSkip = db._query(queryWithSkip, bindVars).toArray();
            const resultsWithoutSkip = db._query(queryWithoutSkip, bindVars).toArray();

            // Check that skip results are contained within without skip results
            const skipKeys = new Set(resultsWithSkip.map(r => r.k));
            const withoutSkipKeys = new Set(resultsWithoutSkip.map(r => r.k));

            assertTrue([...skipKeys].every(key => withoutSkipKeys.has(key)), "Skip results are not contained within without skip results: " + JSON.stringify(resultsWithSkip) + " " + JSON.stringify(resultsWithoutSkip));
        },
    };
}

function MultipleVectorIndexesOnField() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocs = 1000;
    const seed = randomInteger();

    return {
        setUp: function() {
            print("Using seed: " + seed);
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < numberOfDocs; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                if (i === (numberOfDocs / 2)) {
                    randomPoint = vector;
                }
                docs.push({
                    vector,
                    fieldInt: i,
                    fieldVec: vector
                });
            }
            collection.insert(docs);
        },

        tearDown: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxUseL2WhenMultipleIndexes: function() {
            collection.ensureIndex({
                name: "vector_cosine",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "cosine",
                    dimension: dimension,
                    nLists: 10
                },
            });
            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: 10
                },
            });

            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) " +
                " LIMIT 5 RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint
            };
            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);
            assertEqual(indexNodes[0].index.name, "vector_l2");

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);
        },

        testApproxUseCosineWhenMultipleIndexes: function() {
            collection.ensureIndex({
                name: "vector_cosine",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "cosine",
                    dimension: dimension,
                    nLists: 10
                },
            });
            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: 10
                },
            });

            const query =
                "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_COSINE(d.vector, @qp) DESC " +
                " LIMIT 5 RETURN {key: d._key}";

            const bindVars = {
                qp: randomPoint
            };
            const plan = db
                ._createStatement({
                    query,
                    bindVars,
                })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) {
                return n.type === "EnumerateNearVectorNode";
            });
            assertEqual(1, indexNodes.length);
            assertEqual(indexNodes[0].index.name, "vector_cosine");

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);
        },

        // Test is mostly to see behavior
        testApproxL2WhenMultipleIndexesOnDifferentFields: function() {
            collection.ensureIndex({
                name: "field_l2",
                type: "vector",
                fields: ["fieldVec"],
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: 10
                },
            });
            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: 10
                },
            });

            const queries = [{
                query: "FOR d IN " +
                    collection.name() +
                    " SORT APPROX_NEAR_L2(d.vector, @qp) " +
                    " LIMIT 5 RETURN {key: d._key}",
                indexName: "vector_l2",
            }, {
                query: "FOR d IN " +
                    collection.name() +
                    " SORT APPROX_NEAR_L2(d.fieldVec, @qp) " +
                    " LIMIT 5 RETURN {key: d._key}",
                indexName: "field_l2",
            }, ];

            for (let i = 0; i < queries.length; ++i) {
                const query = queries[i].query;
                const indexName = queries[i].indexName;
                const bindVars = {
                    qp: randomPoint
                };
                const plan = db
                    ._createStatement({
                        query,
                        bindVars,
                    })
                    .explain().plan;
                const indexNodes = plan.nodes.filter(function(n) {
                    return n.type === "EnumerateNearVectorNode";
                });

                assertEqual(1, indexNodes.length);
                assertEqual(indexNodes[0].index.name, indexName);

                const results = db._query(query, bindVars).toArray();
                assertEqual(5, results.length);
            }
        },
    };
}

jsunity.run(VectorIndexL2TestSuite);
jsunity.run(VectorIndexCosineTestSuite);
jsunity.run(VectorIndexInnerProductTestSuite);
jsunity.run(MultipleVectorIndexesOnField);

return jsunity.done();
