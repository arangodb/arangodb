/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, fail */

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
const errors = internal.errors;
const {
  generateDocs,
  waitForVectorIndexState,
  VectorIndexTrainingState,
} = require("@arangodb/testutils/vector-index-common");

const isCluster = internal.isCluster();
const dbName = "vectorTrackCreationDb";
const collName = "vectorTrackCreationColl";

const dimension = 128;
const nLists = 10;
const trainingThreshold = nLists;
const docCount = isCluster ? trainingThreshold * 3 + 2000 : trainingThreshold + 2000;

function createIndex(collection, inBackground) {
  return collection.ensureIndex({
    name: "vec_l2",
    type: "vector",
    fields: ["vector"],
    inBackground: inBackground,
    params: {
      metric: "l2",
      dimension: dimension,
      nLists: nLists,
      trainingIterations: 10,
    },
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test that ensureIndex blocks until the vector index is trained.
///        After ensureIndex returns, the index must already be in the ready
///        state — no polling needed.
////////////////////////////////////////////////////////////////////////////////

function VectorTrackIndexCreationForegroundSuite() {
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
      collection = db._create(collName, {numberOfShards: 3});
    },

    tearDown: function () {
      db._drop(collName);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testForegroundIndexIsReadyImmediately: function () {
      const gen = randomNumberGeneratorFloat(seed);
      const docs = generateDocs(gen, docCount, dimension);
      collection.insert(docs);

      const result = createIndex(collection, /*inBackground*/ false);

      // ensureIndex should have blocked until the index is trained.
      // The returned object must already carry the up-to-date state.
      assertEqual(VectorIndexTrainingState.kReady, result.trainingState,
        "Foreground ensureIndex response should have trainingState 'ready'");

      // Double-check via a separate index lookup.
      const idx = collection.indexes().find(i => i.name === "vec_l2");
      assertEqual(VectorIndexTrainingState.kReady, idx.trainingState);
    },
  };
}

function VectorTrackIndexCreationBackgroundSuite() {
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
      collection = db._create(collName, {numberOfShards: 3});
    },

    tearDown: function () {
      db._drop(collName);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testBackgroundIndexDoesNotBlock: function () {
      const gen = randomNumberGeneratorFloat(seed);
      const docs = generateDocs(gen, docCount, dimension);
      collection.insert(docs);

      const result = createIndex(collection, /*inBackground*/ true);

      // inBackground: true — ensureIndex returns without waiting for
      // training, so the response must still show the original unusable
      // state (proving it did not block).
      assertEqual("vector", result.type);
      assertFalse(result.trainingState === VectorIndexTrainingState.kReady,
        "Background ensureIndex response should NOT have trainingState " +
        "'ready' — it should return before training completes");

      // The build manager will train it eventually.
      assertTrue(
        waitForVectorIndexState(
          collection, "vec_l2", VectorIndexTrainingState.kReady, 120),
        "Background vector index should eventually become ready");
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test that ensureIndex returns an error when there is not enough
///        data to train the vector index, rather than hanging forever.
////////////////////////////////////////////////////////////////////////////////

function VectorTrackIndexCreationBelowThresholdSuite() {
  let collection;

  return {
    setUpAll: function () {
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      db._useDatabase(dbName);
    },

    setUp: function () {
      collection = db._create(collName, {numberOfShards: 3});
    },

    tearDown: function () {
      db._drop(collName);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testBelowThresholdReturnsError: function () {
      const seed = generateSeed();
      const gen = randomNumberGeneratorFloat(seed);
      // Insert fewer docs than the training threshold.
      const belowThresholdCount = trainingThreshold - 1;
      const docs = generateDocs(gen, belowThresholdCount, dimension);
      collection.insert(docs);

      try {
        createIndex(collection, /*inBackground*/ false);
        fail("ensureIndex should have thrown for below-threshold data");
      } catch (e) {
        assertEqual(errors.ERROR_QUERY_VECTOR_INDEX_NOT_READY.code,
          e.errorNum,
          "Expected vector index not ready error");
      }
    },

    testEmptyCollectionReturnsError: function () {
      try {
        createIndex(collection, /*inBackground*/ false);
        fail("ensureIndex should have thrown for empty collection");
      } catch (e) {
        assertEqual(errors.ERROR_QUERY_VECTOR_INDEX_NOT_READY.code,
          e.errorNum,
          "Expected vector index not ready error");
      }
    },
  };
}

jsunity.run(VectorTrackIndexCreationForegroundSuite);
jsunity.run(VectorTrackIndexCreationBackgroundSuite);
jsunity.run(VectorTrackIndexCreationBelowThresholdSuite);

return jsunity.done();
