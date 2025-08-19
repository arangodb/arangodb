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

function VectorIndexL2FilterTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocs = 500;
    const seed = randomInteger();
    const nProbe = 10;

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
                    unIndexedVector: vector,
                    val: i % 10
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
                    defaultNProbe: nProbe,
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2WithFilterSimple: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " FILTER d.val < 5 " +
                " LET dist = APPROX_NEAR_L2(@qp, d.vector) " +
                " SORT dist LIMIT 5 RETURN {key: d._key, val: d.val, dist}";

            const bindVars = { qp: randomPoint };
            const plan = db
                ._createStatement({ query, bindVars })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            assertEqual(1, indexNodes.length);
            const filterNodes = plan.nodes.filter(function(n) { return n.type === "FilterNode"; });
            assertEqual(0, filterNodes.length);

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val < 5, "Filter not applied correctly: " + JSON.stringify(results));
            }
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].dist <= results[j].dist, "Distances not ascending: " + JSON.stringify(results));
            }
        },

        testApproxL2WithFilterNoMatches: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " FILTER d.val < 0 " +
                " LET dist = APPROX_NEAR_L2(@qp, d.vector) " +
                " SORT dist LIMIT 5 RETURN {key: d._key}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertEqual(0, results.length);
        },

        testApproxL2WithFilterRange: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " FILTER d.val >= 1 AND d.val < 3 " +
                " LET dist = APPROX_NEAR_L2(@qp, d.vector) " +
                " SORT dist LIMIT 10 RETURN {key: d._key, val: d.val, dist}";

            const bindVars = { qp: randomPoint };
            const plan = db
                ._createStatement({ query, bindVars })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            assertEqual(1, indexNodes.length);
            const filterNodes = plan.nodes.filter(function(n) { return n.type === "FilterNode"; });
            assertEqual(0, filterNodes.length);
            

            const results = db._query(query, bindVars).toArray();
            assertEqual(10, results.length);
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val >= 1 && results[i].val < 3, "Filter not applied correctly: " + JSON.stringify(results));
            }
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].dist <= results[j].dist, "Distances not ascending: " + JSON.stringify(results));
            }
        },

        testApproxL2WithFilterComplexConditions: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.val >= 2 AND d.val <= 7 AND d.nonVector % 2 == 0 " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 10 RETURN {key: d._key, val: d.val, nonVector: d.nonVector, dist}";

            const bindVars = { q: randomPoint };
            const plan = db._createStatement({ query, bindVars }).explain().plan;
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            assertEqual(1, indexNodes.length);
            const filterNodes = plan.nodes.filter(function(n) { return n.type === "FilterNode"; });
            assertEqual(0, filterNodes.length);

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 10, "Results should be limited to 10");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val >= 2 && results[i].val <= 7, "Val filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].nonVector % 2 === 0, "Even nonVector filter not applied: " + JSON.stringify(results[i]));
            }
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].dist <= results[j].dist, "Distances not ascending: " + JSON.stringify(results));
            }
        },

        testApproxL2WithFilterStringComparison: function() {
            // Add string field to test string filtering
            const docs = [];
            for (let i = 0; i < 100; ++i) {
                docs.push({
                    vector: Array.from({ length: dimension }, () => Math.random()),
                    val: i % 10,
                    category: i < 50 ? "A" : "B",
                    priority: i % 3
                });
            }
            collection.insert(docs);

            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.category == 'A' AND d.priority > 0 " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 5 RETURN {key: d._key, category: d.category, priority: d.priority, dist}";

            const bindVars = { q: randomPoint };
            const plan = db._createStatement({ query, bindVars }).explain().plan;
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            assertEqual(1, indexNodes.length);

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5, "Results should be limited to 5");
            for (let i = 0; i < results.length; ++i) {
                assertEqual("A", results[i].category, "Category filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].priority > 0, "Priority filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxL2WithFilterNullHandling: function() {
            // Add documents with null values to test null handling
            const docs = [];
            for (let i = 0; i < 50; ++i) {
                docs.push({
                    vector: Array.from({ length: dimension }, () => Math.random()),
                    val: i % 10,
                    nullableField: i % 3 === 0 ? null : i,
                    optionalField: i % 4 === 0 ? undefined : i * 2
                });
            }
            collection.insert(docs);

            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.nullableField != null AND d.optionalField != null " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 10 RETURN {key: d._key, nullableField: d.nullableField, optionalField: d.optionalField, dist}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 10, "Results should be limited to 10");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].nullableField !== null, "Null filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].optionalField !== null, "Undefined filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxL2WithFilterArrayOperations: function() {
            // Add documents with array fields to test array filtering
            const docs = [];
            for (let i = 0; i < 100; ++i) {
                docs.push({
                    vector: Array.from({ length: dimension }, () => Math.random()),
                    val: i % 10,
                    tags: i % 2 === 0 ? ["tag1", "tag2"] : ["tag2", "tag3"],
                    scores: [i % 5, i % 7, i % 11]
                });
            }
            collection.insert(docs);

            const query =
                "FOR d IN " + collection.name() +
                " FILTER 'tag1' IN d.tags AND d.scores[0] > 2 " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 8 RETURN {key: d._key, tags: d.tags, firstScore: d.scores[0], dist}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 8, "Results should be limited to 8");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].tags.includes("tag1"), "Tag filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].firstScore > 2, "Score filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxL2WithFilterRegex: function() {
            // Add documents with string fields for regex testing
            const docs = [];
            for (let i = 0; i < 100; ++i) {
                docs.push({
                    vector: Array.from({ length: dimension }, () => Math.random()),
                    val: i % 10,
                    name: `item_${i}_${i % 3 === 0 ? 'special' : 'normal'}`,
                    description: i % 2 === 0 ? "high priority" : "low priority"
                });
            }
            collection.insert(docs);

            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.name =~ '.*special.*' AND d.description =~ '.*high.*' " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 6 RETURN {key: d._key, name: d.name, description: d.description, dist}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 6, "Results should be limited to 6");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].name.includes("special"), "Name regex filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].description.includes("high"), "Description regex filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxL2WithFilterFunctionCalls: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER LENGTH(TO_STRING(d.val)) == 1 AND d.val > 0 " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 7 RETURN {key: d._key, val: d.val, dist}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 7, "Results should be limited to 7");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val > 0, "Val filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].val < 10, "Length filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxL2WithFilterOrConditions: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.val == 1 OR d.val == 5 OR d.val == 9 " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 15 RETURN {key: d._key, val: d.val, dist}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 15, "Results should be limited to 15");
            for (let i = 0; i < results.length; ++i) {
                assertTrue([1, 5, 9].includes(results[i].val), "OR filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxL2WithFilterNotConditions: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER NOT (d.val < 3 OR d.val > 7) " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 12 RETURN {key: d._key, val: d.val, dist}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 12, "Results should be limited to 12");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val >= 3 && results[i].val <= 7, "NOT filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxL2WithFilterAndVectorSearchOrder: function() {
            // Test that filtering is applied before vector search
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.val >= 4 AND d.val <= 6 " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 5 RETURN {key: d._key, val: d.val, dist}";

            const bindVars = { q: randomPoint };
            const plan = db._createStatement({ query, bindVars }).explain().plan;
            
            // Verify the execution plan has both filter and vector search nodes
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            const filterNodes = plan.nodes.filter(function(n) { return n.type === "FilterNode"; });
            assertEqual(1, indexNodes.length, "Should have vector search node");
            assertEqual(0, filterNodes.length, "Should not have filter node");

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5, "Results should be limited to 5");
            
            // Verify all results satisfy the filter
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val >= 4 && results[i].val <= 6, "Filter not applied: " + JSON.stringify(results[i]));
            }
            
            // Verify results are sorted by distance
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].dist <= results[j].dist, "Distances not ascending: " + JSON.stringify(results));
            }
        },

        testApproxL2WithFilterPerformance: function() {
            // Test that filtering with vector search is efficient
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.val == 5 " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 10 RETURN {key: d._key, val: d.val, dist}";

            const bindVars = { q: randomPoint };
            
            // Measure execution time
            const startTime = Date.now();
            const results = db._query(query, bindVars).toArray();
            const endTime = Date.now();
            const executionTime = endTime - startTime;
            
            // Verify results
            assertTrue(results.length <= 10, "Results should be limited to 10");
            for (let i = 0; i < results.length; ++i) {
                assertEqual(5, results[i].val, "Filter not applied: " + JSON.stringify(results[i]));
            }
            
            // Performance assertion (should complete within reasonable time)
            assertTrue(executionTime < 5000, "Query took too long: " + executionTime + "ms");
        },
    };
}


function VectorIndexCosineFilterTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocs = 1000;
    const seed = 12;
    const nProbeAndNlists = 10;

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
                    unIndexedVector: vector,
                    val: i % 10
                });
            }
            collection.insert(docs);

            collection.ensureIndex({
                name: "vector_cosine",
                type: "vector",
                fields: ["vector"],
                params: {
                    metric: "cosine",
                    dimension: dimension,
                    nLists: nProbeAndNlists,
                    defaultNProbe: nProbeAndNlists,
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxCosineWithFilterSimple: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " FILTER d.nonVector < 100 " +
                " LET sim = APPROX_NEAR_COSINE(@qp, d.vector) " +
                " SORT sim DESC LIMIT 5 RETURN {key: d._key, nonVector: d.nonVector, sim}";

            const bindVars = { qp: randomPoint };
            const plan = db
                ._createStatement({ query, bindVars })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            assertEqual(1, indexNodes.length);

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].nonVector < 100, "Filter not applied correctly: " + JSON.stringify(results));
            }
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].sim >= results[j].sim, "Similarities not descending: " + JSON.stringify(results));
            }
        },

        testApproxCosineWithFilterNoMatches: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " FILTER d.nonVector < 0 " +
                " LET sim = APPROX_NEAR_COSINE(@qp, d.vector) " +
                " SORT sim DESC LIMIT 5 RETURN {key: d._key}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertEqual(0, results.length);
        },

        testApproxCosineWithFilterComplexConditions: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.nonVector >= 100 AND d.nonVector <= 500 AND d.val % 2 == 1 " +
                " LET sim = APPROX_NEAR_COSINE(@qp, d.vector) " +
                " SORT sim DESC LIMIT 8 RETURN {key: d._key, nonVector: d.nonVector, val: d.val, sim}";

            const bindVars = { qp: randomPoint };
            const plan = db._createStatement({ query, bindVars }).explain().plan;
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            assertEqual(1, indexNodes.length);

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 8, "Results should be limited to 8");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].nonVector >= 100 && results[i].nonVector <= 500, "NonVector filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].val % 2 === 1, "Odd val filter not applied: " + JSON.stringify(results[i]));
            }
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].sim >= results[j].sim, "Similarities not descending: " + JSON.stringify(results));
            }
        },

        testApproxCosineWithFilterStringAndNumeric: function() {
            // Add documents with string fields for testing
            const docs = [];
            for (let i = 0; i < 200; ++i) {
                docs.push({
                    vector: Array.from({ length: dimension }, () => Math.random()),
                    nonVector: i,
                    val: i % 10,
                    status: i % 3 === 0 ? "active" : "inactive",
                    rating: i % 5 + 1
                });
            }
            collection.insert(docs);

            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.status == 'active' AND d.rating >= 3 " +
                " LET sim = APPROX_NEAR_COSINE(@qp, d.vector) " +
                " SORT sim DESC LIMIT 6 RETURN {key: d._key, status: d.status, rating: d.rating, sim}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 6, "Results should be limited to 6");
            for (let i = 0; i < results.length; ++i) {
                assertEqual("active", results[i].status, "Status filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].rating >= 3, "Rating filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxCosineWithFilterArrayAndObject: function() {
            // Add documents with array and object fields
            const docs = [];
            for (let i = 0; i < 150; ++i) {
                docs.push({
                    vector: Array.from({ length: dimension }, () => Math.random()),
                    nonVector: i,
                    val: i % 10,
                    permissions: i % 2 === 0 ? ["read", "write"] : ["read"],
                    metadata: {
                        created: i,
                        priority: i % 4 + 1,
                        tags: i % 3 === 0 ? ["urgent"] : ["normal"]
                    }
                });
            }
            collection.insert(docs);

            const query =
                "FOR d IN " + collection.name() +
                " FILTER 'write' IN d.permissions AND d.metadata.priority > 2 " +
                " LET sim = APPROX_NEAR_COSINE(@qp, d.vector) " +
                " SORT sim DESC LIMIT 7 RETURN {key: d._key, permissions: d.permissions, priority: d.metadata.priority, sim}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 7, "Results should be limited to 7");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].permissions.includes("write"), "Permissions filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].priority > 2, "Priority filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxCosineWithFilterRegexAndFunctions: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.nonVector >= 200 AND d.nonVector < 800 " +
                " LET sim = APPROX_NEAR_COSINE(@qp, d.vector) " +
                " SORT sim DESC LIMIT 9 RETURN {key: d._key, nonVector: d.nonVector, sim}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 9, "Results should be limited to 9");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].nonVector >= 200 && results[i].nonVector < 800, "Range filter not applied: " + JSON.stringify(results[i]));
            }
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].sim >= results[j].sim, "Similarities not descending: " + JSON.stringify(results));
            }
        },

        testApproxCosineWithFilterOrLogic: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.val == 2 OR d.val == 6 OR d.val == 8 " +
                " LET sim = APPROX_NEAR_COSINE(@qp, d.vector) " +
                " SORT sim DESC LIMIT 12 RETURN {key: d._key, val: d.val, sim}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 12, "Results should be limited to 12");
            for (let i = 0; i < results.length; ++i) {
                assertTrue([2, 6, 8].includes(results[i].val), "OR filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxCosineWithFilterNotLogic: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER NOT (d.val < 2 OR d.val > 8) " +
                " LET sim = APPROX_NEAR_COSINE(@qp, d.vector) " +
                " SORT sim DESC LIMIT 10 RETURN {key: d._key, val: d.val, sim}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 10, "Results should be limited to 10");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val >= 2 && results[i].val <= 8, "NOT filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxCosineWithFilterExecutionOrder: function() {
            // Test that filtering is applied before vector search
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.val >= 3 AND d.val <= 7 " +
                " LET sim = APPROX_NEAR_COSINE(@qp, d.vector) " +
                " SORT sim DESC LIMIT 6 RETURN {key: d._key, val: d.val, sim}";

            const bindVars = { qp: randomPoint };
            const plan = db._createStatement({ query, bindVars }).explain().plan;
            
            // Verify the execution plan has both filter and vector search nodes
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            const filterNodes = plan.nodes.filter(function(n) { return n.type === "FilterNode"; });
            assertEqual(1, indexNodes.length, "Should have vector search node");
            assertEqual(0, filterNodes.length, "Should not have filter node");

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 6, "Results should be limited to 6");
            
            // Verify all results satisfy the filter
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val >= 3 && results[i].val <= 7, "Filter not applied: " + JSON.stringify(results[i]));
            }
            
            // Verify results are sorted by similarity (descending)
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].sim >= results[j].sim, "Similarities not descending: " + JSON.stringify(results));
            }
        },
    };
}

