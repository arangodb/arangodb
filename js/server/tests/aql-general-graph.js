/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual, assertTrue, fail */

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
var db = require("org/arangodb").db;
var graph = require("org/arangodb/general-graph");
var helper = require("org/arangodb/aql-helper");
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

  var AQL_VERTICES = "FOR e IN GRAPH_VERTICES(@name, @example, @options) SORT e._id RETURN e";
  var AQL_EDGES = "FOR e IN GRAPH_EDGES(@name, @example, @options) SORT e.what RETURN e.what";
  var AQL_NEIGHBORS = "FOR e IN GRAPH_NEIGHBORS(@name, @example, @options) SORT e.vertex._id, e.path.edges[0].what RETURN e";

  var startExample = [{hugo : true}, {heinz : 1}];
  var vertexExample = {_key: "v1"};

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
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
      var oprhan = db._create(or);

      vertex1.save({ _key: "v1", hugo: true});
      vertex1.save({ _key: "v2", hugo: true});
      vertex2.save({ _key: "v3", heinz: 1});
      vertex2.save({ _key: "v4" });
      vertex3.save({ _key: "v5" });
      vertex3.save({ _key: "v6" });
      vertex4.save({ _key: "v7" });
      vertex4.save({ _key: "v8", heinz: 1});
      oprhan.save({ _key: "orphan" });

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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
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
        options: {direction : 'any'}
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual[0]._id, v1 + '/v1');
    },

    testVerticesRestricted: function() {
      var bindVars = {
        name: gN,
        example: {},
        options: {
          direction : 'any',
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
        options: {direction : 'any'}
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual.length, 4);
      assertEqual(actual[0]._id, 'UnitTestsAhuacatlVertex1/v1');
      assertEqual(actual[1]._id, 'UnitTestsAhuacatlVertex1/v2');
      assertEqual(actual[2]._id, 'UnitTestsAhuacatlVertex2/v3');
      assertEqual(actual[3]._id, 'UnitTestsAhuacatlVertex4/v8');
    },

    testVerticesWithInboundEdges: function () {
      var bindVars = {
        name: gN,
        example: v3 + "/v5",
        options: {direction : 'inbound'}
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0]._id, 'UnitTestsAhuacatlVertex3/v5');
    },

    testVerticesWithInboundEdgesHasNoInboundEdges: function () {
      var bindVars = {
        name: gN,
        example: v2 + "/v3",
        options: {direction : 'inbound'}
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual.length, 0);
    },

    testVerticesWithInboundEdgesAndExample: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {direction : 'inbound'}
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual.length, 3);
      assertEqual(actual[0]._id, 'UnitTestsAhuacatlVertex1/v1');
      assertEqual(actual[1]._id, 'UnitTestsAhuacatlVertex1/v2');
      assertEqual(actual[2]._id, 'UnitTestsAhuacatlVertex4/v8');
    },

    testVerticesWithInboundEdgesRestrictedHasNoInbound: function() {
      var bindVars = {
        name: gN,
        example: {},
        options: {
          direction : 'inbound',
          vertexCollectionRestriction: v2
        }
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual.length, 0);
    },

    testVerticesWithInboundEdgesRestrictedHasInbound: function() {
      var bindVars = {
        name: gN,
        example: {},
        options: {
          direction : 'inbound',
          vertexCollectionRestriction: v3
        }
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0]._id, v3 + '/v5');
      assertEqual(actual[1]._id, v3 + '/v6');
    },

    testVerticesWithOutboundEdgesRestrictedHasOutbound: function() {
      var bindVars = {
        name: gN,
        example: {},
        options: {
          direction : 'outbound',
          vertexCollectionRestriction: v2
        }
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0]._id, v2 + '/v3');
      assertEqual(actual[1]._id, v2 + '/v4');
    },

    testVerticesWithOutboundEdgesRestrictedHasNoOutbound: function() {
      var bindVars = {
        name: gN,
        example: {},
        options: {
          direction : 'outbound',
          vertexCollectionRestriction: v3
        }
      };
      var actual = getRawQueryResults(AQL_VERTICES, bindVars);
      assertEqual(actual.length, 0);
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
        options: {direction : 'any'}
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
          edgeCollectionRestriction: [e1]
        }
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual, [ "v1->v2", "v2->v1" ]);
    },

    testEdgesAnyStartExample: function () {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {direction : 'any'}
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
        options: {direction : 'inbound'}
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual, [ "v2->v1"]);
    },

    testEdgesInboundStartExample: function() {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {direction : 'inbound'}
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
        options: {direction : 'outbound'}
      };
      var actual = getRawQueryResults(AQL_EDGES, bindVars);
      assertEqual(actual[0], "v1->v2");
      assertEqual(actual[1], "v1->v5");
    },

    testEdgesOutboundStartExample: function() {
      var bindVars = {
        name: gN,
        example: startExample,
        options: {direction : 'outbound'}
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
          direction : 'any'
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 3);
      assertEqual(actual[0].path.edges[0].what, "v1->v2");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v2");
      assertEqual(actual[1].path.edges[0].what, "v2->v1");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v2");
      assertEqual(actual[2].path.edges[0].what, "v1->v5");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v5");
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
      assertEqual(actual[0].path.edges[0].what, "v2->v1");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v2");
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
      assertEqual(actual.length, 10);
      assertEqual(actual[0].path.edges[0].what, "v1->v2");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].path.edges[0].what, "v2->v1");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v1");
      assertEqual(actual[2].path.edges[0].what, "v1->v2");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v2");
      assertEqual(actual[3].path.edges[0].what, "v2->v1");
      assertEqual(actual[3].path.edges.length, 1);
      assertEqual(actual[3].vertex._key, "v2");
      assertEqual(actual[4].path.edges[0].what, "v3->v8");
      assertEqual(actual[4].path.edges.length, 1);
      assertEqual(actual[4].vertex._key, "v3");
      assertEqual(actual[5].path.edges[0].what, "v1->v5");
      assertEqual(actual[5].path.edges.length, 1);
      assertEqual(actual[5].vertex._key, "v5");
      assertEqual(actual[6].path.edges[0].what, "v2->v5");
      assertEqual(actual[6].path.edges.length, 1);
      assertEqual(actual[6].vertex._key, "v5");
      assertEqual(actual[7].path.edges[0].what, "v3->v5");
      assertEqual(actual[7].path.edges.length, 1);
      assertEqual(actual[7].vertex._key, "v5");
      assertEqual(actual[8].path.edges[0].what, "v3->v6");
      assertEqual(actual[8].path.edges.length, 1);
      assertEqual(actual[8].vertex._key, "v6");
      assertEqual(actual[9].path.edges[0].what, "v3->v8");
      assertEqual(actual[9].path.edges.length, 1);
      assertEqual(actual[9].vertex._key, "v8");
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
      assertEqual(actual.length, 3);
      assertEqual(actual[0].path.edges[0].what, "v1->v2");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].path.edges[0].what, "v1->v5");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v1");
      assertEqual(actual[2].path.edges[0].what, "v2->v1");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v1");
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
      assertEqual(actual.length, 6);
      assertEqual(actual[0].path.edges[0].what, "v3->v8");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v3");
      assertEqual(actual[1].path.edges[0].what, "v1->v5");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v5");
      assertEqual(actual[2].path.edges[0].what, "v2->v5");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v5");
      assertEqual(actual[3].path.edges[0].what, "v3->v5");
      assertEqual(actual[3].path.edges.length, 1);
      assertEqual(actual[3].vertex._key, "v5");
      assertEqual(actual[4].path.edges[0].what, "v3->v6");
      assertEqual(actual[4].path.edges.length, 1);
      assertEqual(actual[4].vertex._key, "v6");
      assertEqual(actual[5].path.edges[0].what, "v3->v8");
      assertEqual(actual[5].path.edges.length, 1);
      assertEqual(actual[5].vertex._key, "v8");
    },

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
      assertEqual(actual.length, 8);
      assertEqual(actual[0].path.edges[0].what, "v1->v2");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].path.edges[0].what, "v2->v1");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v1");
      assertEqual(actual[2].path.edges[0].what, "v1->v2");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v2");
      assertEqual(actual[3].path.edges[0].what, "v2->v1");
      assertEqual(actual[3].path.edges.length, 1);
      assertEqual(actual[3].vertex._key, "v2");
      assertEqual(actual[4].path.edges[0].what, "v1->v5");
      assertEqual(actual[4].path.edges.length, 1);
      assertEqual(actual[4].vertex._key, "v5");
      assertEqual(actual[5].path.edges[0].what, "v2->v5");
      assertEqual(actual[5].path.edges.length, 1);
      assertEqual(actual[5].vertex._key, "v5");
      assertEqual(actual[6].path.edges[0].what, "v3->v5");
      assertEqual(actual[6].path.edges.length, 1);
      assertEqual(actual[6].vertex._key, "v5");
      assertEqual(actual[7].path.edges[0].what, "v3->v6");
      assertEqual(actual[7].path.edges.length, 1);
      assertEqual(actual[7].vertex._key, "v6");
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
      assertEqual(actual.length, 4);
      assertEqual(actual[0].path.edges[0].what, "v1->v5");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v5");
      assertEqual(actual[1].path.edges[0].what, "v2->v5");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v5");
      assertEqual(actual[2].path.edges[0].what, "v3->v5");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v5");
      assertEqual(actual[3].path.edges[0].what, "v3->v6");
      assertEqual(actual[3].path.edges.length, 1);
      assertEqual(actual[3].vertex._key, "v6");
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
      assertEqual(actual[0].path.edges[0].what, "v1->v2");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v2");
      assertEqual(actual[1].path.edges[0].what, "v1->v5");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v5");
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
      assertEqual(actual[0].path.edges[0].what, "v1->v2");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v2");
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
      assertEqual(actual.length, 7);
      assertEqual(actual[0].path.edges[0].what, "v2->v1");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].path.edges[0].what, "v1->v2");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v2");
      assertEqual(actual[2].path.edges[0].what, "v1->v5");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v5");
      assertEqual(actual[3].path.edges[0].what, "v2->v5");
      assertEqual(actual[3].path.edges.length, 1);
      assertEqual(actual[3].vertex._key, "v5");
      assertEqual(actual[4].path.edges[0].what, "v3->v5");
      assertEqual(actual[4].path.edges.length, 1);
      assertEqual(actual[4].vertex._key, "v5");
      assertEqual(actual[5].path.edges[0].what, "v3->v6");
      assertEqual(actual[5].path.edges.length, 1);
      assertEqual(actual[5].vertex._key, "v6");
      assertEqual(actual[6].path.edges[0].what, "v3->v8");
      assertEqual(actual[6].path.edges.length, 1);
      assertEqual(actual[6].vertex._key, "v8");
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
      assertEqual(actual[0].path.edges[0].what, "v2->v1");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v1");
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
      assertEqual(actual.length, 5);
      assertEqual(actual[0].path.edges[0].what, "v1->v5");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v5");
      assertEqual(actual[1].path.edges[0].what, "v2->v5");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v5");
      assertEqual(actual[2].path.edges[0].what, "v3->v5");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v5");
      assertEqual(actual[3].path.edges[0].what, "v3->v6");
      assertEqual(actual[3].path.edges.length, 1);
      assertEqual(actual[3].vertex._key, "v6");
      assertEqual(actual[4].path.edges[0].what, "v3->v8");
      assertEqual(actual[4].path.edges.length, 1);
      assertEqual(actual[4].vertex._key, "v8");
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
      assertEqual(actual.length, 6);
      assertEqual(actual[0].path.edges[0].what, "v2->v1");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].path.edges[0].what, "v1->v2");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v2");
      assertEqual(actual[2].path.edges[0].what, "v1->v5");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v5");
      assertEqual(actual[3].path.edges[0].what, "v2->v5");
      assertEqual(actual[3].path.edges.length, 1);
      assertEqual(actual[3].vertex._key, "v5");
      assertEqual(actual[4].path.edges[0].what, "v3->v5");
      assertEqual(actual[4].path.edges.length, 1);
      assertEqual(actual[4].vertex._key, "v5");
      assertEqual(actual[5].path.edges[0].what, "v3->v6");
      assertEqual(actual[5].path.edges.length, 1);
      assertEqual(actual[5].vertex._key, "v6");
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
      assertEqual(actual.length, 4);
      assertEqual(actual[0].path.edges[0].what, "v1->v5");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v5");
      assertEqual(actual[1].path.edges[0].what, "v2->v5");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v5");
      assertEqual(actual[2].path.edges[0].what, "v3->v5");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v5");
      assertEqual(actual[3].path.edges[0].what, "v3->v6");
      assertEqual(actual[3].path.edges.length, 1);
      assertEqual(actual[3].vertex._key, "v6");
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
      assertEqual(actual[0].path.edges[0].what, "v2->v1");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v2");
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
      assertEqual(actual.length, 1);
      assertEqual(actual[0].path.edges[0].what, "v2->v1");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v2");
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
      assertEqual(actual[0].path.edges[0].what, "v1->v2");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].path.edges[0].what, "v2->v1");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v2");
      assertEqual(actual[2].path.edges[0].what, "v3->v8");
      assertEqual(actual[2].path.edges.length, 1);
      assertEqual(actual[2].vertex._key, "v3");
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
      assertEqual(actual.length, 2);
      assertEqual(actual[0].path.edges[0].what, "v1->v2");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].path.edges[0].what, "v1->v5");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v1");
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
      assertEqual(actual[0].path.edges[0].what, "v3->v8");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v3");
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
      assertEqual(actual[0].path.edges[0].what, "v1->v2");
      assertEqual(actual[0].path.edges.length, 1);
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].path.edges[0].what, "v2->v1");
      assertEqual(actual[1].path.edges.length, 1);
      assertEqual(actual[1].vertex._key, "v2");
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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks EDGES() OLD
    ////////////////////////////////////////////////////////////////////////////////

    testEdgesOut: function () {
      var actual;

      actual = getQueryResults("FOR e IN GRAPH_NEIGHBORS('bla3', 'UnitTestsAhuacatlVertex1/v1', {direction : 'outbound' ,minDepth : 1, maxDepth : 3}) SORT e.vertex._key RETURN e");
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].vertex._key, "v2");
      assertEqual(actual[2].vertex._key, "v5");
      assertEqual(actual[3].vertex._key, "v5");

      actual = getQueryResults("FOR e IN GRAPH_NEIGHBORS('bla3', 'UnitTestsAhuacatlVertex1/v1', {neighborExamples : {hugo : true} , direction : 'outbound' ,minDepth : 1, maxDepth : 3}) SORT e.vertex._key RETURN e");
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].vertex._key, "v2");

      actual = getQueryResults("FOR e IN GRAPH_NEIGHBORS('bla3', 'UnitTestsAhuacatlVertex1/v1', {direction : 'outbound'}) SORT e.what RETURN e");
      assertEqual(actual[0].path.edges[0].what, "v1->v2");
      assertEqual(actual[0].vertex._key, "v2");
      assertEqual(actual[1].path.edges[0].what, "v1->v5");
      assertEqual(actual[1].vertex._key, "v5");

      actual = getQueryResults("FOR e IN GRAPH_NEIGHBORS('bla3', { hugo : true } , {direction : 'outbound'}) SORT e.vertex._key RETURN e");
      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].vertex._key, "v2");
      assertEqual(actual[2].vertex._key, "v5");
      assertEqual(actual[3].vertex._key, "v5");


      actual = getQueryResults("FOR e IN GRAPH_NEIGHBORS('bla3', { hugo : true } , {direction : 'outbound', endVertexCollectionRestriction : 'UnitTestsAhuacatlVertex3' }) " +
        "SORT e.vertex._key RETURN e");

      assertEqual(actual[0].vertex._key, "v1");
      assertEqual(actual[1].vertex._key, "v2");
      assertEqual(actual[2].vertex._key, "v5");
      assertEqual(actual[3].vertex._key, "v5");
    }

  };
}


