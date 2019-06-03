/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue JSON */
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

function testAlgo(a, p) {
  var pid = pregel.start(a, graphName, p);
  var i = 10000;
  do {
    internal.wait(0.2);
    var stats = pregel.status(pid);
    if (stats.state !== "running") {
      assertEqual(stats.vertexCount, 11, stats);
      assertEqual(stats.edgeCount, 17, stats);

      db[vColl].all().toArray()
        .forEach(function (d) {
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
  } while (i-- >= 0);
  if (i === 0) {
    assertTrue(false, "timeout in pregel execution");
  }
}


function basicTestSuite() {
  'use strict';
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {

      var exists = graph_module._list().indexOf("demo") !== -1;
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

      var vertices = db[vColl];
      var edges = db[eColl];


      var A = vertices.insert({ _key: 'A', sssp: 3, pagerank: 0.027645934 })._id;
      var B = vertices.insert({ _key: 'B', sssp: 2, pagerank: 0.3241496 })._id;
      var C = vertices.insert({ _key: 'C', sssp: 3, pagerank: 0.289220 })._id;
      var D = vertices.insert({ _key: 'D', sssp: 2, pagerank: 0.0329636 })._id;
      var E = vertices.insert({ _key: 'E', sssp: 1, pagerank: 0.0682141 })._id;
      var F = vertices.insert({ _key: 'F', sssp: 2, pagerank: 0.0329636 })._id;
      var G = vertices.insert({ _key: 'G', sssp: -1, pagerank: 0.0136363 })._id;
      var H = vertices.insert({ _key: 'H', sssp: -1, pagerank: 0.01363636 })._id;
      var I = vertices.insert({ _key: 'I', sssp: -1, pagerank: 0.01363636 })._id;
      var J = vertices.insert({ _key: 'J', sssp: -1, pagerank: 0.01363636 })._id;
      var K = vertices.insert({ _key: 'K', sssp: 0, pagerank: 0.013636363 })._id;

      edges.insert({ _from: B, _to: C, vertex: 'B' });
      edges.insert({ _from: C, _to: B, vertex: 'C' });
      edges.insert({ _from: D, _to: A, vertex: 'D' });
      edges.insert({ _from: D, _to: B, vertex: 'D' });
      edges.insert({ _from: E, _to: B, vertex: 'E' });
      edges.insert({ _from: E, _to: D, vertex: 'E' });
      edges.insert({ _from: E, _to: F, vertex: 'E' });
      edges.insert({ _from: F, _to: B, vertex: 'F' });
      edges.insert({ _from: F, _to: E, vertex: 'F' });
      edges.insert({ _from: G, _to: B, vertex: 'G' });
      edges.insert({ _from: G, _to: E, vertex: 'G' });
      edges.insert({ _from: H, _to: B, vertex: 'H' });
      edges.insert({ _from: H, _to: E, vertex: 'H' });
      edges.insert({ _from: I, _to: B, vertex: 'I' });
      edges.insert({ _from: I, _to: E, vertex: 'I' });
      edges.insert({ _from: J, _to: E, vertex: 'J' });
      edges.insert({ _from: K, _to: E, vertex: 'K' });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      graph_module._drop(graphName, true);
    },

    testSSSPNormal: function () {
      testAlgo("sssp", { async: false, source: vColl + "/K", resultField: "result", store: true });
    },

    testSSSPAsync: function () {
      testAlgo("sssp", { async: true, source: vColl + "/K", resultField: "result", store: true });
    },

    testPageRank: function () {
      // should test correct convergence behaviour, might fail if EPS is too low
      testAlgo("pagerank", { threshold: EPS / 10, resultField: "result", store: true });
    },

    testPageRankMMap: function () {
      // should test correct convergence behaviour, might fail if EPS is too low
      testAlgo("pagerank", { threshold: EPS / 10, resultField: "result", store: true, useMemoryMaps: true });
    },

    testPageRankSeeded: function () {
      // test that pagerank picks the seed value
      testAlgo("pagerank", { maxGSS: 1, sourceField: "pagerank", resultField: "result", store: true });
      // since we already use converged values this should not change anything
      testAlgo("pagerank", { maxGSS: 5, sourceField: "pagerank", resultField: "result", store: true });
    },

    // test the PREGEL_RESULT AQL function
    testPageRankAQLResult: function () {
      var pid = pregel.start("pagerank", graphName, { threshold: EPS / 10, store: false });
      var i = 10000;
      do {
        internal.wait(0.2);
        var stats = pregel.status(pid);
        if (stats.state !== "running") {
          assertEqual(stats.vertexCount, 11, stats);
          assertEqual(stats.edgeCount, 17, stats);

          let vertices = db._collection(vColl);
          // no result was written to the default result field
          vertices.all().toArray().forEach(d => assertTrue(!d.result));

          let array = db._query("RETURN PREGEL_RESULT(@id)", { "id": pid }).toArray();
          assertEqual(array.length, 1);
          let results = array[0];
          assertEqual(results.length, 11);

          // verify results
          results.forEach(function (d) {
            let v = vertices.document(d._key);
            assertTrue(v !== null);
            assertTrue(Math.abs(v.pagerank - d.result) < EPS);
          });

          array = db._query("RETURN PREGEL_RESULT(@id, true)", { "id": pid }).toArray();
          assertEqual(array.length, 1);
          results = array[0];
          assertEqual(results.length, 11);

          // verify results
          results.forEach(function (d) {
            let v = vertices.document(d._key);
            assertTrue(v !== null);
            assertTrue(Math.abs(v.pagerank - d.result) < EPS);

            let v2 = db._document(d._id);
            assertEqual(v, v2);
          });

          pregel.cancel(pid); // delete contents
          internal.wait(5.0);

          array = db._query("RETURN PREGEL_RESULT(@id)", { "id": pid }).toArray();
          assertEqual(array.length, 1);
          results = array[0];
          assertEqual(results.length, 0);

          break;
        }
      } while (i-- >= 0);
      if (i === 0) {
        assertTrue(false, "timeout in pregel execution");
      }
    }
  };
};


function exampleTestSuite() {
  'use strict';
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var examples = require("@arangodb/graph-examples/example-graph.js");
      examples.loadGraph("social");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      graph_module._drop("social", true);
    },

    testSocial: function () {
      var key = db._pregelStart("effectivecloseness",
        ['female', 'male'], ['relation'],
        { resultField: "closeness" });
      var i = 10000;
      do {
        internal.wait(0.2);
        var stats = db._pregelStatus(key);
        if (stats.state !== "running") {
          assertEqual(stats.vertexCount, 4, stats);
          assertEqual(stats.edgeCount, 4, stats);
          break;
        }
      } while (i-- >= 0);
      if (i === 0) {
        assertTrue(false, "timeout in pregel execution");
      }
    }
  };
};

function randomTestSuite() {
  'use strict';

  const n = 20000; // vertices
  const m = 300000; // edges

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
        internal.wait(0.2);
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
        internal.wait(0.2);
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
    }
  };
}

jsunity.run(basicTestSuite);
jsunity.run(exampleTestSuite);
jsunity.run(randomTestSuite);


return jsunity.done();
