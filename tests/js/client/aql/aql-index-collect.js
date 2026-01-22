/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */
"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const { db } = require("@arangodb");
const { waitForEstimatorSync } = require('@arangodb/test-helper');

const database = "IndexCollectDatabase";
const collection = "c";
const indexCollectOptimizerRule = "use-index-for-collect";

function OptimizerTestSuite() {
  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);

      const c = db._create(collection, { numberOfShards: 3 });
      const docs = [];
      for (let k = 0; k < 10000; k++) {
        docs.push({ k, a: k % 10, b: k % 100, x: k % 10 });
      }
      c.save(docs);
    },
    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },
    tearDown: function () {
      db[collection].indexes().filter(x => x.type !== "primary").map(x => db[collection].dropIndex(x));
    },

    testDistinctOptimizerRule: function () {
      db[collection].ensureIndex({ type: "persistent", fields: ["a", "b", "d"] });
      db[collection].ensureIndex({ type: "persistent", fields: ["x"], sparse: true });

      const queries = [
        [`FOR doc IN ${collection} COLLECT a = doc.a RETURN a`, true],
        [`FOR doc IN ${collection} COLLECT b = doc.b RETURN b`, false], // index cannot be used
        [`FOR doc IN ${collection} COLLECT a = doc.a.c RETURN a`, false],
        [`FOR doc IN ${collection} COLLECT x = doc.x RETURN x`, false], // does not work on sparse indexes
        [`FOR doc IN ${collection} SORT doc.a DESC COLLECT a = doc.a RETURN [a]`, false], // desc not possible
        [`FOR doc IN ${collection} SORT doc.a ASC COLLECT a = doc.a RETURN [a]`, true],
        [`FOR doc IN ${collection} COLLECT a = doc.a, b = doc.b RETURN [a, b]`, true],
        [`FOR doc IN ${collection} COLLECT a = doc.a, b = doc.b, d = doc.d RETURN [a, b, d]`, true],
        [`FOR doc IN ${collection} COLLECT a = doc.a, d = doc.d RETURN [a, d]`, false],
        [`FOR doc IN ${collection} COLLECT a = doc.a, c = doc.c RETURN [a, c]`, false],
        [`FOR doc IN ${collection} COLLECT b = doc.b, a = doc.a RETURN [a, b]`, true],
        [`FOR doc IN ${collection} COLLECT c = doc.c, a = doc.a RETURN [a, c]`, false],
        [`FOR doc IN ${collection} COLLECT b = doc.b, a = doc.a OPTIONS {disableIndex: true} RETURN [a, b]`, false],
        [`FOR doc IN ${collection} COLLECT a = doc.a INTO d RETURN [a, d]`, false],
        [`FOR doc IN ${collection} COLLECT a = doc.a INTO d = doc.d RETURN [a, d]`, false],
        [`FOR doc IN ${collection} COLLECT a = doc.a WITH COUNT INTO c RETURN [a, c]`, false],
      ];

      for (const [query, optimized] of queries) {
        const explain = db._createStatement(query).explain();
        assertEqual(explain.plan.rules.indexOf(indexCollectOptimizerRule) !== -1, optimized, query);
        if (optimized) {
          const nodes = explain.plan.nodes.filter(x => x.type === "IndexCollectNode");
          assertEqual(nodes.length, 1, query);
          const indexCollect = nodes[0];
          assertEqual(indexCollect.aggregations.length, 0);
        }
      }
    },

    testAggregateOptimizerRule: function () {
      db[collection].ensureIndex({ type: "persistent", fields: ["a", "b", "d"], storedValues: ["e", "f"] });
      db[collection].ensureIndex({ type: "persistent", fields: ["k"] });

      const queries = [
        [`FOR doc IN ${collection} COLLECT a = doc.a AGGREGATE b = MAX(doc.b) RETURN [a, b]`, true], // no selectivity requiement
        [`FOR doc IN ${collection} COLLECT a = doc.a AGGREGATE b = MAX(doc.b), c = SUM(doc.b) RETURN [a, b, c]`, true],
        [`FOR doc IN ${collection} COLLECT a = doc.a AGGREGATE b = MAX(doc.b + doc.c) RETURN [a, b]`, false],
        [`FOR doc IN ${collection} LET x = doc.a COLLECT a = doc.a AGGREGATE b = MAX(x + 1) RETURN [a, b]`, false], // does currently not support aggregation expressions with variables different than the document variable
        [`FOR doc IN ${collection} COLLECT a = doc.a AGGREGATE c = COUNT() RETURN [a, c]`, false], // does currently not support aggregation expressions with no in-variable
        [`FOR doc IN ${collection} COLLECT a = doc.a AGGREGATE b = MAX(doc.b + doc.e) RETURN [a, b]`, true],
        [`FOR doc IN ${collection} COLLECT a = doc.a AGGREGATE b = MAX(doc.b + doc.d) RETURN [a, b]`, true],
        [`FOR doc IN ${collection} COLLECT a = doc.a AGGREGATE b = MAX(doc.b + doc.d), c = PUSH(doc.f) RETURN [a, b, c]`, true],
      ];

      for (const [query, optimized] of queries) {
        const explain = db._createStatement(query).explain();
        assertEqual(explain.plan.rules.indexOf(indexCollectOptimizerRule) !== -1, optimized, query);
        if (optimized) {
          const nodes = explain.plan.nodes.filter(x => x.type === "IndexCollectNode");
          assertEqual(nodes.length, 1, query);
          const indexCollect = nodes[0];
          assertTrue(indexCollect.aggregations.length > 0);
        }
      }
    },

    testCollectOptions: function () {
      const index = db[collection].ensureIndex({ type: "persistent", fields: ["a", "b", "c.d"], name: "foobar" });
      const queries = [
        [`FOR doc IN ${collection} COLLECT a = doc.a RETURN a`, "sorted", [0], [["a"]]],
        [`FOR doc IN ${collection} COLLECT a = doc.a SORT null RETURN a`, "hash", [0], [["a"]]],
        [`FOR doc IN ${collection} COLLECT a = doc.a, b = doc.b RETURN [a, b]`, "sorted", [0, 1], [["a"], ["b"]]],
        [`FOR doc IN ${collection} COLLECT b = doc.b, a = doc.a RETURN [a, b]`, "hash", [1, 0], [["b"], ["a"]]],
        [`FOR doc IN ${collection} COLLECT a = doc.a, b = doc.b, c = doc.c.d RETURN [a, b]`, "sorted", [0, 1, 2], [["a"], ["b"], ["c", "d"]]],
      ];

      for (const [query, method, expectedFields, expectedAttributes] of queries) {
        const explain = db._createStatement(query).explain();
        assertTrue(explain.plan.rules.indexOf(indexCollectOptimizerRule) !== -1, query);
        const nodes = explain.plan.nodes.filter(x => x.type === "IndexCollectNode");
        assertEqual(nodes.length, 1, query);
        const icn = nodes[0];

        assertEqual(icn.collection, collection);
        assertEqual(icn.collectOptions.method, method);
        assertEqual(icn.index.name, index.name);
        assertEqual(icn.oldIndexVariable.name, "doc");

        const indexFields = icn.groups.map(x => x.indexField);
        assertEqual(indexFields, expectedFields);

        const attributes = icn.groups.map(x => x.attribute);
        assertEqual(attributes, expectedAttributes);
      }
    },

    // the following tests apply only to queries that include a COLLECT without an additional AGGREGATE

    testOptimizerRuleAppliesWhenSelectivityIsLowEnough: function () {
      const c_new = db._create(collection + "1", { numberOfShards: 3 });

      let docs = [];
      // number of documents for single server n := 6000
      // number of documents per shard for cluster n := 6000 / 3 = 2000
      for (let l = 0; l < 6000; l++) {
        docs.push({ a: l % 100 }); // number of distinct values k = 100
      }
      c_new.save(docs);
      c_new.ensureIndex({ type: "persistent", fields: ["a"] });
      waitForEstimatorSync();

      // for optimizer rule to be applied k < n / log n
      // single server k < 263
      // cluster k < 689

      // k := 100 distinct values, optimizer rule applies
      let explain = db._createStatement(`FOR doc IN ${c_new.name()} COLLECT a = doc.a RETURN a`).explain();
      assertTrue(explain.plan.rules.indexOf(indexCollectOptimizerRule) !== -1);
    },

    testOptimerRuleDoesNotApplyWhenSelectivityIsTooHigh: function () {
      const c_new = db._create(collection + "2", { numberOfShards: 2 });

      let docs = [];
      // number of documents for single server n := 6000
      // number of documents per shard for cluster n := 6000 / 3 = 2000
      for (let l = 0; l < 6000; l++) {
        docs.push({ a: l % 1000 }); // number of different values k = 1000
      }
      c_new.save(docs);
      c_new.ensureIndex({ type: "persistent", fields: ["a"] });
      waitForEstimatorSync();

      // for optimizer rule to be applied k < n / log n
      // single server k < 263
      // cluster k < 689

      // k := 1000 distinct values, optimizer rule does not apply
      let explain = db._createStatement(`FOR doc IN ${c_new.name()} COLLECT a = doc.a RETURN a`).explain();
      assertFalse(explain.plan.rules.indexOf(indexCollectOptimizerRule) !== -1);
    },

    testDistinctIndexCollectOptimizerRule: function () {
      db[collection].ensureIndex({ type: "persistent", fields: ["k"] });
      db[collection].ensureIndex({ type: "persistent", fields: ["a"] });

      const queries = [
        [`FOR doc IN ${collection} COLLECT k = doc.k RETURN k`, false], // because of bad selectivity
        [`FOR doc IN ${collection} COLLECT a = doc.a RETURN a`, true], // has a good selectivity
      ];

      for (const [query, optimized] of queries) {
        const explain = db._createStatement(query).explain();
        assertEqual(explain.plan.rules.indexOf(indexCollectOptimizerRule) !== -1, optimized, query);
        if (optimized) {
          const nodes = explain.plan.nodes.filter(x => x.type === "IndexCollectNode");
          assertEqual(nodes.length, 1, query);
          const indexCollect = nodes[0];
          assertEqual(indexCollect.aggregations.length, 0);
        }
      }
    }

  };
}

