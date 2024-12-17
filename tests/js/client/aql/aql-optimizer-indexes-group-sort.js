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

  const create_collection = function () {
    db._drop("UnitTestsCollection");
    // TODO make it work with shards
    // return db._create("UnitTestsCollection", { "numberOfShards": 9 });
    return db._create("UnitTestsCollection");
  };
  const copy_collection = function (collection_name) {
    db._drop("UnitTestsExpectedCollection");
    let new_collection = db._create("UnitTestsExpectedCollection");
    db._query("For row in " + collection_name + " INSERT row into " + new_collection.name());
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
  const sort_node_does_a_group_sort = function (plan) {
    const sort_nodes = plan.nodes.filter((node) => node.type === "SortNode");
    if (sort_nodes.length !== 1) {
      return false;
    }
    const sort_node = sort_nodes[0];
    if (sort_node.strategy !== "grouped") {
      return false;
    }
    if (sort_node.numberOfTopGroupedElements !== 1) {
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

        const query = "FOR i IN @@collection SORT i.first_index_field, i.non_indexed_field RETURN {first_index_field: i.first_index_field, non_indexed_field: i.non_indexed_field, second_index_field: i.second_index_field}";
        let plan = query_plan(query, collection.name());
        assertTrue(query_plan_uses_index_for_sorting(plan), row_count + ': ' + plan.rules);
        assertTrue(sort_node_does_a_group_sort(plan), row_count + ': ' + plan.nodes);
        assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), row_count + ': ' + query);
      }
    },

    test_uses_index_in_sort_after_index_creation: function () {
      const collection = create_collection();
      collection.insert(Array.from({ length: 100 }, (_, index) => index).map(i => {
        let random_val = Math.floor(Math.random() * 10);
        return {
          first_index_field: i % 9,
          second_index_field: random_val
        };
      }));

      const query = "FOR i IN @@collection SORT i.first_index_field RETURN i.second_index_field";

      let plan = query_plan(query, collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);

      collection.ensureIndex({ type: "persistent", fields: ["first_index_field", "second_index_field"] });
      waitForEstimatorSync();

      plan = query_plan(query, collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan);
      // here the index is already doing all the sorting, therefore no group sort is required
      assertFalse(sort_node_does_a_group_sort(plan), plan.nodes);
    },

    test_does_not_use_index_when_sort_registers_are_not_in_same_order_as_index: function () {
      const collection = create_collection();
      collection.insert(Array.from({ length: 100 }, (_, index) => index).map(i => {
        let random_val = Math.floor(Math.random() * 10);
        return {
          first_index_field: i % 9,
          second_index_field: random_val
        };
      }));
      collection.ensureIndex({ type: "persistent", fields: ["first_index_field", "second_index_field"] });
      waitForEstimatorSync();

      let plan = query_plan("FOR i IN @@collection SORT i.second_index_field RETURN i.second_index_field", collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);

      plan = query_plan("FOR i IN @@collection SORT i.second_index_field, i.first_index_field RETURN i.second_index_field", collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
    },

    test_sorting_in_different_direction_than_index: function () {
      const collection = create_collection();
      collection.insert(Array.from({ length: 100 }, (_, index) => index).map(i => {
        let random_val = Math.floor(Math.random() * 10);
        return {
          first_index_field: i % 9,
          non_indexed_field: 100 - i - 1,
          second_index_field: random_val,
          third_index_field: random_val
        };
      }));
      collection.ensureIndex({ type: "persistent", fields: ["first_index_field", "second_index_field", "third_index_field"] });
      waitForEstimatorSync();

      // all desc should work
      let plan = query_plan("FOR i IN @@collection SORT i.first_index_field DESC, i.non_indexed_field DESC RETURN i.second_index_field", collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertTrue(sort_node_does_a_group_sort(plan), plan.nodes);

      // TODO all sorted index fields desc should work (currently does not work)

      // combined desc and asc in index fields should not work
      plan = query_plan("FOR i IN @@collection SORT i.first_index_field, i.second_index_field DESC, i.non_indexed_field DESC RETURN i.second_index_field", collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
    },

    // limit only uses group sort when sorting is done stable
    // but I don't see where I can change this, SortNode always created with stable = false
    // test_limit: function () {
    //   const collection = create_collection();
    //   collection.insert(Array.from({ length: 1001 }, (_, index) => index).map(i => {
    //     let random_val = Math.floor(Math.random() * 10);
    //     return {
    //       first_index_field: i % 9,
    //       non_indexed_field: 3000 - i - 1,
    //       second_index_field: random_val
    //     };
    //   }));
    //   collection.ensureIndex({ type: "persistent", fields: ["first_index_field", "second_index_field"] });
    //   waitForEstimatorSync();

    //   for (let count of [1]) {//[0, 1, 100, 1000, 1001, 2000]) {

    //     // TODO currently does not use group sort for some reason
    //     // const query = "FOR i IN @@collection SORT i.first_index_field, i.non_indexed_field LIMIT " + count + " RETURN i.second_index_field";
    //     // let plan = query_plan(query, collection.name());
    //     // assertTrue(["use-indexes", "use-index-for-sort"].every((rule) => plan.rules.indexOf(rule) >= 0), plan.rules);
    //     // assert_query_does_sorting_correctly(query, collection.name());

    //     // this correctly does use group sort
    //     for (let offset of [0]) {//, 1, 100, 1000, 1001, 2000]) {
    //       const query = "FOR i IN @@collection SORT i.first_index_field, i.non_indexed_field LIMIT " + offset + ", " + count + " RETURN [i.first_index_field, i.non_indexed_field, i.second_index_field]";

    //       let plan = query_plan(query, collection.name());
    //       assertTrue(["use-indexes", "use-index-for-sort"].every((rule) => plan.rules.indexOf(rule) >= 0), plan.rules);
    //       assert_query_plan_defines_groups(plan);

    //       let results = execute(query, collection.name());
    //       let expected = expected_results(query, collection.name());
    //       // for (i = 0; i < results.length; i++) {
    //       //   assertEqual(expected[i], results[i], query + " at position " + i);
    //       // }
    //       const query_without_limit = "FOR i IN @@collection SORT i.first_index_field, i.non_indexed_field RETURN [i.first_index_field, i.non_indexed_field, i.second_index_field]";
    //       assertEqual(expected, results, db._explain(query, { "@collection": collection.name() }));//, db._query(query_without_limit, { "@collection": collection.name() }));
    //       // assert_query_does_sorting_correctly(query, collection.name());
    //     }
    //   }
    // },

  };
}


jsunity.run(optimizerIndexesGroupSortTestSuite);

return jsunity.done();
