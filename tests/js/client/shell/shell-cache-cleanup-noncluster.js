/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertEqual, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for inventory
///
/// @file
///
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

function CacheCleanupSuite () {
  'use strict';
      
  let getMetric = () => { 
    return getMetricSingle("rocksdb_cache_active_tables");
  };
  
  return {
    tearDown: function () {
      db._drop(cn);
    },
    
    testCleanupAfterDropCollectionWithCacheEnabled: function () {
      const initial = getMetric();

      // enable caching for documents
      db._create(cn, { cacheEnabled: true });
      
      // must have one cache more for the documents
      let metric = getMetric();
      assertTrue(metric > initial, { metric, initial });

      db._drop(cn);

      // dropping the collection should also remove the cache
      metric = getMetric();
      assertEqual(metric, initial);
    },
    
    testCleanupAfterDropEdgeCollection: function () {
      const initial = getMetric();

      // creating an edge collection creates an edge index with 2 caches
      db._createEdgeCollection(cn);
      
      let metric = getMetric();
      // 2 caches more: one for _from, one for _to
      assertTrue(metric > initial + 1, { metric, initial });

      // this must remove the 2 caches
      db._drop(cn);

      metric = getMetric();
      assertEqual(metric, initial);
    },

    testCleanupAfterDropCollectionWithPersistentIndex: function () {
      const initial = getMetric();

      let c = db._create(cn);
      
      let metric = getMetric();
      assertEqual(metric, initial);
      
      // creating a persistent index with cacheEnabled creates one cache
      c.ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
      metric = getMetric();
      assertTrue(metric > initial, { metric, initial });

      db._drop(cn);

      metric = getMetric();
      assertEqual(metric, initial);
    },
    
    testCleanupAfterDropDatabase: function () {
      const initial = getMetric();

      db._createDatabase(cn);
      try {
        db._useDatabase(cn);

        let c = db._create(cn);
        
        let metric = getMetric();
        assertEqual(metric, initial);
        
        db._createEdgeCollection(cn + "edge");
      
        metric = getMetric();
        // 2 caches more: one for _from, one for _to
        assertTrue(metric > initial + 1, { metric, initial });
        
        c.ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
        metric = getMetric();
        assertTrue(metric > initial + 2, { metric, initial });

        db._create(cn + "cache", { cacheEnabled: true });
        metric = getMetric();
        assertTrue(metric > initial + 3, { metric, initial });

        // drop one of the collections
        db._drop(cn + "cache");
        metric = getMetric();
        assertTrue(metric > initial + 2, { metric, initial });
        
        db._useDatabase("_system");
        db._dropDatabase(cn);

        // wait until the database object gets physically dropped.
        // this is carried out by a background thread and can take some time.
        let tries = 30;
        while (tries-- > 0) {
          metric = getMetric();

          if (metric === initial) {
            break;
          }
          internal.sleep(1);
        }
        assertEqual(metric, initial, { metric, initial, tries });
      } finally {
        db._useDatabase("_system");
        try {
          db._dropDatabase(cn);
        } catch (err) {
          // may have been dropped already
        }
      }
    },

  };
}

jsunity.run(CacheCleanupSuite);
return jsunity.done();
