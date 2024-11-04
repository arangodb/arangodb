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
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const gm = require("@arangodb/general-graph");
const th = require("@arangodb/test-helper");

const graphName = "UnitTestGraph";
const vName = "UnitTestVertices";
const eName = "UnitTestEdges";

const pathSearchTypes = [ "K_SHORTEST_PATHS", "K_PATHS", "ALL_SHORTEST_PATHS" ];
const optimizerRuleName = "optimize-enumerate-path-filters";

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
      _key: i.toString(),
      colour: "green"
    });
  }
  for (var j = 19; j < 29; j++) {
    vertices.push({
      _key: j.toString(),
      colour: "red"
    });
  }

  edges.push({ _from: `${vName}/0`, _to: `${vName}/1`, weight: 0, colour: "green" });
  edges.push({ _from: `${vName}/1`, _to: `${vName}/2`, weight: 1, colour: "green" });
  edges.push({ _from: `${vName}/2`, _to: `${vName}/3`, weight: 0, colour: "green" });

  edges.push({ _from: `${vName}/4`, _to: `${vName}/5`, weight: 0.5, colour: "red" });
  edges.push({ _from: `${vName}/5`, _to: `${vName}/6`, weight: 0, colour: "red" });
  edges.push({ _from: `${vName}/6`, _to: `${vName}/7`, weight: 2.5, colour: "red" });

  edges.push({ _from: `${vName}/8`, _to: `${vName}/9`, weight: 0.5, colour: "red"});
  edges.push({ _from: `${vName}/9`, _to: `${vName}/10`, weight: 0, colour: "green"});
  edges.push({ _from: `${vName}/10`, _to: `${vName}/11`, weight: 2.5, colour: "red"});
  edges.push({ _from: `${vName}/11`, _to: `${vName}/12`, weight: 0, colour: "green"});
  edges.push({ _from: `${vName}/12`, _to: `${vName}/13`, weight: 1.0, colour: "purple"});
  edges.push({ _from: `${vName}/13`, _to: `${vName}/14`, weight: 2, colour: "banana"});

  edges.push({ _from: `${vName}/9`, _to: `${vName}/15`, weight: 0, colour: "green"});
  edges.push({ _from: `${vName}/15`, _to: `${vName}/16`, weight: 1, colour: "green"});
  edges.push({ _from: `${vName}/16`, _to: `${vName}/11`, weight: 1, colour: "green"});

  edges.push({ _from: `${vName}/9`, _to: `${vName}/17`, weight: 0, colour: "purple"});
  edges.push({ _from: `${vName}/17`, _to: `${vName}/18`, weight: 0, colour: "purple"});
  edges.push({ _from: `${vName}/18`, _to: `${vName}/14`, weight: 1, colour: "purple"});


  /* there is a path of length 5 with green edges and a path of length 4 with purple
     the test below ascertains that the path with purple edges is actually skipped
     and the path with green edges is found if there is a filter on edges only
     letting green edges pass. */
  edges.push({ _from: `${vName}/19`, _to: `${vName}/20`, weight: 1, colour: "green" });
  edges.push({ _from: `${vName}/20`, _to: `${vName}/21`, weight: 1, colour: "green" });
  edges.push({ _from: `${vName}/21`, _to: `${vName}/22`, weight: 1, colour: "green" });
  edges.push({ _from: `${vName}/22`, _to: `${vName}/23`, weight: 1, colour: "green" });

  edges.push({ _from: `${vName}/19`, _to: `${vName}/20`, weight: 1, colour: "purple" });
  edges.push({ _from: `${vName}/20`, _to: `${vName}/21`, weight: 1, colour: "purple" });
  edges.push({ _from: `${vName}/21`, _to: `${vName}/23`, weight: 1, colour: "purple" });



  db[vName].save(vertices);
  db[eName].save(edges);
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
  return {
    vertices: path.vertices.map((v) => v._id),
    edges: path.edges.map((e) => [e._from, e._to])
  };
}

function assertSameResults(query) {
  const resultWith = db._query(query).toArray();
  const resultWithPaths = resultWith.map(getVerticesAndEdgesFromPath);

  const resultWithout = db._query(query, {}, {optimizer: {rules: [`-${optimizerRuleName}`]}}).toArray();
  const resultWithoutPaths = resultWithout.map(getVerticesAndEdgesFromPath);

  assertEqual(resultWithPaths, resultWithoutPaths);
}

function queryFilterVerticesByType(pathQueryType) {
  return `
      FOR path IN ANY ${pathQueryType} "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          FILTER path.vertices[* RETURN CURRENT.colour == "green"] ALL == true
          RETURN path`;
}

function queryFilterEdgesByType(pathQueryType) {
  return `
      FOR path IN ANY ${pathQueryType} "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          FILTER path.edges[* RETURN CURRENT.colour == "green"] ALL == true
          RETURN path`;
}

function queryFilterVerticesEdgesByType(pathQueryType) {
  return `
      FOR path IN ANY ${pathQueryType} "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          FILTER path.vertices[* RETURN CURRENT.colour == "red"] ALL == true
          FILTER path.edges[* RETURN CURRENT.colour == "green"] ALL == true
          RETURN path`;
}