function ExecutionTestSuite() {
  const numDocuments = 10000;

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
    },
    tearDown: function () {
      db[collection].drop();
    },
    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testIndexCollectExecution: function () {
      const c = db._create(collection, { numberOfShards: 3 });
      const docs = [];
      for (let k = 0; k < numDocuments; k++) {
        docs.push({ k, a: k % 2, b: k % 4, c: k % 7, m: k % 4 });
      }
      c.save(docs);
      c.ensureIndex({ type: "persistent", fields: ["k"] });
      c.ensureIndex({ type: "persistent", fields: ["a", "b"], storedValues: ["c"] });
      c.ensureIndex({ type: "persistent", fields: ["b"] });
      c.ensureIndex({ type: "persistent", fields: ["c"] });
      c.ensureIndex({ type: "persistent", fields: ["m", "a"] });

      const queries = [
        [`FOR doc IN ${collection} COLLECT a = doc.a RETURN [a]`],
        [`FOR doc IN ${collection} COLLECT a = doc.a, b = doc.b RETURN [a, b]`],
        [`FOR doc IN ${collection} COLLECT m = doc.m, a = doc.a RETURN [m, a]`],
        [`FOR doc IN ${collection} COLLECT m = doc.m RETURN [m]`],

        [`FOR doc IN ${collection} COLLECT b = doc.b, a = doc.a RETURN [a, b]`],
        [`FOR doc IN ${collection} COLLECT a = doc.a, m = doc.m RETURN [a, m]`],
        [`LET as = (FOR doc IN ${collection} COLLECT a = doc.a RETURN a) LET bs = (FOR doc IN ${collection}
            COLLECT b = doc.b RETURN b) RETURN [as, bs]`],

        [`FOR doc IN ${collection} COLLECT m = doc.m AGGREGATE a = SUM(doc.a) RETURN [m, a]`],
        [`FOR doc IN ${collection} COLLECT m = doc.m AGGREGATE a = SUM(doc.a + 1) RETURN [m, a]`],
        [`FOR doc IN ${collection} COLLECT m = doc.m AGGREGATE a = SUM(doc.a), b = SUM(doc.a) RETURN [m, a, b]`],
        [`FOR doc IN ${collection} COLLECT a = doc.a AGGREGATE b = SUM(doc.b) RETURN [a, b]`],
        [`FOR doc IN ${collection} COLLECT a = doc.a, b = doc.b AGGREGATE c = SUM(doc.c), d = MAX(doc.c + doc.a)
            RETURN [a, b, c, d]`],
        [`FOR doc IN ${collection} COLLECT a = doc.a, b = doc.b
            AGGREGATE c = BIT_XOR(doc.c), d = BIT_OR(doc.c), e = BIT_AND(doc.c) RETURN [a, b, c, d, e]`],
      ];

      for (const [query] of queries) {
        const explain = db._createStatement(query).explain();
        assertTrue(explain.plan.rules.indexOf(indexCollectOptimizerRule) !== -1, query);

        const expectedResult = db._query(query, {}, { optimizer: { rules: [`-${indexCollectOptimizerRule}`] } }).toArray();
        const actualResult = db._query(query).toArray();
        assertEqual(expectedResult, actualResult);
      }
    },

    testQueriesOnEmptyCollection: function () {
      const c = db._create(collection, { numberOfShards: 3 });
      c.ensureIndex({ type: "persistent", fields: ["a"] });
      const queries = [
        `FOR doc IN ${collection} COLLECT a = doc.a RETURN a`,
        `FOR doc IN ${collection} COLLECT AGGREGATE max = MAX(doc.a) RETURN max`
      ];
      for (const query of queries) {
        const actualResult = db._query(query).toArray().filter(x => x != null);
        assertEqual(actualResult, []);
      }
    }
  };
}

jsunity.run(OptimizerTestSuite);
jsunity.run(ExecutionTestSuite);
return jsunity.done();
