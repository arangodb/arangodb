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
      ];
      var expectedRules = [[ "use-indexes",
                             "remove-filter-covered-by-index",
                             "remove-unnecessary-calculations-2",
                             "optimize-cluster-single-documnet-operations" ],
                           [  "scatter-in-cluster",
                              "distribute-filtercalc-to-cluster",
                              "remove-unnecessary-remote-scatter" ]
                          ];

      var expectedNodes = [
          ["SingletonNode", "SingleRemoteOperationNode", "ReturnNode"],
          ["SingletonNode", "EnumerateCollectionNode", "CalculationNode",
           "FilterNode", "RemoteNode", "GatherNode", "ReturnNode"  ]
      ];

      queries.forEach(function(query) {
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
