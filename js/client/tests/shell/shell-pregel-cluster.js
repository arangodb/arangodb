/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue */
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
var pregel = require("@arangodb/pregel")
var internal = require("internal");

function testAlgo(algo, params) {
  var key = pregel.start(algo, "demo", params);
  var i = 10000;
  do {
    var doc = pregel.status(key);
    if (doc.status != "running") {
      db.twitter_v
        .all()
        .forEach(d => assertTrue(Math.abs(d[algo] - d.result) < 0.00001));
      return
    }
    internal.wait(1000);
  } while(i-- >= 0);
  
}

function clientTestSuite () {
  'use strict';
  return {
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////
    
    setUp : function () {
      var exists = graph_module._list().indexOf("demo") != -1;
      if (!exists) {
        var vColl = "demo_v", eColl = "demo_e";
        var graph = graph_module._create(gname);
        db._create(vColl, {numberOfShards: 4});
        graph._addVertexCollection(vColl);
        db._createEdgeCollection(eColl, {
                                 numberOfShards: 4,
                                 replicationFactor: repFac,
                                 shardKeys:["vertex"],
                                 distributeShardsLike:vColl});
        
        var rel = graph_module._relation(eColl, [vColl], [vColl]);
        graph._extendEdgeDefinitions(rel);
      }
      
      db.twitter_v.insert({_key:'A', sssp:3, pagerank:0.027645934});
      db.twitter_v.insert({_key:'B', sssp:2, pagerank:0.3241496});
      db.twitter_v.insert({_key:'C', sssp:3, pagerank:0.289220});
      db.twitter_v.insert({_key:'D', sssp:2, pagerank:0.0329636});
      db.twitter_v.insert({_key:'E', sssp:1, pagerank:0.0682141});
      db.twitter_v.insert({_key:'F', sssp:2, pagerank:0.0329636});
      db.twitter_v.insert({_key:'G', sssp:-1, pagerank:0.0136363});
      db.twitter_v.insert({_key:'H', sssp:-1, pagerank:0.01363636});
      db.twitter_v.insert({_key:'I', sssp:-1, pagerank:0.01363636});
      db.twitter_v.insert({_key:'J', sssp:-1, pagerank:0.01363636});
      db.twitter_v.insert({_key:'K', sssp:0, pagerank:0.013636363});

      db.twitter_e.insert({_from:'demo_v/C', _to:'demo_v/B', vertex:'C'})
      db.twitter_e.insert({_from:'demo_v/D', _to:'demo_v/A', vertex:'D'})
      db.twitter_e.insert({_from:'demo_v/D', _to:'demo_v/B', vertex:'D'})
      db.twitter_e.insert({_from:'demo_v/E', _to:'demo_v/B', vertex:'E'})
      db.twitter_e.insert({_from:'demo_v/E', _to:'demo_v/D', vertex:'E'})
      db.twitter_e.insert({_from:'demo_v/E', _to:'demo_v/F', vertex:'E'})
      db.twitter_e.insert({_from:'demo_v/F', _to:'demo_v/B', vertex:'F'})
      db.twitter_e.insert({_from:'demo_v/F', _to:'demo_v/E', vertex:'F'})
      db.twitter_e.insert({_from:'demo_v/G', _to:'demo_v/B', vertex:'G'})
      db.twitter_e.insert({_from:'demo_v/G', _to:'demo_v/E', vertex:'G'})
      db.twitter_e.insert({_from:'demo_v/H', _to:'demo_v/B', vertex:'H'})
      db.twitter_e.insert({_from:'demo_v/H', _to:'demo_v/E', vertex:'H'})
      db.twitter_e.insert({_from:'demo_v/I', _to:'demo_v/B', vertex:'I'})
      db.twitter_e.insert({_from:'demo_v/I', _to:'demo_v/E', vertex:'I'})
      db.twitter_e.insert({_from:'demo_v/J', _to:'demo_v/E', vertex:'J'})
      db.twitter_e.insert({_from:'demo_v/K', _to:'demo_v/E', vertex:'K'})
    },
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////
    
    tearDown : function () {
      graph_module._drop('demo');
    },
    
    testSSSPNormal: function () {
      testAlgo("sssp", {async:false, source:"K"});
    },
    
    testSSSPAsync: function () {
      testAlgo("sssp", {async:true, source:"K"});
    },
    
    testPageRank: function () {
      testAlgo("pagerank", {});
    },
  }
};

jsunity.run(clientTestSuite);

return jsunity.done();
