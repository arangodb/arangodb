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

function testAlgo(v, e, a, p) {  
  var key = db._pregelStart(a, v, e, p);

  var i = 1000;
  do {
    console.log("Waiting...");
    internal.wait(1);
    var doc = db._pregelStatus(key);
    if (doc.state !== "running") {
      console.log("Finished algorithm " + a);
      internal.wait(1);// wait for workers to store data

      db.demo_v.all().toArray()
      .forEach(function(d) {
                 if (d[a] !== -1) {
                   var diff = Math.abs(d[a] - d.result);
                   if (diff > EPS) {
                     console.log("Error on " + JSON.stringify(d));
                     assertTrue(false);// peng
                   }
                 }
               });
      console.log("Done executing " + a + " : " + key);
      break;
    }
  } while(i-- >= 0);
}

function clientTestSuite () {
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
      var vColl = "demo_v", eColl = "demo_e";
      var graph = graph_module._create("demo");
      db._create(vColl, {numberOfShards: 4});
      graph._addVertexCollection(vColl);
      db._createEdgeCollection(eColl, {
                               numberOfShards: 4,
                               replicationFactor: 1,
                               shardKeys:["vertex"],
                               distributeShardsLike:vColl});
      
      var rel = graph_module._relation(eColl, [vColl], [vColl]);
      graph._extendEdgeDefinitions(rel);
    
    
      db.demo_v.insert({_key:'A', sssp:3, pagerank:0.027645934});
      db.demo_v.insert({_key:'B', sssp:2, pagerank:0.3241496});
      db.demo_v.insert({_key:'C', sssp:3, pagerank:0.289220});
      db.demo_v.insert({_key:'D', sssp:2, pagerank:0.0329636});
      db.demo_v.insert({_key:'E', sssp:1, pagerank:0.0682141});
      db.demo_v.insert({_key:'F', sssp:2, pagerank:0.0329636});
      db.demo_v.insert({_key:'G', sssp:-1, pagerank:0.0136363});
      db.demo_v.insert({_key:'H', sssp:-1, pagerank:0.01363636});
      db.demo_v.insert({_key:'I', sssp:-1, pagerank:0.01363636});
      db.demo_v.insert({_key:'J', sssp:-1, pagerank:0.01363636});
      db.demo_v.insert({_key:'K', sssp:0, pagerank:0.013636363});

      db.demo_e.insert({_from:'demo_v/B', _to:'demo_v/C', vertex:'B'});
      db.demo_e.insert({_from:'demo_v/C', _to:'demo_v/B', vertex:'C'});
      db.demo_e.insert({_from:'demo_v/D', _to:'demo_v/A', vertex:'D'});
      db.demo_e.insert({_from:'demo_v/D', _to:'demo_v/B', vertex:'D'});
      db.demo_e.insert({_from:'demo_v/E', _to:'demo_v/B', vertex:'E'});
      db.demo_e.insert({_from:'demo_v/E', _to:'demo_v/D', vertex:'E'});
      db.demo_e.insert({_from:'demo_v/E', _to:'demo_v/F', vertex:'E'});
      db.demo_e.insert({_from:'demo_v/F', _to:'demo_v/B', vertex:'F'});
      db.demo_e.insert({_from:'demo_v/F', _to:'demo_v/E', vertex:'F'});
      db.demo_e.insert({_from:'demo_v/G', _to:'demo_v/B', vertex:'G'});
      db.demo_e.insert({_from:'demo_v/G', _to:'demo_v/E', vertex:'G'});
      db.demo_e.insert({_from:'demo_v/H', _to:'demo_v/B', vertex:'H'});
      db.demo_e.insert({_from:'demo_v/H', _to:'demo_v/E', vertex:'H'});
      db.demo_e.insert({_from:'demo_v/I', _to:'demo_v/B', vertex:'I'});
      db.demo_e.insert({_from:'demo_v/I', _to:'demo_v/E', vertex:'I'});
      db.demo_e.insert({_from:'demo_v/J', _to:'demo_v/E', vertex:'J'});
      db.demo_e.insert({_from:'demo_v/K', _to:'demo_v/E', vertex:'K'});
    },
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////
    
    tearDown : function () {
      graph_module._drop("demo", true);
      //db.demo_v.drop();
      //db.demo_e.drop();
    },
    
    testSSSPNormal: function () {
      testAlgo("demo_v", "demo_e", "sssp", {async:false, source:"demo_v/K"});
    },
    
    testSSSPAsync: function () {
      testAlgo("demo_v", "demo_e", "sssp", {async:true, source:"demo_v/K"});
    },
    
    testPageRank: function () {
      // should test correct convergence behaviour, might fail if EPS is too low
      testAlgo("demo_v", "demo_e", "pagerank", {threshold:EPS / 10});
    },
  };
};

jsunity.run(clientTestSuite);

return jsunity.done();
