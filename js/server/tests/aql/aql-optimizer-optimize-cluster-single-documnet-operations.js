/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for COLLECT w/ COUNT
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
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");
var assertQueryError = helper.assertQueryError;
const isCluster = require("@arangodb/cluster").isCluster();

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerClusterSingleDocumentTestSuite () {
  var ruleName = "optimize-cluster-single-documnet-operations";
  // various choices to control the optimizer:
  var thisRuleEnabled  = { optimizer: { rules: [ "+all" ] } }; // we can only work after other rules
  var thisRuleDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  var cn1 = "UnitTestsCollection";
  var c;

  var explain = function (result) {
    return helper.getCompactPlan(result).map(function(node)
                                             { return node.type; });
  };
  var runTestSet = function(queries, expectedNodes, expectedRules) {
    queries.forEach(function(query) {
      print(query)
      var result = AQL_EXPLAIN(query[0], { }, thisRuleEnabled);
      print(query[1])
      print(expectedNodes[query[1]])
      print(expectedRules[query[1]])
      db._explain(query[0])
      assertEqual(expectedRules[query[1]], result.plan.rules, "Rules: " + JSON.stringify(query));
      assertEqual(expectedNodes[query[1]], explain(result), "Nodes: " + JSON.stringify(query));
      if (query[3]) {
        assertEqual(AQL_EXECUTE(query[0], {}, thisRuleEnabled),
                    AQL_EXECUTE(query[0], {}, thisRuleDisabled),
                    query[0]);
      }
    });
  };

  return {
    setUp : function () {
      db._drop(cn1);
      c = db._create(cn1, { numberOfShards: 5 });

      for (var i = 0; i < 20; ++i) {
        c.save({ _key: `${i}`, group: "test" + (i % 10), value1: i, value2: i % 5 });
      }
    },

    tearDown : function () {
      db._drop(cn1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test plans that should result
////////////////////////////////////////////////////////////////////////////////
        
/*
"INSERT {_key: 'test1', insert1: true} INTO UnitTestsCollection OPTIONS {waitForSync: true, ignoreErrors:true}"
INSERT {_key: 'chris' } INTO persons RETURN NEW

Update/Replace
--------------
UPDATE ... IN/INTO collection OPTIONS ...
UPDATE ... IN/INTO collection OPTIONS ... RETURN OLD
UPDATE ... IN/INTO collection OPTIONS ... RETURN NEW
UPDATE ... IN/INTO collection OPTIONS ... RETURN [OLD, NEW]
UPDATE ... IN/INTO collection OPTIONS ... RETURN { old: OLD, new: NEW }
UPDATE ... WITH ... IN/INTO collection OPTIONS ...
UPDATE ... WITH ... IN/INTO collection OPTIONS ... RETURN OLD
UPDATE ... WITH ... IN/INTO collection OPTIONS ... RETURN NEW
UPDATE ... WITH ... IN/INTO collection OPTIONS ... RETURN [OLD, NEW]
UPDATE ... WITH ... IN/INTO collection OPTIONS ... RETURN { old: OLD, new: NEW }


UPDATE DOCUMENT('persons/bob') WITH {foo: 'bar'} IN persons RETURN NEW
UPDATE 'bob' WITH {foo: 'bar'} IN persons RETURN NEW
FOR u IN persons FILTER u._key == 'bob' UPDATE u WITH {foo: 'bar'} IN persons RETURN NEW
UPDATE 'bob' WITH {foo: 'bar'} IN persons RETURN NEW

REPLACE wie UPDATE

Remove
------
REMOVE ... IN/INTO collection OPTIONS ...
REMOVE ... IN/INTO collection OPTIONS ... RETURN OLD
FOR doc IN collection FILTER doc._key == fixedValue REMOVE doc IN/INTO collection OPTIONS ...
FOR doc IN collection FILTER doc._key == fixedValue REMOVE doc IN/INTO collection OPTIONS ... RETURN OLD

*/

    
    testRuleFetch : function () {
      var queries = [
        [ "FOR d IN " + cn1 + " FILTER d._key == '1' RETURN d", 0, true],
        [ "FOR d IN " + cn1 + " FILTER d.xyz == '1' RETURN d", 1, false],

      ];
      var expectedRules = [[ "use-indexes",
                             "remove-filter-covered-by-index",
                             "remove-unnecessary-calculations-2",
                             "optimize-cluster-single-documnet-operations" ],
                           [ "scatter-in-cluster",
                             "distribute-filtercalc-to-cluster",
                             "remove-unnecessary-remote-scatter" ]
                          ];

      var expectedNodes = [
        [ "SingletonNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "EnumerateCollectionNode", "CalculationNode",
          "FilterNode", "RemoteNode", "GatherNode", "ReturnNode"  ]
      ];
      runTestSet(queries, expectedNodes, expectedRules);
    },
    //*

    testRuleInsert : function () {
      var queries = [
        [ "INSERT {_key: 'test', insert1: true} IN   " + cn1 + " OPTIONS {waitForSync: true, ignoreErrors:true}", 0, false],
        [ "INSERT {_key: 'test1', insert1: true} INTO " + cn1 + " OPTIONS {waitForSync: true, ignoreErrors:true}", 1, true],
        [ "INSERT {_key: 'test', insert1: true} IN   " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN OLD", 2, false],
        [ "INSERT {_key: 'test', insert1: true} INTO " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN OLD", 3, false ],
        [ "INSERT {_key: 'test', insert1: true} IN   " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN NEW", 4, false ],
        [ "INSERT {_key: 'test', insert1: true} INTO " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN NEW", 5, false ],

        // [ "INSERT {_key: 'test', insert1: true} IN   " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN [OLD, NEW]", 6, false ],
        // [ "INSERT {_key: 'test', insert1: true} INTO " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN [OLD, NEW]", 7, false ],
        // [ "INSERT {_key: 'test', insert1: true} IN   " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN { old: OLD, new: NEW }", 8, false ],
        // [ "INSERT {_key: 'test', insert1: true} INTO " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN { old: OLD, new: NEW }", 9, false ],
        //* /
      ];
      
      var expectedNodes = [
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ]];

      var expectedRules = [[ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ],
                           [ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations"
                           ],
                           [ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ],
                           [ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ],
                           [ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ],
                           [ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ],
                           [ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ],
                           [ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ],
                           [ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ],
                           [ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ],
                           [ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ]
                          ];
      
      runTestSet(queries, expectedNodes, expectedRules);
    },

    testRuleUpdate : function () {

      var queries = [
        [ "UPDATE {_key: '1'} IN   " + cn1 + " OPTIONS {}", 1, false],
        [ "UPDATE {_key: '1'} INTO " + cn1 + " OPTIONS {}", 2, false],
        [ "UPDATE {_key: '1'} IN   " + cn1 + " OPTIONS {} RETURN OLD", 3, false],
        [ "UPDATE {_key: '1'} INTO " + cn1 + " OPTIONS {} RETURN OLD", 4, false],
        [ "UPDATE {_key: '1'} IN   " + cn1 + " OPTIONS {} RETURN NEW", 5, false],
        [ "UPDATE {_key: '1'} INTO " + cn1 + " OPTIONS {} RETURN NEW", 6, false],
        //[ "UPDATE {_key: '1'} IN   " + cn1 + " OPTIONS {} RETURN [OLD, NEW]", 7, false],
        //[ "UPDATE {_key: '1'} INTO " + cn1 + " OPTIONS {} RETURN [OLD, NEW]", 8, false],
        //[ "UPDATE {_key: '1'} IN   " + cn1 + " OPTIONS {} RETURN { old: OLD, new: NEW }", 9, false],
        //[ "UPDATE {_key: '1'} INTO " + cn1 + " OPTIONS {} RETURN { old: OLD, new: NEW }", 10, false],
        //X [ "UPDATE {_key: '1'} WITH {foo: 'bar1a'} IN   " + cn1 + " OPTIONS {}", 11, false],
        //X [ "UPDATE {_key: '1'} WITH {foo: 'bar1b'} INTO " + cn1 + " OPTIONS {}", 12, false],
        //X [ "UPDATE {_key: '1'} WITH {foo: 'bar2a'} IN   " + cn1 + " OPTIONS {} RETURN OLD", 13, false],
        //X [ "UPDATE {_key: '1'} WITH {foo: 'bar2b'} INTO " + cn1 + " OPTIONS {} RETURN OLD", 14, false],
        //X [ "UPDATE {_key: '1'} WITH {foo: 'bar3a'} IN   " + cn1 + " OPTIONS {} RETURN NEW", 15, false],
        //X [ "UPDATE {_key: '1'} WITH {foo: 'bar3b'} INTO " + cn1 + " OPTIONS {} RETURN NEW", 16, false],
        //[ "UPDATE {_key: '1'} WITH {foo: 'bar4a'} IN   " + cn1 + " OPTIONS {} RETURN [OLD, NEW]", 17, false],
        //[ "UPDATE {_key: '1'} WITH {foo: 'bar4b'} INTO " + cn1 + " OPTIONS {} RETURN [OLD, NEW]", 18, false],
        //[ "UPDATE {_key: '1'} WITH {foo: 'bar5a'} IN   " + cn1 + " OPTIONS {} RETURN { old: OLD, new: NEW }", 19, false],
        //[ "UPDATE {_key: '1'} WITH {foo: 'bar5b'} INTO " + cn1 + " OPTIONS {} RETURN { old: OLD, new: NEW }", 20, false],
        //*/
      ];

      var expectedRules = [
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
      ];

      var expectedNodes = [
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ],
        [
        ],
        [
        ],
        [
        ],
        [
        ],
        [
        ],
        [
        ],
        [
        ]
      ];

      runTestSet(queries, expectedNodes, expectedRules);
    },

    testRuleRemove : function () {
      var queries = [
        /*
        [ "REMOVE {_key: '1'} IN   " + cn1 + " OPTIONS {}", 1, false],
        [ "REMOVE {_key: '2'} INTO " + cn1 + " OPTIONS {}", 1, false],
        [ "REMOVE {_key: '3'} IN   " + cn1 + " OPTIONS {} RETURN OLD", 1, false],
        [ "REMOVE {_key: '4'} INTO " + cn1 + " OPTIONS {} RETURN OLD", 1, false],
        //*/
        
      ];
      var expectedRules = [
        [ ]
      ];

      
      var expectedNodes = [
        [
        ],
        [
        ],
        [
        ],
        [
        ],
        [
        ],
        [
        ],
        [
        ],
        [
        ],

        
      ];
      runTestSet(queries, expectedNodes, expectedRules);
    }
  };
}
jsunity.run(optimizerClusterSingleDocumentTestSuite);

return jsunity.done();
