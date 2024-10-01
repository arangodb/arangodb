/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertMatch, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Anthony Mahanna
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const gm = require("@arangodb/general-graph");
const _ = require("lodash");
const internal = require('internal');

const graphName = "UnitTestGraph";
const vName = "UnitTestVertices";
const e1Name = "UnitTestEdges1";
const e2Name = "UnitTestEdges2";
const source = `${vName}/source`;
const target = `${vName}/target`;
const badTarget = `${vName}/badTarget`;
const looper = `${vName}/looper`;

const isPathValid = (path, length, allowInbound = false) => {
  assertTrue(_.isObject(path));
  // Assert all attributes are present
  assertTrue(path.hasOwnProperty("vertices"));
  assertTrue(path.hasOwnProperty("edges"));
  // Assert weight and number of edges are correct
  const {vertices, edges} = path;
  assertEqual(edges.length, length);
  assertEqual(edges.length + 1, vertices.length);

  // Assert that source and target are correct
  assertEqual(vertices[0]._id, source);
  assertEqual(vertices[vertices.length - 1]._id, target);

  // Assert that the edges and vertices are in correct order
  // And assert that we have not found a vertex twice
  const seenVertices = new Set();
  for (let i = 0; i < vertices.length - 1; ++i) {
    const from = vertices[i]._id;
    assertFalse(seenVertices.has(from), `Found vertex ${from} twice on path: ${path.vertices.map(v => v._id)}`);
    seenVertices.add(from);

    const to = vertices[i + 1]._id;
    // Do not ADD to, it will be added next step, just make sure to test both
    // for the ends
    assertFalse(seenVertices.has(to), `Found vertex ${to} twice on path: ${path.vertices.map(v => v._id)}`);
    const e = edges[i];
    if (e._from === from) {
      // OUTBOUND EDGE
      assertEqual(e._to, to);
    } else {
      // INBOUND EDGE
      assertEqual(e._to, from);
      assertEqual(e._from, to);
      assertTrue(allowInbound);
    }

  }
};

const allPathsDiffer = (paths) => {
  const seenPath = new Set();
  for (const p of paths) {
    const stringP = JSON.stringify(p);
    assertFalse(seenPath.has(stringP), `Found path ${stringP} twice ${JSON.stringify(paths, null, 2)}`);
    seenPath.add(stringP);
  }
};

const tearDownAll = () => {
  try {
    gm._drop(graphName, true);
  } catch (e) {
    // Don't care for error, we might runinitially with no graph exist
  }
  db._drop(e2Name);
  db._drop(e1Name);
  db._drop(vName);
};


