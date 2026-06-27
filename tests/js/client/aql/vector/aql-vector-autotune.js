/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertNotEqual, arango */

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
const arangodb = require("@arangodb");
const db = internal.db;
const ERRORS = arangodb.errors;
const {
    randomNumberGeneratorFloat,
    generateSeed,
} = require("@arangodb/testutils/seededRandom");
const {
    insertDocsAndAssertIndex,
} = require("@arangodb/testutils/vector-index-common");
const isCluster = internal.isCluster();
const dbName = "vectorAutotuneDB";
const collName = "vectorAutotuneColl";
const numberOfShards = 3;

// Returns the bare numeric id of the collection's single vector index.
function vectorIndexId(collection) {
    const idx = collection.indexes().filter((i) => i.type === "vector")[0];
    assertNotEqual(undefined, idx, "expected a vector index on the collection");
    // idx.id is "<collection>/<numeric-id>"
    return idx.id.split("/")[1];
}

// Assert the operating-point-table fields of a successful tune are well-formed.
function assertTuneSummary(entry) {
    assertTrue(Number.isInteger(entry.topK));
    assertTrue(entry.topK >= 1);
    assertEqual("number", typeof entry.targetRecall);
    assertTrue(entry.targetRecall > 0 && entry.targetRecall <= 1);
    assertTrue(Number.isInteger(entry.operatingPointCount));
    assertEqual("boolean", typeof entry.reachedTargetRecall);
}

// Assert a successful autotune response on either topology. Single-server
// returns the tune summary inline; the coordinator returns a per-shard array.
function assertTunedOk(parsedBody) {
    assertEqual(false, parsedBody.error);
    if (isCluster) {
        assertTrue(Array.isArray(parsedBody.result));
        assertTrue(parsedBody.result.length >= numberOfShards,
            "expected at least one result entry per shard");
        assertEqual("boolean", typeof parsedBody.allShardsTuned);
        assertEqual(true, parsedBody.allShardsTuned);
        for (const entry of parsedBody.result) {
            assertTrue(entry.hasOwnProperty("shard"));
            assertTrue(entry.hasOwnProperty("server"));
            assertEqual(false, entry.error);
            assertTuneSummary(entry);
        }
    } else {
        assertTuneSummary(parsedBody);
    }
}

function assertOperatingPointTable(table) {
    assertTrue(Number.isInteger(table.topK));
    assertTrue(table.topK >= 1);
    assertEqual("number", typeof table.targetRecall);
    assertTrue(Array.isArray(table.points));
    for (const point of table.points) {
        assertEqual("number", typeof point.recall);
        assertEqual("string", typeof point.faissKey);
    }
}

function assertTablesOk(parsedBody) {
    assertEqual(false, parsedBody.error);
    const tableLists = isCluster
        ? parsedBody.result.map((entry) => {
            assertTrue(entry.hasOwnProperty("shard"));
            assertTrue(entry.hasOwnProperty("server"));
            assertEqual(false, entry.error);
            return entry.tunedTables;
        })
        : [parsedBody.tunedTables];
    for (const tables of tableLists) {
        assertTrue(Array.isArray(tables));
        tables.forEach(assertOperatingPointTable);
    }
}

function collectFaissKeys(parsedBody) {
    const tableLists = isCluster
        ? parsedBody.result.map((entry) => entry.tunedTables)
        : [parsedBody.tunedTables];
    const keys = [];
    for (const tables of tableLists) {
        for (const table of (tables || [])) {
            for (const point of table.points) {
                keys.push(point.faissKey);
            }
        }
    }
    return keys;
}

