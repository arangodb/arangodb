/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNotEqual */
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
let pregelTestHelpers = require("@arangodb/graph/pregel-test-helpers");

const graphName = "UnitTest_pregel";
const vColl = "UnitTest_pregel_v", eColl = "UnitTest_pregel_e";

function randomTestSuite() {
  'use strict';

  const n = 20000; // vertices
  const m = 300000; // edges
  
  let checkCancel = function(pid) {
    pregel.cancel(pid);
    let i = 10000;
    do {
      try {
        let stats = pregel.status(pid);
        if (pregelTestHelpers.runCanceled(stats)) {
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
      const stats = pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid);

      assertEqual(stats.vertexCount, n, stats);
      assertEqual(stats.edgeCount, m * 2, stats);
    },

    testHITS: function () {
      const opts = {
        threshold: 0.0000001, resultField: "score",
        store: true
      };
      var pid = pregel.start("hits", graphName, opts);
      const stats = pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid);

      assertEqual(stats.vertexCount, n, stats);
      assertEqual(stats.edgeCount, m * 2, stats);
    },

    testStatusWithNumericId: function () {
      let pid = pregel.start("hits", graphName, { threshold: 0.0000001, resultField: "score", store: true });
      assertTrue(typeof pid === "string");
      pregelTestHelpers.waitUntilRunFinishedSuccessfully(Number(pid));
    },
    
    testStatusWithStringId: function () {
      let pid = pregel.start("hits", graphName, { threshold: 0.0000001, resultField: "score", store: true });
      assertTrue(typeof pid === "string");
      pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid);
    },
    
    testCancelWithNumericId: function () {
      let pid = pregel.start("hits", graphName, { threshold: 0.0000001, resultField: "score", store: false });
      assertTrue(typeof pid === "string");
      pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid);
      checkCancel(Number(pid));
    },
    
    testCancelWithStringId: function () {
      let pid = pregel.start("hits", graphName, { threshold: 0.0000001, resultField: "score", store: false });
      assertTrue(typeof pid === "string");
      pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid);
      checkCancel(pid);
    },
    
    test_garbage_collects_job: function () {
      const ttl = 15;
      let pid = pregel.start("hits", graphName, { threshold: 0.0000001, resultField: "score", store: false, ttl: ttl });
      assertTrue(typeof pid === "string");
          
      let stats = pregel.status(pid);
      assertTrue(stats.hasOwnProperty('id'));
      assertEqual(stats.id, pid);
      assertEqual("_system", stats.database);
      assertEqual("hits", stats.algorithm);
      assertTrue(stats.hasOwnProperty('created'));
      assertTrue(stats.hasOwnProperty('ttl'));
      assertEqual(ttl, stats.ttl);

      stats = pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid);
      assertTrue(stats.hasOwnProperty('expires'));
      assertTrue(stats.expires > stats.created);
      pregelTestHelpers.waitForResultsBeeingGarbageCollected(pid, 2 * ttl);
    }
  };
}

jsunity.run(randomTestSuite);
return jsunity.done();
