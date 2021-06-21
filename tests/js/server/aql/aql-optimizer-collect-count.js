/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, assertNull, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for COLLECT w/ COUNT
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");
var assertQueryError = helper.assertQueryError;
const isCluster = require("@arangodb/cluster").isCluster();

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerCountTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 4 });

      for (var i = 0; i < 1000; ++i) {
        c.save({ group: "test" + (i % 10), value: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test no into, no count
////////////////////////////////////////////////////////////////////////////////

    testInvalidSyntax : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT WITH COUNT i RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT WITH COUNT INTO RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT WITH COUNT g RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT COUNT INTO g RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT g WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT g WITH COUNT INTO RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group WITH COUNT i RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group WITH COUNT i INTO group RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group WITH COUNT COUNT INTO group RETURN 1");

      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE doc = MIN(i.group) WITH COUNT INTO group RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group AGGREGATE doc = MIN(i.group) WITH COUNT INTO group RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountIsTransformedToAggregate : function () {
      var query = "FOR i IN " + c.name() + " COLLECT WITH COUNT INTO cnt RETURN cnt";

      let output = require("@arangodb/aql/explainer").explain(query, {colors: false}, false);
      var plan = AQL_EXPLAIN(query).plan;
      var collectNode = plan.nodes[2];
      assertEqual("CollectNode", collectNode.type);
      if (isCluster) {
        assertEqual("count", collectNode.collectOptions.method);
        assertEqual(1, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        let clusterCollectNode = plan.nodes[5];
        assertEqual("CollectNode", clusterCollectNode.type);
        assertEqual("sorted", clusterCollectNode.collectOptions.method);
        assertEqual(1, clusterCollectNode.aggregates.length);
        assertEqual("SUM", clusterCollectNode.aggregates[0].type);
        assertEqual("cnt", clusterCollectNode.aggregates[0].outVariable.name);
        assertEqual(clusterCollectNode.aggregates[0].inVariable.name, collectNode.aggregates[0].outVariable.name);
        assertTrue(/COLLECT AGGREGATE #[0-9] = LENGTH\(\)/.test(output));
        assertTrue(/COLLECT AGGREGATE cnt = SUM\(#[0-9]\)/.test(output));
      } else {
        assertEqual("count", collectNode.collectOptions.method);
        assertEqual(1, collectNode.aggregates.length);
        assertEqual("cnt", collectNode.aggregates[0].outVariable.name);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        assertTrue(/COLLECT AGGREGATE cnt = LENGTH\(\)/.test(output));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalSimple : function () {
      var query = "FOR i IN " + c.name() + " COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalLimit : function () {
      var query = "FOR i IN " + c.name() + " LIMIT 25, 100 COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(100, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalFiltered : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group == 'test4' COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(100, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalFilteredIndexed : function () {
      c.ensureIndex({ type: "persistent", fields: ["group"] });
      var query = "FOR i IN " + c.name() + " FILTER i.group == 'test5' COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(100, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

    testCountTotalFilteredSkippedIndexed : function () {
      c.ensureIndex({ type: "persistent", fields: ["group"] });
      var query = "FOR i IN " + c.name() + " FILTER i.group == 'test5' LIMIT 25, 100 COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(75, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

    testCountTotalFilteredPostFilteredIndexed : function () {
      c.ensureIndex({ type: "persistent", fields: ["group"] });
      var query = "FOR i IN " + c.name() + " FILTER CHAR_LENGTH(i.group) == 5 COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

    testCountTotalFilteredPostFilteredSkippedIndexed : function () {
      c.ensureIndex({ type: "persistent", fields: ["group"] });
      var query = "FOR i IN " + c.name() + " FILTER CHAR_LENGTH(i.group) == 5 LIMIT 25, 100 COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(100, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalFilteredMulti : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group >= 'test2' && i.group <= 'test4' COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(300, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalFilteredEmpty : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group >= 'test99' COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(0, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalFilteredBig : function () {
      var i;
      for (i = 0; i < 10000; ++i) {
        c.save({ age: 10 + (i % 80), type: 1 });
      }
      for (i = 0; i < 10000; ++i) {
        c.save({ age: 10 + (i % 80), type: 2 });
      }

      var query = "FOR i IN " + c.name() + " FILTER i.age >= 20 && i.age < 50 && i.type == 1 COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(125 * 30, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotal : function () {
      var query = "FOR j IN " + c.name() + " COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalNested : function () {
      var query = "FOR i IN 1..2 FOR j IN " + c.name() + " COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query, null, { optimizer: { rules: ["-interchange-adjacent-enumerations"] } });
      assertEqual(1, results.json.length);
      assertEqual(2000, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalNested2 : function () {
      var query = "FOR j IN " + c.name() + " FOR i IN 1..2 COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query, null, { optimizer: { rules: ["-interchange-adjacent-enumerations"] } });
      assertEqual(1, results.json.length);
      assertEqual(2000, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountSimple : function () {
      var query = "FOR i IN " + c.name() + " COLLECT class = i.group WITH COUNT INTO count RETURN [ class, count ]";

      var results = AQL_EXECUTE(query);
      assertEqual(10, results.json.length);
      for (var i = 0; i < results.json.length; ++i) {
        var group = results.json[i];
        assertTrue(Array.isArray(group));
        assertEqual("test" + i, group[0]);
        assertEqual(100, group[1]);
      }

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountFiltered : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group >= 'test1' && i.group <= 'test4' COLLECT class = i.group WITH COUNT INTO count RETURN [ class, count ]";

      var results = AQL_EXECUTE(query);
      assertEqual(4, results.json.length);
      for (var i = 0; i < results.json.length; ++i) {
        var group = results.json[i];
        assertTrue(Array.isArray(group));
        assertEqual("test" + (i + 1), group[0]);
        assertEqual(100, group[1]);
      }

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountFilteredEmpty : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group >= 'test99' COLLECT class = i.group WITH COUNT INTO count RETURN [ class, count ]";

      var results = AQL_EXECUTE(query);
      assertEqual(0, results.json.length);

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountFilteredBig : function () {
      var i;
      for (i = 0; i < 10000; ++i) {
        c.save({ age: 10 + (i % 80), type: 1 });
      }
      for (i = 0; i < 10000; ++i) {
        c.save({ age: 10 + (i % 80), type: 2 });
      }

      var query = "FOR i IN " + c.name() + " FILTER i.age >= 20 && i.age < 50 && i.type == 1 COLLECT age = i.age WITH COUNT INTO count RETURN [ age, count ]";

      var results = AQL_EXECUTE(query);
      assertEqual(30, results.json.length);
      for (i = 0; i < results.json.length; ++i) {
        var group = results.json[i];
        assertTrue(Array.isArray(group));
        assertEqual(20 + i, group[0]);
        assertEqual(125, group[1]);
      }

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountNested : function () {
      var query = "FOR i IN 1..2 FOR j IN " + c.name() + " COLLECT class1 = i, class2 = j.group WITH COUNT INTO count RETURN [ class1, class2, count ]";

      var results = AQL_EXECUTE(query), x = 0;
      assertEqual(20, results.json.length);
      for (var i = 1; i <= 2; ++i) {
        for (var j = 0; j < 10; ++j) {
          var group = results.json[x++];
          assertTrue(Array.isArray(group));
          assertEqual(i, group[0]);
          assertEqual("test" + j, group[1]);
          assertEqual(100, group[2]);
        }
      }

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count shaped
////////////////////////////////////////////////////////////////////////////////

    testCountShaped : function () {
      var query = "FOR j IN " + c.name() + " COLLECT doc = j WITH COUNT INTO count RETURN doc";

      var results = AQL_EXECUTE(query);
      // expectation is that we get 1000 different docs and do not crash (issue #1265)
      assertEqual(1000, results.json.length);
      if (isCluster) {
        var plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

    // This test is not necessary for hashed collect, as hashed collect will not
    // be used without group variables.
    testCollectSortedUndefined: function () {
      const randomDocumentID = db["UnitTestsCollection"].any()._id;
      const query = 'LET start = DOCUMENT("' + randomDocumentID + '")._key FOR i IN [] COLLECT AGGREGATE sum = SUM(i) RETURN {sum, start}';
      const bindParams = {};
      const options = {optimizer: {rules: ['-remove-unnecessary-calculations','-remove-unnecessary-calculations-2']}};

      const planNodes = AQL_EXPLAIN(query, {}, options).plan.nodes;
      assertEqual(
        [ "SingletonNode", "CalculationNode", "CalculationNode",
          "EnumerateListNode", "CollectNode", "CalculationNode", "ReturnNode" ],
        planNodes.map(n => n.type));
      assertEqual("sorted", planNodes[4].collectOptions.method);

      const results = db._query(query, bindParams, options).toArray();
      // expectation is that we exactly get one result
      // count will be 0, start will be undefined
      assertEqual(1, results.length);
      assertNull(results[0].sum);
      assertEqual(undefined, results[0].start);
    },

    testCollectCountUndefined: function () {
      const randomDocumentID = db["UnitTestsCollection"].any()._id;
      const query = 'LET start = DOCUMENT("' + randomDocumentID + '")._key FOR i IN [] COLLECT AGGREGATE count = count(i) RETURN {count, start}';
      const bindParams = {};
      const options = {optimizer: {rules: ['-remove-unnecessary-calculations','-remove-unnecessary-calculations-2']}};

      const planNodes = AQL_EXPLAIN(query, {}, options).plan.nodes;

      assertEqual(
        [ "SingletonNode", "CalculationNode", "CalculationNode",
          "EnumerateListNode", "CollectNode", "CalculationNode", "ReturnNode" ],
        planNodes.map(n => n.type));
      assertEqual("count", planNodes[4].collectOptions.method);

      const results = db._query(query, bindParams, options).toArray();
      // expectation is that we exactly get one result
      // count will be 0, start will be undefined
      assertEqual(1, results.length);
      assertEqual(0, results[0].count);
      assertEqual(undefined, results[0].start);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerCountTestSuite);

return jsunity.done();