function ahuacatlQueryGeneralCommonTestSuite() {
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop("UnitTestsAhuacatlVertex1");
      db._drop("UnitTestsAhuacatlVertex2");
      db._drop("UnitTestsAhuacatlEdge1");

      var vertex1 = db._create("UnitTestsAhuacatlVertex1");
      var vertex2 = db._create("UnitTestsAhuacatlVertex2");
      var edge1 = db._createEdgeCollection("UnitTestsAhuacatlEdge1");

      vertex1.save({ _key: "v1", hugo: true});
      vertex1.save({ _key: "v2", hugo: true});
      vertex1.save({ _key: "v3", heinz: 1});
      vertex1.save({ _key: "v4", harald: "meier"});
      vertex2.save({ _key: "v5", ageing: true});
      vertex2.save({ _key: "v6", harald: "meier", ageing: true});
      vertex2.save({ _key: "v7", harald: "meier"});
      vertex2.save({ _key: "v8", heinz: 1, harald: "meier"});

      function makeEdge(from, to, collection) {
        collection.save(from, to, { what: from.split("/")[1] + "->" + to.split("/")[1] });
      }

      makeEdge("UnitTestsAhuacatlVertex1/v1", "UnitTestsAhuacatlVertex1/v2", edge1);
      makeEdge("UnitTestsAhuacatlVertex1/v2", "UnitTestsAhuacatlVertex1/v3", edge1);
      makeEdge("UnitTestsAhuacatlVertex1/v3", "UnitTestsAhuacatlVertex2/v5", edge1);
      makeEdge("UnitTestsAhuacatlVertex1/v2", "UnitTestsAhuacatlVertex2/v6", edge1);
      makeEdge("UnitTestsAhuacatlVertex2/v6", "UnitTestsAhuacatlVertex2/v7", edge1);
      makeEdge("UnitTestsAhuacatlVertex1/v4", "UnitTestsAhuacatlVertex2/v7", edge1);
      makeEdge("UnitTestsAhuacatlVertex1/v3", "UnitTestsAhuacatlVertex2/v7", edge1);
      makeEdge("UnitTestsAhuacatlVertex2/v8", "UnitTestsAhuacatlVertex1/v1", edge1);
      makeEdge("UnitTestsAhuacatlVertex1/v3", "UnitTestsAhuacatlVertex2/v5", edge1);
      makeEdge("UnitTestsAhuacatlVertex1/v3", "UnitTestsAhuacatlVertex2/v8", edge1);

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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
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
      var actual = getQueryResults("FOR e IN GRAPH_COMMON_NEIGHBORS('bla3', 'UnitTestsAhuacatlVertex1/v3' , 'UnitTestsAhuacatlVertex2/v6',  {direction : 'any'}) SORT  ATTRIBUTES(e)[0]  RETURN e");

      assertEqual(actual[0]["UnitTestsAhuacatlVertex1/v3"]["UnitTestsAhuacatlVertex2/v6"][0]._id, "UnitTestsAhuacatlVertex1/v2");
      assertEqual(actual[0]["UnitTestsAhuacatlVertex1/v3"]["UnitTestsAhuacatlVertex2/v6"][1]._id, "UnitTestsAhuacatlVertex2/v7");

    },
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_COMMON_NEIGHBORS()
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsIn: function () {
      var actual = getQueryResults("FOR e IN GRAPH_COMMON_NEIGHBORS('bla3', {} , {},  {direction : 'inbound'}, {direction : 'inbound'}) SORT TO_STRING(ATTRIBUTES(e))  RETURN e");

      assertEqual(actual[0]["UnitTestsAhuacatlVertex1/v3"]["UnitTestsAhuacatlVertex2/v6"][0]._id, "UnitTestsAhuacatlVertex1/v2");
      assertEqual(actual[1]["UnitTestsAhuacatlVertex2/v5"]["UnitTestsAhuacatlVertex2/v8"][0]._id, "UnitTestsAhuacatlVertex1/v3");
      assertEqual(actual[1]["UnitTestsAhuacatlVertex2/v5"]["UnitTestsAhuacatlVertex2/v7"][0]._id, "UnitTestsAhuacatlVertex1/v3");


      assertEqual(actual[2]["UnitTestsAhuacatlVertex2/v6"]["UnitTestsAhuacatlVertex1/v3"][0]._id, "UnitTestsAhuacatlVertex1/v2");
      assertEqual(actual[3]["UnitTestsAhuacatlVertex2/v7"]["UnitTestsAhuacatlVertex2/v5"][0]._id, "UnitTestsAhuacatlVertex1/v3");
      assertEqual(actual[3]["UnitTestsAhuacatlVertex2/v7"]["UnitTestsAhuacatlVertex2/v8"][0]._id, "UnitTestsAhuacatlVertex1/v3");

      assertEqual(actual[4]["UnitTestsAhuacatlVertex2/v8"]["UnitTestsAhuacatlVertex2/v5"][0]._id, "UnitTestsAhuacatlVertex1/v3");
      assertEqual(actual[4]["UnitTestsAhuacatlVertex2/v8"]["UnitTestsAhuacatlVertex2/v7"][0]._id, "UnitTestsAhuacatlVertex1/v3");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_COMMON_NEIGHBORS()
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsOut: function () {
      var actual = getQueryResults("FOR e IN GRAPH_COMMON_NEIGHBORS('bla3', { hugo : true } , {heinz : 1}, " +
        " {direction : 'outbound', minDepth : 1, maxDepth : 3}, {direction : 'outbound', minDepth : 1, maxDepth : 3}) SORT e RETURN e");
      assertEqual(Object.keys(actual[0])[0], "UnitTestsAhuacatlVertex1/v2");
      assertEqual(Object.keys(actual[0][Object.keys(actual[0])[0]]), ["UnitTestsAhuacatlVertex2/v8", "UnitTestsAhuacatlVertex1/v3"]);

      assertEqual(actual[0][Object.keys(actual[0])[0]]["UnitTestsAhuacatlVertex2/v8"].length, 3);
      assertEqual(actual[0][Object.keys(actual[0])[0]]["UnitTestsAhuacatlVertex1/v3"].length, 4);

      assertEqual(Object.keys(actual[1])[0], "UnitTestsAhuacatlVertex1/v1");
      assertEqual(Object.keys(actual[1][Object.keys(actual[1])[0]]), ["UnitTestsAhuacatlVertex2/v8", "UnitTestsAhuacatlVertex1/v3"]);

      assertEqual(actual[1][Object.keys(actual[1])[0]]["UnitTestsAhuacatlVertex1/v3"].length, 4);
      assertEqual(actual[1][Object.keys(actual[1])[0]]["UnitTestsAhuacatlVertex2/v8"].length, 3);

    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_COMMON_NEIGHBORS()
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsMixedOptionsDistinctFilters: function () {
      var actual = getQueryResults("FOR e IN GRAPH_COMMON_NEIGHBORS('bla3', {} , {},  " +
        "{direction : 'outbound', vertexCollectionRestriction : 'UnitTestsAhuacatlVertex1'}, " +
        "{direction : 'inbound', minDepth : 1, maxDepth : 2, vertexCollectionRestriction : 'UnitTestsAhuacatlVertex2'}) SORT TO_STRING(e) RETURN e");
      assertEqual(actual.length, 0);
    },

    testCommonNeighborsMixedOptionsFilterBasedOnOneCollectionOnly: function () {
      var actual = getQueryResults("FOR e IN GRAPH_COMMON_NEIGHBORS('bla3', {} , {},  " +
        "{direction : 'outbound', vertexCollectionRestriction : 'UnitTestsAhuacatlVertex2'}, " +
        "{direction : 'inbound', minDepth : 1, maxDepth : 2, vertexCollectionRestriction : 'UnitTestsAhuacatlVertex2'}) SORT TO_STRING(e) RETURN e");

      assertEqual(Object.keys(actual[0])[0], "UnitTestsAhuacatlVertex1/v3");
      assertEqual(Object.keys(actual[0][Object.keys(actual[0])[0]]).sort(), ["UnitTestsAhuacatlVertex1/v1", "UnitTestsAhuacatlVertex1/v2"]);

      assertEqual(actual[0][Object.keys(actual[0])[0]]["UnitTestsAhuacatlVertex1/v1"].length, 1);
      assertEqual(actual[0][Object.keys(actual[0])[0]]["UnitTestsAhuacatlVertex1/v2"].length, 1);

      assertEqual(Object.keys(actual[1])[0], "UnitTestsAhuacatlVertex1/v2");
      assertEqual(Object.keys(actual[1][Object.keys(actual[1])[0]]).sort(), ["UnitTestsAhuacatlVertex2/v7"]);

      assertEqual(actual[1][Object.keys(actual[1])[0]]["UnitTestsAhuacatlVertex2/v7"].length, 1);

    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_COMMON_PROPERTIES()
    ////////////////////////////////////////////////////////////////////////////////

    testCommonProperties: function () {
      var actual = getQueryResults("FOR e IN GRAPH_COMMON_PROPERTIES('bla3', { } , {},  {}) SORT  ATTRIBUTES(e)[0] RETURN e");
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
      var actual = getQueryResults("FOR e IN GRAPH_COMMON_PROPERTIES('bla3', {ageing : true} , {harald : 'meier'},  {}) SORT  ATTRIBUTES(e)[0]  RETURN e");
      assertEqual(actual[0]["UnitTestsAhuacatlVertex2/v5"].length, 1);
      assertEqual(actual[1]["UnitTestsAhuacatlVertex2/v6"].length, 3);

    },

    testCommonPropertiesWithFiltersAndIgnoringKeyHarald: function () {
      var actual = getQueryResults("FOR e IN GRAPH_COMMON_PROPERTIES('bla3', {} , {},  {ignoreProperties : 'harald'}) SORT  ATTRIBUTES(e)[0]  RETURN e");

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

    setUp: function () {
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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
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
        "FOR e IN GRAPH_PATHS('bla3') "
        + "LET length = LENGTH(e.edges) "
        + "SORT e.source._key, e.destination._key, length "
        + "RETURN {src: e.source._key, dest: e.destination._key, edges: e.edges, length: length}"
      );
      assertEqual(actual.length, 12);
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

          default:
          fail("Found unknown path");
        }
      });
    },

    testPathsWithDirectionAnyAndMaxLength1: function () {
      var actual, result = {}, i = 0, ed;

      actual = getQueryResults("FOR e IN GRAPH_PATHS('bla3', {direction :'any', minLength :  1 , maxLength:  1}) SORT e.source._key,e.destination._key RETURN [e.source._key,e.destination._key,e.edges]");
      actual.forEach(function (p) {
        i++;
        ed = "";
        p[2].forEach(function (e) {
          ed += "|" + e._from.split("/")[1] + "->" + e._to.split("/")[1];
        });
        result[i + ":" + p[0] + p[1]] = ed;
      });

      assertEqual(result["1:v1v2"], "|v2->v1");
      assertEqual(result["2:v1v2"], "|v1->v2");
      assertEqual(result["3:v1v5"], "|v1->v5");
      assertEqual(result["4:v2v1"], "|v1->v2");
      assertEqual(result["5:v2v1"], "|v2->v1");
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

      actual = getQueryResults("FOR e IN GRAPH_PATHS('bla3',  {direction : 'inbound', minLength : 1}) SORT e.source._key,e.destination._key RETURN [e.source._key,e.destination._key,e.edges]");

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
      assertEqual(result["5:v5v2"], "|v1->v5|v2->v1");
      assertEqual(result["6:v5v2"], "|v2->v5");
      assertEqual(result["7:v5v3"], "|v3->v5");
      assertEqual(result["8:v7v4"], "|v4->v7");
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for GRAPH_TRAVERSAL() and GRAPH_SHORTEST_PATH function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryGeneralTraversalTestSuite() {
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop("UnitTests_Berliner");
      db._drop("UnitTests_Hamburger");
      db._drop("UnitTests_Frankfurter");
      db._drop("UnitTests_Leipziger");
      db._drop("UnitTests_KenntAnderenBerliner");
      db._drop("UnitTests_KenntAnderen");
      db._collection("_graphs").remove("_graphs/werKenntWen");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_TRAVERSAL()
    ////////////////////////////////////////////////////////////////////////////////

    testGRAPH_TRAVERSALs: function () {
      var actual, result = [];

      actual = getQueryResults("FOR e IN GRAPH_TRAVERSAL('werKenntWen', 'UnitTests_Hamburger/Caesar', 'outbound') RETURN e");
      actual = actual[0];
      actual.forEach(function (s) {
        result.push(s.vertex._key);
      });
      assertEqual(result, [
        "Caesar",
        "Anton",
        "Berta",
        "Anton",
        "Gerda",
        "Dieter",
        "Emil",
        "Fritz"
      ]);
    },

    testGRAPH_TRAVERSALWithExamples: function () {
      var actual;

      actual = getQueryResults("FOR e IN GRAPH_TRAVERSAL('werKenntWen', {}, 'outbound') RETURN e");
      assertEqual(actual.length , 7);
    },

    testGENERAL_GRAPH_TRAVERSAL_TREE: function () {
      var actual, start, middle;

      actual = getQueryResults("FOR e IN GRAPH_TRAVERSAL_TREE('werKenntWen', 'UnitTests_Hamburger/Caesar', 'outbound', 'connected') RETURN e");
      start = actual[0][0][0];

      assertEqual(start._key, "Caesar");
      assertTrue(start.hasOwnProperty("connected"));
      assertTrue(start.connected.length === 2);
      assertEqual(start.connected[0]._key, "Anton");
      assertEqual(start.connected[1]._key, "Berta");

      assertTrue(!start.connected[0].hasOwnProperty("connected"));
      assertTrue(start.connected[1].hasOwnProperty("connected"));

      middle = start.connected[1];

      assertTrue(middle.connected.length === 2);
      assertEqual(middle.connected[0]._key, "Anton");
      assertEqual(middle.connected[1]._key, "Gerda");

      assertTrue(!middle.connected[0].hasOwnProperty("connected"));
      assertTrue(middle.connected[1].hasOwnProperty("connected"));

      middle = middle.connected[1];
      assertTrue(middle.connected.length === 1);
      assertEqual(middle.connected[0]._key, "Dieter");

      middle = middle.connected[0];

      assertTrue(middle.connected.length === 1);
      assertEqual(middle.connected[0]._key, "Emil");

      middle = middle.connected[0];
      assertTrue(middle.connected.length === 1);
      assertEqual(middle.connected[0]._key, "Fritz");

    },
    testGRAPH_SHORTEST_PATH: function () {
      var actual;

      actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
        " 'UnitTests_Frankfurter/Emil', {direction : 'outbound', algorithm : 'Floyd-Warshall'}) SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id]");
      assertEqual(actual,
        [
          [
            "UnitTests_Hamburger/Caesar",
            "UnitTests_Frankfurter/Emil"
          ]
        ]
      );

      actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', {}, " +
        "{}, {direction : 'inbound', algorithm : 'Floyd-Warshall'}) SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id, e.distance]");
      assertEqual(actual, [
        [
          "UnitTests_Berliner/Anton",
          "UnitTests_Berliner/Anton",
          0
        ],
        [
          "UnitTests_Berliner/Anton",
          "UnitTests_Berliner/Berta",
          1
        ],
        [
          "UnitTests_Berliner/Anton",
          "UnitTests_Hamburger/Caesar",
          1
        ],
        [
          "UnitTests_Berliner/Berta",
          "UnitTests_Berliner/Berta",
          0
        ],
        [
          "UnitTests_Berliner/Berta",
          "UnitTests_Hamburger/Caesar",
          1
        ],
        [
          "UnitTests_Frankfurter/Emil",
          "UnitTests_Berliner/Berta",
          3
        ],
        [
          "UnitTests_Frankfurter/Emil",
          "UnitTests_Frankfurter/Emil",
          0
        ],
        [
          "UnitTests_Frankfurter/Emil",
          "UnitTests_Hamburger/Caesar",
          4
        ],
        [
          "UnitTests_Frankfurter/Emil",
          "UnitTests_Hamburger/Dieter",
          1
        ],
        [
          "UnitTests_Frankfurter/Emil",
          "UnitTests_Leipziger/Gerda",
          2
        ],
        [
          "UnitTests_Frankfurter/Fritz",
          "UnitTests_Berliner/Berta",
          4
        ],
        [
          "UnitTests_Frankfurter/Fritz",
          "UnitTests_Frankfurter/Emil",
          1
        ],
        [
          "UnitTests_Frankfurter/Fritz",
          "UnitTests_Frankfurter/Fritz",
          0
        ],
        [
          "UnitTests_Frankfurter/Fritz",
          "UnitTests_Hamburger/Caesar",
          5
        ],
        [
          "UnitTests_Frankfurter/Fritz",
          "UnitTests_Hamburger/Dieter",
          2
        ],
        [
          "UnitTests_Frankfurter/Fritz",
          "UnitTests_Leipziger/Gerda",
          3
        ],
        [
          "UnitTests_Hamburger/Caesar",
          "UnitTests_Hamburger/Caesar",
          0
        ],
        [
          "UnitTests_Hamburger/Dieter",
          "UnitTests_Berliner/Berta",
          2
        ],
        [
          "UnitTests_Hamburger/Dieter",
          "UnitTests_Hamburger/Caesar",
          3
        ],
        [
          "UnitTests_Hamburger/Dieter",
          "UnitTests_Hamburger/Dieter",
          0
        ],
        [
          "UnitTests_Hamburger/Dieter",
          "UnitTests_Leipziger/Gerda",
          1
        ],
        [
          "UnitTests_Leipziger/Gerda",
          "UnitTests_Berliner/Berta",
          1
        ],
        [
          "UnitTests_Leipziger/Gerda",
          "UnitTests_Hamburger/Caesar",
          2
        ],
        [
          "UnitTests_Leipziger/Gerda",
          "UnitTests_Leipziger/Gerda",
          0
        ]
      ]
    );

    actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', {}, " +
      "{}, {direction : 'inbound', algorithm : 'djikstra'}) SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id, e.distance]");
    assertEqual(actual, [
      [
        "UnitTests_Berliner/Anton",
        "UnitTests_Berliner/Anton",
        0
      ],
      [
        "UnitTests_Berliner/Anton",
        "UnitTests_Berliner/Berta",
        1
      ],
      [
        "UnitTests_Berliner/Anton",
        "UnitTests_Hamburger/Caesar",
        1
      ],
      [
        "UnitTests_Berliner/Berta",
        "UnitTests_Berliner/Berta",
        0
      ],
      [
        "UnitTests_Berliner/Berta",
        "UnitTests_Hamburger/Caesar",
        1
      ],
      [
        "UnitTests_Frankfurter/Emil",
        "UnitTests_Berliner/Berta",
        3
      ],
      [
        "UnitTests_Frankfurter/Emil",
        "UnitTests_Frankfurter/Emil",
        0
      ],
      [
        "UnitTests_Frankfurter/Emil",
        "UnitTests_Hamburger/Caesar",
        4
      ],
      [
        "UnitTests_Frankfurter/Emil",
        "UnitTests_Hamburger/Dieter",
        1
      ],
      [
        "UnitTests_Frankfurter/Emil",
        "UnitTests_Leipziger/Gerda",
        2
      ],
      [
        "UnitTests_Frankfurter/Fritz",
        "UnitTests_Berliner/Berta",
        4
      ],
      [
        "UnitTests_Frankfurter/Fritz",
        "UnitTests_Frankfurter/Emil",
        1
      ],
      [
        "UnitTests_Frankfurter/Fritz",
        "UnitTests_Frankfurter/Fritz",
        0
      ],
      [
        "UnitTests_Frankfurter/Fritz",
        "UnitTests_Hamburger/Caesar",
        5
      ],
      [
        "UnitTests_Frankfurter/Fritz",
        "UnitTests_Hamburger/Dieter",
        2
      ],
      [
        "UnitTests_Frankfurter/Fritz",
        "UnitTests_Leipziger/Gerda",
        3
      ],
      [
        "UnitTests_Hamburger/Caesar",
        "UnitTests_Hamburger/Caesar",
        0
      ],
      [
        "UnitTests_Hamburger/Dieter",
        "UnitTests_Berliner/Berta",
        2
      ],
      [
        "UnitTests_Hamburger/Dieter",
        "UnitTests_Hamburger/Caesar",
        3
      ],
      [
        "UnitTests_Hamburger/Dieter",
        "UnitTests_Hamburger/Dieter",
        0
      ],
      [
        "UnitTests_Hamburger/Dieter",
        "UnitTests_Leipziger/Gerda",
        1
      ],
      [
        "UnitTests_Leipziger/Gerda",
        "UnitTests_Berliner/Berta",
        1
      ],
      [
        "UnitTests_Leipziger/Gerda",
        "UnitTests_Hamburger/Caesar",
        2
      ],
      [
        "UnitTests_Leipziger/Gerda",
        "UnitTests_Leipziger/Gerda",
        0
      ]
    ]
  );

  actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', {}, " +
    "{_id : 'UnitTests_Berliner/Berta'}, {direction : 'inbound', algorithm : 'djikstra'}) SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id, e.distance]");
  assertEqual(actual,[
    [
      "UnitTests_Berliner/Anton",
      "UnitTests_Berliner/Berta",
      1
    ],
    [
      "UnitTests_Berliner/Berta",
      "UnitTests_Berliner/Berta",
      0
    ],
    [
      "UnitTests_Frankfurter/Emil",
      "UnitTests_Berliner/Berta",
      3
    ],
    [
      "UnitTests_Frankfurter/Fritz",
      "UnitTests_Berliner/Berta",
      4
    ],
    [
      "UnitTests_Hamburger/Dieter",
      "UnitTests_Berliner/Berta",
      2
    ],
    [
      "UnitTests_Leipziger/Gerda",
      "UnitTests_Berliner/Berta",
      1
    ]
  ]
);


actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
  " 'UnitTests_Berliner/Anton', {direction : 'outbound',  weight: 'entfernung', algorithm : 'Floyd-Warshall'}) SORT e.startVertex, e.vertex._id RETURN e.paths[0].vertices");
assertEqual(actual[0].length, 3);

actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
  " 'UnitTests_Berliner/Anton', {direction : 'outbound', algorithm : 'Floyd-Warshall'}) SORT e.startVertex, e.vertex._id RETURN e.paths[0].vertices");
assertEqual(actual[0].length, 2);

actual = getQueryResults("FOR e IN GRAPH_DISTANCE_TO('werKenntWen', 'UnitTests_Hamburger/Caesar',  'UnitTests_Frankfurter/Emil', " +
  "{direction : 'outbound', weight: 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'}) RETURN [e.startVertex, e.vertex._id, e.distance]");
assertEqual(actual,
  [
    [
      "UnitTests_Hamburger/Caesar",
      "UnitTests_Frankfurter/Emil",
      830.1
    ]
  ]
);


},

testGRAPH_SHORTEST_PATH_WITH_DIJSKTRA: function () {
  var actual;

  actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
    " 'UnitTests_Frankfurter/Emil', {direction : 'outbound', algorithm : 'dijkstra'}) SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id]");
  assertEqual(actual,
    [
      [
        "UnitTests_Hamburger/Caesar",
        "UnitTests_Frankfurter/Emil"
      ]
    ]
  );

  actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
    " 'UnitTests_Berliner/Anton', {direction : 'outbound',  weight: 'entfernung', algorithm : 'dijkstra'}) SORT e.startVertex, e.vertex._id RETURN e.paths[0].vertices");
  assertEqual(actual[0].length, 3);

  actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', 'UnitTests_Hamburger/Caesar', " +
    " 'UnitTests_Berliner/Anton', {direction : 'outbound', algorithm : 'dijkstra'}) SORT e.startVertex, e.vertex._id RETURN e.path.vertices");
  assertEqual(actual[0].length, 2);

  actual = getQueryResults("FOR e IN GRAPH_DISTANCE_TO('werKenntWen', 'UnitTests_Hamburger/Caesar',  'UnitTests_Frankfurter/Emil', " +
    "{direction : 'outbound', weight: 'entfernung', defaultWeight : 80, algorithm : 'dijkstra'}) RETURN [e.startVertex, e.vertex._id, e.distance]");
  assertEqual(actual,
    [
      [
        "UnitTests_Hamburger/Caesar",
        "UnitTests_Frankfurter/Emil",
        830.1
      ]
    ]
  );


},

