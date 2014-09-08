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
  var paramMCU    = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramNoMCU  = { optimizer: { rules: [ "+all", "-" + ruleName ] } };
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
          "FOR a IN 1 RETURN a + 1",
          "FOR a IN 1 RETURN a + 0",
          "FOR a IN 1 RETURN a - 1",
          "FOR a IN 1 RETURN a * 2",
          "FOR a IN 4 RETURN a / 2",
          "FOR a IN 4 RETURN a % 3",
          "FOR a IN 4 RETURN a == 8",
          "FOR a IN 4 RETURN 2",
          "FOR a IN [1, 2, 3, 4, 5, 6] RETURN SLICE(a, 4, 1)",
          "FOR a IN 17.33 RETURN FLOOR(a)",
          "FOR a IN 17.33 RETURN CEIL(a)",
          "FOR a IN 17.33 RETURN FLOOR(a + 3)",
          "FOR a IN 17.33 RETURN CEIL(a + 3)",
          "FOR a IN 17.33 RETURN FLOOR(a / 77)",
          "FOR a IN 17.33 RETURN CEIL(a / 77)",
          "FOR a IN -12 RETURN TO_BOOL(a)",
          "FOR a IN \"-12\" RETURN TO_NUMBER(a)",
          "FOR a IN \"-12\" RETURN IS_NUMBER(a)",
          "FOR a IN \"-12\" RETURN TO_LIST(a)",
          "FOR a IN { \"a\" : null, \"b\" : -63, \"c\" : [ 1, 2 ], \"d\": { \"a\" : \"b\" } } RETURN TO_LIST(a)",
          "FOR a IN -12 RETURN ABS(a)",
          "FOR a IN -12 RETURN ABS(a + 17)",
          "FOR a IN 17.33 RETURN ROUND(a)",
          "FOR a IN 17.33 RETURN SQRT(a)",
          "FOR a IN -17.33 RETURN SQRT(a)",
          "FOR a IN CHAR_LENGTH('äöボカド名üÄÖÜß') return a + 1",
          "FOR a IN 7 return a..12",
          "FOR a IN [1, 7, 3, 12] RETURN AVERAGE(a)",
          "FOR a IN [1, 7, 3, 12] RETURN MEDIAN(a)",
          "FOR a IN [1, 7, 3, 12] RETURN MAX(a)",
          "FOR a IN [1, 7, 3, 12] RETURN SUM(a)",
          "FOR a IN [1, 7, 3, 12, null] RETURN NOT_NULL(a)",
          "FOR a IN [1, 7, 3, 12, null] RETURN FIRST_LIST(a)",
          "FOR a IN [null, \"not a doc!\"] RETURN FIRST_DOCUMENT(a)",
          "FOR a IN [0.75, 0.8] RETURN VARIANCE_SAMPLE(a)",
          "FOR a IN [0.75, 0.8] RETURN STDDEV_POPULATION(a)",
          "FOR a IN 5 RETURN DATE_DAYOFWEEK(a)",
          "FOR a IN 5 RETURN DATE_MONTH(a)",
          "FOR a IN 5 RETURN DATE_DAY(a)",
          "FOR a IN 5 RETURN DATE_HOUR(a)",
          "FOR a IN 5 RETURN DATE_MINUTE(a)",
          "FOR a IN 5 RETURN DATE_SECOND(a)",
          "FOR a IN 5 RETURN DATE_MILLISECOND(a)",
          "FOR a IN 5 RETURN DATE_TIMESTAMP(a)",
          "FOR a IN 5 RETURN DATE_ISO8601(a)",
          "FOR a IN 1975 RETURN DATE_YEAR(a)",
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
          "FOR a IN 1 RETURN a + 1",
          "FOR a IN 1 RETURN a + 0",
          "FOR a IN 1 RETURN a - 1",
          "FOR a IN 1 RETURN a * 2",
          "FOR a IN 4 RETURN a / 2",
          "FOR a IN 4 RETURN a % 3",
          "FOR a IN 4 RETURN a == 8",
          "FOR a IN 4 RETURN 2",
          "FOR a IN [1, 2, 3, 4, 5, 6] RETURN SLICE(a, 4, 1)",
          "FOR a IN 17.33 RETURN FLOOR(a)",
          "FOR a IN 17.33 RETURN CEIL(a)",
          "FOR a IN 17.33 RETURN FLOOR(a + 3)",
          "FOR a IN 17.33 RETURN CEIL(a + 3)",
          "FOR a IN 17.33 RETURN FLOOR(a / 77)",
          "FOR a IN 17.33 RETURN CEIL(a / 77)",
          "FOR a IN -12 RETURN TO_BOOL(a)",
          "FOR a IN \"-12\" RETURN TO_NUMBER(a)",
          "FOR a IN \"-12\" RETURN IS_NUMBER(a)",
          "FOR a IN \"-12\" RETURN TO_LIST(a)",
          "FOR a IN { \"a\" : null, \"b\" : -63, \"c\" : [ 1, 2 ], \"d\": { \"a\" : \"b\" } } RETURN TO_LIST(a)",
          "FOR a IN -12 RETURN ABS(a)",
          "FOR a IN -12 RETURN ABS(a + 17)",
          "FOR a IN 17.33 RETURN ROUND(a)",
          "FOR a IN 17.33 RETURN SQRT(a)",
          "FOR a IN -17.33 RETURN SQRT(a)",
          "FOR a IN CHAR_LENGTH('äöボカド名üÄÖÜß') return a + 1",
          "FOR a IN 7 return a..12",
          "FOR a IN [1, 7, 3, 12] RETURN AVERAGE(a)",
          "FOR a IN [1, 7, 3, 12] RETURN MEDIAN(a)",
          "FOR a IN [1, 7, 3, 12] RETURN MAX(a)",
          "FOR a IN [1, 7, 3, 12] RETURN SUM(a)",
          "FOR a IN [1, 7, 3, 12, null] RETURN NOT_NULL(a)",
          "FOR a IN [1, 7, 3, 12, null] RETURN FIRST_LIST(a)",
          "FOR a IN [null, \"not a doc!\"] RETURN FIRST_DOCUMENT(a)",
          "FOR a IN [0.75, 0.8] RETURN VARIANCE_SAMPLE(a)",
          "FOR a IN [0.75, 0.8] RETURN STDDEV_POPULATION(a)",
          "FOR a IN 5 RETURN DATE_DAYOFWEEK(a)",
          "FOR a IN 5 RETURN DATE_MONTH(a)",
          "FOR a IN 5 RETURN DATE_DAY(a)",
          "FOR a IN 5 RETURN DATE_HOUR(a)",
          "FOR a IN 5 RETURN DATE_MINUTE(a)",
          "FOR a IN 5 RETURN DATE_SECOND(a)",
          "FOR a IN 5 RETURN DATE_MILLISECOND(a)",
          "FOR a IN 5 RETURN DATE_TIMESTAMP(a)",
          "FOR a IN 5 RETURN DATE_ISO8601(a)",
          "FOR a IN 1975 RETURN DATE_YEAR(a)",
          "LET a = SLEEP(2) RETURN 1"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramMCU);
        assertEqual([ ], result.plan.rules, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var queries = [ 
          "LET a = 1 RETURN a + 1",
          "LET a = 1 RETURN a + 0",
          "LET a = 1 RETURN a - 1",
          "LET a = 1 RETURN a * 2",
          "LET a = 4 RETURN a / 2",
          "LET a = 4 RETURN a % 3",
          "LET a = 4 RETURN a == 8",
          "LET a = 4 RETURN 2",
          "LET a = [1, 2, 3, 4, 5, 6] RETURN SLICE(a, 4, 1)",
          "LET a = 17.33 RETURN FLOOR(a)",
          "LET a = 17.33 RETURN CEIL(a)",
          "LET a = 17.33 RETURN FLOOR(a + 3)",
          "LET a = 17.33 RETURN CEIL(a + 3)",
          "LET a = 17.33 RETURN FLOOR(a / 77)",
          "LET a = 17.33 RETURN CEIL(a / 77)",
          "LET a = -12 RETURN TO_BOOL(a)",
          "LET a = \"-12\" RETURN TO_NUMBER(a)",
          "LET a = \"-12\" RETURN IS_NUMBER(a)",
          "LET a = \"-12\" RETURN TO_LIST(a)",
          "LET a = { \"a\" : null, \"b\" : -63, \"c\" : [ 1, 2 ], \"d\": { \"a\" : \"b\" } } RETURN TO_LIST(a)",
          "LET a = -12 RETURN ABS(a)",
          "LET a = -12 RETURN ABS(a + 17)",
          "LET a = 17.33 RETURN ROUND(a)",
          "LET a = 17.33 RETURN SQRT(a)",
          "LET a = -17.33 RETURN SQRT(a)",
          "LET a = CHAR_LENGTH('äöボカド名üÄÖÜß') return a + 1",
          "LET a = 7 return a..12",
          "LET a = 1 LET b = 2 RETURN [ a, b ]",
          "LET a = [1, 7, 3, 12] RETURN AVERAGE(a)",
          "LET a = [1, 7, 3, 12] RETURN MEDIAN(a)",
          "LET a = [1, 7, 3, 12] RETURN MAX(a)",
          "LET a = [1, 7, 3, 12] RETURN SUM(a)",
          "LET a = [1, 7, 3, 12, null] RETURN NOT_NULL(a)",
          "LET a = [1, 7, 3, 12, null] RETURN FIRST_LIST(a)",
          "LET a = [null, \"not a doc!\"] RETURN FIRST_DOCUMENT(a)",
          "LET a = [0.75, 0.8] RETURN VARIANCE_SAMPLE(a)",
          "LET a = [0.75, 0.8] RETURN STDDEV_POPULATION(a)",
          "LET a = 5 RETURN DATE_DAYOFWEEK(a)",
          "LET a = 5 RETURN DATE_MONTH(a)",
          "LET a = 5 RETURN DATE_DAY(a)",
          "LET a = 5 RETURN DATE_HOUR(a)",
          "LET a = 5 RETURN DATE_MINUTE(a)",
          "LET a = 5 RETURN DATE_SECOND(a)",
          "LET a = 5 RETURN DATE_MILLISECOND(a)",
          "LET a = 5 RETURN DATE_TIMESTAMP(a)",
          "LET a = 5 RETURN DATE_ISO8601(a)",
          "LET a = 1975 RETURN DATE_YEAR(a)",
          "FOR i IN 1..10 LET a = 1 FILTER i == a RETURN i"
//          "FOR i IN 1..10 LET a = i + 1 FILTER i != a RETURN i"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramMCU);
          //require("internal").print(result);
        assertEqual([ ruleName ], result.plan.rules);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////


    testPlans : function () {
      var plans = [ 
          ["LET a = 1 RETURN a + 1", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 1 RETURN a + 0", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 1 RETURN a - 1", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 1 RETURN a * 2", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 4 RETURN a / 2", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 4 RETURN a % 3", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 4 RETURN a == 8", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 4 RETURN 2", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 2, 3, 4, 5, 6] RETURN SLICE(a, 4, 1)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN FLOOR(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN CEIL(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN FLOOR(a + 3)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN CEIL(a + 3)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN FLOOR(a / 77)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN CEIL(a / 77)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = -12 RETURN TO_BOOL(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = \"-12\" RETURN TO_NUMBER(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = \"-12\" RETURN IS_NUMBER(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = \"-12\" RETURN TO_LIST(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = { \"a\" : null, \"b\" : -63, \"c\" : [ 1, 2 ], \"d\": { \"a\" : \"b\" } } RETURN TO_LIST(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = -12 RETURN ABS(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = -12 RETURN ABS(a + 17)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN ROUND(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN SQRT(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = -17.33 RETURN SQRT(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = CHAR_LENGTH('äöボカド名üÄÖÜß') return a + 1", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 7 return a..12", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 1 LET b = 2 RETURN [ a, b ]", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12] RETURN AVERAGE(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12] RETURN MEDIAN(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12] RETURN MAX(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12] RETURN SUM(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12, null] RETURN NOT_NULL(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12, null] RETURN FIRST_LIST(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [null, \"not a doc!\"] RETURN FIRST_DOCUMENT(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [0.75, 0.8] RETURN VARIANCE_SAMPLE(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [0.75, 0.8] RETURN STDDEV_POPULATION(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_DAYOFWEEK(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_MONTH(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_DAY(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_HOUR(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_MINUTE(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_SECOND(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_MILLISECOND(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_TIMESTAMP(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_ISO8601(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 1975 RETURN DATE_YEAR(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["FOR i IN 1..10 LET a = 1 FILTER i == a RETURN i", ["SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "ReturnNode" ]]
          ];

      plans.forEach(function(plan) {
          var result = AQL_EXPLAIN(plan[0], { }, paramMCU);
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
          ["LET a = 1 RETURN a + 1", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 1 RETURN a + 0", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 1 RETURN a - 1", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 1 RETURN a * 2", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 4 RETURN a / 2", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 4 RETURN a % 3", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 4 RETURN a == 8", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 4 RETURN 2", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 2, 3, 4, 5, 6] RETURN SLICE(a, 4, 1)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN FLOOR(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN CEIL(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN FLOOR(a + 3)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN CEIL(a + 3)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN FLOOR(a / 77)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN CEIL(a / 77)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = -12 RETURN TO_BOOL(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = \"-12\" RETURN TO_NUMBER(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = \"-12\" RETURN IS_NUMBER(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = \"-12\" RETURN TO_LIST(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = { \"a\" : null, \"b\" : -63, \"c\" : [ 1, 2 ], \"d\": { \"a\" : \"b\" } } RETURN TO_LIST(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = -12 RETURN ABS(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = -12 RETURN ABS(a + 17)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN ROUND(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 17.33 RETURN SQRT(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = -17.33 RETURN SQRT(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = CHAR_LENGTH('äöボカド名üÄÖÜß') return a + 1", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 7 return a..12", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 1 LET b = 2 RETURN [ a, b ]", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12] RETURN AVERAGE(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12] RETURN MEDIAN(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12] RETURN MAX(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12] RETURN SUM(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12, null] RETURN NOT_NULL(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [1, 7, 3, 12, null] RETURN FIRST_LIST(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [null, \"not a doc!\"] RETURN FIRST_DOCUMENT(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [0.75, 0.8] RETURN VARIANCE_SAMPLE(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = [0.75, 0.8] RETURN STDDEV_POPULATION(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_DAYOFWEEK(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_MONTH(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_DAY(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_HOUR(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_MINUTE(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_SECOND(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_MILLISECOND(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_TIMESTAMP(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 5 RETURN DATE_ISO8601(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["LET a = 1975 RETURN DATE_YEAR(a)", ["SingletonNode", "CalculationNode", "ReturnNode"]],
          ["FOR i IN 1..10 LET a = 1 FILTER i == a RETURN i", ["SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "ReturnNode" ]]
      ];

      queries.forEach(function(query) {
          var resultDisabled = AQL_EXECUTE(query[0], { }, paramNoMCU).json;
          var resultEnabled  = AQL_EXECUTE(query[0], { }, paramMCU).json;

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
