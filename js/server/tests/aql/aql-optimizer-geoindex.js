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
var db = require('internal').db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite() {
  // quickly disable tests here
  var enabled = {
    basics : true,
    removeNodes : true,
    sorted : true
  };

  var colName = "UnitTestsAqlOptimizerGeoIndex";

  var geocol;
  var hasSortNode = function (plan,query) {
    assertEqual(findExecutionNodes(plan, "SortNode").length, 1, query.string + " Has SortNode ");
  };
  var hasNoSortNode = function (plan,query) {
    assertEqual(findExecutionNodes(plan, "SortNode").length, 0, query.string + " Has no SortNode");
  };
  var hasFilterNode = function (plan,query) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 1, query.string + " Has FilterNode");
  };
  var hasNoFilterNode = function (plan,query) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 0, query.string + " Has no FilterNode");
  };
  var hasNoIndexNode = function (plan,query) {
    assertEqual(findExecutionNodes(plan, "IndexNode").length, 0, query.string + " Has no IndexNode");
  };
  var hasIndexNode = function (plan,query) {
    var rn = findExecutionNodes(plan,"IndexNode");
    assertEqual(rn.length, 1, query.string + " Has IndexNode");
    assertEqual(rn[0].indexes.length, 1);
    var indexType = rn[0].indexes[0].type;
    assertTrue(indexType === "geo1" || indexType === "geo2");
  };
  var isNodeType = function(node, type, query) {
    assertEqual(node.type, type, query.string + " check whether this node is of type " + type);
  };

  var geodistance = function(latitude1, longitude1, latitude2, longitude2) {
    var p1 = (latitude1) * (Math.PI / 180.0);
    var p2 = (latitude2) * (Math.PI / 180.0);
    var d1 = (latitude2 - latitude1) * (Math.PI / 180.0);
    var d2 = (longitude2 - longitude1) * (Math.PI / 180.0);

    var a = Math.sin(d1 / 2.0) * Math.sin(d1 / 2.0) +
            Math.cos(p1) * Math.cos(p2) *
            Math.sin(d2 / 2.0) * Math.sin(d2 / 2.0);
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1.0 - a));

    return (6371e3 * c);
  };


  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var loopto = 10;

        internal.db._drop(colName);
        geocol = internal.db._create(colName);
        geocol.ensureIndex({type:"geo", fields:["lat","lon"]});
        geocol.ensureIndex({type:"geo", fields:["geo"]});
        geocol.ensureIndex({type:"geo", fields:["loca.tion.lat","loca.tion.lon"]});
        var lat, lon;
        for (lat=-40; lat <=40 ; ++lat) {
            for (lon=-40; lon <= 40; ++lon) {
                //geocol.insert({lat,lon});
                geocol.insert({lat, lon, "geo":[lat,lon], "loca":{"tion":{ lat, lon }} });
            }
        }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(colName);
      geocol = null;
    },

    testRuleBasics : function () {
      if(enabled.basics){
        geocol.ensureIndex({ type: "hash", fields: [ "y", "z" ], unique: false });

        var queries = [
          { string  : "FOR d IN " + colName + " SORT distance(d.lat, d.lon, 0 ,0 ) ASC LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " SORT distance(d.loca.tion.lat, d.loca.tion.lon, 0 ,0 ) ASC LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " SORT distance(0, 0, d.lat,d.lon ) ASC LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " FILTER distance(0, 0, d.lat,d.lon ) < 1 LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " FILTER distance(0, 0, d.geo[0],d.geo[1] ) < 1 LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " FILTER distance(0, 0, d.lat, d.geo[1] ) < 1 LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : true,
            index   : false
          },
          { string  : "FOR d IN " + colName + " SORT distance(0, 0, d.lat, d.lon) FILTER distance(0, 0, d.lat,d.lon ) < 1 LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " SORT distance(0, 0, d.lat, d.lon) FILTER distance(0, 0, d.lat,d.lon ) < 1 LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR i in 1..2 FOR d IN " + colName + " FILTER distance(0, 0, d.lat,d.lon ) < 1 && i > 1 LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : true,
            index   : true
          }
        ];

        queries.forEach(function(query) {
          var result = AQL_EXPLAIN(query.string);

          //sort nodes
          if (query.sort) {
            hasSortNode(result,query);
          } else {
            hasNoSortNode(result,query);
          }

          //filter nodes
          if (query.filter) {
            hasFilterNode(result,query);
          } else {
            hasNoFilterNode(result,query);
          }

          if (query.index){
            hasIndexNode(result,query);
          } else {
            hasNoIndexNode(result,query);
          }

        });
      }
    }, // testRuleBasics

    testRuleRemoveNodes : function () {
      if(enabled.removeNodes){
        var queries = [
          [ "FOR d IN " + colName  + " SORT distance(d.lat,d.lon, 0 ,0 ) ASC LIMIT 5 RETURN d", false, false, false ],
          [ "FOR d IN " + colName  + " SORT distance(0, 0, d.lat,d.lon ) ASC LIMIT 5 RETURN d", false, false, false ],
          [ "FOR d IN " + colName  + " FILTER distance(0, 0, d.lat,d.lon ) < 111200 RETURN d", false, false, false ],
          [ "FOR d IN " + colName + " SORT distance(d.loca.tion.lat,d.loca.tion.lon, 0 ,0 ) ASC LIMIT 5 RETURN d", false, false, false ],
          [ "FOR d IN " + colName + " SORT distance(d.geo[0], d.geo[1], 0 ,0 ) ASC LIMIT 5 RETURN d", false, false, false ],
          [ "FOR d IN " + colName + " SORT distance(0, 0, d.geo[0], d.geo[1]) ASC LIMIT 5 RETURN d", false, false, false ],
        ];

        var expected = [
          [[0,0], [-1,0], [0,1], [1,0], [0,-1]],
          [[0,0], [-1,0], [0,1], [1,0], [0,-1]],
          [[0,0], [-1,0], [0,1], [1,0], [0,-1]],
          [[0,0], [-1,0], [0,1], [1,0], [0,-1]],
          [[0,0], [-1,0], [0,1], [1,0], [0,-1]],
          [[0,0], [-1,0], [0,1], [1,0], [0,-1]],
        ];

        queries.forEach(function(query, qindex) {
          var result = AQL_EXECUTE(query[0]);
          expect(expected[qindex].length).to.be.equal(result.json.length);
          var pairs = result.json.map(function(res){
              return [res.lat,res.lon];
          });
          assertEqual(expected[qindex].sort(),pairs.sort());
        });

      }
    }, // testRuleSort

    testRuleSorted : function(){
      if(enabled.sorted){
        var old=0;
        var query = "FOR d IN " + colName + " SORT distance(d.lat, d.lon, 0, 0) RETURN distance(d.lat, d.lon, 0, 0)";
        var result = AQL_EXECUTE(query);
        var distances = result.json.map(d => { return parseFloat(d.toFixed(5)); });
        old=0;
        distances.forEach(d => { assertTrue( d >= old); old = d; });
      }
    } //testSorted

  }; // test dictionary (return)
} // optimizerRuleTestSuite