testGRAPH_CLOSENESS: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.69);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.92);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.73);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.55);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), 0.69);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.92);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);


  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.89);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.89);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.63);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.63);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), 0.54);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.95);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);


  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_CLOSENESS('werKenntWen', {gender: 'male'}, {weight : 'entfernung', defaultWeight : 80})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(1), 1890.9);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(1), 2670.4);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), 2671.4);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(1), 3140.9);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(1), 1770.4);
},


testGRAPH_CLOSENESS_OUTBOUND: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {direction : 'outbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.94);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(1), 0.3);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.46);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), 0.56);


  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, direction : 'outbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(1), 0.5);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(3), 0.001);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(3), 0.001);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(3), 0.002);
},

testGRAPH_CLOSENESS_INBOUND: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {direction : 'inbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.88);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.44);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.91);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(1), 0.8);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), 0.66);


  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, direction : 'inbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(4), 0.0004);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(4), 0.0009);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), 0.5);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(3), 0.002);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(4), 0.0007);
},


testGRAPH_ECCENTRICITY: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(1), 0.6);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.75);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.75);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), 0.6);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(1), 0.6);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 1);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);

  actual = getQueryResults("RETURN GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.25);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), 0.2);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.33);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(1), 0.5);


  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_ECCENTRICITY('werKenntWen', {gender : 'female'}, {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 2);


  actual = getQueryResults("RETURN GRAPH_ECCENTRICITY('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.78);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.78);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.54);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.54);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), 0.54);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.85);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);


},

testGRAPH_BETWEENNESS: function () {
  var actual;

  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 16);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 10);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 16);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 18);


  actual = getQueryResults("RETURN GRAPH_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");

  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.89);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.56);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.89);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);


  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 4);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 4);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 6);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 6);

  actual = getQueryResults("RETURN GRAPH_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.67);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.67);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 1);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);



  actual = getQueryResults("RETURN GRAPH_BETWEENNESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.56);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.89);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);


},

testGRAPH_DIAMETER_AND_RADIUS: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 3);

  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0].toFixed(1), 450.1);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 5);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0].toFixed(1), 830.3);



  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {direction : 'inbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 1);

  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {direction : 'inbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0].toFixed(1), 250.1);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {direction : 'inbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 5);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {direction : 'inbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0].toFixed(1), 830.3);

  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {direction : 'outbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 1);

  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {direction : 'outbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0].toFixed(1), 0.2);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {direction : 'outbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 5);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {direction : 'outbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0].toFixed(1), 830.3);



},

testGRAPH_SHORTEST_PATHWithExamples: function () {
  var actual;

  actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', {gender : 'female'},  {gender : 'male', age : 30}, " +
    "{direction : 'any', algorithm : 'Floyd-Warshall'}) SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id]");
  assertEqual(actual, [
    [
      "UnitTests_Berliner/Berta",
      "UnitTests_Frankfurter/Fritz"
    ],
    [
      "UnitTests_Berliner/Berta",
      "UnitTests_Hamburger/Caesar"
    ],
    [
      "UnitTests_Leipziger/Gerda",
      "UnitTests_Frankfurter/Fritz"
    ],
    [
      "UnitTests_Leipziger/Gerda",
      "UnitTests_Hamburger/Caesar"
    ]
  ]
);

actual = getQueryResults("FOR e IN GRAPH_DISTANCE_TO('werKenntWen', {gender : 'female'},  {gender : 'male', age : 30}, " +
  "{direction : 'any'}) SORT e.startVertex, e.vertex._id SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id, e.distance]");
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
]
);


},

