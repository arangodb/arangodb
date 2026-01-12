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
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;
const db = internal.db;
const {
    randomNumberGeneratorFloat,
    randomInteger,
} = require("@arangodb/testutils/seededRandom");
const dbName = "vectorDb";
const collName = "vectorColl";

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper functions for verification
////////////////////////////////////////////////////////////////////////////////

const verifyVectorIndexUsed = function(plan) {
    const indexNodes = plan.nodes.filter(function(n) {
        return n.type === "EnumerateNearVectorNode";
    });
    assertEqual(1, indexNodes.length, "Expected vector index to be used");
};

const verifyNoFilterNodes = function(plan) {
    const filterNodes = plan.nodes.filter(function(n) {
        return n.type === "FilterNode";
    });
    assertEqual(0, filterNodes.length, "Expected no separate filter nodes (filters should be absorbed)");
};

const verifyCalculationNodeCount = function(plan, expectedCount, message) {
    const calculationNodes = plan.nodes.filter(function(n) {
        return n.type === "CalculationNode";
    });
    assertEqual(expectedCount, calculationNodes.length, message || (`Expected ${expectedCount} calculation nodes`));
};

const verifyResultsMatchFilter = function(results, filterFn, message) {
    for (let i = 0; i < results.length; ++i) {
        assertTrue(filterFn(results[i]), message || "Result should match filter condition");
    }
};

const verifyDistancesAscending = function(results) {
    for (let j = 1; j < results.length; ++j) {
        assertTrue(results[j - 1].dist <= results[j].dist, `Distances not ascending: ${JSON.stringify(results)}`);
    }
};

