/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

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
  var ruleName = "inline-subqueries";

  // various choices to control the optimizer: 
  var paramNone = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled = { optimizer: { rules: [ "-all", "+" + ruleName ] }, inspectSimplePlans: true };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has no effect when explicitly disabled
    ////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      var queries = [ 
        "FOR i IN (FOR x IN [1,2,3] RETURN x) RETURN i",
        "FOR i IN (FOR x IN (FOR y IN [1,2,3] RETURN y) RETURN x) RETURN i",
        "FOR i IN [1,2,3] LET x = (FOR j IN (FOR k IN [1,2,3] RETURN k) RETURN j) RETURN x"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramNone);
        assertEqual([ ], result.plan.rules, query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has no effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queries = [ 
        "FOR i IN [1,2,3] RETURN i",
        "LET a = [1,2,3] FOR i IN a RETURN i",
        "FOR i IN [1,2,3] LET x = (FOR j IN [1,2,3] RETURN j) RETURN x",
        "FOR i IN (FOR j IN [1,2,3] COLLECT v = j INTO g RETURN [v, g]) RETURN i"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual([ ], result.plan.rules, query);
        result = AQL_EXPLAIN(query, { }, paramNone);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has an effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var queries = [ 
        "FOR i IN (FOR j IN [1,2,3] RETURN j) RETURN i",
        "FOR i IN (FOR j IN [1,2,3] FILTER j > 1 RETURN j) RETURN i",
        "FOR i IN (FOR j IN [1,2,3] FILTER j > 1 RETURN j) FILTER i > 1 RETURN i",
        "FOR i IN (FOR j IN [1,2,3] FILTER j > 1 SORT j RETURN j) FILTER i > 1 RETURN i",
        "FOR i IN (FOR j IN [1,2,3] FILTER j > 1 SORT j RETURN j) FILTER i > 1 SORT i RETURN i",
        "FOR i IN (FOR j IN [1,2,3] FILTER j > 1 RETURN j * 2) FILTER i > 1 RETURN i * 3"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        result = AQL_EXPLAIN(query, { }, paramNone);
        var resultDisabled = AQL_EXECUTE(query, { }, paramDisabled).json;
        var resultEnabled  = AQL_EXECUTE(query, { }, paramEnabled).json;

        assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test generated plans
    ////////////////////////////////////////////////////////////////////////////////

    testPlans : function () {
      var plans = [ 
        // Const propagation:
        ["FOR i IN (FOR j IN [1,2,3] RETURN j) RETURN i", ["SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode"]],
        ["FOR i IN (FOR j IN [1,2,3] FILTER j > 1 RETURN j) RETURN i", ["SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "ReturnNode"]],
        ["FOR i IN (FOR j IN [1,2,3] RETURN j * 2) RETURN i", ["SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "ReturnNode"]],
        ["FOR i IN (FOR j IN (FOR k IN [1,2,3] RETURN k) RETURN j) RETURN i", ["SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode"]]
      ];

      plans.forEach(function(plan) {
        var result = AQL_EXPLAIN(plan[0], { }, paramEnabled);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), plan[0]);
        assertEqual(plan[1], helper.getCompactPlan(result).map(function(node) { return node.type; }), plan[0]);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test generated results
    ////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      var queries = [ 
        [ "FOR i IN (FOR j IN [1,2,3] RETURN j) RETURN i", [ 1, 2, 3 ] ],
        [ "FOR i IN (FOR j IN [1,2,3] FILTER j > 1 RETURN j) RETURN i", [ 2, 3 ] ],
        [ "FOR i IN (FOR j IN [1,2,3] FILTER j > 1 RETURN j * 2) RETURN i", [ 4, 6 ] ],
        [ "FOR i IN (FOR j IN [1,2,3] FILTER j > 1 RETURN j * 2) FILTER i >= 6 RETURN i", [ 6 ] ],
        [ "FOR i IN (FOR j IN (FOR k IN [1,2,3] RETURN k) RETURN j * 2) RETURN i * 2", [ 4, 8, 12 ] ],
        [ "FOR i IN (FOR j IN (FOR k IN [1,2,3,4] SORT k DESC LIMIT 3 RETURN k) LIMIT 2 RETURN j) RETURN i", [ 4, 3 ] ]
      ];
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);

        result = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], result, query);
      });
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