const createGraph = () => {
  gm._create(graphName, [
      gm._relation(e1Name, vName, vName),
      gm._relation(e2Name, vName, vName)
    ], [],
    {
      numberOfShards: 9
    }
  );

  const vertices = [];
  const e1s = [];
  const e2s = [];
  vertices.push({
    _key: "source"
  });
  vertices.push({
    _key: "target"
  });
  vertices.push({
    _key: "badTarget"
  });
  vertices.push({_key: "looper"});

  // Insert Data:

  // Pathnum 0 - 2 are relevant for result.
  // Pathnum 3 - 5 are noise on the start vertex
  // Pathnum 6 - 8 are noise on the target vertex
  // Pathnum 9 - 11 are noise on the isolated vertex

  for (let pathNum = 0; pathNum < 12; ++pathNum) {
    const weight = (pathNum + 1) * (pathNum + 1); // TODO: Introduce weighted ALL_SHORTEST_PATHS
    for (let step = 0; step < 3; ++step) {
      const key = `vertex_${pathNum}_${step}`;
      vertices.push({_key: key});
      // Add valid edges:
      switch (step) {
        case 0: {
          if (pathNum < 6) {
            // source -> v
            e1s.push({_from: source, _to: `${vName}/${key}`, weight});
            if (pathNum < 3) {
              // Add INBOUND shortcut 0 <- 2 in e2 (we intentionally go to path 6-8 to not interfer with the original paths)
              e2s.push({_from: `${vName}/vertex_${pathNum + 6}_0`, _to: `${vName}/${key}`, weight});
            }
          } else if (pathNum < 9) {
            // v -> target
            e1s.push({_from: `${vName}/${key}`, _to: target, weight});
          } else {
            // v-> bad
            e1s.push({_from: `${vName}/${key}`, _to: badTarget, weight});
          }
          break;
        }
        case 1: {
          // connect to step 0
          e1s.push({_from: `${vName}/vertex_${pathNum}_0`, _to: `${vName}/${key}`, weight});
          const mod = pathNum % 3;
          if (mod !== 0) {
            // Connect to the path before
            e1s.push({_from: `${vName}/vertex_${pathNum - 1}_0`, _to: `${vName}/${key}`, weight});
          }
          if (mod !== 2) {
            // Connect to the path after
            e1s.push({_from: `${vName}/vertex_${pathNum + 1}_0`, _to: `${vName}/${key}`, weight});
          }
          if (mod === 2 && pathNum === 2) {
            // Add a path loop and a duplicate edge
            // duplicate edge
            e1s.push({_from: `${vName}/vertex_${pathNum}_0`, _to: `${vName}/${key}`, weight: weight + 1});
            e1s.push({_from: `${vName}/${key}`, _to: looper, weight});
            e1s.push({_from: looper, _to: `${vName}/vertex_${pathNum}_0`, weight});
          }
          break;
        }
        case 2: {
          if (pathNum < 3) {
            // These are the valid paths we care for
            if (pathNum === 1) {
              const additional = `vertex_${pathNum}_3`;
              vertices.push({_key: additional});
              // Add an aditional step only on the second path to have differnt path lengths
              e1s.push({_from: `${vName}/${key}`, _to: `${vName}/${additional}`, weight});
              e1s.push({_from: `${vName}/${additional}`, _to: target, weight});
            } else {
              e1s.push({_from: `${vName}/${key}`, _to: target, weight});
            }
          }
          // Always connect to source:
          // 1 -> 2 is connected in e2
          e2s.push({_from: `${vName}/vertex_${pathNum}_1`, _to: `${vName}/${key}`, weight});
          break;
        }
      }
    }
  }

  db[vName].save(vertices);
  db[e1Name].save(e1s);
  db[e2Name].save(e2s);


  // This graph has the following features:
  // we have 5 paths source -> target of length 4 (via postfix of path0 and path2)
  // we have 3 paths source -> target of length 5 (via postfix of path1)
  // we have 1 path source -> target of length 8 (S,V1_0,V2_1,Loop,V2_0,V1_1,V1_2,V1_3,T)
  // The weights are defined as (1 + pathNum)^2 on every edge (the duplicate on path2 is 10 instead of 9).
  // So we end up with weight on the following paths:

  // * 4 on path0 (4 edges)
  // * 7 path1->path0 (4 edges)
  // * 17 path0->path1 (5 edges)
  // * 20 path1 (5edges)
  // * 25 path2->path1 (5 edges)
  // * 31 path1->path2 (4 edges)
  // * 36 path2 (4 edges)
  // * 37 path2 alt(4 edges)
  // * 47 on loop path (8 edges) 


  // We hav no paths source -> badTarget
  // We have 4 paths when e2 is traversed inbound instead of outbound
  // source -e1> <e2- target of length 3
  // source -e1> loop -e1> 0_2 <e2- t of length 6

  // So we end up with weight on the following paths:
  // * 51 on path0
  // * 72 on path1
  // * 99 on path2
  // * 121 on loop
};

