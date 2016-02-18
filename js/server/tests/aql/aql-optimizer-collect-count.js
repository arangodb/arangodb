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

function optimizerCountTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

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

    testCountTotalSimple : function () {
      var query = "FOR i IN " + c.name() + " COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);
       
      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalNested : function () {
      var query = "FOR i IN 1..2 FOR j IN " + c.name() + " COLLECT WITH COUNT INTO count RETURN count";

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(2000, results.json[0]);

      var plan = AQL_EXPLAIN(query).plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count shaped
////////////////////////////////////////////////////////////////////////////////

    testCountShaped : function () {
      var query = "FOR j IN " + c.name() + " COLLECT doc = j WITH COUNT INTO count RETURN doc";

      var results = AQL_EXECUTE(query);
      // expectation is that we get 1000 different docs and do not crash (issue #1265)
      assertEqual(1000, results.json.length);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerCountTestSuite);

return jsunity.done();

