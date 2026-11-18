/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, fail */

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const gm = require("@arangodb/general-graph");
const internal = require('internal');

/*

  1 -> 2 - 10 -> 3 -> 5
   \             |
   1.5         0.5
     \          |
      4 - 2 -> 6

 */
function WeightedTraveralsTestSuite() {

  const graphName = "UnitTestGraph";
  const vName = "UnitTestVertices";
  const eName = "UnitTestEdges";

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


  return {
    setUpAll : function () {
      createGraph();
    },

    tearDownAll : function () {
      gm._drop(graphName, true);
    },

    testSimpleTraversal : function () {
      const query = `
        FOR v, e, p IN 1..10 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          OPTIONS {order: "weighted", weightAttribute: "weight"}
          LIMIT 3
          RETURN {path: p.vertices[*]._key, weight: p.weights[-1]}
      `;

      const expectedResult = [
        { "path" : [ "1", "2" ], "weight" : 1 },
        { "path" : [ "1", "4" ], "weight" : 1.5 },
        { "path" : [ "1", "4", "6" ], "weight" : 3.5 }
      ];

      const result = db._query(query).toArray();
      assertEqual(expectedResult, result);
    },

    testSimpleTraversalSingleEdge : function () {
      const query = `
        FOR v, e, p IN 1..1 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          OPTIONS {order: "weighted", weightAttribute: "weight"}
          LIMIT 3
          RETURN {path: p.vertices[*]._key, weight: p.weights[-1]}
      `;

      const expectedResult = [
        { "path" : [ "1", "2" ], "weight" : 1 },
        { "path" : [ "1", "4" ], "weight" : 1.5 },
      ];

      const result = db._query(query).toArray();
      assertEqual(expectedResult, result);
    },

    testWeightAttributePath : function () {
      const topLevelQueryString = `
        FOR v, e, p IN 2 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          OPTIONS {order: "weighted", weightAttribute: "Data.Weight"}
          LIMIT 2
          RETURN p.weights
      `;

      const topLevelQuerySingleArray = `
        FOR v, e, p IN 2 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          OPTIONS {order: "weighted", weightAttribute: ["Data.Weight"]}
          LIMIT 2
          RETURN p.weights
      `;

      const nestedQuery = `
        FOR v, e, p IN 2 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          OPTIONS {order: "weighted", weightAttribute: ["Data", "Weight"]}
          LIMIT 2
          RETURN p.weights
      `;

      assertEqual([[0, 2, 6], [0, 1, 9]], db._query(topLevelQueryString).toArray());
      assertEqual([[0, 2, 6], [0, 1, 9]], db._query(topLevelQuerySingleArray).toArray());
      assertEqual([[0, 20, 60], [0, 10, 90]], db._query(nestedQuery).toArray());
    },

    testShortestPath : function () {
      const target = `${vName}/5`;

      const query = `
        FOR v, e, p IN 0..10 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          PRUNE v._id == "${target}"
          OPTIONS {order: "weighted", weightAttribute: "weight", uniqueVertices: "global"}
          FILTER v._id == "${target}"
          LIMIT 1
          RETURN {path: p.vertices[*]._key, weight: p.weights[-1]}
      `;

      const expectedResult = [
        { "path" : [ "1", "4", "6", "3", "5" ], "weight" : 5 }
      ];

      const result = db._query(query).toArray();
      assertEqual(expectedResult, result);
    },

    testShortestPathWithVertexCondition : function () {
      const target = `${vName}/5`;

      const query = `
        FOR v, e, p IN 1..10 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          PRUNE v._id == "${target}"
          OPTIONS {order: "weighted", weightAttribute: "weight", uniqueVertices: "global"}
          FILTER p.vertices[*].value ALL > 0
          FILTER v._id == "${target}"
          LIMIT 1
          RETURN {path: p.vertices[*]._key, weight: p.weights[-1]}
      `;

      const expectedResult = [
        { "path" : [ "1", "2", "3", "5" ], "weight" : 12 }
      ];

      const result = db._query(query).toArray();
      assertEqual(expectedResult, result);
    },

    testShortestPathWithEdgeCondition : function () {
      const target = `${vName}/5`;

      const query = `
        FOR v, e, p IN 1..10 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          PRUNE v._id == "${target}"
          OPTIONS {order: "weighted", weightAttribute: "weight", uniqueVertices: "global"}
          FILTER p.edges[*].weight ALL >= 1
          FILTER v._id == "${target}"
          LIMIT 1
          RETURN {path: p.vertices[*]._key, weight: p.weights[-1]}
      `;

      const expectedResult = [
        { "path" : [ "1", "2", "3", "5" ], "weight" : 12 }
      ];

      const result = db._query(query).toArray();
      assertEqual(expectedResult, result);
    },

    testKShortestPaths : function () {  // slow version :D
      const target = `${vName}/5`;

      const query = `
        FOR v, e, p IN 1..10 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          PRUNE v._id == "${target}"
          OPTIONS {order: "weighted", weightAttribute: "weight", uniqueVertices: "path"}
          FILTER v._id == "${target}"
          LIMIT 3
          RETURN {path: p.vertices[*]._key, weight: p.weights[-1]}
      `;

      const expectedResult = [
        { "path" : [ "1", "4", "6", "3", "5" ], "weight" : 5 },
        { "path" : [ "1", "2", "3", "5" ], "weight" : 12 }
      ];

      const result = db._query(query).toArray();
      assertEqual(expectedResult, result);
    }
  };
}


function WeightedTraveralsErrorTestSuite() {

  const graphName = "UnitTestGraph";
  const vName = "UnitTestVertices";
  const eName = "UnitTestEdges";

  function createGraph () {
    gm._create(graphName, [gm._relation(eName, vName, vName)], [], {});

    const vertexes = [
      { _key: "1", value: 1 },
      { _key: "2", value: 1 },
    ];

    const edges = [
      { _from: `${vName}/1`, _to: `${vName}/2`, weight: -1 },
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

    testShortestPathNegativeDefaultEdgeWeight: function () {
      const query = `
        FOR v, e, p IN 0..10 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          OPTIONS {order: "weighted", defaultWeight: @weight, uniqueVertices: "global"}
          LIMIT 1
          RETURN {path: p.vertices[*]._key, weight: p.weights[-1]}
      `;

      try {
        db._query({query, bindVars: {weight: -1}});
        fail();
      } catch (err) {
        assertEqual(err.errorNum, internal.errors.ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT.code);
      }
    },

    testShortestPathNegativeEdgeWeight: function () {
      const query = `
        FOR v, e, p IN 0..10 OUTBOUND "${vName}/1" GRAPH "${graphName}"
          OPTIONS {order: "weighted", weightAttribute: "weight", uniqueVertices: "global"}
          LIMIT 1
          RETURN {path: p.vertices[*]._key, weight: p.weights[-1]}
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


jsunity.run(WeightedTraveralsTestSuite);
jsunity.run(WeightedTraveralsErrorTestSuite);
return jsunity.done();
