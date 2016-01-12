/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerAggregateTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 2000; ++i) {
        c.save({ group: "test" + (i % 10), value1: i, value2: i % 13 });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid queries
////////////////////////////////////////////////////////////////////////////////

    testInvalidSyntax : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " AGGREGATE RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " AGGREGATE length = LENGTH(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT COUNT AGGREGATE length = LENGTH(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE length = LENGTH(i) WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE length = LENGTH(i) WITH COUNT INTO x RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT WITH COUNT g AGGREGATE length = LENGTH(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group AGGREGATE WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group AGGREGATE length = LENGTH(i) WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group AGGREGATE length = LENGTH(i) INTO group RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid queries
////////////////////////////////////////////////////////////////////////////////

    testInvalidAggregateFunctions : function () {
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = 1 RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = i.test RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = i.test + 1 RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = LENGTH(i) + 1 RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = 1 + LENGTH(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = IS_NUMBER(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = IS_STRING(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = IS_ARRAY(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = IS_OBJECT(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE length = LENGTH(i), c = IS_OBJECT(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT group = i.group AGGREGATE c = IS_OBJECT(i) RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateAll : function () {
      var query = "FOR i IN " + c.name() + " COLLECT group = i.group AGGREGATE length = LENGTH(i.value1), min = MIN(i.value1), max = MAX(i.value1), sum = SUM(i.value1), avg = AVERAGE(i.value1) RETURN { group, length, min, max, sum, avg }";

      var results = AQL_EXECUTE(query);
      assertEqual(10, results.json.length);
      for (var i = 0; i < 10; ++i) {
        assertEqual("test" + i, results.json[i].group);
        assertEqual(200, results.json[i].length);
        assertEqual(i, results.json[i].min);
        assertEqual(1990 + i, results.json[i].max);
        assertEqual(199000 + i * 200, results.json[i].sum);
        assertEqual(995 + i, results.json[i].avg);
      }

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateExpression : function () {
      var query = "FOR i IN " + c.name() + " COLLECT group = i.group AGGREGATE length = LENGTH(1), min = MIN(i.value1 + 1), max = MAX(i.value1 * 2) RETURN { group, length, min, max }";

      var results = AQL_EXECUTE(query);
      assertEqual(10, results.json.length);
      for (var i = 0; i < 10; ++i) {
        assertEqual("test" + i, results.json[i].group);
        assertEqual(200, results.json[i].length);
        assertEqual(i + 1, results.json[i].min);
        assertEqual((1990 + i) * 2, results.json[i].max);
      }

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateAllReferToCollectvariable : function () {
      assertQueryError(errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code, "FOR i IN " + c.name() + " COLLECT group = i.group AGGREGATE length = LENGTH(group) RETURN { group, length }");
      assertQueryError(errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code, "FOR j IN " + c.name() + " COLLECT doc = j AGGREGATE length = LENGTH(doc) RETURN doc");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateFiltered : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group == 'test4' COLLECT group = i.group AGGREGATE length = LENGTH(i.value1), min = MIN(i.value1), max = MAX(i.value1), sum = SUM(i.value1), avg = AVERAGE(i.value1) RETURN { group, length, min, max, sum, avg }";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual("test4", results.json[0].group);
      assertEqual(200, results.json[0].length);
      assertEqual(4, results.json[0].min);
      assertEqual(1994, results.json[0].max);
      assertEqual(199800, results.json[0].sum);
      assertEqual(999, results.json[0].avg);

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateFilteredMulti : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group >= 'test2' && i.group <= 'test4' COLLECT group = i.group AGGREGATE length = LENGTH(i.value1), min = MIN(i.value1), max = MAX(i.value1), sum = SUM(i.value1), avg = AVERAGE(i.value1) RETURN { group, length, min, max, sum, avg }";

      var results = AQL_EXECUTE(query);
      assertEqual(3, results.json.length);
      for (var i = 2; i <= 4; ++i) {
        assertEqual("test" + i, results.json[i - 2].group);
        assertEqual(200, results.json[i - 2].length);
        assertEqual(i, results.json[i - 2].min);
        assertEqual(1990 + i, results.json[i - 2].max);
        assertEqual(199000 + i * 200, results.json[i - 2].sum);
        assertEqual(995 + i, results.json[i - 2].avg);
      }

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateFilteredEmpty : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group >= 'test99' COLLECT group = i.group AGGREGATE length = LENGTH(i.value1), min = MIN(i.value1), max = MAX(i.value1), sum = SUM(i.value1), avg = AVERAGE(i.value1) RETURN { group, length, min, max, sum, avg }";

      var results = AQL_EXECUTE(query);
      assertEqual(0, results.json.length);

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateFilteredBig : function () {
      var i;
      for (i = 0; i < 10000; ++i) {
        c.save({ age: 10 + (i % 80), type: 1 });
      }
      for (i = 0; i < 10000; ++i) {
        c.save({ age: 10 + (i % 80), type: 2 });
      }

      var query = "FOR i IN " + c.name() + " FILTER i.age >= 20 && i.age < 50 && i.type == 1 COLLECT AGGREGATE length = LENGTH(i) RETURN length";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(125 * 30, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateNested : function () {
      var query = "FOR i IN 1..2 FOR j IN " + c.name() + " COLLECT AGGREGATE length = LENGTH(j) RETURN length";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(4000, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateSimple : function () {
      var query = "FOR i IN " + c.name() + " COLLECT class = i.group AGGREGATE length = LENGTH(i) RETURN [ class, length ]";

      var results = AQL_EXECUTE(query);
      assertEqual(10, results.json.length);
      for (var i = 0; i < results.json.length; ++i) {
        var group = results.json[i];
        assertTrue(Array.isArray(group));
        assertEqual("test" + i, group[0]);
        assertEqual(200, group[1]);
      }

      var plan = AQL_EXPLAIN(query).plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate shaped
////////////////////////////////////////////////////////////////////////////////

    testAggregateShaped : function () {
      var query = "FOR j IN " + c.name() + " COLLECT doc = j AGGREGATE length = LENGTH(j) RETURN doc";

      var results = AQL_EXECUTE(query);
      // expectation is that we get 1000 different docs and do not crash (issue #1265)
      assertEqual(2000, results.json.length);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerAggregateTestSuite);

return jsunity.done();

