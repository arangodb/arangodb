/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, fail */

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const gm = require("@arangodb/general-graph");
const internal = require('internal');

/*
    Graph layout (same as aql-graph-weighted-traversal.js):

  1 -> 2 - 10 -> 3 -> 5
   \             |
   1.5         0.5
     \          |
      4 - 2 -> 6

 */
function WeightedShortestPathTestSuite() {

  const graphName = "UnitTestWeightedShortestPathGraph";
  const vName = "UnitTestWeightedShortestPathVertices";
  const eName = "UnitTestWeightedShortestPathEdges";

  function createGraph () {
    gm._create(graphName, [gm._relation(eName, vName, vName)], [], {});

    const vertexes = [
      { _key: "1", value: 1 },
      { _key: "2", value: 1 },
      { _key: "3", value: 1 },
      { _key: "4", value: 1 },
      { _key: "5", value: 1 },
      { _key: "6", value: 0 },
    ];

    const edges = [
      { _from: `${vName}/1`, _to: `${vName}/2`, weight: 1, "Data.Weight": 2, Data: { Weight: 20 } },
      { _from: `${vName}/2`, _to: `${vName}/3`, weight: 10, "Data.Weight": 4, Data: { Weight: 40 } },
      { _from: `${vName}/1`, _to: `${vName}/4`, weight: 1.5, "Data.Weight": 1, Data: { Weight: 10 } },
      { _from: `${vName}/4`, _to: `${vName}/6`, weight: 2, "Data.Weight": 8, Data: { Weight: 80 } },
      { _from: `${vName}/6`, _to: `${vName}/3`, weight: 0.5, "Data.Weight": 1, Data: { Weight: 1 } },
      { _from: `${vName}/3`, _to: `${vName}/5`, weight: 1, "Data.Weight": 1, Data: { Weight: 1 } },
    ];

    db[vName].insert(vertexes);
    db[eName].insert(edges);
  }

  function runShortestPath(weightAttribute) {
    const attributeLiteral = JSON.stringify(weightAttribute);
    const query = `
      FOR v, e IN OUTBOUND SHORTEST_PATH "${vName}/1" TO "${vName}/5" GRAPH "${graphName}"
        OPTIONS {weightAttribute: ${attributeLiteral}}
        RETURN {key: v._key, weight: e == null ? null : e}
    `;
    return db._query(query).toArray();
  }

  function runKShortestPaths(weightAttribute, limit) {
    const attributeLiteral = JSON.stringify(weightAttribute);
    const query = `
      FOR path IN OUTBOUND K_SHORTEST_PATHS "${vName}/1" TO "${vName}/5" GRAPH "${graphName}"
        OPTIONS {weightAttribute: ${attributeLiteral}}
        LIMIT ${limit}
        RETURN {path: path.vertices[*]._key, weight: path.weight}
    `;
    return db._query(query).toArray();
  }

  function runTraversalShortest(weightAttribute) {
    const attributeLiteral = JSON.stringify(weightAttribute);
    const query = `
      FOR v, e, p IN 0..10 OUTBOUND "${vName}/1" GRAPH "${graphName}"
        PRUNE v._id == "${vName}/5"
        OPTIONS {order: "weighted", weightAttribute: ${attributeLiteral}, uniqueVertices: "global"}
        FILTER v._id == "${vName}/5"
        LIMIT 1
        RETURN {path: p.vertices[*]._key, weight: p.weights[-1]}
    `;
    return db._query(query).toArray();
  }

  return {
    setUpAll : function () {
      createGraph();
    },

    tearDownAll : function () {
      gm._drop(graphName, true);
    },

    testShortestPathSimpleWeightAttribute : function () {
      // hop count path would be 1-2-3-5 (weight 12); weighted prefers 1-4-6-3-5 (weight 5)
      const keys = runShortestPath("weight").map(row => row.key);
      assertEqual(["1", "4", "6", "3", "5"], keys);

      const ksp = runKShortestPaths("weight", 1);
      assertEqual([{ path: ["1", "4", "6", "3", "5"], weight: 5 }], ksp);

      assertEqual(runTraversalShortest("weight"), [
        { path: ["1", "4", "6", "3", "5"], weight: 5 }
      ]);
    },

    testShortestPathDottedTopLevelAttribute : function () {
      // "Data.Weight" selects a top-level attribute, not a nested path.
      // 1-2-3-5 = 7, 1-4-6-3-5 = 11 => prefers 1-2-3-5
      const keys = runShortestPath("Data.Weight").map(row => row.key);
      assertEqual(["1", "2", "3", "5"], keys);
      assertEqual(runShortestPath(["Data.Weight"]).map(row => row.key), keys);

      const expected = [{ path: ["1", "2", "3", "5"], weight: 7 }];
      assertEqual(runKShortestPaths("Data.Weight", 1), expected);
      assertEqual(runKShortestPaths(["Data.Weight"], 1), expected);
      assertEqual(runTraversalShortest("Data.Weight"), expected);
      assertEqual(runTraversalShortest(["Data.Weight"]), expected);
    },

    testShortestPathNestedAttributePath : function () {
      // Nested Data.Weight: 1-2-3-5 = 61, 1-4-6-3-5 = 92 => prefers 1-2-3-5
      const keys = runShortestPath(["Data", "Weight"]).map(row => row.key);
      assertEqual(["1", "2", "3", "5"], keys);

      const expected = [{ path: ["1", "2", "3", "5"], weight: 61 }];
      assertEqual(runKShortestPaths(["Data", "Weight"], 1), expected);
      assertEqual(runTraversalShortest(["Data", "Weight"]), expected);
    },

    testKShortestPathsNestedMultiple : function () {
      const result = runKShortestPaths(["Data", "Weight"], 2);
      assertEqual([
        { path: ["1", "2", "3", "5"], weight: 61 },
        { path: ["1", "4", "6", "3", "5"], weight: 92 }
      ], result);
    },

    testDefaultWeightForMissingNestedAttribute : function () {
      // Edges without Data.Missing use defaultWeight 100.
      const query = `
        FOR path IN OUTBOUND K_SHORTEST_PATHS "${vName}/1" TO "${vName}/5" GRAPH "${graphName}"
          OPTIONS {weightAttribute: ["Data", "Missing"], defaultWeight: 100}
          LIMIT 1
          RETURN {path: path.vertices[*]._key, weight: path.weight}
      `;
      const result = db._query(query).toArray();
      // hop-shortest among equal weights: 1-2-3-5 has 3 edges => weight 300
      assertEqual([{ path: ["1", "2", "3", "5"], weight: 300 }], result);
    },
  };
}


