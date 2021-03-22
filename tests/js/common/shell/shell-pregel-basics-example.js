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

function testAlgo(a, p) {
  let pid = pregel.start(a, graphName, p);
  assertTrue(typeof pid === "string");
  let i = 10000;
  do {
    internal.sleep(0.2);
    let stats = pregel.status(pid);
    if (stats.state !== "running") {
      assertEqual(stats.vertexCount, 11, stats);
      assertEqual(stats.edgeCount, 17, stats);
      let attrs = ["totalRuntime", "startupTime", "computationTime"];
      if (p.store && stats.hasOwnProperty('storageTime')) {
        assertEqual("number", typeof stats.storageTime);
        assertTrue(stats.storageTime >= 0.0, stats);
      }
      attrs.forEach((k) => {
        assertTrue(stats.hasOwnProperty(k), { p, stats });
        assertEqual("number", typeof stats[k]);
        assertTrue(stats[k] >= 0.0, stats);
      });

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


      vertices.insert([{ _key: 'A', sssp: 3, pagerank: 0.027645934 },
                       { _key: 'B', sssp: 2, pagerank: 0.3241496 },
                       { _key: 'C', sssp: 3, pagerank: 0.289220 },
                       { _key: 'D', sssp: 2, pagerank: 0.0329636 },
                       { _key: 'E', sssp: 1, pagerank: 0.0682141 },
                       { _key: 'F', sssp: 2, pagerank: 0.0329636 },
                       { _key: 'G', sssp: -1, pagerank: 0.0136363 },
                       { _key: 'H', sssp: -1, pagerank: 0.01363636 },
                       { _key: 'I', sssp: -1, pagerank: 0.01363636 },
                       { _key: 'J', sssp: -1, pagerank: 0.01363636 },
                       { _key: 'K', sssp: 0, pagerank: 0.013636363 }]);

      edges.insert([{ _from: vColl + '/B', _to: vColl + '/C', vertex: 'B' },
                    { _from: vColl + '/C', _to: vColl + '/B', vertex: 'C' },
                    { _from: vColl + '/D', _to: vColl + '/A', vertex: 'D' },
                    { _from: vColl + '/D', _to: vColl + '/B', vertex: 'D' },
                    { _from: vColl + '/E', _to: vColl + '/B', vertex: 'E' },
                    { _from: vColl + '/E', _to: vColl + '/D', vertex: 'E' },
                    { _from: vColl + '/E', _to: vColl + '/F', vertex: 'E' },
                    { _from: vColl + '/F', _to: vColl + '/B', vertex: 'F' },
                    { _from: vColl + '/F', _to: vColl + '/E', vertex: 'F' },
                    { _from: vColl + '/G', _to: vColl + '/B', vertex: 'G' },
                    { _from: vColl + '/G', _to: vColl + '/E', vertex: 'G' },
                    { _from: vColl + '/H', _to: vColl + '/B', vertex: 'H' },
                    { _from: vColl + '/H', _to: vColl + '/E', vertex: 'H' },
                    { _from: vColl + '/I', _to: vColl + '/B', vertex: 'I' },
                    { _from: vColl + '/I', _to: vColl + '/E', vertex: 'I' },
                    { _from: vColl + '/J', _to: vColl + '/E', vertex: 'J' },
                    { _from: vColl + '/K', _to: vColl + '/E', vertex: 'K' }]);
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
      // should test correct convergence behavior, might fail if EPS is too low
      testAlgo("pagerank", { threshold: EPS / 10, resultField: "result", store: true });
    },

    testPageRankMMap: function () {
      // should test correct convergence behavior, might fail if EPS is too low
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
        internal.sleep(0.2);
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
          internal.sleep(5.0);

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
      var key = pregel.start("effectivecloseness", {
        vertexCollections: ['female', 'male'], 
        edgeCollections: ['relation'],
      }, { resultField: "closeness" });
      assertTrue(typeof key === "string");
      var i = 10000;
      do {
        internal.sleep(0.2);
        var stats = pregel.status(key);
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

jsunity.run(basicTestSuite);
jsunity.run(exampleTestSuite);
return jsunity.done();
