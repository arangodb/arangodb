/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertEqual, assertNull */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const internal = require("internal");
const { getCompleteMetricsValues } = require('@arangodb/test-helper');
  
const cn = "UnitTestsCollection";

function IndexEstimatesMemorySuite () {
  'use strict';

  const n = 10000;
  
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
      docs.push({_from: i, _to: i+1});
      if (docs.length === 5000) {
        c.insert(docs);
        docs = [];
      }
    }
  };
  
  return {
    setUp: function () {
      // wait until all pending estimates & revision tree buffers have been applied
      let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
    },

    tearDown: function () {
      internal.debugClearFailAt();
      db._drop(cn);
    },
    
    testEstimatesShouldNotChangeWhenCreatingDocumentCollection: function () {
      const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");

      db._create(cn);
      
      // estimates should not have changed.
      // we allow it to go down here because from a previous testsuite
      // there may still be buffered index estimates updates that are
      // processed in the background while this test is running.
      let metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      assertTrue(metric1 <= metric0, { metric1, metric0 });
    },

    testEstimatesShouldChangeWhenCreatingEdgeCollection: function () {
      const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");

      db._createEdgeCollection(cn);
      
      // estimates should have changed because of edge index
      let metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      assertTrue(metric1 > metric0, { metric1, metric0 });
    },
    
    testEstimatesShouldNotChangeWhenCreatingPersistentIndexWithoutEstimates: function () {
      const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");

      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], estimates: false });
      
      // estimates should not have changed.
      // we allow it to go down here because from a previous testsuite
      // there may still be buffered index estimates updates that are
      // processed in the background while this test is running.
      let metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      assertTrue(metric1 <= metric0, { metric1, metric0 });
    },
    
    testEstimatesShouldChangeWhenCreatingPersistentIndexWithEstimates: function () {
      const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");

      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], estimates: true });
      
      // estimates should have changed.
      let metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      assertTrue(metric1 > metric0, { metric1, metric0 });
    },

    testEstimatesShouldNotChangeWhenCreatingUniquePersistentIndex: function () {
      const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");

      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], unique: true });
      
      // estimates should not have changed.
      let metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      assertTrue(metric1 <= metric0, { metric1, metric0 });
    },
    
    testEstimatesShouldIncreaseWhenInsertingIntoPersistentIndex: function () {
      // block sync thread from doing anything from now on
      internal.debugSetFailAt("RocksDBSettingsManagerSync");

      // wait until all pending estimates & revision tree buffers have been applied
      let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);

      const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");

      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", name: "persistent", fields: ["value"] });

      const metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");

      assertTrue(metric1 > metric0, { metric1, metric0 });

      insertDocuments(cn);

      let metric2 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      // memory usage tracking. assumption is each index modification takes
      // at least 8 bytes. we do not expect the exact amount here, as it depends
      // on how many batches we will use etc.

      assertTrue(metric2 >= metric1 + n * 8, { metric2, metric1 });

      db._collection(cn).dropIndex("persistent");

      let metric3 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      let timeout = 30;
      while (metric3 > metric2) {
        if (timeout == 0) {
          throw "arangodb_index_estimates_memory_usage value is not decreased";
        }
        timeout -= 1;
        require("internal").sleep(1);
        metric3 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      }
    },

    testEstimatesShouldIncreaseWhenInsertingIntoEdgeIndex: function () {
      // block sync thread from doing anything from now on
      internal.debugSetFailAt("RocksDBSettingsManagerSync");

      // wait until all pending estimates & revision tree buffers have been applied
      let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);

      const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");

      db._createEdgeCollection(cn);
      
      const metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      // estimates should have changed.
      assertTrue(metric1 > metric0, { metric1, metric0 });

      insertEdges(cn);

      // estimates should have changed.
      const metric2 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      assertTrue(metric1 > metric0, { metric2, metric0 });

      db._drop(cn);
      let metric3 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      let timeout = 30;
      while (metric3 > metric2) {
        if (timeout == 0) {
          throw "arangodb_index_estimates_memory_usage value is not decreased";
        }
        timeout -= 1;
        require("internal").sleep(1);
        metric3 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      }
    },
  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(IndexEstimatesMemorySuite);
}
return jsunity.done();
