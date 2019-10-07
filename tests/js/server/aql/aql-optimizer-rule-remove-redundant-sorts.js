/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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

const _ = require('lodash');
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const isEqual = helper.isEqual;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "remove-redundant-sorts";

  // various choices to control the optimizer: 
  var paramNone     = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };
  var paramMore     = { optimizer: { rules: [ "-all", "+" + ruleName, "+remove-unnecessary-calculations-2" ] } };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      var queries = [
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a SORT i.a RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a FILTER i.a == 1 SORT i.a RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a LIMIT 1 SORT i.a RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a SORT i.b RETURN i"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramNone);
        assertEqual([ ], result.plan.rules);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queries = [ 
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a + FAIL() SORT i.b RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a SORT RAND() RETURN i"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual([ ], result.plan.rules, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var queries = [ 
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a SORT i.a RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a, i.b SORT i.a, i.b RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a + 1 SORT i.b RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a DESC SORT i.a DESC RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a, i.b DESC SORT i.a, i.b DESC RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a FILTER i.a == 1 SORT i.a RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a DESC FILTER i.a == 1 SORT i.a DESC RETURN i",
        "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a SORT i.a, i.b RETURN i", 
        "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a, i.b SORT i.a RETURN i", 
        "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a, i.b SORT i.a SORT i.a, i.b RETURN i", 
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a ASC SORT i.a DESC RETURN i",
        "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] COLLECT x = i.a SORT x RETURN x"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual([ ruleName ], result.plan.rules, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////


    testPlans : function () {
      var plans = [
        [ "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a SORT i.a RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "SortNode", "ReturnNode" ] ],
        [ "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a DESC SORT i.a DESC RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "SortNode", "ReturnNode" ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 2, b: 1 }, { a: 3, b: 1 } ] SORT i.a ASC, i.b DESC SORT i.a DESC, i.b ASC RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "CalculationNode", "SortNode", "ReturnNode" ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 2, b: 1 }, { a: 3, b: 1 } ] SORT i.a ASC, i.b DESC FILTER i.a == 1 SORT i.a DESC, i.b ASC RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "SortNode", "ReturnNode" ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 2, b: 1 }, { a: 3, b: 1 } ] SORT i.a ASC COLLECT x = i.a RETURN x", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "SortNode", "CollectNode", "ReturnNode" ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 2, b: 1 }, { a: 3, b: 1 } ] SORT i.a, i.b SORT i.a SORT i.a RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "CalculationNode", "SortNode", "ReturnNode" ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 2, b: 1 }, { a: 3, b: 1 } ] SORT i.a, i.b SORT i.a DESC SORT i.a ASC RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "CalculationNode", "SortNode", "ReturnNode" ] ]
      ]; 

      plans.forEach(function(plan) {
        var result = AQL_EXPLAIN(plan[0], { }, paramMore);
        assertTrue(result.plan.rules.indexOf(ruleName) !== -1, plan[0]);
        assertEqual(plan[1], helper.getCompactPlan(result).map(function(node) { return node.type; }), plan[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      var queries = [ 
        [ "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a SORT i.a RETURN i", [ { a: 1 }, { a: 2 }, { a: 3 } ] ],
        [ "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a DESC SORT i.a DESC RETURN i", [ { a: 3 }, { a: 2 }, { a: 1 } ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a, i.b ASC SORT i.a, i.b ASC RETURN i", [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a, i.b DESC SORT i.a, i.b DESC RETURN i", [ { a: 1, b: 2 }, { a: 1, b: 1 }, { a: 2, b: 2 }, { a: 2, b: 1 }, { a: 3, b: 2 }, { a: 3, b: 1 } ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a DESC, i.b DESC SORT i.a DESC, i.b DESC RETURN i", [ { a: 3, b: 2 }, { a: 3, b: 1 }, { a: 2, b: 2 }, { a: 2, b: 1 }, { a: 1, b: 2 }, { a: 1, b: 1 } ] ],
        [ "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a DESC SORT i.a RETURN i", [ { a: 1 }, { a: 2 }, { a: 3 } ] ],
        [ "FOR i IN [ { a: 1 }, { a: 2 }, { a: 3 } ] SORT i.a ASC SORT i.a DESC RETURN i", [ { a: 3 }, { a: 2 }, { a: 1 } ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a, i.b DESC SORT i.a, i.b ASC RETURN i", [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a, i.b ASC SORT i.a, i.b DESC RETURN i", [ { a: 1, b: 2 }, { a: 1, b: 1 }, { a: 2, b: 2 }, { a: 2, b: 1 }, { a: 3, b: 2 }, { a: 3, b: 1 } ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a, i.b ASC SORT i.a DESC, i.b DESC RETURN i", [ { a: 3, b: 2 }, { a: 3, b: 1 }, { a: 2, b: 2 }, { a: 2, b: 1 }, { a: 1, b: 2 }, { a: 1, b: 1 } ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a ASC, i.b DESC COLLECT x = i.a INTO g RETURN { x: x, g: g[*].i.b }", [ { x: 1, g: [ 2, 1 ] }, { x: 2, g: [ 2, 1 ] }, { x: 3, g: [ 2, 1 ] } ] ],
      ];

      queries.forEach(function(query) {
        var planDisabled   = AQL_EXPLAIN(query[0], { }, paramDisabled);
        var planEnabled    = AQL_EXPLAIN(query[0], { }, paramEnabled);
        var resultDisabled = AQL_EXECUTE(query[0], { }, paramDisabled).json;
        var resultEnabled  = AQL_EXECUTE(query[0], { }, paramEnabled).json;

        assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);

        assertTrue(planDisabled.plan.rules.indexOf(ruleName) === -1, query[0]);
        assertTrue(planEnabled.plan.rules.indexOf(ruleName) !== -1, query[0]);

        assertEqual(resultDisabled, query[1]);
        assertEqual(resultEnabled, query[1]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test limit
////////////////////////////////////////////////////////////////////////////////

    testLimit : function () {
      var queries = [ 
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a, i.b LIMIT 2 SORT i.a DESC, i.b DESC RETURN i", [ { a: 1, b: 2 }, { a: 1, b: 1 } ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a DESC, i.b DESC LIMIT 2 SORT i.a, i.b RETURN i", [ { a: 3, b: 1 }, { a: 3, b: 2 } ] ],
      ];

      queries.forEach(function(query) {
        var result = AQL_EXECUTE(query[0], { }, paramDisabled).json;
        assertEqual(result, query[1]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collect
////////////////////////////////////////////////////////////////////////////////

    testCollect : function () {
      var queries = [ 
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] COLLECT x = i.a, y = i.b SORT x, y RETURN { a: x, b: y }", [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.b, i.a COLLECT x = i.a RETURN { a: x }", [ { a: 1 }, { a: 2 }, { a: 3 } ] ],
        [ "FOR i IN [ { a: 1, b: 1 }, { a: 1, b: 2 }, { a: 2, b: 1 }, { a: 2, b: 2 }, { a: 3, b: 1 }, { a: 3, b: 2 } ] SORT i.a COLLECT x = i.a RETURN { a: x }", [ { a: 1 }, { a: 2 }, { a: 3 } ] ]
      ];

      queries.forEach(function(query) {
        var result = AQL_EXECUTE(query[0], { }, paramDisabled).json;
        assertEqual(result, query[1], query[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collect variable pass through
///
/// Regression test, input registers were only copied in the first block
/// in sorted collect.
////////////////////////////////////////////////////////////////////////////////


    testCollectWithVariablePassThrough : function () {
      const query = `
        LET x = LENGTH(1..42)
        FOR i IN 1..2000
        LET d = {val: i}
        SORT d.val
        COLLECT v = d.val
        RETURN {v, x}
      `;

      const expected = _.range(1, 2001).map(i => ({v: i, x: 42}));

      const result = AQL_EXECUTE(query, {}, paramEnabled).json;
      assertEqual(result.length, expected.length);
      // Don't compare the whole arrays, for a better readable output in case
      // of errors.
      for (let i = 0; i < expected.length; i++) {
        assertEqual(result[i], expected[i], `mismatch at index ${i}`);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collect with offset
///
/// Regression test, there was a segfault in sorted collect when at least one
/// row was skipped.
////////////////////////////////////////////////////////////////////////////////


    testCollectWithOffset : function () {
      const query = `
        FOR i IN 1..2
        LET d = {val: i}
        SORT d.val
        COLLECT v = d.val
        LIMIT 1,1
        RETURN v
      `;

      const result = AQL_EXECUTE(query, {}, paramEnabled).json;
      assertEqual(result, [2]);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