function geoVariationsTestSuite() {
  var colName = "UnitTestsAqlOptimizerGeoIndex";

  var geocol;
  var hasIndexNode = function (plan) {
    var rn = findExecutionNodes(plan,"IndexNode");
    assertEqual(rn.length, 1);
    assertEqual(rn[0].indexes.length, 1);
    var indexType = rn[0].indexes[0].type;
    assertTrue(indexType === "geo1" || indexType === "geo2");
  };

  return {

    setUp : function () {
      internal.db._drop(colName);
      geocol = internal.db._create(colName);
      geocol.ensureIndex({ type:"geo", fields: ["location"] });
      let documents = [
        {"_key":"1138","_id":"test/1138","_rev":"_WjFfhsm---","location":[11,0]},
        {"_key":"1232","_id":"test/1232","_rev":"_WjFgKfC---","location":[0,0]},
        {"_key":"1173","_id":"test/1173","_rev":"_WjFfvBC---","location":[10,10]},
        {"_key":"1197","_id":"test/1197","_rev":"_WjFf9AC---","location":[0,50]},
        {"_key":"1256","_id":"test/1256","_rev":"_WjFgVtC---","location":[10,10]}
      ];
      geocol.insert(documents);
    },

    tearDown : function () {
      internal.db._drop(colName);
    },

    testQueries : function () {
      // all of these queries should produce exactly the same result, with and without optimizations
      let queries = [
        "FOR e IN " + colName + " FILTER @2 <= distance(e.location[0], e.location[1], @0, @1) FILTER distance(e.location[0], e.location[1], @0, @1) <= @3 SORT distance(e.location[0], e.location[1], 0.000000, 0.000000) RETURN e._key",
        "FOR e IN " + colName + " FILTER @2 <= distance(e.location[0], e.location[1], @0, @1) AND distance(e.location[0], e.location[1], @0, @1) <= @3 SORT distance(e.location[0], e.location[1], 0.000000, 0.000000) RETURN e._key",
        "FOR e IN " + colName + " FILTER distance(e.location[0], e.location[1], @0, @1) <= @3 FILTER @2 <= distance(e.location[0], e.location[1], @0, @1) SORT distance(e.location[0], e.location[1], 0.000000, 0.000000) RETURN e._key",
        "FOR e IN " + colName + " FILTER distance(e.location[0], e.location[1], @0, @1) <= @3 AND @2 <= distance(e.location[0], e.location[1], @0, @1) SORT distance(e.location[0], e.location[1], 0.000000, 0.000000) RETURN e._key"
      ];

      let bind = { "0": 0, "1": 0, "2": 1000000, "3": 5000000 }; 
      let noOpt = { optimizer:  { rules: ["-all"] } }; 
      let doOpt = { optimizer: { rules: ["+all"] } }; 

      queries.forEach(function(q) {
        let result = AQL_EXECUTE(q, bind, noOpt);
        assertEqual([ "1138", "1173", "1256" ], result.json);
        
        result = AQL_EXECUTE(q, bind, doOpt);
        assertEqual([ "1138", "1173", "1256" ], result.json);
     
        let plan = AQL_EXPLAIN(q, bind);
        hasIndexNode(plan);       
      });
    }
  };
} 

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);
jsunity.run(geoVariationsTestSuite);

return jsunity.done();
