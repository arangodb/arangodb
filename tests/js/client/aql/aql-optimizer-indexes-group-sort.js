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
    return db._create("UnitTestsCollection", {"numberOfShards": 9});
  };
  const copy_collection = function (collection_name) {
    db._drop("UnitTestsExpectedCollection");
    let new_collection = db._create("UnitTestsExpectedCollection");
    db._query("FOR row IN " + collection_name + " INSERT row INTO " + new_collection.name());
    return new_collection;
  };
  const query_plan = function (query, collection_name) {
    const stmt = db._createStatement(query);
    stmt.bind({"@collection": collection_name});
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
    // for a group sort there is at least one element that is sorted by
    if (sort_node.elements.length <= numberOfGroupedElements) {
      return false;
    }
    return true;
  };
  const execute = function (query, collection_name) {
    return db._query(query, {"@collection": collection_name}).toArray();
  };
  const expected_results = function (query, collection_name) {
    const c_expected = copy_collection(collection_name);
    return db._query(query, {"@collection": c_expected.name()}).toArray();
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
        collection.insert(Array.from({length: row_count}, (_, index) => index).map(i => {
          let random_val = Math.floor(Math.random() * 10);
          return {
            a: i % 9,
            x: row_count - i - 1,
            b: random_val
          };
        }));
        collection.ensureIndex({type: "persistent", fields: ["a", "b"]});
        waitForEstimatorSync();

        const query = "FOR doc IN @@collection SORT doc.a, doc.x RETURN [doc.a, doc.x, doc.b]";
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
      collection.insert(Array.from({length: 10}, (_, index) => index).map(i => {
        let random_val = Math.floor(Math.random() * 10);
        return {
          a: i % 9,
          x: 100 - i - 1,
          b: random_val,
          c: i
        };
      }));
      collection.ensureIndex({type: "persistent", fields: ["a", "b", "c"]});
      waitForEstimatorSync();

      // queries that are fully covered by index, no additional sorting needed
      let queries = [
        "FOR doc IN @@collection SORT doc.a RETURN [doc.a]",
        "FOR doc IN @@collection SORT doc.a, doc.b RETURN [doc.a, doc.x, doc.b, doc.c]",
        "FOR doc IN @@collection SORT doc.a, doc.b, doc.c RETURN [doc.a, doc.x, doc.b, doc.c]"
      ];
      for (let query of queries) {
        let plan = query_plan(query, collection.name());
        assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
        // here the index is already doing all the sorting, therefore no group sort is required
        assertFalse(sort_node_does_a_group_sort(plan), plan.nodes);
        assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);
      }

      queries = [
        [1, "FOR doc IN @@collection SORT doc.a, doc.x RETURN [doc.a, doc.x, doc.b, doc.c]"],
        [1, "FOR doc IN @@collection SORT doc.a, doc.x, doc.b RETURN [doc.a, doc.x, doc.b, doc.c]"],
        [2, "FOR doc IN @@collection SORT doc.a, doc.b, doc.x RETURN [doc.a, doc.x, doc.b, doc.c]"]
      ];
      for (let [numberOfGroupedElements, query] of queries) {
        let plan = query_plan(query, collection.name());
        assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
        assertTrue(sort_node_does_a_group_sort(plan, numberOfGroupedElements), plan.nodes);
        assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);
      }
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    test_does_not_use_index_when_sort_registers_are_not_in_same_order_as_index: function () {
      const collection = create_collection();
      collection.insert(Array.from({length: 100}, (_, index) => index).map(i => {
        let random_val = Math.floor(Math.random() * 10);
        return {
          a: i % 9,
          x: 100 - i - 1,
          b: random_val
        };
      }));
      collection.ensureIndex({type: "persistent", fields: ["a", "b"]});
      waitForEstimatorSync();

      let query = "FOR doc IN @@collection SORT doc.b RETURN [doc.a, doc.x, doc.b]";
      let plan = query_plan(query, collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);

      query = "FOR doc IN @@collection SORT doc.b, doc.a RETURN [doc.a, doc.x, doc.b]";
      plan = query_plan(query, collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    test_sorting_in_different_direction: function () {
      const collection = create_collection();
      collection.ensureIndex({type: "persistent", fields: ["a", "b", "c"]});
      waitForEstimatorSync();

      // all desc should work
      let plan = query_plan("FOR doc IN @@collection SORT doc.a DESC, doc.x DESC RETURN doc", collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertTrue(sort_node_does_a_group_sort(plan), plan.nodes);

      // TODO all sorted index fields desc should work (currently does not work)
      plan = query_plan("FOR doc IN @@collection SORT doc.a DESC, doc.x ASC RETURN doc", collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules); // TODO should be true
      // assertTrue(sort_node_does_a_group_sort(plan), plan.nodes); // TODO should work

      // combined desc and asc in index fields should not work
      plan = query_plan("FOR doc IN @@collection SORT doc.a ASC, doc.b DESC, doc.x DESC RETURN doc", collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
    },

  };
}

function optimizerIndexesGroupSortMultiTestSuite() {

  const database = "GroupSortTestDb";
  const collection = "MyCollection";
  const fields = ["a", "b", "c", "d"];
  const coveredFields = ["a", "b", "c"];
  const numberOfSteps = 4;

  const permute = function* (permutation) {
    var length = permutation.length,
        c = new Array(length).fill(0),
        i = 1, k, p;

    yield permutation.slice();
    while (i < length) {
      if (c[i] < i) {
        k = i % 2 && c[i];
        p = permutation[i];
        permutation[i] = permutation[k];
        permutation[k] = p;
        ++c[i];
        i = 1;
        yield permutation.slice();
      } else {
        c[i] = 0;
        ++i;
      }
    }
  };

  const computeSubsets = function* (s) {
    if (s.length === 0) {
      yield [];
    } else {
      const r = s.slice(1);
      for (const m of computeSubsets(r)) {
        yield m;
        yield [s[0], ...m];
      }
    }
  };

  const computeCommonPrefix = function (a, b) {
    let k = 0;
    while (k < Math.min(a.length, b.length)) {
      if (a[k] !== b[k]) {
        break;
      }
      k += 1;
    }
    return a.slice(0, k);
  };

  const suite = {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);

      const c = db._create(collection, {numberOfShards: 3});
      c.ensureIndex({type: 'persistent', fields: coveredFields});
      const query = fields.map(f => `FOR ${f} IN 1..${numberOfSteps}`).join(" ") + " INSERT {" + fields.join(",") + `} INTO ${collection}`;
      db._query(query);
    },
    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testFoo: function () {
    },
  };

  const compareDocuments = function (sortFields, docA, docB) {
    // returns true if docA is smaller or equal to docB
    for (let f of sortFields) {
      if (docA[f] < docB[f]) {
        return true;
      }
      if (docA[f] > docB[f]) {
        return false;
      }
    }
    return true;
  };

  const testFunction = function (sortFields) {
    return function () {
      const query = "FOR doc IN " + collection + " SORT " + sortFields.map(f => `doc.${f}`).join(",") + " RETURN doc";
      const result = db._query(query).toArray();

      // first check that the result is correct
      for (let k = 1; k < result.length; k++) {
        assertTrue(compareDocuments(sortFields, result[k - 1], result[k]),
            `sort keys = ${sortFields}, docA = ${JSON.stringify(result[k - 1])}, docB = ${JSON.stringify(result[k])}`);
      }

      // check if we use the correct group sort
      const plan = db._createStatement({query}).explain().plan;
      const sortNodes = plan.nodes.filter(x => x.type === "SortNode");

      const commonPrefix = computeCommonPrefix(sortFields, coveredFields);
      if (commonPrefix.length === sortFields.length) {
        // Index fully covers the sort.
        assertEqual(sortNodes.length, 0);
      } else {
        assertEqual(sortNodes.length, 1);
        const [sortNode] = sortNodes;

        assertEqual(sortNode.numberOfTopGroupedElements, commonPrefix.length);
      }
    };
  };

  for (let subset of computeSubsets(fields)) {
    if (subset.length === 0) {
      continue;
    }
    for (let p of permute(subset)) {
      suite["testMultiGroupSort_" + p.join("")] = testFunction(p);
    }
  }

  return suite;
}

jsunity.run(optimizerIndexesGroupSortTestSuite);
jsunity.run(optimizerIndexesGroupSortMultiTestSuite);

return jsunity.done();
