/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "move-calculations-up";

  // various choices to control the optimizer:
  var paramNone   = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled    = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramDisabled  = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      var queries = [
        "LET a = 1 FOR i IN 1..10 FILTER a == 1 RETURN i",
        "FOR i IN 1..10 LET a = 25 RETURN i",
        "FOR i IN 1..10 LET a = 1 LET b = 2 LET c = 3 FILTER i > 3 LET d = 1 RETURN i"
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
        "FOR i IN 1..10 FILTER i == 1 RETURN i",
        "FOR i IN 1..10 LET a = 25 + i RETURN i",
        "LET values = 1..10 FOR i IN values LET a = i + 1 FOR j IN values LET b = j + 2 RETURN i",
        "FOR i IN 1..10 FILTER i == 7 COLLECT x = i RETURN x"
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
        "FOR i IN 1..10 LET a = 1 FILTER i == 1 RETURN i",
        "FOR i IN 1..10 LET a = 1 FILTER i == 1 RETURN a",
        "FOR i IN 1..10 FILTER i == 1 LET a = 1 RETURN i",
        "FOR i IN 1..10 LET a = 25 + 7 RETURN i",
        "FOR i IN 1..10 LET a = MIN([ i, 1 ]) LET b = i + 1 RETURN [ a, b ]",
        "FOR i IN 1..10 LET a = RAND() LET b = 25 + i RETURN i",
        "FOR i IN 1..10 LET a = SLEEP(0.1) LET b = i + 1 RETURN b",
        "FOR i IN 1..10 FOR j IN 1..10 LET a = i + 2 RETURN i",
        "FOR i IN 1..10 FILTER i == 7 LET a = i COLLECT x = i RETURN x"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual([ ruleName ], result.plan.rules, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule moves calculations out of subquery
////////////////////////////////////////////////////////////////////////////////

    testRuleMovesCalculationOutOfSubquery : function () {
      var query = "FOR i IN 1..10 LET x = (FOR j IN 1..10 LET a = i * 2 LET b = j * 2 RETURN a + b) RETURN x";
      var result = AQL_EXPLAIN(query, { }, paramEnabled);
      let nodes = result.plan.nodes;
      // calculations of variables "#7" and "a" are moved out
      assertEqual([
        "SingletonNode",     // ROOT
        "CalculationNode",   // - LET #9 = 1 .. 10   /* range */   /* simple expression */
        "CalculationNode",   // - LET #7 = 1 .. 10   /* range */   /* simple expression */
        "EnumerateListNode", // - FOR i IN #7   /* list iteration */
        "CalculationNode",   //   - LET a = (i * 2)   /* simple expression */
        "SubqueryStartNode", //   - LET x = ( /* subquery begin */
        "EnumerateListNode", //     - FOR j IN #9   /* list iteration */
        "CalculationNode",   //       - LET b = (j * 2)   /* simple expression */
        "CalculationNode",   //       - LET #11 = (a + b)   /* simple expression */
        "SubqueryEndNode",   //       - RETURN  #11 ) /* subquery end */
        "ReturnNode"         //   - RETURN x
      ], nodes.map(n => n.type));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////

    testPlans : function () {
      var plans = [
        [ "FOR i IN 1..10 LET a = 1 RETURN i", [ "SingletonNode", "CalculationNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 FILTER i == 1 LET a = 1 RETURN i", [ "SingletonNode", "CalculationNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "ReturnNode"  ] ]
      ];

      plans.forEach(function(plan) {
        var result = AQL_EXPLAIN(plan[0], { }, paramEnabled);
        assertEqual([ ruleName ], result.plan.rules, plan[0]);
        assertEqual(plan[1], helper.getCompactPlan(result).map(function(node) { return node.type; }), plan[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      var queries = [
        [ "FOR i IN 1..10 LET a = 1 RETURN i", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 LET a = 1 RETURN a", [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ] ],
        [ "FOR i IN 1..10 FILTER i == 1 LET a = 1 RETURN i", [ 1 ] ],
        [ "FOR i IN 1..10 FILTER i == 1 LET a = 1 RETURN a", [ 1 ] ],
      ];

      queries.forEach(function(query) {
        var planDisabled   = AQL_EXPLAIN(query[0], { }, paramDisabled);
        var planEnabled    = AQL_EXPLAIN(query[0], { }, paramEnabled);

        var resultDisabled = AQL_EXECUTE(query[0], { }, paramDisabled);
        var resultEnabled  = AQL_EXECUTE(query[0], { }, paramEnabled);

        assertTrue(planDisabled.plan.rules.indexOf(ruleName) === -1, query[0]);
        assertTrue(planEnabled.plan.rules.indexOf(ruleName) !== -1, query[0]);

        assertEqual(resultDisabled.json, query[1], query[0]);
        assertEqual(resultEnabled.json, query[1], query[0]);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