function allConstantWeightShortestPathTestSuite() {
  return {
    setUpAll: function () {
      tearDownAll();
      createGraph();
    },
    tearDownAll,

    testPathsExistsLimit: function () {
      const query = `
        FOR path IN OUTBOUND ALL_SHORTEST_PATHS "${source}" TO "${target}" GRAPH "${graphName}"
          LIMIT 3
          RETURN path
      `;
      const result = db._query(query).toArray();
      allPathsDiffer(result);
      assertEqual(result.length, 3);
      for (let i = 0; i < 3; ++i) {
        isPathValid(result[i], 4);
      }
    },

    testNoPathExistsLimit: function () {
      const query = `
        FOR path IN OUTBOUND ALL_SHORTEST_PATHS "${source}" TO "${badTarget}" GRAPH "${graphName}"
          LIMIT 3
          RETURN path
      `;
      const result = db._query(query).toArray();
      assertEqual(result.length, 0);
    },

    testFewerPathsThanLimit: function () {
      const query = `
        FOR path IN OUTBOUND ALL_SHORTEST_PATHS "${source}" TO "${target}" GRAPH "${graphName}"
          LIMIT 1000
          RETURN path
      `;
      const result = db._query(query).toArray();
      allPathsDiffer(result);
      assertEqual(result.length, 5);
      for (let i = 0; i < 5; ++i) {
        isPathValid(result[i], 4);
      }
    },

    testPathsExistsNoLimit: function () {
      const query = `
        FOR source IN ${vName}
          FILTER source._id == "${source}"
          FOR target IN ${vName}
            FILTER target._id == "${target}"
            FOR path IN OUTBOUND ALL_SHORTEST_PATHS source TO target GRAPH "${graphName}"
            RETURN path
      `;
      const result = db._query(query).toArray();
      allPathsDiffer(result);
      assertEqual(result.length, 5);
      for (let i = 0; i < 5; ++i) {
        isPathValid(result[i], 4);
      }
    },

    testNoPathsExistsNoLimit: function () {
      const query = `
        FOR source IN ${vName}
          FILTER source._id == "${source}"
          FOR target IN ${vName}
            FILTER target._id == "${badTarget}"
            FOR path IN OUTBOUND ALL_SHORTEST_PATHS source TO target GRAPH "${graphName}"
            RETURN path
      `;
      const result = db._query(query).toArray();
      assertEqual(result.length, 0);
    },

    testPathsSkip: function () {
      const query = `
        FOR path IN OUTBOUND ALL_SHORTEST_PATHS "${source}" TO "${target}" GRAPH "${graphName}"
          LIMIT 3, 3
          RETURN path
      `;
      const result = db._query(query).toArray();
      allPathsDiffer(result);
      assertEqual(result.length, 2);

      for (let i = 0; i < 2; ++i) {
        isPathValid(result[i], 4);
      }
    },

    testPathsSkipMoreThanExists: function () {
      const query = `
        FOR path IN OUTBOUND ALL_SHORTEST_PATHS "${source}" TO "${target}" GRAPH "${graphName}"
          LIMIT 1000, 2
          RETURN path
      `;
      const result = db._query(query).toArray();
      assertEqual(result.length, 0);
    },

    testMultiDirections: function () {
      const query = `
        WITH ${vName}
        FOR path IN OUTBOUND ALL_SHORTEST_PATHS "${source}" TO "${target}" ${e1Name}, INBOUND ${e2Name}
          RETURN path
      `;
      const result = db._query(query).toArray();
      allPathsDiffer(result);
      assertEqual(result.length, 3);
      for (let i = 0; i < 3; ++i) {
        isPathValid(result[i], 3, true);
      }
    },

    testPathDepth0: function() {
      const query = `
        WITH ${vName}
        FOR path IN OUTBOUND ALL_SHORTEST_PATHS "${source}" TO "${source}" ${e1Name}, INBOUND ${e2Name}
          RETURN path
      `;
      const result = db._query(query).toArray();
      assertEqual(result.length, 1);

      // Unique assertions for depth 0
      const path = result[0];
      assertTrue(_.isObject(path));
      assertTrue(path.hasOwnProperty("vertices"));
      assertTrue(path.hasOwnProperty("edges"));
      const {vertices, edges} = path;
      assertEqual(edges.length, 0);
      assertEqual(vertices.length, 1);
      assertEqual(vertices[0]._id, source);
    }

  };

}

function allShortestPathsSyntaxTestSuite() {
  return {
    testTooManyVariables: function () {
      const query = `
        FOR a, b IN OUTBOUND ALL_SHORTEST_PATHS "x" TO "y" GRAPH "G" RETURN a`;
      try {
        db._query(query);
        fail();
      } catch (e) {
        assertMatch(/ALL_SHORTEST_PATHS should only have a single output variable/, e.errorMessage);
      }
    }
  };
}

function allShortestPathsErrorTestSuite() {

  const graphName = "UnitTestGraph";
  const vName = "UnitTestVertices";
  const eName = "UnitTestEdges";

  const keyA = "A";
  const keyB = "B";
  const keyC = "C";
  const keyD = "D";

  function createGraph() {
    // Graph: Simple diamond
    gm._create(graphName, [gm._relation(eName, vName, vName)], [], {});

    const vertexes = [
      {_key: keyA, value: 1},
      {_key: keyB, value: 1},
      {_key: keyC, value: 1},
      {_key: keyD, value: 1}
    ];

    const edges = [
      {_from: `${vName}/${keyA}`, _to: `${vName}/${keyB}`, weight: -1},
      {_from: `${vName}/${keyB}`, _to: `${vName}/${keyD}`, weight: 1},
      {_from: `${vName}/${keyA}`, _to: `${vName}/${keyC}`, weight: 1},
      {_from: `${vName}/${keyC}`, _to: `${vName}/${keyD}`, weight: -1}
    ];

    db[vName].insert(vertexes);
    db[eName].insert(edges);
  }


  return {
    setUpAll: function () {
      createGraph();
    },

    tearDownAll: function () {
      gm._drop(graphName, true);
    },

    testAllShortestPathsNegativeDefaultEdgeWeight: function () {
      const source = `${vName}/${keyA}`;
      const target = `${vName}/${keyD}`;

      const bindVars = {
        weight: -1
      };

      const query = `
        FOR path IN OUTBOUND ALL_SHORTEST_PATHS "${source}" TO "${target}" GRAPH "${graphName}"
          OPTIONS {defaultWeight: @weight}
          LIMIT 2
          RETURN path
      `;

      try {
        let res = db._query(query, bindVars);
        fail();
      } catch (err) {
        console.warn(err);
        assertEqual(err.errorNum, internal.errors.ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT.code);
      }
    }
  };
}

jsunity.run(allShortestPathsSyntaxTestSuite);
jsunity.run(allConstantWeightShortestPathTestSuite);
jsunity.run(allShortestPathsErrorTestSuite);


return jsunity.done();
