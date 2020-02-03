/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, graph functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const gm = require("@arangodb/general-graph");
const _ = require("underscore");

const graphName = "UnitTestGraph";
const vName = "UnitTestVertices";
const eName = "UnitTestEdges";

const tearDownAll = () => {
  try {
    gm._drop(graphName, true);
  } catch (e) {
    // Don't care for error, we might runinitially with no graph exist
  }
  db._drop(eName);
  db._drop(vName);
};

const createGraph = () => {
  gm._create(graphName, [gm._relation(eName, vName, vName)
  ], [], {});

  const vertices = [];
  const edges = [];

  for (var i = 0; i < 19; i++) {
    vertices.push({
      _key: i.toString()
    });
  }

  edges.push({ _from: `${vName}/0`, _to: `${vName}/1`, weight: 0 });
  edges.push({ _from: `${vName}/1`, _to: `${vName}/2`, weight: 1 });
  edges.push({ _from: `${vName}/2`, _to: `${vName}/3`, weight: 0 });

  edges.push({ _from: `${vName}/4`, _to: `${vName}/5`, weight: 0.5});
  edges.push({ _from: `${vName}/5`, _to: `${vName}/6`, weight: 0});
  edges.push({ _from: `${vName}/6`, _to: `${vName}/7`, weight: 2.5});

  edges.push({ _from: `${vName}/8`, _to: `${vName}/9`, weight: 0.5});
  edges.push({ _from: `${vName}/9`, _to: `${vName}/10`, weight: 0});
  edges.push({ _from: `${vName}/10`, _to: `${vName}/11`, weight: 2.5});
  edges.push({ _from: `${vName}/11`, _to: `${vName}/12`, weight: 0});
  edges.push({ _from: `${vName}/12`, _to: `${vName}/13`, weight: 1.0});
  edges.push({ _from: `${vName}/13`, _to: `${vName}/14`, weight: 2});

  edges.push({ _from: `${vName}/9`, _to: `${vName}/15`, weight: 0});
  edges.push({ _from: `${vName}/15`, _to: `${vName}/16`, weight: 1});
  edges.push({ _from: `${vName}/16`, _to: `${vName}/11`, weight: 1});

  edges.push({ _from: `${vName}/9`, _to: `${vName}/17`, weight: 0});
  edges.push({ _from: `${vName}/17`, _to: `${vName}/18`, weight: 0});
  edges.push({ _from: `${vName}/18`, _to: `${vName}/14`, weight: 1});



  db[vName].save(vertices);
  db[eName].save(edges);
};

function kZeroWeightRegressionTest() {
  return {
    setUpAll: function () {
      tearDownAll();
      createGraph();
    },
    tearDownAll,

    testPathsExists: function () {
      const query = `
        FOR path IN ANY K_SHORTEST_PATHS "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          RETURN path`;
      const result = db._query(query).toArray();
      assertEqual(result.length, 1);
      const path = result[0];
      assertEqual(path.vertices.map((v) => v._key), [ "0", "1", "2" ]);
      assertEqual(path.edges.map((e) => [e._from, e._to]), [ [`${vName}/0`, `${vName}/1`],
                                                             [`${vName}/1`, `${vName}/2`] ] );
      assertEqual(path.weight, 1);
    },

    testPathsExists2: function () {
      const query = `
        FOR path IN ANY K_SHORTEST_PATHS "${vName}/4" TO "${vName}/7" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          RETURN path`;
      const result = db._query(query).toArray();
      assertEqual(result.length, 1);
      const path = result[0];
      assertEqual(path.vertices.map((v) => v._key), [ "4", "5", "6", "7" ]);
      assertEqual(path.edges.map((e) => [e._from, e._to]), [ [`${vName}/4`, `${vName}/5`],
                                                             [`${vName}/5`, `${vName}/6`],
                                                             [`${vName}/6`, `${vName}/7`] ] );
      assertEqual(path.weight, 3);
    },

    testPathsExists3: function () {
      const query = `
        FOR path IN ANY K_SHORTEST_PATHS "${vName}/8" TO "${vName}/18" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          RETURN path`;
      const result = db._query(query).toArray();
      assertEqual(result.length, 3); 

      assertEqual(result[0].vertices.map((v) => v._key), [ "8", "9", "17", "18" ]);
      assertEqual(result[0].weight, 0.5);
      assertEqual(result[1].vertices.map((v) => v._key), [ "8", "9", "15", "16", "11", "12", "13", "14", "18" ]);
      assertEqual(result[1].weight, 6.5);
      assertEqual(result[2].vertices.map((v) => v._key), [ "8", "9", "10", "11", "12", "13", "14", "18" ]);
      assertEqual(result[2].weight, 7);
    },



  };
}

jsunity.run(kZeroWeightRegressionTest);
return jsunity.done();
