/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

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
/// http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Florian Bartels
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var graph = require("@arangodb/general-graph");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var getRawQueryResults = helper.getRawQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for EDGES() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryGeneralEdgesTestSuite() {

  var gN = "bla3";
  var v1 = "UnitTestsAhuacatlVertex1";
  var v2 = "UnitTestsAhuacatlVertex2";
  var v3 = "UnitTestsAhuacatlVertex3";
  var v4 = "UnitTestsAhuacatlVertex4";
  var e1 = "UnitTestsAhuacatlEdge1";
  var e2 = "UnitTestsAhuacatlEdge2";
  var or = "UnitTestsAhuacatlOrphan";

  var AQL_VERTICES = "FOR e IN arangodb::GRAPH_VERTICES(@name, @example, @options) SORT e RETURN e";
  var AQL_EDGES = "FOR e IN arangodb::GRAPH_EDGES(@name, @example, @options) SORT e.what RETURN e.what";
  var AQL_NEIGHBORS = "FOR e IN arangoDB::GRAPH_NEIGHBORS(@name, @example, @options) SORT e RETURN e";

  var startExample = [{hugo : true}, {heinz : 1}];
  var vertexExample = {_key: "v1"};

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop(v1);
      db._drop(v2);
      db._drop(v3);
      db._drop(v4);
      db._drop(e1);
      db._drop(e2);
      db._drop(or);

      var vertex1 = db._create(v1);
      var vertex2 = db._create(v2);
      var vertex3 = db._create(v3);
      var vertex4 = db._create(v4);
      var edge1 = db._createEdgeCollection(e1);
      var edge2 = db._createEdgeCollection(e2);
      var orphan = db._create(or);

      vertex1.save({ _key: "v1", hugo: true});
      vertex1.save({ _key: "v2", hugo: true});
      vertex2.save({ _key: "v3", heinz: 1});
      vertex2.save({ _key: "v4" });
      vertex3.save({ _key: "v5" });
      vertex3.save({ _key: "v6" });
      vertex4.save({ _key: "v7" });
      vertex4.save({ _key: "v8", heinz: 1});
      orphan.save({ _key: "orphan" });

      function makeEdge(from, to, collection) {
        collection.save(from, to, { what: from.split("/")[1] + "->" + to.split("/")[1] });
      }

      makeEdge(v1 + "/v1", v1 + "/v2", edge1);
      makeEdge(v1 + "/v2", v1 + "/v1", edge1);
      makeEdge(v1 + "/v1", v3 + "/v5", edge2);
      makeEdge(v1 + "/v2", v3 + "/v5", edge2);
      makeEdge(v2 + "/v3", v3 + "/v6", edge2);
      makeEdge(v2 + "/v4", v4 + "/v7", edge2);
      makeEdge(v2 + "/v3", v3 + "/v5", edge2);
      makeEdge(v2 + "/v3", v4 + "/v8", edge2);

      try {
        db._collection("_graphs").remove("_graphs/bla3");
      } catch (ignore) {
      }
      graph._create(
        "bla3",
        graph._edgeDefinitions(
          graph._relation(e1, v1, v1),
          graph._relation(e2,
            [v1, v2],
            [v3, v4]
          )
        ),
        [or]
      );
      graph._registerCompatibilityFunctions();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      db._drop(v1);
      db._drop(v2);
      db._drop(v3);
      db._drop(v4);
      db._drop(e1);
      db._drop(e2);
      db._drop(or);
      db._collection("_graphs").remove("_graphs/bla3");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_VERTICES()
    ////////////////////////////////////////////////////////////////////////////////
    
    testVertices: function () {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {}
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual[0]._id, v1 + '/v1');
    },

    testVerticesRestricted: function() {
      var bindVars = {
        name: gN,
        example: {},
        options: {
          vertexCollectionRestriction: or
        }
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0]._id, 'UnitTestsAhuacatlOrphan/orphan');
    },

    testVerticesExample: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {}
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual.length, 4);
      assertEqual(actual[0]._id, 'UnitTestsAhuacatlVertex1/v1');
      assertEqual(actual[1]._id, 'UnitTestsAhuacatlVertex1/v2');
      assertEqual(actual[2]._id, 'UnitTestsAhuacatlVertex2/v3');
      assertEqual(actual[3]._id, 'UnitTestsAhuacatlVertex4/v8');
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_EDGES()
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    /// Section any direction
    ////////////////////////////////////////////////////////////////////////////////
    testEdgesAny: function () {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          direction : 'any',
          includeData: true
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual, [ "v1->v2", "v1->v5", "v2->v1" ]);
    },

    testEdgesAnyRestricted: function () {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          direction : 'any',
          edgeCollectionRestriction: [e1],
          includeData: true
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual, [ "v1->v2", "v2->v1" ]);
    },

    testEdgesAnyStartExample: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'any',
          includeData: true
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual, [
        "v1->v2",
        "v1->v5",
        "v2->v1",
        "v2->v5",
        "v3->v5",
        "v3->v6",
        "v3->v8"
      ]);
    },

    testEdgesAnyStartExampleEdgeExample: function () {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          direction : 'any',
          includeData: true,
          edgeExamples: [{what: 'v2->v1'}]
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "v2->v1");
    },

    testEdgesInbound: function() {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          includeData: true,
          direction : 'inbound'
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual, [ "v2->v1"]);
    },

    testEdgesInboundStartExample: function() {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          includeData: true,
          direction : 'inbound'
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual.length, 3);
      assertEqual(actual[0], "v1->v2");
      assertEqual(actual[1], "v2->v1");
      assertEqual(actual[2], "v3->v8");
    },

    testEdgesInboundStartExampleRestricted: function() {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          includeData: true,
          direction : 'inbound',
          edgeCollectionRestriction: [e2]
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "v3->v8");
    },

    testEdgesInboundStartExampleEdgeExample: function() {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          includeData: true,
          direction : 'inbound',
          edgeExamples: [{'what' : 'v3->v8'}]
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "v3->v8");
    },

    testEdgesOutbound: function() {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          includeData: true,
          direction : 'outbound'
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual[0], "v1->v2");
      assertEqual(actual[1], "v1->v5");
    },

    testEdgesOutboundStartExample: function() {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'outbound',
          includeData: true
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual.length, 7);
      assertEqual(actual[0], "v1->v2");
      assertEqual(actual[1], "v1->v5");
      assertEqual(actual[2], "v2->v1");
      assertEqual(actual[3], "v2->v5");
      assertEqual(actual[4], "v3->v5");
      assertEqual(actual[5], "v3->v6");
      assertEqual(actual[6], "v3->v8");
    },

    testEdgesOutboundStartExampleRestricted: function() {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'outbound',
          includeData: true,
          edgeCollectionRestriction: [e2]
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual.length, 5);
      assertEqual(actual[0], "v1->v5");
      assertEqual(actual[1], "v2->v5");
      assertEqual(actual[2], "v3->v5");
      assertEqual(actual[3], "v3->v6");
      assertEqual(actual[4], "v3->v8");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// Test Neighbors
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    /// Any direction
    ////////////////////////////////////////////////////////////////////////////////
    
    testNeighborsAny: function () {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          direction : 'any',
          maxIterations : 10000
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v1 + "/v2");
      assertEqual(actual[1], v3 + "/v5");
    },

    testNeighborsAnyEdgeExample: function () {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          direction : 'any',
          edgeExamples : [{'what' : 'v2->v1'}]
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v2");
    },

    testNeighborsAnyStartExample: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'any'
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 6);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
      assertEqual(actual[2], v2 + "/v3");
      assertEqual(actual[3], v3 + "/v5");
      assertEqual(actual[4], v3 + "/v6");
      assertEqual(actual[5], v4 + "/v8");
    },

    testNeighborsAnyVertexExample: function () {
      var bindVars = {
        name: gN,
        example: {},
        options: {
          direction : 'any',
          neighborExamples: vertexExample
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v1");
    },

    testNeighborsAnyStartExampleRestrictEdges: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'any',
          edgeCollectionRestriction: e2
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 4);
      assertEqual(actual[0], v2 + "/v3");
      assertEqual(actual[1], v3 + "/v5");
      assertEqual(actual[2], v3 + "/v6");
      assertEqual(actual[3], v4 + "/v8");
    },

    /* FOR v IN ...
    FILTER (
      IS_SAME_COLLECTION(v, `UnitTestsAhuacatlVertex1`) ||
      IS_SAME_COLLECTION(v, `UnitTestsAhuacatlVertex3`) 
     )*/
      
    testNeighborsAnyStartExampleRestrictVertices: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'any',
          vertexCollectionRestriction: [v1, v3]
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars); 
      assertEqual(actual.length, 4);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
      assertEqual(actual[2], v3 + "/v5");
      assertEqual(actual[3], v3 + "/v6");
    },

    testNeighborsAnyStartExampleRestrictEdgesAndVertices: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'any',
          vertexCollectionRestriction: [v1, v3],
          edgeCollectionRestriction: e2
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v3 + "/v5");
      assertEqual(actual[1], v3 + "/v6");
    },

 
    ////////////////////////////////////////////////////////////////////////////////
    /// direction outbound
    ////////////////////////////////////////////////////////////////////////////////
    
    testNeighborsOutbound: function () {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          direction : 'outbound'
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v1 + "/v2");
      assertEqual(actual[1], v3 + "/v5");
    },

    testNeighborsOutboundEdgeExample: function () {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          direction : 'outbound',
          edgeExamples : [{'what' : 'v1->v2'}, {'what' : 'v2->v1'}]
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v2");
    },

    testNeighborsOutboundStartExample: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'outbound'
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 5);

      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
      assertEqual(actual[2], v3 + "/v5");
      assertEqual(actual[3], v3 + "/v6");
      assertEqual(actual[4], v4 + "/v8");
    },

    testNeighborsOutboundVertexExample: function () {
      var bindVars = {
        name: gN,
        example: {},
        options: {
          direction : 'outbound',
          neighborExamples: vertexExample
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v1");
    },

    testNeighborsOutboundStartExampleRestrictEdges: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'outbound',
          edgeCollectionRestriction: e2
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 3);
      assertEqual(actual[0], v3 + "/v5");
      assertEqual(actual[1], v3 + "/v6");
      assertEqual(actual[2], v4 + "/v8");
    },

    testNeighborsOutboundStartExampleRestrictVertices: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'outbound',
          vertexCollectionRestriction: [v1, v3]
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 4);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
      assertEqual(actual[2], v3 + "/v5");
      assertEqual(actual[3], v3 + "/v6");
    },

    testNeighborsOutboundStartExampleRestrictEdgesAndVertices: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'outbound',
          vertexCollectionRestriction: [v1, v3],
          edgeCollectionRestriction: e2
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v3 + "/v5");
      assertEqual(actual[1], v3 + "/v6");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// inbound direction
    ////////////////////////////////////////////////////////////////////////////////
    
    testNeighborsInbound: function () {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          direction : 'inbound'
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v2");
    },

    testNeighborsInboundEdgeExample: function () {
      var bindVars = {
        name: gN,
        example: v1 + "/v1",
        options: {
          direction : 'inbound',
          edgeExamples : [{'what' : 'v2->v1'}]
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual[0], v1 + "/v2");
    },

    testNeighborsInboundStartExample: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'inbound'
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 3);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
      assertEqual(actual[2], v2 + "/v3");
    },

    testNeighborsInboundNeighborExample: function () {
      var bindVars = {
        name: gN,
        example: {},
        options: {
          direction : 'inbound',
          neighborExamples: vertexExample
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v1");
    },

    testNeighborsInboundStartExampleRestrictEdges: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'inbound',
          edgeCollectionRestriction: e2
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v2 + "/v3");
    },

    testNeighborsInboundStartExampleRestrictVertices: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'inbound',
          vertexCollectionRestriction: [v1, v3]
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
    },

    testNeighborsInboundStartExampleRestrictEdgesAndVertices: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {
          direction : 'inbound',
          vertexCollectionRestriction: [v1, v3],
          edgeCollectionRestriction: e2
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 0);
    }

  };
}


