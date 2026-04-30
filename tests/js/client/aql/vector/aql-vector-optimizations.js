/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

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
const db = internal.db;
const {
    randomNumberGeneratorFloat,
    generateSeed,
} = require("@arangodb/testutils/seededRandom");
const {
    insertDocsAndAssertIndex,
} = require("@arangodb/testutils/vector-index-common");
const isCluster = require("internal").isCluster();

const dbName = "vectorIteratorScenariosDb";
const collName = "vectorColl";
const numberOfShards = 3;

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

// Walks the iterator-selection table from RocksDBVectorIndexList.h:
//
// Proj | Filter | scF | scP | Iterator         | Capture
// ---- | ------ | --- | --- | ---------------- | --------------------
//   F  |   F    |  -  |  -  | IVT              | none                (S1)
//   T  |   F    |  -  |  F  | IVT              | none                (S2)
//   T  |   F    |  -  |  T  | IVT + on_heap    | storedValues        (S3)
//   F  |   T    |  F  |  -  | IVFT             | none                (S4)
//   F  |   T    |  T  |  -  | IVFST            | none                (S5)
//   T  |   T    |  F  |  F  | IVFT + on_heap   | full document       (S6)
//   T  |   T    |  T  |  F  | IVFST            | none                (S7)
//   T  |   T    |  F  |  T  | IVFT + on_heap   | storedValues        (S8)
//   T  |   T    |  T  |  T  | IVFST + on_heap  | storedValues        (S9)
//
// The collection has storedValues = ["val", "category"]. `extra` is NOT in
// storedValues, so it forces scF=F or scP=F when used.
function VectorIndexIteratorScenariosTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 16;
    const numberOfDocsFactor = isCluster ? numberOfShards : 1;
    const numberOfDocs = 1000 * numberOfDocsFactor;
    const seed = generateSeed();
    const nProbeAndNlists = 8;

    const indexNode = function(plan) {
        const ns = plan.nodes.filter(n => n.type === "EnumerateNearVectorNode");
        assertEqual(1, ns.length);
        return ns[0];
    };

    const hasMaterializeNode = function(plan) {
        return plan.nodes.some(n => n.type === "MaterializeNode");
    };

    const explainPlan = function(query, bindVars) {
        return db._createStatement({query, bindVars}).explain().plan;
    };

    return {
        setUpAll: function() {
            db._useDatabase("_system");
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {numberOfShards});

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < numberOfDocs; ++i) {
                const vector = Array.from({length: dimension}, () => gen());
                if (i === Math.floor(numberOfDocs / 2)) {
                    randomPoint = vector;
                }
                docs.push({
                    vector,
                    val: i,
                    category: i % 4,
                    extra: i * 2,
                });
            }

            insertDocsAndAssertIndex({
                collection, docs, seed,
                indexName: "vector_l2_scenarios",
                indexDef: {
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2",
                        dimension,
                        nLists: nProbeAndNlists,
                        trainingIterations: 10,
                        defaultNProbe: nProbeAndNlists,
                    },
                    storedValues: ["val", "category"],
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        // S1: no filter, no projections (RETURN d).
        testS1_noFilter_noProjections: function() {
            const query = `FOR d IN ${collection.name()}
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5 RETURN d`;
            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);
            assertEqual("none", node.filterMode);
            assertEqual("pass-through-id", node.projectionMode);
            if (!isCluster) {
                assertTrue(hasMaterializeNode(plan), "MaterializeNode should load the doc");
            }
            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);
        },

        // S2: no filter, projections requiring fields outside storedValues.
        testS2_noFilter_projectionsNotCovered: function() {
            const query = `FOR d IN ${collection.name()}
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {key: d._key, extra: d.extra, dist}`;
            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);
            assertEqual("none", node.filterMode);
            assertEqual("pass-through-id", node.projectionMode);
            if (!isCluster) {
                assertTrue(hasMaterializeNode(plan), "MaterializeNode needed for non-covered projections");
            }
            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);
            verifyDistancesAscending(results);
        },

        // S3: no filter, projections covered by storedValues. The vector
        // node should produce projections directly from the captured
        // storedValues array; the unconditional MaterializeNode should be
        // dropped.
        testS3_noFilter_projectionsCovered: function() {
            const query = `FOR d IN ${collection.name()}
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {val: d.val, category: d.category, dist}`;
            const bindVars = {qp: randomPoint};

            print(`Run expain`);
            db._explain(query, bindVars);
            print(`Explain ran`);
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);

            assertEqual("none", node.filterMode);
            assertEqual("covering", node.projectionMode);
            if (!isCluster) {
                assertFalse(hasMaterializeNode(plan), "MaterializeNode should be dropped when storedValues cover projections");
            }

            const results = db._query(query, bindVars).toArray();
            assertEqual(5, results.length);
            verifyDistancesAscending(results);
        },

        // S4: filter not covered (extra), no projections.
        testS4_filterNotCovered_noProjections: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.extra < 100
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5 RETURN d`;
            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);
            assertEqual("document", node.filterMode);
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5);
            verifyResultsMatchFilter(results, r => r.extra < 100);
        },

        // S5: filter covered by storedValues, no projections.
        testS5_filterCovered_noProjections: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val < 100
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5 RETURN d`;
            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);
            assertEqual("storedValues", node.filterMode);
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5);
            verifyResultsMatchFilter(results, r => r.val < 100);
        },

        // S6: filter not covered, projections not covered.
        testS6_filterNotCovered_projectionsNotCovered: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.extra < 200
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {key: d._key, extra: d.extra, dist}`;
            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);
            assertEqual("document", node.filterMode);
            assertEqual("document", node.projectionMode);
            if (!isCluster) {
                assertFalse(hasMaterializeNode(plan), "filter already loaded the doc");
            }
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5);
            verifyResultsMatchFilter(results, r => r.extra < 200);
            verifyDistancesAscending(results);
        },

        // S7: filter covered, projections not covered.
        testS7_filterCovered_projectionsNotCovered: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val < 200
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {key: d._key, extra: d.extra, dist}`;
            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);
            assertEqual("storedValues", node.filterMode);
            assertEqual("pass-through-id", node.projectionMode);
            if (!isCluster) {
                assertTrue(hasMaterializeNode(plan), "Materialize handles projections");
            }
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5);
            verifyDistancesAscending(results);
        },

        // S8: filter not covered (loads doc), projections covered by
        // storedValues. The cheaper capture is the storedValues array (not
        // the full doc), so projectionMode should be "covering".
        testS8_filterNotCovered_projectionsCovered: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.extra < 200
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {val: d.val, category: d.category, dist}`;
            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);
            assertEqual("document", node.filterMode);
            assertEqual("covering", node.projectionMode);
            if (!isCluster) {
                assertFalse(hasMaterializeNode(plan), "covered projections; no Materialize needed");
            }
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5);
            verifyDistancesAscending(results);
        },

        // S9: filter covered, projections covered (the all-storedValues
        // path -- IVFST + on_heap capture). No document load anywhere.
        testS9_filterCovered_projectionsCovered: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val < 200
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 5
              RETURN {val: d.val, category: d.category, dist}`;
            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);
            assertEqual("storedValues", node.filterMode);
            assertEqual("covering", node.projectionMode);
            if (!isCluster) {
                assertFalse(hasMaterializeNode(plan), "all stored; no doc load");
            }
            const results = db._query(query, bindVars).toArray();
            assertTrue(results.length <= 5);
            verifyResultsMatchFilter(results, r => r.val < 200);
            verifyDistancesAscending(results);
        },
    };
}

jsunity.run(VectorIndexIteratorScenariosTestSuite);

return jsunity.done();