function VectorIndexInnerProductFilterTestSuite() {
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
                    unIndexedVector: vector,
                    val: i % 10
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

        testApproxInnerProductWithFilterSimple: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " FILTER d.nonVector >= 100 AND d.nonVector < 300 " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) " +
                " SORT sim DESC LIMIT 7 RETURN {key: d._key, nonVector: d.nonVector, sim}";

            const bindVars = { qp: randomPoint };
            const plan = db
                ._createStatement({ query, bindVars })
                .explain().plan;
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            assertEqual(1, indexNodes.length);

            const results = db._query(query, bindVars).toArray();
            assertEqual(7, results.length);
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].nonVector >= 100 && results[i].nonVector < 300, "Filter not applied correctly: " + JSON.stringify(results));
            }
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].sim >= results[j].sim, "Similarities not descending: " + JSON.stringify(results));
            }
        },

        testApproxInnerProductWithFilterNoMatches: function() {
            const query =
                "FOR d IN " +
                collection.name() +
                " FILTER d.nonVector < 0 " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) " +
                " SORT sim DESC LIMIT 5 RETURN {key: d._key}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertEqual(0, results.length);
        },

        testApproxInnerProductWithFilterComplexConditions: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.nonVector >= 200 AND d.nonVector <= 700 AND d.val % 3 == 0 " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) " +
                " SORT sim DESC LIMIT 9 RETURN {key: d._key, nonVector: d.nonVector, val: d.val, sim}";

            const bindVars = { qp: randomPoint };
            const plan = db._createStatement({ query, bindVars }).explain().plan;
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            assertEqual(1, indexNodes.length);

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 9, "Results should be limited to 9");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].nonVector >= 200 && results[i].nonVector <= 700, "NonVector filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].val % 3 === 0, "Divisible by 3 filter not applied: " + JSON.stringify(results[i]));
            }
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].sim >= results[j].sim, "Similarities not descending: " + JSON.stringify(results));
            }
        },

        testApproxInnerProductWithFilterStringAndNumeric: function() {
            // Add documents with string fields for testing
            const docs = [];
            for (let i = 0; i < 300; ++i) {
                docs.push({
                    vector: Array.from({ length: dimension }, () => Math.random()),
                    nonVector: i,
                    val: i % 10,
                    type: i % 4 === 0 ? "premium" : "standard",
                    level: i % 6 + 1
                });
            }
            collection.insert(docs);

            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.type == 'premium' AND d.level >= 4 " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) " +
                " SORT sim DESC LIMIT 8 RETURN {key: d._key, type: d.type, level: d.level, sim}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 8, "Results should be limited to 8");
            for (let i = 0; i < results.length; ++i) {
                assertEqual("premium", results[i].type, "Type filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].level >= 4, "Level filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxInnerProductWithFilterArrayAndObject: function() {
            // Add documents with array and object fields
            const docs = [];
            for (let i = 0; i < 250; ++i) {
                docs.push({
                    vector: Array.from({ length: dimension }, () => Math.random()),
                    nonVector: i,
                    val: i % 10,
                    features: i % 2 === 0 ? ["feature1", "feature2", "feature3"] : ["feature2"],
                    config: {
                        enabled: i % 3 !== 0,
                        version: i % 5 + 1,
                        flags: i % 2 === 0 ? ["flag1", "flag2"] : ["flag1"]
                    }
                });
            }
            collection.insert(docs);

            const query =
                "FOR d IN " + collection.name() +
                " FILTER 'feature3' IN d.features AND d.config.enabled == true " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) " +
                " SORT sim DESC LIMIT 10 RETURN {key: d._key, features: d.features, enabled: d.config.enabled, sim}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 10, "Results should be limited to 10");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].features.includes("feature3"), "Features filter not applied: " + JSON.stringify(results[i]));
                assertTrue(results[i].enabled === true, "Enabled filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxInnerProductWithFilterRangeAndModulo: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.nonVector >= 300 AND d.nonVector < 900 " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) " +
                " SORT sim DESC LIMIT 11 RETURN {key: d._key, nonVector: d.nonVector, sim}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 11, "Results should be limited to 11");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].nonVector >= 300 && results[i].nonVector < 900, "Range filter not applied: " + JSON.stringify(results[i]));
            }
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].sim >= results[j].sim, "Similarities not descending: " + JSON.stringify(results));
            }
        },

        testApproxInnerProductWithFilterOrLogic: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.val == 0 OR d.val == 4 OR d.val == 7 " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) " +
                " SORT sim DESC LIMIT 14 RETURN {key: d._key, val: d.val, sim}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 14, "Results should be limited to 14");
            for (let i = 0; i < results.length; ++i) {
                assertTrue([0, 4, 7].includes(results[i].val), "OR filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxInnerProductWithFilterNotLogic: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER NOT (d.val < 1 OR d.val > 9) " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) " +
                " SORT sim DESC LIMIT 13 RETURN {key: d._key, val: d.val, sim}";

            const bindVars = { qp: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 13, "Results should be limited to 13");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val >= 1 && results[i].val <= 9, "NOT filter not applied: " + JSON.stringify(results[i]));
            }
        },

        testApproxInnerProductWithFilterExecutionOrder: function() {
            // Test that filtering is applied before vector search
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.val >= 2 AND d.val <= 8 " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) " +
                " SORT sim DESC LIMIT 7 RETURN {key: d._key, val: d.val, sim}";

            const bindVars = { qp: randomPoint };
            const plan = db._createStatement({ query, bindVars }).explain().plan;
            
            // Verify the execution plan has both filter and vector search nodes
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            const filterNodes = plan.nodes.filter(function(n) { return n.type === "FilterNode"; });
            assertEqual(1, indexNodes.length, "Should have vector search node");
            assertEqual(1, filterNodes.length, "Should have filter node");

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 7, "Results should be limited to 7");
            
            // Verify all results satisfy the filter
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val >= 2 && results[i].val <= 8, "Filter not applied: " + JSON.stringify(results[i]));
            }
            
            // Verify results are sorted by similarity (descending)
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].sim >= results[j].sim, "Similarities not descending: " + JSON.stringify(results));
            }
        },

        testApproxInnerProductWithFilterPerformance: function() {
            // Test that filtering with vector search is efficient
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.val == 3 " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@qp, d.vector) " +
                " SORT sim DESC LIMIT 12 RETURN {key: d._key, val: d.val, sim}";

            const bindVars = { qp: randomPoint };
            
            // Measure execution time
            const startTime = Date.now();
            const results = db._query(query, bindVars).toArray();
            const endTime = Date.now();
            const executionTime = endTime - startTime;
            
            // Verify results
            assertTrue(results.length <= 12, "Results should be limited to 12");
            for (let i = 0; i < results.length; ++i) {
                assertEqual(3, results[i].val, "Filter not applied: " + JSON.stringify(results[i]));
            }
            
            // Performance assertion (should complete within reasonable time)
            assertTrue(executionTime < 5000, "Query took too long: " + executionTime + "ms");
        },
    };
}

function AdvancedVectorFilteringTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 500;
    const numberOfDocs = 1000;
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
                
                // Create diverse document structure for comprehensive filtering tests
                docs.push({
                    vector,
                    id: i,
                    category: i % 5 === 0 ? "premium" : i % 3 === 0 ? "standard" : "basic",
                    priority: i % 10 + 1,
                    tags: i % 2 === 0 ? ["tag1", "tag2", "tag3"] : ["tag2", "tag4"],
                    metadata: {
                        created: i,
                        updated: i + 100,
                        status: i % 4 === 0 ? "active" : i % 3 === 0 ? "pending" : "inactive",
                        flags: i % 2 === 0 ? ["flag1", "flag2"] : ["flag1"],
                        score: i % 100 + 1
                    },
                    attributes: {
                        size: i % 3 === 0 ? "small" : i % 2 === 0 ? "medium" : "large",
                        color: i % 4 === 0 ? "red" : i % 3 === 0 ? "blue" : i % 2 === 0 ? "green" : "yellow",
                        weight: i % 50 + 1
                    },
                    arrayField: [i % 5, i % 7, i % 11],
                    numericField: i % 20,
                    booleanField: i % 2 === 0,
                    nullField: i % 7 === 0 ? null : i,
                    val: i % 10
                });
            }
            collection.insert(docs);

            // Create multiple vector indexes for different metrics
            collection.ensureIndex({
                name: "vector_l2_advanced",
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

            collection.ensureIndex({
                name: "vector_cosine_advanced",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "cosine",
                    dimension: dimension,
                    nLists: 10,
                },
            });

            collection.ensureIndex({
                name: "vector_inner_product_advanced",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "innerProduct",
                    dimension: dimension,
                    nLists: 10,
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testComplexFilteringWithL2: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.category == 'premium' " +
                " AND d.priority >= 5 " +
                " AND d.metadata.status == 'active' " +
                " AND 'tag1' IN d.tags " +
                " AND d.attributes.size != 'small' " +
                " AND d.numericField % 2 == 0 " +
                " AND d.booleanField == true " +
                " AND d.nullField != null " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 8 " +
                " RETURN {key: d._key, category: d.category, priority: d.priority, status: d.metadata.status, dist}";

            const bindVars = { q: randomPoint };
            const plan = db._createStatement({ query, bindVars }).explain().plan;
            
            // Verify execution plan
            const indexNodes = plan.nodes.filter(function(n) { return n.type === "EnumerateNearVectorNode"; });
            const filterNodes = plan.nodes.filter(function(n) { return n.type === "FilterNode"; });
            assertEqual(1, indexNodes.length, "Should have vector search node");
            assertEqual(0, filterNodes.length, "Should not have filter node");

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 8, "Results should be limited to 8");
            
            // Verify all filters are applied
            for (let i = 0; i < results.length; ++i) {
                assertEqual("premium", results[i].category, "Category filter not applied");
                assertTrue(results[i].priority >= 5, "Priority filter not applied");
                assertEqual("active", results[i].status, "Status filter not applied");
            }
            
            // Verify results are sorted by distance
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].dist <= results[j].dist, "Distances not ascending");
            }
        },

        testComplexFilteringWithCosine: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.category IN ['standard', 'premium'] " +
                " AND d.priority > 3 " +
                " AND d.metadata.score >= 50 " +
                " AND 'flag2' IN d.metadata.flags " +
                " AND d.attributes.color IN ['red', 'blue'] " +
                " AND d.arrayField[0] > 2 " +
                " LET sim = APPROX_NEAR_COSINE(@q, d.vector) " +
                " SORT sim DESC LIMIT 10 " +
                " RETURN {key: d._key, category: d.category, priority: d.priority, score: d.metadata.score, sim}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 10, "Results should be limited to 10");
            
            // Verify all filters are applied
            for (let i = 0; i < results.length; ++i) {
                assertTrue(["standard", "premium"].includes(results[i].category), "Category filter not applied");
                assertTrue(results[i].priority > 3, "Priority filter not applied");
                assertTrue(results[i].score >= 50, "Score filter not applied");
            }
            
            // Verify results are sorted by similarity (descending)
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].sim >= results[j].sim, "Similarities not descending");
            }
        },

        testComplexFilteringWithInnerProduct: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.category == 'basic' " +
                " AND d.priority <= 5 " +
                " AND d.metadata.status != 'inactive' " +
                " AND d.attributes.weight < 30 " +
                " AND d.numericField % 3 == 0 " +
                " AND d.booleanField == false " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@q, d.vector) " +
                " SORT sim DESC LIMIT 6 " +
                " RETURN {key: d._key, category: d.category, priority: d.priority, weight: d.attributes.weight, sim}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 6, "Results should be limited to 6");
            
            // Verify all filters are applied
            for (let i = 0; i < results.length; ++i) {
                assertEqual("basic", results[i].category, "Category filter not applied");
                assertTrue(results[i].priority <= 5, "Priority filter not applied");
                assertTrue(results[i].weight < 30, "Weight filter not applied");
            }
            
            // Verify results are sorted by similarity (descending)
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].sim >= results[j].sim, "Similarities not descending");
            }
        },

        testNestedObjectFiltering: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.metadata.flags[*] == 'flag1' " +
                " AND d.attributes.size == 'medium' " +
                " AND d.metadata.updated > 150 " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 7 " +
                " RETURN {key: d._key, flags: d.metadata.flags, size: d.attributes.size, updated: d.metadata.updated, dist}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 7, "Results should be limited to 7");
            
            // Verify all filters are applied
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].flags.includes("flag1"), "Flags filter not applied");
                assertEqual("medium", results[i].size, "Size filter not applied");
                assertTrue(results[i].updated > 150, "Updated filter not applied");
            }
        },

        testArrayAndRegexFiltering: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.tags[*] =~ 'tag[24]' " +
                " AND d.attributes.color =~ '^(red|green)$' " +
                " AND d.metadata.status =~ '.*active.*' " +
                " LET sim = APPROX_NEAR_COSINE(@q, d.vector) " +
                " SORT sim DESC LIMIT 9 " +
                " RETURN {key: d._key, tags: d.tags, color: d.attributes.color, status: d.metadata.status, sim}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 9, "Results should be limited to 9");
            
            // Verify all filters are applied
            for (let i = 0; i < results.length; ++i) {
                // Check that at least one tag matches the pattern
                const hasMatchingTag = results[i].tags.some(tag => /tag[24]/.test(tag));
                assertTrue(hasMatchingTag, "Tags regex filter not applied");
                
                assertTrue(["red", "green"].includes(results[i].color), "Color regex filter not applied");
                assertTrue(results[i].status.includes("active"), "Status regex filter not applied");
            }
        },

        testFunctionBasedFiltering: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER LENGTH(d.tags) >= 2 " +
                " AND LENGTH(d.metadata.flags) == 2 " +
                " AND d.metadata.score > AVERAGE([d.metadata.score, d.priority, d.attributes.weight]) " +
                " LET sim = APPROX_NEAR_INNER_PRODUCT(@q, d.vector) " +
                " SORT sim DESC LIMIT 8 " +
                " RETURN {key: d._key, tagsLength: LENGTH(d.tags), flagsLength: LENGTH(d.metadata.flags), score: d.metadata.score, sim}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 8, "Results should be limited to 8");
            
            // Verify all filters are applied
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].tagsLength >= 2, "Tags length filter not applied");
                assertEqual(2, results[i].flagsLength, "Flags length filter not applied");
            }
        },

        testCombinedMetricFiltering: function() {
            // Test filtering with different vector metrics in the same query
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.category == 'premium' " +
                " AND d.priority >= 7 " +
                " LET l2_dist = APPROX_NEAR_L2(@q, d.vector) " +
                " LET cosine_sim = APPROX_NEAR_COSINE(@q, d.vector) " +
                " LET inner_sim = APPROX_NEAR_INNER_PRODUCT(@q, d.vector) " +
                " SORT l2_dist LIMIT 5 " +
                " RETURN {key: d._key, category: d.category, priority: d.priority, l2_dist, cosine_sim, inner_sim}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5, "Results should be limited to 5");
            
            // Verify all filters are applied
            for (let i = 0; i < results.length; ++i) {
                assertEqual("premium", results[i].category, "Category filter not applied");
                assertTrue(results[i].priority >= 7, "Priority filter not applied");
            }
            
            // Verify results are sorted by L2 distance
            for (let j = 1; j < results.length; ++j) {
                assertTrue(results[j - 1].l2_dist <= results[j].l2_dist, "L2 distances not ascending");
            }
        },

        testFilteringWithSubqueries: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.category == 'standard' " +
                " LET neighbours = (FOR doc IN " + collection.name() +
                "   FILTER doc.category == 'basic' " +
                "   AND doc.priority <= 3 " +
                "   LET dist = APPROX_NEAR_L2(doc.vector, d.vector) " +
                "   SORT dist LIMIT 3 " +
                "   RETURN {key: doc._key, category: doc.category, priority: doc.priority, dist }) " +
                " FILTER LENGTH(neighbours) > 0 " +
                " LET sim = APPROX_NEAR_COSINE(@q, d.vector) " +
                " SORT sim DESC LIMIT 6 " +
                " RETURN {key: d._key, category: d.category, neighbours: neighbours, sim}";

            const bindVars = { q: randomPoint };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 6, "Results should be limited to 6");
            
            // Verify all filters are applied
            for (let i = 0; i < results.length; ++i) {
                assertEqual("standard", results[i].category, "Category filter not applied");
                assertTrue(results[i].neighbours.length > 0, "Neighbours filter not applied");
                
                // Verify subquery filters
                for (let j = 0; j < results[i].neighbours.length; ++j) {
                    const neighbour = results[i].neighbours[j];
                    assertEqual("basic", neighbour.category, "Subquery category filter not applied");
                    assertTrue(neighbour.priority <= 3, "Subquery priority filter not applied");
                }
            }
        },

        testFilteringPerformanceWithComplexQueries: function() {
            const query =
                "FOR d IN " + collection.name() +
                " FILTER d.category IN ['premium', 'standard'] " +
                " AND d.priority >= 5 " +
                " AND d.metadata.status == 'active' " +
                " AND 'tag1' IN d.tags " +
                " AND d.attributes.size != 'small' " +
                " AND d.numericField % 2 == 0 " +
                " AND d.booleanField == true " +
                " AND d.nullField != null " +
                " AND d.metadata.score >= 30 " +
                " AND 'flag1' IN d.metadata.flags " +
                " AND d.attributes.color IN ['red', 'blue', 'green'] " +
                " AND d.arrayField[0] > 1 " +
                " LET dist = APPROX_NEAR_L2(@q, d.vector) " +
                " SORT dist LIMIT 10 " +
                " RETURN {key: d._key, category: d.category, priority: d.priority, score: d.metadata.score, dist}";

            const bindVars = { q: randomPoint };
            
            // Measure execution time
            const startTime = Date.now();
            const results = db._query(query, bindVars).toArray();
            const endTime = Date.now();
            const executionTime = endTime - startTime;
            
            // Verify results
            assertTrue(results.length <= 10, "Results should be limited to 10");
            
            // Verify all filters are applied
            for (let i = 0; i < results.length; ++i) {
                assertTrue(["premium", "standard"].includes(results[i].category), "Category filter not applied");
                assertTrue(results[i].priority >= 5, "Priority filter not applied");
                assertTrue(results[i].score >= 30, "Score filter not applied");
            }
            
            // Performance assertion (should complete within reasonable time)
            assertTrue(executionTime < 10000, "Complex query took too long: " + executionTime + "ms");
        },
    };
}

jsunity.run(VectorIndexL2FilterTestSuite);
jsunity.run(VectorIndexCosineFilterTestSuite);
jsunity.run(VectorIndexInnerProductFilterTestSuite);

return jsunity.done();
