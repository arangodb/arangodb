/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, AQL_EXPLAIN */

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
/// @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const gm = require("@arangodb/general-graph");
const th = require("@arangodb/test-helper");

const graphName = "UnitTestGraph";
const vName1 = "UnitTestVertices1";
const vName2 = "UnitTestVertices2";
const eName = "UnitTestEdges";

const optimizerRuleName = "short-traversal-to-join";

const tearDownAll = () => {
  try {
    gm._drop(graphName, true);
  } catch (e) {
    // Don't care for error, we might runinitially with no graph exist
  }
  db._drop(eName);
  db._drop(vName1);
  db._drop(vName2);
};

const createGraph = () => {
  gm._create(graphName, [gm._relation(eName, vName1, vName1)
  ], [], {});

  {
    const vertices = [];
    vertices.push({ _key: "A" });
    vertices.push({ _key: "B" });
    vertices.push({ _key: "C" });
    db[vName1].save(vertices);
  }

/*  {
    const vertices = [];
    vertices.push({ _key: "1" });
    vertices.push({ _key: "2" });
    db[vName2].save(vertices);
  } */

  {
    const edges = [];
    edges.push({ _from: vName1 + "/A", _to: vName1 + "/A"});
    edges.push({ _from: vName1 + "/A", _to: vName1 + "/B"});
    edges.push({ _from: vName1 + "/B", _to: vName1 + "/doesnaeexist"});
    edges.push({ _from: vName1 + "/A", _to: vName1 + "/C"});
    db[eName].save(edges);
  }
};

function assertRuleFires(query) {
  const result = th.AQL_EXPLAIN(query);
  assertTrue(result.plan.rules.includes(optimizerRuleName),
             `[${result.plan.rules}] does not contain "${optimizerRuleName}"`);
}

function assertRuleDoesNotFire(query) {
  const result = th.AQL_EXPLAIN(query);
  assertFalse(result.plan.rules.includes(optimizerRuleName),
             `[${result.plan.rules}] contains "${optimizerRuleName}"`);
}

function getVerticesAndEdgesFromPath(path) {
  let vertices = path.vertices.map((v) => v._id);
  let edges = path.edges.map((e) => [e._from, e._to]);

  var result = [];
  for(var v = 0; v < vertices.length - 1; v++) {
    result.push(vertices[v]);
    result.push(edges[v][0]);
    result.push(edges[v][1]);
  }
  result.push(vertices[vertices.length - 1]);
  return result;
}

function assertSameResults(query) {
  const resultWith = db._query(query).toArray();
  const resultWithPaths = resultWith.map(getVerticesAndEdgesFromPath).sort();

  const resultWithout = db._query(query, {}, {optimizer: {rules: [`-${optimizerRuleName}`]}}).toArray();
  const resultWithoutPaths = resultWithout.map(getVerticesAndEdgesFromPath).sort();

  assertEqual(resultWithPaths, resultWithoutPaths, "first result with rule applied, second with rule not applied");
}

function enumeratePathsFilter() {
  var testObj = {
    setUpAll: function () {
      tearDownAll();
      createGraph();
    },
    tearDownAll,

    testRuleFires_basic: function() {
      const query = `FOR v,e,p IN 1..1 OUTBOUND "${vName1}/A" GRAPH ${graphName}
                       RETURN p`;
      assertRuleFires(query);
      assertSameResults(query);
    },
    testRuleDoesNotFire_inbound: function() {
      const query = `FOR v,e,p IN 1..1 INBOUND "${vName1}/A" GRAPH ${graphName}
                       RETURN p`;
      assertRuleDoesNotFire(query);
//      assertSameResults(query);
    },

    testRuleDoesNotFire_variableDepth: function() {
      const query = `FOR v,e,p IN 1..2 OUTBOUND "${vName1}/A" GRAPH ${graphName}
                       RETURN p`;
      assertRuleDoesNotFire(query);
    },
    testRuleDoesNotFire_directionAny: function() {
      const query = `FOR v,e,p IN 1..1 ANY "${vName1}/A" GRAPH ${graphName}
                       RETURN p`;
      assertRuleDoesNotFire(query);
    },
    testRuleDoesNotFire_depth2: function() {
      const query = `FOR v,e,p IN 2..2 OUTBOUND "${vName1}/A" GRAPH ${graphName}
                       RETURN p`;
      assertRuleDoesNotFire(query);
    },
  };

  return testObj;
}

jsunity.run(enumeratePathsFilter);
return jsunity.done();
