/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, assertUndefined, assertNotUndefined, assertMatch, assertNotMatch, fail  */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const getMetricSingle = require("@arangodb/test-helper").getMetricSingle;
const arangodb = require("@arangodb");
const aql = arangodb.aql;

function QueryMetricsTestSuite() {

  return {
    tearDownAll: function () {
      db._useDatabase("_system");
    },

    testMetricAqlCurrentQueryWithoutRunningQueries: function () {
      let aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
      assertEqual(aqlCurrentQueryMetric, 0);

      for (let i = 0; i < 1000; ++i) {
        const query = aql`
          FOR i IN 1..100 RETURN i`;
        db._query(query);
      }

      aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
      assertEqual(aqlCurrentQueryMetric, 0);
    },

    testMetricAqlCurrentQueryWithStreamingQueries: function () {
      let aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
      assertEqual(aqlCurrentQueryMetric, 0);

      let queries = [];
      for (let i = 0; i < 100; ++i) {
        let stmt = db._createStatement({ query: "FOR i IN 1..100 RETURN i",
                                       options: { stream: true },
                                       batchSize: 2});
        queries.push(stmt.execute());
      }
      aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
      assertEqual(aqlCurrentQueryMetric, 100);

      queries.forEach(function(cursor) {
        for (var i = 1; i <= 100; ++i) {
          assertEqual(i, cursor.next());
          assertEqual(i !== 100, cursor.hasNext());
        }
        assertFalse(cursor.hasNext());
      });

      aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
      assertEqual(aqlCurrentQueryMetric, 0);
    },
  };
}

jsunity.run(QueryMetricsTestSuite);
return jsunity.done();