function ahuacatlQueryGeneralCommonTestSuite() {

  var vertexIds = {};

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop("UnitTestsAhuacatlVertex1");
      db._drop("UnitTestsAhuacatlVertex2");
      db._drop("UnitTestsAhuacatlEdge1");

      var vertex1 = db._create("UnitTestsAhuacatlVertex1");
      var vertex2 = db._create("UnitTestsAhuacatlVertex2");
      var edge1 = db._createEdgeCollection("UnitTestsAhuacatlEdge1");

      var v1 = vertex1.save({ _key: "v1", hugo: true})._id;
      var v2 = vertex1.save({ _key: "v2", hugo: true})._id;
      var v3 = vertex1.save({ _key: "v3", heinz: 1})._id;
      var v4 = vertex1.save({ _key: "v4", harald: "meier"})._id;
      var v5 = vertex2.save({ _key: "v5", ageing: true})._id;
      var v6 = vertex2.save({ _key: "v6", harald: "meier", ageing: true})._id;
      var v7 = vertex2.save({ _key: "v7", harald: "meier"})._id;
      var v8 = vertex2.save({ _key: "v8", heinz: 1, harald: "meier"})._id;

      vertexIds.v1 = v1;
      vertexIds.v2 = v2;
      vertexIds.v3 = v3;
      vertexIds.v4 = v4;
      vertexIds.v5 = v5;
      vertexIds.v6 = v6;
      vertexIds.v7 = v7;
      vertexIds.v8 = v8;

      function makeEdge(from, to, collection) {
        collection.save(from, to, { what: from.split("/")[1] + "->" + to.split("/")[1] });
      }

      makeEdge(v1, v2, edge1);
      makeEdge(v2, v3, edge1);
      makeEdge(v3, v5, edge1);
      makeEdge(v2, v6, edge1);
      makeEdge(v6, v7, edge1);
      makeEdge(v4, v7, edge1);
      makeEdge(v3, v7, edge1);
      makeEdge(v8, v1, edge1);
      makeEdge(v3, v5, edge1);
      makeEdge(v3, v8, edge1);

      try {
        db._collection("_graphs").remove("_graphs/bla3");
      } catch (ignore) {
      }
      graph._create(
        "bla3",
        graph._edgeDefinitions(
          graph._relation("UnitTestsAhuacatlEdge1",
            ["UnitTestsAhuacatlVertex1", "UnitTestsAhuacatlVertex2"],
            ["UnitTestsAhuacatlVertex1", "UnitTestsAhuacatlVertex2"]
          )
        )
      );
      graph._registerCompatibilityFunctions();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      db._drop("UnitTestsAhuacatlVertex1");
      db._drop("UnitTestsAhuacatlVertex2");
      db._drop("UnitTestsAhuacatlEdge1");
      try {
        db._collection("_graphs").remove("_graphs/bla3");
      } catch (ignore) {
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_COMMON_NEIGHBORS() and GRAPH_COMMON_PROPERTIES()
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighbors: function () {
      var actual = getQueryResults("FOR e IN arangodb::GRAPH_COMMON_NEIGHBORS('bla3', 'UnitTestsAhuacatlVertex1/v3' , 'UnitTestsAhuacatlVertex2/v6',  {direction : 'any'}) SORT e.left  RETURN e");
      assertEqual(actual.length, 1);
      assertEqual(actual[0].left, vertexIds.v3);
      assertEqual(actual[0].right, vertexIds.v6);
      assertEqual(actual[0].neighbors.sort(), [vertexIds.v2, vertexIds.v7].sort());
      /* Legacy
      assertEqual(actual[0]["UnitTestsAhuacatlVertex1/v3"]["UnitTestsAhuacatlVertex2/v6"][0]._id, "UnitTestsAhuacatlVertex1/v2");
      assertEqual(actual[0]["UnitTestsAhuacatlVertex1/v3"]["UnitTestsAhuacatlVertex2/v6"][1]._id, "UnitTestsAhuacatlVertex2/v7");
      */

    },
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_COMMON_NEIGHBORS()
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsIn: function () {
      var actual = getQueryResults("FOR e IN arangodb::GRAPH_COMMON_NEIGHBORS('bla3', {} , {},  {direction : 'inbound'}, {direction : 'inbound'}) SORT e.left, e.right RETURN e");
      assertEqual(actual.length, 8, "We expect one entry for each pair of vertices having at least one common neighbor");

      assertEqual(actual[0].left, vertexIds.v3);
      assertEqual(actual[0].right, vertexIds.v6);
      assertEqual(actual[0].neighbors.sort(), [vertexIds.v2].sort());

      assertEqual(actual[1].left, vertexIds.v5);
      assertEqual(actual[1].right, vertexIds.v7);
      assertEqual(actual[1].neighbors.sort(), [vertexIds.v3].sort());

      assertEqual(actual[2].left, vertexIds.v5);
      assertEqual(actual[2].right, vertexIds.v8);
      assertEqual(actual[2].neighbors.sort(), [vertexIds.v3].sort());

      assertEqual(actual[3].left, vertexIds.v6);
      assertEqual(actual[3].right, vertexIds.v3);
      assertEqual(actual[3].neighbors.sort(), [vertexIds.v2].sort());

      assertEqual(actual[4].left, vertexIds.v7);
      assertEqual(actual[4].right, vertexIds.v5);
      assertEqual(actual[4].neighbors.sort(), [vertexIds.v3].sort());

      assertEqual(actual[5].left, vertexIds.v7);
      assertEqual(actual[5].right, vertexIds.v8);
      assertEqual(actual[5].neighbors.sort(), [vertexIds.v3].sort());

      assertEqual(actual[6].left, vertexIds.v8);
      assertEqual(actual[6].right, vertexIds.v5);
      assertEqual(actual[6].neighbors.sort(), [vertexIds.v3].sort());

      assertEqual(actual[7].left, vertexIds.v8);
      assertEqual(actual[7].right, vertexIds.v7);
      assertEqual(actual[7].neighbors.sort(), [vertexIds.v3].sort());

      /* Legacy
      assertEqual(actual[0]["UnitTestsAhuacatlVertex1/v3"]["UnitTestsAhuacatlVertex2/v6"][0]._id, "UnitTestsAhuacatlVertex1/v2");
      assertEqual(actual[1]["UnitTestsAhuacatlVertex2/v5"]["UnitTestsAhuacatlVertex2/v8"][0]._id, "UnitTestsAhuacatlVertex1/v3");
      assertEqual(actual[1]["UnitTestsAhuacatlVertex2/v5"]["UnitTestsAhuacatlVertex2/v7"][0]._id, "UnitTestsAhuacatlVertex1/v3");


      assertEqual(actual[2]["UnitTestsAhuacatlVertex2/v6"]["UnitTestsAhuacatlVertex1/v3"][0]._id, "UnitTestsAhuacatlVertex1/v2");
      assertEqual(actual[3]["UnitTestsAhuacatlVertex2/v7"]["UnitTestsAhuacatlVertex2/v5"][0]._id, "UnitTestsAhuacatlVertex1/v3");
      assertEqual(actual[3]["UnitTestsAhuacatlVertex2/v7"]["UnitTestsAhuacatlVertex2/v8"][0]._id, "UnitTestsAhuacatlVertex1/v3");

      assertEqual(actual[4]["UnitTestsAhuacatlVertex2/v8"]["UnitTestsAhuacatlVertex2/v5"][0]._id, "UnitTestsAhuacatlVertex1/v3");
      assertEqual(actual[4]["UnitTestsAhuacatlVertex2/v8"]["UnitTestsAhuacatlVertex2/v7"][0]._id, "UnitTestsAhuacatlVertex1/v3");
      */
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_COMMON_NEIGHBORS()
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsOut: function () {
      var actual = getQueryResults("FOR e IN arangodb::GRAPH_COMMON_NEIGHBORS('bla3', { hugo : true } , {heinz : 1}, " +
        " {direction : 'outbound', minDepth : 1, maxDepth : 3}, {direction : 'outbound', minDepth : 1, maxDepth : 3}) SORT e.left, e.right RETURN e");

      assertEqual(actual.length, 4, "Expect one result for each pair of vertices sharing neighbors");

      assertEqual(actual[0].left, vertexIds.v1);
      assertEqual(actual[0].right, vertexIds.v3);
      assertEqual(actual[0].neighbors.sort(), [vertexIds.v2, vertexIds.v5, vertexIds.v7, vertexIds.v8].sort());

      assertEqual(actual[1].left, vertexIds.v1);
      assertEqual(actual[1].right, vertexIds.v8);
      assertEqual(actual[1].neighbors.sort(), [vertexIds.v2, vertexIds.v3, vertexIds.v6].sort());

      assertEqual(actual[2].left, vertexIds.v2);
      assertEqual(actual[2].right, vertexIds.v3);
      assertEqual(actual[2].neighbors.sort(), [vertexIds.v1, vertexIds.v5, vertexIds.v7, vertexIds.v8].sort());

      assertEqual(actual[3].left, vertexIds.v2);
      assertEqual(actual[3].right, vertexIds.v8);
      assertEqual(actual[3].neighbors.sort(), [vertexIds.v1, vertexIds.v6, vertexIds.v3].sort());

      /* Legacy
      assertEqual(Object.keys(actual[0])[0], "UnitTestsAhuacatlVertex1/v2");
      assertEqual(Object.keys(actual[0][Object.keys(actual[0])[0]]), ["UnitTestsAhuacatlVertex2/v8", "UnitTestsAhuacatlVertex1/v3"]);

      assertEqual(actual[0][Object.keys(actual[0])[0]]["UnitTestsAhuacatlVertex2/v8"].length, 3);
      assertEqual(actual[0][Object.keys(actual[0])[0]]["UnitTestsAhuacatlVertex1/v3"].length, 4);

      assertEqual(Object.keys(actual[1])[0], "UnitTestsAhuacatlVertex1/v1");
      assertEqual(Object.keys(actual[1][Object.keys(actual[1])[0]]), ["UnitTestsAhuacatlVertex2/v8", "UnitTestsAhuacatlVertex1/v3"]);

      assertEqual(actual[1][Object.keys(actual[1])[0]]["UnitTestsAhuacatlVertex1/v3"].length, 4);
      assertEqual(actual[1][Object.keys(actual[1])[0]]["UnitTestsAhuacatlVertex2/v8"].length, 3);
      */

    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_COMMON_NEIGHBORS()
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsMixedOptionsDistinctFilters: function () {
      var actual = getQueryResults("FOR e IN arangodb::GRAPH_COMMON_NEIGHBORS('bla3', {} , {},  " +
        "{direction : 'outbound', vertexCollectionRestriction : 'UnitTestsAhuacatlVertex1'}, " +
        "{direction : 'inbound', minDepth : 1, maxDepth : 2, vertexCollectionRestriction : 'UnitTestsAhuacatlVertex2'}) SORT e.left, e.right RETURN e");
      assertEqual(actual.length, 0, "Expect one result for each pair of vertices sharing neighbors");
    },

    testCommonNeighborsMixedOptionsFilterBasedOnOneCollectionOnly: function () {
      var actual = getQueryResults("FOR e IN arangodb::GRAPH_COMMON_NEIGHBORS('bla3', {} , {},  " +
        "{direction : 'outbound', vertexCollectionRestriction : 'UnitTestsAhuacatlVertex2'}, " +
        "{direction : 'inbound', minDepth : 1, maxDepth : 2, vertexCollectionRestriction : 'UnitTestsAhuacatlVertex2'}) SORT e.left, e.right RETURN e");

      assertEqual(actual.length, 3, "Expect one result for each pair of vertices sharing neighbors");
      assertEqual(actual[0].left, vertexIds.v2);
      assertEqual(actual[0].right, vertexIds.v7);
      assertEqual(actual[0].neighbors.sort(), [vertexIds.v6].sort());

      assertEqual(actual[1].left, vertexIds.v3);
      assertEqual(actual[1].right, vertexIds.v1);
      assertEqual(actual[1].neighbors.sort(), [vertexIds.v8].sort());

      assertEqual(actual[2].left, vertexIds.v3);
      assertEqual(actual[2].right, vertexIds.v2);
      assertEqual(actual[2].neighbors.sort(), [vertexIds.v8].sort());

      /* Legacy
      assertEqual(Object.keys(actual[0])[0], "UnitTestsAhuacatlVertex1/v3");
      assertEqual(Object.keys(actual[0][Object.keys(actual[0])[0]]).sort(), ["UnitTestsAhuacatlVertex1/v1", "UnitTestsAhuacatlVertex1/v2"]);

      assertEqual(actual[0][Object.keys(actual[0])[0]]["UnitTestsAhuacatlVertex1/v1"].length, 1);
      assertEqual(actual[0][Object.keys(actual[0])[0]]["UnitTestsAhuacatlVertex1/v2"].length, 1);

      assertEqual(Object.keys(actual[1])[0], "UnitTestsAhuacatlVertex1/v2");
      assertEqual(Object.keys(actual[1][Object.keys(actual[1])[0]]).sort(), ["UnitTestsAhuacatlVertex2/v7"]);

      assertEqual(actual[1][Object.keys(actual[1])[0]]["UnitTestsAhuacatlVertex2/v7"].length, 1);
      */

    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_COMMON_PROPERTIES()
    ////////////////////////////////////////////////////////////////////////////////

    testCommonProperties: function () {
      var actual = getQueryResults("FOR e IN arangodb::GRAPH_COMMON_PROPERTIES('bla3', { } , {},  {}) SORT  ATTRIBUTES(e)[0] RETURN e");
      assertEqual(actual[0]["UnitTestsAhuacatlVertex1/v1"].length, 1);
      assertEqual(actual[1]["UnitTestsAhuacatlVertex1/v2"].length, 1);
      assertEqual(actual[2]["UnitTestsAhuacatlVertex1/v3"].length, 1);
      assertEqual(actual[3]["UnitTestsAhuacatlVertex1/v4"].length, 3);

      assertEqual(actual[4]["UnitTestsAhuacatlVertex2/v5"].length, 1);
      assertEqual(actual[5]["UnitTestsAhuacatlVertex2/v6"].length, 4);

      assertEqual(actual[6]["UnitTestsAhuacatlVertex2/v7"].length, 3);

      assertEqual(actual[7]["UnitTestsAhuacatlVertex2/v8"].length, 4);

    },

    testCommonPropertiesWithFilters: function () {
      var actual = getQueryResults("FOR e IN arangodb::GRAPH_COMMON_PROPERTIES('bla3', {ageing : true} , {harald : 'meier'},  {}) SORT  ATTRIBUTES(e)[0]  RETURN e");
      assertEqual(actual[0]["UnitTestsAhuacatlVertex2/v5"].length, 1);
      assertEqual(actual[1]["UnitTestsAhuacatlVertex2/v6"].length, 3);

    },

    testCommonPropertiesWithFiltersAndIgnoringKeyHarald: function () {
      var actual = getQueryResults("FOR e IN arangodb::GRAPH_COMMON_PROPERTIES('bla3', {} , {},  {ignoreProperties : 'harald'}) SORT  ATTRIBUTES(e)[0]  RETURN e");

      assertEqual(actual[0]["UnitTestsAhuacatlVertex1/v1"].length, 1);
      assertEqual(actual[1]["UnitTestsAhuacatlVertex1/v2"].length, 1);
      assertEqual(actual[2]["UnitTestsAhuacatlVertex1/v3"].length, 1);

      assertEqual(actual[3]["UnitTestsAhuacatlVertex2/v5"].length, 1);

      assertEqual(actual[4]["UnitTestsAhuacatlVertex2/v6"].length, 1);
      assertEqual(actual[5]["UnitTestsAhuacatlVertex2/v8"].length, 1);

    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for GRAPH_PATHS() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryGeneralPathsTestSuite() {
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop("UnitTestsAhuacatlVertex1");
      db._drop("UnitTestsAhuacatlVertex2");
      db._drop("UnitTestsAhuacatlVertex3");
      db._drop("UnitTestsAhuacatlVertex4");
      db._drop("UnitTestsAhuacatlEdge1");
      db._drop("UnitTestsAhuacatlEdge2");

      var e1 = "UnitTestsAhuacatlEdge1";
      var e2 = "UnitTestsAhuacatlEdge2";

      var vertex1 = db._create("UnitTestsAhuacatlVertex1");
      var vertex2 = db._create("UnitTestsAhuacatlVertex2");
      var vertex3 = db._create("UnitTestsAhuacatlVertex3");
      var vertex4 = db._create("UnitTestsAhuacatlVertex4");
      db._createEdgeCollection(e1);
      db._createEdgeCollection(e2);

      var v1 = vertex1.save({ _key: "v1" });
      var v2 = vertex1.save({ _key: "v2" });
      var v3 = vertex2.save({ _key: "v3" });
      var v4 = vertex2.save({ _key: "v4" });
      var v5 = vertex3.save({ _key: "v5" });
      vertex3.save({ _key: "v6" }); // not used
      var v7 = vertex4.save({ _key: "v7" });

      try {
        db._collection("_graphs").remove("_graphs/bla3");
      } catch (ignore) {
      }
      var g = graph._create(
        "bla3",
        graph._edgeDefinitions(
          graph._relation("UnitTestsAhuacatlEdge1", "UnitTestsAhuacatlVertex1", "UnitTestsAhuacatlVertex1"),
          graph._relation("UnitTestsAhuacatlEdge2",
            ["UnitTestsAhuacatlVertex1", "UnitTestsAhuacatlVertex2"],
            ["UnitTestsAhuacatlVertex3", "UnitTestsAhuacatlVertex4"]
          )
        )
      );

      function makeEdge(from, to, collection) {
        collection.save(from, to, { what: from.split("/")[1] + "->" + to.split("/")[1] });
      }

      makeEdge(v1._id, v2._id, g[e1]);
      makeEdge(v2._id, v1._id, g[e1]);
      makeEdge(v1._id, v5._id, g[e2]);
      makeEdge(v2._id, v5._id, g[e2]);
      makeEdge(v4._id, v7._id, g[e2]);
      makeEdge(v3._id, v5._id, g[e2]);
      graph._registerCompatibilityFunctions();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      db._drop("UnitTestsAhuacatlVertex1");
      db._drop("UnitTestsAhuacatlVertex2");
      db._drop("UnitTestsAhuacatlVertex3");
      db._drop("UnitTestsAhuacatlVertex4");
      db._drop("UnitTestsAhuacatlEdge1");
      db._drop("UnitTestsAhuacatlEdge2");
      db._collection("_graphs").remove("_graphs/bla3");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_PATHS()
    ////////////////////////////////////////////////////////////////////////////////

    testPaths: function () {
      var actual;

      actual = getQueryResults(
        "FOR e IN arangodb::GRAPH_PATHS('bla3') "
        + "LET length = LENGTH(e.edges) "
        + "SORT e.source._key, e.destination._key, length "
        + "RETURN {src: e.source._key, dest: e.destination._key, edges: e.edges, length: length}"
      );
      assertEqual(actual.length, 15);
      actual.forEach(function (p) {
        switch (p.src) {
          case "v1":
          switch (p.dest) {
            case "v1":
            assertEqual(p.length, 0);
            break;
            case "v2":
            assertEqual(p.length, 1);
            assertEqual(p.edges[0].what, "v1->v2");
            break;
            case "v5":
            if (p.length === 1) {
              assertEqual(p.edges[0].what, "v1->v5");

            } else {
              assertEqual(p.length, 2);
              assertEqual(p.edges[0].what, "v1->v2");
              assertEqual(p.edges[1].what, "v2->v5");
            }
            break;
            default:
            fail("Found unknown path");
          }
          break;
          case "v2":
          switch (p.dest) {
            case "v2":
            assertEqual(p.length, 0);
            break;
            case "v1":
            assertEqual(p.length, 1);
            assertEqual(p.edges[0].what, "v2->v1");
            break;
            case "v5":
            if (p.length === 1) {
              assertEqual(p.edges[0].what, "v2->v5");

            } else {
              assertEqual(p.length, 2);
              assertEqual(p.edges[0].what, "v2->v1");
              assertEqual(p.edges[1].what, "v1->v5");
            }
            break;
            default:
            fail("Found unknown path");
          }
          break;
          case "v3":
          switch (p.dest) {
            case "v3":
            assertEqual(p.length, 0);
            break;
            case "v5":
            assertEqual(p.length, 1);
            assertEqual(p.edges[0].what, "v3->v5");
            break;
            default:
            fail("Found unknown path");
          }
          break;
          case "v4":
          switch (p.dest) {
            case "v4":
            assertEqual(p.length, 0);
            break;
            case "v7":
            assertEqual(p.length, 1);
            assertEqual(p.edges[0].what, "v4->v7");
            break;
            default:
            fail("Found unknown path");
          }
          break;
          case "v5":
          case "v6":
          case "v7":
            assertEqual(p.length, 0);
          break;
          default:
          fail("Found unknown path");
        }
      });
    },

    testPathsWithDirectionAnyAndMaxLength1: function () {
      var actual, result = {}, i = 0, ed;

      actual = getQueryResults("FOR e IN arangodb::GRAPH_PATHS('bla3', {direction :'any', minLength :  1 , maxLength:  1}) SORT e.source._key,e.destination._key RETURN [e.source._key,e.destination._key,e.edges]");
      actual.forEach(function (p) {
        i++;
        ed = "";
        p[2].forEach(function (e) {
          ed += "|" + e._from.split("/")[1] + "->" + e._to.split("/")[1];
        });
        result[i + ":" + p[0] + p[1]] = ed;
      });

      assertTrue(result["1:v1v2"] === "|v2->v1" || result["1:v1v2"] === "|v1->v2");
      assertTrue(result["2:v1v2"] === "|v2->v1" || result["2:v1v2"] === "|v1->v2");
      assertEqual(result["3:v1v5"], "|v1->v5");
      assertTrue(result["4:v2v1"] === "|v1->v2" || result["4:v2v1"] === "|v2->v1");
      assertTrue(result["5:v2v1"] === "|v1->v2" || result["5:v2v1"] === "|v2->v1");
      assertEqual(result["6:v2v5"], "|v2->v5");
      assertEqual(result["7:v3v5"], "|v3->v5");
      assertEqual(result["8:v4v7"], "|v4->v7");
      assertEqual(result["9:v5v1"], "|v1->v5");
      assertEqual(result["10:v5v2"], "|v2->v5");
      assertEqual(result["11:v5v3"], "|v3->v5");
      assertEqual(result["12:v7v4"], "|v4->v7");


    },

    testInBoundPaths: function () {
      var actual, result = {}, i = 0, ed;

      actual = getQueryResults("FOR e IN arangodb::GRAPH_PATHS('bla3',  {direction : 'inbound', minLength : 1}) SORT e.source._key,e.destination._key,LENGTH(e.edges) RETURN [e.source._key,e.destination._key,e.edges]");

      actual.forEach(function (p) {
        i++;
        ed = "";
        p[2].forEach(function (e) {
          ed += "|" + e._from.split("/")[1] + "->" + e._to.split("/")[1];
        });
        result[i + ":" + p[0] + p[1]] = ed;
      });

      assertEqual(result["1:v1v2"], "|v2->v1");
      assertEqual(result["2:v2v1"], "|v1->v2");
      assertEqual(result["3:v5v1"], "|v1->v5");
      assertEqual(result["4:v5v1"], "|v2->v5|v1->v2");
      assertEqual(result["5:v5v2"], "|v2->v5");
      assertEqual(result["6:v5v2"], "|v1->v5|v2->v1");
      assertEqual(result["7:v5v3"], "|v3->v5");
      assertEqual(result["8:v7v4"], "|v4->v7");
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for GRAPH_SHORTEST_PATH function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryGeneralTraversalTestSuite() {

  var vertexIds = {};

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop("UnitTests_Berliner");
      db._drop("UnitTests_Hamburger");
      db._drop("UnitTests_Frankfurter");
      db._drop("UnitTests_Leipziger");
      db._drop("UnitTests_KenntAnderenBerliner");
      db._drop("UnitTests_KenntAnderen");

      var KenntAnderenBerliner = "UnitTests_KenntAnderenBerliner";
      var KenntAnderen = "UnitTests_KenntAnderen";

      var Berlin = db._create("UnitTests_Berliner");
      var Hamburg = db._create("UnitTests_Hamburger");
      var Frankfurt = db._create("UnitTests_Frankfurter");
      var Leipzig = db._create("UnitTests_Leipziger");
      db._createEdgeCollection(KenntAnderenBerliner);
      db._createEdgeCollection(KenntAnderen);

      var Anton = Berlin.save({ _key: "Anton", gender: "male", age: 20});
      var Berta = Berlin.save({ _key: "Berta", gender: "female", age: 25});
      var Caesar = Hamburg.save({ _key: "Caesar", gender: "male", age: 30});
      var Dieter = Hamburg.save({ _key: "Dieter", gender: "male", age: 20});
      var Emil = Frankfurt.save({ _key: "Emil", gender: "male", age: 25});
      var Fritz = Frankfurt.save({ _key: "Fritz", gender: "male", age: 30});
      var Gerda = Leipzig.save({ _key: "Gerda", gender: "female", age: 40});

      vertexIds.Anton = Anton._id;
      vertexIds.Berta = Berta._id;
      vertexIds.Caesar = Caesar._id;
      vertexIds.Dieter = Dieter._id;
      vertexIds.Emil = Emil._id;
      vertexIds.Fritz = Fritz._id;
      vertexIds.Gerda = Gerda._id;

      try {
        db._collection("_graphs").remove("_graphs/werKenntWen");
      } catch (ignore) {
      }
      var g = graph._create(
        "werKenntWen",
        graph._edgeDefinitions(
          graph._relation(KenntAnderenBerliner, "UnitTests_Berliner", "UnitTests_Berliner"),
          graph._relation(KenntAnderen,
            ["UnitTests_Hamburger", "UnitTests_Frankfurter", "UnitTests_Berliner", "UnitTests_Leipziger"],
            ["UnitTests_Hamburger", "UnitTests_Frankfurter", "UnitTests_Berliner", "UnitTests_Leipziger"]
          )
        )
      );

      function makeEdge(from, to, collection, distance) {
        collection.save(from, to, { what: from.split("/")[1] + "->" + to.split("/")[1], entfernung: distance});
      }

      makeEdge(Berta._id, Anton._id, g[KenntAnderenBerliner], 0.1);
      makeEdge(Caesar._id, Anton._id, g[KenntAnderen], 350);
      makeEdge(Caesar._id, Berta._id, g[KenntAnderen], 250.1);
      makeEdge(Berta._id, Gerda._id, g[KenntAnderen], 200);
      makeEdge(Gerda._id, Dieter._id, g[KenntAnderen], "blub");
      makeEdge(Dieter._id, Emil._id, g[KenntAnderen], 300);
      makeEdge(Emil._id, Fritz._id, g[KenntAnderen], 0.2);
      graph._registerCompatibilityFunctions();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      graph._drop("werKenntWen", true);
    },

    testGRAPH_SHORTEST_PATH: function () {
      var actual;

      // Caesar -> Berta -> Gerda -> Dieter -> Emil
      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
        " 'UnitTests_Frankfurter/Emil', {direction : 'outbound', algorithm : 'Floyd-Warshall'}) " +
        " RETURN e");
      assertEqual(actual.length, 1, "Exactly one element is returned");
      var path = actual[0];
      assertTrue(path.hasOwnProperty("vertices"), "The path contains all vertices");
      assertTrue(path.hasOwnProperty("edges"), "The path contains all edges");
      assertTrue(path.hasOwnProperty("distance"), "The path contains the distance");
      assertEqual(path.vertices, [
        vertexIds.Caesar, vertexIds.Berta, vertexIds.Gerda, vertexIds.Dieter, vertexIds.Emil
      ], "The correct shortest path is using these vertices");
      assertEqual(path.distance, 4, "The distance is 1 per edge");

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', {}, " +
        "{}, {direction : 'inbound', algorithm : 'Floyd-Warshall'}) SORT e.vertices[0], e.vertices[LENGTH(e.vertices) - 1] " +
        "RETURN {vertices: e.vertices, distance: e.distance}");
      assertEqual(actual.length, 17, "For each pair that has a shortest path one entry is required");
      assertEqual(actual[0], {
        vertices: [vertexIds.Anton, vertexIds.Berta],
        distance: 1
      });
      assertEqual(actual[1], {
        vertices: [vertexIds.Anton, vertexIds.Caesar],
        distance: 1
      });
      assertEqual(actual[2], {
        vertices: [vertexIds.Berta, vertexIds.Caesar],
        distance: 1
      });
      assertEqual(actual[3], {
        vertices: [vertexIds.Emil,vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta],
        distance: 3
      });
      assertEqual(actual[4], {
        vertices: [vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta, vertexIds.Caesar],
        distance: 4
      });
      assertEqual(actual[5], {
        vertices: [vertexIds.Emil, vertexIds.Dieter],
        distance: 1
      });
      assertEqual(actual[6], {
        vertices: [vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda],
        distance: 2
      });
      assertEqual(actual[7], {
        vertices: [vertexIds.Fritz, vertexIds.Emil,vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta],
        distance: 4
      });
      assertEqual(actual[8], {
        vertices: [vertexIds.Fritz, vertexIds.Emil],
        distance: 1
      });
      assertEqual(actual[9], {
        vertices: [vertexIds.Fritz, vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta, vertexIds.Caesar],
        distance: 5
      });
      assertEqual(actual[10], {
        vertices: [vertexIds.Fritz, vertexIds.Emil, vertexIds.Dieter],
        distance: 2
      });
      assertEqual(actual[11], {
        vertices: [vertexIds.Fritz, vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda],
        distance: 3
      });
      assertEqual(actual[12], {
        vertices: [vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta],
        distance: 2
      });
      assertEqual(actual[13], {
        vertices: [vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta, vertexIds.Caesar],
        distance: 3
      });
      assertEqual(actual[14], {
        vertices: [vertexIds.Dieter, vertexIds.Gerda],
        distance: 1
      });
      assertEqual(actual[15], {
        vertices: [vertexIds.Gerda, vertexIds.Berta],
        distance: 1
      });
      assertEqual(actual[16], {
        vertices: [vertexIds.Gerda, vertexIds.Berta, vertexIds.Caesar],
        distance: 2
      });

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', {}, " +
        "{}, {direction : 'inbound', algorithm : 'dijkstra'}) SORT e.vertices[0], e.vertices[LENGTH(e.vertices) - 1] " +
        "RETURN {vertices: e.vertices, distance: e.distance}");
      assertEqual(actual.length, 17, "For each pair that has a shortest path one entry is required");
      assertEqual(actual[0], {
        vertices: [vertexIds.Anton, vertexIds.Berta],
        distance: 1
      });
      assertEqual(actual[1], {
        vertices: [vertexIds.Anton, vertexIds.Caesar],
        distance: 1
      });
      assertEqual(actual[2], {
        vertices: [vertexIds.Berta, vertexIds.Caesar],
        distance: 1
      });
      assertEqual(actual[3], {
        vertices: [vertexIds.Emil,vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta],
        distance: 3
      });
      assertEqual(actual[4], {
        vertices: [vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta, vertexIds.Caesar],
        distance: 4
      });
      assertEqual(actual[5], {
        vertices: [vertexIds.Emil, vertexIds.Dieter],
        distance: 1
      });
      assertEqual(actual[6], {
        vertices: [vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda],
        distance: 2
      });
      assertEqual(actual[7], {
        vertices: [vertexIds.Fritz, vertexIds.Emil,vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta],
        distance: 4
      });
      assertEqual(actual[8], {
        vertices: [vertexIds.Fritz, vertexIds.Emil],
        distance: 1
      });
      assertEqual(actual[9], {
        vertices: [vertexIds.Fritz, vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta, vertexIds.Caesar],
        distance: 5
      });
      assertEqual(actual[10], {
        vertices: [vertexIds.Fritz, vertexIds.Emil, vertexIds.Dieter],
        distance: 2
      });
      assertEqual(actual[11], {
        vertices: [vertexIds.Fritz, vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda],
        distance: 3
      });
      assertEqual(actual[12], {
        vertices: [vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta],
        distance: 2
      });
      assertEqual(actual[13], {
        vertices: [vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta, vertexIds.Caesar],
        distance: 3
      });
      assertEqual(actual[14], {
        vertices: [vertexIds.Dieter, vertexIds.Gerda],
        distance: 1
      });
      assertEqual(actual[15], {
        vertices: [vertexIds.Gerda, vertexIds.Berta],
        distance: 1
      });
      assertEqual(actual[16], {
        vertices: [vertexIds.Gerda, vertexIds.Berta, vertexIds.Caesar],
        distance: 2
      });

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', {}, " +
        "{_id: 'UnitTests_Berliner/Berta'}, {direction : 'inbound', algorithm : 'dijkstra'}) SORT e.vertices[0], e.vertices[LENGTH(e.vertices) - 1] " +
        "RETURN {vertices: e.vertices, distance: e.distance}");
      assertEqual(actual.length, 5, "For each pair that has a shortest path one entry is required");
      assertEqual(actual[0], {
        vertices: [vertexIds.Anton, vertexIds.Berta],
        distance: 1
      });
      assertEqual(actual[1], {
        vertices: [vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta],
        distance: 3
      });
      assertEqual(actual[2], {
        vertices: [vertexIds.Fritz, vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta],
        distance: 4
      });
      assertEqual(actual[3], {
        vertices: [vertexIds.Dieter, vertexIds.Gerda, vertexIds.Berta],
        distance: 2
      });
      assertEqual(actual[4], {
        vertices: [vertexIds.Gerda, vertexIds.Berta],
        distance: 1
      });

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
        " 'UnitTests_Berliner/Anton', {direction : 'outbound',  weight: 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'}) RETURN e.vertices");
      assertEqual(actual[0].length, 3);

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
        " 'UnitTests_Berliner/Anton', {direction : 'outbound', algorithm : 'Floyd-Warshall'}) RETURN e.vertices");
      assertEqual(actual[0].length, 2);

      actual = getQueryResults("FOR e IN arangodb::GRAPH_DISTANCE_TO('werKenntWen', 'UnitTests_Hamburger/Caesar',  'UnitTests_Frankfurter/Emil', " +
        "{direction : 'outbound', weight: 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'}) " +
        "RETURN e");
      assertEqual(actual,
        [
          {
            vertex: vertexIds.Emil,
            startVertex: vertexIds.Caesar,
            distance: 830.1
          }
        ]
      );
    },

    testGRAPH_SHORTEST_PATH_WITH_DIJKSTRA: function () {
      var actual;

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
        " 'UnitTests_Frankfurter/Emil', {direction : 'outbound', algorithm : 'dijkstra'}) RETURN e");

      assertEqual(actual.length, 1, "Exactly one element is returned");
      var path = actual[0];
      assertTrue(path.hasOwnProperty("vertices"), "The path contains all vertices");
      assertTrue(path.hasOwnProperty("edges"), "The path contains all edges");
      assertTrue(path.hasOwnProperty("distance"), "The path contains the distance");
      assertEqual(path.vertices, [
        vertexIds.Caesar, vertexIds.Berta, vertexIds.Gerda, vertexIds.Dieter, vertexIds.Emil
      ], "The correct shortest path is using these vertices");
      assertEqual(path.distance, 4, "The distance is 1 per edge");

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
        "'UnitTests_Berliner/Anton', {direction : 'outbound', algorithm : 'dijkstra'}) " +
        "RETURN e.vertices");
      assertEqual(actual[0].length, 2);

      actual = getQueryResults("FOR e IN arangodb::GRAPH_DISTANCE_TO('werKenntWen', 'UnitTests_Hamburger/Caesar',  'UnitTests_Frankfurter/Emil', " +
        "{direction : 'outbound', weight: 'entfernung', defaultWeight : 80, algorithm : 'dijkstra'}) RETURN e");
      assertEqual(actual,
        [
          {
            vertex: vertexIds.Emil,
            startVertex: vertexIds.Caesar,
            distance: 830.1
          }
        ]
      );
    },

    testGRAPH_SHORTEST_PATH_with_stopAtFirstMatch: function () {
      var actual;

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Frankfurter/Fritz', " +
        " {gender: 'female'}, {direction : 'inbound', stopAtFirstMatch: true}) RETURN e");

      // Find only one match, ignore the second one.
      assertEqual(actual.length, 1);
      // Find the right match
      var path = actual[0];
      assertEqual(path.distance, 3);
      assertEqual(path.vertices, [
        vertexIds.Fritz, vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda
      ]);
    },

    testGRAPH_CLOSENESS: function () {
      var actual;

      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen')");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (0.69).toFixed(2));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (0.92).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0.69).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.92).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.73).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0.55).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (1).toFixed(2));

      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (0.89).toFixed(2));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (0.89).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0.54).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.95).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.63).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0.63).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (1).toFixed(2));


      actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_CLOSENESS('werKenntWen', {gender: 'male'}, {weight : 'entfernung', defaultWeight : 80})");
      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(1), 1890.9);
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(1), 2670.4);
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), 2671.4);
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(1), 3140.9);
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(1), 1770.4);
    },


    testGRAPH_CLOSENESS_OUTBOUND: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {direction : 'outbound', algorithm : 'Floyd-Warshall'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (0.0909).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0.0625).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.3333).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (0.1666).toFixed(2));
    },

    testGRAPH_CLOSENESS_OUTBOUND_WEIGHT: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, direction : 'outbound', algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(6), (0.00012192).toFixed(6));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(6), (0.00006367).toFixed(6));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(6), (0.00033322).toFixed(6));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(6), (1).toFixed(6));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(6), (0.00023803).toFixed(6));
    },

    testGRAPH_CLOSENESS_INBOUND: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {direction : 'inbound', algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (0.5).toFixed(2));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.1666).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0.0666).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (0.3333).toFixed(2));
    },

    testGRAPH_CLOSENESS_INBOUND_WEIGHT: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, direction : 'inbound', algorithm : 'Floyd-Warshall'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(4), (0.999200).toFixed(4));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(4), (1).toFixed(4));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(4), (0).toFixed(4));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(4), (0.280979).toFixed(4));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(4), (0.119659).toFixed(4));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(4), (0.119602).toFixed(4));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(4), (0.3847100).toFixed(4));
    },


    testGRAPH_ECCENTRICITY: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'Floyd-Warshall'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(1), (0.6).toFixed(1));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (0.75).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(1), (0.6).toFixed(1));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.75).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), (0.6).toFixed(1));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (1).toFixed(2));
    },

    testGRAPH_ECCENTRICITY_inbound: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.3333).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.25).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0.2).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (0.5).toFixed(2));
    },

    testGRAPH_ECCENTRICITY_inbound_example: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_ECCENTRICITY('werKenntWen', {gender : 'female'}, {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
      assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 2);
    },

    testGRAPH_ECCENTRICITY_weight: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_ECCENTRICITY('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (0.78).toFixed(2));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (0.78).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0.54).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.85).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.54).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0.54).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (1).toFixed(2));
    },

    /*
    testGRAPH_BETWEENNESS: function () {
      var actual;

      actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
      assertEqual(actual[0]["UnitTests_Berliner/Berta"], 16);
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 10);
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 16);
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 18);


      actual = getQueryResults("RETURN arangodb::GRAPH_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.89);
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.56);
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.89);
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);


      actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
      assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
      assertEqual(actual[0]["UnitTests_Berliner/Berta"], 4);
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 4);
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 6);
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 6);

      actual = getQueryResults("RETURN arangodb::GRAPH_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
      assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.67);
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.67);
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 1);
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);



      actual = getQueryResults("RETURN arangodb::GRAPH_BETWEENNESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
      assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.56);
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.89);
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);


    },
*/

    testGRAPH_DIAMETER_AND_RADIUS: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0], 3);

      actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0].toFixed(1), 450.1);

      actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0], 5);

      actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0].toFixed(1), 830.3);



      actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {direction : 'inbound', algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0], 1);

      actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {direction : 'inbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0].toFixed(1), 250.1);

      actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {direction : 'inbound', algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0], 5);

      actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {direction : 'inbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0].toFixed(1), 830.3);

      actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {direction : 'outbound', algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0], 1);

      actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {direction : 'outbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0].toFixed(1), 0.2);

      actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {direction : 'outbound', algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0], 5);

      actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {direction : 'outbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
      assertEqual(actual[0].toFixed(1), 830.3);
    },

    testGRAPH_SHORTEST_PATHWithExamples: function () {
      var actual;

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', {gender : 'female'},  {gender : 'male', age : 30}, " +
        "{direction : 'any', algorithm : 'Floyd-Warshall'}) SORT e.vertices[0], e.vertices[LENGTH(e.vertices) - 1] " +
        "RETURN e");
      assertEqual(actual.length, 4, "All connected pairs should have one entry.");
      assertEqual(actual[0].vertices, [
        vertexIds.Berta, vertexIds.Gerda, vertexIds.Dieter, vertexIds.Emil, vertexIds.Fritz
      ]);
      assertEqual(actual[0].distance, 4);

      assertEqual(actual[1].vertices, [
        vertexIds.Berta, vertexIds.Caesar
      ]);
      assertEqual(actual[1].distance, 1);

      assertEqual(actual[2].vertices, [
        vertexIds.Gerda, vertexIds.Dieter, vertexIds.Emil, vertexIds.Fritz
      ]);
      assertEqual(actual[2].distance, 3);

      assertEqual(actual[3].vertices, [
        vertexIds.Gerda, vertexIds.Berta, vertexIds.Caesar
      ]);
      assertEqual(actual[3].distance, 2);
    },

    testGRAPH_DISTANCE_TO_WithExamples: function () {
      var actual;
      actual = getQueryResults("FOR e IN arangodb::GRAPH_DISTANCE_TO('werKenntWen', {gender : 'female'},  {gender : 'male', age : 30}, " +
        "{direction : 'any'}) SORT e.startVertex, e.vertex._id SORT e.startVertex, e.vertex RETURN [e.startVertex, e.vertex, e.distance]");
      assertEqual(actual, [
        [
          "UnitTests_Berliner/Berta",
          "UnitTests_Frankfurter/Fritz",
          4
        ],
        [
          "UnitTests_Berliner/Berta",
          "UnitTests_Hamburger/Caesar",
          1

        ],
        [
          "UnitTests_Leipziger/Gerda",
          "UnitTests_Frankfurter/Fritz",
          3
        ],
        [
          "UnitTests_Leipziger/Gerda",
          "UnitTests_Hamburger/Caesar",
          2
        ]
      ]);
    },

    testGRAPH_CLOSENESS_WITH_DIJKSTRA: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (0.69).toFixed(2));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (0.92).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0.69).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.92).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.73).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0.55).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (1).toFixed(2));
    },

    testGRAPH_CLOSENESS_WITH_DIJKSTRA_WEIGHT: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (0.89).toFixed(2));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (0.89).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0.54).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.95).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.63).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0.63).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (1).toFixed(2));
    },


    testGRAPH_CLOSENESS_OUTBOUND_WITH_DIJKSTRA: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra', direction : 'outbound'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (0.0909).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0.0625).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.3333).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (0.1666).toFixed(2));
    },

    testGRAPH_CLOSENESS_OUTBOUND_WITH_DIJKSTRA_WEIGHT: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80, direction : 'outbound'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(6), (0.00012192).toFixed(6));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(6), (0.00006367).toFixed(6));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(6), (0.00033322).toFixed(6));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(6), (1).toFixed(6));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(6), (0.00023803).toFixed(6));
    },


    testGRAPH_CLOSENESS_INBOUND_WITH_DIJKSTRA: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra', direction : 'inbound'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (0.5).toFixed(2));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.1666).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0.0666).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (0.3333).toFixed(2));
    },

    testGRAPH_CLOSENESS_INBOUND_WITH_DIJKSTRA_WEIGHT: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80, direction : 'inbound'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(4), (0.999200).toFixed(4));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(4), (1).toFixed(4));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(4), (0).toFixed(4));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(4), (0.280979).toFixed(4));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(4), (0.119659).toFixed(4));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(4), (0.119602).toFixed(4));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(4), (0.3847100).toFixed(4));

    },


    testGRAPH_ECCENTRICITY_WITH_DIJKSTRA: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'dijkstra'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(1), (0.6).toFixed(1));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (0.75).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(1), (0.6).toFixed(1));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.75).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), (0.6).toFixed(1));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (1).toFixed(2));
    },


    testGRAPH_ECCENTRICITY_WITH_DIJKSTRA_inbound: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'dijkstra', direction : 'inbound'})");

      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (1).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.33).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.25).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), (0.2).toFixed(1));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(1), (0.5).toFixed(1));
    },

    testGRAPH_ECCENTRICITY_WITH_DIJKSTRA_weight: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80})");
      
      assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (0.78).toFixed(2));
      assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (0.78).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (0.54).toFixed(2));
      assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), (0.85).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), (0.54).toFixed(2));
      assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (0.54).toFixed(2));
      assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), (1).toFixed(2));
    },

    testGRAPH_DIAMETER_AND_RADIUS_WITH_DIJKSTRA: function () {
      var actual;
      actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {algorithm : 'dijkstra'})");
      assertEqual(actual[0], 3);

      actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80})");
      assertEqual(actual[0].toFixed(1), 450.1);

      actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {algorithm : 'dijkstra'})");
      assertEqual(actual[0], 5);

      actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80})");
      assertEqual(actual[0].toFixed(1), 830.3);
    },

    testGRAPH_SHORTEST_PATHWithExamples_WITH_DIJKSTRA: function () {
      var actual;

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', {gender : 'female'},  {gender : 'male', age : 30}, " +
        "{direction : 'any', algorithm : 'dijkstra'}) SORT e.vertices[0], e.vertices[LENGTH(e.vertices) - 1] " +
        "RETURN e");
      assertEqual(actual.length, 4, "All connected pairs should have one entry.");
      assertEqual(actual[0].vertices, [
        vertexIds.Berta, vertexIds.Gerda, vertexIds.Dieter, vertexIds.Emil, vertexIds.Fritz
      ]);
      assertEqual(actual[0].distance, 4);

      assertEqual(actual[1].vertices, [
        vertexIds.Berta, vertexIds.Caesar
      ]);
      assertEqual(actual[1].distance, 1);

      assertEqual(actual[2].vertices, [
        vertexIds.Gerda, vertexIds.Dieter, vertexIds.Emil, vertexIds.Fritz
      ]);
      assertEqual(actual[2].distance, 3);

      assertEqual(actual[3].vertices, [
        vertexIds.Gerda, vertexIds.Berta, vertexIds.Caesar
      ]);
      assertEqual(actual[3].distance, 2);
    },

    testGRAPH_DISTANCE_TO_WithExamples_WITH_DIJKSTRA: function () {
      var actual;
      actual = getQueryResults("FOR e IN arangodb::GRAPH_DISTANCE_TO('werKenntWen', {gender : 'female'},  {gender : 'male', age : 30}, " +
        "{direction : 'any'}) SORT e.startVertex, e.vertex SORT e.startVertex, e.vertex RETURN [e.startVertex, e.vertex, e.distance]");
      assertEqual(actual, [
        [
          "UnitTests_Berliner/Berta",
          "UnitTests_Frankfurter/Fritz",
          4
        ],
        [
          "UnitTests_Berliner/Berta",
          "UnitTests_Hamburger/Caesar",
          1

        ],
        [
          "UnitTests_Leipziger/Gerda",
          "UnitTests_Frankfurter/Fritz",
          3
        ],
        [
          "UnitTests_Leipziger/Gerda",
          "UnitTests_Hamburger/Caesar",
          2
        ]
      ]);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for GRAPH_SHORTEST_PATH function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryGeneralCyclesSuite() {

  var vertexIds = {};

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop("UnitTests_Berliner");
      db._drop("UnitTests_Hamburger");
      db._drop("UnitTests_Frankfurter");
      db._drop("UnitTests_Leipziger");
      db._drop("UnitTests_KenntAnderenBerliner");
      db._drop("UnitTests_KenntAnderen");

      var KenntAnderenBerliner = "UnitTests_KenntAnderenBerliner";
      var KenntAnderen = "UnitTests_KenntAnderen";

      var Berlin = db._create("UnitTests_Berliner");
      var Hamburg = db._create("UnitTests_Hamburger");
      var Frankfurt = db._create("UnitTests_Frankfurter");
      var Leipzig = db._create("UnitTests_Leipziger");
      db._createEdgeCollection(KenntAnderenBerliner);
      db._createEdgeCollection(KenntAnderen);

      var Anton = Berlin.save({ _key: "Anton", gender: "male", age: 20});
      var Berta = Berlin.save({ _key: "Berta", gender: "female", age: 25});
      var Caesar = Hamburg.save({ _key: "Caesar", gender: "male", age: 30});
      Hamburg.save({ _key: "Dieter", gender: "male", age: 20});
      Frankfurt.save({ _key: "Emil", gender: "male", age: 25});
      var Fritz = Frankfurt.save({ _key: "Fritz", gender: "male", age: 30});
      Leipzig.save({ _key: "Gerda", gender: "female", age: 40});

      vertexIds.Anton = Anton._id;
      vertexIds.Berta = Berta._id;
      vertexIds.Caesar = Caesar._id;
      vertexIds.Fritz = Fritz._id;

      try {
        db._collection("_graphs").remove("_graphs/werKenntWen");
      } catch (ignore) {
      }
      var g = graph._create(
        "werKenntWen",
        graph._edgeDefinitions(
          graph._relation(KenntAnderenBerliner, "UnitTests_Berliner", "UnitTests_Berliner"),
          graph._relation(KenntAnderen,
            ["UnitTests_Hamburger", "UnitTests_Frankfurter", "UnitTests_Berliner", "UnitTests_Leipziger"],
            ["UnitTests_Hamburger", "UnitTests_Frankfurter", "UnitTests_Berliner", "UnitTests_Leipziger"]
          )
        )
      );

      function makeEdge(from, to, collection, distance) {
        collection.save(from, to, { what: from.split("/")[1] + "->" + to.split("/")[1], entfernung: distance});
      }

      makeEdge(Anton._id, Berta._id, g[KenntAnderen], 7);
      makeEdge(Berta._id, Caesar._id, g[KenntAnderen], 8);
      makeEdge(Anton._id, Fritz._id, g[KenntAnderen], 9);
      makeEdge(Fritz._id, Caesar._id, g[KenntAnderen], 11);
      makeEdge(Caesar._id, Berta._id, g[KenntAnderen], 2);
      makeEdge(Berta._id, Anton._id, g[KenntAnderen], 3);
      graph._registerCompatibilityFunctions();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      db._drop("UnitTests_Berliner");
      db._drop("UnitTests_Hamburger");
      db._drop("UnitTests_Frankfurter");
      db._drop("UnitTests_Leipziger");
      db._drop("UnitTests_KenntAnderenBerliner");
      db._drop("UnitTests_KenntAnderen");
      db._collection("_graphs").remove("_graphs/werKenntWen");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_SHORTEST_PATH()
    ////////////////////////////////////////////////////////////////////////////////

    testCycleGRAPH_SHORTEST_PATH: function () {
      var actual;
      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', {}, " +
        "{}, {direction : 'inbound', algorithm : 'Floyd-Warshall'}) SORT e.vertices[0], e.vertices[LENGTH(e.vertices) - 1] " +
        "RETURN {vertices: e.vertices, distance: e.distance}");
      assertEqual(actual.length, 12, "Expect one entry for every connected pair.");
      assertEqual(actual[0], {
        vertices: [vertexIds.Anton, vertexIds.Berta],
        distance: 1
      });
      assertEqual(actual[1], {
        vertices: [vertexIds.Anton, vertexIds.Berta, vertexIds.Caesar, vertexIds.Fritz],
        distance: 3
      });
      assertEqual(actual[2], {
        vertices: [vertexIds.Anton, vertexIds.Berta, vertexIds.Caesar],
        distance: 2
      });
      assertEqual(actual[3], {
        vertices: [vertexIds.Berta, vertexIds.Anton],
        distance: 1
      });
      assertEqual(actual[4], {
        vertices: [vertexIds.Berta, vertexIds.Caesar, vertexIds.Fritz],
        distance: 2
      });
      assertEqual(actual[5], {
        vertices: [vertexIds.Berta, vertexIds.Caesar],
        distance: 1
      });
      assertEqual(actual[6], {
        vertices: [vertexIds.Fritz, vertexIds.Anton],
        distance: 1
      });
      assertEqual(actual[7], {
        vertices: [vertexIds.Fritz, vertexIds.Anton, vertexIds.Berta],
        distance: 2
      });
      assertEqual(actual[8], {
        vertices: [vertexIds.Fritz, vertexIds.Anton, vertexIds.Berta, vertexIds.Caesar],
        distance: 3
      });
      // we have two possible paths here.
      assertEqual(actual[9].distance, 2);
      assertEqual(actual[9].vertices[0], vertexIds.Caesar);
      assertEqual(actual[9].vertices[2], vertexIds.Anton);
      if (actual[9].vertices[1] !== vertexIds.Berta) {
        assertEqual(actual[9].vertices[1], vertexIds.Fritz);
      }
      assertEqual(actual[10], {
        vertices: [vertexIds.Caesar, vertexIds.Berta],
        distance: 1
      });
      assertEqual(actual[11], {
        vertices: [vertexIds.Caesar, vertexIds.Fritz],
        distance: 1
      });

      actual = getQueryResults("FOR e IN arangodb::GRAPH_SHORTEST_PATH('werKenntWen', {}, " +
        "{}, {direction : 'inbound', algorithm : 'dijkstra'}) SORT e.vertices[0], e.vertices[LENGTH(e.vertices) - 1] " +
        "RETURN {vertices: e.vertices, distance: e.distance}");
      assertEqual(actual.length, 12, "Expect one entry for every connected pair.");
      assertEqual(actual[0], {
        vertices: [vertexIds.Anton, vertexIds.Berta],
        distance: 1
      });
      assertEqual(actual[1], {
        vertices: [vertexIds.Anton, vertexIds.Berta, vertexIds.Caesar, vertexIds.Fritz],
        distance: 3
      });
      assertEqual(actual[2], {
        vertices: [vertexIds.Anton, vertexIds.Berta, vertexIds.Caesar],
        distance: 2
      });
      assertEqual(actual[3], {
        vertices: [vertexIds.Berta, vertexIds.Anton],
        distance: 1
      });
      assertEqual(actual[4], {
        vertices: [vertexIds.Berta, vertexIds.Caesar, vertexIds.Fritz],
        distance: 2
      });
      assertEqual(actual[5], {
        vertices: [vertexIds.Berta, vertexIds.Caesar],
        distance: 1
      });
      assertEqual(actual[6], {
        vertices: [vertexIds.Fritz, vertexIds.Anton],
        distance: 1
      });
      assertEqual(actual[7], {
        vertices: [vertexIds.Fritz, vertexIds.Anton, vertexIds.Berta],
        distance: 2
      });
      assertEqual(actual[8], {
        vertices: [vertexIds.Fritz, vertexIds.Anton, vertexIds.Berta, vertexIds.Caesar],
        distance: 3
      });
      // we have two possible paths here.
      assertEqual(actual[9].distance, 2);
      assertEqual(actual[9].vertices[0], vertexIds.Caesar);
      assertEqual(actual[9].vertices[2], vertexIds.Anton);
      if (actual[9].vertices[1] !== vertexIds.Berta) {
        assertEqual(actual[9].vertices[1], vertexIds.Fritz);
      }
      assertEqual(actual[10], {
        vertices: [vertexIds.Caesar, vertexIds.Berta],
        distance: 1
      });
      assertEqual(actual[11], {
        vertices: [vertexIds.Caesar, vertexIds.Fritz],
        distance: 1
      });
  },

  testCycleGRAPH_CLOSENESS: function () {

    var actual;
    actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_CLOSENESS('werKenntWen', {}, {algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 4);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 4);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 4);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 4);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


    actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");

    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 1);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

    actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");

    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.53);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), 0.94);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);
  },

  testCycleGRAPH_CLOSENESS_OUTBOUND: function () {
    var actual;
    actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_CLOSENESS('werKenntWen',{}, {direction : 'outbound', algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 4);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 4);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 6);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 6);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

    actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, direction : 'outbound', algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(6), (0.67741935).toFixed(6));
    assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(6), (0.913043478).toFixed(6));
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(6), (1).toFixed(6));
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(6), (0).toFixed(6));
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(6), (0).toFixed(6));
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(6), (0.525).toFixed(6));
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(6), (0).toFixed(6));
  },

  testCycleGRAPH_CLOSENESS_INBOUND: function () {
    var actual;
    actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_CLOSENESS('werKenntWen', {}, {direction : 'inbound', algorithm : 'Floyd-Warshall'})");

    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 6);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 4);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 6);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 4);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

    actual = getQueryResults("RETURN arangodb::GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, direction : 'inbound', algorithm : 'Floyd-Warshall'})");

    assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(4), (0.9166666).toFixed(4));
    assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(4), (1).toFixed(4));
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(4), (0.64705882).toFixed(4));
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(4), (0).toFixed(4));
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(4), (0).toFixed(4));
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(4), (0.6285714).toFixed(4));
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(4), (0).toFixed(4));
  },


  testCycleGRAPH_ECCENTRICITY: function () {
    var actual;

    actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_ECCENTRICITY('werKenntWen',{}, {algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 2);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 2);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 2);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 2);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

    actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_ECCENTRICITY('werKenntWen',{}, {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 9);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 12);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 12);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 11);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


    actual = getQueryResults("RETURN arangodb::GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 1);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


    actual = getQueryResults("RETURN arangodb::GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (2/3).toFixed(2));
    assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (1).toFixed(2));
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (1).toFixed(2));
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (2/3).toFixed(2));

    actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_ECCENTRICITY('werKenntWen', {gender : 'female'}, {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 2);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

    actual = getQueryResults("RETURN arangodb::GRAPH_ECCENTRICITY('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), (1).toFixed(2));
    assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), (9 / 12).toFixed(2));
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), (9 / 11).toFixed(2));
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), (9 / 12).toFixed(2));
  },

