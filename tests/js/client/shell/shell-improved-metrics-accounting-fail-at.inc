/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global arango, fail, assertEqual, assertNotEqual, assertTrue, assertNull */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Alexey Bakharew
// //////////////////////////////////////////////////////////////////////////////
let IM = global.instanceManager;

(function () {
  'use strict';

  const db = require('@arangodb').db;
  const internal = require("internal");
  const { getCompleteMetricsValues, waitForEstimatorSync } = require('@arangodb/test-helper');

  const bufferedMemoryMetric = "arangodb_revision_tree_buffered_memory_usage";
  const estimatesMetric = "arangodb_index_estimates_memory_usage";

  const waitForMetricToBeAtMost = (metricName, expected) => {
    let current = getCompleteMetricsValues(metricName);
    let tries = 120;
    while (current > expected && tries-- > 0) {
      internal.sleep(0.25);
      current = getCompleteMetricsValues(metricName);
    }
    assertTrue(current <= expected, metricName + " value is not decreased to at least " + expected);
  };

  const waitForMetricToBeZero = (metricName) => {
    waitForMetricToBeAtMost(metricName, 0);
  };

  return function (dbName, databaseCreationOptions, collectionCreationOptions, shardingEnabled = false) {
    const collectionName = "test";
    const viewName = "testView";
    const n = 10000;

    let generateAndInsert = (collName) => {
      if( typeof generateAndInsert.counter === 'undefined' ) {
        generateAndInsert.counter = 0;
      }
      if( typeof generateAndInsert.factor === 'undefined' ) {
        generateAndInsert.factor = 0;
      }
      generateAndInsert.factor++;

      let docs = [];
      for (let i = 0; i < 1000 * generateAndInsert.factor; i++) {
        let custom_field = "field_" + generateAndInsert.counter;
        let d = {
          'stringValue': "" + generateAndInsert.counter, 
          'numericValue': generateAndInsert.counter
        };
        d[custom_field] = generateAndInsert.counter;
        docs.push(d);
        generateAndInsert.counter++;
      }
      db._collection(collName).save(docs);
    };

    let insertDocuments = (collName) => {
      let c = db._collection(collName);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({value: i});
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
    };

    let insertEdges = (collName) => {
      let c = db._collection(collName);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({_from: "v/" + i, _to: "v/" + (i + 1)});
        if (docs.length === 1000) {
          c.insert(docs);
          docs = [];
        }
      }
    };

    return {
      setUpAll: function () {
        if (databaseCreationOptions) {
          db._createDatabase(dbName, databaseCreationOptions);
        } else {
          db._createDatabase(dbName);
        }
        db._useDatabase(dbName);
      },

      tearDownAll: function () {
        db._useDatabase('_system');
        db._dropDatabase(dbName);
        IM.debugClearFailAt();
      },

      setUp: function () {
        IM.debugClearFailAt();
      },

      tearDown: function () {
        IM.debugClearFailAt();
        db._drop(collectionName);
      },

      testRevisionTreeMemoryUsageShouldIncreaseAfterBatchedInserts: function () {
        // wait until all pending estimates & revision tree buffers have been applied
        let res = waitForEstimatorSync();
        assertNull(res);
        
        // block sync thread from doing anything from now on
        IM.debugSetFailAt("RocksDBSettingsManagerSync");

        // wait until all pending estimates & revision tree buffers have been applied
        res = waitForEstimatorSync();
        assertNull(res);
        
        db._create(collectionName, collectionCreationOptions);
        let metric0 = getCompleteMetricsValues(bufferedMemoryMetric);
        insertDocuments(collectionName);
        
        // must have more memory allocated for the documents
        let metric1 = getCompleteMetricsValues(bufferedMemoryMetric);
        assertTrue(metric1 >= metric0 + n * 8, { metric1, metric0 });

        // Now check that metric value will go down
        IM.debugClearFailAt();
        // wait until all pending estimates & revision tree buffers have been applied
        res = waitForEstimatorSync();
        assertNull(res);
        
        waitForMetricToBeZero(bufferedMemoryMetric);
      },

      testRevisionTreeMemoryUsageShouldIncreaseAfterSingleInserts: function () {
        // wait until all pending estimates & revision tree buffers have been applied
        let res = waitForEstimatorSync();
        assertNull(res);

        // block sync thread from doing anything from now on
        IM.debugSetFailAt("RocksDBSettingsManagerSync");

        // wait until all pending estimates & revision tree buffers have been applied
        res = waitForEstimatorSync();
        assertNull(res);

        let c = db._create(collectionName, collectionCreationOptions);
        let metric0 = getCompleteMetricsValues(bufferedMemoryMetric);

        for (let i = 0; i < n; ++i) {
          c.insert({value: i});
        }

        // must have more memory allocated for the documents
        let metric1 = getCompleteMetricsValues(bufferedMemoryMetric);
        // 48 = sizeof(void*) + sizeof(decltype(_revisionInsertBuffers)::mapped_type).
        // must be changed when type of RocksDBMetaCollection::_revisionInsertBuffers
        // changes.
        assertTrue(metric1 >= metric0 + n * 48, { metric1, metric0 });

        // Now check that metric value will go down
        IM.debugClearFailAt();
        // wait until all pending estimates & revision tree buffers have been applied
        res = waitForEstimatorSync();
        assertNull(res);

        waitForMetricToBeZero(bufferedMemoryMetric);
      },

      testRevisionTreeMemoryUsageShouldIncreaseAndDecreaseAfterAql: function () {               
        // wait until all pending estimates & revision tree buffers have been applied
        let res = waitForEstimatorSync();
        assertNull(res);

        // Check that metric value is 0 before inserting documents
        waitForMetricToBeZero(bufferedMemoryMetric);

        // Create collection and insert documents 
        db._create(collectionName, collectionCreationOptions);
        db._query(`for i in 1..${n} insert {value: i} into ${collectionName}`);

        waitForMetricToBeZero(bufferedMemoryMetric);
      },

      testEstimatesShouldNotChangeWhenCreatingDocumentCollection: function () {
        const metric0 = getCompleteMetricsValues(estimatesMetric);

        db._create(collectionName, collectionCreationOptions);

        // estimates should not have changed.
        // we allow it to go down here because from a previous testsuite
        // there may still be buffered index estimates updates that are
        // processed in the background while this test is running.
        let metric1 = getCompleteMetricsValues(estimatesMetric);
        assertTrue(metric1 <= metric0, { metric1, metric0 });
      },

      testEstimatesShouldChangeWhenCreatingEdgeCollection: function () {
        const metric0 = getCompleteMetricsValues(estimatesMetric);

        db._createEdgeCollection(collectionName, collectionCreationOptions);

        // estimates should have changed because of edge index
        let metric1 = getCompleteMetricsValues(estimatesMetric);
        assertTrue(metric1 > metric0, { metric1, metric0 });
      },

      testEstimatesShouldNotChangeWhenCreatingPersistentIndexWithoutEstimates: function () {
        const metric0 = getCompleteMetricsValues(estimatesMetric);

        let c = db._create(collectionName, collectionCreationOptions);
        c.ensureIndex({ type: "persistent", fields: ["value"], estimates: false });

        // estimates should not have changed.
        // we allow it to go down here because from a previous testsuite
        // there may still be buffered index estimates updates that are
        // processed in the background while this test is running.
        let metric1 = getCompleteMetricsValues(estimatesMetric);
        assertTrue(metric1 <= metric0, { metric1, metric0 });
      },

      testEstimatesShouldChangeWhenCreatingPersistentIndexWithEstimates: function () {
        const metric0 = getCompleteMetricsValues(estimatesMetric);

        let c = db._create(collectionName, collectionCreationOptions);
        c.ensureIndex({ type: "persistent", fields: ["value"], estimates: true });

        // estimates should have changed.
        let metric1 = getCompleteMetricsValues(estimatesMetric);
        assertTrue(metric1 > metric0, { metric1, metric0 });
      },

      testEstimatesShouldNotChangeWhenCreatingUniquePersistentIndex: function () {
        const metric0 = getCompleteMetricsValues(estimatesMetric);

        let opts = {...collectionCreationOptions, "shardKeys": ["value"]};
        let c = db._create(collectionName, opts);
        c.ensureIndex({ type: "persistent", fields: ["value"], unique: true });

        // estimates should not have changed.
        let metric1 = getCompleteMetricsValues(estimatesMetric);
        assertTrue(metric1 <= metric0, { metric1, metric0 });
      },

      testEstimatesShouldIncreaseWhenInsertingIntoPersistentIndex: function () {
        // block sync thread from doing anything from now on
        IM.debugSetFailAt("RocksDBSettingsManagerSync");

        // wait until all pending estimates & revision tree buffers have been applied
        let res = waitForEstimatorSync();
        assertNull(res);

        const metric0 = getCompleteMetricsValues(estimatesMetric);

        let c = db._create(collectionName, collectionCreationOptions);
        c.ensureIndex({ type: "persistent", name: "persistent", fields: ["value"] });

        const metric1 = getCompleteMetricsValues(estimatesMetric);

        assertTrue(metric1 > metric0, { metric1, metric0 });

        insertDocuments(collectionName);

        let metric2 = getCompleteMetricsValues(estimatesMetric);
        // memory usage tracking. assumption is each index modification takes
        // at least 8 bytes. we do not expect the exact amount here, as it depends
        // on how many batches we will use etc.

        assertTrue(metric2 >= metric1 + n * 8, { metric2, metric1 });

        db._collection(collectionName).dropIndex("persistent");

        waitForMetricToBeAtMost(estimatesMetric, metric2);
      },

      testEstimatesShouldIncreaseWhenInsertingIntoEdgeIndex: function () {
        // block sync thread from doing anything from now on
        IM.debugSetFailAt("RocksDBSettingsManagerSync");

        // wait until all pending estimates & revision tree buffers have been applied
        let res = waitForEstimatorSync();
        assertNull(res);

        const metric0 = getCompleteMetricsValues(estimatesMetric);

        db._createEdgeCollection(collectionName, collectionCreationOptions);

        const metric1 = getCompleteMetricsValues(estimatesMetric);
        // estimates should have changed.
        assertTrue(metric1 > metric0, { metric1, metric0 });

        insertEdges(collectionName);

        // estimates should have changed.
        const metric2 = getCompleteMetricsValues(estimatesMetric);
        assertTrue(metric1 > metric0, { metric2, metric0 });

        db._drop(collectionName);

        waitForMetricToBeAtMost(estimatesMetric, metric2);
      }
    };
  };

}());
