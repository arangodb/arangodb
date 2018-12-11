/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

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
var db = require('internal').db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function legacyOptimizerRuleTestSuite() {
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
    assertTrue(indexType === "geo1" || indexType === "geo2" || indexType === "geo");
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
        geocol.ensureIndex({type:"geo2", fields:["lat","lon"]});
        geocol.ensureIndex({type:"geo1", fields:["geo"]});
        geocol.ensureIndex({type:"geo2", fields:["loca.tion.lat","loca.tion.lon"]});
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

    testLegacyRuleBasics : function () {
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
            let cluster = require('@arangodb/cluster');
            if (!cluster.isCoordinator()) {
              hasNoSortNode(result,query);
            }
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

    testLegacyRuleRemoveNodes : function () {
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

    testLegacyRuleSorted : function(){
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

function legacyGeoVariationsTestSuite() {
  var colName = "UnitTestsAqlOptimizerGeoIndex";

  var geocol;
  var hasIndexNode = function (plan) {
    var rn = findExecutionNodes(plan,"IndexNode");
    assertEqual(rn.length, 1);
    assertEqual(rn[0].indexes.length, 1);
    var indexType = rn[0].indexes[0].type;
    assertTrue(indexType === "geo1" || indexType === "geo2" || indexType === "geo");
  };

  return {

    setUp : function () {
      internal.db._drop(colName);
      geocol = internal.db._create(colName);
      geocol.ensureIndex({ type:"geo1", fields: ["location"] });
      let documents = [
        {"_key":"1138","_id":"test/1138","_rev":"_WjFfhsm---","location":[11,0]},
        {"_key":"1232","_id":"test/1232","_rev":"_WjFgKfC---","location":[0,0]},
        {"_key":"1173","_id":"test/1173","_rev":"_WjFfvBC---","location":[10,10]},
        {"_key":"1197","_id":"test/1197","_rev":"_WjFf9AC---","location":[0,50]},
        {"_key":"1256","_id":"test/1256","_rev":"_WjFgVtC---","location":[10,10.1]}
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
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite() {
  // quickly disable tests here
  var enabled = {
    basics: true,
    removeNodes: true,
    sorted: true
  };

  var ruleName = "geoindex";
  var colName = "UnitTestsAqlOptimizer" + ruleName.replace(/-/g, "_");
  var colName2 = colName2;

  var sortArray = function (l, r) {
    if (l[0] !== r[0]) {
      return l[0] < r[0] ? -1 : 1;
    }
    if (l[1] !== r[1]) {
      return l[1] < r[1] ? -1 : 1;
    }
    return 0;
  };
  var hasSortNode = function (plan, query) {
    assertEqual(findExecutionNodes(plan, "SortNode").length, 1, query.string + " Has SortNode ");
  };
  var hasNoSortNode = function (plan, query) {
    assertEqual(findExecutionNodes(plan, "SortNode").length, 0, query.string + " Has no SortNode");
  };
  var hasFilterNode = function (plan, query) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 1, query.string + " Has FilterNode");
  };
  var hasNoFilterNode = function (plan, query) {
    assertEqual(findExecutionNodes(plan, "FilterNode").length, 0, query.string + " Has no FilterNode");
  };
  var hasNoIndexNode = function (plan, query) {
    assertEqual(findExecutionNodes(plan, "IndexNode").length, 0, query.string + " Has no IndexNode");
  };
  var hasNoResultsNode = function (plan, query) {
    assertEqual(findExecutionNodes(plan, "NoResultsNode").length, 1, query.string + " Has NoResultsNode");
  };
  var hasCalculationNodes = function (plan, query, countXPect) {
    assertEqual(findExecutionNodes(plan, "CalculationNode").length,
      countXPect, "Has " + countXPect + " CalculationNode");
  };
  var hasIndexNode = function (plan, query) {
    var rn = findExecutionNodes(plan, "IndexNode");
    assertEqual(rn.length, 1, query.string + " Has IndexNode");
    assertEqual(rn[0].indexes.length, 1);
    var indexType = rn[0].indexes[0].type;
    assertTrue(indexType === "geo");
  };
  var isNodeType = function (node, type, query) {
    assertEqual(node.type, type, query.string + " check whether this node is of type " + type);
  };

  var geocol;
  let locations;

  // GeoJSON test data. https://gist.github.com/aaronlidman/7894176?short_path=2b56a92
  // Mostly from the spec: http://geojson.org/geojson-spec.html.
  // stuff over Java island
  let indonesia = [
    { "type": "Polygon", "coordinates": [[[100.0, 0.0], [101.0, 0.0], [101.0, 1.0], [100.0, 1.0], [100.0, 0.0]]] },
    { "type": "LineString", "coordinates": [[102.0, 0.0], [103.0, 1.0], [104.0, 0.0], [105.0, 1.0]] },
    { "type": "Point", "coordinates": [102.0, 0.5] }];
  let indonesiaKeys = [];

  // EMEA region
  let emea = [ // TODO implement multi-polygon
    /*{ "type": "MultiPolygon", "coordinates": [ [[[40, 40], [20, 45], [45, 30], [40, 40]]],
      [[[20, 35], [10, 30], [10, 10], [30, 5], [45, 20], [20, 35]],  [[30, 20], [20, 15], [20, 25], [30, 20]]]] }*/
    { "type": "Polygon", "coordinates": [[[35, 10], [45, 45], [15, 40], [10, 20], [35, 10]], [[20, 30], [35, 35], [30, 20], [20, 30]]] },
    { "type": "LineString", "coordinates": [[25, 10], [10, 30], [25, 40]] },
    { "type": "MultiLineString", "coordinates": [[[10, 10], [20, 20], [10, 40]], [[40, 40], [30, 30], [40, 20], [30, 10]]] },
    { "type": "MultiPoint", "coordinates": [[10, 40], [40, 30], [20, 20], [30, 10]] }
  ];
  let emeaKeys = [];
  let rectEmea1 = { "type": "Polygon", "coordinates": [[[2, 4], [26, 4], [26, 49], [2, 49], [2, 4]]] };
  let rectEmea2 = { "type": "Polygon", "coordinates": [[[35, 42], [49, 42], [49, 50], [35, 51], [35, 42]]] };
  let rectEmea3 = { "type": "Polygon", "coordinates": [[[35, 42], [49, 42], [49, 50], [35, 50], [35, 42]]] };


  return {

    ////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////
    setUp: function () {
      var loopto = 10;

      internal.db._drop(colName);
      geocol = internal.db._create(colName);
      geocol.ensureIndex({ type: "geo", fields: ["lat", "lon"], geoJson: false, legacy: false });
      geocol.ensureIndex({ type: "geo", fields: ["geo"], geoJson: false, legacy: false });
      geocol.ensureIndex({ type: "geo", fields: ["loca.tion.lat", "loca.tion.lon"], geoJson: false, legacy: false });
      var lat, lon;
      for (lat = -40; lat <= 40; ++lat) {
        for (lon = -40; lon <= 40; ++lon) {
          //geocol.insert({lat,lon});
          geocol.insert({ lat, lon, "geo": [lat, lon], "loca": { "tion": { lat, lon } } });
        }
      }

      db._drop("UnitTestsGeoJsonTestSuite");
      locations = db._create("UnitTestsGeoJsonTestSuite");
      locations.ensureIndex({ type: "geo", fields: ["geometry"], geoJson: true, legacy: false });

      indonesiaKeys = indonesia.map(doc => locations.save({ geometry: doc })._key);
      emeaKeys = emea.map(doc => locations.save({ geometry: doc })._key);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////
    tearDown: function () {
      internal.db._drop(colName);
      geocol = null;
      db._drop("UnitTestsGeoJsonTestSuite");
    },

    testRuleBasics: function () {
      if (enabled.basics) {
        geocol.ensureIndex({ type: "hash", fields: ["y", "z"], unique: false });

        var queries = [
          {
            string: "FOR d IN " + colName + " SORT distance(d.lat, d.lon, 0 ,0 ) ASC LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " SORT distance(d.loca.tion.lat, d.loca.tion.lon, 0 ,0 ) ASC LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " SORT distance(0, 0, d.lat,d.lon ) ASC LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " FILTER distance(0, 0, d.lat,d.lon ) < 1 LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " FILTER distance(0, 0, d.geo[0],d.geo[1] ) < 1 LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " FILTER distance(0, 0, d.lat, d.geo[1] ) < 1 LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: true,
            index: false
          },
          {
            string: "FOR d IN " + colName + " SORT distance(0, 0, d.lat, d.lon) FILTER distance(0, 0, d.lat,d.lon ) < 1 LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " SORT distance(0, 0, d.lat, d.lon) FILTER distance(0, 0, d.lat,d.lon ) < 1 LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR i in 1..2 FOR d IN " + colName + " FILTER distance(0, 0, d.lat,d.lon ) < 1 && i > 1 LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: true,
            index: true
          }
        ];

        queries.forEach(function (query) {
          var result = AQL_EXPLAIN(query.string);

          //sort nodes
          if (query.sort) {
            hasSortNode(result, query);
          } else {
            let cluster = require('@arangodb/cluster');
            if (!cluster.isCoordinator()) {
              hasNoSortNode(result, query);
            }
          }

          //filter nodes
          if (query.filter) {
            hasFilterNode(result, query);
          } else {
            hasNoFilterNode(result, query);
          }

          if (query.index) {
            hasIndexNode(result, query);
          } else {
            hasNoIndexNode(result, query);
          }

        });
      }
    }, // testRuleBasics

    testRuleRemoveNodes: function () {
      if (enabled.removeNodes) {
        var queries = [
          ["FOR d IN " + colName + " SORT distance(d.lat,d.lon, 0 ,0 ) ASC LIMIT 5 RETURN d", false, false, false],
          ["FOR d IN " + colName + " SORT distance(0, 0, d.lat,d.lon ) ASC LIMIT 5 RETURN d", false, false, false],
          ["FOR d IN " + colName + " FILTER distance(0, 0, d.lat,d.lon ) < 111200 RETURN d", false, false, false],
          ["FOR d IN " + colName + " SORT distance(d.loca.tion.lat,d.loca.tion.lon, 0 ,0 ) ASC LIMIT 5 RETURN d", false, false, false],
          ["FOR d IN " + colName + " SORT distance(d.geo[0], d.geo[1], 0 ,0 ) ASC LIMIT 5 RETURN d", false, false, false],
          ["FOR d IN " + colName + " SORT distance(0, 0, d.geo[0], d.geo[1]) ASC LIMIT 5 RETURN d", false, false, false],
        ];

        var expected = [
          [[0, 0], [-1, 0], [0, 1], [1, 0], [0, -1]],
          [[0, 0], [-1, 0], [0, 1], [1, 0], [0, -1]],
          [[0, 0], [-1, 0], [0, 1], [1, 0], [0, -1]],
          [[0, 0], [-1, 0], [0, 1], [1, 0], [0, -1]],
          [[0, 0], [-1, 0], [0, 1], [1, 0], [0, -1]],
          [[0, 0], [-1, 0], [0, 1], [1, 0], [0, -1]],
        ];

        queries.forEach(function (query, qindex) {
          var result = AQL_EXECUTE(query[0]);
          expect(expected[qindex].length).to.be.equal(result.json.length, query[0]);
          var pairs = result.json.map(function (res) {
            return [res.lat, res.lon];
          });

          assertEqual(pairs.sort(), expected[qindex].sort(), query[0]);
        });

      }
    }, // testRuleSort

    testRuleSorted: function () {
      if (enabled.sorted) {
        const internal = require('internal');
        var old = 0;
        var query = "FOR d IN " + colName + " SORT distance(d.lat, d.lon, 0, 0) ASC RETURN distance(d.lat, d.lon, 0, 0)";
        var result = AQL_EXECUTE(query);
        var distances = result.json.map(d => { return parseFloat(d.toFixed(5)); });
        old = 0;
        distances.forEach(d => {
          assertTrue(d >= old, d + " >= " + old);
          old = d;
        });
      }
    }, //testSorted

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////

    testContainsCircle1: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_DISTANCE([102, 0], x.geometry) <= 450000
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere (without circle edge included)
    ////////////////////////////////////////////////////////////////////////////

    testContainsCircle2: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_DISTANCE([101, 0], x.geometry) < 283439.318405
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////

    testContainsCircle3: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_DISTANCE([101, 0], x.geometry) <= 100000
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////

    testContainsCircle4: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_DISTANCE([101, 0], x.geometry) <= 100000
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on sphere (a donut)
    ////////////////////////////////////////////////////////////////////////////////

    testContainsAnnulus: function () {
      let query = {
        string: `FOR x IN @@cc
                  FILTER DISTANCE(-10, 25, x.lat, x.lon) <= 150000
                  FILTER DISTANCE(-10, 25, x.lat, x.lon) > 108500
                  RETURN x`,
        bindVars: {
          "@cc": geocol.name(),
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on a sphere (a donut) with nested FOR loops
    ////////////////////////////////////////////////////////////////////////////////

    testContainsAnnulusReference: function () {
      let query = {
        string: `LET latLng = [25, -10]
                 FOR x IN @@cc
                    FILTER GEO_DISTANCE(latLng, [x.lon, x.lat]) <= 150000
                    FILTER GEO_DISTANCE(latLng, [x.lon, x.lat]) > 109545
                    SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": geocol.name(),
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on a sphere (a donut) with nested FOR loops
    ////////////////////////////////////////////////////////////////////////////////

    testContainsAnnulusNested: function () {
      let query = {
        string: `FOR latLng IN [[25, -10], [150, 70]]
                  FOR x IN @@cc
                    FILTER GEO_DISTANCE(latLng, [x.lon, x.lat]) <= 150000
                    FILTER GEO_DISTANCE(latLng, [x.lon, x.lat]) > 109545
                    SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": geocol.name(),
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test simple rectangle contains
    ////////////////////////////////////////////////////////////////////////////

    testContainsPolygon1: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_CONTAINS(@poly, x.geometry)
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea1
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test simple rectangle contains
    ////////////////////////////////////////////////////////////////////////////

    /*testContainsPolygon2: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_CONTAINS(@poly, x.geometry)
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea2
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },*/

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test simple rectangle contains
    ////////////////////////////////////////////////////////////////////////////

    testIntersectsPolygon1: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_INTERSECTS(@poly, x.geometry)
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea1
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test simple rectangle contains
    ////////////////////////////////////////////////////////////////////////////

    testIntersectsPolygon2: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_INTERSECTS(@poly, x.geometry)
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea2
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test simple rectangle contains
    ////////////////////////////////////////////////////////////////////////////

    testIntersectsPolygon3: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_INTERSECTS(@poly, x.geometry)
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea3
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    testSortDistance: function () {
      var query = {
        string: `
          FOR x IN @@cc
            SORT GEO_DISTANCE([102, 0], x.geometry)
            LIMIT 1
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      let cluster = require('@arangodb/cluster');
      if (!cluster.isCoordinator()) {
        hasNoSortNode(result, query);
      }
    },

    testSortContains: function () {
      var query = {
        string: `
          FOR x IN @@cc
            SORT GEO_CONTAINS(@poly, x.geometry)
            LIMIT 1
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea1
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasNoIndexNode(result, query);
      hasSortNode(result, query);
    },

    testSortIntersects: function () {
      var query = {
        string: `
          FOR x IN @@cc
            SORT GEO_INTERSECTS(@poly, x.geometry)
            LIMIT 1
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "poly": rectEmea3
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasNoIndexNode(result, query);
      hasSortNode(result, query);
    },

    testContainsGeoConstructorPolygon1: function () {
      var query = {
        string: `
          LET poly = GEO_POLYGON(@coords)
          FOR x IN @@cc
            FILTER GEO_CONTAINS(poly, x.geometry)
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "coords": rectEmea1.coordinates
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    testContainsGeoConstructorPolygon2: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_CONTAINS(GEO_POLYGON(@coords), x.geometry)
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "coords": rectEmea1.coordinates
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    testIntersectsGeoConstructorPolygon1: function () {
      var query = {
        string: `
          LET poly = GEO_POLYGON(@coords)
          FOR x IN @@cc
            FILTER GEO_INTERSECTS(poly, x.geometry)
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "coords": rectEmea1.coordinates
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    },

    testIntersectsGeoConstructorPolygon2: function () {
      var query = {
        string: `
          FOR x IN @@cc
            FILTER GEO_INTERSECTS(GEO_POLYGON(@coords), x.geometry)
            RETURN x._key`,
        bindVars: {
          "@cc": locations.name(),
          "coords": rectEmea1.coordinates
        }
      };

      var result = AQL_EXPLAIN(query.string, query.bindVars);
      hasIndexNode(result, query);
      hasNoFilterNode(result, query);
    }

  }; // test dictionary (return)
} // optimizerRuleTestSuite

jsunity.run(legacyOptimizerRuleTestSuite);
jsunity.run(legacyGeoVariationsTestSuite);
jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
