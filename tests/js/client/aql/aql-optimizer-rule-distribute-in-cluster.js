/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertMatch */

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

const db = require("@arangodb").db;
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  const ruleName = "distribute-in-cluster";
  // various choices to control the optimizer: 
  const rulesNone        = { optimizer: { rules: [ "-all" ] } };
  const rulesAll         = { optimizer: { rules: [ "+all", "-reduce-extraction-to-projection", "-parallelize-gather" ] } };
  const thisRuleEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  const thisRuleDisabled = { optimizer: { rules: [ "+all", "-reduce-extraction-to-projection", "-parallelize-gather", "-" + ruleName ] } };
  const maxPlans         = { optimizer: { rules: [ "-all" ] }, maxNumberOfPlans: 1 };

  const cn1 = "UnitTestsAqlOptimizerRuleUndist1";
  const cn2 = "UnitTestsAqlOptimizerRuleUndist2";
  let c1, c2;
  
  const explain = function (result) {
    return helper.getCompactPlan(result).map(function(node) 
        { return node.type; });
  };

  return {

    setUpAll : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1, {numberOfShards:9});
      c2 = db._create(cn2, {numberOfShards:9, shardKeys:["a","b"]});
      for (let i = 0; i < 10; ++i) { 
        c1.insert({Hallo1:i});
        c2.insert({Hallo2:i});
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(cn1);
      db._drop(cn2);
    },
    
    testKeyGenerationOnInsertDefaultShardKey : function () {
      const temp = "UnitTestsTarget";

      let queries = [
        // [ query string, must have calculation node ],
        [ "FOR d IN " + cn1 + " INSERT d IN " + temp, false ],
        [ "FOR d IN " + cn1 + " INSERT { _key: d._key } IN " + temp, true ],
        [ "FOR d IN " + cn1 + " INSERT d._key IN " + temp, true ],
        [ "FOR d IN 1..10 INSERT { _key: CONCAT('test', d) } IN " + temp, true ],
        [ "FOR d IN 1..10 LET doc = { _key: CONCAT('test', d) } INSERT NOOPT(doc) IN " + temp, true ],
        [ "FOR d IN " + cn1 + " INSERT UNSET(d, ['_key']) IN " + temp, true ],
        [ "FOR d IN " + cn1 + " INSERT { fuchs: d._key } IN " + temp, true ],
      ];

      for (let i = 1; i <= 3; ++i) {
        let c = db._create(temp, { numberOfShards: i });
        try {
          queries.forEach(function(query) {
            let result = db._createStatement({query: query[0], options: thisRuleEnabled}).explain();
            assertNotEqual(-1, explain(result).indexOf("DistributeNode"));
            if (query[1]) {
              assertMatch(/MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION/, JSON.stringify(result.plan.nodes));
            }
            
            c.truncate();

            // the execute command must not fail
            db._query(query[0], {}, thisRuleEnabled);
          });
        } finally {
          db._drop(temp);
        }
      }
    },
    
    testKeyGenerationOnInsertCustomShardKey : function () {
      const temp = "UnitTestsTarget";

      let queries = [
        // [ query string, query will execute successfully ]
        [ "FOR d IN " + cn1 + " INSERT d IN " + temp, false ],
        [ "FOR d IN " + cn1 + " INSERT { _key: d._key } IN " + temp, false ],
        [ "FOR d IN " + cn1 + " INSERT d._key IN " + temp, false ],
        [ "FOR d IN 1..10 INSERT { _key: CONCAT('test', d) } IN " + temp, false ],
        [ "FOR d IN 1..10 LET doc = { _key: CONCAT('test', d) } INSERT NOOPT(doc) IN " + temp, false ],

        [ "FOR d IN " + cn1 + " INSERT UNSET(d, ['_key']) IN " + temp, true ],
        [ "FOR d IN " + cn1 + " INSERT { fuchs: d._key } IN " + temp, true ],
      ];

      for (let i = 1; i <= 3; ++i) {
        let c = db._create(temp, { numberOfShards: i, shardKeys: ["testi"] });
        try {
          queries.forEach(function(query) {
            let result = db._createStatement({query: query[0], options: thisRuleEnabled}).explain();
            assertNotEqual(-1, explain(result).indexOf("DistributeNode"));
            assertMatch(/MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION/, JSON.stringify(result.plan.nodes));

            c.truncate();
            let success = true;
            try {
              // the execute command can fail for some of the queries
              db._query(query[0], {}, thisRuleEnabled);
            } catch (err) {
              success = false;
            }

            assertEqual(query[1], success, query);
          });
        } finally {
          db._drop(temp);
        }
      }
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
        var result = db._createStatement({query: query, options: maxPlans}).explain();
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
                              "CalculationNode", 
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
                              "CalculationNode", 
                              "DistributeNode",
                              "RemoteNode",
                              "UpdateNode",
                              "RemoteNode",
                              "GatherNode"
                            ]
                          ];

      queries.forEach(function(query, i) {
        var result = db._createStatement({query: query, options: thisRuleEnabled}).explain();
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
                              "optimize-projections",
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-remote-scatter",
                              "scatter-in-cluster", 
                              "undistribute-remove-after-enum-coll",
                            ], 
                            [ 
                              "distribute-in-cluster", 
                              "distribute-filtercalc-to-cluster", 
                              "optimize-projections",
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-calculations-4", 
                              "remove-unnecessary-remote-scatter",
                              "scatter-in-cluster", 
                              "undistribute-remove-after-enum-coll",
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
                              "optimize-projections",
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-calculations-4", 
                              "remove-unnecessary-remote-scatter",
                              "scatter-in-cluster",
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
                              "RemoveNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [ 
                              "SingletonNode", 
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
        var result = db._createStatement({query: query, options:  rulesAll}).explain();

        assertEqual(expectedRules[i].sort(), result.plan.rules.sort(), query);
        result = db._createStatement({query: query, options:  thisRuleDisabled}).explain();
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
                              "optimize-projections",
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-remote-scatter",
                              "scatter-in-cluster", 
                              "undistribute-remove-after-enum-coll",
                            ], 
                            [ 
                              "distribute-filtercalc-to-cluster", 
                              "distribute-in-cluster", 
                              "optimize-projections",
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-calculations-4", 
                              "remove-unnecessary-remote-scatter",
                              "scatter-in-cluster", 
                              "undistribute-remove-after-enum-coll",
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
                              "optimize-projections",
                              "remove-data-modification-out-variables",
                              "remove-unnecessary-calculations-4", 
                              "remove-unnecessary-remote-scatter", 
                              "scatter-in-cluster",
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
                              "RemoveNode",
                              "RemoteNode",
                              "GatherNode"
                            ],
                            [ 
                              "SingletonNode", 
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
        var result = db._createStatement({query: query, options: rulesAll}).explain();
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
                              "CalculationNode", 
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
        var result = db._createStatement({query: query, options: rulesNone}).explain();
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
        var result1 = db._createStatement({query: query, options: thisRuleEnabled}).explain();
        var result2 = db._createStatement({query: query, options: rulesAll}).explain();

        assertTrue(result1.plan.rules.indexOf(ruleName) === -1, query);
        assertTrue(result2.plan.rules.indexOf(ruleName) === -1, query);
      });
    },
    
    
    testInsertsDistributeInputCalculationForModification : function () {
      var queries = [ 
        ["FOR k IN  ['1','2','3'] REMOVE k IN  " + cn1, "REMOVE"],
        ["FOR k IN  ['1','2','3'] UPDATE k WITH { } IN  " + cn1, "UPDATE"],
        ["FOR k IN  ['1','2','3'] REPLACE k WITH { } IN  " + cn1, "REPLACE"],
      ];

      const explainer = require("@arangodb/aql/explainer");
      queries.forEach(function(query, i) {
        const output = explainer.explain(query[0], {...thisRuleEnabled, colors: false}, false);
        const variable = output.match(/LET #([0-9]) = MAKE_DISTRIBUTE_INPUT\(k, { "allowKeyConversionToObject" : true, "ignoreErrors" : false, "canUseCustomKey" : true }\)/);
        assertTrue(variable);
        assertTrue(output.includes(`DISTRIBUTE #${variable[1]}`));
        assertTrue(output.includes(`${query[1]} #${variable[1]}`));
      });
    },
    
    testInsertsDistributeInputCalculationForInsert : function () {
      const query = "FOR k IN  ['1','2','3'] INSERT k IN  " + cn1;
      const explainer = require("@arangodb/aql/explainer");
      const output = explainer.explain(query, {...thisRuleEnabled, colors: false}, false);
      const variable = output.match(/LET #([0-9]) = MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION/);
      assertTrue(variable);
      assertTrue(output.includes(`MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION(k, null, { "allowSpecifiedKeys" : false, "ignoreErrors" : false, "collection" : "${cn1}" })`));
      assertTrue(output.includes(`DISTRIBUTE #${variable[1]}`));
      assertTrue(output.includes(`INSERT #${variable[1]}`));
    },
    
    testInsertsDistributeInputCalculationForUpsert : function () {
      const query = "FOR k IN  ['1','2','3'] UPSERT {_key: k} INSERT { miau: 42 } UPDATE { } IN  " + cn1;
      const explainer = require("@arangodb/aql/explainer");
      const output = explainer.explain(query, {...thisRuleEnabled, colors: false}, false);
      const distributeVar = output.match(/LET #([0-9]+) = MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION/);
      const inputVar = output.match(/LET #([0-9]+) = \{ \"miau\" : 42 \}/);
      assertTrue(distributeVar);
      assertTrue(inputVar);
      assertTrue(output.includes(`MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION($OLD, #${inputVar[1]}, { "allowSpecifiedKeys" : true, "ignoreErrors" : false, "collection" : "${cn1}" })`));
      assertTrue(output.includes(`DISTRIBUTE #${distributeVar[1]}`));
      assertTrue(output.includes(`UPSERT $OLD INSERT #${distributeVar[1]} UPDATE`));
    },
  };
}

function interactionOtherRulesTestSuite () {
  var distribute = "distribute-in-cluster";           // Rule 1
  var scatter = "scatter-in-cluster";                 // Rule 2
  var undist = "undistribute-remove-after-enum-coll"; // Rule 3

  // various choices to control the optimizer: 
  var allRules         = { optimizer: { rules: [ "+all", "-reduce-extraction-to-projection", "-move-filters-into-enumerate" ] } };
  var allRulesNoInter  = 
    { optimizer: { rules: [ "+all", "-interchange-adjacent-enumerations", "-reduce-extraction-to-projection", "-move-filters-into-enumerate" ] } };
  var ruleDisabled   = { optimizer: { rules: [ "+all", "-" + undist, "-move-filters-into-enumerate" ] } };
  var ruleDisabledNoInter  = 
    { optimizer: { rules: [ "+all", 
                            "-interchange-adjacent-enumerations", 
                            "-reduce-extraction-to-projection",
                            "-move-filters-into-enumerate",
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
      const projectionNode = "IndexNode";
      
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
                                      projectionNode, 
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
        var result = db._createStatement({query: query, options: allRules}).explain();
        assertTrue(result.plan.rules.indexOf(distribute) !== -1, query);
        assertTrue(result.plan.rules.indexOf(undist) !== -1, query);
        assertEqual(expectedNodesEnabled[i], explain(result), query);
        
        result = db._createStatement({query: query, options: ruleDisabled}).explain();
        assertTrue(result.plan.rules.indexOf(distribute) !== -1, query);
        assertTrue(result.plan.rules.indexOf(undist) === -1, query);
        assertEqual(expectedNodesDisabled[i], explain(result), query);
        
        db._query(query, { }, allRules);
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
        var result = db._createStatement({query: query, options: allRules}).explain();
        assertTrue(result.plan.rules.indexOf(scatter) !== -1, query);
        assertTrue(result.plan.rules.indexOf(undist) !== -1, query);
        assertTrue(result.plan.rules.indexOf(distribute) === -1, query);
        assertEqual(expectedNodesEnabled[i], explain(result), query);
        
        result = db._createStatement({query: query, options: ruleDisabled}).explain();
        assertTrue(result.plan.rules.indexOf(scatter) !== -1, query);
        assertTrue(result.plan.rules.indexOf(undist) === -1, query);
        assertTrue(result.plan.rules.indexOf(distribute) === -1, query);
        assertEqual(expectedNodesDisabled[i], explain(result), query);
        
        db._query(query, { }, allRules);
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
          "CalculationNode", 
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
          "CalculationNode", 
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
          "CalculationNode", 
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
        var result = db._createStatement({query: query, options: allRulesNoInter}).explain();
        assertTrue(result.plan.rules.indexOf(undist) === -1, query);
        assertTrue(result.plan.rules.indexOf(distribute) !== -1, query);
        assertEqual(expectedNodes[i], explain(result), query);
        
        result = db._createStatement({query: query, options: ruleDisabledNoInter}).explain();
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
        var result = db._createStatement({query: query, options: allRulesNoInter}).explain();
        assertTrue(result.plan.rules.indexOf(undist) === -1, query);
        assertTrue(result.plan.rules.indexOf(distribute) === -1, query);
        assertTrue(result.plan.rules.indexOf(scatter) !== -1, query);
        assertEqual(expectedNodes[i], explain(result), query);
        
        result = db._createStatement({query: query, options: ruleDisabledNoInter}).explain();
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
