/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, print */

// /////////////////////////////////////////////////////////////////////////////
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
/// @author Max Neunhöffer
// ///////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const gm = require("@arangodb/general-graph");
const _ = require("lodash");
const internal = require('internal');
const insertManyDocumentsIntoCollection = require("@arangodb/test-helper").insertManyDocumentsIntoCollection;

const graphName = "UnitTestGraph";
const vName = "UnitTestVertices";
const eName = "UnitTestEdges";
const theDepth = 10;   // intentionally small for PR tests to be quick
const theSize = 8;

// Some general information about the tests here:
// The tests are downsized to be quick for PR tests. They print some
// performance information, but this is not used for testing. Due to the
// small size of the tests, the performance information is not very
// meaningful. The tests are just here to make sure that the k-shortest-paths
// algorithm works in principle in these examples. We can use these tests
// to run them manually with a larger size and we can copy them over to
// the simple-performance-tests repository.

function tearDownAll() {
  try {
    gm._drop(graphName, true);
  } catch (e) {
    // Don't care for error, we might run initially with no graph exist
  }
  db._drop(eName);
  db._drop(vName);
}

function createGraph() {
  gm._create(graphName, [
      gm._relation(eName, vName, vName),
    ], [],
    {
      numberOfShards: 9
    }
  );
}

function makeBinaryTree(vName, eName, prefix, depth, direction) {
  // This creates a binary tree of depth `depth` with vertices in the
  // collection with name
  // vName and edges in the collection with name eName. The vertices are
  // named prefix + number, where number is the number of the vertex, 
  // ranging from 0 to 2^depth-1. Random weights are assigned to the edges.
  // The direction of the edges is either "IN" or "OUT".
  let vertices = [];
  for (let i = 0; i < Math.pow(2, depth); ++i) {
    vertices.push({
      _key: `${prefix}${i}`
    });
  }
  insertManyDocumentsIntoCollection("_system", vName, (i) => vertices[i],
    vertices.length, 10000);
  let edges = [];
  for (let i =0; i < Math.pow(2, depth-1); ++i) {
    if (direction === "OUT") {
      edges.push({_from: `${vName}/${prefix}${i}`, _to: `${vName}/${prefix}${2*i+1}`, weight: Math.random()});
      edges.push({_from: `${vName}/${prefix}${i}`, _to: `${vName}/${prefix}${2*i+2}`, weight: Math.random()});
    } else {
      edges.push({_to: `${vName}/${prefix}${i}`, _from: `${vName}/${prefix}${2*i+1}`, weight: Math.random()});
      edges.push({_to: `${vName}/${prefix}${i}`, _from: `${vName}/${prefix}${2*i+2}`, weight: Math.random()});
    }
  }
  insertManyDocumentsIntoCollection("_system", eName, (i) => edges[i],
    edges.length, 10000);
}

function makeCompleteGraph(vName, eName, prefix, size) {
  // This creates a complete directed graph in which every vertex can reach 
  // every other vertex. The vertices are named prefix + number, where number
  // is the number of the vertex, ranging from 0 to size-1. Random weights
  // are assigned to the edges.
  let vertices = [];
  for (let i = 0; i < size; ++i) {
    vertices.push({
      _key: `${prefix}${i}`
    });
  }
  db._collection(vName).insert(vertices);
  let edges = [];
  for (let i = 0; i < size; ++i) {
    for (let j = 0; j < size; ++j) {
      if (i !== j) {
        edges.push({_from: `${vName}/${prefix}${i}`, _to: `${vName}/${prefix}${j}`, weight: Math.random()});
      }
    }
  }
  db._collection(eName).insert(edges);
}

