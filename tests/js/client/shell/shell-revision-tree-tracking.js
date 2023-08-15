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
const { getMetricSingle, getDBServersClusterMetricsByName } = require('@arangodb/test-helper');
const isCluster = require("internal").isCluster();
  
const cn = "UnitTestsCollection";

function RevisionTreeTrackingSuite () {
  'use strict';
      
  let getMetric = () => { 
    if (isCluster) {
      let sum = 0;
      getDBServersClusterMetricsByName("arangodb_revision_tree_buffered_memory_usage").forEach( num => {
        sum += num;
      });
      return sum;
    } else {
      return getMetricSingle("arangodb_revision_tree_buffered_memory_usage");
    }
  };

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
    if (docs.length > 0) {
      c.insert(docs);
      docs = [];
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
    if (docs.length > 0) {
      c.insert(docs);
      docs = [];
    }
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
      
      let initial = getMetric();
      db._create(cn);
      insertDocuments(cn);

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
      
      let initial = getMetric();
      let c = db._create(cn);
      for (let i = 0; i < n; ++i) {
        c.insert({value: i});
      }

      // must have more memory allocated for the documents
      let metric = getMetric();
      // 48 = sizeof(void*) + sizeof(decltype(_revisionInsertBuffers)::mapped_type).
      // must be changed when type of RocksDBMetaCollection::_revisionInsertBuffers
      // changes.
      assertTrue(metric >= initial + n * 48, { metric, initial });
    },
    
  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(RevisionTreeTrackingSuite);
}

return jsunity.done();