const verifyPlan = function(query, bindVars, numberOfCalculationNodes) {
    const plan = bindVars ?
        db._createStatement({
            query,
            bindVars
        }).explain().plan :
        db._createStatement(query).explain().plan;

    verifyVectorIndexUsed(plan);
    verifyNoFilterNodes(plan);
    if (numberOfCalculationNodes !== undefined) {
        verifyCalculationNodeCount(plan, numberOfCalculationNodes);
    }

    return plan;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexL2FilterTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 20;
    const numberOfDocs = 500;
    const seed = randomInteger();
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

            // Create all documents in a single loop
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
                    val: i,
                    // Fields for string comparison tests
                    category: i < 250 ? "A" : "B",
                    priority: i % 3,
                    // Fields for null handling tests
                    nullableField: i % 3 === 0 ? null : i,
                    optionalField: i % 4 === 0 ? undefined : i * 2,
                    // Fields for array operations tests
                    tags: i % 2 === 0 ? ["tag1", "tag2"] : ["tag2", "tag3"],
                    scores: [i % 5, i % 7, i % 11],
                    // Fields for regex testing
                    name: `item_${i}_${i % 3 === 0 ? 'special' : 'normal'}`,
                    description: i % 2 === 0 ? "high priority" : "low priority"
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
                    nLists: nProbeAndNlists,
                    trainingIterations: 10,
                    defaultNProbe: nProbeAndNlists,
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2WithFilterSimple: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val < 5
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5 
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            verifyPlan(query, bindVars, 2);

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);

            verifyResultsMatchFilter(results, (r) => r.val < 5, "Filter not applied correctly");
            verifyDistancesAscending(results);
        },

        testApproxL2WithFilterNotEnoughDocuments: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val < 5
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10 
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            verifyPlan(query, bindVars, 2);

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length, JSON.stringify(results));

            verifyResultsMatchFilter(results, (r) => r.val < 5, "Filter not applied correctly");
        },

        testApproxL2WithDoubleLoop: function() {
            const query = aql`
              FOR docOuter IN ${collection}
              FOR docInner IN ${collection}
              FILTER docInner.val > 3
              SORT APPROX_NEAR_L2(docInner.vector, docOuter.vector)
              LIMIT 5 RETURN docInner`;

            verifyPlan(query, undefined, 0);

            const results = db._query(query).toArray();
            assertEqual(5, results.length);

            verifyResultsMatchFilter(results, (r) => r.val > 3, "Filter not applied correctly");
        },

        testApproxL2WithDoubleLoopNotEnoughDocuments: function() {
            const query = aql`
              FOR docOuter IN ${collection}
              FOR docInner IN ${collection}
              FILTER docInner.val < 3
              SORT APPROX_NEAR_L2(docInner.vector, docOuter.vector)
              LIMIT 6 RETURN {key: docInner._key, val: docInner.val}`;

            verifyPlan(query, undefined, 1);

            const results = db._query(query).toArray();
            assertEqual(6, results.length, `Inccorect number of results ${JSON.stringify(results)}`);

            verifyResultsMatchFilter(results, (r) => r.val < 3, "Filter not applied correctly");
        },

        testApproxL2WithFilterNoMatches: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val < 0
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5 
              RETURN {key: d._key}`;

            const bindVars = {
                qp: randomPoint
            };
            const results = db._query(query, bindVars).toArray();
            assertEqual(0, results.length);
        },

        testApproxL2WithFilterNonExistentField: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.nonExistentField == 'someValue'
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5 
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            verifyPlan(query, bindVars);

            const results = db._query(query, bindVars).toArray();
            assertEqual(0, results.length, "Should return no results when filtering on non-existent field");
        },

        testApproxL2WithFilterRange: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val >= 1 AND d.val < 30
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10 
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            verifyPlan(query, bindVars, 2);

            const results = db._query(query, bindVars).toArray();
            assertEqual(10, results.length);

            verifyResultsMatchFilter(results, (r) => r.val >= 1 && r.val < 30, "Filter not applied correctly");
            verifyDistancesAscending(results);
        },

        testApproxL2WithFilterComplexConditions: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val >= 2 AND d.val <= 7 AND d.nonVector % 2 == 0
              LET dist = APPROX_NEAR_L2(@q, d.vector)
              SORT dist LIMIT 10 
              RETURN {key: d._key, val: d.val, nonVector: d.nonVector, dist}`;

            const bindVars = {
                q: randomPoint
            };
            verifyPlan(query, bindVars, 2);

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 10, "Results should be limited to 10");

            verifyResultsMatchFilter(results, (r) => r.val >= 2 && r.val <= 7 && r.nonVector % 2 === 0, "Filter not applied correctly");
            verifyDistancesAscending(results);
        },

        testApproxL2WithFilterStringComparison: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.category == 'A' AND d.priority > 0
              LET dist = APPROX_NEAR_L2(@q, d.vector)
              SORT dist LIMIT 5 
              RETURN {key: d._key, category: d.category, priority: d.priority, dist}`;

            const bindVars = {
                q: randomPoint
            };
            verifyPlan(query, bindVars);

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5, "Results should be limited to 5");

            verifyResultsMatchFilter(results, (r) => r.category === 'A' && r.priority > 0, "Filter not applied correctly");
        },

        testApproxL2WithFilterNullHandling: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.nullableField != null AND d.optionalField != null
              LET dist = APPROX_NEAR_L2(@q, d.vector)
              SORT dist LIMIT 10 
              RETURN {key: d._key, nullableField: d.nullableField, optionalField: d.optionalField, dist}`;

            const bindVars = {
                q: randomPoint
            };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 10, "Results should be limited to 10");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].nullableField !== null, `Null filter not applied: ${JSON.stringify(results[i])}`);
                assertTrue(results[i].optionalField !== null, `Undefined filter not applied: ${JSON.stringify(results[i])}`);
            }
        },

        testApproxL2WithFilterArrayOperations: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER 'tag1' IN d.tags AND d.scores[0] > 2
              LET dist = APPROX_NEAR_L2(@q, d.vector)
              SORT dist LIMIT 8 
              RETURN {key: d._key, tags: d.tags, firstScore: d.scores[0], dist}`;

            const bindVars = {
                q: randomPoint
            };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 8, "Results should be limited to 8");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].tags.includes("tag1"), `Tag filter not applied: ${JSON.stringify(results[i])}`);
                assertTrue(results[i].firstScore > 2, `Score filter not applied: ${JSON.stringify(results[i])}`);
            }
        },

        testApproxL2WithFilterRegex: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.name =~ '.*special.*' AND d.description =~ '.*high.*'
              LET dist = APPROX_NEAR_L2(@q, d.vector)
              SORT dist LIMIT 6 
              RETURN {key: d._key, name: d.name, description: d.description, dist}`;

            const bindVars = {
                q: randomPoint
            };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 6, "Results should be limited to 6");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].name.includes("special"), `Name regex filter not applied: ${JSON.stringify(results[i])}`);
                assertTrue(results[i].description.includes("high"), `Description regex filter not applied: ${JSON.stringify(results[i])}`);
            }
        },

        testApproxL2WithFilterFunctionCalls: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER LENGTH(TO_STRING(d.val)) == 1 AND d.val > 0
              LET dist = APPROX_NEAR_L2(@q, d.vector)
              SORT dist LIMIT 7 
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                q: randomPoint
            };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 7, "Results should be limited to 7");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val > 0, `Val filter not applied: ${JSON.stringify(results[i])}`);
                assertTrue(results[i].val < 10, `Length filter not applied: ${JSON.stringify(results[i])}`);
            }
        },

        testApproxL2WithFilterOrConditions: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val == 1 OR d.val == 5 OR d.val == 9
              LET dist = APPROX_NEAR_L2(@q, d.vector)
              SORT dist LIMIT 15 
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                q: randomPoint
            };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 15, "Results should be limited to 15");
            for (let i = 0; i < results.length; ++i) {
                assertTrue([1, 5, 9].includes(results[i].val), `OR filter not applied: ${JSON.stringify(results[i])}`);
            }
        },

        testApproxL2WithFilterNotConditions: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER NOT (d.val < 3 OR d.val > 7)
              LET dist = APPROX_NEAR_L2(@q, d.vector)
              SORT dist LIMIT 12 
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                q: randomPoint
            };
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 12, "Results should be limited to 12");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val >= 3 && results[i].val <= 7, `NOT filter not applied: ${JSON.stringify(results[i])}`);
            }
        },

        testApproxL2WithFilterAndVectorSearchOrder: function() {
            // Test that filtering is applied before vector search
            const query = `FOR d IN ${collection.name()}
              FILTER d.val >= 4 AND d.val <= 6
              LET dist = APPROX_NEAR_L2(@q, d.vector)
              SORT dist LIMIT 5 
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                q: randomPoint
            };
            verifyPlan(query, bindVars, 2);

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
            verifyResultsMatchFilter(results, (r) => r.val >= 4 && r.val <= 6, "Filter not applied");
            verifyDistancesAscending(results);
        },

        testApproxL2WithExplicitCalculationNodes: function() {
            const query = `FOR d IN ${collection.name()}
              LET cond = d.val < 10
              FILTER cond
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            verifyPlan(query, bindVars, 2);

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);

            verifyResultsMatchFilter(results, (r) => r.val < 10);
        },

        testApproxL2WithCalculationReusedInReturn: function() {
            // Test where calculation is used both in FILTER and in RETURN
            const query = `FOR d IN ${collection.name()}
              LET cond = d.val < 10
              FILTER cond
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {key: d._key, val: d.val, cond1: NOOPT(cond), dist, cond2: NOOPT(cond)}`;

            const bindVars = {
                qp: randomPoint
            };
            verifyPlan(query, bindVars, 3);

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);

            verifyResultsMatchFilter(results, (r) => r.val < 10 && r.cond1 === true && r.cond2 === true);
        },

        testApproxL2CalculationAfterFilter: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val >= 5
              LET a = 2 * d.val
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {key: d._key, val: d.val, dist, newVar: a}`;

            const bindVars = {
                qp: randomPoint
            };
            verifyPlan(query, bindVars);

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);

            verifyResultsMatchFilter(results, (r) => r.val >= 5 && r.newVar === 2 * r.val);
        },

        testApproxL2WithFilterAfterVectorSearch: function() {
            const query = `FOR d IN ${collection.name()}
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              FILTER d.val >= 5
              SORT dist LIMIT 5
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            verifyPlan(query, bindVars);

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);

            verifyResultsMatchFilter(results, (r) => r.val >= 5);
        },

        testApproxL2WithNestedLoopFilterInOuter: function() {
            const query = `FOR x IN ${collection.name()}
              FILTER x.val >= 5
              FOR d IN ${collection.name()}
                LET dist = APPROX_NEAR_L2(@qp, d.vector)
                SORT dist LIMIT 5
                RETURN {outerKey: x._key, key: d._key, val: d.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            const plan = db._createStatement({
                query,
                bindVars
            }).explain().plan;
            verifyVectorIndexUsed(plan);

            const results = db._query(query, bindVars).toArray();

            // Each outer loop iteration returns 5 results from the inner vector search
            assertTrue(results.length > 0);
            assertTrue(results.length % 5 === 0 || results.length >= 5);

            verifyResultsMatchFilter(results, (r) => r.outerKey !== undefined);
        },

        testApproxL2WithMultipleFilterClauses: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val >= 5
              FILTER d.val < 15
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query,
                bindVars,
            );
        },

        testApproxL2WithMultipleFiltersAndLetStatements: function() {
            const query = `FOR d IN ${collection.name()}
              LET minVal = 5
              FILTER d.val >= minVal
              LET maxVal = 15
              FILTER d.val < maxVal
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {key: d._key, val: d.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query,
                bindVars,
            );
        },

        testApproxL2WithFilterOnDistance: function() {
            const query = `FOR d IN ${collection.name()}
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              FILTER dist < 0.5
              SORT dist LIMIT 5
              RETURN d  `;

            const bindVars = {
                qp: randomPoint
            };
            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query,
                bindVars,
            );
        },
    };
}

