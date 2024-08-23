/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
const db = require('internal').db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "remove-unnecessary-filters";
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
        "FOR i IN 1..10 FILTER true RETURN 1",
        "FOR i IN 1..10 FILTER 1 != 7 RETURN 1",
        "FOR i IN 1..10 FILTER 1 == 1 && 2 == 2 RETURN 1",
        "FOR i IN 1..10 FILTER 1 != 1 && 2 != 2 RETURN 1"
      ];

      queries.forEach(function(query) {
        var result = db._createStatement({query: query, bindVars:  { }, options:  paramNone}).explain();
        assertEqual([ ], result.plan.rules);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queries = [ 
        "FOR i IN 1..10 FILTER i > 1 RETURN i",
        "FOR i IN 1..10 LET a = 99 FILTER i > a RETURN i",
        "FOR i IN 1..10 LET a = i FILTER a != 99 RETURN i"
      ];

      queries.forEach(function(query) {
        var result = db._createStatement({query: query, bindVars:  { }, options:  paramEnabled}).explain();
        assertEqual([ ], result.plan.rules, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var queries = [ 
        [ "FOR i IN 1..2 FILTER true RETURN i", true ],
        [ "FOR i IN 1..2 FILTER 1 > 9 RETURN i", false ],
        [ "FOR i IN 1..2 FILTER 1 < 9 RETURN i", true ],
        [ "FOR i IN 1..2 LET a = 1 FILTER a == 1 RETURN i", true ],
        [ "FOR i IN 1..2 LET a = 1 LET b = 1 FILTER a == b RETURN i", true ],
        [ "FOR i IN 1..2 LET a = 1 LET b = 1 FILTER a != b RETURN i", false ],
        [ "FOR i IN 1..2 LET a = 1 LET b = 2 FILTER a != b RETURN i", true ],
        [ "FOR i IN 1..2 LET a = 1 LET b = 2 FILTER a == b RETURN i", false ],
        [ "FOR i IN 1..2 FILTER false RETURN i", false ],
        [ "FOR i IN 1..2 LET a = 1 FILTER a == 9 RETURN i", false ],
        [ "FOR i IN 1..2 LET a = 1 FILTER a != 1 RETURN i", false ],
        [ "FOR i IN 1..2 FILTER 1 == 1 && 2 == 2 RETURN i", true ],
        [ "FOR i IN 1..2 FILTER 1 != 1 && 2 != 2 RETURN i", false ],
      ];

      queries.forEach(function(query) {
        var result = db._createStatement({query: query[0], bindVars:  { }, options:  paramEnabled}).explain();
        assertEqual([ ], result.plan.rules, query);
        result = db._query(query[0], { }, paramEnabled).toArray();
        if (query[1]) {
          assertEqual([ 1, 2 ], result, query);
        } else {
          assertEqual([ ], result, query);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////


    testPlans : function () {
      var plans = [ 
        [ "FOR i IN 1..10 FILTER true RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 FILTER 1 < 9 RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a == 1 RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET a = 1 LET b = 1 FILTER a == b RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET a = 1 LET b = 2 FILTER a != b RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 FILTER false RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "NoResultsNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a == 9 RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "NoResultsNode", "ReturnNode" ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a != 1 RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "NoResultsNode", "ReturnNode" ] ]
      ];

      plans.forEach(function(plan) {
        var result = db._createStatement({query: plan[0], bindVars:  { }, options:  paramMore}).explain();
        // rule will not fire anymore for constant filters
        assertFalse(result.plan.rules.indexOf(ruleName) !== -1, plan[0]);
        assertEqual(plan[1], helper.getCompactPlan(result).map(function(node) { return node.type; }), plan[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      var queries = [ 
        [ "FOR i IN 1..10 FILTER true RETURN i", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a == 1 RETURN i", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a != 99 RETURN i", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 LET a = 1 LET b = 1 FILTER a == b RETURN i", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ "FOR i IN 1..10 FILTER false RETURN i", [ ] ],
        [ "FOR i IN 1..10 FILTER 1 == 7 RETURN i", [ ] ],
        [ "LET a = 1 FOR i IN 1..10 FILTER a == 7 && a == 3 RETURN i", [ ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a == 7 RETURN i", [ ] ],
        [ "FOR i IN 1..10 LET a = 1 FILTER a != 1 RETURN i", [ ] ]
      ];

      queries.forEach(function(query) {
        var planDisabled   = db._createStatement({query: query[0], bindVars:  { }, options:  paramDisabled}).explain();
        var planEnabled    = db._createStatement({query: query[0], bindVars:  { }, options:  paramEnabled}).explain();
        var resultDisabled = db._query(query[0], { }, paramDisabled).toArray();
        var resultEnabled  = db._query(query[0], { }, paramEnabled).toArray();

        assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);

        assertTrue(planDisabled.plan.rules.indexOf(ruleName) === -1, query[0]);
        // rule will not fire anymore for constant filters
        assertTrue(planEnabled.plan.rules.indexOf(ruleName) === -1, query[0]);

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