function VectorIndexAutotuneTestSuite() {
    let collection;
    let randomPoint;
    const dimension = 64;
    const seed = generateSeed();
    const numberOfDocsFactor = isCluster ? numberOfShards : 1;
    const nLists = 50;
    const numberOfDocs = numberOfDocsFactor * nLists * 50;

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
                if (i === 0) {
                    randomPoint = vector;
                }
                docs.push({
                    vector
                });
            }
            insertDocsAndAssertIndex({
                collection, docs, seed,
                indexName: "vector_l2",
                indexDef: {
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2",
                        dimension: dimension,
                        nLists: nLists,
                    },
                },
            });
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        // topK has no server-side default; omitting it is a 400.
        testAutotuneRequiresTopK: function() {
            const id = vectorIndexId(collection);
            const res = arango.POST_RAW(
                `/_api/index/${collName}/${id}/autotune`, {targetRecall: 0.9});
            assertEqual(400, res.code);
            assertEqual(true, res.parsedBody.error);
        },

        // targetRecall has no server-side default; omitting it is a 400.
        testAutotuneRequiresTargetRecall: function() {
            const id = vectorIndexId(collection);
            const res = arango.POST_RAW(
                `/_api/index/${collName}/${id}/autotune`, {topK: 5});
            assertEqual(400, res.code);
            assertEqual(true, res.parsedBody.error);
        },

        // Explicit per-request parameters are accepted.
        testAutotuneWithParams: function() {
            const id = vectorIndexId(collection);
            const res = arango.POST_RAW(
                `/_api/index/${collName}/${id}/autotune`,
                {topK: 5, targetRecall: 0.95});
            assertEqual(200, res.code);
            assertTunedOk(res.parsedBody);
        },

        testGetAutotuneTables: function() {
            const id = vectorIndexId(collection);
            const tuned = arango.POST_RAW(
                `/_api/index/${collName}/${id}/autotune`,
                {topK: 5, targetRecall: 0.9});
            assertEqual(200, tuned.code);

            const res = arango.GET_RAW(
                `/_api/index/${collName}/${id}/autotune`);
            assertEqual(200, res.code);
            assertTablesOk(res.parsedBody);
        },

        testGetAutotuneTablesUnknownIndex: function() {
            const res = arango.GET_RAW(
                `/_api/index/${collName}/999999999/autotune`);
            assertNotEqual(200, res.code);
            assertEqual(true, res.parsedBody.error);
        },

        // Search keeps working after a re-tune.
        testSearchWorksAfterAutotune: function() {
            const id = vectorIndexId(collection);
            arango.POST_RAW(`/_api/index/${collName}/${id}/autotune`,
                {topK: 5, targetRecall: 0.9});

            const query = "FOR d IN " + collName +
                " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN d._key";
            const results = db._query(query, {qp: randomPoint}).toArray();
            assertEqual(5, results.length);
        },

        // targetRecall outside (0, 1] is rejected with a 400.
        testAutotuneRejectsInvalidTargetRecall: function() {
            const id = vectorIndexId(collection);
            const res = arango.POST_RAW(
                `/_api/index/${collName}/${id}/autotune`,
                {topK: 5, targetRecall: 2});
            assertEqual(400, res.code);
            assertEqual(true, res.parsedBody.error);
        },

        // Autotuning a non-existent index id returns not-found.
        testAutotuneUnknownIndex: function() {
            const res = arango.POST_RAW(
                `/_api/index/${collName}/999999999/autotune`,
                {topK: 5, targetRecall: 0.9});
            assertNotEqual(200, res.code);
            assertEqual(true, res.parsedBody.error);
        },
    };
}

// Composite FAISS factories surface more than nprobe (ht for PQ,
// quantizer_efSearch for an HNSW coarse quantizer). These tests confirm the
// sweep tunes those, persists composite keys, and that a targetRecall query
// replays them — proving FAISS only emits parameters we can set per query.
function VectorIndexAutotuneCompositeTestSuite() {
    const compositeDbName = "vectorAutotuneCompositeDB";
    const dimension = 32;
    const seed = generateSeed();
    const numberOfDocs = (isCluster ? numberOfShards : 1) * 1000;

    function runComposite(factory, nLists, expectedTokens, targetRecall = 0.5) {
        const cName = "composite";
        const collection = db._create(cName, { numberOfShards });
        try {
            const gen = randomNumberGeneratorFloat(seed);
            const docs = [];
            let queryPoint;
            for (let i = 0; i < numberOfDocs; ++i) {
                const vector = Array.from({ length: dimension }, () => gen());
                if (i === 0) {
                    queryPoint = vector;
                }
                docs.push({ vector });
            }
            insertDocsAndAssertIndex({
                collection, docs, seed,
                indexName: "vector_l2",
                indexDef: {
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: { metric: "l2", dimension, factory, nLists },
                },
            });

            const id = vectorIndexId(collection);
            const tuned = arango.POST_RAW(
                `/_api/index/${cName}/${id}/autotune`,
                {topK: 5, targetRecall: targetRecall});
            assertEqual(200, tuned.code);
            assertTunedOk(tuned.parsedBody);

            const tables = arango.GET_RAW(`/_api/index/${cName}/${id}/autotune`);
            assertEqual(200, tables.code);
            assertTablesOk(tables.parsedBody);
            const keys = collectFaissKeys(tables.parsedBody);
            assertTrue(keys.length > 0, "expected operating points");
            assertTrue(
                keys.some((k) => expectedTokens.every((t) => k.includes(t))),
                `expected a key with all of ${JSON.stringify(expectedTokens)},` +
                ` got ` + JSON.stringify(keys));

            // A targetRecall query replays the composite operating point; it
            // throws if any tuned parameter cannot be applied per query.
            const results = db._query(
                "FOR d IN " + cName +
                ` SORT APPROX_NEAR_L2(d.vector, @qp, {targetRecall: ${targetRecall}})` +
                " LIMIT 5 RETURN d._key", {qp: queryPoint}).toArray();
            assertEqual(5, results.length);
        } finally {
            db._drop(cName);
        }
    }

    return {
        setUpAll: function() {
            db._useDatabase("_system");
            db._createDatabase(compositeDbName);
            db._useDatabase(compositeDbName);
        },

        tearDownAll: function() {
            db._useDatabase("_system");
            db._dropDatabase(compositeDbName);
        },

        testAutotuneHnswQuantizerComposite: function() {
            runComposite("IVF8_HNSW8,Flat", 8, ["quantizer_efSearch="]);
        },

        testAutotunePqComposite: function() {
            runComposite("IVF8,PQ4np", 8, ["ht="], 0.3);
        },

        testAutotuneHnswQuantizerPqComposite: function() {
            runComposite("IVF8_HNSW8,PQ2np", 8,
                ["quantizer_efSearch=", "ht="], 0.1);
        },

        testAutotuneScalarQuantizerComposite: function() {
            runComposite("IVF8,SQ8", 8, ["nprobe="]);
        },
    };
}

jsunity.run(VectorIndexAutotuneTestSuite);
jsunity.run(VectorIndexAutotuneCompositeTestSuite);

return jsunity.done();
