/*global require, assertTrue, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */
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

var db = require("org/arangodb").db;
var jsunity = require("jsunity");
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults2;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "distribute-in-cluster";
  // various choices to control the optimizer: 
  var rulesNone        = { optimizer: { rules: [ "-all" ] } };
  var rulesAll         = { optimizer: { rules: [ "+all" ] } };
  var thisRuleEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var thisRuleDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  var cn1 = "UnitTestsAqlOptimizerRuleUndist1";
  var cn2 = "UnitTestsAqlOptimizerRuleUndist2";
  var c1, c2;
  
  var explain = function (result) {
    return helper.getCompactPlan(result).map(function(node) 
        { return node.type; });
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var i;
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1, {numberOfShards:9});
      c2 = db._create(cn2);
      for (i = 0; i < 10; i++){ 
          c1.insert({Hallo1:i});
          c2.insert({Hallo2:i});
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule does not fire when it is not enabled 
    ////////////////////////////////////////////////////////////////////////////////

    testThisRuleEnabled : function () {
      var queries = [ 
        [ "FOR d IN " + cn1 + " REMOVE d in " + cn1, 0],
        [ "FOR d IN " + cn1 + " REMOVE d._key in " + cn1, 1],
        [ "FOR d IN " + cn1 + " INSERT d in " + cn2, 2],
        [ "FOR d IN " + cn1 + " INSERT d._key in " + cn2, 3],
        [ "FOR d IN " + cn1 + " REPLACE d in " + cn1, 4],
        [ "FOR d IN " + cn1 + " REPLACE d._key in " + cn1, 5],
        [ "FOR d IN " + cn1 + " UPDATE d in " + cn1 , 6],
        [ "FOR d IN " + cn1 + " UPDATE d._key in " + cn1 , 7]
      ];

      var expectedRules = [
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster", 
                              "remove-unnecessary-remote-scatter" 
                            ], 
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster", 
                              "distribute-filtercalc-to-cluster", 
                              "remove-unnecessary-remote-scatter" 
                            ]
                          ];

      var expectedNodes = [ 
                            [
                              "SingletonNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "DistributeNode",
                              "RemoteNode"
                            ],
                            [
                              "SingletonNode",
                              "EnumerateCollectionNode", 
                              "CalculationNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "DistributeNode",
                              "RemoteNode"
                            ]
                          ];

      var finalNodes = [ 
                        "RemoveNode", "RemoveNode", 
                        "InsertNode", "InsertNode",
                        "ReplaceNode", "ReplaceNode", 
                        "UpdateNode", "UpdateNode"
                       ];

      queries.forEach(function(query) {
        var i = query[1] % 2;
        var result = AQL_EXPLAIN(query[0], { }, rulesAll);
        assertEqual(expectedRules[i], result.plan.rules, query);
        expectedNodes[i].push(finalNodes[query[1]]);
        assertEqual(expectedNodes[i], explain(result), query);
        expectedNodes[i].pop();
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has no effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
       var queries = [ 
         "FOR d IN " + cn1 +  " RETURN d",
         "FOR i IN 1..10 RETURN i" ];

      queries.forEach(function(query) {
        var result1 = AQL_EXPLAIN(query, { }, thisRuleEnabled);
        var result2 = AQL_EXPLAIN(query, { }, rulesAll);

        assertTrue(result1.plan.rules.indexOf(ruleName) === -1, query);
        assertTrue(result2.plan.rules.indexOf(ruleName) === -1, query);
      });
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