/*
  testCycleGRAPH_BETWEENNESS: function () {
    var actual;

    actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 1);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

    actual = getQueryResults("RETURN arangodb::GRAPH_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 1);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


    actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 2);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 3.5);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0.5);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 2);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);



    actual = getQueryResults("RETURN arangodb::GRAPH_ABSOLUTE_BETWEENNESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 2);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 2);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"],0 );
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

    actual = getQueryResults("RETURN arangodb::GRAPH_BETWEENNESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
    assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
    assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
    assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
    assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
    assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
    assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


  },
*/

  testCycleGRAPH_DIAMETER_AND_RADIUS: function () {
    var actual;
    actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0], 2);

    actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0], 9);

    actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0], 2);

    actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0], 12);



    actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {direction : 'inbound', algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0], 2);

    actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {direction : 'inbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0], 13);

    actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {direction : 'inbound', algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0], 3);

    actual = getQueryResults("RETURN arangodb::GRAPH_DIAMETER('werKenntWen', {direction : 'inbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0], 16);

    actual = getQueryResults("RETURN arangodb::GRAPH_RADIUS('werKenntWen', {direction : 'outbound', algorithm : 'Floyd-Warshall'})");
    assertEqual(actual[0], 2);

  }
};
}

function ahuacatlQueryMultiCollectionMadnessTestSuite() {
  var gN = "UnitTestsAhuacatlGraph";
  var v1 = "UnitTestsAhuacatlVertex1";
  var v2 = "UnitTestsAhuacatlVertex2";
  var v3 = "UnitTestsAhuacatlVertex3";
  var e1 = "UnitTestsAhuacatlEdge1";
  var e2 = "UnitTestsAhuacatlEdge2";

  var s1;
  var c1;
  var t1;
  var s2;
  var c2;
  var t2;
 
  var AQL_NEIGHBORS = "FOR e IN arangodb::GRAPH_NEIGHBORS(@name, @example, @options) SORT e._id RETURN e";

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop(v1);
      db._drop(v2);
      db._drop(v3);
      db._drop(e1);
      db._drop(e2);

      var vertex1 = db._create(v1);
      var vertex2 = db._create(v2);
      var vertex3 = db._create(v3);

      var edge1 = db._createEdgeCollection(e1);
      var edge2 = db._createEdgeCollection(e2);

      s1 = vertex1.save({ _key: "start"})._id;
      c1 = vertex2.save({ _key: "center"})._id;
      t1 = vertex3.save({ _key: "target"})._id;
      s2 = vertex1.save({ _key: "start2"})._id;
      c2 = vertex2.save({ _key: "center2"})._id;
      t2 = vertex3.save({ _key: "target2"})._id;

      function makeEdge(from, to, collection) {
        collection.save(from, to, {});
      }

      makeEdge(s1, c1, edge1);
      makeEdge(t1, c1, edge2);
      makeEdge(s2, c2, edge1);
      makeEdge(t2, c2, edge2);
      makeEdge(t1, c2, edge2);
      try {
        graph._drop(gN);
      } catch (ignore) {
      }
      graph._create(
        gN,
        graph._edgeDefinitions(
          graph._relation(e1, v1, v2),
          graph._relation(e2, v3, v2)
        )
      );
      graph._registerCompatibilityFunctions();
    },

    tearDownAll: function () {
      graph._drop(gN, true);
    },

    testRestrictedPathHops1: function() {
      var bindVars = {
        name: gN,
        example: s1,
        options: {
          direction : 'any',
          includeData: true,
          minDepth: 2,
          maxDepth: 2,
          vertexCollectionRestriction: v3,
          edgeCollectionRestriction: [e1, e2]
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0]._id, t1);
    },

    testRestrictedPathHops2: function() {
      var bindVars = {
        name: gN,
        example: s2,
        options: {
          direction : 'any',
          includeData: true,
          minDepth: 2,
          maxDepth: 2,
          vertexCollectionRestriction: v3,
          edgeCollectionRestriction: [e1, e2]
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0]._id, t1);
      assertEqual(actual[1]._id, t2);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryGeneralCommonTestSuite);
jsunity.run(ahuacatlQueryGeneralCyclesSuite);
jsunity.run(ahuacatlQueryGeneralTraversalTestSuite);
jsunity.run(ahuacatlQueryGeneralPathsTestSuite);
jsunity.run(ahuacatlQueryGeneralEdgesTestSuite);
jsunity.run(ahuacatlQueryMultiCollectionMadnessTestSuite);

return jsunity.done();
