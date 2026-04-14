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

/// Tests that concurrent DML during vector index build is correctly picked up
/// via the WAL catch-up mechanism. Three failure points pause the build at
/// different phases; while paused, a special document is inserted or removed
/// from the client side. After the build completes, a full-scan vector query
/// (nProbe = nLists) verifies the document is present or absent.

const jsunity = require("jsunity");
const internal = require("internal");
const db = internal.db;
const IM = global.instanceManager;
const {
    randomNumberGeneratorFloat,
    generateSeed,
} = require("@arangodb/testutils/seededRandom");
const {
    VectorIndexTrainingState,
    waitForVectorIndexState,
    generateDocs,
} = require("@arangodb/testutils/vector-index-common");

const dbName = "vectorWalCatchupDB";
const collName = "vectorColl";
const dimension = 100;
const nLists = 10;
const numInitialDocs = 3000;
const indexName = "vec_l2";
const specialKey = "special_doc";
const specialVector = Array.from({length: dimension}, () => 0.5);

////////////////////////////////////////////////////////////////////////////////
/// Helpers
////////////////////////////////////////////////////////////////////////////////

function createVectorIndex(collection) {
    collection.ensureIndex({
        name: indexName,
        type: "vector",
        fields: ["vector"],
        inBackground: true,
        params: {metric: "l2", dimension, nLists},
    });
}

function specialDocFound(collection) {
    const results = db._query(
        `FOR d IN @@coll
         SORT APPROX_NEAR_L2(d.vector, @qp, {nProbe: @nProbe})
         LIMIT @limit
         RETURN d._key`,
        {"@coll": collection.name(), qp: specialVector, nProbe: nLists,
         limit: numInitialDocs + 10}
    ).toArray();
    return results.includes(specialKey);
}

function insertInitialDocs(collection, gen) {
    const docs = generateDocs(gen, numInitialDocs, dimension);
    const batchSize = 500;
    for (let i = 0; i < docs.length; i += batchSize) {
        collection.insert(docs.slice(i, i + batchSize));
    }
}

////////////////////////////////////////////////////////////////////////////////
/// Insert suite: insert a special document during each build phase and verify
/// it is found after the index reaches ready state.
////////////////////////////////////////////////////////////////////////////////

function VectorIndexWalCatchupInsertSuite() {
    const seed = generateSeed();
    let gen;
    let collection;

    return {
        setUp: function () {
            gen = randomNumberGeneratorFloat(seed);
            db._useDatabase("_system");
            try { db._dropDatabase(dbName); } catch (e) {}
            db._createDatabase(dbName);
            db._useDatabase(dbName);
            collection = db._create(collName, {numberOfShards: 1});
            insertInitialDocs(collection, gen);
        },

        tearDown: function () {
            IM.debugClearFailAt();
            db._useDatabase("_system");
            try { db._dropDatabase(dbName); } catch (e) {}
        },

        testInsertDuringTraining: function () {
            if (!IM.debugCanUseFailAt()) {
                return;
            }
            IM.debugSetFailAt("RocksDBVectorIndex::pauseBeforeTraining");
            createVectorIndex(collection);

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kTraining, 30),
                "Index should reach training state"
            );

            collection.insert({_key: specialKey, vector: specialVector});

            IM.debugClearFailAt();

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kReady, 120),
                "Index should reach ready state"
            );
            assertTrue(specialDocFound(collection),
                "Special doc inserted during training should be found");
        },

        testInsertBeforeIngestion: function () {
            if (!IM.debugCanUseFailAt()) {
                return;
            }
            IM.debugSetFailAt("RocksDBVectorIndex::pauseBeforeIngestion");
            createVectorIndex(collection);

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kIngesting, 60),
                "Index should reach ingesting state"
            );

            collection.insert({_key: specialKey, vector: specialVector});

            IM.debugClearFailAt();

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kReady, 120),
                "Index should reach ready state"
            );
            assertTrue(specialDocFound(collection),
                "Special doc inserted before ingestion should be found");
        },

        testInsertDuringIngestion: function () {
            if (!IM.debugCanUseFailAt()) {
                return;
            }
            IM.debugSetFailAt("RocksDBVectorIndex::pauseDuringIngestion");
            createVectorIndex(collection);

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kIngesting, 60),
                "Index should reach ingesting state"
            );
            // Allow time for fillIndexBackground to take a snapshot and call
            // ingestVectors, which then hits the failure point.
            internal.sleep(3);

            collection.insert({_key: specialKey, vector: specialVector});

            IM.debugClearFailAt();

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kReady, 120),
                "Index should reach ready state"
            );
            assertTrue(specialDocFound(collection),
                "Special doc inserted during ingestion should be found " +
                "via WAL catch-up");
        },
    };
}

////////////////////////////////////////////////////////////////////////////////
/// Remove suite: insert a special document with the initial data, remove it
/// during each build phase, and verify it is absent after the index reaches
/// ready state.
////////////////////////////////////////////////////////////////////////////////

function VectorIndexWalCatchupRemoveSuite() {
    const seed = generateSeed();
    let gen;
    let collection;

    return {
        setUp: function () {
            gen = randomNumberGeneratorFloat(seed);
            db._useDatabase("_system");
            try { db._dropDatabase(dbName); } catch (e) {}
            db._createDatabase(dbName);
            db._useDatabase(dbName);
            collection = db._create(collName, {numberOfShards: 1});
            insertInitialDocs(collection, gen);
            collection.insert({_key: specialKey, vector: specialVector});
        },

        tearDown: function () {
            IM.debugClearFailAt();
            db._useDatabase("_system");
            try { db._dropDatabase(dbName); } catch (e) {}
        },

        testRemoveDuringTraining: function () {
            if (!IM.debugCanUseFailAt()) {
                return;
            }
            IM.debugSetFailAt("RocksDBVectorIndex::pauseBeforeTraining");
            createVectorIndex(collection);

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kTraining, 30),
                "Index should reach training state"
            );

            collection.remove(specialKey);

            IM.debugClearFailAt();

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kReady, 120),
                "Index should reach ready state"
            );
            assertFalse(specialDocFound(collection),
                "Special doc removed during training should not be found");
        },

        testRemoveBeforeIngestion: function () {
            if (!IM.debugCanUseFailAt()) {
                return;
            }
            IM.debugSetFailAt("RocksDBVectorIndex::pauseBeforeIngestion");
            createVectorIndex(collection);

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kIngesting, 60),
                "Index should reach ingesting state"
            );

            collection.remove(specialKey);

            IM.debugClearFailAt();

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kReady, 120),
                "Index should reach ready state"
            );
            assertFalse(specialDocFound(collection),
                "Special doc removed before ingestion should not be found");
        },

        testRemoveDuringIngestion: function () {
            if (!IM.debugCanUseFailAt()) {
                return;
            }
            IM.debugSetFailAt("RocksDBVectorIndex::pauseDuringIngestion");
            createVectorIndex(collection);

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kIngesting, 60),
                "Index should reach ingesting state"
            );
            internal.sleep(3);

            collection.remove(specialKey);

            IM.debugClearFailAt();

            assertTrue(
                waitForVectorIndexState(collection, indexName,
                    VectorIndexTrainingState.kReady, 120),
                "Index should reach ready state"
            );
            assertFalse(specialDocFound(collection),
                "Special doc removed during ingestion should not be found " +
                "via WAL catch-up");
        },
    };
}

jsunity.run(VectorIndexWalCatchupInsertSuite);
jsunity.run(VectorIndexWalCatchupRemoveSuite);
return jsunity.done();
