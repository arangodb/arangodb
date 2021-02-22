/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNotEqual, JSON */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Pregel Tests
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
let pregel = require("@arangodb/pregel");

const graphName = "UnitTest_pregel";
const vColl = "UnitTest_pregel_v", eColl = "UnitTest_pregel_e";

function randomTestSuite() {
  'use strict';

  const n = 20000; // vertices
  const m = 300000; // edges
  
  let checkStatus = function(key) {
    let i = 10000;
    do {
      internal.sleep(0.2);
      let stats = pregel.status(key);
      if (stats.state !== "running") {
        break;
      }
    } while (i-- >= 0);
    if (i === 0) {
      assertTrue(false, "timeout in pregel execution");
    }
  };
  
  let checkCancel = function(key) {
    checkStatus(key);
    pregel.cancel(key);
    let i = 10000;
    do {
      try {
        let stats = pregel.status(key);
        if (stats.state === "canceled") {
          break;
        }
      } catch (err) {
        // fine.
        break;
      }
      internal.sleep(0.2);
    } while (i-- >= 0);
    if (i === 0) {
      assertTrue(false, "timeout in pregel execution");
    }
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {

      console.log("Beginning to insert test data with " + n + 
                  " vertices, " + m + " edges");

      var exists = graph_module._list().indexOf("random") !== -1;
      if (exists || db.demo_v) {
        return;
      }
      var graph = graph_module._create(graphName);
      db._create(vColl, { numberOfShards: 4 });
      graph._addVertexCollection(vColl);
      db._createEdgeCollection(eColl, {
        numberOfShards: 4,
        replicationFactor: 1,
        shardKeys: ["vertex"],
        distributeShardsLike: vColl
      });

      var rel = graph_module._relation(eColl, [vColl], [vColl]);
      graph._extendEdgeDefinitions(rel);

      let x = 0;
      while (x < n) {
        let vertices = [];
        for (let i = 0; i < 1000; i++) {
          vertices.push({ _key: String(x++) });
        }
        db[vColl].insert(vertices);
        db[vColl].count();

        if (x % 100000 === 0) {
          console.log("Inserted " + x + " vertices");
        }
      }
      assertEqual(db[vColl].count(), n);

      console.log("Done inserting vertices, inserting edges");

      x = 0;
      while (x < m) {
        let edges = [];
        for (let i = 0; i < 1000; i++) {
          let fromID = Math.floor(Math.random() * n);
          let toID = Math.floor(Math.random() * n);
          let from = vColl + '/' + fromID;
          let to = vColl + '/' + toID;
          edges.push({ _from: from, _to: to, vertex: String(fromID) });
          edges.push({ _from: to, _to: from, vertex: String(toID) });
          x++;
        }
        db[eColl].insert(edges);

        if (x % 100000 === 0) {
          console.log("Inserted " + x + " edges");
        }
      }
      assertEqual(db[eColl].count(), m * 2);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      graph_module._drop(graphName, true);
    },

    testPageRankRandom: function () {
      var pid = pregel.start("pagerank", graphName, { threshold: 0.0000001, resultField: "result", store: true });
      var i = 10000;
      do {
        internal.sleep(0.2);
        var stats = pregel.status(pid);
        if (stats.state !== "running") {
          assertEqual(stats.vertexCount, n, stats);
          assertEqual(stats.edgeCount, m * 2, stats);
          break;
        }
      } while (i-- >= 0);
      if (i === 0) {
        assertTrue(false, "timeout in pregel execution");
      }
    },

    testPageRankRandomMMap: function () {
      const opts = {
        threshold: 0.0000001, resultField: "result",
        store: true, useMemoryMaps: true
      };
      var pid = pregel.start("pagerank", graphName, opts);
      var i = 10000;
      do {
        internal.sleep(0.2);
        var stats = pregel.status(pid);
        if (stats.state !== "running") {
          assertEqual(stats.vertexCount, n, stats);
          assertEqual(stats.edgeCount, m * 2, stats);
          break;
        }
      } while (i-- >= 0);
      if (i === 0) {
        assertTrue(false, "timeout in pregel execution");
      }
    },

    testHITS: function () {
      const opts = {
        threshold: 0.0000001, resultField: "score",
        store: true
      };
      var pid = pregel.start("hits", graphName, opts);
      var i = 10000;
      do {
        internal.sleep(0.2);
        var stats = pregel.status(pid);
        if (stats.state !== "running") {
          assertEqual(stats.vertexCount, n, stats);
          assertEqual(stats.edgeCount, m * 2, stats);
          break;
        }
      } while (i-- >= 0);
      if (i === 0) {
        assertTrue(false, "timeout in pregel execution");
      }
    },

    testStatusWithNumericId: function () {
      let key = pregel.start("hits", graphName, { threshold: 0.0000001, resultField: "score", store: true });
      assertTrue(typeof key === "string");
      checkStatus(Number(key));
    },
    
    testStatusWithStringId: function () {
      let key = pregel.start("hits", graphName, { threshold: 0.0000001, resultField: "score", store: true });
      assertTrue(typeof key === "string");
      checkStatus(key);
    },
    
    testCancelWithNumericId: function () {
      let key = pregel.start("hits", graphName, { threshold: 0.0000001, resultField: "score", store: false });
      assertTrue(typeof key === "string");
      checkCancel(Number(key));
    },
    
    testCancelWithStringId: function () {
      let key = pregel.start("hits", graphName, { threshold: 0.0000001, resultField: "score", store: false });
      assertTrue(typeof key === "string");
      checkCancel(key);
    },

  };
}

jsunity.run(randomTestSuite);
return jsunity.done();
