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

function multiCollectionTestSuite() {
  'use strict';

  const numComponents = 20; // components
  const n = 200; // vertices
  const m = 300; // edges
  const cn = 10;
  const cm = cn * cn;

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
                  " vertices, " + (numComponents * (m + n)) + " edges spread " +
                  "across " + cn + " vertex collections and " + cm + " edge " +
                  "collections" );

      // var exists = graph_module._list().indexOf("random") !== -1;
      // if (exists || db.demo_v) {
      //   return;
      // }
      var graph = graph_module._create(graphName);
      const v0 = `${vColl}_0`;
      db._create(v0, { numberOfShards: 4});
      graph._addVertexCollection(v0);
      for (let i = 1; i < cn; ++i) {
        const vi = `${vColl}_${i}`;
        db._create(vi, { numberOfShards: 4, distributeShardsLike: v0 });
        graph._addVertexCollection(vi);
      }

      console.log("created graph");

      for (let i = 0; i < cn; ++i) {
        const vi = `${vColl}_${i}`;
        for (let j = 0; j < cn; ++j) {
          const vj = `${vColl}_${j}`;
          const eij = `${eColl}_${i}_${j}`;
          db._createEdgeCollection(eij, {
            numberOfShards: 4,
            replicationFactor: 1,
            shardKeys: ["vertex"],
            distributeShardsLike: v0
          });

          var rel = graph_module._relation(eij, [vi], [vj]);
          graph._extendEdgeDefinitions(rel);
        }
      }

      console.log("extended edge definition");

      for (let c = 0; c < numComponents; c++) {
        let x = 0;
        let vertices = [];
        for (let i = 0; i < cn; ++i) {
          vertices.push([]);
        }
        while (x < n) {
          const i = (x + c) % cn;
          vertices[i].push({ _key: String(c) + ":" + String(x++) });
        }
        for (let i = 0; i < cn; ++i) {
          db[`${vColl}_${i}`].insert(vertices[i]);
        }
      }

      for (let i = 0; i < cn; ++i) {
        assertEqual(db[`${vColl}_${i}`].count(), numComponents * n / cn);
      }

      console.log("Done inserting vertices, inserting edges");

      let lcg = createRand();

      for (let c = 0; c < numComponents; c++) {
        let edges = [];
        for (let i = 0; i < cn; ++i) {
          edges.push([]);
          for (let j = 0; j < cn; ++j) {
            edges[i].push([]);
          }
        }
        for (let x = 0; x < m; x++) {
          const fromId = Math.floor(lcg() * n);
          const toId = Math.floor(lcg() * n);
          const fromKey = String(c) + ":" + fromId;
          const toKey = String(c) + ":" + toId;
          const vFrom = (fromId + c) % cn;
          const vTo = (toId + c) % cn;
          const from = `${vColl}_${vFrom}/${fromKey}`;
          const to = `${vColl}_${vTo}/${toKey}`;
          edges[vFrom][vTo].push({ _from: from, _to: to, vertex: String(fromKey) });
        }
        for (let i = 0; i < cn; ++i) {
          for (let j = 0; j < cn; ++j) {
            db[`${eColl}_${i}_${j}`].insert(edges[i][j]);
          }
        }

        for (let x = 0; x < n; x++) {
          const fromId = x;
          const toId = x + 1;
          let fromKey = String(c) + ":" + x;
          let toKey = String(c) + ":" + (x + 1);
          const vFrom = (fromId + c) % cn;
          const vTo = (toId + c) % cn;
          const from = `${vColl}_${vFrom}/${fromKey}`;
          const to = `${vColl}_${vTo}/${toKey}`;
          db[`${eColl}_${vFrom}_${vTo}`].insert({ _from: from, _to: to, vertex: String(fromKey) });
        }
      }
      
      let count = 0;
      for (let i = 0; i < cn; ++i) {
        for (let j = 0; j < cn; ++j) {
          count += db[`${eColl}_${i}_${j}`].count();
        }
      }
      console.log("Got %s edges", count);
      assertEqual(count, numComponents * m + numComponents * n);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      graph_module._drop(graphName, true);
    },

    testMultiWCC: function () {
      var pid = pregel.start("wcc", graphName, { resultField: "result", store: true });
      var i = 10000;
      do {
        internal.sleep(0.2);
        let stats = pregel.status(pid);
        if (stats.state !== "running" && stats.state !== "storing") {
          assertEqual(stats.vertexCount, numComponents * n, stats);
          assertEqual(stats.edgeCount, numComponents * (m + n), stats);

          let mySet = new Set();
          for (let j = 0; j < cn; ++j) {
            let c = db[`${vColl}_${j}`].all();
            while (c.hasNext()) {
              let doc = c.next();
              assertTrue(doc.result !== undefined, doc);
              mySet.add(doc.result);
            }
          }

          assertEqual(mySet.size, numComponents);

          break;
        }
      } while (i-- >= 0);
      if (i === 0) {
        assertTrue(false, "timeout in WCC execution");
      }
    },
    
    testMultiWCCParallelism: function () {
      [ 1, 2, 4, 8 ].forEach((parallelism) => {
        let pid = pregel.start("wcc", graphName, { resultField: "result", store: true, parallelism });
        let i = 10000;
        do {
          internal.sleep(0.2);
          let stats = pregel.status(pid);
          if (stats.state !== "running" && stats.state !== "storing") {
            assertEqual(500, stats.gss);
            assertEqual(stats.vertexCount, numComponents * n, stats);
            assertEqual(stats.edgeCount, numComponents * (m + n), stats);

            let mySet = new Set();
            for (let j = 0; j < cn; ++j) {
              let c = db[`${vColl}_${j}`].all();
              while (c.hasNext()) {
                let doc = c.next();
                assertTrue(doc.result !== undefined, doc);
                mySet.add(doc.result);
              }
            }

            assertEqual(mySet.size, numComponents);

            break;
          }
        } while (i-- >= 0);
        if (i === 0) {
          assertTrue(false, "timeout in WCC execution");
        }
      });
    },
    
    testMultiWCCParallelismMemoryMapping: function () {
      [ 1, 2, 4, 8 ].forEach((parallelism) => {
        let pid = pregel.start("wcc", graphName, { resultField: "result", store: true, parallelism, useMemoryMaps: true });
        let i = 10000;
        do {
          internal.sleep(0.2);
          let stats = pregel.status(pid);
          if (stats.state !== "running" && stats.state !== "storing") {
            assertEqual(500, stats.gss);
            assertEqual(stats.vertexCount, numComponents * n, stats);
            assertEqual(stats.edgeCount, numComponents * (m + n), stats);

            let mySet = new Set();
            for (let j = 0; j < cn; ++j) {
              let c = db[`${vColl}_${j}`].all();
              while (c.hasNext()) {
                let doc = c.next();
                assertTrue(doc.result !== undefined, doc);
                mySet.add(doc.result);
              }
            }

            assertEqual(mySet.size, numComponents);

            break;
          }
        } while (i-- >= 0);
        if (i === 0) {
          assertTrue(false, "timeout in WCC execution");
        }
      });
    },

  };
}

