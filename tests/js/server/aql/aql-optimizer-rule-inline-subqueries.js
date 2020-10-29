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
var db = require("@arangodb").db;
var ruleName = "inline-subqueries";

function optimizerRuleTestSuite () {
  // various choices to control the optimizer: 
  var paramNone = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled = { optimizer: { rules: [ "-all", "+" + ruleName ] }, inspectSimplePlans: true };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  return {

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
        "FOR i IN (FOR j IN [1,2,3] COLLECT v = j INTO g RETURN [v, g]) RETURN i",
        "FOR i IN [1,2,3] LET sub = (FOR j IN [1,2,3] COLLECT v = j INTO g RETURN [v, g]) FOR x IN sub RETURN [i, x]",
        "FOR i IN [1,2,3] LET x = (FOR j IN [1,2,3] LIMIT 1 RETURN j) FOR k IN x RETURN k",
        "FOR i IN [1,2,3] LET x = (FOR j IN [1,2,3] SORT j RETURN j) FOR k IN x RETURN k",
        "FOR i IN [3,2,1] SORT i LET sub = (FOR j IN [1,2,3] SORT j DESC RETURN j) FOR k IN sub RETURN [i, k]",
        "FOR i IN [1,2,3] LET sub = (FOR j IN [1,1,1] RETURN DISTINCT j) FOR k IN sub RETURN [i, k]",
        "FOR i IN [1,2,3] LET sub = (FOR j IN [1,1,1] COLLECT x = j RETURN x) FOR k IN sub RETURN [i, k]",
        "FOR i IN [1,2,3] LET sub = (FOR j IN [1,1,1] COLLECT x = j OPTIONS { method: 'hash' } RETURN x) FOR k IN sub RETURN [i, k]",
        "FOR i IN [1,2,3] LET sub = (FOR j IN [1,1,1] COLLECT x = j OPTIONS { method: 'sorted' } RETURN x) FOR k IN sub RETURN [i, k]",
        "FOR i IN [1,2,3] LET sub = (FOR j IN [1,1,2,3,3] RETURN DISTINCT j) FOR x IN sub RETURN [i, x]",
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual([ ], result.plan.rules, query);
        result = AQL_EXPLAIN(query, { }, paramNone);
        assertEqual([ ], result.plan.rules, query);
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
        "FOR i IN (FOR j IN [1,2,3] FILTER j > 1 RETURN j * 2) FILTER i > 1 RETURN i * 3",
        "LET x = (FOR j IN [1,2,3] LIMIT 2 RETURN j) FOR k IN x RETURN k",
        "LET x = (FOR j IN [1,2,3] SORT j RETURN j) FOR k IN x RETURN k"
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
        [ "FOR i IN (FOR j IN (FOR k IN [1,2,3,4] SORT k DESC LIMIT 3 RETURN k) LIMIT 2 RETURN j) RETURN i", [ 4, 3 ] ],
        [ "LET x = (FOR j IN [1,2,3] LIMIT 2 RETURN j) FOR k IN x RETURN k", [ 1, 2 ] ],
        [ "LET x = (FOR j IN [1,2,3] LIMIT 1, 2 RETURN j) FOR k IN x RETURN k", [ 2, 3 ] ],
        [ "LET x = (FOR j IN [1,2,3,4] LIMIT 2 RETURN j) FOR k IN x LIMIT 1, 1 RETURN k", [ 2 ] ],
        [ "LET x = (FOR j IN [1,2,3,4] LIMIT 1, 2 RETURN j) FOR k IN x LIMIT 1, 1 RETURN k", [ 3 ] ],
        [ "LET x = (FOR j IN [1,2,3,4] RETURN j) FOR k IN x LIMIT 1, 1 RETURN k", [ 2 ] ],
        [ "LET x = (FOR j IN [1,2,3,4] SORT j DESC RETURN j) FOR k IN x RETURN k", [ 4, 3, 2, 1 ] ],
        [ "LET x = (FOR j IN [1,2,3,4] SORT j DESC LIMIT 2 RETURN j) FOR k IN x RETURN k", [ 4, 3 ] ],
        [ "LET x = (FOR j IN [1,2,3,4] SORT j DESC LIMIT 2 RETURN j) FOR k IN x LIMIT 1 RETURN k", [ 4 ] ],
        
        [ "FOR i IN [3,2,1] SORT i LET sub = (FOR j IN [1,2,3] RETURN j) FOR k IN sub RETURN [i, k]", [ [1, 1], [1, 2], [1, 3], [2, 1], [2, 2], [2, 3], [3, 1], [3, 2], [3, 3] ] ],
        [ "FOR i IN [3,2,1] SORT i DESC LET sub = (FOR j IN [1,2,3] RETURN j) FOR k IN sub RETURN [i, k]", [ [3, 1], [3, 2], [3, 3], [2, 1], [2, 2], [2, 3], [1, 1], [1, 2], [1, 3] ] ],
      ];
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);

        result = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], result, query);
      });
    }

  };
}