function VectorIndexL2FilterTestMultipleCollectionsSuite() {
    let collection1;
    let collection2;
    let randomPoint;
    const dimension = 20;
    const numberOfDocs = 500;
    const seed = randomInteger();
    const nProbeAndNlists = 10;
    const col2 = "col2";

    return {
        setUpAll: function() {
            print(`Using seed: ${seed}`);
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection1 = db._create(collName, {
                numberOfShards: 3
            });
            collection2 = db._create(col2, {
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
                    val: i
                });
            }
            collection1.insert(docs);
            collection2.insert(docs);

            collection1.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: nProbeAndNlists,
                    trainingIterations: 10,
                    defaultNProbe: nProbeAndNlists,
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2FilterNotApplied: function() {
            const query = `FOR d1 IN ${collection2.name()}
              FOR d2 IN ${collection1.name()}
              FILTER d2.val < 5 OR d1.val < 5
              LET dist = APPROX_NEAR_L2(@qp, d2.vector)
              SORT dist LIMIT 5 
              RETURN {key: d2._key, val: d2.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };

            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query,
                bindVars,
            );
        },

        testApproxL2FilterNotAppliedReversedLoop: function() {
            // Test with reversed loops - vector search collection in outer loop
            const query = `FOR d2 IN ${collection1.name()}
              FOR d1 IN ${collection2.name()}
              FILTER d2.val < 5 OR d1.val < 5
              LET dist = APPROX_NEAR_L2(@qp, d1.vector)
              SORT dist LIMIT 5 
              RETURN {key: d1._key, val: d1.val, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            assertQueryError(
                errors.ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED.code,
                query,
                bindVars,
            );
        },
    };
}

function VectorIndexL2FilterStoredValuesTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 20;
    const numberOfDocs = 500;
    const seed = randomInteger();
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
                    val: i,
                    nonStoredValue: i * 2,
                    stringField: i % 3 === 0 ? "type_A" : (i % 3 === 1 ? "type_B" : "type_C"),
                    boolField: i % 2 === 0,
                    arrayField: [i % 5, i % 7],
                    objectField: {
                        nested: i % 4,
                        category: i < numberOfDocs / 2 ? "first_half" : "second_half"
                    },
                    floatField: i + 0.5
                });
            }
            collection.insert(docs);

            collection.ensureIndex({
                name: "vector_l2_stored_values",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension: dimension,
                    nLists: nProbeAndNlists,
                    trainingIterations: 10,
                    defaultNProbe: nProbeAndNlists,
                },
                storedValues: ["val", "stringField", "boolField", "floatField"]
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testApproxL2WithFilterStoredValuesSimple: function() {
            const query = `
                FOR d IN ${collection.name()}
                FILTER d.val < 5 AND d.stringField == 'type_A'
                LET dist = APPROX_NEAR_L2(@qp, d.vector)
                SORT dist LIMIT 5
                RETURN {key: d._key, val: d.val, stringField: d.stringField, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            
            const plan = verifyPlan(query, bindVars);
            const indexNodes = plan.nodes.filter(n => n.type === "EnumerateNearVectorNode");
            assertTrue(indexNodes[0].isCoveredByStoredValues);

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5, "Results should be limited to 5");
            
            verifyResultsMatchFilter(results, r => r.val < 5, "Val filter not applied correctly");
            verifyResultsMatchFilter(results, r => r.stringField === "type_A", "String filter not applied correctly");
            verifyDistancesAscending(results);
        },

        testApproxL2WithFilterCannotUseStoredValues: function() {
            const query = `
                FOR d IN ${collection.name()}
                FILTER d.nonStoredValue < 50
                LET dist = APPROX_NEAR_L2(@qp, d.vector)
                SORT dist LIMIT 5
                RETURN {key: d._key, nonStoredValue: d.nonStoredValue, dist}`;

            const bindVars = {
                qp: randomPoint
            };
            
            const plan = verifyPlan(query, bindVars);
            const indexNodes = plan.nodes.filter(n => n.type === "EnumerateNearVectorNode");
            assertFalse(indexNodes[0].isCoveredByStoredValues);

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5, "Results should be limited to 5");
            
            verifyResultsMatchFilter(results, r => r.nonStoredValue < 50, "Val filter not applied correctly");
            verifyDistancesAscending(results);
        },

        testApproxL2WithFilterStoredValuesComplex: function() {
            const query = `
                FOR d IN ${collection.name()}
                FILTER d.val >= 2 AND d.val <= 50 AND d.boolField == true AND d.floatField > 10.0
                LET dist = APPROX_NEAR_L2(@q, d.vector)
                SORT dist LIMIT 10
                RETURN {key: d._key, val: d.val, boolField: d.boolField, floatField: d.floatField, dist}`;

            const bindVars = {
                q: randomPoint
            };
            
            const plan = verifyPlan(query, bindVars);
            const indexNodes = plan.nodes.filter(n => n.type === "EnumerateNearVectorNode");
            assertTrue(indexNodes[0].isCoveredByStoredValues);

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 10, "Results should be limited to 10");
            
            verifyResultsMatchFilter(results, r => r.val >= 2 && r.val <= 50, "Val filter not applied");
            verifyResultsMatchFilter(results, r => r.boolField === true, "Bool filter not applied");
            verifyResultsMatchFilter(results, r => r.floatField > 10.0, "Float filter not applied");
            verifyDistancesAscending(results);
        },

        testApproxL2WithFilterNonStoredValues: function() {
            const query = `
                FOR d IN ${collection.name()}
                FILTER d.val >= 10 AND d.val <= 50 AND d.arrayField[0] > 2 AND d.objectField.nested == 1
                LET dist = APPROX_NEAR_L2(@q, d.vector)
                SORT dist LIMIT 5
                RETURN {key: d._key, val: d.val, arrayField: d.arrayField, objectField: d.objectField, dist}`;

            const bindVars = {
                q: randomPoint
            };
            
            const plan = db._createStatement({query, bindVars}).explain().plan;
            verifyVectorIndexUsed(plan);
            verifyNoFilterNodes(plan);

            // Not fully covered by stored values (val field is stored, but array/object fields are not)
            const indexNodes = plan.nodes.filter(n => n.type === "EnumerateNearVectorNode");
            assertFalse(indexNodes[0].isCoveredByStoredValues, "Should not be fully covered by stored values");

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5, "Results should be limited to 5");
            
            verifyResultsMatchFilter(results, r => r.val >= 10 && r.val <= 50, "Val filter not applied");
            verifyResultsMatchFilter(results, r => r.arrayField[0] > 2, "Array filter not applied");
            verifyResultsMatchFilter(results, r => r.objectField.nested === 1, "Object filter not applied");
        },

        testApproxL2WithFilterStoredValuesAndDocumentId: function() {
            const allDocs = db._query(`FOR d IN ${collection.name()} LIMIT 100 RETURN d`).toArray();
            const targetKeys = allDocs.filter(d => d.val < 50).slice(0, 10).map(d => d._key);

            const query = `
                FOR d IN ${collection.name()}
                FILTER d.stringField == 'type_A' AND d._key IN @targetKeys
                LET dist = APPROX_NEAR_L2(@q, d.vector)
                SORT dist LIMIT 5
                RETURN {key: d._key, val: d.val, stringField: d.stringField, dist}`;

            const bindVars = {
                q: randomPoint,
                targetKeys: targetKeys
            };

            const plan = verifyPlan(query, bindVars);
            const indexNodes = plan.nodes.filter(n => n.type === "EnumerateNearVectorNode");
            assertFalse(indexNodes[0].isCoveredByStoredValues, "Cannot be covered by stored values");

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5, "Results should be limited to 5");

            verifyResultsMatchFilter(results, r => r.stringField === 'type_A', "String filter not applied");
            verifyResultsMatchFilter(results, r => targetKeys.includes(r.key), "Key filter not applied");
            verifyDistancesAscending(results);
        },

        testApproxL2WithFilterConstantExpressionAndStoredValue: function() {
            // Test filtering with constant expression combined with stored value
            const query = `
                FOR d IN ${collection.name()}
                FILTER 1 + 1 == 2 AND d.val >= 5 AND d.val < 30 AND d.boolField == (1 < 2)
                LET dist = APPROX_NEAR_L2(@q, d.vector)
                SORT dist LIMIT 8
                RETURN {key: d._key, val: d.val, boolField: d.boolField, dist}`;

            const bindVars = {
                q: randomPoint
            };

            const plan = verifyPlan(query, bindVars);
            const indexNodes = plan.nodes.filter(n => n.type === "EnumerateNearVectorNode");
            assertTrue(indexNodes[0].isCoveredByStoredValues, "Should be covered by stored values");

            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 8, "Results should be limited to 8");

            verifyResultsMatchFilter(results, r => r.val >= 5 && r.val < 30, "Val filter not applied");
            verifyResultsMatchFilter(results, r => r.boolField === true, "Bool filter not applied");
            verifyDistancesAscending(results);
        },
    };
}

jsunity.run(VectorIndexL2FilterTestSuite);
jsunity.run(VectorIndexL2FilterTestMultipleCollectionsSuite);
jsunity.run(VectorIndexL2FilterStoredValuesTestSuite);

return jsunity.done();
