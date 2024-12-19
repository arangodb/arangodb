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
const waitForEstimatorSync = require('@arangodb/test-helper').waitForEstimatorSync;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerIndexesGroupSortTestSuite() {

  const create_collection = function () {
    db._drop("UnitTestsCollection");
    return db._create("UnitTestsCollection", { "numberOfShards": 9 });
  };
  const copy_collection = function (collection_name) {
    db._drop("UnitTestsExpectedCollection");
    let new_collection = db._create("UnitTestsExpectedCollection");
    db._query("FOR row IN " + collection_name + " INSERT row INTO " + new_collection.name());
    return new_collection;
  };
  const query_plan = function (query, collection_name) {
    const stmt = db._createStatement(query);
    stmt.bind({ "@collection": collection_name });
    return stmt.explain().plan;
  };
  const query_plan_uses_index_for_sorting = function (plan) {
    return ["use-indexes", "use-index-for-sort"].every((rule) => plan.rules.indexOf(rule) >= 0);
  };
  const sort_node_does_a_group_sort = function (plan, numberOfGroupedElements = 1) {
    const sort_nodes = plan.nodes.filter((node) => node.type === "SortNode");
    if (sort_nodes.length !== 1) {
      return false;
    }
    const sort_node = sort_nodes[0];
    if (sort_node.strategy !== "grouped") {
      return false;
    }
    if (sort_node.numberOfTopGroupedElements !== numberOfGroupedElements) {
      return false;
    }
    return true;
  };
  const execute = function (query, collection_name) {
    return db._query(query, { "@collection": collection_name }).toArray();
  };
  const expected_results = function (query, collection_name) {
    const c_expected = copy_collection(collection_name);
    return db._query(query, { "@collection": c_expected.name() }).toArray();
  };

  return {

    tearDown: function () {
      db._drop("UnitTestsCollection");
      db._drop("UnitTestsExpectedCollection");
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    test_indexed_group_sort_gives_same_results_as_unindexed_sort: function () {
      for (let row_count of [1, 2, 10, 100, 800, 1000, 1001, 2501, 10000]) {
        const collection = create_collection();
        collection.insert(Array.from({ length: row_count }, (_, index) => index).map(i => {
          let random_val = Math.floor(Math.random() * 10);
          return {
            first_index_field: i % 9,
            non_indexed_field: row_count - i - 1,
            second_index_field: random_val
          };
        }));
        collection.ensureIndex({ type: "persistent", fields: ["first_index_field", "second_index_field"] });
        waitForEstimatorSync();

        const query = "FOR doc IN @@collection SORT doc.first_index_field, doc.non_indexed_field RETURN doc";
        let plan = query_plan(query, collection.name());
        assertTrue(query_plan_uses_index_for_sorting(plan), row_count + ': ' + plan.rules);
        assertTrue(sort_node_does_a_group_sort(plan), row_count + ': ' + plan.nodes);
        assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), row_count + ': ' + query);
      }
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    test_uses_index_when_sort_registers_start_with_index_fields: function () {
      const collection = create_collection();
      collection.ensureIndex({ type: "persistent", fields: ["first_index_field", "second_index_field", "third_index_field"] });
      waitForEstimatorSync();

      let plan = query_plan("FOR doc IN @@collection SORT doc.first_index_field RETURN doc", collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      // here the index is already doing all the sorting, therefore no group sort is required
      assertFalse(sort_node_does_a_group_sort(plan), plan.nodes);

      plan = query_plan("FOR doc IN @@collection SORT doc.first_index_field, doc.second_index_field RETURN doc", collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      // here the index is already doing all the sorting, therefore no group sort is required
      assertFalse(sort_node_does_a_group_sort(plan), plan.nodes);

      plan = query_plan("FOR doc IN @@collection SORT doc.first_index_field, doc.second_index_field, doc.third_index_field RETURN doc", collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      // here the index is already doing all the sorting, therefore no group sort is required
      assertFalse(sort_node_does_a_group_sort(plan), plan.nodes);

      plan = query_plan("FOR doc IN @@collection SORT doc.first_index_field, doc.non_indexed_field RETURN doc", collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertTrue(sort_node_does_a_group_sort(plan), plan.nodes);

      plan = query_plan("FOR doc IN @@collection SORT doc.first_index_field, doc.non_indexed_field, doc.second_index_field RETURN doc", collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertTrue(sort_node_does_a_group_sort(plan), plan.nodes);

      plan = query_plan("FOR doc IN @@collection SORT doc.first_index_field, doc.second_index_field, doc.non_indexed_field RETURN doc", collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertTrue(sort_node_does_a_group_sort(plan, 2), plan.nodes);
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    test_does_not_use_index_when_sort_registers_are_not_in_same_order_as_index: function () {
      const collection = create_collection();
      collection.ensureIndex({ type: "persistent", fields: ["first_index_field", "second_index_field"] });
      waitForEstimatorSync();

      let plan = query_plan("FOR doc IN @@collection SORT doc.second_index_field RETURN doc", collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);

      plan = query_plan("FOR doc IN @@collection SORT doc.second_index_field, doc.first_index_field RETURN doc", collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    test_sorting_in_different_direction: function () {
      const collection = create_collection();
      collection.ensureIndex({ type: "persistent", fields: ["first_index_field", "second_index_field", "third_index_field"] });
      waitForEstimatorSync();

      // all desc should work
      let plan = query_plan("FOR doc IN @@collection SORT doc.first_index_field DESC, doc.non_indexed_field DESC RETURN doc", collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertTrue(sort_node_does_a_group_sort(plan), plan.nodes);

      // TODO all sorted index fields desc should work (currently does not work)
      plan = query_plan("FOR doc IN @@collection SORT doc.first_index_field DESC, doc.non_indexed_field ASC RETURN doc", collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules); // TODO should be true
      // assertTrue(sort_node_does_a_group_sort(plan), plan.nodes); // TODO should work

      // combined desc and asc in index fields should not work
      plan = query_plan("FOR doc IN @@collection SORT doc.first_index_field ASC, doc.second_index_field DESC, doc.non_indexed_field DESC RETURN doc", collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
    },

  };
}

jsunity.run(optimizerIndexesGroupSortTestSuite);

return jsunity.done();
