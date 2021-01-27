/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const gm = require("@arangodb/general-graph");
const _ = require("underscore");

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
      { _from: `${vName}/1`, _to: `${vName}/2`, weight: 1 },
      { _from: `${vName}/2`, _to: `${vName}/3`, weight: 10 },
      { _from: `${vName}/1`, _to: `${vName}/4`, weight: 1.5 },
      { _from: `${vName}/4`, _to: `${vName}/6`, weight: 2 },
      { _from: `${vName}/6`, _to: `${vName}/3`, weight: 0.5 },
      { _from: `${vName}/3`, _to: `${vName}/5`, weight: 1 },
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

jsunity.run(WeightedTraveralsTestSuite);
return jsunity.done();
