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
const { getMetricSingle } = require('@arangodb/test-helper');
  
const cn = "UnitTestsCollection";

function IndexEstimatesMemorySuite () {
  'use strict';
      
  let getMetric = () => { 
    return getMetricSingle("arangodb_index_estimates_memory_usage");
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
      const initial = getMetric();

      db._create(cn);
      
      // estimates should not have changed.
      // we allow it to go down here because from a previous testsuite
      // there may still be buffered index estimates updates that are
      // processed in the background while this test is running.
      let metric = getMetric();
      assertTrue(metric <= initial, { metric, initial });
    },
    
    testEstimatesShouldNotChangeWhenCreatingPersistentIndexWithoutEstimates: function () {
      const initial = getMetric();

      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], estimates: false });
      
      // estimates should not have changed.
      // we allow it to go down here because from a previous testsuite
      // there may still be buffered index estimates updates that are
      // processed in the background while this test is running.
      let metric = getMetric();
      assertTrue(metric <= initial, { metric, initial });
    },
    
    testEstimatesShouldChangeWhenCreatingPersistentIndexWithEstimates: function () {
      const initial = getMetric();

      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], estimates: true });
      
      // estimates should not have changed.
      // we allow it to go down here because from a previous testsuite
      // there may still be buffered index estimates updates that are
      // processed in the background while this test is running.
      let metric = getMetric();
      assertTrue(metric > initial, { metric, initial });
    },
    
    testEstimatesShouldIncreaseWithEdgeIndex: function () {
      // block sync thread from doing anything from now on
      internal.debugSetFailAt("RocksDBSettingsManagerSync");

      // wait until all pending estimates & revision tree buffers have been applied
      let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);

      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"] });
      
      const initial = getMetric();

      const n = 10000;

      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({value: i});
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
      
      let metric = getMetric();
      // memory usage tracking. assumption is each index modification takes
      // at least 8 bytes. we do not expect the exact amount here, as it depends
      // on how many batches we will use etc.
      assertTrue(metric >= initial + n * 8, { metric, initial });
    },

    testEstimatesShouldWhenInsertingIntoPersistentIndex: function () {
    },
  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(IndexEstimatesMemorySuite);
}
return jsunity.done();