function enumeratePathsFilter() {
  var testObj = {
    setUpAll: function () {
      tearDownAll();
      createGraph();
    },
    tearDownAll,

    testDoesNotFireWhenInvalidVariableReferenced: function () {
      assertRuleDoesNotFire(`
        FOR path IN ANY K_PATHS "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          FOR x IN [1,2,3]
            FILTER path.vertices[* RETURN CURRENT.colour + x == "green"] ANY == false
            RETURN path`);
    },

    testDoesNotFireWhenANY: function () {
      assertRuleDoesNotFire(`
        FOR path IN ANY K_PATHS "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          FILTER path.vertices[* RETURN CURRENT.colour == "green"] ANY == false
          RETURN path`);
    },
    testDoesNotFireWhenLimit: function () {
      assertRuleDoesNotFire(`
        FOR path IN ANY K_PATHS "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          FILTER path.vertices[* LIMIT 5] ALL == false
          RETURN path`);
    },
    testDoesNotFireWhenFilter: function () {
      assertRuleDoesNotFire(`
        FOR path IN ANY K_PATHS "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          FILTER path.vertices[* FILTER CURRENT.foo < 5] ALL == false
          RETURN path`);
    },
    testFiresButLeavesOtherFilters: function() {
      const query = `
        FOR x IN [0,1,2] 
        FOR path IN ANY K_PATHS "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight"}
          FILTER path.vertices[* RETURN CURRENT.colour == "green"] ALL == false && x.y == 0
          RETURN path`;
      assertRuleDoesNotFire(query);
    },
    testFiresWithNesting: function() {
      for (let o of ['', ', algorithm: "legacy"']) {
        const query = `
          FOR path2 IN ANY K_SHORTEST_PATHS "${vName}/19" TO "${vName}/22" GRAPH "${graphName}"  
            FOR path IN ANY K_PATHS "${vName}/0" TO "${vName}/2" GRAPH "${graphName}" OPTIONS {weightAttribute: "weight" ${o}}
              FILTER path.vertices[* RETURN CURRENT.colour == "green"] ALL == false
              FILTER path2.edges[* RETURN CURRENT.colour == "red"] ALL == true
              RETURN [path2, path]`;
        assertRuleFires(query);
        assertSameResults(query);
      }
    },
    testExpectedPathFound: function() {
      for (let o of ['', ' OPTIONS { algorithm: "legacy" } ']) {
        const unrestrictedQuery = `
          FOR path IN ANY K_SHORTEST_PATHS "${vName}/19" TO "${vName}/23" GRAPH "${graphName}" ${o}
              LIMIT 1
              RETURN path`;
        const ru = db._query(unrestrictedQuery).toArray();
        assertEqual(ru.length, 1, "expecting precisely one result"); 
        const shortPath = getVerticesAndEdgesFromPath(ru[0]);
        assertEqual(shortPath.vertices.length, 4);

        const restrictedQuery = `
          FOR path IN ANY K_SHORTEST_PATHS "${vName}/19" TO "${vName}/23" GRAPH "${graphName}" ${o}
              FILTER path.edges[* RETURN CURRENT.colour == "green"] ALL == true
              LIMIT 1
              RETURN path`;

        assertRuleFires(restrictedQuery);
        const rr = db._query(restrictedQuery).toArray();
        assertEqual(rr.length, 1, "expecting precisely one result");
        const longPath = getVerticesAndEdgesFromPath(rr[0]);
        assertEqual(longPath.vertices.length, 5, `found path ${JSON.stringify(longPath)}`);
        
        const restrictedQuery2 = `
          FOR path IN ANY K_SHORTEST_PATHS "${vName}/19" TO "${vName}/23" GRAPH "${graphName}" ${o}
              FILTER path.edges[* RETURN CURRENT.colour] ALL == "green"
              LIMIT 1
              RETURN path`;

        assertRuleFires(restrictedQuery2);
        const rr2 = db._query(restrictedQuery2).toArray();
        assertEqual(rr2.length, 1, "expecting precisely one result");
        const longPath2 = getVerticesAndEdgesFromPath(rr2[0]);
        assertEqual(longPath2.vertices.length, 5, `found path ${JSON.stringify(longPath)}`);
        
        const restrictedQuery3 = `
          FOR path IN ANY K_SHORTEST_PATHS "${vName}/19" TO "${vName}/23" GRAPH "${graphName}" ${o}
              FILTER path.edges[*].colour ALL == "green"
              LIMIT 1
              RETURN path`;

        assertRuleFires(restrictedQuery3);
        const rr3 = db._query(restrictedQuery3).toArray();
        assertEqual(rr3.length, 1, "expecting precisely one result");
        const longPath3 = getVerticesAndEdgesFromPath(rr3[0]);
        assertEqual(longPath3.vertices.length, 5, `found path ${JSON.stringify(longPath)}`);
      }
    },
  };

  for (const pathType of pathSearchTypes) {
    testObj[`testFiresVertices_${pathType}`] = () => assertRuleFires(queryFilterVerticesByType(pathType));
    testObj[`testFiresEdges_${pathType}`] = () => assertRuleFires(queryFilterEdgesByType(pathType));
    testObj[`testFiresVerticesEdges_${pathType}`] = () => assertRuleFires(queryFilterVerticesEdgesByType(pathType));
    testObj[`testSameResultVertices_${pathType}`] =  () => assertSameResults(queryFilterVerticesByType(pathType));
    testObj[`testSameResultEdges_${pathType}`] =  () => assertSameResults(queryFilterEdgesByType(pathType));
    testObj[`testSameResultVerticesEdges_${pathType}`] = () => assertSameResults(queryFilterVerticesEdgesByType(pathType));
   }

  return testObj;
}

jsunity.run(enumeratePathsFilter);
return jsunity.done();
