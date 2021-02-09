/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

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

let db = require("@arangodb").db;
let jsunity = require("jsunity");
let helper = require("@arangodb/aql-helper");
  
const ruleName = "undistribute-remove-after-enum-coll";
const rulesNone        = { optimizer: { rules: [ "-all" ] } };
const rulesAll         = { optimizer: { rules: [ "+all", "-cluster-one-shard" ] } };
const thisRuleEnabled  = { optimizer: { rules: [ "-all", "+distribute-filtercalc-to-cluster", "+" + ruleName ] } };
  
let explain = function (result) {
  return helper.getCompactPlan(result).map(function(node) 
      { return node.type; });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var cn1 = "UnitTestsAqlOptimizerRuleUndist1";
  var cn2 = "UnitTestsAqlOptimizerRuleUndist2";
  var c1, c2;
  
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
    /// @brief test that rule fires when it is enabled 
    ////////////////////////////////////////////////////////////////////////////////

    testThisRuleEnabled : function () {
      var queries = [ 
        [ "FOR d IN " + cn1 + " FILTER d.Hallo < 5 REMOVE d in " + cn1, 0 ],
        [ "FOR d IN " + cn1 + " FILTER d.Hallo < 5 REMOVE d._key in " + cn1, 1 ]
      ];

      var expectedRules = [ "distribute-in-cluster",
                            "scatter-in-cluster", 
                            "distribute-filtercalc-to-cluster", 
                            "undistribute-remove-after-enum-coll" ];

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
                             "GatherNode" ] ];

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
         ["FOR d IN " + cn1 + " FILTER d.Hallo < 5 REMOVE d in " + cn1, 0 ], 
         ["FOR d IN " + cn1 + " FILTER d.Hallo < 5 REMOVE d._key in " + cn1, 1 ]
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
          + cn2
      ];

      queries.forEach(function(query) {
        var result1 = AQL_EXPLAIN(query, { }, thisRuleEnabled);
        var result2 = AQL_EXPLAIN(query, { }, rulesAll);
        assertTrue(result1.plan.rules.indexOf(ruleName) !== -1, query);
        assertTrue(result2.plan.rules.indexOf(ruleName) !== -1, query);
      });
    },
  };
}

function optimizerRuleRemoveTestSuite () {
  const cn = "UnitTestsAqlOptimizerRule";
  
  return {

    setUp : function () {
      db._drop(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testRemoveDefaultShardingEffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["_key"] });

      let queries = [
        "FOR doc IN " + cn + " REMOVE doc IN " + cn,
        "FOR doc IN " + cn + " REMOVE doc._key IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc._key IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc._key IN " + cn,
        "FOR doc IN " + cn + " REMOVE doc.abc IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc.abc IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc.abc IN " + cn,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testRemoveDefaultShardingEffectiveResults : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["_key"] });
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }

      let queries = [
        [ "FOR doc IN " + cn + " REMOVE doc IN " + cn, 0 ],
        [ "FOR doc IN " + cn + " REMOVE doc._key IN " + cn, 0 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc IN " + cn, 1 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc._key IN " + cn, 1 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc IN " + cn, 4902 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc._key IN " + cn, 4902 ],
        [ "FOR doc IN " + cn + " REMOVE doc.abc IN " + cn + " OPTIONS { ignoreErrors: true }", 5000 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc.abc IN " + cn + " OPTIONS { ignoreErrors: true }", 5000 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc.abc IN " + cn + " OPTIONS { ignoreErrors: true }", 5000 ],
      ];

      queries.forEach(function(query) {
        c.truncate({ compact: false });
        c.insert(docs);

        let result = AQL_EXECUTE(query[0]);
        assertEqual(query[1], c.count(), query);
      });
    },
    
    testRemoveDefaultShardingIneffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["_key"] });

      let queries = [
        "FOR doc IN " + cn + " LIMIT 1 REMOVE doc IN " + cn,
        "FOR doc IN " + cn + " LIMIT 1 REMOVE doc._key IN " + cn,
        "FOR doc IN " + cn + " LIMIT 1 REMOVE doc.abc IN " + cn,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testRemoveCustomShardingEffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["value"] });

      let queries = [
        "FOR doc IN " + cn + " REMOVE doc IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc IN " + cn,
      ];
      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testRemoveCustomShardingEffectiveResults : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["value"] });
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value: i });
      }

      let queries = [
        [ "FOR doc IN " + cn + " REMOVE doc IN " + cn, 0 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc IN " + cn, 1 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc IN " + cn, 4902 ]
      ];

      queries.forEach(function(query) {
        c.truncate({ compact: false });
        c.insert(docs);

        let result = AQL_EXECUTE(query[0]);
        assertEqual(query[1], c.count(), query);
      });
    },
    
    testRemoveCustomShardingIneffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["value"] });

      let queries = [
        "FOR doc IN " + cn + " REMOVE doc._key IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc._key IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc._key IN " + cn,
        "FOR doc IN " + cn + " REMOVE doc.value IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc.value IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc.value IN " + cn,
        "FOR doc IN " + cn + " REMOVE doc.abc IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REMOVE doc.abc IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REMOVE doc.abc IN " + cn,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

  };
}

