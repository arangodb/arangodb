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

function QueryMetricsTestSuite() {
  const dbName = "metricsTestDB";
  const collName = "col";
  let collection;

  return {
    setUpAll: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      collection = db._create(collName, {
        numberOfShards: 3,
      });

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      collection.save(docs);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testMetricAqlCurrentQueryNoRunningQueries: function () {
      const aqlCurrentQueryMetric = getMetricSingle(
        "arangodb_aql_current_query",
      );
      assertEqual(aqlCurrentQueryMetric, 0);
    },

    testMetricAqlCurrentQueryWithRunningQueries: function () {
      let aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
      assertEqual(aqlCurrentQueryMetric, 0);

      for (let i = 0; i < 1000; ++i) {
        const query = aql`
          FOR d IN ${collection}
          FILTER d.value > ${i}
          LIMIT 3
          RETURN d`;
        db._query(query);
      }

      aqlCurrentQueryMetric = getMetricSingle("arangodb_aql_current_query");
      assertEqual(aqlCurrentQueryMetric, 0);
    },
  };
}

jsunity.run(QueryMetricsTestSuite);
return jsunity.done();
