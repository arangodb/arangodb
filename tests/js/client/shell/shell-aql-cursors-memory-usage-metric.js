/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertEqual */

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
const { getMetricSingle } = require("@arangodb/test-helper");
const queries = require("@arangodb/aql/queries");

const cn = "UnitTestsCollection";
  
function MetricsSuite () {
  'use strict';

  const name = "arangodb_aql_cursors_memory_usage";
      
  return {
    testMemoryUsageNonStream: function () {
      const metricBefore = getMetricSingle(name);

      const n = 10;
      let ids = [];
      // create server-side cursors
      for (let i = 0; i < n; ++i) {
        let cursor = db._createStatement({query: "FOR i IN 1..1000 * 100 RETURN CONCAT('this-is-a-string-return-value', i)", batchSize: 100, ttl: 60 }).execute();
        ids.push(cursor.data.id);
      }

      let metricAfter = getMetricSingle(name);
      assertTrue(metricAfter >= metricBefore + 32 * 1024 * 1024, { metricBefore, metricAfter });

      // delete all cursors
      ids.forEach((id) => {
        let res = arango.DELETE_RAW("/_api/cursor/" + id, "");
        assertEqual(202, res.code, res);
      });
      
      metricAfter = getMetricSingle(name);
      // add some buffer for arbitrary other background queries
      assertTrue(Math.abs(metricAfter - metricBefore) < 1024 * 1024, { metricBefore, metricAfter });
    },
    
    testMemoryUsageStream: function () {
      const metricBefore = getMetricSingle(name);

      const n = 10;
      let ids = [];
      // create server-side cursors
      for (let i = 0; i < n; ++i) {
        let cursor = db._createStatement({query: "FOR i IN 1..1000 * 100 RETURN CONCAT('this-is-a-string-return-value', i)", batchSize: 100, ttl: 60, stream: true }).execute();
        ids.push(cursor.data.id);
      }

      let metricAfter = getMetricSingle(name);
      assertTrue(metricAfter >= metricBefore + 10 * 2000, { metricBefore, metricAfter });

      // delete all cursors
      ids.forEach((id) => {
        let res = arango.DELETE_RAW("/_api/cursor/" + id, "");
        assertEqual(202, res.code, res);
      });
      
      metricAfter = getMetricSingle(name);
      // add some buffer for arbitrary other background queries
      assertTrue(Math.abs(metricAfter - metricBefore) < 10000, { metricBefore, metricAfter });
    },

  };
}

jsunity.run(MetricsSuite);
return jsunity.done();