function optimizerRuleReplaceTestSuite () {
  const cn = "UnitTestsAqlOptimizerRule";
  
  return {

    setUp : function () {
      db._drop(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testReplaceDefaultShardingEffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["_key"] });

      let queries = [
        "FOR doc IN " + cn + " REPLACE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " REPLACE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " REPLACE doc.abc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc.abc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc.abc WITH {} IN " + cn,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testReplaceDefaultShardingEffectiveResults : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["_key"] });
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }

      let queries = [
        [ "FOR doc IN " + cn + " REPLACE doc WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 5000 ],
        [ "FOR doc IN " + cn + " REPLACE doc._key WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 5000 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 4999 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc._key WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 4999 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 98 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc._key WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 98 ],
        [ "FOR doc IN " + cn + " REPLACE doc.abc WITH {_key: doc._key, replaced: 1} IN " + cn + " OPTIONS { ignoreErrors: true } RETURN NEW", 0 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc.abc WITH {_key: doc._key, replaced: 1} IN " + cn + " OPTIONS { ignoreErrors: true } RETURN NEW", 0 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc.abc WITH {_key: doc._key, replaced: 1} IN " + cn + " OPTIONS { ignoreErrors: true } RETURN NEW", 0 ],
      ];

      queries.forEach(function(query) {
        c.truncate({ compact: false });
        c.insert(docs);

        let results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query);
      });
    },
    
    testReplaceDefaultShardingIneffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["_key"] });

      let queries = [
        "FOR doc IN " + cn + " LIMIT 1 REPLACE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " LIMIT 1 REPLACE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " LIMIT 1 REPLACE doc.abc WITH {} IN " + cn,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testReplaceCustomShardingEffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["value"] });

      let queries = [
        "FOR doc IN " + cn + " REPLACE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc WITH {} IN " + cn,
      ];
      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testReplaceCustomShardingEffectiveResults : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["value"] });
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value: i });
      }

      let queries = [
        [ "FOR doc IN " + cn + " REPLACE doc WITH {value: doc.value, replaced: 1} IN " + cn + " RETURN NEW", 5000 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc WITH {value: doc.value, replaced: 1} IN " + cn + " RETURN NEW", 4999 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc WITH {value: doc.value, replaced: 1} IN " + cn + " RETURN NEW", 98 ], 
      ];

      queries.forEach(function(query) {
        c.truncate({ compact: false });
        c.insert(docs);

        let results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query);
      });
    },
    
    testReplaceCustomShardingIneffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["value"] });

      let queries = [
        "FOR doc IN " + cn + " REPLACE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " REPLACE doc.value WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc.value WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc.value WITH {} IN " + cn,
        "FOR doc IN " + cn + " REPLACE doc.abc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 REPLACE doc.abc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 REPLACE doc.abc WITH {} IN " + cn,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
  };
}

function optimizerRuleUpdateTestSuite () {
  const cn = "UnitTestsAqlOptimizerRule";
  
  return {

    setUp : function () {
      db._drop(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testUpdateDefaultShardingEffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["_key"] });

      let queries = [
        "FOR doc IN " + cn + " UPDATE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " UPDATE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " UPDATE doc.abc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc.abc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc.abc WITH {} IN " + cn,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testUpdateDefaultShardingEffectiveResults : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["_key"] });
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }

      let queries = [
        [ "FOR doc IN " + cn + " UPDATE doc WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 5000 ],
        [ "FOR doc IN " + cn + " UPDATE doc._key WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 5000 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 4999 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc._key WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 4999 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 98 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc._key WITH {_key: doc._key, replaced: 1} IN " + cn + " RETURN NEW", 98 ],
        [ "FOR doc IN " + cn + " UPDATE doc.abc WITH {_key: doc._key, replaced: 1} IN " + cn + " OPTIONS { ignoreErrors: true } RETURN NEW", 0 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc.abc WITH {_key: doc._key, replaced: 1} IN " + cn + " OPTIONS { ignoreErrors: true } RETURN NEW", 0 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc.abc WITH {_key: doc._key, replaced: 1} IN " + cn + " OPTIONS { ignoreErrors: true } RETURN NEW", 0 ],
      ];

      queries.forEach(function(query) {
        c.truncate({ compact: false });
        c.insert(docs);

        let results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query);
      });
    },
    
    testUpdateDefaultShardingIneffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["_key"] });

      let queries = [
        "FOR doc IN " + cn + " LIMIT 1 UPDATE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " LIMIT 1 UPDATE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " LIMIT 1 UPDATE doc.abc WITH {} IN " + cn,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testUpdateCustomShardingEffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["value"] });

      let queries = [
        "FOR doc IN " + cn + " UPDATE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc WITH {} IN " + cn,
      ];
      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
    
    testUpdateCustomShardingEffectiveResults : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["value"] });
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value: i });
      }

      let queries = [
        [ "FOR doc IN " + cn + " UPDATE doc WITH {replaced: 1} IN " + cn + " RETURN NEW", 5000 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc WITH {replaced: 1} IN " + cn +  " RETURN NEW", 4999 ],
        [ "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc WITH {replaced: 1} IN " + cn + " RETURN NEW", 98 ],
      ];

      queries.forEach(function(query) {
        c.truncate({ compact: false });
        c.insert(docs);

        let results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query);
      });
    },
    
    testUpdateCustomShardingIneffective : function () {
      let c = db._create(cn, { numberOfShards: 3, shardKeys: ["value"] });

      let queries = [
        "FOR doc IN " + cn + " UPDATE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc._key WITH {} IN " + cn,
        "FOR doc IN " + cn + " UPDATE doc.value WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc.value WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc.value WITH {} IN " + cn,
        "FOR doc IN " + cn + " UPDATE doc.abc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 UPDATE doc.abc WITH {} IN " + cn,
        "FOR doc IN " + cn + " FILTER doc.value > 0 FILTER doc.value < 99 UPDATE doc.abc WITH {} IN " + cn,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },
  };
}

jsunity.run(optimizerRuleTestSuite);
jsunity.run(optimizerRuleRemoveTestSuite);
jsunity.run(optimizerRuleReplaceTestSuite);
jsunity.run(optimizerRuleUpdateTestSuite);

return jsunity.done();