function kShortestPathsDifficultTreesLeftRightSuite() {
  // Note that 
  return {
    setUpAll: function () {
      tearDownAll();
      createGraph();
      makeBinaryTree(vName, eName, "S", theDepth, "OUT");
      makeBinaryTree(vName, eName, "T", theDepth, "IN");
    },
    tearDownAll,
    testTreeLeftRightOneBridgeSomeWhere: function () {
      let edgeKey;
      let E = db._collection(eName);
      try {
        let id = Math.pow(2, theDepth-1)-1;
        edgeKey = E.insert({
          _from: `${vName}/S${id}`,
          _to: `${vName}/T${id}`,
          weight: 1.0
        })._key;
        let startTime = new Date();
        let res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}'
            RETURN p`).toArray();
        print("Runtime legacy:", new Date() - startTime);
        assertEqual(1, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen' }
            RETURN p`).toArray();
        print("Runtime yen:", new Date() - startTime);
        assertEqual(1, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { weightAttribute: 'weight' }
            RETURN p`).toArray();
        print("Runtime legacy weighted:", new Date() - startTime);
        assertEqual(1, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen', weightAttribute: 'weight' }
            RETURN p`).toArray();
        print("Runtime yen weighted:", new Date() - startTime);
        assertEqual(1, res.length);
      } finally {
        E.remove(edgeKey);
      }
    },

    testTreeLeftRightOneBridgeSomeWhereToTarget: function () {
      let edgeKey;
      let E = db._collection(eName);
      try {
        let id = Math.pow(2, theDepth-1)-1;
        edgeKey = E.insert({
          _from: `${vName}/S${id}`,
          _to: `${vName}/T0`,
          weight: 1.0
        })._key;
        let startTime = new Date();
        let res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}'
            RETURN p`).toArray();
        print("Runtime legacy:", new Date() - startTime);
        assertEqual(1, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen' }
            RETURN p`).toArray();
        print("Runtime yen:", new Date() - startTime);
        assertEqual(1, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { weightAttribute: 'weight' }
            RETURN p`).toArray();
        print("Runtime legacy weighted:", new Date() - startTime);
        assertEqual(1, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen', weightAttribute: 'weight' }
            RETURN p`).toArray();
        print("Runtime yen weighted:", new Date() - startTime);
        assertEqual(1, res.length);
      } finally {
        E.remove(edgeKey);
      }
    },

    testTreeLeftRightOneBridgeFromSourceToSomeWhere: function () {
      let edgeKey;
      let E = db._collection(eName);
      try {
        let id = Math.pow(2, theDepth-1)-1;
        edgeKey = E.insert({
          _from: `${vName}/S0`,
          _to: `${vName}/T${id}`,
          weight: 1.0
        })._key;
        let startTime = new Date();
        let res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}'
            RETURN p`).toArray();
        print("Runtime legacy:", new Date() - startTime);
        assertEqual(1, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen' }
            RETURN p`).toArray();
        print("Runtime yen:", new Date() - startTime);
        assertEqual(1, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { weightAttribute: 'weight' }
            RETURN p`).toArray();
        print("Runtime legacy weighted:", new Date() - startTime);
        assertEqual(1, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen', weightAttribute: 'weight' }
            RETURN p`).toArray();
        print("Runtime yen weighted:", new Date() - startTime);
        assertEqual(1, res.length);
      } finally {
        E.remove(edgeKey);
      }
    },

  };

}

