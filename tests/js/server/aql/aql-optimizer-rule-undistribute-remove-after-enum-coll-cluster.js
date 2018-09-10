/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, AQL_EXPLAIN */

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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "undistribute-remove-after-enum-coll";
  // various choices to control the optimizer: 
  var rulesNone        = { optimizer: { rules: [ "-all" ] } };
  var rulesAll         = { optimizer: { rules: [ "+all" ] } };
  var thisRuleEnabled  = { optimizer: { rules: [ "-all", "+distribute-filtercalc-to-cluster", "+" + ruleName ] } };

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
    /// @brief test that rule fires when it is enabled 
    ////////////////////////////////////////////////////////////////////////////////

    testThisRuleEnabled : function () {
      var queries = [ 
        [ "FOR d IN " + cn1 + " FILTER d.Hallo < 5 REMOVE d in " + cn1, 0],
        [ "FOR d IN " + cn1 + " FILTER d.Hallo < 5 REMOVE d._key in " + cn1, 1]
      ];

      var expectedRules = [ "distribute-in-cluster",
                            "scatter-in-cluster", 
                            "distribute-filtercalc-to-cluster", 
                            "undistribute-remove-after-enum-coll"];

      var expectedNodes = [ ["SingletonNode", 
                             "ScatterNode", 
                             "RemoteNode", 
                             "EnumerateCollectionNode", 
                             "CalculationNode", 
                             "FilterNode", 
                             "RemoveNode", 
                             "RemoteNode", 
                             "GatherNode"],
                            ["SingletonNode", 
                             "ScatterNode", 
                             "RemoteNode", 
                             "EnumerateCollectionNode", 
                             "CalculationNode", 
                             "FilterNode", 
                             "CalculationNode", 
                             "RemoveNode", 
                             "RemoteNode", 
                             "GatherNode"]];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0], { }, thisRuleEnabled);
        assertEqual(expectedRules, result.plan.rules, query);
        assertEqual(expectedNodes[query[1]], explain(result), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule does not fire when it is not enabled 
    ////////////////////////////////////////////////////////////////////////////////
    
    testNoRules : function () {
      var queries = [ 
         ["FOR d IN " + cn1 + " FILTER d.Hallo < 5 REMOVE d in " + cn1, 0], 
         ["FOR d IN " + cn1 + " FILTER d.Hallo < 5 REMOVE d._key in " + cn1, 1]
      ];

      var expectedRules = [ "distribute-in-cluster",
                            "scatter-in-cluster" ];

      var expectedNodes = [ ["SingletonNode", 
                             "ScatterNode", 
                             "RemoteNode", 
                             "EnumerateCollectionNode", 
                             "RemoteNode", 
                             "GatherNode",
                             "CalculationNode", 
                             "FilterNode",
                             "DistributeNode", 
                             "RemoteNode", 
                             "RemoveNode",
                             "RemoteNode", 
                             "GatherNode"
                            ],
                            [ "SingletonNode", 
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode", 
                              "CalculationNode", 
                              "FilterNode", 
                              "CalculationNode", 
                              "DistributeNode", 
                              "RemoteNode", 
                              "RemoveNode",
                              "RemoteNode", 
                              "GatherNode"
                            ] ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0], { }, rulesNone);
        assertEqual(expectedRules, result.plan.rules, query[0]);
        assertEqual(expectedNodes[query[1]], explain(result), query[0]);
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
        "FOR d IN " + cn1 + " FILTER d.Hallo < 5 FILTER d.Hallo > 1 REMOVE d in " + cn1,
        "FOR d IN " + cn1 + " FILTER d.Hallo < 5 FILTER d.Hallo > 1 REMOVE d._key in " 
          + cn1,
        "FOR d IN " + cn1 + " FILTER d.Hallo < 5 FILTER d.Hallo > 1 REMOVE d.blah in " 
          + cn1,
        "FOR d IN " + cn2 + " FILTER d.Hallo < 5 FILTER d.Hallo > 1 REMOVE d in " + cn2,
        "FOR d IN " + cn2 + " FILTER d.Hallo < 5 FILTER d.Hallo > 1 REMOVE d._key in " 
          + cn2,
        "FOR d IN " + cn2 + " FILTER d.Hallo < 5 FILTER d.Hallo > 1 REMOVE d.blah in " 
          + cn2];

      queries.forEach(function(query) {
        var result1 = AQL_EXPLAIN(query, { }, thisRuleEnabled);
        var result2 = AQL_EXPLAIN(query, { }, rulesAll);
        assertTrue(result1.plan.rules.indexOf(ruleName) !== -1, query);
        assertTrue(result2.plan.rules.indexOf(ruleName) !== -1, query);
      });
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

