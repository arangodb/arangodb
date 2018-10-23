/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

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
  var ruleName = "distribute-in-cluster";
  // various choices to control the optimizer: 
  var rulesNone        = { optimizer: { rules: [ "-all" ] } };
  var rulesAll         = { optimizer: { rules: [ "+all", "-reduce-extraction-to-projection" ] } };
  var thisRuleEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var thisRuleDisabled = { optimizer: { rules: [ "+all", "-reduce-extraction-to-projection", "-" + ruleName ] } };
  var maxPlans         = { optimizer: { rules: [ "-all" ] }, maxNumberOfPlans: 1 };

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
      c2 = db._create(cn2, {numberOfShards:9, shardKeys:["a","b"]});
      for (i = 0; i < 10; i++) { 
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
    /// @brief test that the rules fire even if the max number of plans is reached
    ////////////////////////////////////////////////////////////////////////////////

    testThisRuleEnabledMaxNumberOfPlans : function () {
      var queries = [ 
        "FOR d IN " + cn1 + " REMOVE d in " + cn1,
        "FOR d IN " + cn1 + " REMOVE d._key in " + cn1,
        "FOR d IN " + cn1 + " INSERT d in " + cn2,
        "FOR d IN " + cn1 + " INSERT d._key in " + cn2
      ];

      var expectedRules = [
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster", 
                            ], 
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster" 
                            ],
                            [
                              "distribute-in-cluster", 
                              "scatter-in-cluster" 
                            ],
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster" 
                            ]
                          ];

      queries.forEach(function(query, i) {
        var result = AQL_EXPLAIN(query, { }, maxPlans);
        assertEqual(expectedRules[i], result.plan.rules, query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that the rule fires when it is enabled
    ////////////////////////////////////////////////////////////////////////////////

    testThisRuleEnabled : function () {
      var queries = [ 
        "FOR d IN " + cn1 + " REMOVE d in " + cn1,
        "FOR d IN " + cn1 + " REMOVE d._key in " + cn1,
        "FOR d IN " + cn1 + " INSERT d in " + cn2,
        "FOR d IN " + cn1 + " INSERT d._key in " + cn2,
        "FOR d IN " + cn1 + " REPLACE d IN " + cn1, 
        "FOR d IN " + cn1 + " REPLACE d._key WITH d IN " + cn1,
        "FOR d IN " + cn1 + " REPLACE d IN " + cn2, 
        "FOR d IN " + cn1 + " REPLACE d._key WITH d IN " + cn2,
        "FOR d IN " + cn1 + " UPDATE d IN " + cn1, 
        "FOR d IN " + cn1 + " UPDATE d._key WITH d IN " + cn1
      ];

      var expectedRules = [
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster", 
                            ], 
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster", 
                            ],
                            [
                              "distribute-in-cluster", 
                              "scatter-in-cluster" 
                            ],
                            [
                              "distribute-in-cluster", 
                              "scatter-in-cluster" 
                            ],
                            [
                              "distribute-in-cluster", 
                              "scatter-in-cluster" 
                            ],
                            [
                              "distribute-in-cluster", 
                              "scatter-in-cluster" 
                            ],
                            [
                              "distribute-in-cluster", 
                              "scatter-in-cluster" 
                            ],
                            [
                              "distribute-in-cluster", 
                              "scatter-in-cluster" 
                            ],
                            [
                              "distribute-in-cluster", 
                              "scatter-in-cluster" 
                            ],
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster", 
                            ]
                          ];

      var expectedNodes = [ 
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "DistributeNode",
                              "RemoteNode",
                              "RemoveNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode",
                              "RemoteNode", 
                              "GatherNode",
                              "CalculationNode", 
                              "DistributeNode",
                              "RemoteNode",
                              "RemoveNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "DistributeNode",
                              "RemoteNode",
                              "InsertNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "CalculationNode", 
                              "DistributeNode",
                              "RemoteNode",
                              "InsertNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "DistributeNode",
                              "RemoteNode",
                              "ReplaceNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "CalculationNode", 
                              "DistributeNode",
                              "RemoteNode",
                              "ReplaceNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "DistributeNode",
                              "RemoteNode",
                              "ReplaceNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "CalculationNode", 
                              "DistributeNode",
                              "RemoteNode",
                              "ReplaceNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "DistributeNode",
                              "RemoteNode",
                              "UpdateNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "CalculationNode", 
                              "DistributeNode",
                              "RemoteNode",
                              "UpdateNode",
                              "RemoteNode",
                              "GatherNode"
                            ]
                          ];

      queries.forEach(function(query, i) {
        var result = AQL_EXPLAIN(query, { }, thisRuleEnabled);
        assertEqual(expectedRules[i], result.plan.rules, query);
        assertEqual(expectedNodes[i], explain(result), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule fires when it is disabled (i.e. it can't be disabled)
    ////////////////////////////////////////////////////////////////////////////////

    testThisRuleDisabled : function () {
      var queries = [ 
        "FOR d IN " + cn1 + " REMOVE d in " + cn1,
        "FOR d IN " + cn1 + " REMOVE d._key in " + cn1,
        "FOR d IN " + cn1 + " INSERT d in " + cn2,
        "FOR d IN " + cn1 + " INSERT d._key in " + cn2,
      ];

      var expectedRules = [
                            [ 
                              "distribute-in-cluster", 
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-remote-scatter",
                              "scatter-in-cluster", 
                              "undistribute-remove-after-enum-coll" 
                            ], 
                            [ 
                              "distribute-in-cluster", 
                              "distribute-filtercalc-to-cluster", 
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-remote-scatter",
                              "scatter-in-cluster", 
                              "undistribute-remove-after-enum-coll" 
                            ],
                            [ 
                              "distribute-in-cluster", 
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-remote-scatter", 
                              "scatter-in-cluster"
                            ], 
                            [ 
                              "distribute-filtercalc-to-cluster", 
                              "distribute-in-cluster", 
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-remote-scatter",
                              "scatter-in-cluster"
                            ]
                          ];

      var expectedNodes = [ 
                            [
                              "SingletonNode", 
                              "EnumerateCollectionNode", 
                              "RemoveNode", 
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "EnumerateCollectionNode", 
                              "CalculationNode", 
                              "RemoveNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [ 
                              "SingletonNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode", 
                              "DistributeNode", 
                              "RemoteNode", 
                              "InsertNode", 
                              "RemoteNode", 
                              "GatherNode" 
                            ],
                            [ 
                              "SingletonNode", 
                              "EnumerateCollectionNode", 
                              "CalculationNode", 
                              "RemoteNode", 
                              "GatherNode", 
                              "DistributeNode", 
                              "RemoteNode", 
                              "InsertNode", 
                              "RemoteNode", 
                              "GatherNode" 
                            ]
                          ];

      queries.forEach(function(query, i) {
        // can't turn this rule off so should always get the same answer
        var result = AQL_EXPLAIN(query, { }, rulesAll);

        assertEqual(expectedRules[i].sort(), result.plan.rules.sort(), query);
        result = AQL_EXPLAIN(query, { }, thisRuleDisabled);
        assertEqual(expectedNodes[i], explain(result), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule does not fire when it is not enabled 
    ////////////////////////////////////////////////////////////////////////////////

    testRulesAll : function () {
      var queries = [ 
        "FOR d IN " + cn1 + " REMOVE d in " + cn1,
        "FOR d IN " + cn1 + " REMOVE d._key in " + cn1,
        "FOR d IN " + cn1 + " INSERT d in " + cn2,
        "FOR d IN " + cn1 + " INSERT d._key in " + cn2,
      ];

      var expectedRules = [
                            [ 
                              "distribute-in-cluster", 
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-remote-scatter",
                              "scatter-in-cluster", 
                              "undistribute-remove-after-enum-coll" 
                            ], 
                            [ 
                              "distribute-filtercalc-to-cluster", 
                              "distribute-in-cluster", 
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-remote-scatter",
                              "scatter-in-cluster", 
                              "undistribute-remove-after-enum-coll" 
                            ],
                            [ 
                              "distribute-in-cluster", 
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-remote-scatter", 
                              "scatter-in-cluster"
                            ], 
                            [ 
                              "distribute-filtercalc-to-cluster", 
                              "distribute-in-cluster", 
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-remote-scatter", 
                              "scatter-in-cluster"
                            ]
                          ];

      var expectedNodes = [ 
                            [
                              "SingletonNode", 
                              "EnumerateCollectionNode", 
                              "RemoveNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [
                              "SingletonNode",
                              "EnumerateCollectionNode", 
                              "CalculationNode", 
                              "RemoveNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [ 
                              "SingletonNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode", 
                              "DistributeNode", 
                              "RemoteNode", 
                              "InsertNode", 
                              "RemoteNode", 
                              "GatherNode" 
                            ],
                            [ 
                              "SingletonNode", 
                              "EnumerateCollectionNode", 
                              "CalculationNode", 
                              "RemoteNode", 
                              "GatherNode", 
                              "DistributeNode", 
                              "RemoteNode", 
                              "InsertNode", 
                              "RemoteNode", 
                              "GatherNode" 
                            ]
                          ];

      queries.forEach(function(query, i) {
        // can't turn this rule off so should always get the same answer
        var result = AQL_EXPLAIN(query, { }, rulesAll);
        assertEqual(expectedRules[i].sort(), result.plan.rules.sort(), query);
        assertEqual(expectedNodes[i], explain(result), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule does not fire when it is not enabled 
    ////////////////////////////////////////////////////////////////////////////////

    testRulesNone : function () {
      var queries = [ 
        "FOR d IN " + cn1 + " REMOVE d in " + cn1,
        "FOR d IN " + cn1 + " REMOVE d._key in " + cn1,
        "FOR d IN " + cn1 + " INSERT d in " + cn2,
        "FOR d IN " + cn1 + " INSERT d._key in " + cn2,
      ];

      var expectedRules = [
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster", 
                            ], 
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster", 
                            ],
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster", 
                            ], 
                            [ 
                              "distribute-in-cluster", 
                              "scatter-in-cluster", 
                            ]
                          ];

      var expectedNodes = [ 
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "DistributeNode",
                              "RemoteNode",
                              "RemoveNode",
                              "RemoteNode",
                              "GatherNode",
                            ],
                            [
                              "SingletonNode",
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode",
                              "CalculationNode", 
                              "DistributeNode",
                              "RemoteNode",
                              "RemoveNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [ 
                              "SingletonNode", 
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode", 
                              "DistributeNode", 
                              "RemoteNode", 
                              "InsertNode", 
                              "RemoteNode", 
                              "GatherNode" 
                            ],
                            [ 
                              "SingletonNode", 
                              "ScatterNode", 
                              "RemoteNode", 
                              "EnumerateCollectionNode", 
                              "RemoteNode", 
                              "GatherNode", 
                              "CalculationNode", 
                              "DistributeNode", 
                              "RemoteNode", 
                              "InsertNode", 
                              "RemoteNode", 
                              "GatherNode" 
                            ]
                          ];

      queries.forEach(function(query, i) {
        // can't turn this rule off so should always get the same answer
        var result = AQL_EXPLAIN(query, { }, rulesNone);
        assertEqual(expectedRules[i], result.plan.rules, query);
        assertEqual(expectedNodes[i], explain(result), query);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test that rule has no effect
    ////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
       var queries = [ 
         "FOR d IN " + cn1 +  " RETURN d",
         "FOR d IN " + cn1 + " UPDATE d IN " + cn2,
         "FOR d IN " + cn1 + " UPDATE d._key WITH d IN " + cn2 ,
         "FOR d IN " + cn2 + " REMOVE d IN " + cn2,
         "FOR i IN 1..10 RETURN i" 
       ];

      queries.forEach(function(query) {
        var result1 = AQL_EXPLAIN(query, { }, thisRuleEnabled);
        var result2 = AQL_EXPLAIN(query, { }, rulesAll);

        assertTrue(result1.plan.rules.indexOf(ruleName) === -1, query);
        assertTrue(result2.plan.rules.indexOf(ruleName) === -1, query);
      });
    },
  };
}

function interactionOtherRulesTestSuite () {
  var distribute = "distribute-in-cluster";           // Rule 1
  var scatter = "scatter-in-cluster";                 // Rule 2
  var undist = "undistribute-remove-after-enum-coll"; // Rule 3

  // various choices to control the optimizer: 
  var allRules         = { optimizer: { rules: [ "+all" ] } };
  var allRulesNoInter  = 
    { optimizer: { rules: [ "+all", "-interchange-adjacent-enumerations" ] } };
  var ruleDisabled   = { optimizer: { rules: [ "+all", "-" + undist ] } };
  var ruleDisabledNoInter  = 
    { optimizer: { rules: [ "+all", 
                            "-interchange-adjacent-enumerations", 
                            "-" + undist ] } };

  var cn1 = "UnitTestsAql1";
  var cn2 = "UnitTestsAql2";
  var cn3 = "UnitTestsAql3";
  var c1, c2, c3;
  
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
      db._drop(cn3);
      c1 = db._create(cn1, {numberOfShards:3});
      c2 = db._create(cn2, {numberOfShards:3});
      c3 = db._create(cn3, {numberOfShards:3, shardKeys:["a","b"]});
      for (i = 0; i < 10; i++) { 
        c1.insert({age:i});
        c2.insert({age:i});
        c3.insert({age:i});
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      c1 = null;
      c2 = null;
      c3 = null;
    },

    ////////////////////////////////////////////////////////////////////////////////
    // uses "distribute-in-cluster" and "undistribute-remove-after-enum-coll"
    ////////////////////////////////////////////////////////////////////////////////
    
    testRule1AndRule2 : function () {
      var queries = [ 
        // collection sharded by _key
        "FOR d IN " + cn1 + " FILTER d.age > 4 REMOVE d._key IN " + cn1,
        // collection sharded by _key
        "FOR d IN " + cn1 + " FILTER d.age > 0 REMOVE d IN " + cn1,
        // collection sharded by _key
        "FOR x IN " + cn1 + " FOR y IN " + cn2 + " REMOVE y IN " + cn2
      ];

      var expectedNodesEnabled = [ 
                                   [
                                     "SingletonNode", 
                                     "EnumerateCollectionNode", 
                                     "CalculationNode", 
                                     "FilterNode", 
                                     "CalculationNode", 
                                     "RemoveNode", 
                                     "RemoteNode", 
                                     "GatherNode"
                                   ],
                                   [
                                     "SingletonNode", 
                                     "EnumerateCollectionNode", 
                                     "CalculationNode", 
                                     "FilterNode", 
                                     "RemoveNode", 
                                     "RemoteNode", 
                                     "GatherNode"
                                   ],
                                   [
                                     "SingletonNode", 
                                     "EnumerateCollectionNode", 
                                     "RemoteNode", 
                                     "GatherNode", 
                                     "ScatterNode", 
                                     "RemoteNode", 
                                     "EnumerateCollectionNode", 
                                     "RemoveNode", 
                                     "RemoteNode", 
                                     "GatherNode"
                                   ]
                                ];

      var expectedNodesDisabled = [ 
                                    [
                                      "SingletonNode", 
                                      "EnumerateCollectionNode", 
                                      "CalculationNode", 
                                      "FilterNode", 
                                      "CalculationNode", 
                                      "RemoteNode", 
                                      "GatherNode", 
                                      "DistributeNode", 
                                      "RemoteNode", 
                                      "RemoveNode", 
                                      "RemoteNode", 
                                      "GatherNode", 
                                    ],
                                    [
                                      "SingletonNode", 
                                      "EnumerateCollectionNode", 
                                      "CalculationNode", 
                                      "FilterNode", 
                                      "RemoteNode", 
                                      "GatherNode", 
                                      "DistributeNode", 
                                      "RemoteNode", 
                                      "RemoveNode", 
                                      "RemoteNode", 
                                      "GatherNode" 
                                    ],
                                    [
                                      "SingletonNode", 
                                      "EnumerateCollectionNode", 
                                      "RemoteNode", 
                                      "GatherNode", 
                                      "ScatterNode", 
                                      "RemoteNode", 
                                      "EnumerateCollectionNode", 
                                      "RemoteNode", 
                                      "GatherNode", 
                                      "DistributeNode", 
                                      "RemoteNode", 
                                      "RemoveNode", 
                                      "RemoteNode", 
                                      "GatherNode"
                                    ],
                                  ];

      queries.forEach(function(query, i) {
        var result = AQL_EXPLAIN(query, { }, allRules);
        assertTrue(result.plan.rules.indexOf(distribute) !== -1, query);
        assertTrue(result.plan.rules.indexOf(undist) !== -1, query);
        assertEqual(expectedNodesEnabled[i], explain(result), query);
        
        result = AQL_EXPLAIN(query, { }, ruleDisabled);
        assertTrue(result.plan.rules.indexOf(distribute) !== -1, query);
        assertTrue(result.plan.rules.indexOf(undist) === -1, query);
        assertEqual(expectedNodesDisabled[i], explain(result), query);
        
        AQL_EXECUTE(query, { }, allRules);
        if (i === 0) {
            assertEqual(5,  c1.count());
            assertEqual(10, c2.count());
            assertEqual(10, c3.count());
        }
        if (i === 1) {
            assertEqual(1,  c1.count());
            assertEqual(10, c2.count());
            assertEqual(10, c3.count());
        }
        if (i === 2) {
            assertEqual(1,  c1.count());
            assertEqual(0, c2.count());
            assertEqual(10, c3.count());
        }
      });
    },
    
    ////////////////////////////////////////////////////////////////////////////////
    // uses "scatter-in-cluster" and "undistribute-remove-after-enum-coll"
    ////////////////////////////////////////////////////////////////////////////////
    
    testRule2AndRule3 : function () {
      var queries = [ 
       // collection not sharded by _key  
        "FOR d IN " + cn3 + " FILTER d.age > 4 REMOVE d IN " + cn3
      ];

      var expectedNodesEnabled = [ 
        [
        "SingletonNode", 
        "EnumerateCollectionNode", 
        "CalculationNode", 
        "FilterNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
        ] 
      ];
      var expectedNodesDisabled = [
        [
        "SingletonNode", 
        "EnumerateCollectionNode", 
        "CalculationNode", 
        "FilterNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
        ] 
      ];

      queries.forEach(function(query, i) {
        var result = AQL_EXPLAIN(query, { }, allRules);
        assertTrue(result.plan.rules.indexOf(scatter) !== -1, query);
        assertTrue(result.plan.rules.indexOf(undist) !== -1, query);
        assertTrue(result.plan.rules.indexOf(distribute) === -1, query);
        assertEqual(expectedNodesEnabled[i], explain(result), query);
        
        result = AQL_EXPLAIN(query, { }, ruleDisabled);
        assertTrue(result.plan.rules.indexOf(scatter) !== -1, query);
        assertTrue(result.plan.rules.indexOf(undist) === -1, query);
        assertTrue(result.plan.rules.indexOf(distribute) === -1, query);
        assertEqual(expectedNodesDisabled[i], explain(result), query);
        
        AQL_EXECUTE(query, { }, allRules);
        assertEqual(10, c1.count());
        assertEqual(10,  c2.count());
        assertEqual(5, c3.count());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    // uses "distribute-in-cluster" but not "undistribute-remove-after-enum-coll"
    ////////////////////////////////////////////////////////////////////////////////

    testRule1NotRule3 : function () {
      var queries = [ 
        // different collections 
        "FOR d IN  " + cn1 + " FILTER d.age > 100 REMOVE d IN  " + cn2,

        // different collections 
        "FOR d IN  " + cn1 + " FILTER d.age > 100 REMOVE d._key IN  " + cn2,

        // different variables in FOR and REMOVE 
        "LET x = {_key: 'blah'} FOR y IN  " + cn1 + " REMOVE x IN  " + cn1,

        // different variables in FOR and REMOVE 
        "LET x = {_key: 'blah'} FOR y IN  " + cn1 + " REMOVE x._key IN  " + cn1,

        // not removing x or x._key 
        "FOR x IN  " + cn1 + " FILTER x.age > 5 REMOVE {_key: x._key} IN  " + cn1,

        // not removing x or x._key 
        "FOR x IN  " + cn1 + " FILTER x.age > 5 REMOVE {_key: x._key} IN  " + cn2,

        // different collections  (the outer scatters, the inner distributes)
        "FOR x IN  " + cn1 + " FOR y IN  " + cn2 + " REMOVE y IN  " + cn1,

        // different variable  (the outer scatters and the inner distributes)
        "FOR x IN  " + cn1 + " FOR y IN  " + cn2 + " REMOVE x IN  " + cn1
      ];

      var expectedNodes = [ 
        [
        "SingletonNode", 
        "EnumerateCollectionNode", 
        "CalculationNode", 
        "FilterNode", 
        "RemoteNode", 
        "GatherNode", 
        "DistributeNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "EnumerateCollectionNode", 
        "CalculationNode", 
        "FilterNode", 
        "CalculationNode", 
        "RemoteNode", 
        "GatherNode", 
        "DistributeNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "CalculationNode", 
        "EnumerateCollectionNode", 
        "RemoteNode", 
        "GatherNode", 
        "DistributeNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "CalculationNode", 
        "EnumerateCollectionNode", 
        "RemoteNode", 
        "GatherNode", 
        "DistributeNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "EnumerateCollectionNode", 
        "CalculationNode", 
        "FilterNode", 
        "CalculationNode", 
        "RemoteNode", 
        "GatherNode", 
        "DistributeNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "EnumerateCollectionNode", 
        "CalculationNode", 
        "FilterNode", 
        "CalculationNode", 
        "RemoteNode", 
        "GatherNode", 
        "DistributeNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "EnumerateCollectionNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "EnumerateCollectionNode", 
        "RemoteNode", 
        "GatherNode", 
        "DistributeNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "EnumerateCollectionNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "EnumerateCollectionNode", 
        "RemoteNode", 
        "GatherNode", 
        "DistributeNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ]
          ];

      queries.forEach(function(query, i) {
        var result = AQL_EXPLAIN(query, { }, allRulesNoInter);
        assertTrue(result.plan.rules.indexOf(undist) === -1, query);
        assertTrue(result.plan.rules.indexOf(distribute) !== -1, query);
        assertEqual(expectedNodes[i], explain(result), query);
        
        result = AQL_EXPLAIN(query, { }, ruleDisabledNoInter);
        assertTrue(result.plan.rules.indexOf(undist) === -1, query);
        assertTrue(result.plan.rules.indexOf(distribute) !== -1, query);
        assertEqual(expectedNodes[i], explain(result), query);
        
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    // uses "scatter-in-cluster" but not "undistribute-remove-after-enum-coll"
    ////////////////////////////////////////////////////////////////////////////////

    testRule2NotRule3 : function () {
      var queries = [ 
        // shard key of  " + cn3 + "  is not _key - OK
        "FOR d IN  " + cn1 + " FILTER d.age > 100 REMOVE d IN  " + cn3,

        // shard key of  " + cn3 + "  is not _key - OK 
        // [undistribute-remove-after-enum-coll was applied previously]
        "FOR d IN  " + cn3 + "  FILTER d.age > 100 REMOVE d._key IN  " + cn3,

        // different variables in FOR and REMOVE - OK
        "LET x = {_key: 'blah'} FOR y IN  " + cn3 + "  REMOVE x IN  " + cn3,

        // different variables in FOR and REMOVE - OK [doesn't use DistributeBlock and
        // so undistribute-remove-after-enum-coll is not applied]
        "LET x = {_key: 'blah'} FOR y IN  " + cn3 + "  REMOVE x._key IN  " + cn3,

        // not removing x or x._key - OK
        "FOR x IN  " + cn3 + "  FILTER x.age > 5 REMOVE {_key: x._key} IN  " + cn3,

        // not removing x or x._key - OK
        "FOR x IN  " + cn1 + " FILTER x.age > 5 REMOVE {_key: x._key} IN  " + cn3,

        // different variable - OK the outer and inner scatter
        "FOR x IN  " + cn3 + "  FOR y IN  " + cn2 + "  REMOVE x IN  " + cn3
      ];

      var expectedNodes = [ 
        [
        "SingletonNode", 
        "EnumerateCollectionNode", 
        "CalculationNode", 
        "FilterNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "EnumerateCollectionNode", 
        "CalculationNode", 
        "FilterNode", 
        "CalculationNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "CalculationNode", 
        "EnumerateCollectionNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "CalculationNode", 
        "EnumerateCollectionNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "EnumerateCollectionNode", 
        "CalculationNode", 
        "FilterNode", 
        "CalculationNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "EnumerateCollectionNode", 
        "CalculationNode", 
        "FilterNode", 
        "CalculationNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ],
        [
          "SingletonNode", 
        "EnumerateCollectionNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "EnumerateCollectionNode", 
        "RemoteNode", 
        "GatherNode", 
        "ScatterNode", 
        "RemoteNode", 
        "RemoveNode", 
        "RemoteNode", 
        "GatherNode" 
          ]
          ];

      queries.forEach(function(query, i) {
        var result = AQL_EXPLAIN(query, { }, allRulesNoInter);
        assertTrue(result.plan.rules.indexOf(undist) === -1, query);
        assertTrue(result.plan.rules.indexOf(distribute) === -1, query);
        assertTrue(result.plan.rules.indexOf(scatter) !== -1, query);
        assertEqual(expectedNodes[i], explain(result), query);
        
        result = AQL_EXPLAIN(query, { }, ruleDisabledNoInter);
        assertTrue(result.plan.rules.indexOf(undist) === -1, query);
        assertTrue(result.plan.rules.indexOf(distribute) === -1, query);
        assertTrue(result.plan.rules.indexOf(scatter) !== -1, query);
        assertEqual(expectedNodes[i], explain(result), query);
      });
    }
  };
}
jsunity.run(optimizerRuleTestSuite);
jsunity.run(interactionOtherRulesTestSuite);

return jsunity.done();