function kShortestPathsCompleteGraphsLeftRightSuite() {
  // Note that 
  return {
    setUpAll: function () {
      tearDownAll();
      createGraph();
      makeCompleteGraph(vName, eName, "S", theSize);
      makeCompleteGraph(vName, eName, "T", theSize);
    },
    tearDownAll,
    testCompleteGraphLeftRightOneBridgeSomeWhere: function () {
      let edgeKey;
      let E = db._collection(eName);
      try {
        let id = theSize-1;
        edgeKey = E.insert({
          _from: `${vName}/S${id}`,
          _to: `${vName}/T${id}`,
          weight: 1.0
        })._key;
        let startTime = new Date();
        let res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' LIMIT 3
            RETURN p`).toArray();
        print("Runtime legacy:", new Date() - startTime);
        assertEqual(3, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen' }
            LIMIT 3
            RETURN p`).toArray();
        print("Runtime yen:", new Date() - startTime);
        assertEqual(3, res.length);
        // We do not do the following one, since it takes an awful lot of 
        // time. We just keep the code here for completeness.
        //startTime = new Date();
        //res = db._query(`
        //  FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
        //    GRAPH '${graphName}' OPTIONS { weightAttribute: 'weight' }
        //    LIMIT 3
        //    RETURN p`).toArray();
        //print("Runtime legacy weighted:", new Date() - startTime);
        //assertEqual(3, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen', weightAttribute: 'weight' }
            LIMIT 3
            RETURN p`).toArray();
        print("Runtime yen weighted:", new Date() - startTime);
        assertEqual(3, res.length);
      } finally {
        E.remove(edgeKey);
      }
    },

    testCompleteGraphLeftRightOneBridgeSomeWhereToTarget: function () {
      let edgeKey;
      let E = db._collection(eName);
      try {
        let id = theSize-1;
        edgeKey = E.insert({
          _from: `${vName}/S${id}`,
          _to: `${vName}/T0`,
          weight: 1.0
        })._key;
        let startTime = new Date();
        let res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}'
            LIMIT 3
            RETURN p`).toArray();
        print("Runtime legacy:", new Date() - startTime);
        assertEqual(3, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen' }
            LIMIT 3
            RETURN p`).toArray();
        print("Runtime yen:", new Date() - startTime);
        assertEqual(3, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { weightAttribute: 'weight' }
            LIMIT 3
            RETURN p`).toArray();
        print("Runtime legacy weighted:", new Date() - startTime);
        assertEqual(3, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen', weightAttribute: 'weight' }
            LIMIT 3
            RETURN p`).toArray();
        print("Runtime yen weighted:", new Date() - startTime);
        assertEqual(3, res.length);
      } finally {
        E.remove(edgeKey);
      }
    },

    testCompleteGraphLeftRightOneBridgeFromSourceToSomeWhere: function () {
      let edgeKey;
      let E = db._collection(eName);
      try {
        let id = theSize-1;
        edgeKey = E.insert({
          _from: `${vName}/S0`,
          _to: `${vName}/T${id}`,
          weight: 1.0
        })._key;
        let startTime = new Date();
        let res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}'
            LIMIT 3
            RETURN p`).toArray();
        print("Runtime legacy:", new Date() - startTime);
        assertEqual(3, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen' }
            LIMIT 3
            RETURN p`).toArray();
        print("Runtime yen:", new Date() - startTime);
        assertEqual(3, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { weightAttribute: 'weight' }
            LIMIT 3
            RETURN p`).toArray();
        print("Runtime legacy weighted:", new Date() - startTime);
        assertEqual(3, res.length);
        startTime = new Date();
        res = db._query(`
          FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
            GRAPH '${graphName}' OPTIONS { algorithm: 'yen', weightAttribute: 'weight' }
            LIMIT 3
            RETURN p`).toArray();
        print("Runtime yen weighted:", new Date() - startTime);
        assertEqual(3, res.length);
      } finally {
        E.remove(edgeKey);
      }
    },

    testCompleteGraphLeftRightNoBridge: function () {
      let startTime = new Date();
      let res = db._query(`
        FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
          GRAPH '${graphName}'
          LIMIT 3
          RETURN p`).toArray();
      print("Runtime legacy:", new Date() - startTime);
      assertEqual(0, res.length);
      startTime = new Date();
      res = db._query(`
        FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
          GRAPH '${graphName}' OPTIONS { algorithm: 'yen' }
          LIMIT 3
          RETURN p`).toArray();
      print("Runtime yen:", new Date() - startTime);
      assertEqual(0, res.length);
      startTime = new Date();
      res = db._query(`
        FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
          GRAPH '${graphName}' OPTIONS { weightAttribute: 'weight' }
          LIMIT 3
          RETURN p`).toArray();
      print("Runtime legacy weighted:", new Date() - startTime);
      assertEqual(0, res.length);
      startTime = new Date();
      res = db._query(`
        FOR p IN OUTBOUND K_SHORTEST_PATHS '${vName}/S0' TO '${vName}/T0'
          GRAPH '${graphName}' OPTIONS { algorithm: 'yen', weightAttribute: 'weight' }
          LIMIT 3
          RETURN p`).toArray();
      print("Runtime yen weighted:", new Date() - startTime);
      assertEqual(0, res.length);
    },

  };

}

jsunity.run(kShortestPathsDifficultTreesLeftRightSuite);
jsunity.run(kShortestPathsCompleteGraphsLeftRightSuite);

return jsunity.done();