testGRAPH_CLOSENESS_WITH_DIJSKTRA: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra'})");

  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.69);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.92);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.73);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.55);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), 0.69);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.92);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);


  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.89);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.89);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.63);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.63);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), 0.54);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.95);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);

},


testGRAPH_CLOSENESS_OUTBOUND_WITH_DIJSKTRA: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra', direction : 'outbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.94);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(1), 0.3);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.46);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), 0.56);


  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80, direction : 'outbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 0);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(1), 0.5);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(3), 0.001);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(3), 0.001);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(3), 0.002);
},


testGRAPH_CLOSENESS_INBOUND_WITH_DIJSKTRA: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra', direction : 'inbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.88);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.44);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.91);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(1), 0.8);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(2), 0.66);

  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80, direction : 'inbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(4), 0.0004);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(4), 0.0009);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), 0.5);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(3), 0.002);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(4), 0.0007);

},


testGRAPH_ECCENTRICITY_WITH_DIJSKTRA: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'dijkstra'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(1), 0.6);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.75);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.75);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), 0.6);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(1), 0.6);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 1);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);


  actual = getQueryResults("RETURN GRAPH_ECCENTRICITY('werKenntWen')");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(1), 0.6);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.75);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.75);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), 0.6);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(1), 0.6);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 1);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);


  actual = getQueryResults("RETURN GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'dijkstra', direction : 'inbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.25);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), 0.2);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.33);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"].toFixed(1), 0.5);

  actual = getQueryResults("RETURN GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.78);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.78);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"].toFixed(2), 0.54);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.54);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), 0.54);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"].toFixed(2), 0.85);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 1);
},

