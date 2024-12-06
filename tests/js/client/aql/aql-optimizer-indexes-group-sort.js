/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual */

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
/// @author Julia Volmer
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
const isCluster = require("internal").isCluster();
const waitForEstimatorSync = require('@arangodb/test-helper').waitForEstimatorSync;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerIndexesGroupSortTestSuite() {
  var c;
  create_collection = function (row_count) {
    db._drop("UnitTestsCollection");
    c = db._create("UnitTestsCollection");

    let docs = [];
    for (let i = 0; i < row_count; ++i) {
      docs.push({ _key: "test" + i, first_index_field: i % 9, non_indexed_field: 9 - (i % 10), second_index_field: Math.random() });
    }
    c.insert(docs);
    c.ensureIndex({ type: "persistent", fields: ["first_index_field", "second_index_field"] });
    waitForEstimatorSync();
  };

  return {

    tearDown: function () {
      db._drop("UnitTestsCollection");
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    testMultiSortUsesPartOfPersistentIndex: function () {
      const row_count = 2;
      create_collection(row_count);

      const query = "FOR i IN " + c.name() + " SORT i.first_index_field, i.non_indexed_field RETURN i.first_index_field";

      const plan = db._createStatement(query).explain().plan;

      assertTrue(["use-indexes", "use-index-for-sort"].every((rule) => plan.rules.indexOf(rule) >= 0), plan.rules);

      const sort_nodes = plan.nodes.filter((node) => node.type == "SortNode")
      assertEqual(1, sort_nodes.length);
      const sort_node = sort_nodes[0];

      const index_nodes = plan.nodes.filter((node) => node.type == "IndexNode")
      assertEqual(1, index_nodes.length);
      // const index_node = index_nodes[0];
      // const grouped_ids = index_node.projections.filter((p) => p.path.includes("value")).map((p) => p.variable.id)
      // assertEqual(1, grouped_ids.length);
      // const grouped_ids_in_sort_node = sort_node.elements.filter((e) => e.inVariable.id == grouped_ids[0]).map((e) => assertTrue(e.grouped));
      // assertEqual(1, grouped_ids_in_sort_node.length);

      var results = db._query(query);
      var expected = [];
      for (var j = 0; j < row_count; ++j) {
        expected.push(j % 9);
      }
      assertEqual(expected, results.toArray(), query);
      assertEqual(0, results.getExtra().stats.scannedFull);
      if (isCluster) {
        assertEqual(expected.length, results.getExtra().stats.scannedIndex);
      }
    },

    testLimit: function () {
      const queries = [
        "FOR i IN " + c.name() + " SORT i.value, i.value3 RETURN i.value LIMIT count",
        "FOR i IN " + c.name() + " SORT i.value, i.value3 RETURN i.value LIMIT offset, count"
      ];
    },

    testDifferentNumberOfRows: function () {
      const number_of_rows = [1, 2, 600, 800, 999, 1000, 1001, 1999, 2000, 2001, 4999, 5000, 5001];
    },

    testSortingHasToStartWithPersistentIndexFieldsToUseOptimizerRule: function () {
      const query = "FOR i IN " + c.name() + " SORT i.second_index_field, i.random_field RETURN i.value";
    },

    // what if sorting is done in different direction than index?

    // what if sort one asc and one desc?
  };
}

jsunity.run(optimizerIndexesGroupSortTestSuite);

return jsunity.done();
