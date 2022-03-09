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
  var hasIndexNode = function (plan, query) {
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

    setUpAll : function () {
      var loopto = 10;

        internal.db._drop(colName);
        geocol = internal.db._create(colName);
        geocol.ensureIndex({type:"geo2", fields:["lat","lon"]});
        geocol.ensureIndex({type:"geo1", fields:["geo"]});
        geocol.ensureIndex({type:"geo2", fields:["loca.tion.lat","loca.tion.lon"]});
        var lat, lon;
        let docs = [];
        for (lat=-40; lat <=40 ; ++lat) {
            for (lon=-40; lon <= 40; ++lon) {
                //geocol.insert({lat,lon});
                docs.push({lat, lon, "geo":[lat,lon], "loca":{"tion":{ lat, lon }} });
            }
        }
        geocol.insert(docs);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.db._drop(colName);
      geocol = null;
    },

    testLegacyRuleBasics : function () {
      if (enabled.basics) {
        geocol.ensureIndex({ type: "hash", fields: [ "y", "z" ], unique: false });

        var queries = [
          { string  : "FOR d IN " + colName + " SORT distance(d.lat, d.lon, 0 ,0) ASC LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " SORT distance(d.loca.tion.lat, d.loca.tion.lon, 0 ,0) ASC LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " SORT distance(0, 0, d.lat,d.lon) ASC LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " FILTER distance(0, 0, d.lat,d.lon) < 1 LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " FILTER distance(0, 0, d.geo[0],d.geo[1]) < 1 LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR d IN " + colName + " FILTER distance(0, 0, d.lat, d.geo[1]) < 1 LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : false
          },
          { string  : "FOR d IN " + colName + " SORT distance(0, 0, d.lat, d.lon) FILTER distance(0, 0, d.lat, d.lon) < 1 LIMIT 1 RETURN d",
            cluster : false,
            sort    : false,
            filter  : false,
            index   : true
          },
          { string  : "FOR i in 1..2 FOR d IN " + colName + " FILTER distance(0, 0, d.lat, d.lon) < 1 && i > 1 LIMIT 1 RETURN d",
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
      if (enabled.removeNodes) {
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

    testLegacyRuleSorted : function() {
      if (enabled.sorted) {
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

    setUpAll : function () {
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

    tearDownAll : function () {
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
  var colName3 = "UnitTestsGeoJsonTestSuite";

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
    setUpAll: function () {
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

      db._drop(colName3);
      locations = db._create(colName3);
      locations.ensureIndex({ type: "geo", fields: ["geometry"], geoJson: true, legacy: false });

      indonesiaKeys = indonesia.map(doc => locations.save({ geometry: doc })._key);
      emeaKeys = emea.map(doc => locations.save({ geometry: doc })._key);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////
    tearDownAll: function () {
      internal.db._drop(colName);
      geocol = null;
      db._drop(colName3);
    },
    
    testRuleMultiIndexesInLoops: function () {
      let query = "FOR d1 IN " + colName + " FILTER DISTANCE(d1.lat, d1.lon, 0, 0) <= 15000 FOR d2 IN " + colName + " FILTER DISTANCE(d2.lat, d2.lon, 1, 1) <= 25000 FOR d3 IN " + colName + " FILTER DISTANCE(d3.lat, d3.lon, 2, 2) <= 35000 RETURN 1";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf("geo-index-optimizer"));

      let nodes = helper.findExecutionNodes(plan, "IndexNode");
      assertEqual(3, nodes.length);
      nodes.sort(function(l, r) {
        if (l.outVariable.name !== r.outVariable.name) {
          return (l.outVariable.name < r.outVariable.name ? -1 : 1);
        }
        return 0;
      });
      nodes.forEach(function(n, i) {
        assertEqual("IndexNode", n.type);
        assertEqual(colName, n.collection);
        assertEqual(1, n.indexes.length);
        let index = n.indexes[0];

        assertEqual("geo", index.type);
        assertEqual(["lat", "lon"], index.fields);
        
        assertFalse(n.sorted);
        assertEqual("d" + (i + 1), n.outVariable.name);
        let cond = n.condition.subNodes[0];
        assertEqual("n-ary and", cond.type);
        assertEqual(1, cond.subNodes.length);
        cond = cond.subNodes[0];
        assertEqual("compare <=", cond.type);
        assertEqual(2, cond.subNodes.length);
        assertEqual("value", cond.subNodes[1].type);
        assertEqual(15000 + i * 10000, cond.subNodes[1].value);
        cond = cond.subNodes[0];
        assertEqual("function call", cond.type);
        assertEqual("GEO_DISTANCE", cond.name);
        assertEqual(1, cond.subNodes.length);
        cond = cond.subNodes[0];
        assertEqual(cond.type, "array");
        assertEqual(2, cond.subNodes.length);
        assertEqual("value", cond.subNodes[0].subNodes[0].type);
        assertEqual("value", cond.subNodes[0].subNodes[1].type);
        assertEqual(i, cond.subNodes[0].subNodes[0].value);
        assertEqual(i, cond.subNodes[0].subNodes[1].value);
        cond = cond.subNodes[1];
        assertEqual("array", cond.type);
        assertEqual(2, cond.subNodes.length);
        assertEqual("attribute access", cond.subNodes[0].type);
        assertEqual("lon", cond.subNodes[0].name);
        assertEqual("d" + (i + 1), cond.subNodes[0].subNodes[0].name);
        assertEqual("attribute access", cond.subNodes[1].type);
        assertEqual("lat", cond.subNodes[1].name);
        assertEqual("d" + (i + 1), cond.subNodes[1].subNodes[0].name);

      });
    },
    
    testRuleMultiIndexesInSubqueries: function () {
      let query = "LET x1 = (FOR d IN " + colName + " SORT distance(d.lat, d.lon, 0, 0) LIMIT 1 RETURN d) LET x2 = (FOR d IN " + colName + " SORT distance(d.lat, d.lon, 0, 0) LIMIT 1 RETURN d) RETURN { x1, x2 }";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf("geo-index-optimizer"));

      let nodes = helper.findExecutionNodes(plan, "IndexNode");
      assertEqual(2, nodes.length);
      nodes.forEach(function(n) {
        assertEqual("IndexNode", n.type);
        assertEqual(colName, n.collection);
        assertEqual(1, n.indexes.length);
        let index = n.indexes[0];

        assertEqual("geo", index.type);
        assertEqual(["lat", "lon"], index.fields);
      });
    },

    testRuleBasics: function () {
      if (enabled.basics) {
        geocol.ensureIndex({ type: "hash", fields: ["y", "z"], unique: false });

        var queries = [
          {
            string: "FOR d IN " + colName + " SORT distance(d.lat, d.lon, 0, 0) ASC LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " SORT distance(d.loca.tion.lat, d.loca.tion.lon, 0, 0) ASC LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " SORT distance(0, 0, d.lat, d.lon) ASC LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " FILTER distance(0, 0, d.lat, d.lon) < 1 LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " FILTER distance(0, 0, d.geo[0], d.geo[1]) < 1 LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR d IN " + colName + " FILTER distance(0, 0, d.lat, d.geo[1]) < 1 LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: false
          },
          {
            string: "FOR d IN " + colName + " SORT distance(0, 0, d.lat, d.lon) FILTER distance(0, 0, d.lat, d.lon) < 1 LIMIT 1 RETURN d",
            cluster: false,
            sort: false,
            filter: false,
            index: true
          },
          {
            string: "FOR i in 1..2 FOR d IN " + colName + " FILTER distance(0, 0, d.lat, d.lon) < 1 && i > 1 LIMIT 1 RETURN d",
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
    },

    // Regression test (https://arangodb.atlassian.net/browse/AR-113):
    // Previously, the filter was moved into the index, but one access of `dist` in `dist < dist` was left, while the
    // variable obviously can't be available yet at the index node.
    // When failing, throws an error like
    //   missing variable #1 (dist) for node #8 (IndexNode) while planning registers
    testSelfReference1: function () {
      const query = {
        string: `
          FOR doc IN @@col
            LET dist = GEO_DISTANCE(doc.geo, [16, 53])
            FILTER dist < dist
            RETURN {doc, dist}
        `,
        bindVars: {'@col': colName},
      };

      const result = AQL_EXPLAIN(query.string, query.bindVars);
      hasNoIndexNode(result, query);
      hasFilterNode(result, query);
    },

    // See testSelfReference1
    testSelfReference2: function () {
      const query = {
        string: `
          FOR doc IN @@col
            LET dist = GEO_DISTANCE(doc.geo, [16, 53])
            LET dist2 = GEO_DISTANCE(doc.geo, [3, 7])
            FILTER dist < dist2
            RETURN {doc, dist, dist2}
        `,
        bindVars: {'@col': colName},
      };

      const result = AQL_EXPLAIN(query.string, query.bindVars);
      hasNoIndexNode(result, query);
      hasFilterNode(result, query);
    },

    // See testSelfReference1
    testSelfReference3: function () {
      const query = {
        string: `
          FOR doc IN @@col
            LET dist = GEO_DISTANCE(doc.geo, [16, 53])
            LET dist2 = doc.maxDist
            FILTER dist < dist2
            RETURN {doc, dist, dist2}
        `,
        bindVars: {'@col': colName},
      };

      const result = AQL_EXPLAIN(query.string, query.bindVars);
      hasNoIndexNode(result, query);
      hasFilterNode(result, query);
    },

    // See testSelfReference1
    // Note that currently, the optimizer rule does not work with `dist2` being
    // not a value (but a variable reference) in the filter expression. When
    // this changes in the future, the expectations in this test can simply be
    // changed.
    testInaccessibleReference: function () {
      const query = {
        string: `
          FOR doc IN @@col
            LET dist = GEO_DISTANCE(doc.geo, [16, 53])
            LET dist2 = NOEVAL(7)
            FILTER dist < dist2
            RETURN {doc, dist, dist2}
        `,
        bindVars: {'@col': colName},
      };

      const result = AQL_EXPLAIN(query.string, query.bindVars);
      hasNoIndexNode(result, query);
      hasFilterNode(result, query);
    },

  }; // test dictionary (return)
} // optimizerRuleTestSuite

jsunity.run(legacyOptimizerRuleTestSuite);
jsunity.run(legacyGeoVariationsTestSuite);
jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
