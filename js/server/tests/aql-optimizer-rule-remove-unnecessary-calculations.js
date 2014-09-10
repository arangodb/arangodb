/*jslint indent: 2, nomen: true, maxlen: 200, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, assertTrue, assertEqual, AQL_EXECUTE, AQL_EXPLAIN */
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

var PY = function (plan) { require("internal").print(require("js-yaml").safeDump(plan))}
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
          "FOR a IN 1 LET b=a+1 RETURN b+2",
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
          "FOR a IN [5] RETURN DATE_DAYOFWEEK(a)",
          "FOR a IN [5] RETURN DATE_MONTH(a)",
          "FOR a IN [5] RETURN DATE_DAY(a)",
          "FOR a IN [5] RETURN DATE_HOUR(a)",
          "FOR a IN [5] RETURN DATE_MINUTE(a)",
          "FOR a IN [5] RETURN DATE_SECOND(a)",
          "FOR a IN [5] RETURN DATE_MILLISECOND(a)",
          "FOR a IN [5] RETURN DATE_TIMESTAMP(a)",
          "FOR a IN [5] RETURN DATE_ISO8601(a)",
          "FOR a IN [1975] RETURN DATE_YEAR(a)",
          "LET a = SLEEP(2) RETURN 1"
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
          "FOR a in [1..10] RETURN SQRT(a)",
          "LET a = RAND() LET b = a + 1 RETURN b",

          "FOR a IN [1] RETURN a + 1",
          "FOR a IN [1] RETURN a + 0",
          "FOR a IN [1] RETURN a - 1",
          "FOR a IN [1] RETURN a * 2",
          "FOR a IN [4] RETURN a / 2",
          "FOR a IN [4] RETURN a % 3",
          "FOR a IN [4] RETURN a == 8",
          "FOR a IN [4] RETURN 2",
          "FOR a IN [1, 2, 3, 4, 5, 6] RETURN SLICE(a, 4, 1)",
          "FOR a IN [17.33] RETURN FLOOR(a)",
          "FOR a IN ['-12'] RETURN TO_LIST(a)",
          "FOR a IN { \"a\" : null, \"b\" : -63, \"c\" : [ 1, 2 ], \"d\": { \"a\" : \"b\" } } RETURN TO_LIST(a)",
          "FOR a IN -12 RETURN ABS(a)",
          "FOR a IN -12 RETURN ABS(a + 17)",
          "FOR a IN 17.33 RETURN ROUND(a)",
          "FOR a IN 17.33 RETURN SQRT(a)",
          "FOR a IN -17.33 RETURN SQRT(a)",
          "FOR a IN CHAR_LENGTH('äöボカド名üÄÖÜß') return a + 1",
          "FOR a IN 7 return a..12",
          "FOR a IN [1, 7, 3, 12] RETURN AVERAGE(a)",
          "FOR a IN [1, 7, 3, 12] RETURN AVERAGE(a)",
          "FOR a IN [1, 7, 3, 12] RETURN MEDIAN(a)",
          "FOR a IN [1, 7, 3, 12] RETURN MAX(a)",
          "FOR a IN [1, 7, 3, 12] RETURN SUM(a)",
          "FOR a IN [1, 7, 3, 12, null] RETURN NOT_NULL(a)",
          "FOR a IN [1, 7, 3, 12, null] RETURN FIRST_LIST(a)",
          "FOR a IN [null, \"not a doc!\"] RETURN FIRST_DOCUMENT(a)",
          "FOR a IN [0.75, 0.8] RETURN VARIANCE_SAMPLE(a)",
          "FOR a IN [0.75, 0.8] RETURN STDDEV_POPULATION(a)",
          "FOR a IN [5] RETURN DATE_DAYOFWEEK(a)",
          "FOR a IN [5] RETURN DATE_MONTH(a)",
          "FOR a IN [5] RETURN DATE_DAY(a)",
          "FOR a IN [5] RETURN DATE_HOUR(a)",
          "FOR a IN [5] RETURN DATE_MINUTE(a)",
          "FOR a IN [5] RETURN DATE_SECOND(a)",
          "FOR a IN [5] RETURN DATE_MILLISECOND(a)",
          "FOR a IN [5] RETURN DATE_TIMESTAMP(a)",
          "FOR a IN [5] RETURN DATE_ISO8601(a)",
          "FOR a IN [1975] RETURN DATE_YEAR(a)",

          "LET a = SLEEP(2) RETURN 1",

          // String functions: 
          "FOR a IN ['aoeu', 'snth'] RETURN CONCAT(a, 'htns')",
          "FOR a IN ['aoeu', 'snth'] RETURN CONCAT_SEPARATOR('|', a, 'htns')",
          "FOR a IN ['AOEU', 'SNTH'] RETURN LOWER(a)",
          "FOR a IN ['aoeu', 'snth'] RETURN UPPER(a)",
          "FOR a IN ['aoeu', 'snth'] RETURN SUBSTRING(a, 1, 2)",
          "FOR a IN ['aoeu', 'snth'] RETURN CONTAINS(a, 'htns')",
          "FOR a IN ['aoeu', 'snth'] RETURN LIKE(a, '%nt%')",
          "FOR a IN ['aoeu', 'snth'] RETURN LEFT(a, 2)",
          "FOR a IN ['aoeu', 'snth'] RETURN RIGHT(a, 2)",
          "FOR a IN [' aoeu ', ' snth '] RETURN TRIM(a)",
          "FOR a IN ['äöボカド名üÄÖÜß'] RETURN CHAR_LENGTH(a)",

          "FOR a in [SQRT(1), SQRT(2)] RETURN a",
          "FOR a IN [{'age':1},{'age':3}] COLLECT age = a.age RETURN age",
          "FOR a in [1,7,4,3,4,5,6,9,8] SORT a + 1 DESC RETURN a",
          "FOR a in [1,7,4,3,4,5,6,9,8] SORT a + 1 RETURN a",
          "FOR a in [1,7,4,3,4,5,6,9,8] SORT a + 1, SQRT(a) RETURN a",
          "FOR a in [1,7,4,3,4,5,6,9,8] FILTER a != 4 RETURN a",

          

          "RETURN 1"
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
/// AQL_EXPLAIN("FOR a IN 1 LET b=a+1 RETURN a+2"); -> tuts nur kombiniert
    testRuleHasEffect : function () {
      var queries = [ 
//          "FOR a IN 1 LET b=a+1 RETURN b+2"
// Const propagation:
          "LET a = 11 RETURN a + 1",
          "LET a = 11 RETURN a + 0",
          "LET a = 11 RETURN a - 1",
          "LET a = 11 RETURN a * 2",
          "LET a = 14 RETURN a / 2",
          "LET a = 14 RETURN a % 3",
          "LET a = 14 RETURN a == 8",
          "LET a = 14 RETURN a == 8 && a != 7 ",
          "LET a = [2, 3, 4, 5, 6, 7] RETURN SLICE(a, 4, 1)",
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
          "LET a = 7 LET b = 2 RETURN [ a, b ]",
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
// here we test the calculation.

          "LET a = 1 RETURN 1",

          "LET b = 22 LET a = b + 1 RETURN 1",
          "LET b = 22 LET a = b == 8 RETURN 1",
          "LET b = 22 LET a = b == 8 && b != 7 RETURN 1",

          "LET b = SQRT(22) LET a = b == 8 && b != 7 RETURN a",
          "LET c = 1 LET b = c + 1  RETURN c", // todo ergebnis pruefen b+c weg
          "LET c = 1 LET b = c + 1  RETURN b", // todo ergebnis pruefen 
         // "LET b = RAND() LET a = b == 8 && b != 7 RETURN a",


          "LET b = 22 LET a = [1, 2, 3, 4, 5, 6, b] RETURN SLICE(a, 4, 1)",
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


          // String functions:
          "LET b = 'snth' LET a = CONCAT('aoeu', b) RETURN CONCAT(a, 'htns')",
          "LET a = 'aoeu' RETURN CONCAT(a, 'htns')",
          "LET a = '' return CONCAT(a, 'htns')"


          //"FOR i IN 1..10 LET a = 1 FILTER i == a RETURN i"
//          "FOR i IN 1..10 LET a = i + 1 FILTER i != a RETURN i"
      ];

      queries.forEach(function(query) {
          var result = AQL_EXPLAIN(query, { }, paramEnabled);
          assertEqual([ ruleName ], result.plan.rules, query);
          result = AQL_EXPLAIN(query, { }, paramNone);
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
