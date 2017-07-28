/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, JSON */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for foxx manager
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Simon Grätzer
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var graph_module = require("@arangodb/general-graph");
var internal = require("internal");
var console = require("console");
var EPS = 0.0001;

var graphName = "UnitTest_pregel";
var vColl = "UnitTest_pregel_v", eColl = "UnitTest_pregel_e";

function testAlgo(a, p) {
  var key = db._pregelStart(a, vColl, eColl, p);
  var i = 10000;
  do {
    internal.wait(0.2);
    var stats = db._pregelStatus(key);
    if (stats.state !== "running") {
      assertEqual(stats.vertexCount, 11, stats);
      assertEqual(stats.edgeCount, 17, stats);

      db[vColl].all().toArray()
      .forEach(function(d) {
                 if (d[a] && d[a] !== -1) {
                   var diff = Math.abs(d[a] - d.result);
                   if (diff > EPS) {
                     console.log("Error on " + JSON.stringify(d));
                     assertTrue(false);// peng
                   }
                 }
               });
      break;
    }
  } while(i-- >= 0);
  if (i === 0) {
    assertTrue(false, "timeout in pregel execution");
  }
}

function basicTestSuite () {
  'use strict';
  return {
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////
    
    setUp : function () {
      
      var exists = graph_module._list().indexOf("demo") !== -1;
      if (exists || db.demo_v) {
        return;
      }
      var graph = graph_module._create(graphName);
      db._create(vColl, {numberOfShards: 4});
      graph._addVertexCollection(vColl);
      db._createEdgeCollection(eColl, {
                               numberOfShards: 4,
                               replicationFactor: 1,
                               shardKeys:["vertex"],
                               distributeShardsLike:vColl});
      
      var rel = graph_module._relation(eColl, [vColl], [vColl]);
      graph._extendEdgeDefinitions(rel);
      
      var vertices = db[vColl];
      var edges = db[eColl];
    
    
      var A = vertices.insert({_key:'A', sssp:3, pagerank:0.027645934})._id;
      var B = vertices.insert({_key:'B', sssp:2, pagerank:0.3241496})._id;
      var C = vertices.insert({_key:'C', sssp:3, pagerank:0.289220})._id;
      var D = vertices.insert({_key:'D', sssp:2, pagerank:0.0329636})._id;
      var E = vertices.insert({_key:'E', sssp:1, pagerank:0.0682141})._id;
      var F = vertices.insert({_key:'F', sssp:2, pagerank:0.0329636})._id;
      var G = vertices.insert({_key:'G', sssp:-1, pagerank:0.0136363})._id;
      var H = vertices.insert({_key:'H', sssp:-1, pagerank:0.01363636})._id;
      var I = vertices.insert({_key:'I', sssp:-1, pagerank:0.01363636})._id;
      var J = vertices.insert({_key:'J', sssp:-1, pagerank:0.01363636})._id;
      var K = vertices.insert({_key:'K', sssp:0, pagerank:0.013636363})._id;

      edges.insert({_from:B, _to:C, vertex:'B'});
      edges.insert({_from:C, _to:B, vertex:'C'});
      edges.insert({_from:D, _to:A, vertex:'D'});
      edges.insert({_from:D, _to:B, vertex:'D'});
      edges.insert({_from:E, _to:B, vertex:'E'});
      edges.insert({_from:E, _to:D, vertex:'E'});
      edges.insert({_from:E, _to:F, vertex:'E'});
      edges.insert({_from:F, _to:B, vertex:'F'});
      edges.insert({_from:F, _to:E, vertex:'F'});
      edges.insert({_from:G, _to:B, vertex:'G'});
      edges.insert({_from:G, _to:E, vertex:'G'});
      edges.insert({_from:H, _to:B, vertex:'H'});
      edges.insert({_from:H, _to:E, vertex:'H'});
      edges.insert({_from:I, _to:B, vertex:'I'});
      edges.insert({_from:I, _to:E, vertex:'I'});
      edges.insert({_from:J, _to:E, vertex:'J'});
      edges.insert({_from:K, _to:E, vertex:'K'});
    },
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////
    
    tearDown : function () {
      graph_module._drop(graphName, true);
    },
    
    testSSSPNormal: function () {
      testAlgo("sssp", {async:false, source:vColl + "/K"});
    },
    
    testSSSPAsync: function () {
      testAlgo("sssp", {async:true, source:vColl + "/K"});
    },
    
    testPageRank: function () {
      // should test correct convergence behaviour, might fail if EPS is too low
      testAlgo("pagerank", {threshold:EPS / 10});
    },
  };
};


function exampleTestSuite () {
  'use strict';
  return {
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////
    
    setUp : function () {
      var examples = require("@arangodb/graph-examples/example-graph.js"); 
      var graph = examples.loadGraph("social");
    },
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////
    
    tearDown : function () {
      graph_module._drop("social", true);
    },
    
    testSocial: function () {
      var key = db._pregelStart("effectivecloseness", 
                                ['female', 'male'], ['relation'], 
                                {resultField: "closeness"});
      var i = 10000;
      do {
        internal.wait(0.2);
        var stats = db._pregelStatus(key);
        if (stats.state !== "running") {
          assertEqual(stats.vertexCount, 4, stats);
          assertEqual(stats.edgeCount, 4, stats);
          break;
        }
      } while(i-- >= 0);
      if (i === 0) {
        assertTrue(false, "timeout in pregel execution");
      }
    }
  };
};

jsunity.run(basicTestSuite);
jsunity.run(exampleTestSuite);

return jsunity.done();
