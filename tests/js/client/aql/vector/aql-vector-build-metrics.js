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
  generateDocs,
  waitForAllVectorIndexesState,
} = require("@arangodb/testutils/vector-index-common");
const {
  getMetricSingle,
  getCompleteMetricsValues,
} = require("@arangodb/test-helper");

const isCluster = internal.isCluster();
const dbName = "vectorBuildMetricsDB";
const collName = "coll";
const dimension = 100;
const numberOfShards = 3;
const replicationFactor = 2;

const metricUnusableCount = "arangodb_vector_index_unusable";
const metricTrainingOngoingCount = "arangodb_vector_index_training_ongoing";
const metricTrainingDurationCount = "arangodb_vector_index_training_duration_count";
const metricIngestionDurationCount = "arangodb_vector_index_ingestion_duration_count";

function getMetricValue(name) {
  if (isCluster) {
    return getCompleteMetricsValues(name, "dbservers");
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
      collection = db._create(collName, {numberOfShards, replicationFactor});
    },

    tearDown: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testUnusableCount: function () {
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
          waitForAllVectorIndexesState(collection, "unusable", 10),
          "Index should remain unusable with no documents"
      );

      // The untrained gauge should reflect the exact number of untrained
      // index instances: in cluster mode each shard has its own index copy,
      // in single server there is just one.
      const expectedUnusable = isCluster ? numberOfShards : 1;
      let unusableFound = false;
      for (let i = 0; i < 30; ++i) {
        internal.wait(1);
        let count = getMetricValue(metricUnusableCount);
        if (count === expectedUnusable) {
          unusableFound = true;
          break;
        }
      }
      assertTrue(unusableFound,
          metricUnusableCount + " should be " + expectedUnusable);
    },

    testMetricsAfterSuccessfulBuild: function () {
      let newCollection =
      collection.ensureIndex({
        name: "vec_l2",
        type: "vector",
        fields: ["vector"],
        inBackground: false,
        params: {metric: "l2", dimension, nLists: 1},
      });

      // Initial metrics
      let initialTrainingCount = getMetricValue(metricTrainingDurationCount);
      let initialIngestionCount = getMetricValue(metricIngestionDurationCount);

      let gen = randomNumberGeneratorFloat(seed);
      const insertCount = 1500 * insertCountFactor;
      collection.insert(generateDocs(gen, insertCount, dimension));
      assertEqual(insertCount, collection.count());

      // Wait for vec idx to be built
      assertTrue(
          waitForAllVectorIndexesState(collection, "ready", 120),
          "Index should become trained after " + insertCount + " docs"
      );

      // Chek metrics for both
      let ongoingCount = 0;
      for (let i = 0; i < 30; ++i) {
        internal.wait(1);
        ongoingCount = getMetricValue(metricTrainingOngoingCount);
        if (ongoingCount === 0) {
          break;
        }
      }
      assertEqual(0, ongoingCount,
          metricTrainingOngoingCount + " should be 0 after build completes");

      // The untrained count should be 0 (the only index is now trained).
      // Wait for a scan cycle to update the gauge.
      let unusableCount = -1;
      for (let i = 0; i < 30; ++i) {
        internal.wait(1);
        unusableCount = getMetricValue(metricUnusableCount);
        if (unusableCount === 0) {
          break;
        }
      }
      assertEqual(0, unusableCount,
          metricUnusableCount + " should be 0 after successful build");

      // The training duration histogram should have at least one new
      // observation.
      let trainingCount = getMetricValue(metricTrainingDurationCount);
      assertTrue(trainingCount > initialTrainingCount,
          metricTrainingDurationCount + " should increase after build" +
          " (was " + initialTrainingCount + ", now " + trainingCount + ")");

      // The ingestion duration histogram should have at least one new
      // observation.
      let ingestionCount = getMetricValue(metricIngestionDurationCount);
      assertTrue(ingestionCount > initialIngestionCount,
          metricIngestionDurationCount + " should increase after build" +
          " (was " + initialIngestionCount + ", now " + ingestionCount + ")");
    },
  };
}

jsunity.run(VectorIndexBuildMetricsSuite);

return jsunity.done();
