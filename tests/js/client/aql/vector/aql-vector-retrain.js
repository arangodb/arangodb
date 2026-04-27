/* jshint globalstrict:false, strict:false, maxlen: 500 */
/* global arango, assertEqual, assertTrue, assertFalse, assertNotEqual, fail */

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
// / @author Jure Bajic
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
  generateSeed
} = require("@arangodb/testutils/seededRandom");
const {
  generateDocs,
  waitForAllVectorIndexesState,
  VectorIndexTrainingState
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

function createIndex (collection) {
  return collection.ensureIndex({
    name: indexName,
    type: "vector",
    fields: ["vector"],
    inBackground: false,
    params: {
      metric: "l2",
      dimension,
      nLists,
      trainingIterations: 5
    }
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
function waitForRetrainComplete (collection, preIdx, timeoutSeconds) {
  const deadline = internal.time() + timeoutSeconds;
  while (internal.time() < deadline) {
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

function VectorRetrainTestSuite () {
  let collection;
  const seed = generateSeed();

  return {
    setUpAll: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(dbName);
      } catch (e) {}
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
      try {
        db._drop(collName);
      } catch (e) {}
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(dbName);
      } catch (e) {}
    },

    testRetrainRebuildsIndexAndKeepsDataSearchable: function () {
      const preIdx = collection.indexes().find(i => i.name === indexName);
      assertEqual(VectorIndexTrainingState.kReady, preIdx.trainingState);

      collection.retrain(indexName);

      const postIdx = waitForRetrainComplete(collection, preIdx, 180);
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
        type: "persistent",
        fields: ["foo"],
        name: "persIdx"
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

      const postIdx = waitForRetrainComplete(collection, preIdx, 180);
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

      const postIdx = waitForRetrainComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined,
        "REST retrain-by-JSON-number did not finish within timeout");
      if (!isCluster) {
        assertNotEqual(preIdx.id, postIdx.id,
          "REST retrain-by-JSON-number should produce a new IndexId");
      }
    },

    testRetrainPreservesStoredValuesDefinition: function () {
      // Build a vector index that carries storedValues, then retrain. The
      // shadow constructed by makeRetrainShadow() has to round-trip the
      // full user-facing definition: fields, params, and storedValues.
      const storedIdxName = "vec_with_stored";
      const storedFields = ["name", "category", "value"];

      // Add a few attributes to the existing docs so storedValues has
      // real material to project.
      db._query(aql`FOR d IN ${collection}
        UPDATE d WITH { name: CONCAT("doc-", d._key),
                        category: "all",
                        value: LENGTH(d._key) }
        IN ${collection}`);

      collection.ensureIndex({
        name: storedIdxName,
        type: "vector",
        fields: ["vector"],
        inBackground: false,
        sparse: true,
        storedValues: storedFields,
        params: {
          metric: "l2",
          dimension,
          nLists,
          trainingIterations: 5
        }
      });

      assertTrue(
        waitForAllVectorIndexesState(
          collection, VectorIndexTrainingState.kReady, 120),
        "Stored-values vector index did not reach ready state");

      const preIdx = collection.indexes().find(i => i.name === storedIdxName);
      assertTrue(preIdx !== undefined, "Pre-retrain stored-values index missing");
      assertEqual(storedFields, preIdx.storedValues,
        "Pre-retrain index must carry the requested storedValues");
      assertTrue(preIdx.sparse,
        "Pre-retrain index must carry the requested sparse flag");

      collection.retrain(storedIdxName);

      const postIdx = waitForRetrainComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined,
        "Stored-values retrain did not finish within timeout");
      if (!isCluster) {
        assertNotEqual(preIdx.id, postIdx.id,
          "Retrain should produce a new IndexId on single server");
      }

      // The user-visible definition must round-trip the rebuild.
      assertEqual(storedFields, postIdx.storedValues,
        "Retrain must preserve storedValues across the swap");
      assertEqual(preIdx.fields, postIdx.fields,
        "Retrain must preserve indexed fields");
      assertEqual(preIdx.type, postIdx.type);
      assertEqual(preIdx.sparse, postIdx.sparse,
        "Retrain must preserve the sparse flag");
      assertEqual(preIdx.params.metric, postIdx.params.metric);
      assertEqual(preIdx.params.dimension, postIdx.params.dimension);
      assertEqual(preIdx.params.nLists, postIdx.params.nLists);

      // And the stored-values cover-search path must still work — confirms
      // the shadow's storedValues were wired into the new objectId's CF
      // range, not just echoed in metadata.
      const qp = collection.any().vector;
      const projected = db._query(aql`FOR d IN ${collection}
        SORT APPROX_NEAR_L2(d.vector, ${qp}, {nProbe: ${nLists}})
        LIMIT 5
        RETURN { name: d.name, category: d.category, value: d.value }`)
        .toArray();
      assertEqual(5, projected.length);
      for (const row of projected) {
        assertTrue(row.name !== undefined, "name must be projectable post-retrain");
        assertTrue(row.category !== undefined, "category must be projectable post-retrain");
        assertTrue(row.value !== undefined, "value must be projectable post-retrain");
      }

      collection.dropIndex(created.id);
    },

    testRetrainShadowIsVisibleInIndexes: function () {
      if (isCluster || !IM.debugCanUseFailAt()) {
        // The shadow is a DBServer-local object; the coordinator's
        // indexes() view hides it. Only exercise this on single server.
        return;
      }
      const preIdx = collection.indexes().find(i => i.name === indexName);
      assertEqual(VectorIndexTrainingState.kReady, preIdx.trainingState);

      // Pause the shadow build at its first training checkpoint so the
      // old index and the shadow coexist in the collection's index set
      // long enough for us to observe them.
      IM.debugSetFailAt("RocksDBVectorIndex::pauseBeforeTraining");
      collection.retrain(indexName);

      let matching = [];
      const deadline = internal.time() + 30;
      while (internal.time() < deadline) {
        matching = collection.indexes().filter(i => i.name === indexName);
        if (matching.length === 2) {
          break;
        }
        internal.sleep(0.2);
      }
      assertEqual(2, matching.length,
        "Both old and shadow indexes should be visible during retrain");

      const shadow = matching.find(i => i.id !== preIdx.id);
      assertTrue(shadow !== undefined, "Shadow must carry a fresh id");
      assertNotEqual(VectorIndexTrainingState.kReady, shadow.trainingState,
        "Shadow should not be ready while the build is paused");

      // Unpause and wait for the swap. Post-retrain the collection must
      // report exactly one index under the name, carrying the shadow's
      // fresh id and kReady.
      IM.debugClearFailAt();
      const postIdx = waitForRetrainComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined, "Retrain did not complete in time");
      assertNotEqual(preIdx.id, postIdx.id);
      const finalMatching =
        collection.indexes().filter(i => i.name === indexName);
      assertEqual(1, finalMatching.length,
        "Exactly one index with the given name should remain after swap");
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// Verifies RocksDBVectorIndex::truncateCommit: the range-delete truncate path
// (triggered when the collection holds >= 32 * 1024 docs) calls truncateCommit
// //////////////////////////////////////////////////////////////////////////////
function VectorTruncateCommitTestSuite () {
  const truncDbName = "vectorTruncateCommitDb";
  const truncCollName = "vectorTruncateCommitColl";
  // 32 * 1024 is the hard-coded cutoff for the range-delete truncate path in
  // RocksDBCollection::truncate. One doc over is enough to exercise it.
  const rangeDeleteThreshold = 32 * 1024;
  const docCount = rangeDeleteThreshold + 1;
  const truncDimension = 4;
  const truncNLists = 5;
  let truncCollection;

  return {
    setUpAll: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(truncDbName);
      } catch (e) {}
      db._createDatabase(truncDbName);
      db._useDatabase(truncDbName);
    },

    setUp: function () {
      truncCollection = db._create(truncCollName, {numberOfShards: 1});
      const gen = randomNumberGeneratorFloat(generateSeed());
      // Insert in batches to keep peak memory bounded.
      const batchSize = 5000;
      for (let i = 0; i < docCount; i += batchSize) {
        const n = Math.min(batchSize, docCount - i);
        truncCollection.insert(generateDocs(gen, n, truncDimension));
      }
      assertEqual(docCount, truncCollection.count());

      truncCollection.ensureIndex({
        name: indexName,
        type: "vector",
        fields: ["vector"],
        inBackground: false,
        params: {
          metric: "l2",
          dimension: truncDimension,
          nLists: truncNLists,
          trainingIterations: 5
        }
      });
      assertTrue(
        waitForAllVectorIndexesState(
          truncCollection, VectorIndexTrainingState.kReady, 180),
        "Index did not reach ready state before truncate-commit test");
    },

    tearDown: function () {
      try {
        db._drop(truncCollName);
      } catch (e) {}
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(truncDbName);
      } catch (e) {}
    },

    testTruncateCommitResetsTrainingState: function () {
      const preIdx =
        truncCollection.indexes().find(i => i.name === indexName);
      assertEqual(VectorIndexTrainingState.kReady, preIdx.trainingState);

      truncCollection.truncate();
      assertEqual(0, truncCollection.count());

      const postIdx =
        truncCollection.indexes().find(i => i.name === indexName);
      assertTrue(postIdx !== undefined, "Index disappeared after truncate");
      assertNotEqual(VectorIndexTrainingState.kReady, postIdx.trainingState,
        "truncateCommit must reset the training state off kReady");
    }
  };
}

jsunity.run(VectorRetrainTestSuite);
if (!isCluster) {
  jsunity.run(VectorTruncateCommitTestSuite);
}

return jsunity.done();
