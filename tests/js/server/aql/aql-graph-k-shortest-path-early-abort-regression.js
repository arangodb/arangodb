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
  gm._create(graphName, [gm._relation(eName, vName, vName)], [], {});

  const vertices = [];
  const edges = [];

  for (var i = 0; i < 9; i++) {
    vertices.push({
      _key: i.toString(),
    });
  }

  edges.push({ _from: `${vName}/0`, _to: `${vName}/1`, weight: 1000 });
  edges.push({ _from: `${vName}/0`, _to: `${vName}/4`, weight: 500 });

  edges.push({ _from: `${vName}/2`, _to: `${vName}/1`, weight: 500 });

  edges.push({ _from: `${vName}/3`, _to: `${vName}/2`, weight: 50 });
  edges.push({ _from: `${vName}/3`, _to: `${vName}/6`, weight: 200 });
  edges.push({ _from: `${vName}/3`, _to: `${vName}/7`, weight: 300 });

  edges.push({ _from: `${vName}/4`, _to: `${vName}/3`, weight: 50 });

  edges.push({ _from: `${vName}/5`, _to: `${vName}/2`, weight: 100 });

  db[vName].save(vertices);
  db[eName].save(edges);
};

function kEarlyAbortRegressionTest() {
  return {
    setUpAll: function () {
      tearDownAll();
      createGraph();
    },
    tearDownAll,

    testPathsExists: function () {
      const query = `
        FOR path IN OUTBOUND K_SHORTEST_PATHS "${vName}/0" TO "${vName}/1" GRAPH "${graphName}"
          OPTIONS {weightAttribute: "weight"}
          LIMIT 1
          RETURN path`;
      const result = db._query(query).toArray();
      assertEqual(result.length, 1);
      const path = result[0];
      assertEqual(1000, path.weight);
      assertEqual(
        ["0", "1"],
        path.vertices.map((v) => v._key)
      );
      assertEqual(
        [[`${vName}/0`, `${vName}/1`]],
        path.edges.map((e) => [e._from, e._to])
      );
    },

    testPaths: function () {
      const query = `
        FOR path IN OUTBOUND K_SHORTEST_PATHS "${vName}/0" TO "${vName}/1" GRAPH "${graphName}"
          OPTIONS {weightAttribute: "weight"}
          RETURN path.weight`;
      const result = db._query(query).toArray();
      assertEqual([1000,1100], result);
    },
  };
}

jsunity.run(kEarlyAbortRegressionTest);
return jsunity.done();
