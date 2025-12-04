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
const ERRORS = arangodb.errors;
let IM = global.instanceManager;
const {sleep, arango} = require('internal');

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

    testFailingQuery : function () {
      let st = db._createStatement({ query : "for i in" });

      try {
        st.parse();
        fail();
      } catch (e) {
        assertEqual(ERRORS.ERROR_QUERY_PARSE.code, e.errorNum);
      }

      let aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
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
        for (let i = 1; i <= 100; ++i) {
          assertEqual(i, cursor.next());
          assertEqual(i !== 100, cursor.hasNext());
        }
        assertFalse(cursor.hasNext());
      });

      aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
      assertEqual(aqlCurrentQueryMetric, 0);
    },

    testMetricAqlCurrentQueryFailurePoint: function () {
      let aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
      assertEqual(aqlCurrentQueryMetric, 0);

      try {
        IM.debugSetFailAt("Query::delayingExecutionPhase");
        const data = { query: `RETURN 1`, options: { cache: false } };
        const numberOfRunningQueries = 3;
        let cursorsIds = [];
        // Launch queries
        for (let i = 0; i < numberOfRunningQueries; ++i) {
          let res = arango.POST_RAW("/_api/cursor", JSON.stringify(data), {"x-arango-async": "store"});
          assertTrue(res.headers.hasOwnProperty("x-arango-async-id"));
          cursorsIds.push(res.headers["x-arango-async-id"]);
        }

        // Wait for queries to enter execution phase with timeout
        const maxIterations = 100; // 10 seconds with 0.1s sleep
        let iterations = 0;
        while (iterations < maxIterations) {
          aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
          if (aqlCurrentQueryMetric >= numberOfRunningQueries) {
            break;
          }
          sleep(0.1); // Small sleep to avoid busy waiting
          iterations++;
        }

        IM.debugRemoveFailAt("Query::delayingExecutionPhase");
        // Some other background queries might be executing
        assertTrue(aqlCurrentQueryMetric >= numberOfRunningQueries, 
                   `Expected at least ${numberOfRunningQueries} running queries, but got ${aqlCurrentQueryMetric} after 10s timeout`);

        // Make sure that the queries are cleaned
        for (let i = 0; i < numberOfRunningQueries; ++i) {
          arango.PUT_RAW('/_api/job/' + cursorsIds[i], "");
        }

        iterations = 0;
        // The metrics should eventually reach 0
        while (iterations < maxIterations) {
          aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
          if (aqlCurrentQueryMetric === 0) {
            break;
          }
          sleep(0.1);
          iterations++;
        }

        assertEqual(aqlCurrentQueryMetric, 0);
      } finally {
        IM.debugRemoveFailAt("Query::delayingExecutionPhase");
      }
    },
  };
}

jsunity.run(QueryMetricsTestSuite);
return jsunity.done();
