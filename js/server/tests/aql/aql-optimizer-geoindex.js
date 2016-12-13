/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

// execute with:
// ./scripts/unittest shell_server_aql --test js/server/tests/aql/aql-optimizer-geoindex.js 

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
var findExecutionNodes = helper.findExecutionNodes;
var findReferencedNodes = helper.findReferencedNodes;
var getQueryMultiplePlansAndExecutions = helper.getQueryMultiplePlansAndExecutions;
var removeAlwaysOnClusterRules = helper.removeAlwaysOnClusterRules;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite() {
  // quickly disable tests here
  var enabled = {
    basics : true,
    removeNodes : true,
    sorted : true
  }

  var ruleName = "use-geoindex";
  var secondRuleName = "use-geoindexes";
  var removeCalculationNodes = "remove-unnecessary-calculations-2";
  var colName = "UnitTestsAqlOptimizer" + ruleName.replace(/-/g, "_");
  var colNameOther = colName + "_XX";

  // various choices to control the optimizer: 
  var paramNone = { optimizer: { rules: [ "-all" ] } };
  var paramIndexFromSort  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramIndexRange   = { optimizer: { rules: [ "-all", "+" + secondRuleName ] } };
  var paramIndexFromSort_IndexRange = { optimizer: { rules: [ "-all", "+" + ruleName, "+" + secondRuleName ] } };
  var paramIndexFromSort_IndexRange_RemoveCalculations = {
    optimizer: { rules: [ "-all", "+" + ruleName, "+" + secondRuleName, "+" + removeCalculationNodes ] }
  };
  var paramIndexFromSort_RemoveCalculations = {
    optimizer: { rules: [ "-all", "+" + ruleName, "+" + removeCalculationNodes ] }
  };

  var geocol;
  var sortArray = function (l, r) {
    if (l[0] !== r[0]) {
      return l[0] < r[0] ? -1 : 1;
    }
    if (l[1] !== r[1]) {
      return l[1] < r[1] ? -1 : 1;
    }
    return 0;
  };
  var hasSortNode = function (plan) {
    assertEqual(findExecutionNodes(plan, "SortNode").length, 1, "Has SortNode");
  };
  var hasNoSortNode = function (plan) {
    assertEqual(findExecutionNodes(plan, "SortNode").length, 0, "Has no SortNode");
  };
  var hasFilterNode = function (plan) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 1, "Has FilterNode");
  };
  var hasNoFilterNode = function (plan) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 0, "Has no FilterNode");
  };
  var hasNoIndexNode = function (plan) {
    assertEqual(findExecutionNodes(plan, "IndexNode").length, 0, "Has no IndexNode");
  };
  var hasNoResultsNode = function (plan) {
    assertEqual(findExecutionNodes(plan, "NoResultsNode").length, 1, "Has NoResultsNode");
  };
  var hasCalculationNodes = function (plan, countXPect) {
    assertEqual(findExecutionNodes(plan, "CalculationNode").length,
                countXPect, "Has " + countXPect +  " CalculationNode");
  };
  var hasIndexNode = function (plan) {
    var rn = findExecutionNodes(plan, "IndexNode");
    assertEqual(rn.length, 1, "Has IndexNode");
    return;
  };
  var isNodeType = function(node, type) {
    assertEqual(node.type, type, "check whether this node is of type "+type);
  };

  var geodistance = function(latitude1, longitude1, latitude2, longitude2) {
    //if (TYPEWEIGHT(latitude1) !== TYPEWEIGHT_NUMBER ||
    //  TYPEWEIGHT(longitude1) !== TYPEWEIGHT_NUMBER ||
    //  TYPEWEIGHT(latitude2) !== TYPEWEIGHT_NUMBER ||
    //  TYPEWEIGHT(longitude2) !== TYPEWEIGHT_NUMBER) {
    //  WARN('DISTANCE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    //  return null;
    //}

    //var p1 = AQL_TO_NUMBER(latitude1) * (Math.PI / 180.0);
    //var p2 = AQL_TO_NUMBER(latitude2) * (Math.PI / 180.0);
    //var d1 = AQL_TO_NUMBER(latitude2 - latitude1) * (Math.PI / 180.0);
    //var d2 = AQL_TO_NUMBER(longitude2 - longitude1) * (Math.PI / 180.0);

    var p1 = (latitude1) * (Math.PI / 180.0);
    var p2 = (latitude2) * (Math.PI / 180.0);
    var d1 = (latitude2 - latitude1) * (Math.PI / 180.0);
    var d2 = (longitude2 - longitude1) * (Math.PI / 180.0);

    var a = Math.sin(d1 / 2.0) * Math.sin(d1 / 2.0) +
            Math.cos(p1) * Math.cos(p2) *
            Math.sin(d2 / 2.0) * Math.sin(d2 / 2.0);
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1.0 - a));

    return (6371e3 * c);
  }


  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var loopto = 10;

        internal.db._drop(colName);
        geocol = internal.db._create(colName);
        geocol.ensureIndex({type:"geo", fields:["lat","lon"]})
        for (lat=-40; lat <=40 ; ++lat){
            for (lon=-40; lon <= 40; ++lon){
                geocol.insert({lat,lon});
            }
        }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(colName);
      internal.db._drop(colNameOther);
      geocol = null;
    },

    testRuleBasics : function () {
      if(enabled.basics){
        geocol.ensureIndex({ type: "hash", fields: [ "y", "z" ], unique: false });

        var queries = [
          //query                                                         clust  sort   filter index
          [ "FOR d IN " + colName + " SORT distance(d.lat,d.lon, 0 ,0 ) ASC LIMIT 1 RETURN d", false, false, false, true ],
          [ "FOR d IN " + colName + " SORT distance(0, 0, d.lat,d.lon ) ASC LIMIT 1 RETURN d", false, false, false, true ],
          [ "FOR d IN " + colName + " FILTER distance(0, 0, d.lat,d.lon ) < 1 LIMIT 1 RETURN d", false, false, false, true ],
          [ "FOR d IN " + colName + " SORT distance(0, 0, d.lat, d.lon) FILTER distance(0, 0, d.lat,d.lon ) < 1 LIMIT 1 RETURN d", false, false, false, true ],
          [ "FOR d IN " + colName + " SORT distance(0, 0, d.lat, d.lon) FILTER distance(0, 0, d.lat,d.lon ) < 1 LIMIT 1 RETURN d", false, false, false, true ],
          [ "FOR i in 1..2 FOR d IN " + colName + " FILTER distance(0, 0, d.lat,d.lon ) < 1 && i > 1 LIMIT 1 RETURN d", false, false, true, false ],
        ];

        queries.forEach(function(query) {
          var result = AQL_EXPLAIN(query[0]);

          // //optimized on cluster
          // if (query[1]) {
          //   assertNotEqual(-1, removeAlwaysOnClusterRules(result.plan.rules).indexOf(ruleName), query[0]);
          // }
          // else {
          //   assertEqual(-1, removeAlwaysOnClusterRules(result.plan.rules).indexOf(ruleName), query[0]);
          // }

          //sort nodes
          if (query[2]) {
            hasSortNode(result);
          } else {
            hasNoSortNode(result);
          }

          //filter nodes
          if (query[3]) {
            hasFilterNode(result);
          } else {
            hasNoFilterNode(result);
          }

          if (query[4]) {
            hasIndexNode(result);
          } else {
            hasNoIndexNode(result);
          }

        });
      }
    }, // testRuleBasics

    testRuleRemoveNodes : function () {
      if(enabled.removeNodes){
        var queries = [ 
          [ "FOR d IN " + colName + " SORT distance(d.lat,d.lon, 0 ,0 ) ASC LIMIT 5 RETURN d", false, false, false ],
          [ "FOR d IN " + colName + " SORT distance(0, 0, d.lat,d.lon ) ASC LIMIT 5 RETURN d", false, false, false ],
          [ "FOR d IN " + colName + " FILTER distance(0, 0, d.lat,d.lon ) < 111200 RETURN d", false, false, false ],
//          [ "FOR i IN 1..2 FOR d IN geocol SORT distance(i,2,d.lat,d.lon) ASC LIMIT 5 RETURN d", false, false, false ],
        ];

        var expected = [
          [[0,0], [-1,0], [0,1], [1,0], [0,-1]],
          [[0,0], [-1,0], [0,1], [1,0], [0,-1]],
          [[0,0], [-1,0], [0,1], [1,0], [0,-1]],
        ]

        queries.forEach(function(query, qindex) {
          var result = AQL_EXECUTE(query[0]);
          expect(expected[qindex].length).to.be.equal(result.json.length)
          pairs = result.json.map(function(res){
              return [res.lat,res.lon];
          });
          //internal.print(pairs)
          assertEqual(expected[qindex].sort(),pairs.sort())
          //expect(expected[qindex].sort()).to.be.equal(result.json.sort())
        });
      }
    }, // testRuleSort

    testRuleSorted : function(){
      if(enabled.sorted){
        var old=0;
        var query = "FOR d IN " + colName + " SORT distance(d.lat, d.lon, 0, 0) RETURN distance(d.lat, d.lon, 0, 0)";
        var result = AQL_EXECUTE(query);
        distances = result.json.map(d => { return parseFloat(d.toFixed(5))});
        //internal.print(distances);
        old=0;
        distances.forEach(d => { assertTrue( d >= old); old = d; });
      }
    } //testSorted

  }; // test dictionary (return)
} // optimizerRuleTestSuite

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