testGRAPH_DIAMETER_AND_RADIUS_WITH_DIJSKTRA: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {algorithm : 'dijkstra'})");
  assertEqual(actual[0], 3);

  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80})");
  assertEqual(actual[0].toFixed(1), 450.1);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {algorithm : 'dijkstra'})");
  assertEqual(actual[0], 5);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {algorithm : 'dijkstra', weight : 'entfernung', defaultWeight : 80})");
  assertEqual(actual[0].toFixed(1), 830.3);
},

testGRAPH_SHORTEST_PATHWithExamples_WITH_DIJSKTRA: function () {
  var actual;

  actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', {gender : 'female'},  {gender : 'male', age : 30}, " +
    "{direction : 'any'}) SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id]");
  assertEqual(actual, [
    [
      "UnitTests_Berliner/Berta",
      "UnitTests_Frankfurter/Fritz"
    ],
    [
      "UnitTests_Berliner/Berta",
      "UnitTests_Hamburger/Caesar"
    ],
    [
      "UnitTests_Leipziger/Gerda",
      "UnitTests_Frankfurter/Fritz"
    ],
    [
      "UnitTests_Leipziger/Gerda",
      "UnitTests_Hamburger/Caesar"
    ]
  ]
);

actual = getQueryResults("FOR e IN GRAPH_DISTANCE_TO('werKenntWen', {gender : 'female'},  {gender : 'male', age : 30}, " +
  "{direction : 'any'}) SORT e.startVertex, e.vertex._id SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id, e.distance]");
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
]
);


}
};
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for GRAPH_TRAVERSAL() and GRAPH_SHORTEST_PATH function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryGeneralCyclesSuite() {
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop("UnitTests_Berliner");
      db._drop("UnitTests_Hamburger");
      db._drop("UnitTests_Frankfurter");
      db._drop("UnitTests_Leipziger");
      db._drop("UnitTests_KenntAnderenBerliner");
      db._drop("UnitTests_KenntAnderen");
      db._collection("_graphs").remove("_graphs/werKenntWen");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks GRAPH_TRAVERSAL()
    ////////////////////////////////////////////////////////////////////////////////

    testGRAPH_SHORTEST_PATH: function () {
      var actual;

      actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', {}, " +
        "{}, {direction : 'inbound', algorithm : 'Floyd-Warshall'}) SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id, e.distance]");
      assertEqual(actual, [
        [
          "UnitTests_Berliner/Anton",
          "UnitTests_Berliner/Anton",
          0
        ],
        [
          "UnitTests_Berliner/Anton",
          "UnitTests_Berliner/Berta",
          1
        ],
        [
          "UnitTests_Berliner/Anton",
          "UnitTests_Frankfurter/Fritz",
          3
        ],
        [
          "UnitTests_Berliner/Anton",
          "UnitTests_Hamburger/Caesar",
          2
        ],
        [
          "UnitTests_Berliner/Berta",
          "UnitTests_Berliner/Anton",
          1
        ],
        [
          "UnitTests_Berliner/Berta",
          "UnitTests_Berliner/Berta",
          0
        ],
        [
          "UnitTests_Berliner/Berta",
          "UnitTests_Frankfurter/Fritz",
          2
        ],
        [
          "UnitTests_Berliner/Berta",
          "UnitTests_Hamburger/Caesar",
          1
        ],
        [
          "UnitTests_Frankfurter/Emil",
          "UnitTests_Frankfurter/Emil",
          0
        ],
        [
          "UnitTests_Frankfurter/Fritz",
          "UnitTests_Berliner/Anton",
          1
        ],
        [
          "UnitTests_Frankfurter/Fritz",
          "UnitTests_Berliner/Berta",
          2
        ],
        [
          "UnitTests_Frankfurter/Fritz",
          "UnitTests_Frankfurter/Fritz",
          0
        ],
        [
          "UnitTests_Frankfurter/Fritz",
          "UnitTests_Hamburger/Caesar",
          3
        ],
        [
          "UnitTests_Hamburger/Caesar",
          "UnitTests_Berliner/Anton",
          2
        ],
        [
          "UnitTests_Hamburger/Caesar",
          "UnitTests_Berliner/Berta",
          1
        ],
        [
          "UnitTests_Hamburger/Caesar",
          "UnitTests_Frankfurter/Fritz",
          1
        ],
        [
          "UnitTests_Hamburger/Caesar",
          "UnitTests_Hamburger/Caesar",
          0
        ],
        [
          "UnitTests_Hamburger/Dieter",
          "UnitTests_Hamburger/Dieter",
          0
        ],
        [
          "UnitTests_Leipziger/Gerda",
          "UnitTests_Leipziger/Gerda",
          0
        ]
      ]
    );

    actual = getQueryResults("FOR e IN GRAPH_SHORTEST_PATH('werKenntWen', {}, " +
      "{}, {direction : 'inbound', algorithm : 'djikstra'}) SORT e.startVertex, e.vertex._id RETURN [e.startVertex, e.vertex._id, e.distance]");
    assertEqual(actual, [
      [
        "UnitTests_Berliner/Anton",
        "UnitTests_Berliner/Anton",
        0
      ],
      [
        "UnitTests_Berliner/Anton",
        "UnitTests_Berliner/Berta",
        1
      ],
      [
        "UnitTests_Berliner/Anton",
        "UnitTests_Frankfurter/Fritz",
        3
      ],
      [
        "UnitTests_Berliner/Anton",
        "UnitTests_Hamburger/Caesar",
        2
      ],
      [
        "UnitTests_Berliner/Berta",
        "UnitTests_Berliner/Anton",
        1
      ],
      [
        "UnitTests_Berliner/Berta",
        "UnitTests_Berliner/Berta",
        0
      ],
      [
        "UnitTests_Berliner/Berta",
        "UnitTests_Frankfurter/Fritz",
        2
      ],
      [
        "UnitTests_Berliner/Berta",
        "UnitTests_Hamburger/Caesar",
        1
      ],
      [
        "UnitTests_Frankfurter/Emil",
        "UnitTests_Frankfurter/Emil",
        0
      ],
      [
        "UnitTests_Frankfurter/Fritz",
        "UnitTests_Berliner/Anton",
        1
      ],
      [
        "UnitTests_Frankfurter/Fritz",
        "UnitTests_Berliner/Berta",
        2
      ],
      [
        "UnitTests_Frankfurter/Fritz",
        "UnitTests_Frankfurter/Fritz",
        0
      ],
      [
        "UnitTests_Frankfurter/Fritz",
        "UnitTests_Hamburger/Caesar",
        3
      ],
      [
        "UnitTests_Hamburger/Caesar",
        "UnitTests_Berliner/Anton",
        2
      ],
      [
        "UnitTests_Hamburger/Caesar",
        "UnitTests_Berliner/Berta",
        1
      ],
      [
        "UnitTests_Hamburger/Caesar",
        "UnitTests_Frankfurter/Fritz",
        1
      ],
      [
        "UnitTests_Hamburger/Caesar",
        "UnitTests_Hamburger/Caesar",
        0
      ],
      [
        "UnitTests_Hamburger/Dieter",
        "UnitTests_Hamburger/Dieter",
        0
      ],
      [
        "UnitTests_Leipziger/Gerda",
        "UnitTests_Leipziger/Gerda",
        0
      ]
    ]
  );
},