function WeightedShortestPathFallbackTestSuite() {

  const graphName = "UnitTestWeightedShortestPathFallbackGraph";
  const vName = "UnitTestWeightedShortestPathFallbackVertices";
  const eName = "UnitTestWeightedShortestPathFallbackEdges";

  return {
    setUp: function () {
      gm._create(graphName, [gm._relation(eName, vName, vName)], [], {});
      db[vName].insert([{ _key: "1" }, { _key: "2" }, { _key: "5" }]);
    },

    tearDown: function () {
      gm._drop(graphName, true);
    },

    testNullNestedWeightUsesDefault : function () {
      db[eName].insert([
        { _from: `${vName}/1`, _to: `${vName}/2`, Data: { Weight: null } },
        { _from: `${vName}/2`, _to: `${vName}/5`, Data: { Weight: null } },
      ]);

      const query = `
        FOR path IN OUTBOUND K_SHORTEST_PATHS "${vName}/1" TO "${vName}/5" GRAPH "${graphName}"
          OPTIONS {weightAttribute: ["Data", "Weight"], defaultWeight: 3}
          LIMIT 1
          RETURN path.weight
      `;
      assertEqual([6], db._query(query).toArray());
    },

    testNonNumericNestedWeightUsesDefault : function () {
      db[eName].insert([
        { _from: `${vName}/1`, _to: `${vName}/2`, Data: { Weight: "heavy" } },
        { _from: `${vName}/2`, _to: `${vName}/5`, Data: { Weight: "heavy" } },
      ]);

      const query = `
        FOR path IN OUTBOUND K_SHORTEST_PATHS "${vName}/1" TO "${vName}/5" GRAPH "${graphName}"
          OPTIONS {weightAttribute: ["Data", "Weight"], defaultWeight: 4}
          LIMIT 1
          RETURN path.weight
      `;
      assertEqual([8], db._query(query).toArray());
    },
  };
}


function WeightedShortestPathErrorTestSuite() {

  const graphName = "UnitTestWeightedShortestPathErrorGraph";
  const vName = "UnitTestWeightedShortestPathErrorVertices";
  const eName = "UnitTestWeightedShortestPathErrorEdges";

  function createGraph () {
    gm._create(graphName, [gm._relation(eName, vName, vName)], [], {});

    db[vName].insert([
      { _key: "1" },
      { _key: "2" },
    ]);

    db[eName].insert([
      { _from: `${vName}/1`, _to: `${vName}/2`, weight: -1, Data: { Weight: -5 } },
    ]);
  }

  return {
    setUpAll: function () {
      createGraph();
    },

    tearDownAll: function () {
      gm._drop(graphName, true);
    },

    testShortestPathNegativeNestedWeight: function () {
      const query = `
        FOR v IN OUTBOUND SHORTEST_PATH "${vName}/1" TO "${vName}/2" GRAPH "${graphName}"
          OPTIONS {weightAttribute: ["Data", "Weight"]}
          RETURN v._key
      `;

      try {
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, internal.errors.ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT.code);
      }
    },

    testKShortestPathsNegativeNestedWeight: function () {
      const query = `
        FOR path IN OUTBOUND K_SHORTEST_PATHS "${vName}/1" TO "${vName}/2" GRAPH "${graphName}"
          OPTIONS {weightAttribute: ["Data", "Weight"]}
          RETURN path
      `;

      try {
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, internal.errors.ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT.code);
      }
    },
  };
}


jsunity.run(WeightedShortestPathTestSuite);
jsunity.run(WeightedShortestPathFallbackTestSuite);
jsunity.run(WeightedShortestPathErrorTestSuite);
return jsunity.done();
