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
const { getCompleteMetricsValues } = require('@arangodb/test-helper');

  
const cn = "UnitTestsCollection";
const n = 10000;

function RevisionTreeTrackingSuite () {
  'use strict';
       
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
    
    testRevisionTreeMemoryUsageShouldIncreaseAfterBatchedInserts: function () {
      // wait until all pending estimates & revision tree buffers have been applied
      let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
      
      // block sync thread from doing anything from now on
      internal.debugSetFailAt("RocksDBSettingsManagerSync");
      
      // wait until all pending estimates & revision tree buffers have been applied
      res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
      
      db._create(cn);
      let metric0 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
      insertDocuments(cn);

      // must have more memory allocated for the documents
      let metric1 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
      assertTrue(metric1 >= metric0 + n * 8, { metric1, metric0 });

      // Now check that metric value will go down
      internal.debugClearFailAt();

      let tries = 0;
      let metric2;
      while (++tries < 120) {
        metric2 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
        if (0 === metric2) {
          break;
        }
        require("internal").sleep(0.5);
      }

      assertTrue(metric2 === 0);
    },
    
    testRevisionTreeMemoryUsageShouldIncreaseAfterSingleInserts: function () {
      // wait until all pending estimates & revision tree buffers have been applied
      let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
      
      // block sync thread from doing anything from now on
      internal.debugSetFailAt("RocksDBSettingsManagerSync");
      
      // wait until all pending estimates & revision tree buffers have been applied
      res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
      
      let c = db._create(cn);
      let metric0 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");

      for (let i = 0; i < n; ++i) {
        c.insert({value: i});
      }
      
      // must have more memory allocated for the documents
      let metric1 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
      // 48 = sizeof(void*) + sizeof(decltype(_revisionInsertBuffers)::mapped_type).
      // must be changed when type of RocksDBMetaCollection::_revisionInsertBuffers
      // changes.
      assertTrue(metric1 >= metric0 + n * 48, { metric1, metric0 });

      // Now check that metric value will go down
      internal.debugClearFailAt();

      let tries = 0;
      let metric2;
      while (++tries < 120) {
        // internal.debugClearFailAt();
        metric2 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
        if (0 === metric2) {
          break;
        }
        require("internal").sleep(0.5);
      }

      assertTrue(metric2 === 0);
    },

    testRevisionTreeMemoryUsageShouldIncreaseAndDecreaseAfterAql: function () {
      // wait until all pending estimates & revision tree buffers have been applied
      let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
          
      // wait until all pending estimates & revision tree buffers have been applied
      res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
      assertNull(res);
      
      // Check that metric value is 0 before inserting documents

      internal.debugClearFailAt();

      let tries = 0;
      let metric0;
      while (++tries < 120) {
        metric0 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
        if (0 === metric0) {
          break;
        }
        require("internal").sleep(0.5);
      }

      assertTrue(metric0 === 0);

      // Create collection and insert documents 
      let c = db._create(cn);
      db._query(`for i in 1..${n} insert {value: i} into ${cn}`);

      let metric1;
      while (++tries < 120) {
        metric1 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
        if (0 === metric1) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertTrue(0 === metric1);
    }
  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(RevisionTreeTrackingSuite);
}

return jsunity.done();
