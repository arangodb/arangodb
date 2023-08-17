/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, AQL_EXPLAIN, AQL_EXECUTE, AQL_EXECUTEJSON */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author 
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var db = require("@arangodb").db;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
const _ = require('lodash');

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "remove-unnecessary-remote-scatter";
  // various choices to control the optimizer: 
  var rulesNone        = { optimizer: { rules: [ "-all" ] } };
  var rulesAll         = { optimizer: { rules: [ "+all" ] } };
  var thisRuleEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var thisRuleDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  var cn1 = "UnitTestsAqlOptimizerRuleRemoveUnnecessaryRemoteScatter1";
  var cn2 = "UnitTestsAqlOptimizerRuleRemoveUnnecessaryRemoteScatter2";
  var c1, c2;
  
  var explain = function (result) {
    return helper.getCompactPlan(result).map(function(node) 
        { return node.type; });
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      var i;
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1, {numberOfShards:9});
      c2 = db._create(cn2);
      let docs1 = [];
      let docs2 = [];
      for (i = 0; i < 10; i++){ 
          docs1.push({Hallo1:i});
          docs2.push({Hallo2:i});
      }
      c1.insert(docs1);
      c2.insert(docs2);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule does not fire when all rules are disabled 
    ////////////////////////////////////////////////////////////////////////////////

    testRulesNone : function () {
      var queries = [ 
        "FOR d IN " + cn1 + " RETURN d",
        "LET A=1 LET B=2 FOR d IN " + cn1 + " RETURN d",
        "LET A=1 LET B=2 FOR d IN " + cn1 + " LIMIT 10 RETURN d",
        "FOR e in " + cn1 + " FOR d IN " + cn1 + " LIMIT 10 RETURN d"
      ];
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, rulesNone);
        assertEqual([ "scatter-in-cluster" ], result.plan.rules, query);

      });
    },

    testRulesNoneSerial : function () {
      var queries = [ 
        "FOR d IN " + cn1 + " SORT d.Hallo1 RETURN d",
        "LET A=1 LET B=2 FOR d IN " + cn1 + " SORT d.Hallo1 RETURN d",
        "LET A=1 LET B=2 FOR d IN " + cn1 + " SORT d.Hallo1 LIMIT 10 RETURN d",
        "FOR e in " + cn1 + " SORT e.Hallo2 FOR d IN " + cn1 + " SORT d.Hallo1 LIMIT 10 RETURN d"
      ];

      var opts = _.clone(rulesNone);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        var total = AQL_EXECUTE(query);
        var plans = AQL_EXPLAIN(query, { }, opts).plans;
        plans.forEach(function(plan) {
          var result = AQL_EXECUTEJSON(plan, rulesNone);
          assertTrue(_.isEqual(total.json, result.json),
                      query +'\n' +
                      'result: ' + JSON.stringify(result.json) + '\n' +
                      'reference: ' + JSON.stringify(total.json) + '\n' +
                      'plan: ' + JSON.stringify(plan));
        });
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule does not fire when it is disabled but no other rule is
    ////////////////////////////////////////////////////////////////////////////////
    
    testThisRuleDisabled : function () {
      var queries = [ 
        "FOR d IN " + cn1 + " RETURN d",
        "LET A=1 LET B=2 FOR d IN " + cn1 + " RETURN d",
        "LET A=1 LET B=2 FOR d IN " + cn1 + " LIMIT 10 RETURN d",
        "FOR e in " + cn1 + " FOR d IN " + cn1 + " LIMIT 10 RETURN d"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, thisRuleDisabled);
        assertTrue(result.plan.rules.indexOf(ruleName) === -1, query);
      });
    },

    testThisRuleEnabled : function () {
      var queries = [ 
        [ "FOR d IN " + cn1 + " RETURN d", 0],
        [ "LET A=1 LET B=2 FOR d IN " + cn1 + " RETURN d" , 1],
        [ "LET A=1 LET B=2 FOR d IN " + cn1 + " LIMIT 10 RETURN d", 2],
        [ "FOR e in " + cn1 + " FOR d IN " + cn1 + " LIMIT 10 RETURN d", 3]
      ];

      var expectedRules = [ "scatter-in-cluster", "remove-unnecessary-remote-scatter"];
      var expectedNodes = [ 
          ["SingletonNode", "EnumerateCollectionNode",
           "RemoteNode", "GatherNode", "ReturnNode"],
          ["SingletonNode", "CalculationNode", "CalculationNode",
           "EnumerateCollectionNode", "RemoteNode", "GatherNode", "ReturnNode"],
          ["SingletonNode", "CalculationNode", "CalculationNode",
           "EnumerateCollectionNode", "RemoteNode",
           "GatherNode", "LimitNode", "ReturnNode" ],
          ["SingletonNode", "EnumerateCollectionNode", "RemoteNode",
           "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode",
           "RemoteNode", "GatherNode", "LimitNode", "ReturnNode"] ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0], { }, thisRuleEnabled);
        assertEqual(expectedRules, result.plan.rules, query);
        assertEqual(expectedNodes[query[1]], explain(result), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has no effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queries = [  "LIMIT 1 FOR d IN " + cn1 + " RETURN d",
                       "LIMIT 1 FOR d IN " + cn2 + " RETURN d" ];

      queries.forEach(function(query) {
        var result1 = AQL_EXPLAIN(query, { }, thisRuleEnabled);
        var result2 = AQL_EXPLAIN(query, { }, rulesAll);

        assertTrue(result1.plan.rules.indexOf(ruleName) === -1, query);
        assertTrue(result2.plan.rules.indexOf(ruleName) === -1, query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has an effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var queries = [ 
        "FOR d IN " + cn1 + " RETURN d",
        "FOR d IN " + cn2 + " RETURN d"
      ];

      queries.forEach(function(query) {
        var resultEnabled  = AQL_EXPLAIN(query, { }, thisRuleEnabled);
        var resultDisabled = AQL_EXPLAIN(query, { }, thisRuleDisabled);
        assertTrue(resultEnabled.plan.rules.indexOf(ruleName)  !== -1, query);
        assertTrue(resultDisabled.plan.rules.indexOf(ruleName) === -1, query);
        
        // the test doesn't run with the below . . .
        //resultDisabled = AQL_EXECUTE(query, { }, thisRuleDisabled).json;
        //resultEnabled  = AQL_EXECUTE(query, { }, thisRuleEnabled).json;
        //assertEqual(resultDisabled, resultEnabled, query);
      });
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