function optimizerRuleCollectionTestSuite () {
  var c = null;
  var cn = "UnitTestsOptimizer";
  const noMoveFilters = { optimizer: { rules: [ "-move-filters-into-enumerate" ] } };

  return {

    setUpAll : function () {
      db._drop(cn);
      c = db._create(cn);
    },

    tearDownAll : function () {
      db._drop(cn);
      c = null;
    },

    testSpecificPlan1 : function () {
      var query = "LET x = (FOR doc IN @@cn RETURN doc) FOR doc2 IN x RETURN doc2";

      var result = AQL_EXPLAIN(query, { "@cn" : cn });
      assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      var nodes = helper.removeClusterNodesFromPlan(result.plan.nodes);
      assertEqual(3, nodes.length);
      assertEqual("ReturnNode", nodes[nodes.length - 1].type);
      assertEqual("doc2", nodes[nodes.length - 1].inVariable.name);
      assertEqual("EnumerateCollectionNode", nodes[nodes.length - 2].type);
      assertEqual(cn, nodes[nodes.length - 2].collection);
    },

    testSpecificPlan2 : function () {
      var query = "LET x = (FOR doc IN @@cn FILTER doc.foo == 'bar' RETURN doc) FOR doc2 IN x RETURN doc2";

      var result = AQL_EXPLAIN(query, { "@cn" : cn }, noMoveFilters);
      assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      var nodes = helper.removeClusterNodesFromPlan(result.plan.nodes);
      assertEqual(5, nodes.length);
      assertEqual("ReturnNode", nodes[nodes.length - 1].type);
      assertEqual("doc2", nodes[nodes.length - 1].inVariable.name);
      assertEqual("FilterNode", nodes[nodes.length - 2].type);
      assertEqual("CalculationNode", nodes[nodes.length - 3].type);
      assertEqual("EnumerateCollectionNode", nodes[nodes.length - 4].type);
      assertEqual(cn, nodes[nodes.length - 4].collection);
    },

    testSpecificPlan3 : function () {
      var query = "LET x = (FOR doc IN @@cn RETURN doc) FOR doc2 IN x RETURN x";
      var result = AQL_EXPLAIN(query, { "@cn" : cn });
      assertEqual(-1, result.plan.rules.indexOf(ruleName), query); // no optimization
    },

    testSpecificPlan4 : function () {
      var query = "LET x = (FOR doc IN @@cn RETURN doc) FOR i IN 1..10 FILTER LENGTH(x) FOR y IN x RETURN y";
      var result = AQL_EXPLAIN(query, { "@cn" : cn });
      assertEqual(-1, result.plan.rules.indexOf(ruleName), query); // no optimization
    },

    testSpecificPlan5 : function () {
      var query = "FOR j IN 1..10 LET x = (FOR doc IN @@cn RETURN doc) FOR i IN 1..10 FILTER LENGTH(x) FOR y IN x RETURN y";
      var result = AQL_EXPLAIN(query, { "@cn" : cn });
      assertEqual(-1, result.plan.rules.indexOf(ruleName), query); // no optimization
    }

  };
}

