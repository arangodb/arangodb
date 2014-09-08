/*jslint indent: 2, nomen: true, maxlen: 200, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, assertTrue, assertEqual, AQL_EXECUTE, AQL_EXPLAIN, fail, loopmax */
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
var errors = require("internal").errors;
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults2;
var assertQueryError = helper.assertQueryError2;
var isEqual = helper.isEqual;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "remove-unnecessary-calculations";
  // various choices to control the optimizer: 
  var paramNone   = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled    = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramDisabled  = { optimizer: { rules: [ "+all", "-" + ruleName ] } };
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
          "LET a = SLEEP(2) RETURN 1"
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
          "FOR a IN 1 RETURN a + 1"
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
          "FOR i IN 1..10 LET a = 1 FILTER i == a RETURN i"
//          "FOR i IN 1..10 LET a = i + 1 FILTER i != a RETURN i"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
          //require("internal").print(result);
        assertEqual([ ruleName ], result.plan.rules);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////


    testPlans : function () {
      var plans = [ 
          ["FOR i IN 1..10 LET a = 1 FILTER i == a RETURN i", ["SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "ReturnNode" ]]
          ];

      plans.forEach(function(plan) {
          var result = AQL_EXPLAIN(plan[0], { }, paramEnabled);
          assertEqual([ ruleName ], result.plan.rules, plan[0]);
          //require("internal").print(helper.getCompactPlan(result).map(function(node) { return node.type; }));
          assertEqual(plan[1], helper.getCompactPlan(result).map(function(node) { return node.type; }), plan[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      var queries = [ 
          ["FOR i IN 1..10 LET a = 1 FILTER i == a RETURN i", ["SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "ReturnNode" ]]
      ];

      queries.forEach(function(query) {
          var resultDisabled = AQL_EXECUTE(query[0], { }, paramDisabled).json;
          var resultEnabled  = AQL_EXECUTE(query[0], { }, paramEnabled).json;

          assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