testGRAPH_CLOSENESS: function () {

  var actual;
  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_CLOSENESS('werKenntWen', {}, {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 4);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 4);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 4);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 4);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");

  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.53);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), 0.94);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);
},


testGRAPH_CLOSENESS_OUTBOUND: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_CLOSENESS('werKenntWen',{}, {direction : 'outbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 4);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 4);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 6);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 6);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, direction : 'outbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.42);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(1), 0.7);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(1), 0.3);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);
},

testGRAPH_CLOSENESS_INBOUND: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_CLOSENESS('werKenntWen', {}, {direction : 'inbound', algorithm : 'Floyd-Warshall'})");

  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 6);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 4);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 6);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 4);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

  actual = getQueryResults("RETURN GRAPH_CLOSENESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, direction : 'inbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.83);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.37);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), 0.39);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);
},


testGRAPH_ECCENTRICITY: function () {
  var actual;

  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_ECCENTRICITY('werKenntWen',{}, {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 2);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 2);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 2);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 2);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_ECCENTRICITY('werKenntWen',{}, {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 9);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 12);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 12);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 11);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


  actual = getQueryResults("RETURN GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


  actual = getQueryResults("RETURN GRAPH_ECCENTRICITY('werKenntWen', {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"].toFixed(2), 0.67);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.67);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);



  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_ECCENTRICITY('werKenntWen', {gender : 'female'}, {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 2);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


  actual = getQueryResults("RETURN GRAPH_ECCENTRICITY('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"].toFixed(2), 0.75);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"].toFixed(2), 0.75);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"].toFixed(2), 0.82);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


},

