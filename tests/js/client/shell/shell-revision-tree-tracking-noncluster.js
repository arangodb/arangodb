/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertNull */

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

function RevisionTreeTrackingSuite () {
  'use strict';
      
  let getMetric = () => { 
    return getMetricSingle("arangodb_revision_tree_buffered_memory_usage");
  };
  
  return {
    setUp: function () {
      internal.debugClearFailAt();
    },

    tearDown: function () {
      internal.debugClearFailAt();
      db._drop(cn);
    },
    
    testMemoryUsageShouldIncreaseAfterBatchedInserts: function () {
      // wait until all pending estimates & revision tree buffers have been applied
      let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
      
      // block sync thread from doing anything from now on
      internal.debugSetFailAt("RocksDBSettingsManagerSync");
      
      // wait until all pending estimates & revision tree buffers have been applied
      res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
      
      const initial = getMetric();

      const n = 100000;

      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({value: i});
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }

      // must have more memory allocated for the documents
      let metric = getMetric();
      assertTrue(metric >= initial + n * 8, { metric, initial });
    },
    
    testMemoryUsageShouldIncreaseAfterSingleInserts: function () {
      // wait until all pending estimates & revision tree buffers have been applied
      let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
      
      // block sync thread from doing anything from now on
      internal.debugSetFailAt("RocksDBSettingsManagerSync");
      
      // wait until all pending estimates & revision tree buffers have been applied
      res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
      
      const initial = getMetric();

      const n = 10000;

      let c = db._create(cn);
      for (let i = 0; i < n; ++i) {
        c.insert({value: i});
      }

      // must have more memory allocated for the documents
      let metric = getMetric();
      assertTrue(metric >= initial + n * (8 + 32), { metric, initial });
    },
    
  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(RevisionTreeTrackingSuite);
}

return jsunity.done();