function optimizerRuleViewTestSuite () {
  let cn = "UnitTestsOptimizer";

  return {

    setUpAll : function () {
      db._dropView(cn + "View");
      db._drop(cn);
      db._create(cn);
      db._createView(cn + "View", "arangosearch", { links: { "UnitTestsOptimizer" : { includeAllFields: true } } });
    },

    tearDownAll : function () {
      db._dropView(cn + "View");
      db._drop(cn);
    },

    testVariableReplacementInSearchCondition : function () {
      let query = "LET sub = (RETURN 1) FOR outer IN sub FOR v IN " + cn + "View SEARCH v.something == outer RETURN v";

      let result = AQL_EXPLAIN(query);
      assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      
      let nodes = helper.removeClusterNodesFromPlan(result.plan.nodes);

      assertEqual("ReturnNode", nodes[nodes.length - 1].type);
      assertEqual("EnumerateViewNode", nodes[nodes.length - 2].type);
      
      let viewNode = nodes[nodes.length - 2];
      assertEqual(cn + "View", viewNode.view);
      assertEqual("v", viewNode.outVariable.name);
      assertEqual("n-ary or", viewNode.condition.type);
      assertEqual("n-ary and", viewNode.condition.subNodes[0].type);
      assertEqual("compare ==", viewNode.condition.subNodes[0].subNodes[0].type);
      assertEqual("attribute access", viewNode.condition.subNodes[0].subNodes[0].subNodes[0].type);
      assertEqual("something",        viewNode.condition.subNodes[0].subNodes[0].subNodes[0].name);
      assertEqual("reference",        viewNode.condition.subNodes[0].subNodes[0].subNodes[0].subNodes[0].type);
      assertEqual("v", viewNode.condition.subNodes[0].subNodes[0].subNodes[0].subNodes[0].name);
      assertEqual("reference", viewNode.condition.subNodes[0].subNodes[0].subNodes[1].type);
      assertEqual("outer", viewNode.condition.subNodes[0].subNodes[0].subNodes[1].name);
      assertEqual([], viewNode.scorers);
    },
    
    testNoVariableReplacementInSearchCondition : function () {
      let query = "LET sub = (RETURN 1) FOR outer IN sub FOR v IN " + cn + "View SEARCH v.something == 1 RETURN v";

      let result = AQL_EXPLAIN(query);
      assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      
      let nodes = helper.removeClusterNodesFromPlan(result.plan.nodes);

      assertEqual("ReturnNode", nodes[nodes.length - 1].type);
      assertEqual("EnumerateViewNode", nodes[nodes.length - 2].type);
      
      let viewNode = nodes[nodes.length - 2];
      assertEqual(cn + "View", viewNode.view);
      assertEqual("v", viewNode.outVariable.name);
      assertEqual("n-ary or", viewNode.condition.type);
      assertEqual("n-ary and", viewNode.condition.subNodes[0].type);
      assertEqual("compare ==", viewNode.condition.subNodes[0].subNodes[0].type);
      assertEqual("attribute access", viewNode.condition.subNodes[0].subNodes[0].subNodes[0].type);
      assertEqual("something",        viewNode.condition.subNodes[0].subNodes[0].subNodes[0].name);
      assertEqual("reference",        viewNode.condition.subNodes[0].subNodes[0].subNodes[0].subNodes[0].type);
      assertEqual("v", viewNode.condition.subNodes[0].subNodes[0].subNodes[0].subNodes[0].name);
      assertEqual("value", viewNode.condition.subNodes[0].subNodes[0].subNodes[1].type);
      assertEqual([], viewNode.scorers);
    },

  };
}

jsunity.run(optimizerRuleTestSuite);
jsunity.run(optimizerRuleCollectionTestSuite);
jsunity.run(optimizerRuleViewTestSuite);

return jsunity.done();
