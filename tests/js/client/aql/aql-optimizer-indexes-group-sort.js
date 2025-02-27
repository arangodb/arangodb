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
const { randomNumberGeneratorInt } = require("@arangodb/testutils/seededRandom");

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
    return ["use-index-for-sort"].every((rule) => plan.rules.indexOf(rule) >= 0);
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
    return db._query(query, { "@collection": collection_name }).toArray();
  };
  const expected_results = function (query, collection_name) {
    const c_expected = copy_collection(collection_name);
    return db._query(query, { "@collection": c_expected.name() }).toArray();
  };

  const seed = 18430991235;

  return {

    tearDown: function () {
      db._drop("UnitTestsCollection");
      db._drop("UnitTestsExpectedCollection");
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    test_indexed_group_sort_gives_same_results_as_unindexed_sort: function () {
      let randomNumber = randomNumberGeneratorInt(seed);
      for (let row_count of [1, 2, 10, 100, 800, 1000, 1001, 2501, 10000]) {
        const collection = create_collection();
        collection.insert(Array.from({ length: row_count }, (_, index) => index).map(i => {
          return {
            a: i % 9,
            x: row_count - i - 1,
            b: randomNumber()
          };
        }));
        collection.ensureIndex({ type: "persistent", fields: ["a", "b"] });
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
      let randomNumber = randomNumberGeneratorInt(seed);
      const collection = create_collection();
      collection.insert(Array.from({ length: 10 }, (_, index) => index).map(i => {
        return {
          a: i % 9,
          x: 100 - i - 1,
          b: randomNumber(),
          c: i
        };
      }));
      collection.ensureIndex({ type: "persistent", fields: ["a", "b", "c"] });
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
      let randomNumber = randomNumberGeneratorInt(seed);
      const collection = create_collection();
      collection.insert(Array.from({ length: 100 }, (_, index) => index).map(i => {
        return {
          a: i % 9,
          x: 100 - i - 1,
          b: Math.floor(randomNumber() * 100)
        };
      }));
      collection.ensureIndex({ type: "persistent", fields: ["a", "b"] });
      waitForEstimatorSync();

      let query = "FOR doc IN @@collection SORT doc.b RETURN [doc.b]";
      let plan = query_plan(query, collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);

      query = "FOR doc IN @@collection SORT doc.b, doc.a RETURN [doc.b, doc.a]";
      plan = query_plan(query, collection.name());
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    test_index_is_used_when_sort_directions_of_all_used_index_fields_is_the_same: function () {
      let randomNumber = randomNumberGeneratorInt(seed);
      const collection = create_collection();
      collection.insert(Array.from({ length: 100 }, (_, index) => index).map(i => {
        return {
          a: i % 9,
          x: 100 - i - 1,
          b: Math.floor(randomNumber() * 100),
          c: i
        };
      }));
      collection.ensureIndex({ type: "persistent", fields: ["a", "b", "c"] });
      waitForEstimatorSync();

      var queries = [
        [1, "FOR doc IN @@collection SORT doc.a DESC, doc.x DESC RETURN doc.b"],
        [1, "FOR doc IN @@collection SORT doc.a ASC, doc.x ASC RETURN doc.b"],
        [2, "FOR doc IN @@collection SORT doc.a DESC, doc.b DESC, doc.x DESC RETURN doc.b"],
        [2, "FOR doc IN @@collection SORT doc.a ASC, doc.b ASC, doc.x ASC RETURN doc.b"],

        [1, "FOR doc IN @@collection SORT doc.a DESC, doc.x ASC RETURN doc.b"],
        [1, "FOR doc IN @@collection SORT doc.a ASC, doc.x DESC RETURN doc.b"],
        [1, "FOR doc IN @@collection SORT doc.a ASC, doc.b DESC, doc.x DESC RETURN doc.b"],
        [1, "FOR doc IN @@collection SORT doc.a ASC, doc.b DESC, doc.x ASC RETURN doc.b"],
        [1, "FOR doc IN @@collection SORT doc.a DESC, doc.b ASC, doc.x DESC RETURN doc.b"],
        [1, "FOR doc IN @@collection SORT doc.a DESC, doc.b ASC, doc.x ASC RETURN doc.b"],
        [2, "FOR doc IN @@collection SORT doc.a DESC, doc.b DESC, doc.c ASC, doc.x ASC RETURN doc.b"],
        [2, "FOR doc IN @@collection SORT doc.a DESC, doc.b DESC, doc.c ASC, doc.x DESC RETURN doc.b"],
        [2, "FOR doc IN @@collection SORT doc.a ASC, doc.b ASC, doc.c DESC, doc.x ASC RETURN doc.b"],
        [2, "FOR doc IN @@collection SORT doc.a ASC, doc.b ASC, doc.c DESC, doc.x DESC RETURN doc.b"],
      ];
      for (let [numberOfGroupedElements, query] of queries) {
        let plan = query_plan(query, collection.name());
        assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
        assertTrue(sort_node_does_a_group_sort(plan, numberOfGroupedElements), plan.nodes);
        assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);
      }

      queries = [
        "FOR doc IN @@collection SORT doc.a ASC, doc.b ASC, doc.c ASC, doc.x ASC RETURN doc.b",
        "FOR doc IN @@collection SORT doc.a ASC, doc.b ASC, doc.c ASC, doc.x DESC RETURN doc.b",
        "FOR doc IN @@collection SORT doc.a DESC, doc.b DESC, doc.c DESC, doc.x DESC RETURN doc.b",
        "FOR doc IN @@collection SORT doc.a DESC, doc.b DESC, doc.c DESC, doc.x ASC RETURN doc.b",
      ];
      for (let query of queries) {
        let plan = query_plan(query, collection.name());
        assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
        // uses full index, therefore full sort node is exchanged by index node
        assertFalse(sort_node_does_a_group_sort(plan), plan.nodes);
        assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);
      }
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    test_uses_sparse_index_when_sort_registers_start_with_index_fields_and_are_not_null: function () {
      let randomNumber = randomNumberGeneratorInt(seed);
      const collection = create_collection();
      collection.insert(Array.from({ length: 100 }, (_, index) => index).map(i => {
        return {
          a: i % 9 === 0 ? null : i % 9,
          x: 100 - i - 1,
          b: Math.floor(randomNumber() * 100),
          c: i % 2 === 0 ? null : i
        };
      }));
      collection.ensureIndex({ type: "persistent", fields: ["a"], sparse: true });
      waitForEstimatorSync();

      let query = "FOR doc IN @@collection SORT doc.a, doc.x RETURN doc.b";
      let plan = query_plan(query, collection.name());
      // cannot use index because a document with a==null is not included in the sparse index
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);

      var queries = [
        "FOR doc IN @@collection FILTER doc.a > null SORT doc.a, doc.x RETURN doc.b",
        "FOR doc IN @@collection FILTER doc.a != null SORT doc.a, doc.x RETURN doc.b",
        "FOR doc IN @@collection FILTER doc.a > 0 SORT doc.a, doc.x RETURN doc.b",
        "FOR doc IN @@collection FILTER doc.a >= 'some_string' SORT doc.a, doc.x RETURN doc.b",
      ];
      for (let query of queries) {
        let plan = query_plan(query, collection.name());
        assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
        assertTrue(sort_node_does_a_group_sort(plan), plan.nodes);
        assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);
      }

      collection.ensureIndex({ type: "persistent", fields: ["c", "d"], sparse: true });
      waitForEstimatorSync();

      query = "FOR doc IN @@collection FILTER doc.c > null AND doc.d > null SORT doc.c, doc.d, doc.x RETURN doc.b";
      plan = query_plan(query, collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      // uses full index, therefore no group sort necessary
      assertFalse(sort_node_does_a_group_sort(plan), plan.nodes);
      assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);

      query = "FOR doc IN @@collection FILTER doc.c > null AND doc.d > null SORT doc.c, doc.x RETURN doc.b";
      plan = query_plan(query, collection.name());
      assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertTrue(sort_node_does_a_group_sort(plan), plan.nodes);
      assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);

      query = "FOR doc IN @@collection FILTER doc.c > null SORT doc.c, doc.x RETURN doc.b";
      plan = query_plan(query, collection.name());
      // does not assure that doc.d != null, therefore cannot use index
      assertFalse(query_plan_uses_index_for_sorting(plan), plan.rules);
      assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);
    },

    ///////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    test_sparse_index_works_with_different_sort_directions: function () {
      let randomNumber = randomNumberGeneratorInt(seed);
      const collection = create_collection();
      collection.insert(Array.from({ length: 100 }, (_, index) => index).map(i => {
        return {
          a: i % 9 === 0 ? null : i % 9,
          x: 100 - i - 1,
          b: Math.floor(randomNumber() * 100),
          c: i % 2 === 0 ? null : i
        };
      }));
      collection.ensureIndex({ type: "persistent", fields: ["a", "b", "c"], sparse: true });
      waitForEstimatorSync();

      var queries = [
        [1, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a DESC, doc.x DESC RETURN doc.b"],
        [1, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a ASC, doc.x ASC RETURN doc.b"],
        [2, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a DESC, doc.b DESC, doc.x DESC RETURN doc.b"],
        [2, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a ASC, doc.b ASC, doc.x ASC RETURN doc.b"],

        [1, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a DESC, doc.x ASC RETURN doc.b"],
        [1, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a ASC, doc.x DESC RETURN doc.b"],
        [1, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a ASC, doc.b DESC, doc.x DESC RETURN doc.b"],
        [1, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a ASC, doc.b DESC, doc.x ASC RETURN doc.b"],
        [1, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a DESC, doc.b ASC, doc.x DESC RETURN doc.b"],
        [1, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a DESC, doc.b ASC, doc.x ASC RETURN doc.b"],
        [2, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a DESC, doc.b DESC, doc.c ASC, doc.x ASC RETURN doc.b"],
        [2, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a DESC, doc.b DESC, doc.c ASC, doc.x DESC RETURN doc.b"],
        [2, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a ASC, doc.b ASC, doc.c DESC, doc.x ASC RETURN doc.b"],
        [2, "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a ASC, doc.b ASC, doc.c DESC, doc.x DESC RETURN doc.b"],
      ];
      for (let [numberOfGroupedElements, query] of queries) {
        let plan = query_plan(query, collection.name());
        assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
        assertTrue(sort_node_does_a_group_sort(plan, numberOfGroupedElements), plan.nodes);
        assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);
      }

      queries = [
        "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a ASC, doc.b ASC, doc.c ASC, doc.x ASC RETURN doc.b",
        "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a ASC, doc.b ASC, doc.c ASC, doc.x DESC RETURN doc.b",
        "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a DESC, doc.b DESC, doc.c DESC, doc.x DESC RETURN doc.b",
        "FOR doc IN @@collection FILTER doc.a > null AND doc.b > null AND doc.c > null SORT doc.a DESC, doc.b DESC, doc.c DESC, doc.x ASC RETURN doc.b",
      ];
      for (let query of queries) {
        let plan = query_plan(query, collection.name());
        assertTrue(query_plan_uses_index_for_sorting(plan), plan.rules);
        // uses full index, therefore full sort node is exchanged by index node
        assertFalse(sort_node_does_a_group_sort(plan), plan.nodes);
        assertEqual(expected_results(query, collection.name()), execute(query, collection.name()), query);
      }
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

      const c = db._create(collection, { numberOfShards: 3 });
      c.ensureIndex({ type: 'persistent', fields: coveredFields });
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
      const plan = db._createStatement({ query }).explain().plan;
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
