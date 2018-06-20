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
  var c1;

  var cn2 = "UnitTestsCollectionEmty";
  var c2;

  var cn3 = "UnitTestsCollectionModify";
  var c3;

  var s = function() {}

  var setupC1 = function() {
    db._drop(cn1);
    c1 = db._create(cn1, { numberOfShards: 5 });

    for (var i = 0; i < 20; ++i) {
      c1.save({ _key: `${i}`, group: "test" + (i % 10), value1: i, value2: i % 5 });
    }
  }
  
  var setupC2 = function() {
    print("flush!");
    db._drop(cn2);
    c2 = db._create(cn2, { numberOfShards: 5 });
  }

  var setupC3 = function() {
    db._drop(cn3);
    c3 = db._create(cn3, { numberOfShards: 5 });

    for (var i = 0; i < 20; ++i) {
      c3.save({ _key: `${i}`, group: "test" + (i % 10), value1: i, value2: i % 5 });
    }
  }

  var explain = function (result) {
    return helper.getCompactPlan(result).map(function(node)
                                             { return node.type; });
  };
  var runTestSet = function(queries, expectedRules, expectedNodes) {
    let count = 0;
    queries.forEach(function(query) {
      print(query)
      var result = AQL_EXPLAIN(query[0], { }, thisRuleEnabled);
      print(count)
      print(query)
      print(expectedRules[query[1]])
      print(expectedNodes[query[2]])
      // db._explain(query[0])
      assertEqual(expectedRules[query[1]], result.plan.rules, "Rules: " + JSON.stringify(query));
      assertEqual(expectedNodes[query[2]], explain(result), "Nodes: " + JSON.stringify(query));
      if (query[3]) {
        query[4]();
        let r1 = AQL_EXECUTE(query[0], {}, thisRuleEnabled);
        query[4]();
        let r2 = AQL_EXECUTE(query[0], {}, thisRuleDisabled);
        assertEqual(r1.json, r2.json, query);
      }
      count += 1;
    });
  };

  return {
    setUp : function () {
      setupC1();
      setupC2();
      setupC3();
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
        [ "FOR d IN " + cn1 + " FILTER d._key == '1' RETURN d", 0, 0, true, s],
        [ "FOR d IN " + cn1 + " FILTER d.xyz == '1' RETURN d", 1, 1, false, s],

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
      runTestSet(queries, expectedRules, expectedNodes);
    },

    testRuleInsert : function () {
      var queries = [
        [ "INSERT {_key: 'test', insert1: true} IN   " + cn2 + " OPTIONS {waitForSync: true, ignoreErrors:true}", 0, 0, true, s ],
        [ "INSERT {_key: 'test1', insert1: true} INTO " + cn2 + " OPTIONS {waitForSync: true, ignoreErrors:true}", 0, 0, true, s ],
        [ "INSERT {_key: 'test', insert1: true} IN   " + cn2 + " OPTIONS {waitForSync: true, overwrite: true} RETURN OLD", 0, 1, true, setupC2 ],
        [ "INSERT {_key: 'test', insert1: true} INTO " + cn2 + " OPTIONS {waitForSync: true, overwrite: true} RETURN OLD", 0, 1, true, setupC2 ],
        [ "INSERT {_key: 'test', insert1: true} IN   " + cn2 + " OPTIONS {waitForSync: true, overwrite: true} RETURN NEW", 0, 1, true, setupC2 ],
        [ "INSERT {_key: 'test', insert1: true} INTO " + cn2 + " OPTIONS {waitForSync: true, overwrite: true} RETURN NEW", 0, 1, true, setupC2 ],

        // [ "INSERT {_key: 'test', insert1: true} IN   " + cn2 + " OPTIONS {waitForSync: true, overwrite: true} RETURN [OLD, NEW]", 1, 0, false ],
        // [ "INSERT {_key: 'test', insert1: true} INTO " + cn2 + " OPTIONS {waitForSync: true, overwrite: true} RETURN [OLD, NEW]", 1, 0, false ],
        // [ "INSERT {_key: 'test', insert1: true} IN   " + cn2 + " OPTIONS {waitForSync: true, overwrite: true} RETURN { old: OLD, new: NEW }", 1, 0, false ],
        // [ "INSERT {_key: 'test', insert1: true} INTO " + cn2 + " OPTIONS {waitForSync: true, overwrite: true} RETURN { old: OLD, new: NEW }", 1, 0, false ],
        //* /
      ];
      
      var expectedRules = [[ "remove-data-modification-out-variables", 
                             "optimize-cluster-single-documnet-operations" 
                           ]];
      var expectedNodes = [
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ]
      ];

      
      runTestSet(queries, expectedRules, expectedNodes);
    },
    testRuleUpdate : function () {

      var queries = [
        [ "UPDATE {_key: '1'} IN   " + cn1 + " OPTIONS {}", 0, 0, false],
        [ "UPDATE {_key: '1'} INTO " + cn1 + " OPTIONS {}", 0, 0, false],
        [ "UPDATE {_key: '1'} IN   " + cn1 + " OPTIONS {} RETURN OLD", 0, 1, false],
        [ "UPDATE {_key: '1'} INTO " + cn1 + " OPTIONS {} RETURN OLD", 0, 1, false],
        [ "UPDATE {_key: '1'} IN   " + cn1 + " OPTIONS {} RETURN NEW", 0, 1, false],
        [ "UPDATE {_key: '1'} INTO " + cn1 + " OPTIONS {} RETURN NEW", 0, 1, false],
        //[ "UPDATE {_key: '1'} IN   " + cn1 + " OPTIONS {} RETURN [OLD, NEW]", 0, 1, false],
        //[ "UPDATE {_key: '1'} INTO " + cn1 + " OPTIONS {} RETURN [OLD, NEW]", 0, 1, false],
        //[ "UPDATE {_key: '1'} IN   " + cn1 + " OPTIONS {} RETURN { old: OLD, new: NEW }", 0, 1, false],
        //[ "UPDATE {_key: '1'} INTO " + cn1 + " OPTIONS {} RETURN { old: OLD, new: NEW }", 0, 1, false],
        [ "UPDATE {_key: '1'} WITH {foo: 'bar1a'} IN   " + cn1 + " OPTIONS {}", 1, 0, false],
        [ "UPDATE {_key: '1'} WITH {foo: 'bar1b'} INTO " + cn1 + " OPTIONS {}", 1, 0, false],
        [ "UPDATE {_key: '1'} WITH {foo: 'bar2a'} IN   " + cn1 + " OPTIONS {} RETURN OLD", 1, 1, false],
        [ "UPDATE {_key: '1'} WITH {foo: 'bar2b'} INTO " + cn1 + " OPTIONS {} RETURN OLD", 1, 1, false],
        [ "UPDATE {_key: '1'} WITH {foo: 'bar3a'} IN   " + cn1 + " OPTIONS {} RETURN NEW", 1, 1, false],
        [ "UPDATE {_key: '1'} WITH {foo: 'bar3b'} INTO " + cn1 + " OPTIONS {} RETURN NEW", 1, 1, false],
        //[ "UPDATE {_key: '1'} WITH {foo: 'bar4a'} IN   " + cn1 + " OPTIONS {} RETURN [OLD, NEW]", 0, 1, false],
        //[ "UPDATE {_key: '1'} WITH {foo: 'bar4b'} INTO " + cn1 + " OPTIONS {} RETURN [OLD, NEW]", 0, 1, false],
        //[ "UPDATE {_key: '1'} WITH {foo: 'bar5a'} IN   " + cn1 + " OPTIONS {} RETURN { old: OLD, new: NEW }", 0, 1, false],
        //[ "UPDATE {_key: '1'} WITH {foo: 'bar5b'} INTO " + cn1 + " OPTIONS {} RETURN { old: OLD, new: NEW }", 0, 1, false],
      ];

      var expectedRules = [
        [ "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
        [ "move-calculations-up", "move-calculations-up-2", "remove-data-modification-out-variables", 
          "optimize-cluster-single-documnet-operations"],
        [ "move-calculations-up", "move-calculations-up-2",
          "remove-data-modification-out-variables", "optimize-cluster-single-documnet-operations" ],
      ];

      var expectedNodes = [
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode" ],
        [ "SingletonNode", "CalculationNode", "SingleRemoteOperationNode", "ReturnNode" ]
      ];

      runTestSet(queries, expectedRules, expectedNodes);
    },

    testRuleRemove : function () {
      var queries = [
        
        [ "REMOVE {_key: '1'} IN   " + cn1 + " OPTIONS {}", 0, 0, false],
        [ "REMOVE {_key: '2'} INTO " + cn1 + " OPTIONS {}", 0, 0, false],
        [ "REMOVE {_key: '3'} IN   " + cn1 + " OPTIONS {} RETURN OLD", 0, 1, false],
        [ "REMOVE {_key: '4'} INTO " + cn1 + " OPTIONS {} RETURN OLD", 0, 1, false],
      ];
      var expectedRules = [
        ["remove-data-modification-out-variables", 
         "optimize-cluster-single-documnet-operations" ]
      ];

      var expectedNodes = [
        ["SingletonNode", 
         "SingleRemoteOperationNode" ],
        [ "SingletonNode", 
          "SingleRemoteOperationNode", 
          "ReturnNode" ],
      ];
      runTestSet(queries, expectedRules, expectedNodes);
    }
  };
}
jsunity.run(optimizerClusterSingleDocumentTestSuite);

return jsunity.done();
