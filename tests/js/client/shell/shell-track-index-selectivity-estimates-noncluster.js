/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertEqual, assertNotEqual */

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
    return getMetricSingle("arangodb_internal_index_estimates_memory");
  };
  
  return {
    setUpAll: function () {
      // unfortunately this test may be affected by previous tests in 
      // other test suites that had created indexes with selectivity
      // estimates in other databases. database dropping executes the
      // dropping of the collection only slightly deferred, so it is
      // possible that a database is still being dropped (and estimates
      // are being shut down for the underlying collections) while this
      // test executes.
      internal.sleep(1);
    },

    tearDown: function () {
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
      const initial = getMetric();

      // creating an edge collection creates an edge index with 2 caches
      db._createEdgeCollection(cn);
      
      let metric = getMetric();
      assertTrue(metric > initial, { metric, initial });
    },

  };
}

jsunity.run(IndexEstimatesMemorySuite);
return jsunity.done();
