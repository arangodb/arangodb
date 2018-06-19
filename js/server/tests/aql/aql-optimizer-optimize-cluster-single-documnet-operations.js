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

    testRuleFetch : function () {
      var queries = [
        [ "FOR d IN " + cn1 + " FILTER d._key == '1' RETURN d", 0, true],
        [ "FOR d IN " + cn1 + " FILTER d.xyz == '1' RETURN d", 1, false],

//*

        [ "INSERT {_key: 'test', insert1: true} IN   " + cn1 + " OPTIONS {waitForSync: true, ignoreErrors:true}", 2, true],
        [ "INSERT {_key: 'test', insert1: true} INTO " + cn1 + " OPTIONS {waitForSync: true, ignoreErrors:true}", 3, true],

        [ "INSERT {_key: 'test', insert1: true} IN   " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN OLD" ],
        [ "INSERT {_key: 'test', insert1: true} INTO " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN OLD" ],
        [ "INSERT {_key: 'test', insert1: true} IN   " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN NEW" ],
        [ "INSERT {_key: 'test', insert1: true} INTO " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN NEW" ],
        [ "INSERT {_key: 'test', insert1: true} IN   " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN [OLD, NEW]" ],
        [ "INSERT {_key: 'test', insert1: true} INTO " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN [OLD, NEW]" ],
        [ "INSERT {_key: 'test', insert1: true} IN   " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN { old: OLD, new: NEW }" ],
        [ "INSERT {_key: 'test', insert1: true} INTO " + cn1 + " OPTIONS {waitForSync: true, overwrite: true} RETURN { old: OLD, new: NEW }" ]

//*/
        
/*
        Insert
------
INSERT ... IN/INTO collection OPTIONS ...
INSERT ... IN/INTO collection OPTIONS ... RETURN OLD
INSERT ... IN/INTO collection OPTIONS ... RETURN NEW
INSERT ... IN/INTO collection OPTIONS ... RETURN [OLD, NEW]
INSERT ... IN/INTO collection OPTIONS ... RETURN { old: OLD, new: NEW }

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

        
      ];
      var expectedRules = [[ "use-indexes",
                             "remove-filter-covered-by-index",
                             "remove-unnecessary-calculations-2",
                             "optimize-cluster-single-documnet-operations" ],
                           [  "scatter-in-cluster",
                              "distribute-filtercalc-to-cluster",
                              "remove-unnecessary-remote-scatter" ],
                           [],
                           []
                          ];

      var expectedNodes = [
          ["SingletonNode", "SingleRemoteOperationNode", "ReturnNode"],
          ["SingletonNode", "EnumerateCollectionNode", "CalculationNode",
           "FilterNode", "RemoteNode", "GatherNode", "ReturnNode"  ],
        [],
        []
      ];

      queries.forEach(function(query) {
        print(query)
        var result = AQL_EXPLAIN(query[0], { }, thisRuleEnabled);
        assertEqual(expectedRules[query[1]], result.plan.rules, query);
        assertEqual(expectedNodes[query[1]], explain(result), query);
        if (query[3]) {
          assertEqual(AQL_EXECUTE(query[0], {}, thisRuleEnabled),
                      AQL_EXECUTE(query[0], {}, thisRuleDisabled),
                      query[0]);
        }
      });
    },
  };
}
jsunity.run(optimizerClusterSingleDocumentTestSuite);

return jsunity.done();
