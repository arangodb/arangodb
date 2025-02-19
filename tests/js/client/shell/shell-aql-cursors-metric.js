/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertEqual */

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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const { getMetricSingle } = require("@arangodb/test-helper");
const queries = require("@arangodb/aql/queries");

const cn = "UnitTestsCollection";
  
function MetricsSuite () {
  'use strict';

  const name = "arangodb_aql_cursors_active";
      
  return {
    testMetricForCursorWithTtl: function () {
      const metricBefore = getMetricSingle(name);

      const n = 10;
      let ids = [];
      // create server-side cursors
      for (let i = 0; i < n; ++i) {
        let cursor = db._createStatement({query: "FOR i IN 1..1000 RETURN i", batchSize: 100, ttl: 60 }).execute();
        ids.push(cursor.data.id);
      }

      let metricAfter = getMetricSingle(name);
      // add some leeway in case some other, unrelated query from outside this test
      // was running in the before snapshot
      assertTrue(metricAfter >= metricBefore + (n - 1), { metricBefore, metricAfter });

      // delete all cursors
      ids.forEach((id) => {
        let res = arango.DELETE_RAW("/_api/cursor/" + id, "");
        assertEqual(202, res.code, res);
      });
      
      metricAfter = getMetricSingle(name);
      // add some leeway in case some other, unrelated query from outside this test
      // is running in the current snapshot
      assertTrue(metricAfter <= metricBefore + 1, { metricBefore, metricAfter });
    },

  };
}

jsunity.run(MetricsSuite);
return jsunity.done();
