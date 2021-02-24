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

function componentsTestSuite() {
  'use strict';

  const numComponents = 20; // components
  const n = 200; // vertices
  const m = 300; // edges
  const problematicGraphName = 'problematic';

  // a simple LCG to create deterministic pseudorandom numbers, 
  // from https://gist.github.com/Protonk/5389384
  let createRand = function(seed) {
    const m = 25;
    const a = 11;
    const c = 17;

    let z = seed || 3;
    return function() {
      z = (a * z + c) % m;
      return z / m;
    };
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {

      console.log("Beginning to insert test data with " + (numComponents * n) + 
                  " vertices, " + (numComponents * (m + n)) + " edges");

      // var exists = graph_module._list().indexOf("random") !== -1;
      // if (exists || db.demo_v) {
      //   return;
      // }
      var graph = graph_module._create(graphName);
      db._create(vColl, { numberOfShards: 4 });
      graph._addVertexCollection(vColl);
      db._createEdgeCollection(eColl, {
        numberOfShards: 4,
        replicationFactor: 1,
        shardKeys: ["vertex"],
        distributeShardsLike: vColl
      });

      console.log("created graph");

      var rel = graph_module._relation(eColl, [vColl], [vColl]);
      graph._extendEdgeDefinitions(rel);

      console.log("extended edge definition");

      for (let c = 0; c < numComponents; c++) {
        let x = 0;
        let vertices = [];
        while (x < n) {
          vertices.push({ _key: String(c) + ":" + String(x++) });
        }
        db[vColl].insert(vertices);
      }

      assertEqual(db[vColl].count(), numComponents * n);

      console.log("Done inserting vertices, inserting edges");

      let lcg = createRand();

      for (let c = 0; c < numComponents; c++) {
        let edges = [];
        for (let x = 0; x < m; x++) {
          let fromID = String(c) + ":" + Math.floor(lcg() * n);
          let toID = String(c) + ":" + Math.floor(lcg() * n);
          let from = vColl + '/' + fromID;
          let to = vColl + '/' + toID;
          edges.push({ _from: from, _to: to, vertex: String(fromID) });
        }
        db[eColl].insert(edges);

        for (let x = 0; x < n; x++) {
          let fromID = String(c) + ":" + x;
          let toID = String(c) + ":" + (x + 1);
          let from = vColl + '/' + fromID;
          let to = vColl + '/' + toID;
          db[eColl].insert({ _from: from, _to: to, vertex: String(fromID) });
        }
      }
      
      console.log("Got %s edges", db[eColl].count());
      assertEqual(db[eColl].count(), numComponents * m + numComponents * n);

      {
        const v = 'problematic_components';
        const e = 'problematic_connections';

        const edges = [
          ["A1", "A2"],
          ["A2", "A3"],
          ["A3", "A4"],
          ["A4", "A1"],
          ["B1", "B3"],
          ["B2", "B4"],
          ["B3", "B6"],
          ["B4", "B3"],
          ["B4", "B5"],
          ["B6", "B7"],
          ["B7", "B8"],
          ["B7", "B9"],
          ["B7", "B10"],
          ["B7", "B19"],
          ["B11", "B10"],
          ["B12", "B11"],
          ["B13", "B12"],
          ["B13", "B20"],
          ["B14", "B13"],
          ["B15", "B14"],
          ["B15", "B16"],
          ["B17", "B15"],
          ["B17", "B18"],
          ["B19", "B17"],
          ["B20", "B21"],
          ["B20", "B22"],
          ["C1", "C2"],
          ["C2", "C3"],
          ["C3", "C4"],
          ["C4", "C5"],
          ["C4", "C7"],
          ["C5", "C6"],
          ["C5", "C7"],
          ["C7", "C8"],
          ["C8", "C9"],
          ["C8", "C10"],
        ];

        const vertices = new Set(edges.flat());

        try {
          graph_module._drop(problematicGraphName, true);
        } catch (err) { }

        const graph = graph_module._create(problematicGraphName, [graph_module._relation(e, v, v)]);

        vertices.forEach(vertex => {
          graph[v].save({ _key: vertex });
        });

        edges.forEach(([from, to]) => {
          graph[e].save({ _from: `${v}/${from}`, _to: `${v}/${to}` });
        });
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      graph_module._drop(graphName, true);
      graph_module._drop(problematicGraphName, true);
    },

    testWCC: function () {
      var pid = pregel.start("wcc", graphName, { resultField: "result", store: true });
      var i = 10000;
      do {
        internal.sleep(0.2);
        let stats = pregel.status(pid);
        if (stats.state !== "running" && stats.state !== "storing") {
          assertEqual(stats.vertexCount, numComponents * n, stats);
          assertEqual(stats.edgeCount, numComponents * (m + n), stats);

          let c = db[vColl].all();
          let mySet = new Set();
          while (c.hasNext()) {
            let doc = c.next();
            assertTrue(doc.result !== undefined, doc);
            mySet.add(doc.result);
          }
          assertEqual(mySet.size, numComponents);

          break;
        }
      } while (i-- >= 0);
      if (i === 0) {
        assertTrue(false, "timeout in WCC execution");
      }
    },

    testWCC2: function() {
      const v = 'problematic_components';

      if (internal.isCluster()) {
        return;
      }

      // weakly connected components algorithm
      var handle = pregel.start('wcc', problematicGraphName, {
        maxGSS: 250, resultField: 'component'
      });

      while (true) {
        var status = pregel.status(handle);
        if (status.state !== 'running') {
          console.log(status);
          break;
        } else {
          console.log('Waiting for Pregel result...');
          internal.sleep(1);
        }
      }

      const counts = db._query(
        `FOR vert IN @@v
          COLLECT component = vert.component
          WITH COUNT INTO count
          SORT count DESC
          RETURN count`,
        { "@v": v }
      ).toArray();

      assertEqual(counts.length, 3);
      assertEqual(counts[0], 22);
      assertEqual(counts[1], 10);
      assertEqual(counts[2], 4);
    }
  };
}

jsunity.run(componentsTestSuite);
return jsunity.done();
