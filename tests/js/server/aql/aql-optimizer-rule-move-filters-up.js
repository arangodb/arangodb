/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

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

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "move-filters-up";
  // various choices to control the optimizer: 
  var paramNone     = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled  = { optimizer: { rules: [ "-all", "+move-calculations-up", "+" + ruleName ] } };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      var queries = [ 
        "FOR i IN 1..10 SORT i FILTER i == 1 RETURN 1",
        "FOR i IN 1..10 LET x = (FOR j IN [ i ] RETURN j) FILTER i == 2 RETURN x",
        "FOR i IN 1..10 FOR j IN 1..10 FILTER i == 1 FILTER j == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, { }, paramNone);
        assertEqual([], result.plan.rules.filter((r) => r !== "splice-subqueries"));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queries = [ 
        "FOR i IN 1..10 FILTER i > 1 RETURN i",
        "LET a = 99 FOR i IN 1..10 FILTER i > a RETURN i",
        "LET a = 1 FOR i IN 1..10 FILTER a != i RETURN i",
        "FOR i IN 1..10 LET a = i LET b = i FILTER a == b RETURN i",
        "FOR i IN 1..10 FOR j IN 1..10 FILTER i > j RETURN i",
        "FOR i IN 1..10 LET a = 2 * i FILTER a == 1 RETURN i",
        "FOR i IN 1..10 INSERT { } INTO _users FILTER i < 5 RETURN i", // can't pull filter beyond INSERT
        "FOR i IN 1..10 REMOVE 'foo' INTO _users FILTER i < 5 RETURN i", // can't pull filter beyond REMOVE
        "FOR i IN 1..10 UPDATE 'foo' WITH { } INTO _users FILTER i < 5 RETURN i", // can't pull filter beyond UPDATE
        "FOR i IN 1..10 REPLACE 'foo' WITH { } INTO _users FILTER i < 5 RETURN i", // can't pull filter beyond REPLACE
        "FOR i IN 1..10 LET a = SLEEP(1) FILTER i == 1 RETURN i" // SLEEP is non-deterministic
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertTrue(result.plan.rules.indexOf(ruleName) === -1, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var queries = [ 
        "FOR i IN 1..10 FOR j IN 1..10 FILTER i > 1 RETURN i",
        "FOR i IN 1..10 LET x = (FOR j IN [i] RETURN j) FILTER i > 1 RETURN i",
        "FOR i IN 1..10 FOR l IN 1..10 LET a = 2 * i FILTER i == 1 RETURN a",
        "FOR i IN 1..10 FOR l IN 1..10 LET a = 2 * i FILTER i == 1 LIMIT 1 RETURN a",
        "FOR i IN 1..10 FOR l IN 1..10 LET a = MIN([1, l]) FILTER i == 1 LIMIT 1 RETURN a",
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////


    testPlans : function () {
      var plans = [ 
        [ "FOR i IN 1..10 FOR j IN 1..10 FILTER i > 1 RETURN i", [ "SingletonNode", "CalculationNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET x = (FOR j IN [i] RETURN j) FILTER i > 1 RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "CalculationNode", "FilterNode", "SubqueryStartNode", "EnumerateListNode", "SubqueryEndNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 FOR l IN 1..10 LET a = 2 * i FILTER i == 1 RETURN a", [ "SingletonNode", "CalculationNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "CalculationNode", "FilterNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 FOR j IN 1..10 FILTER i == 1 FILTER j == 2 RETURN i", [ "SingletonNode", "CalculationNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "EnumerateListNode", "CalculationNode", "FilterNode", "ReturnNode" ] ]
      ];

      plans.forEach(function(plan) {
        var result = AQL_EXPLAIN(plan[0], { }, paramEnabled);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), plan[0]);
        assertEqual(plan[1], helper.getCompactPlan(result).map(function(node) { return node.type; }), plan[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      var queries = [ 
        [ "FOR i IN 1..10 FOR j IN 1..10 FILTER i > 9 RETURN j", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 LET x = (FOR j IN [i] RETURN j) FILTER i > 9 RETURN x", [ [ 10 ] ] ],
        [ "FOR i IN 1..10 FOR l IN 1..1 LET a = 2 * i FILTER i == 1 RETURN a", [ 2 ] ],
        [ "FOR i IN 1..10 LET a = i FOR j IN 1..10 LET b = j FILTER a == 1 FILTER b == 2 RETURN [ a, b ]", [ [ 1, 2 ] ] ]
      ];

      queries.forEach(function(query) {
        var planDisabled   = AQL_EXPLAIN(query[0], { }, paramDisabled);
        var planEnabled    = AQL_EXPLAIN(query[0], { }, paramEnabled);
        var resultDisabled = AQL_EXECUTE(query[0], { }, paramDisabled).json;
        var resultEnabled  = AQL_EXECUTE(query[0], { }, paramEnabled).json;

        assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);

        assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query[0]);
        assertNotEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query[0]);

        assertEqual(resultDisabled, query[1]);
        assertEqual(resultEnabled, query[1]);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

