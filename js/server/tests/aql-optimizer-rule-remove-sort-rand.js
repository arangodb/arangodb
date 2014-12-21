/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual, assertTrue, assertFalse, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

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
var helper = require("org/arangodb/aql-helper");
var db = require("org/arangodb").db;
var removeAlwaysOnClusterRules = helper.removeAlwaysOnClusterRules;
var removeClusterNodes = helper.removeClusterNodes;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "remove-sort-rand";
  // various choices to control the optimizer: 
  var paramNone     = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName, "+remove-unnecessary-calculations-2" ] } };
  var c;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 1000; ++i) {
        c.save({ value: i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      var queries = [ 
        "FOR i IN " + c.name() + " SORT RAND() RETURN i",
        "FOR i IN " + c.name() + " SORT RAND() LIMIT 1 RETURN i"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramNone);
        assertEqual([ ], removeAlwaysOnClusterRules(result.plan.rules));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      c.ensureSkiplist("value");

      var queries = [ 
        "FOR i IN 1..10 SORT RAND() RETURN i",  // no collection
        "FOR i IN " + c.name() + " SORT i.value, RAND() RETURN i", // more than one sort criterion
        "FOR i IN " + c.name() + " SORT RAND(), i.value RETURN i", // more than one sort criterion
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " SORT RAND() RETURN i", // more than one collection
        "FOR i IN " + c.name() + " LET a = FAIL(1) SORT RAND() RETURN [ i, a ]", // may throw
        "FOR i IN " + c.name() + " FILTER i.value > 10 SORT RAND() RETURN i", // uses an IndexRangeNode
        "FOR i IN " + c.name() + " FILTER i.what == 2 SORT RAND() RETURN i", // contains FilterNode
        "FOR i IN " + c.name() + " COLLECT v = i.value SORT RAND() RETURN v", // contains CollectNode
        "FOR i IN " + c.name() + " LET x = (FOR j IN 1..1 RETURN i.value) SORT RAND() RETURN x" // contains SubqueryNode
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      c.ensureSkiplist("value");

      var queries = [ 
        "FOR i IN " + c.name() + " SORT RAND() RETURN i", 
        "FOR i IN " + c.name() + " SORT RAND() ASC RETURN i", 
        "FOR i IN " + c.name() + " SORT RAND() DESC RETURN i", 
        "FOR i IN " + c.name() + " SORT RAND() LIMIT 2 RETURN i", 
        "FOR i IN " + c.name() + " LIMIT 1 SORT RAND() RETURN i", 
        "FOR i IN " + c.name() + " LET x = i.value + 1 SORT RAND() RETURN x", 
        "FOR i IN " + c.name() + " SORT RAND() FILTER i.value > 10 RETURN i", // does not use an IndexRangeNode
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        var collectionNode = result.plan.nodes.map(function(node) { return node.type; }).indexOf("EnumerateCollectionNode");
        assertEqual("EnumerateCollectionNode", result.plan.nodes[collectionNode].type);
        assertTrue(result.plan.nodes[collectionNode].random); // check for random iteration flag
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////

    testPlans : function () {
      var plans = [ 
        [ "FOR i IN " + c.name() + " SORT RAND() RETURN i", [ "SingletonNode", "EnumerateCollectionNode", "ReturnNode" ] ],
        [ "FOR i IN " + c.name() + " LET x = i.value + 1 SORT RAND() RETURN x", [ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "ReturnNode" ] ],
        [ "FOR i IN " + c.name() + " SORT RAND() LIMIT 1 RETURN i", [ "SingletonNode", "EnumerateCollectionNode", "LimitNode", "ReturnNode" ] ],
        [ "FOR i IN " + c.name() + " LIMIT 1 SORT RAND() RETURN i", [ "SingletonNode", "EnumerateCollectionNode", "LimitNode", "ReturnNode" ] ]
      ];

      plans.forEach(function(plan) {
        var result = AQL_EXPLAIN(plan[0], { }, paramEnabled);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), plan[0]);
        assertEqual(plan[1], removeClusterNodes(helper.getCompactPlan(result).map(function(node) { return node.type; })), plan[0]);
        var collectionNode = result.plan.nodes.map(function(node) { return node.type; }).indexOf("EnumerateCollectionNode");
        assertEqual("EnumerateCollectionNode", result.plan.nodes[collectionNode].type);
        assertTrue(result.plan.nodes[collectionNode].random); // check for random iteration flag
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      var result = AQL_EXECUTE("FOR i IN 1..5 LET values = (FOR j IN " + c.name() + " SORT RAND() RETURN j) RETURN values").json;

      assertEqual(5, result.length);

      // assert all random results are unique
      var hasSeen = { };
      for (var i = 0; i < result.length; ++i) {
        assertEqual(1000, result[i].length);
        var key = JSON.stringify(result[i]);
        assertFalse(hasSeen.hasOwnProperty(key));
        hasSeen[key] = 1;
      }
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