function edgeCollectionRestrictionsTestSuite() {
  'use strict';

  const cn = 'UnitTestCollection';

  let checkResult = function(pid) {
    var i = 10000;
    do {
      internal.sleep(0.2);
      let stats = pregel.status(pid);
      if (stats.state !== "running" && stats.state !== "storing") {
        assertEqual(200, stats.vertexCount, stats);
        assertEqual(90, stats.edgeCount, stats);

        let fromComponents = {};
        let toComponents = {};
        for (let i = 0; i < 10; ++i) {
          let fromName = cn + 'VertexFrom' + i;
          let fromDocs = db._query(`FOR doc IN ${fromName} SORT doc.order RETURN doc`).toArray();
          assertEqual(10, fromDocs.length);

          fromDocs.forEach((doc, index) => {
            if (!fromComponents.hasOwnProperty(doc.result)) {
              fromComponents[doc.result] = 0;
            }
            ++fromComponents[doc.result];
          });

          let toName = cn + 'VertexTo' + i;
          let toDocs = db._query(`FOR doc IN ${toName} SORT doc.order RETURN doc`).toArray();
          assertEqual(10, toDocs.length);

          toDocs.forEach((doc, index) => {
            if (!toComponents.hasOwnProperty(doc.result)) {
              toComponents[doc.result] = 0;
            }
            ++toComponents[doc.result];
          });
        }
        assertEqual(100, Object.keys(fromComponents).length); 
        Object.keys(fromComponents).forEach((k) => {
          assertEqual(1, fromComponents[k]);
        });
        assertEqual(100, Object.keys(toComponents).length); 
        Object.keys(toComponents).forEach((k) => {
          assertEqual(1, toComponents[k]);
        });
        break;
      }
    } while (i-- >= 0);
    if (i === 0) {
      assertTrue(false, "timeout in algorithm execution");
    }
  };

  return {

    setUpAll: function () {
      try {
        db._dropDatabase('PregelTest');
      } catch (err) {}

      db._createDatabase('PregelTest');
      db._useDatabase('PregelTest');
        
      // only used as a prototype for sharding
      const numberOfShards = 3;
      db._create("proto", { numberOfShards });

      let edgeDefinitions = [];

      let vertices = [];
      for (let i = 0; i < 10; ++i) {
        vertices.push({ vertex: "test" + i, order: i });
      }
      for (let i = 0; i < 10; ++i) {
        let fromName = cn + 'VertexFrom' + i;
        let toName = cn + 'VertexTo' + i;
        let f = db._create(fromName, { numberOfShards, distributeShardsLike: 'proto', shardKeys: ["_key"] });
        let fromKeys = f.insert(vertices).map((doc) => doc._key );
        let t = db._create(toName, { numberOfShards, distributeShardsLike: 'proto', shardKeys: ["_key"] });
        let toKeys = t.insert(vertices).map((doc) => doc._key );

        let edges = [];
        for (let j = 0; j < i; ++j) {
          // requirement for connectedcomponents is that we have edges in both directions!
          edges.push({ _from: fromName + "/" + fromKeys[j], _to: toName + "/" + toKeys[j], vertex: fromKeys[j] });
          edges.push({ _to: fromName + "/" + fromKeys[j], _from: toName + "/" + toKeys[j], vertex: toKeys[j] });
        }
        let edgeName = cn + 'Edge_' + fromName + '_' + toName;
        let e = db._createEdgeCollection(edgeName, { numberOfShards, distributeShardsLike: 'proto', shardKeys: ["vertex"] });
        e.insert(edges);

        edgeDefinitions.push(graph_module._relation(edgeName, [fromName], [toName]));
      }

      graph_module._create(graphName, edgeDefinitions, [], {});
    },

    tearDownAll: function () {
      db._useDatabase('_system');
      db._dropDatabase('PregelTest');
    },

    testWithEdgeCollectionRestrictions: function () {
      let pid = pregel.start("connectedcomponents", graphName, { resultField: "result", store: true, parallelism: 4 });
      checkResult(pid);
    },
    
    testNoEdgeCollectionRestrictions: function () {
      let pid = pregel.start("connectedcomponents", graphName, { resultField: "result", store: true, edgeCollectionRestrictions: {}, parallelism: 4 });
      checkResult(pid);
    },
  };
}

jsunity.run(multiCollectionTestSuite);
jsunity.run(edgeCollectionRestrictionsTestSuite);

return jsunity.done();