testGRAPH_BETWEENNESS: function () {
  var actual;

  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

  actual = getQueryResults("RETURN GRAPH_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 1);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_BETWEENNESS('werKenntWen', {algorithm : 'Floyd-Warshall', direction : 'inbound'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 2);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 3.5);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0.5);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 2);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);



  actual = getQueryResults("RETURN GRAPH_ABSOLUTE_BETWEENNESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 2);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 2);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"],0 );
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);

  actual = getQueryResults("RETURN GRAPH_BETWEENNESS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0]["UnitTests_Berliner/Anton"], 1);
  assertEqual(actual[0]["UnitTests_Berliner/Berta"], 1);
  assertEqual(actual[0]["UnitTests_Frankfurter/Emil"], 0);
  assertEqual(actual[0]["UnitTests_Frankfurter/Fritz"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Caesar"], 0);
  assertEqual(actual[0]["UnitTests_Hamburger/Dieter"], 0);
  assertEqual(actual[0]["UnitTests_Leipziger/Gerda"], 0);


},

testGRAPH_DIAMETER_AND_RADIUS: function () {
  var actual;
  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 2);

  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 9);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 2);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 12);



  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {direction : 'inbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 2);

  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {direction : 'inbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 13);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {direction : 'inbound', algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 3);

  actual = getQueryResults("RETURN GRAPH_DIAMETER('werKenntWen', {direction : 'inbound', weight : 'entfernung', defaultWeight : 80, algorithm : 'Floyd-Warshall'})");
  assertEqual(actual[0], 16);

  actual = getQueryResults("RETURN GRAPH_RADIUS('werKenntWen', {direction : 'outbound', algorithm : 'Floyd-Warshall'})");
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
 
  var AQL_NEIGHBORS = "FOR e IN GRAPH_NEIGHBORS(@name, @example, @options) SORT e.vertex._id, e.path.edges[0].what RETURN e";

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
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
    },

    tearDown: function () {
      graph._drop(gN, true);
    },

    testRestrictedPathHops1: function() {
      var bindVars = {
        name: gN,
        example: s1,
        options: {
          direction : 'any',
          minDepth: 2,
          maxDepth: 2,
          vertexCollectionRestriction: v3,
          edgeCollectionRestriction: [e1, e2]
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0].vertex._id, t1);
    },

    testRestrictedPathHops2: function() {
      var bindVars = {
        name: gN,
        example: s2,
        options: {
          direction : 'any',
          minDepth: 2,
          maxDepth: 2,
          vertexCollectionRestriction: v3,
          edgeCollectionRestriction: [e1, e2]
        }
      };
      var actual = getRawQueryResults(AQL_NEIGHBORS, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0].vertex._id, t1);
      assertEqual(actual[1].vertex._id, t2);
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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
