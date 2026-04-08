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
const helper = require("@arangodb/aql-helper");
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;
const db = internal.db;
const {
  randomNumberGeneratorFloat,
  generateSeed,
} = require("@arangodb/testutils/seededRandom");
const {
  generateDocs,
  waitForVectorIndexState,
  VectorIndexTrainingState,
} = require("@arangodb/testutils/vector-index-common");
const {deriveTestSuite} = require("@arangodb/test-helper-common");

const isCluster = require("internal").isCluster();
const dbName = "vectorTrainingStateDb";
const collName = "vectorTrainingStateColl";

const dimension = 128;
const nLists = 10;
// threshold = nLists
const trainingThreshold = nLists;
const belowThresholdCount = nLists - 1;
const aboveThresholdCount = isCluster ? trainingThreshold * 3 + 500 : trainingThreshold + 500;

function createIndex(collection, sparse, inBackground = false) {
  return collection.ensureIndex({
    name: "vec_l2",
    type: "vector",
    fields: ["vector"],
    inBackground: inBackground,
    sparse: sparse,
    params: {
      metric: "l2",
      dimension: dimension,
      nLists: nLists,
      trainingIterations: 10,
    },
  });
}

function buildSearchQuery(collectionName) {
  return "FOR d IN " + collectionName +
    " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN d._key";
}

function assertIndexUnusable(collection, indexName) {
  const idx = collection.indexes().find(i => i.name === indexName);
  assertEqual(VectorIndexTrainingState.kUnusable, idx.trainingState);
  assertTrue(idx.errorMessage.length > 0,
    "Unusable vector index should have an error message");
  assertFalse(idx.hasOwnProperty("shards"),
    "Shards should not be present without hidden flag");

  if (isCluster) {
    const hiddenIdx = collection.indexes(true, true)
      .find(i => i.name === indexName);
    assertTrue(hiddenIdx.hasOwnProperty("shards"),
      "Shards should be present with hidden flag");
    assertTrue(hiddenIdx.errorMessage.length > 0,
      "Hidden index should also have a top-level error message");
  }
}

