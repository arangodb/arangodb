/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, fail */

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
const aql = arangodb.aql;
const errors = internal.errors;
const db = internal.db;
const IM = global.instanceManager;
const {
  randomNumberGeneratorFloat,
  generateSeed,
} = require("@arangodb/testutils/seededRandom");
const {
  generateDocs,
  waitForAllVectorIndexesState,
  VectorIndexTrainingState,
} = require("@arangodb/testutils/vector-index-common");

const isCluster = internal.isCluster();
const dbName = "vectorRetrainDb";
const collName = "vectorRetrainColl";

const dimension = 64;
const nLists = 10;
const trainingThreshold = nLists;
const aboveThresholdCount =
    isCluster ? trainingThreshold * 3 + 500 : trainingThreshold + 500;
const indexName = "vec_l2";

function createIndex(collection) {
  return collection.ensureIndex({
    name: indexName,
    type: "vector",
    fields: ["vector"],
    inBackground: false,
    params: {
      metric: "l2",
      dimension,
      nLists,
      trainingIterations: 5,
    },
  });
}

// Poll until the named index is back to kReady with a different id from
// preIdx. Returns the post-retrain index, or undefined on timeout.
//
// Single-server: retrain swaps the real index for its shadow, producing
// a fresh DBServer-local IndexId visible via collection.indexes(). The
// id-change check is a strong completion signal.
//
// Cluster: the coordinator's Plan-level IndexId is agency-bound and
// does NOT change across a retrain.
function waitForRetrainComplete(collection, preIdx, timeoutMs) {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    const idx = collection.indexes().find(i => i.name === preIdx.name);
    if (idx !== undefined &&
        idx.trainingState === VectorIndexTrainingState.kReady &&
        (isCluster || idx.id !== preIdx.id)) {
      return idx;
    }
    internal.sleep(0.5);
  }
  return undefined;
}

function VectorRetrainTestSuite() {
  let collection;
  const seed = generateSeed();

  return {
    setUpAll: function () {
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      db._useDatabase(dbName);
    },

    setUp: function () {
      collection = db._create(collName, {numberOfShards: isCluster ? 3 : 1});
      const gen = randomNumberGeneratorFloat(seed);
      const docs = generateDocs(gen, aboveThresholdCount, dimension);
      collection.insert(docs);
      createIndex(collection);
      assertTrue(
        waitForAllVectorIndexesState(
          collection, VectorIndexTrainingState.kReady, 120),
        "Index did not reach ready state before retrain test");
    },

    tearDown: function () {
      if (IM.debugCanUseFailAt()) {
        IM.debugClearFailAt();
      }
      try { db._drop(collName); } catch (e) {}
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) {}
    },

    testRetrainRebuildsIndexAndKeepsDataSearchable: function () {
      const preIdx = collection.indexes().find(i => i.name === indexName);
      assertEqual(VectorIndexTrainingState.kReady, preIdx.trainingState);

      collection.retrain(indexName);

      const postIdx = waitForRetrainComplete(collection, preIdx, 180_000);
      assertTrue(postIdx !== undefined,
        "Retrain did not finish within timeout");
      if (!isCluster) {
        assertNotEqual(preIdx.id, postIdx.id,
          "Retrain should produce a new IndexId on single server");
      }

      const qp = collection.any().vector;
      const results = db._query(aql`FOR d IN ${collection}
        SORT APPROX_NEAR_L2(d.vector, ${qp}, {nProbe: ${nLists}})
        LIMIT 5 RETURN d._key`).toArray();
      assertEqual(5, results.length);
    },

    testRetrainOnNonExistingIndexFails: function () {
      try {
        collection.retrain("does-not-exist");
        fail();
      } catch (e) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, e.errorNum);
      }
    },

    testRetrainOnNonVectorIndexFails: function () {
      collection.ensureIndex({
        type: "persistent", fields: ["foo"], name: "persIdx"
      });
      try {
        collection.retrain("persIdx");
        fail();
      } catch (e) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
    },

    testRetrainSurvivesConcurrentInserts: function () {
      const gen = randomNumberGeneratorFloat(seed + 1);
      const extra = generateDocs(gen, 200, dimension);

      collection.retrain(indexName);

      // Inject inserts while the shadow is being built. The old index
      // keeps serving writes; the shadow captures them via the standard
      // WAL catch-up path.
      collection.insert(extra);

      assertTrue(
        waitForAllVectorIndexesState(
          collection, VectorIndexTrainingState.kReady, 180),
        "Index did not return to ready after concurrent-insert retrain");

      const count = db._query(aql`FOR d IN ${collection}
        FILTER HAS(d, 'vector')
        COLLECT WITH COUNT INTO c RETURN c`).toArray()[0];
      assertEqual(aboveThresholdCount + extra.length, count);
    },

    testRetrainRejectsConcurrentRetrain: function () {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      // Hold the shadow build at pauseBeforeTraining so _activeRetrains
      // stays populated long enough for us to fire a second retrain.
      IM.debugSetFailAt("RocksDBVectorIndex::pauseBeforeTraining");
      collection.retrain(indexName);

      try {
        collection.retrain(indexName);
        fail();
      } catch (e) {
        assertEqual(errors.ERROR_ARANGO_CONFLICT.code, e.errorNum);
      }

      // Release the paused build so the index returns to kReady before
      // tearDown runs.
      IM.debugClearFailAt();
      assertTrue(
        waitForAllVectorIndexesState(
          collection, VectorIndexTrainingState.kReady, 180),
        "Index did not return to ready after paused retrain");
    },

    testRetrainByNumericIdViaJsClient: function () {
      const preIdx = collection.indexes().find(i => i.name === indexName);
      // idx.id is in "<coll>/<numericId>" form; retrain() accepts either.
      const numericId = preIdx.id.split('/')[1];

      collection.retrain(numericId);

      const postIdx = waitForRetrainComplete(collection, preIdx, 180_000);
      assertTrue(postIdx !== undefined,
        "Retrain-by-id did not finish within timeout");
      if (!isCluster) {
        assertNotEqual(preIdx.id, postIdx.id,
          "Retrain-by-id should produce a new IndexId on single server");
      }
    },

    testRetrainViaRestApiByJsonNumberId: function () {
      const preIdx = collection.indexes().find(i => i.name === indexName);
      const numericId = parseInt(preIdx.id.split('/')[1], 10);
      const url = '/_api/index/retrain?collection=' +
                  encodeURIComponent(collName);
      const res = arango.POST(url, {index: numericId});
      assertFalse(res.error, JSON.stringify(res));
      assertEqual(200, res.code);

      const postIdx = waitForRetrainComplete(collection, preIdx, 180_000);
      assertTrue(postIdx !== undefined,
        "REST retrain-by-JSON-number did not finish within timeout");
      if (!isCluster) {
        assertNotEqual(preIdx.id, postIdx.id,
          "REST retrain-by-JSON-number should produce a new IndexId");
      }
    },
  };
}

jsunity.run(VectorRetrainTestSuite);

return jsunity.done();
