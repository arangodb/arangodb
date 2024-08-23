/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertNotEqual */

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
        let result = db._createStatement({query: query, bindVars:  { }, options:  paramNone}).explain();
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
        "FOR i IN 1..10 LET x = (FOR j IN [i] RETURN j) FILTER i > 1 RETURN i",
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
        var result = db._createStatement({query: query, bindVars:  { }, options:  paramEnabled}).explain();
        assertTrue(result.plan.rules.indexOf(ruleName) === -1, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      const queries = [ 
        "FOR i IN 1..10 FOR j IN 1..10 FILTER i > 1 RETURN i",
        "FOR i IN 1..10 FOR l IN 1..10 LET a = 2 * i FILTER i == 1 RETURN a",
        "FOR i IN 1..10 FOR l IN 1..10 LET a = 2 * i FILTER i == 1 LIMIT 1 RETURN a",
        "FOR i IN 1..10 FOR l IN 1..10 LET a = MIN([1, l]) FILTER i == 1 LIMIT 1 RETURN a",
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query, bindVars:  { }, options:  paramEnabled}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////


    testPlans : function () {
      const plans = [ 
        [ "FOR i IN 1..10 FOR j IN 1..10 FILTER i > 1 RETURN i", [ "SingletonNode", "CalculationNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "EnumerateListNode", "ReturnNode" ], true ],
        [ "FOR i IN 1..10 LET x = (FOR j IN [i] RETURN j) FILTER i > 1 RETURN i", [ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "SubqueryStartNode", "EnumerateListNode", "SubqueryEndNode", "CalculationNode", "FilterNode", "ReturnNode" ], false ],
        [ "FOR i IN 1..10 FOR l IN 1..10 LET a = 2 * i FILTER i == 1 RETURN a", [ "SingletonNode", "CalculationNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "CalculationNode", "FilterNode", "EnumerateListNode", "ReturnNode" ], true ],
        [ "FOR i IN 1..10 FOR j IN 1..10 FILTER i == 1 FILTER j == 2 RETURN i", [ "SingletonNode", "CalculationNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "EnumerateListNode", "CalculationNode", "FilterNode", "ReturnNode" ], true ]
      ];

      plans.forEach(function(plan) {
        let result = db._createStatement({query: plan[0], bindVars:  { }, options:  paramEnabled}).explain();
        if (!plan[2]) {
          assertEqual(-1, result.plan.rules.indexOf(ruleName), plan[0]);
        } else {
         // because of the optimization rules for moving subqueries up,
         // there's a dependency of the subquery on the variable i of the outer query,
         // filter is not going to be moved up
          assertNotEqual(-1, result.plan.rules.indexOf(ruleName), plan[0]);
        }
        assertEqual(plan[1], helper.getCompactPlan(result).map(function(node) { return node.type; }), plan[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      const queries = [ 
        [ "FOR i IN 1..10 FOR j IN 1..10 FILTER i > 9 RETURN j", [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], true ],
        [ "FOR i IN 1..10 LET x = (FOR j IN [i] RETURN j) FILTER i > 9 RETURN x", [ [ 10 ] ], false ],
        [ "FOR i IN 1..10 FOR l IN 1..1 LET a = 2 * i FILTER i == 1 RETURN a", [ 2 ], true ],
        [ "FOR i IN 1..10 LET a = i FOR j IN 1..10 LET b = j FILTER a == 1 FILTER b == 2 RETURN [ a, b ]", [ [ 1, 2 ] ], true ]
      ];

      queries.forEach(function(query) {
        var planDisabled   = db._createStatement({query: query[0], bindVars:  { }, options:  paramDisabled}).explain();
        var planEnabled    = db._createStatement({query: query[0], bindVars:  { }, options:  paramEnabled}).explain();
        var resultDisabled = db._query(query[0], { }, paramDisabled).toArray();
        var resultEnabled  = db._query(query[0], { }, paramEnabled).toArray();

        assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);

        assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query[0]);
        if (!query[2]) {
          assertEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query[0]);
        } else {
          // because of the optimization rules for moving subqueries up,
          // there's a dependency of the subquery on the variable i of the outer query,
          // filter is not going to be moved up
          assertNotEqual(-1, planEnabled.plan.rules.indexOf(ruleName), query[0]);
        }
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