function assertIndexReady(collection, indexName) {
  const idx = collection.indexes().find(i => i.name === indexName);
  assertEqual(VectorIndexTrainingState.kReady, idx.trainingState);
  assertFalse(idx.hasOwnProperty("errorMessage"),
    "Ready vector index should not have an error message");
  assertFalse(idx.hasOwnProperty("shards"),
    "Shards should not be present without hidden flag");

  if (isCluster) {
    const hiddenIdx = collection.indexes(true, true)
      .find(i => i.name === indexName);
    assertTrue(hiddenIdx.hasOwnProperty("shards"),
      "Shards should be present with hidden flag");
    assertFalse(hiddenIdx.hasOwnProperty("errorMessage"),
      "Ready hidden index should not have an error message");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Vector index training state tests (parameterized by sparse flag)
////////////////////////////////////////////////////////////////////////////////

function VectorTrainingStateTestSuite(sparse) {
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

    testNoDataIndexRemainsUnusable: function () {
      createIndex(collection, sparse, /*inBackground*/ true);

      assertTrue(
        waitForVectorIndexState(
          collection, "vec_l2", VectorIndexTrainingState.kUnusable),
        "Index should remain unusable with no documents"
      );

      assertIndexUnusable(collection, "vec_l2");

      const gen = randomNumberGeneratorFloat(seed);
      const qp = Array.from({length: dimension}, () => gen());
      assertQueryError(
        errors.ERROR_QUERY_VECTOR_INDEX_NOT_READY.code,
        buildSearchQuery(collection.name()),
        {qp},
      );
    },

    testBelowThresholdIndexRemainsUnusable: function () {
      const gen = randomNumberGeneratorFloat(seed);
      const docs = generateDocs(gen, belowThresholdCount, dimension);
      collection.insert(docs);

      createIndex(collection, sparse, /*inBackground*/ true);

      assertTrue(
        waitForVectorIndexState(
          collection, "vec_l2", VectorIndexTrainingState.kUnusable),
        "Index should remain unusable with " + belowThresholdCount + " docs"
      );

      assertIndexUnusable(collection, "vec_l2");

      const qp = Array.from({length: dimension}, () => gen());
      assertQueryError(
        errors.ERROR_QUERY_VECTOR_INDEX_NOT_READY.code,
        buildSearchQuery(collection.name()),
        {qp},
      );
    },

    testAboveThresholdIndexBecomesReady: function () {
      const gen = randomNumberGeneratorFloat(seed);
      const docs = generateDocs(gen, aboveThresholdCount, dimension);
      collection.insert(docs);

      const result = createIndex(collection, sparse);
      assertFalse(result.hasOwnProperty("errorMessage") && result.errorMessage.length > 0,
        "ensureIndex response should not have an errorMessage when index becomes ready");

      assertTrue(
        waitForVectorIndexState(
          collection, "vec_l2", VectorIndexTrainingState.kReady, 120),
        "Index should become ready with " + aboveThresholdCount + " docs"
      );

      assertIndexReady(collection, "vec_l2");

      const qp = docs[0].vector;
      const results = db._query(buildSearchQuery(collection.name()), {qp}).toArray();
      assertEqual(5, results.length);
    },

    testUnusableBecomesReadyAfterInsertingMoreData: function () {
      const gen = randomNumberGeneratorFloat(seed);
      const initialDocs = generateDocs(gen, belowThresholdCount, dimension);
      collection.insert(initialDocs);

      createIndex(collection, sparse, /*inBackground*/ true);

      assertTrue(
        waitForVectorIndexState(
          collection, "vec_l2", VectorIndexTrainingState.kUnusable),
        "Index should start as unusable with " + belowThresholdCount + " docs"
      );

      const remainingCount = aboveThresholdCount - belowThresholdCount;
      const moreDocs = generateDocs(gen, remainingCount, dimension);
      collection.insert(moreDocs);

      assertTrue(
        waitForVectorIndexState(
          collection, "vec_l2", VectorIndexTrainingState.kReady, 120),
        "Index should become ready after inserting more data"
      );

      const qp = initialDocs[0].vector;
      const results = db._query(buildSearchQuery(collection.name()), {qp}).toArray();
      assertEqual(5, results.length);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Sparse-specific vector index tests
////////////////////////////////////////////////////////////////////////////////

function SparseVectorIndexTestSuite() {
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

    testEnoughDocsButNotEnoughWithVectorField: function () {
      const gen = randomNumberGeneratorFloat(seed);

      // Insert enough total docs but only a few with actual vector fields
      const vectorDocs = generateDocs(gen, belowThresholdCount, dimension);
      const nonVectorDocs = [];
      for (let i = 0; i < aboveThresholdCount; ++i) {
        nonVectorDocs.push({name: "doc_without_vector_" + i});
      }
      collection.insert(vectorDocs);
      collection.insert(nonVectorDocs);

      createIndex(collection, /*sparse*/ true, /*inBackground*/ true);

      assertTrue(
        waitForVectorIndexState(
          collection, "vec_l2", VectorIndexTrainingState.kUnusable),
        "Sparse index should remain unusable when total docs exceed threshold " +
        "but vector-bearing docs (" + belowThresholdCount + ") do not"
      );

      assertIndexUnusable(collection, "vec_l2");

      const qp = vectorDocs[0].vector;
      assertQueryError(
        errors.ERROR_QUERY_VECTOR_INDEX_NOT_READY.code,
        buildSearchQuery(collection.name()),
        {qp},
      );
    },
  };
}

function DenseVectorTrainingStateTestSuite() {
  let suite = {};
  deriveTestSuite(VectorTrainingStateTestSuite(/*sparse*/ false), suite, "_dense");
  return suite;
}

function SparseVectorTrainingStateTestSuite() {
  let suite = {};
  deriveTestSuite(VectorTrainingStateTestSuite(/*sparse*/ true), suite, "_sparse");
  return suite;
}

jsunity.run(DenseVectorTrainingStateTestSuite);
jsunity.run(SparseVectorTrainingStateTestSuite);
jsunity.run(SparseVectorIndexTestSuite);

return jsunity.done();
