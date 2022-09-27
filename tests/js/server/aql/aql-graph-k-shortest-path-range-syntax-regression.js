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
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const gm = require("@arangodb/general-graph");

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

  db[vName].save(vertices);
  db[eName].save(edges);
};
function kShortestPathsSyntaxRegressionTest() {
  return {
    setUpAll: function () {
      tearDownAll();
      createGraph();
    },
    tearDownAll,

    testSyntaxErrorHappens: function () {
      const query = `
        FOR path IN 1..5 ANY K_SHORTEST_PATHS "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"} RETURN path`;
      try {
        const result = db._explain(query);
      } catch(err) {
        return;
      } 
      assertTrue(false, `Query "${query}" was accepted by parser, but should not have been: "IN 1..5 ANY K_SHORTEST_PATHS" is invalid syntax.`);
    },
  };
}

jsunity.run(kShortestPathsSyntaxRegressionTest);
return jsunity.done();
