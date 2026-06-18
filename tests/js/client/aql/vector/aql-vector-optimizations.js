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
    insertDocsAndAssertIndexStates,
    waitForVectorIndexState,
    VectorIndexTrainingState
} = require("@arangodb/testutils/vector-index-common");
const isCluster = require("internal").isCluster();
const IM = global.instanceManager;
const numberOfShards = 3;

const ITERATOR_SCENARIO_CONFIGS = [
    {
        name: "trainedIndex",
        numberOfDocs: 1000,
        nProbeAndNlists: 8,
    },
    {
        name: "linearScan",
        numberOfDocs: 100,
        nProbeAndNlists: 101,
    },
];

const verifyResultsMatchFilter = function(results, filterFn, message) {
    for (let i = 0; i < results.length; ++i) {
        assertTrue(filterFn(results[i]), message || "Result should match filter condition");
    }
};

const verifyTopK = function(results, k) {
    assertEqual(k, results.length);
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
function VectorIndexIteratorScenariosTestSuite(config) {
    let collection;
    let randomPoint;
    const dimension = 16;
    const numberOfDocsFactor = isCluster ? numberOfShards : 1;
    const numberOfDocs = config.numberOfDocs * numberOfDocsFactor;
    const nProbeAndNlists = config.nProbeAndNlists;
    const dbName = "vectorIteratorScenariosDb_" + config.name;
    const collName = "vectorColl_" + config.name;
    const indexName = "vector_l2_scenarios_" + config.name;

    const seed = generateSeed();

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

    const explainText = function(query, bindVars) {
        return require("@arangodb/aql/explainer").explain(
            {query, bindVars}, {colors: false}, false);
    };

    // Asserts that the textual explainer output for an EnumerateNearVectorNode
    // mirrors the node's structured filterMode/projectionMode fields, plus the
    // FILTER and LET clauses when applicable.
    const verifyExplainerOutput = function(text, node) {
        assertTrue(text.includes("/* vector index"),
            `missing vector index annotation: ${text}`);

        if (node.filterMode === "storedValues") {
            assertTrue(text.includes("filter via storedValues"), text);
        } else if (node.filterMode === "document") {
            assertTrue(text.includes("filter via document"), text);
        } else {
            assertFalse(text.includes("filter via"), text);
        }

        if (node.projectionMode === "covering") {
            assertTrue(text.includes("projections via storedValues"), text);
        } else if (node.projectionMode === "document") {
            assertTrue(text.includes("projections via document"), text);
        } else {
            assertFalse(text.includes("projections via"), text);
        }

        if (node.filter) {
            assertTrue(/\bFILTER\b/.test(text), text);
            assertTrue(text.includes("early pruning"), text);
        } else {
            assertFalse(text.includes("early pruning"), text);
        }

        const hasProjectionVars = (node.projections || []).some(
            p => p.hasOwnProperty('variable'));
        if (hasProjectionVars) {
            assertTrue(/\bLET\b/.test(text),
                `expected LET for projection output registers: ${text}`);
        }
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

            insertDocsAndAssertIndexStates({
                collection, docs, seed,
                indexName: indexName,
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
                allowedVectorIndexStates: [VectorIndexTrainingState.kReady, VectorIndexTrainingState.kUnusable]
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testS1_noFilter_noProjections: function() {
            const query = `FOR d IN ${collection.name()}
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10 RETURN d`;

            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);

            assertEqual("none", node.filterMode);
            assertEqual("pass-through-id", node.projectionMode);
            assertTrue(hasMaterializeNode(plan), "MaterializeNode needed to load the doc");

            verifyExplainerOutput(explainText(query, bindVars), node);

            const results = db._query(query, bindVars).toArray();
            assertEqual(10, results.length);
        },

        testS2_noFilter_projectionsNotCovered: function() {
            const query = `FOR d IN ${collection.name()}
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10
              RETURN {key: d._key, extra: d.extra, dist}`;

            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);

            assertEqual("none", node.filterMode);
            assertEqual("pass-through-id", node.projectionMode);
            assertTrue(hasMaterializeNode(plan), "MaterializeNode needed for non-covered projections");

            verifyExplainerOutput(explainText(query, bindVars), node);

            const results = db._query(query, bindVars).toArray();
            verifyTopK(results, 10);
        },

        testS3_noFilter_projectionsCovered: function() {
            const query = `FOR d IN ${collection.name()}
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10
              RETURN {val: d.val, category: d.category, dist}`;

            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);

            assertEqual("none", node.filterMode);
            assertEqual("covering", node.projectionMode);
            assertFalse(hasMaterializeNode(plan), "MaterializeNode should be dropped when storedValues cover projections");

            verifyExplainerOutput(explainText(query, bindVars), node);

            const results = db._query(query, bindVars).toArray();
            verifyTopK(results, 10);
        },

        testS4_filterNotCovered_noProjections: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.extra < 100
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10 RETURN d`;

            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);

            assertEqual("document", node.filterMode);
            assertEqual("document", node.projectionMode);
            assertFalse(hasMaterializeNode(plan), "filter already loaded the doc");

            verifyExplainerOutput(explainText(query, bindVars), node);

            const results = db._query(query, bindVars).toArray();
            assertEqual(10, results.length);
            verifyResultsMatchFilter(results, r => r.extra < 100);
        },

        testS5_filterCovered_noProjections: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val < 100
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10 RETURN d`;

            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);

            assertEqual("storedValues", node.filterMode);
            assertEqual("pass-through-id", node.projectionMode);
            assertTrue(hasMaterializeNode(plan), "MaterializeNode needed to load the doc");

            verifyExplainerOutput(explainText(query, bindVars), node);

            const results = db._query(query, bindVars).toArray();
            assertEqual(10, results.length);
            verifyResultsMatchFilter(results, r => r.val < 100);
        },

        testS6_filterNotCovered_projectionsNotCovered: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.extra < 200
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10
              RETURN {key: d._key, extra: d.extra, dist}`;

            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);

            assertEqual("document", node.filterMode);
            assertEqual("document", node.projectionMode);
            assertFalse(hasMaterializeNode(plan), "filter already loaded the doc");

            verifyExplainerOutput(explainText(query, bindVars), node);

            const results = db._query(query, bindVars).toArray();
            verifyTopK(results, 10);
            verifyResultsMatchFilter(results, r => r.extra < 200);
        },

        testS7_filterCovered_projectionsNotCovered: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val < 200
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10
              RETURN {key: d._key, extra: d.extra, dist}`;

            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);

            assertEqual("storedValues", node.filterMode);
            assertEqual("pass-through-id", node.projectionMode);
            assertTrue(hasMaterializeNode(plan), "Materialize handles projections");

            verifyExplainerOutput(explainText(query, bindVars), node);

            const results = db._query(query, bindVars).toArray();
            verifyTopK(results, 10);
        },

        testS8_filterNotCovered_projectionsCovered: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.extra < 200
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10
              RETURN {val: d.val, category: d.category, dist}`;

            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);

            assertEqual("document", node.filterMode);
            assertEqual("covering", node.projectionMode);
            assertFalse(hasMaterializeNode(plan), "covered projections; no Materialize needed");

            verifyExplainerOutput(explainText(query, bindVars), node);

            const results = db._query(query, bindVars).toArray();
            verifyTopK(results, 10);
        },

        testS9_filterCovered_projectionsCovered: function() {
            const query = `FOR d IN ${collection.name()}
              FILTER d.val < 200
              LET dist = APPROX_NEAR_L2(@qp, d.vector)
              SORT dist LIMIT 10
              RETURN {val: d.val, category: d.category, dist}`;

            const bindVars = {qp: randomPoint};
            const plan = explainPlan(query, bindVars);
            const node = indexNode(plan);

            assertEqual("storedValues", node.filterMode);
            assertEqual("covering", node.projectionMode);
            assertFalse(hasMaterializeNode(plan), "all stored; no doc load");

            verifyExplainerOutput(explainText(query, bindVars), node);

            const results = db._query(query, bindVars).toArray();
            verifyTopK(results, 10);
            verifyResultsMatchFilter(results, r => r.val < 200);
        },
    };
}

// During index creation there existins RocksDBBuilderIndex which acts diffrently
// then normal index when asked if it covers the given projections
function VectorCoveredProjectionDuringIngestionRegressionSuite() {
    const dbName = "vectorCoveredIngestionDB";
    const collName = "vectorCoveredIngestionColl";
    const indexName = "vec_l2_stored_ingestion";
    const dimension = 16;
    const nLists = 4;
    // A single shard is enough: even with one shard the cluster keeps the
    // coordinator/DB-server split, so the coordinator picks a COVERED plan and
    // the DB server re-checks covers() against the swapped-in builder index.
    // One shard also reaches (and holds) the ingesting state far more reliably
    // than waiting for several parallel background builds to line up.
    const ingestionShards = 1;
    const numberOfDocs = 3000;
    const topK = 10;

    const seed = generateSeed();
    let collection;
    let queryPoint;

    return {
        setUp: function () {
            db._useDatabase("_system");
            try { db._dropDatabase(dbName); } catch (e) {}
            db._createDatabase(dbName);
            db._useDatabase(dbName);
            collection = db._create(collName, {numberOfShards: ingestionShards});

            const gen = randomNumberGeneratorFloat(seed);
            const docs = [];
            for (let i = 0; i < numberOfDocs; ++i) {
                const vector = Array.from({length: dimension}, () => gen());
                if (i === Math.floor(numberOfDocs / 2)) {
                    queryPoint = vector;
                }
                docs.push({vector, val: i, category: i % 4, extra: i * 2});
            }
            const batchSize = 500;
            for (let i = 0; i < docs.length; i += batchSize) {
                collection.insert(docs.slice(i, i + batchSize));
            }
        },

        tearDown: function () {
            IM.debugClearFailAt();
            db._useDatabase("_system");
            try { db._dropDatabase(dbName); } catch (e) {}
        },

        testCoveredProjectionWhileIndexIsIngesting: function () {
            if (!isCluster || !IM.debugCanUseFailAt()) {
                return;
            }

            IM.debugSetFailAt("RocksDBVectorIndex::pauseBeforeIngestion");

            collection.ensureIndex({
                name: indexName,
                type: "vector",
                fields: ["vector"],
                inBackground: true,
                params: {metric: "l2", dimension, nLists},
                storedValues: ["val", "category"],
            });

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kIngesting, 120),
                "index should reach (and hold) the ingesting state");

            const query = `FOR d IN ${collName}
                FILTER d.extra < 100000
                LET dist = APPROX_NEAR_L2(@qp, d.vector)
                SORT dist LIMIT @topK
                RETURN {val: d.val, category: d.category, dist}`;
            const bindVars = {qp: queryPoint, topK};

            // Must be COVERED on the coordinator -- that is what makes the DB
            // server re-check covers() against the swapped-in wrapper.
            const node = db._createStatement({query, bindVars})
                .explain().plan.nodes
                .find(n => n.type === "EnumerateNearVectorNode");
            assertTrue(node !== undefined, "expected an EnumerateNearVectorNode");
            assertEqual("covering", node.projectionMode);
            assertEqual("document", node.filterMode);

            // Pre-fix this aborts the DB server in
            // EnumerateNearVectorNode::createBlock (covers() assertion).
            const results = db._query(query, bindVars).toArray();

            assertEqual(topK, results.length, "expected top-K results");
            for (let i = 0; i < results.length; ++i) {
                assertTrue(results[i].val !== undefined, "covered val missing");
                assertTrue(results[i].category !== undefined,
                    "covered category missing");
            }
            for (let i = 1; i < results.length; ++i) {
                assertTrue(results[i - 1].dist <= results[i].dist,
                    `distances must be ascending: ${JSON.stringify(results)}`);
            }

            IM.debugClearFailAt();
            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kReady, 120),
                "index should reach ready state after clearing failure point");
        },
    };
}

for (const config of ITERATOR_SCENARIO_CONFIGS) {
    jsunity.run(function () {
        return VectorIndexIteratorScenariosTestSuite(config);
    });
}

jsunity.run(VectorCoveredProjectionDuringIngestionRegressionSuite);

return jsunity.done();
