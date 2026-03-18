/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertFalse, print */

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

const jsunity = require("jsunity");
const db = require("internal").db;
const internal = require("internal");
const {
  randomNumberGeneratorFloat,
  randomInteger,
} = require("@arangodb/testutils/seededRandom");
const {
  waitForVectorIndexState,
  waitForAllVectorIndexesTrainingStateOnDBServers,
} = require("@arangodb/testutils/vector-index-common");
const {
  getMetricSingle,
  getCompleteMetricsValues,
} = require("@arangodb/test-helper");

const isCluster = internal.isCluster();
const dbName = "vectorBuildMetricsDB";
const collName = "coll";
const dimension = 100;

function generateDocs(gen, count) {
  let docs = [];
  for (let i = 0; i < count; ++i) {
    docs.push({vector: Array.from({length: dimension}, () => gen())});
  }
  return docs;
}

function waitForState(collection, state, timeoutSec) {
  if (isCluster) {
    return waitForAllVectorIndexesTrainingStateOnDBServers(
        db, collection, state, timeoutSec);
  }
  return waitForVectorIndexState(collection, "vec_l2", state, timeoutSec);
}

function getMetricValue(name) {
  if (isCluster) {
    let values = getCompleteMetricsValues(name, "dbservers");
    let sum = 0;
    for (let v of values) {
      sum += v;
    }
    return sum;
  }
  return getMetricSingle(name);
}

function VectorIndexBuildMetricsSuite() {
  let collection;
  const seed = randomInteger();
  const insertCountFactor = isCluster ? 3 : 1;

  return {
    setUp: function () {
      print("Using seed: " + seed);
      db._useDatabase("_system");
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      collection = db._create(collName, {numberOfShards: 3});
    },

    tearDown: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testUntrainedCountIncreasesWithUntrainedIndex: function () {
      collection.ensureIndex({
        name: "vec_l2",
        type: "vector",
        fields: ["vector"],
        inBackground: false,
        params: {metric: "l2", dimension, nLists: 1},
      });

      // With no documents, the index should remain unusable.
      // The build coordinator scans every ~5 seconds, so wait a bit.
      assertTrue(
          waitForState(collection, "unusable", 10),
          "Index should remain unusable with no documents"
      );

      // The untrained gauge should reflect at least 1 untrained index.
      // Wait for at least one scan cycle to update the metric.
      let found = false;
      for (let i = 0; i < 30; ++i) {
        internal.wait(1);
        let count = getMetricValue("arangodb_vector_index_untrained_count");
        if (count >= 1) {
          found = true;
          break;
        }
      }
      assertTrue(found,
          "arangodb_vector_index_untrained_count should be >= 1");
    },

    testMetricsAfterSuccessfulBuild: function () {
      collection.ensureIndex({
        name: "vec_l2",
        type: "vector",
        fields: ["vector"],
        inBackground: false,
        params: {metric: "l2", dimension, nLists: 1},
      });

      // Record initial histogram _count values before training.
      let initialTrainingCount = getMetricValue(
          "arangodb_vector_index_training_duration_count");
      let initialIngestionCount = getMetricValue(
          "arangodb_vector_index_ingestion_duration_count");

      // Insert enough documents to trigger training.
      let gen = randomNumberGeneratorFloat(seed);
      const insertCount = 1500 * insertCountFactor;
      collection.insert(generateDocs(gen, insertCount));
      assertEqual(insertCount, collection.count());

      // Wait for training to complete.
      assertTrue(
          waitForState(collection, "ready", 120),
          "Index should become trained after " + insertCount + " docs"
      );

      // After training, the ongoing count should be back to 0.
      // Give the scan loop a moment to update.
      let ongoingCount = 0;
      for (let i = 0; i < 30; ++i) {
        internal.wait(1);
        ongoingCount = getMetricValue(
            "arangodb_vector_index_training_ongoing_count");
        if (ongoingCount === 0) {
          break;
        }
      }
      assertEqual(0, ongoingCount,
          "arangodb_vector_index_training_ongoing_count should be 0 " +
          "after build completes");

      // The untrained count should be 0 (the only index is now trained).
      // Wait for a scan cycle to update the gauge.
      let untrainedCount = -1;
      for (let i = 0; i < 30; ++i) {
        internal.wait(1);
        untrainedCount = getMetricValue(
            "arangodb_vector_index_untrained_count");
        if (untrainedCount === 0) {
          break;
        }
      }
      assertEqual(0, untrainedCount,
          "arangodb_vector_index_untrained_count should be 0 " +
          "after successful build");

      // The training duration histogram should have at least one new
      // observation.
      let trainingCount = getMetricValue(
          "arangodb_vector_index_training_duration_count");
      assertTrue(trainingCount > initialTrainingCount,
          "arangodb_vector_index_training_duration_count should " +
          "increase after build (was " + initialTrainingCount +
          ", now " + trainingCount + ")");

      // The ingestion duration histogram should have at least one new
      // observation.
      let ingestionCount = getMetricValue(
          "arangodb_vector_index_ingestion_duration_count");
      assertTrue(ingestionCount > initialIngestionCount,
          "arangodb_vector_index_ingestion_duration_count should " +
          "increase after build (was " + initialIngestionCount +
          ", now " + ingestionCount + ")");
    },
  };
}

jsunity.run(VectorIndexBuildMetricsSuite);

return jsunity.done();
