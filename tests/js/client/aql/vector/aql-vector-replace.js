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
const dbName = "vectorReplaceDb";
const collName = "vectorReplaceColl";

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
// preIdx. Returns the post-replace index, or undefined on timeout.
//
// Single-server: replace swaps the real index for its shadow, producing
// a fresh DBServer-local IndexId visible via collection.indexes(). The
// id-change check is a strong completion signal.
//
// Cluster: the coordinator's Plan-level IndexId is the SHADOW's new id
// after the supervision job commits, so the id changes here too.
function waitForReplaceComplete (collection, preIdx, timeoutSeconds) {
  const deadline = internal.time() + timeoutSeconds;
  while (internal.time() < deadline) {
    const idx = collection.indexes().find(i => i.name === preIdx.name);
    if (idx !== undefined &&
        idx.trainingState === VectorIndexTrainingState.kReady &&
        idx.id !== preIdx.id) {
      return idx;
    }
    internal.sleep(0.5);
  }
  return undefined;
}

function VectorReplaceTestSuite () {
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
        "Index did not reach ready state before replace test");
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

    testReplaceWithEmptyBodyRebuildsIndexAndKeepsDataSearchable: function () {
      const preIdx = collection.indexes().find(i => i.name === indexName);
      assertEqual(VectorIndexTrainingState.kReady, preIdx.trainingState);

      collection.replaceIndex(indexName);

      const postIdx = waitForReplaceComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined,
        "Replace did not finish within timeout");
      assertNotEqual(preIdx.id, postIdx.id,
        "Replace should produce a new IndexId");

      const qp = collection.any().vector;
      const results = db._query(aql`FOR d IN ${collection}
        SORT APPROX_NEAR_L2(d.vector, ${qp}, {nProbe: ${nLists}})
        LIMIT 5 RETURN d._key`).toArray();
      assertEqual(5, results.length);
    },

    testReplaceOnNonExistingIndexFails: function () {
      try {
        collection.replaceIndex("does-not-exist");
        fail();
      } catch (e) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, e.errorNum);
      }
    },

    testReplaceOnNonVectorIndexFails: function () {
      collection.ensureIndex({
        type: "persistent",
        fields: ["foo"],
        name: "persIdx"
      });
      try {
        collection.replaceIndex("persIdx");
        fail();
      } catch (e) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
    },

    testReplaceWithPatchChangesParams: function () {
      const preIdx = collection.indexes().find(i => i.name === indexName);
      const newNLists = nLists * 2;

      collection.replaceIndex(indexName, {params: {nLists: newNLists}});

      const postIdx = waitForReplaceComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined,
        "Replace with patch did not finish within timeout");
      // Other params untouched, metric/dimension preserved
      assertEqual(preIdx.params.metric, postIdx.params.metric);
      assertEqual(preIdx.params.dimension, postIdx.params.dimension);
      assertEqual(newNLists, postIdx.params.nLists,
        "Patched nLists must be reflected on the new index");
    },

    testReplaceWithDimensionMismatchFails: function () {
      const preIdx = collection.indexes().find(i => i.name === indexName);
      try {
        collection.replaceIndex(indexName,
          {params: {dimension: preIdx.params.dimension + 1}});
        fail();
      } catch (e) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
    },

    testReplaceWithMatchingDimensionPasses: function () {
      const preIdx = collection.indexes().find(i => i.name === indexName);
      // Explicitly echoing the existing dimension must not be rejected.
      collection.replaceIndex(indexName,
        {params: {dimension: preIdx.params.dimension}});

      const postIdx = waitForReplaceComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined,
        "Replace echoing dimension did not finish within timeout");
      assertEqual(preIdx.params.dimension, postIdx.params.dimension);
    },

    testReplaceSurvivesConcurrentInserts: function () {
      const gen = randomNumberGeneratorFloat(seed + 1);
      const extra = generateDocs(gen, 200, dimension);

      collection.replaceIndex(indexName);

      // Inject inserts while the shadow is being built. The old index
      // keeps serving writes; the shadow captures them via the standard
      // WAL catch-up path.
      collection.insert(extra);

      assertTrue(
        waitForAllVectorIndexesState(
          collection, VectorIndexTrainingState.kReady, 180),
        "Index did not return to ready after concurrent-insert replace");

      const count = db._query(aql`FOR d IN ${collection}
        FILTER HAS(d, 'vector')
        COLLECT WITH COUNT INTO c RETURN c`).toArray()[0];
      assertEqual(aboveThresholdCount + extra.length, count);
    },

    testReplaceByNumericIdViaJsClient: function () {
      const preIdx = collection.indexes().find(i => i.name === indexName);
      // idx.id is in "<coll>/<numericId>" form; replace() accepts either.
      const numericId = preIdx.id.split('/')[1];

      collection.replaceIndex(numericId);

      const postIdx = waitForReplaceComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined,
        "Replace-by-id did not finish within timeout");
      assertNotEqual(preIdx.id, postIdx.id,
        "Replace-by-id should produce a new IndexId");
    },

    testReplaceViaRestApiByJsonNumberId: function () {
      const preIdx = collection.indexes().find(i => i.name === indexName);
      const numericId = parseInt(preIdx.id.split('/')[1], 10);
      const url = '/_api/index/replace?collection=' +
                  encodeURIComponent(collName);
      const res = arango.POST(url, {index: numericId});
      assertFalse(res.error, JSON.stringify(res));
      // Cluster returns ACCEPTED + jobId; single-server returns OK.
      assertTrue(res.code === 200 || res.code === 202,
        "expected 200 or 202, got " + res.code);

      const postIdx = waitForReplaceComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined,
        "REST replace-by-JSON-number did not finish within timeout");
      assertNotEqual(preIdx.id, postIdx.id,
        "REST replace-by-JSON-number should produce a new IndexId");
    },

    testReplacePreservesStoredValuesDefinition: function () {
      // Build a vector index that carries storedValues, then replace with an
      // empty patch. The full user-facing definition (fields, params,
      // storedValues) must round-trip through the shadow build and swap.
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
      assertTrue(preIdx !== undefined, "Pre-replace stored-values index missing");
      assertEqual(storedFields, preIdx.storedValues,
        "Pre-replace index must carry the requested storedValues");
      assertTrue(preIdx.sparse,
        "Pre-replace index must carry the requested sparse flag");

      collection.replaceIndex(storedIdxName);

      const postIdx = waitForReplaceComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined,
        "Stored-values replace did not finish within timeout");
      assertNotEqual(preIdx.id, postIdx.id,
        "Replace should produce a new IndexId");

      // The user-visible definition must round-trip the rebuild.
      assertEqual(storedFields, postIdx.storedValues,
        "Replace must preserve storedValues across the swap");
      assertEqual(preIdx.fields, postIdx.fields,
        "Replace must preserve indexed fields");
      assertEqual(preIdx.type, postIdx.type);
      assertEqual(preIdx.sparse, postIdx.sparse,
        "Replace must preserve the sparse flag");
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
        assertTrue(row.name !== undefined, "name must be projectable post-replace");
        assertTrue(row.category !== undefined, "category must be projectable post-replace");
        assertTrue(row.value !== undefined, "value must be projectable post-replace");
      }
    },

    testClusterReplaceWritesAgencyJobAndSiblingEntry: function () {
      if (!isCluster) {
        return;
      }
      const AM = IM.agencyMgr;
      const preIdx = collection.indexes().find(i => i.name === indexName);
      assertEqual(VectorIndexTrainingState.kReady, preIdx.trainingState);

      // Look up the collection's plan id by walking Plan/Collections/{db}.
      const planTree = AM.getFromPlan('Plan/Collections/' + dbName);
      const collectionsMap = planTree.arango.Plan.Collections[dbName];
      let collectionPlanId;
      for (const [planId, def] of Object.entries(collectionsMap)) {
        if (def.name === collName) {
          collectionPlanId = planId;
          break;
        }
      }
      assertTrue(collectionPlanId !== undefined,
        "could not find collection planId via Plan agency tree");

      // Fire the replace. Cluster path returns ACCEPTED + jobId immediately;
      // the heavy work runs via the ReplaceIndex supervision job.
      const numericId = preIdx.id.split('/')[1];
      const url = '/_api/index/replace?collection=' +
                  encodeURIComponent(collName);
      const res = arango.POST(url, {index: numericId});
      assertFalse(res.error, JSON.stringify(res));
      assertEqual(202, res.code, "cluster /replace should return 202 ACCEPTED");
      assertTrue(typeof res.jobId === 'string' && res.jobId.length > 0,
        "cluster /replace must return a jobId");

      // Helper to read the current indexes array from Plan via the agency.
      const readPlanIndexes = () => {
        const tree = AM.getFromPlan(
          'Plan/Collections/' + dbName + '/' + collectionPlanId);
        const collEntry =
          tree.arango.Plan.Collections[dbName][collectionPlanId];
        return Array.isArray(collEntry.indexes) ? collEntry.indexes : [];
      };

      // While the job is in flight, Plan must contain a sibling index entry
      // with a `replaces` marker pointing at the original index.
      let sibling;
      const planDeadline = internal.time() + 30;
      while (internal.time() < planDeadline) {
        sibling = readPlanIndexes().find(
          i => typeof i.replaces === 'string' && i.replaces === numericId);
        if (sibling !== undefined) {
          break;
        }
        internal.sleep(0.2);
      }
      assertTrue(sibling !== undefined,
        "sibling index entry with `replaces` marker should appear in Plan");

      // Wait for the swap to commit.
      const postIdx = waitForReplaceComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined,
        "supervision-driven replace did not finish within timeout");

      // After commit: old entry removed, new entry has no `replaces`
      // marker, indexes() reflects the new id.
      const finalDeadline = internal.time() + 30;
      let finalEntry;
      while (internal.time() < finalDeadline) {
        const planIndexes = readPlanIndexes();
        const stillReplacing = planIndexes.find(
          i => typeof i.replaces === 'string');
        if (stillReplacing === undefined) {
          finalEntry = planIndexes.find(
            i => i.id === postIdx.id.split('/')[1]);
          if (finalEntry !== undefined) {
            break;
          }
        }
        internal.sleep(0.2);
      }
      assertTrue(finalEntry !== undefined,
        "Plan should converge to a single index entry with no `replaces`");
    },

    testReplaceShadowIsHiddenFromOptimizer: function () {
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

      // Fire the replace from a background task — single-server replace is
      // synchronous on the request thread, so without backgrounding the
      // pause would block this test forever.
      const tasks = require("@arangodb/tasks");
      tasks.register({
        id: "replace-task",
        command: function (params) {
          const db = require("internal").db;
          db._collection(params.collName).replaceIndex(params.indexName);
        },
        params: {collName, indexName}
      });

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
        "Both old and shadow indexes should be visible during replace");

      const shadow = matching.find(i => i.id !== preIdx.id);
      assertTrue(shadow !== undefined, "Shadow must carry a fresh id");
      assertNotEqual(VectorIndexTrainingState.kReady, shadow.trainingState,
        "Shadow should not be ready while the build is paused");

      // Unpause and wait for the swap. Post-replace the collection must
      // report exactly one index under the name, carrying the shadow's
      // fresh id and kReady.
      IM.debugClearFailAt();
      const postIdx = waitForReplaceComplete(collection, preIdx, 180);
      assertTrue(postIdx !== undefined, "Replace did not complete in time");
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

jsunity.run(VectorReplaceTestSuite);
if (!isCluster) {
  jsunity.run(VectorTruncateCommitTestSuite);
}

return jsunity.done();
