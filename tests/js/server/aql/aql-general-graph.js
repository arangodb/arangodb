/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXECUTE */

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
var errors = require("internal").errors;
var graph = require("@arangodb/general-graph");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var getRawQueryResults = helper.getRawQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for traversals using GRAPHS
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

  // var AQL_VERTICES = "FOR v IN GRAPH_VERTICES(@name, @example, @options) SORT v RETURN v";
  var exampleFilter = "FILTER x.hugo == true OR x.heinz == 1 RETURN x";
  var AQL_PICK_START_EXAMPLE = `FOR start IN UNION (
        (FOR x IN ${v1} ${exampleFilter}),
        (FOR x IN ${v2} ${exampleFilter}),
        (FOR x IN ${v3} ${exampleFilter}),
        (FOR x IN ${v4} ${exampleFilter})
      )`;
  var AQL_START_EVERYWHERE = `FOR start IN UNION (
        (FOR x IN ${v1} RETURN x),
        (FOR x IN ${v2} RETURN x),
        (FOR x IN ${v3} RETURN x),
        (FOR x IN ${v4} RETURN x)
      )`;

  // var startExample = [{hugo : true}, {heinz : 1}];

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
    /// @brief checks edges with a GRAPH
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    /// Section any direction
    ////////////////////////////////////////////////////////////////////////////////
    testEdgesAny: function () {
      var query = `FOR v, e IN ANY @start GRAPH @name SORT e.what RETURN e.what`;
      
      var bindVars = {
        name: gN,
        start: v1 + "/v1"
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual, [ "v1->v2", "v1->v5", "v2->v1" ]);
    },

    testEdgesAnyRestricted: function () {
      var query = `WITH ${v1}, ${v2}, ${v3}, ${v4}
                   FOR v, e IN ANY @start @@collection
                   SORT e.what
                   RETURN e.what`;
      var bindVars = {
        start: v1 + "/v1",
        "@collection": e1
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual, [ "v1->v2", "v2->v1" ]);
    },

    testEdgesAnyStartExample: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
        FOR v, e IN ANY start GRAPH @name COLLECT what = e.what RETURN what`;
      var bindVars = {
        name: gN
      };
      var actual = getRawQueryResults(query, bindVars);
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
      var query = `FOR v, e IN ANY @start GRAPH @name FILTER e.what == "v2->v1" SORT e.what RETURN e.what`;
      var bindVars = {
        name: gN,
        start: v1 + "/v1"
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "v2->v1");
    },

    testEdgesInbound: function() {
      var query = `FOR v, e IN INBOUND @start GRAPH @name SORT e.what RETURN e.what`;
      var bindVars = {
        name: gN,
        start: v1 + "/v1"
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual, [ "v2->v1"]);
    },

    testEdgesInboundStartExample: function() {
      var query = `${AQL_PICK_START_EXAMPLE}
        FOR v, e IN INBOUND start GRAPH @name SORT e.what RETURN e.what`;
      var bindVars = {
        name: gN
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 3);
      assertEqual(actual[0], "v1->v2");
      assertEqual(actual[1], "v2->v1");
      assertEqual(actual[2], "v3->v8");
    },

    testEdgesInboundStartExampleRestricted: function() {
      var query = `WITH ${v1}, ${v2}, ${v3}, ${v4}
                   ${AQL_PICK_START_EXAMPLE}
                   FOR v, e IN INBOUND start @@collection SORT e.what RETURN e.what`;
      var bindVars = {
        "@collection": e2
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "v3->v8");
    },

    testEdgesInboundStartExampleEdgeExample: function() {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v, e IN INBOUND start GRAPH @name FILTER e.what == 'v3->v8' SORT e.what RETURN e.what`;
      var bindVars = {
        name: gN
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "v3->v8");
    },

    testEdgesOutbound: function() {
      var query = `FOR v, e IN OUTBOUND @start GRAPH @name SORT e.what RETURN e.what`;
      var bindVars = {
        name: gN,
        start: v1 + "/v1"
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual[0], "v1->v2");
      assertEqual(actual[1], "v1->v5");
    },

    testEdgesOutboundStartExample: function() {
      var query = `${AQL_PICK_START_EXAMPLE}
        FOR v, e IN OUTBOUND start GRAPH @name SORT e.what RETURN e.what`;
      var bindVars = {
        name: gN
      };
      var actual = getRawQueryResults(query, bindVars);
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
      var query = `WITH ${v1}, ${v2}, ${v3}, ${v4}
                   ${AQL_PICK_START_EXAMPLE}
                   FOR v, e IN OUTBOUND start @@collection
                   SORT e.what
                   RETURN e.what`;
      var bindVars = {
        "@collection": e2
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 5);
      assertEqual(actual[0], "v1->v5");
      assertEqual(actual[1], "v2->v5");
      assertEqual(actual[2], "v3->v5");
      assertEqual(actual[3], "v3->v6");
      assertEqual(actual[4], "v3->v8");
    },

    testEdgesOutboundStartExampleRestrictedModify: function() {
      var query = `${AQL_PICK_START_EXAMPLE}
                    FOR v, e IN 1..2 OUTBOUND start @@collection SORT e.what
                    UPDATE e WITH {WasHere: True, When: DATE_NOW()} IN @@collection
                    RETURN e.what
                  `;
      var bindVars = {
        "@collection": e2
      };
      var actual = getRawQueryResults(query, bindVars);
      actual = getRawQueryResults("FOR x IN @@collection FILTER x.WasHere == True return x", bindVars);
      assertEqual(actual.length, 5);
    },

    testEdgesOutboundStartExampleRestrictedLoadVertextByDocument: function() {
      var query = `
        ${AQL_PICK_START_EXAMPLE}
        FOR v, edge IN 1..2 OUTBOUND start @@collection SORT edge.what
          LET fromVertex = DOCUMENT(edge._from)
          LET toVertex = DOCUMENT(edge._to)
            RETURN {edge, fromVertex, toVertex}`;
      var bindVars = {
        "@collection": e2
      };
      var actual = getRawQueryResults(query, bindVars);

      actual.forEach(function (oneEdgeSet) {
        assertEqual(oneEdgeSet.edge._from, oneEdgeSet.fromVertex._id );
        assertEqual(oneEdgeSet.edge._to,   oneEdgeSet.toVertex._id );
      });
    },


    ////////////////////////////////////////////////////////////////////////////////
    /// Test Neighbors
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    /// Any direction
    ////////////////////////////////////////////////////////////////////////////////
    
    testNeighborsAny: function () {
      var query = `FOR v IN ANY @start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true} SORT v._id RETURN v._id`;
      var bindVars = {
        name: gN,
        start: v1 + "/v1"
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v1 + "/v2");
      assertEqual(actual[1], v3 + "/v5");
    },

    testNeighborsAnyEdgeExample: function () {
      var query = `FOR v, e IN ANY @start GRAPH @name
                   FILTER e.what == "v2->v1"
                   COLLECT id = v._id
                   SORT id RETURN id`;
      var bindVars = {
        name: gN,
        start: v1 + "/v1",
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v2");
    },

    testNeighborsAnyStartExample: function () {
      var query = `${AQL_PICK_START_EXAMPLE} FOR v IN ANY start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   COLLECT id = v._id
                   SORT id RETURN id`;
      var bindVars = {
        name: gN,
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 6);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
      assertEqual(actual[2], v2 + "/v3");
      assertEqual(actual[3], v3 + "/v5");
      assertEqual(actual[4], v3 + "/v6");
      assertEqual(actual[5], v4 + "/v8");
    },

    testNeighborsAnyVertexExample: function () {
      var query = `${AQL_START_EVERYWHERE}
                   FOR v IN ANY start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER v._key == "v1"
                   COLLECT id = v._id
                   SORT id RETURN id`;
      var bindVars = {
        name: gN
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v1");
    },

    testNeighborsAnyStartExampleRestrictEdges: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                    FOR v IN ANY start ${e2} OPTIONS {uniqueVertices: "global", bfs: true}
                    COLLECT id = v._id
                    SORT id RETURN id`;
      var actual = getRawQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual[0], v2 + "/v3");
      assertEqual(actual[1], v3 + "/v5");
      assertEqual(actual[2], v3 + "/v6");
      assertEqual(actual[3], v4 + "/v8");
    },

    testNeighborsAnyStartExampleRestrictVertices: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v IN ANY start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER IS_SAME_COLLECTION(${v1}, v) OR IS_SAME_COLLECTION(${v3}, v)
                   COLLECT id = v._id SORT id RETURN id`;
      var bindVars = {
        name: gN
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 4);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
      assertEqual(actual[2], v3 + "/v5");
      assertEqual(actual[3], v3 + "/v6");
    },

    testNeighborsAnyStartExampleRestrictEdgesAndVertices: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v IN ANY start ${e2} OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER IS_SAME_COLLECTION(${v1}, v) OR IS_SAME_COLLECTION(${v3}, v)
                   COLLECT id = v._id SORT id RETURN id`;
      var actual = getRawQueryResults(query);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v3 + "/v5");
      assertEqual(actual[1], v3 + "/v6");
    },

 
    ////////////////////////////////////////////////////////////////////////////////
    /// direction outbound
    ////////////////////////////////////////////////////////////////////////////////
    
    testNeighborsOutbound: function () {
      var query = `FOR v IN OUTBOUND @start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   SORT v._id RETURN v._id`;
      var bindVars = {
        name: gN,
        start: v1 + "/v1"
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v1 + "/v2");
      assertEqual(actual[1], v3 + "/v5");
    },

    testNeighborsOutboundEdgeExample: function () {
      var query = `FOR v, e IN OUTBOUND @start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER e.what == "v2->v1" OR e.what == "v1->v2"
                   SORT v._id RETURN v._id`;
      var bindVars = {
        name: gN,
        start: v1 + "/v1",
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v2");
    },

    testNeighborsOutboundStartExample: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v IN OUTBOUND start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   COLLECT id = v._id SORT id RETURN id`;
      var bindVars = {
        name: gN,
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 5);

      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
      assertEqual(actual[2], v3 + "/v5");
      assertEqual(actual[3], v3 + "/v6");
      assertEqual(actual[4], v4 + "/v8");
    },

    testNeighborsOutboundVertexExample: function () {
      var query = `${AQL_START_EVERYWHERE}
                   FOR v IN OUTBOUND start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER v._key == "v1"
                   SORT v._id RETURN v._id`;
      var bindVars = {
        name: gN
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v1");
    },

    testNeighborsOutboundStartExampleRestrictEdges: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v IN OUTBOUND start ${e2} OPTIONS {uniqueVertices: "global", bfs: true}
                   COLLECT id = v._id
                   SORT id
                   RETURN id`;
      var actual = getRawQueryResults(query);
      assertEqual(actual.length, 3);
      assertEqual(actual[0], v3 + "/v5");
      assertEqual(actual[1], v3 + "/v6");
      assertEqual(actual[2], v4 + "/v8");
    },

    testNeighborsOutboundStartExampleRestrictVertices: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v IN OUTBOUND start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER IS_SAME_COLLECTION(${v1}, v) OR IS_SAME_COLLECTION(${v3}, v)
                   COLLECT id = v._id
                   SORT id RETURN id`;
      var bindVars = {
        name: gN
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 4);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
      assertEqual(actual[2], v3 + "/v5");
      assertEqual(actual[3], v3 + "/v6");
    },

    testNeighborsOutboundStartExampleRestrictEdgesAndVertices: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v IN OUTBOUND start ${e2} OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER IS_SAME_COLLECTION(${v1}, v) OR IS_SAME_COLLECTION(${v3}, v)
                   COLLECT id = v._id
                   SORT id RETURN id`;
      var actual = getRawQueryResults(query);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v3 + "/v5");
      assertEqual(actual[1], v3 + "/v6");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// inbound direction
    ////////////////////////////////////////////////////////////////////////////////
    
    testNeighborsInbound: function () {
      var query = `FOR v IN INBOUND @start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   SORT v._id RETURN v._id`;
      var bindVars = {
        name: gN,
        start: v1 + "/v1"
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v1 + "/v2");
    },

    testNeighborsInboundEdgeExample: function () {
      var query = `FOR v, e IN INBOUND @start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER e.what == "v2->v1"
                   SORT v._id RETURN v._id`;
      var bindVars = {
        name: gN,
        start: v1 + "/v1"
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual[0], v1 + "/v2");
    },

    testNeighborsInboundStartExample: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v IN INBOUND start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   SORT v._id RETURN v._id`;
      var bindVars = {
        name: gN,
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 3);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
      assertEqual(actual[2], v2 + "/v3");
    },

    testNeighborsInboundNeighborExample: function () {
      var query = `${AQL_START_EVERYWHERE}
                   FOR v IN INBOUND start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER v._key == "v1"
                   SORT v._id RETURN v._id`;
      var bindVars = {
        name: gN
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v1");
    },

    testNeighborsInboundStartExampleRestrictEdges: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v IN INBOUND start ${e2} OPTIONS {uniqueVertices: "global", bfs: true}
                   SORT v._id
                   RETURN v._id`;
      var actual = getRawQueryResults(query);
      assertEqual(actual.length, 1);
      assertEqual(actual[0], v2 + "/v3");
    },

    testNeighborsInboundStartExampleRestrictVertices: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v IN INBOUND start GRAPH @name OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER IS_SAME_COLLECTION(${v1}, v) OR IS_SAME_COLLECTION(${v3}, v)
                   SORT v._id RETURN v._id`;
      var bindVars = {
        name: gN
      };
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0], v1 + "/v1");
      assertEqual(actual[1], v1 + "/v2");
    },

    testNeighborsInboundStartExampleRestrictEdgesAndVertices: function () {
      var query = `${AQL_PICK_START_EXAMPLE}
                   FOR v IN INBOUND start ${e2} OPTIONS {uniqueVertices: "global", bfs: true}
                   FILTER IS_SAME_COLLECTION(${v1}, v) OR IS_SAME_COLLECTION(${v3}, v)
                   SORT v._id RETURN v._id`;
      var actual = getRawQueryResults(query);
      assertEqual(actual.length, 0);
    },

    testNeighborsInboundStartExampleRestrictEdgesAndVerticesMergeIntoEdges: function () {
      var bindVars = {
        name: gN,
        '@eCol': e1
      };
      var query = `
            ${AQL_PICK_START_EXAMPLE}
            FOR v IN INBOUND start GRAPH @name OPTIONS {bfs: true, uniqueVertices: "global"}
              FILTER IS_SAME_COLLECTION(${v1}, v) OR IS_SAME_COLLECTION(${v3}, v)
              SORT v._id
              FOR e in @@eCol
                FILTER e._from == v._id 
                RETURN MERGE(e, {v_key: v._key, v_hugo: v.hugo, v_id: v._id})`;
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 2);
    },

    testNeighborsInboundStartExampleRestrictEdgesAndVerticesSubLoops: function () {
      var bindVars = {
        name: gN,
        '@eCol': e1
      };
      var query = `
        FOR edgeDoc in @@eCol
          LET thisVertex = DOCUMENT(edgeDoc._to)
          LET vertices = (
            ${AQL_PICK_START_EXAMPLE}
            FOR v IN INBOUND start GRAPH @name OPTIONS {bfs: true, uniqueVertices: "global"}
              FILTER IS_SAME_COLLECTION(${v1}, v) OR IS_SAME_COLLECTION(${v3}, v)
              RETURN v)
          FOR oneVertex IN vertices RETURN {hugo: thisVertex.hugo, neighborVertex: oneVertex}`;

      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 4);
    }

  };
}


function ahuacatlQueryGeneralCommonTestSuite() {

  var vertexIds = {};

  var AQL_START_EVERYWHERE = `UNION((FOR x IN UnitTestsAhuacatlVertex1 RETURN x), (FOR x IN UnitTestsAhuacatlVertex2 RETURN x))`;
  var startWithFilter = function (filter) {
    return `UNION((FOR x IN UnitTestsAhuacatlVertex1 ${filter} RETURN x), (FOR x IN UnitTestsAhuacatlVertex2 ${filter} RETURN x))`;
  };

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
    /// @brief checks common neighbors
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighbors: function () {
      var query = `
        LET n1 = (FOR n IN ANY 'UnitTestsAhuacatlVertex1/v3' GRAPH 'bla3' OPTIONS {bfs: true, uniqueVertices: "global"} RETURN n._id)
        LET n2 = (FOR n IN ANY 'UnitTestsAhuacatlVertex2/v6' GRAPH 'bla3' OPTIONS {bfs: true, uniqueVertices: "global"} RETURN n._id)
        LET common = INTERSECTION(n1, n2)
        RETURN {left: 'UnitTestsAhuacatlVertex1/v3', right: 'UnitTestsAhuacatlVertex2/v6', neighbors: common}
      `;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 1);
      assertEqual(actual[0].left, vertexIds.v3);
      assertEqual(actual[0].right, vertexIds.v6);
      assertEqual(actual[0].neighbors.sort(), [vertexIds.v2, vertexIds.v7].sort());
    },
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks common neighbors
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsIn: function () {
      var query = `
        FOR left IN ${AQL_START_EVERYWHERE}
          LET n1 = (FOR n IN INBOUND left GRAPH 'bla3' OPTIONS {bfs: true, uniqueVertices: "global"} RETURN n._id)
          FOR right IN ${AQL_START_EVERYWHERE}
            FILTER left != right
            LET n2 = (FOR n IN INBOUND right GRAPH 'bla3' OPTIONS {bfs: true, uniqueVertices: "global"} RETURN n._id)
            LET neighbors = INTERSECTION(n1, n2)
            FILTER LENGTH(neighbors) > 0
            SORT left._id, right._id
            RETURN {left: left._id, right: right._id, neighbors: neighbors}
      `;

      var actual = getQueryResults(query);
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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks common neighbors
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsOut: function () {
      var query =`
        FOR left IN ${startWithFilter("FILTER x.hugo == true")}
          LET n1 = (FOR n IN 1..3 OUTBOUND left GRAPH 'bla3' RETURN DISTINCT n._id)
          FOR right IN ${startWithFilter("FILTER x.heinz == 1")}
            FILTER left != right
            LET n2 = (FOR n IN 1..3 OUTBOUND right GRAPH 'bla3' RETURN DISTINCT n._id)
            LET neighbors = INTERSECTION(n1, n2)
            FILTER LENGTH(neighbors) > 0
            SORT left._id, right._id
            RETURN {left: left._id, right: right._id, neighbors: neighbors}`;

      var actual = getQueryResults(query);
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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks common neighbors
    ////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsMixedOptionsDistinctFilters: function () {
      var query = `
      FOR left IN ${AQL_START_EVERYWHERE}
      LET n1 = (FOR v IN OUTBOUND left GRAPH 'bla3' OPTIONS {bfs: true, uniqueVertices: "global"}
        FILTER IS_SAME_COLLECTION(UnitTestsAhuacatlVertex1, v) RETURN v._id)
        FOR right IN ${AQL_START_EVERYWHERE}
          FILTER left != right
          LET n2 = (FOR v IN 1..2 INBOUND right GRAPH 'bla3' OPTIONS {bfs: true, uniqueVertices: "global"}
            FILTER IS_SAME_COLLECTION(UnitTestsAhuacatlVertex2, v) RETURN v._id)
          LET neighbors = INTERSECTION(n1, n2)
          FILTER LENGTH(neighbors) > 0
          SORT left._id, right._id
          RETURN {left: left._id, right: right._id, neighbors: neighbors}
      `;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 0, "Expect one result for each pair of vertices sharing neighbors");
    },

    testCommonNeighborsMixedOptionsFilterBasedOnOneCollectionOnly: function () {
      var query = `
        FOR left IN ${AQL_START_EVERYWHERE}
        LET n1 = (FOR v IN OUTBOUND left GRAPH 'bla3' OPTIONS {bfs: true, uniqueVertices: "global"}
          FILTER IS_SAME_COLLECTION(UnitTestsAhuacatlVertex2, v) RETURN v._id)
          FOR right IN ${AQL_START_EVERYWHERE}
            FILTER left != right
            LET n2 = (FOR v IN 1..2 INBOUND right GRAPH 'bla3' OPTIONS {bfs: true, uniqueVertices: "global"}
              FILTER IS_SAME_COLLECTION(UnitTestsAhuacatlVertex2, v) RETURN v._id)
            LET neighbors = INTERSECTION(n1, n2)
            FILTER LENGTH(neighbors) > 0
            SORT left._id, right._id
            RETURN {left: left._id, right: right._id, neighbors: neighbors}
        `;
      var actual = getQueryResults(query);

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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks common properties
    ////////////////////////////////////////////////////////////////////////////////

    testCommonProperties: function () {
      var query = `FOR left IN ${AQL_START_EVERYWHERE}
                     SORT left._id
                     RETURN ZIP([left._id], [(FOR right IN ${AQL_START_EVERYWHERE}
                       FILTER left != right
                       LET shared = (FOR a IN ATTRIBUTES(left) 
                                       FILTER a != "_rev" AND (a == "_id"
                                       OR left[a] == right[a])
                                       RETURN a)
                       FILTER LENGTH(shared) > 1
                       RETURN KEEP(right, shared))]
                     )`;
      var actual = getQueryResults(query);
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
      var query = `FOR left IN ${AQL_START_EVERYWHERE}
                     FILTER left.ageing == true
                     SORT left._id
                     RETURN ZIP([left._id], [(FOR right IN ${AQL_START_EVERYWHERE}
                       FILTER left != right
                       FILTER right.harald == "meier"
                       LET shared = (FOR a IN ATTRIBUTES(left) 
                                       FILTER a != "_rev" AND (a == "_id"
                                       OR left[a] == right[a])
                                       RETURN a)
                       FILTER LENGTH(shared) > 1
                       RETURN KEEP(right, shared))]
                     )`;
      var actual = getQueryResults(query);
      assertEqual(actual[0]["UnitTestsAhuacatlVertex2/v5"].length, 1);
      assertEqual(actual[1]["UnitTestsAhuacatlVertex2/v6"].length, 3);

    },

    testCommonPropertiesWithFiltersAndIgnoringKeyHarald: function () {
      var query = `FOR left IN ${AQL_START_EVERYWHERE}
                     SORT left._id
                     LET tmp = (FOR right IN ${AQL_START_EVERYWHERE}
                       FILTER left != right
                       LET shared = (FOR a IN ATTRIBUTES(left) 
                                       FILTER a != "_rev" AND a != "harald"
                                       AND (a == "_id" OR left[a] == right[a])
                                       RETURN a)
                       FILTER LENGTH(shared) > 1
                       RETURN KEEP(right, shared))
                     FILTER LENGTH(tmp) > 0
                     RETURN ZIP([left._id], [tmp])`;
      var actual = getQueryResults(query);

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
/// @brief test suite for SHORTEST PATH function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryGeneralTraversalTestSuite() {
  const v1 = "UnitTests_Berliner";
  const v2 = "UnitTests_Hamburger";
  const v3 = "UnitTests_Frankfurter";
  const v4 = "UnitTests_Leipziger";
  var vertexIds = {};
  const graphName = "werKenntWen";
  const AQL_START_EVERYWHERE = `UNION(
    (FOR x IN ${v1} RETURN x),
    (FOR x IN ${v2} RETURN x),
    (FOR x IN ${v3} RETURN x),
    (FOR x IN ${v4} RETURN x)
  )`;

  var startWithFilter = function (filter) {
    return `UNION(
      (FOR x IN ${v1} ${filter} RETURN x),
      (FOR x IN ${v2} ${filter} RETURN x),
      (FOR x IN ${v3} ${filter} RETURN x),
      (FOR x IN ${v4} ${filter} RETURN x)
    )`;
  };


  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop(v1);
      db._drop(v2);
      db._drop(v3);
      db._drop(v4);
      db._drop("UnitTests_KenntAnderenBerliner");
      db._drop("UnitTests_KenntAnderen");

      var KenntAnderenBerliner = "UnitTests_KenntAnderenBerliner";
      var KenntAnderen = "UnitTests_KenntAnderen";

      var Berlin = db._create(v1);
      var Hamburg = db._create(v2);
      var Frankfurt = db._create(v3);
      var Leipzig = db._create(v4);
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
        db._collection("_graphs").remove(graphName);
      } catch (ignore) {
      }
      var g = graph._create(
        graphName,
        graph._edgeDefinitions(
          graph._relation(KenntAnderenBerliner, v1, v1),
          graph._relation(KenntAnderen,
            [v1, v2, v3, v4],
            [v1, v2, v3, v4]
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

    tearDownAll: function () {
      graph._drop("werKenntWen", true);
    },

    testShortestPathWithGraphName: function () {
      var query = `LET p = (FOR v, e IN OUTBOUND SHORTEST_PATH "${vertexIds.Caesar}" TO "${vertexIds.Emil}" GRAPH "${graphName}" RETURN {v: v._id, e: e._id})
                         LET edges = (FOR e IN p[*].e FILTER e != null RETURN e)
                         LET vertices = p[*].v
                         LET distance = LENGTH(edges)
                         RETURN {edges, vertices, distance}`;

      var actual;

      // Caesar -> Berta -> Gerda -> Dieter -> Emil
      actual = getQueryResults(query);
      assertEqual(actual.length, 1, "Exactly one element is returned");
      var path = actual[0];
      assertTrue(path.hasOwnProperty("vertices"), "The path contains all vertices");
      assertTrue(path.hasOwnProperty("edges"), "The path contains all edges");
      assertTrue(path.hasOwnProperty("distance"), "The path contains the distance");
      assertEqual(path.vertices, [
        vertexIds.Caesar, vertexIds.Berta, vertexIds.Gerda, vertexIds.Dieter, vertexIds.Emil
      ], "The correct shortest path is using these vertices");
      assertEqual(path.distance, 4, "The distance is 1 per edge");

      query = `
        FOR source in ${AQL_START_EVERYWHERE}
          FOR target in ${AQL_START_EVERYWHERE}
            FILTER source != target
            LET vertices = (FOR v, e IN INBOUND SHORTEST_PATH source TO target GRAPH "${graphName}" RETURN v._id)
            FILTER LENGTH(vertices) > 0
            LET distance = LENGTH(vertices) - 1 
            SORT source, target
            RETURN {vertices, distance}`;
      actual = getQueryResults(query);
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

      query = `FOR source IN ${AQL_START_EVERYWHERE}
                FILTER source._id != "${vertexIds.Berta}"
                LET vertices = (FOR v, e IN INBOUND SHORTEST_PATH source TO "${vertexIds.Berta}" GRAPH "${graphName}" RETURN v._id)
                FILTER LENGTH(vertices) > 0
                LET distance = LENGTH(vertices) - 1 
                SORT source 
                RETURN {vertices, distance}`;
      actual = getQueryResults(query);
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

      query = `FOR v IN OUTBOUND SHORTEST_PATH "${vertexIds.Caesar}" TO "${vertexIds.Anton}" GRAPH "${graphName}" OPTIONS {weightAttribute: "entfernung", defaultWeight: 80} RETURN v._id`;
      actual = getQueryResults(query);
      assertEqual(actual.length, 3);

      query = `FOR v IN OUTBOUND SHORTEST_PATH "${vertexIds.Caesar}" TO "${vertexIds.Anton}" GRAPH "${graphName}" RETURN v._id`;
      actual = getQueryResults(query);
      assertEqual(actual.length, 2);

      /*
       COLLECT AGGREGATE does NOT work with non numeric values yet
      query = `FOR v, e IN OUTBOUND SHORTEST_PATH "${vertexIds.Caesar}" TO "${vertexIds.Emil}" GRAPH "${graphName}" OPTIONS {weightAttribute: "entfernung", defaultWeight: 80} COLLECT AGGREGATE distance = SUM(e.entfernung) RETURN {vertex: "${vertexIds.Emil}", startVertex: "${vertexIds.Caesar}", distance: distance}`;
      actual = getQueryResults(query);
      assertEqual(actual,
        [
          {
            vertex: vertexIds.Emil,
            startVertex: vertexIds.Caesar,
            distance: 830.1
          }
        ]
      );
      */
    },

    testFindShortestShortestPath: function () {
      var query = `
        FOR target IN ${startWithFilter("FILTER x.gender == 'female'")}
          LET p = (FOR v IN INBOUND SHORTEST_PATH "${vertexIds.Fritz}" TO target GRAPH "${graphName}" RETURN v._id)
          LET l = LENGTH(p)
          FILTER l > 0
          SORT l
          LIMIT 1
          RETURN {
            distance: l - 1,
            vertices: p
          }
      `;
      var actual = getQueryResults(query);
      // Find only one match, ignore the second one.
      assertEqual(actual.length, 1);
      // Find the right match
      var path = actual[0];
      assertEqual(path.distance, 3);
      assertEqual(path.vertices, [
        vertexIds.Fritz, vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda
      ]);

      // Alternative (identical, in some cases this is more performant, in others the first one is better)
      query = `
        FOR v, e, p IN 1..1000 INBOUND "${vertexIds.Fritz}" GRAPH "${graphName}" OPTIONS {bfs: true}
          FILTER v.gender == "female"
          LIMIT 1
          RETURN {
            distance: LENGTH(p.edges),
            vertices: p.vertices[*]._id
          }`;

      actual = getQueryResults(query);
      // Find only one match, ignore the second one.
      assertEqual(actual.length, 1);
      // Find the right match
      path = actual[0];
      assertEqual(path.distance, 3);
      assertEqual(path.vertices, [
        vertexIds.Fritz, vertexIds.Emil, vertexIds.Dieter, vertexIds.Gerda
      ]);
    },

    testShortestPathWithExamples: function () {
      var query = `
        FOR start IN ${startWithFilter("FILTER x.gender == 'female'")}
          FOR target IN ${startWithFilter("FILTER x.gender == 'male' AND x.age == 30")}
            FILTER start != target
            SORT start._id, target._id
            LET p = (FOR v, e IN ANY SHORTEST_PATH start TO target GRAPH "${graphName}" RETURN {v: v._id, e: e._id})
            LET edges = (FOR e IN p[*].e FILTER e != null RETURN e)
            LET vertices = p[*].v
            LET distance = LENGTH(edges)
            FILTER distance > 0
            RETURN {edges, vertices, distance}`;
      var actual;
      actual = getQueryResults(query);
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
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for and SHORTEST PATH function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryGeneralCyclesSuite() {

  const v1 = "UnitTests_Berliner";
  const v2 = "UnitTests_Hamburger";
  const v3 = "UnitTests_Frankfurter";
  const v4 = "UnitTests_Leipziger";
  var vertexIds = {};

  const graphName = "werKenntWen";
  const AQL_START_EVERYWHERE = `UNION(
    (FOR x IN ${v1} RETURN x),
    (FOR x IN ${v2} RETURN x),
    (FOR x IN ${v3} RETURN x),
    (FOR x IN ${v4} RETURN x)
  )`;

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop(v1);
      db._drop(v2);
      db._drop(v3);
      db._drop(v4);
      db._drop("UnitTests_KenntAnderenBerliner");
      db._drop("UnitTests_KenntAnderen");

      var KenntAnderenBerliner = "UnitTests_KenntAnderenBerliner";
      var KenntAnderen = "UnitTests_KenntAnderen";

      var Berlin = db._create(v1);
      var Hamburg = db._create(v2);
      var Frankfurt = db._create(v3);
      var Leipzig = db._create(v4);
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
        db._collection("_graphs").remove(graphName);
      } catch (ignore) {
      }
      var g = graph._create(
        graphName,
        graph._edgeDefinitions(
          graph._relation(KenntAnderenBerliner, v1, v1),
          graph._relation(KenntAnderen,
            [v1, v2, v3, v4],
            [v1, v2, v3, v4]
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
    /// @brief checks shortest path with graph name
    ////////////////////////////////////////////////////////////////////////////////

    testCycleShortestPathWithGraphName: function () {
      var actual;
      var query = `
        FOR source in ${AQL_START_EVERYWHERE}
          FOR target in ${AQL_START_EVERYWHERE}
            FILTER source != target
            LET vertices = (FOR v, e IN INBOUND SHORTEST_PATH source TO target GRAPH "${graphName}" RETURN v._id)
            FILTER LENGTH(vertices) > 0
            LET distance = LENGTH(vertices) - 1 
            SORT source, target
            RETURN {vertices, distance}`;
      actual = getQueryResults(query);
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
    },

    tearDownAll: function () {
      graph._drop(gN, true);
    },

    testRestrictedPathHops1: function() {
      var bindVars = {
        start: s1
      };
      var query = `WITH ${v1}, ${v2}, ${v3}
                   FOR v IN 2 ANY @start ${e1}, ${e2}
                   FILTER IS_SAME_COLLECTION(${v3}, v)
                   SORT v._id
                   RETURN v`;
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0]._id, t1);
    },

    testRestrictedPathHops2: function() {
      var bindVars = {
        start: s2
      };
      var query = `WITH ${v1}, ${v2}, ${v3}
                   FOR v IN 2 ANY @start ${e1}, ${e2}
                   FILTER IS_SAME_COLLECTION(${v3}, v)
                   SORT v._id
                   RETURN v`;
      var actual = getRawQueryResults(query, bindVars);
      assertEqual(actual.length, 2);
      assertEqual(actual[0]._id, t1);
      assertEqual(actual[1]._id, t2);
    }
  };
}

function ahuacatlQueryShortestPathTestSuite() {
  
  const v1 = "UnitTestsAhuacatlVertex1";
  const v2 = "UnitTestsAhuacatlVertex2";
  const v3 = "UnitTestsAhuacatlVertex3";
  const e1 = "UnitTestsAhuacatlEdge1";
  const e2 = "UnitTestsAhuacatlEdge2";
  const e3 = "UnitTestsAhuacatlEdge3";
  const e4 = "UnitTestsAhuacatlEdge4";
  
  const graphName = "abc";
  
  return {
    setUpAll: function () {
      db._drop(v1);
      db._drop(v2);
      db._drop(v3);
      db._drop(e1);
      db._drop(e2);
      db._drop(e3);
      db._drop(e4);
      
      var v1Col = db._create(v1);
      var v2Col = db._create(v2);
      var v3Col = db._create(v3);
      db._createEdgeCollection(e1);
      db._createEdgeCollection(e2);
      db._createEdgeCollection(e3);
      db._createEdgeCollection(e4);
      
      var A = v1Col.save({ _key: "A"});
      var B = v2Col.save({ _key: "B"});
      var C = v3Col.save({ _key: "C"});
      var D = v2Col.save({ _key: "D"});
      var E = v3Col.save({ _key: "E"});
      var F = v1Col.save({ _key: "F"});
      
      try {
        db._collection("_graphs").remove(graphName);
      } catch (ignore) {
      }
      var g = graph._create(
        graphName,
        graph._edgeDefinitions(
          graph._relation(e1, [v1, v2, v3], [v1, v2, v3]),
          graph._relation(e2, [v1, v2, v3], [v1, v2, v3]),
          graph._relation(e3, [v1, v2, v3], [v1, v2, v3]),
          graph._relation(e4, [v1, v2, v3], [v1, v2, v3])
        )
      );
      
      function makeEdge(from, to, collection, distance) {
        collection.save(from, to, { what: from.split("/")[1] + "->" + to.split("/")[1], entfernung: distance});
      }
      
      makeEdge(A._id, B._id, g[e1], 4);
      makeEdge(A._id, C._id, g[e2], 2);
      makeEdge(B._id, C._id, g[e1], 5);
      makeEdge(B._id, D._id, g[e3], 10);
      makeEdge(C._id, E._id, g[e1], 3);
      makeEdge(D._id, F._id, g[e4], 11);
      makeEdge(E._id, D._id, g[e1], 4);
     
      makeEdge(F._id, C._id, g[e1], 13);
      makeEdge(F._id, E._id, g[e1], 6);
      makeEdge(E._id, B._id, g[e1], 6);
      makeEdge(C._id, A._id, g[e3], 2);
      makeEdge(B._id, A._id, g[e2], 2);
    },
    
    tearDownAll: function () {
      db._drop(v1);
      db._drop(v2);
      db._drop(v3);
      db._drop(e1);
      db._drop(e2);
      db._drop(e3);
      db._drop(e4);
      db._collection("_graphs").remove(graphName);
    },
    
    testShortestPathAtoFoutbound: function () {
      var query = `
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN OUTBOUND SHORTEST_PATH source TO target GRAPH "${graphName}" RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "B");
      assertEqual(actual[1].e.entfernung, 4);
      assertEqual(actual[2].v._key, "D");
      assertEqual(actual[2].e.entfernung, 10);
      assertEqual(actual[3].v._key, "F");
      assertEqual(actual[3].e.entfernung, 11);
    },
    
    testShortestPathAtoFoutboundWeight: function () {
      var query = `
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN OUTBOUND SHORTEST_PATH source TO target GRAPH "${graphName}" OPTIONS {weightAttribute: "entfernung", defaultWeight: 100}  RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 5);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "C");
      assertEqual(actual[1].e.entfernung, 2);
      assertEqual(actual[2].v._key, "E");
      assertEqual(actual[2].e.entfernung, 3);
      assertEqual(actual[3].v._key, "D");
      assertEqual(actual[3].e.entfernung, 4);
      assertEqual(actual[4].v._key, "F");
      assertEqual(actual[4].e.entfernung, 11);
    },
    
    testShortestPathAtoFoutboundEdgeCollectionRestriction: function () {
      var query = `
        WITH ${v1}, ${v2}, ${v3}
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN OUTBOUND SHORTEST_PATH source TO target ${e1},${e4} RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 6);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "B");
      assertEqual(actual[1].e.entfernung, 4);
      assertEqual(actual[2].v._key, "C");
      assertEqual(actual[2].e.entfernung, 5);
      assertEqual(actual[3].v._key, "E");
      assertEqual(actual[3].e.entfernung, 3);
      assertEqual(actual[4].v._key, "D");
      assertEqual(actual[4].e.entfernung, 4);
      assertEqual(actual[5].v._key, "F");
      assertEqual(actual[5].e.entfernung, 11);
    },
    
    testShortestPathAtoFoutboundWeightEdgeCollectionRestriction: function () {
      var query = `
        WITH ${v1}, ${v2}, ${v3}
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN OUTBOUND SHORTEST_PATH source TO target ${e1},${e3},${e4} OPTIONS {weightAttribute: "entfernung", defaultWeight: 100}  RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "B");
      assertEqual(actual[1].e.entfernung, 4);
      assertEqual(actual[2].v._key, "D");
      assertEqual(actual[2].e.entfernung, 10);
      assertEqual(actual[3].v._key, "F");
      assertEqual(actual[3].e.entfernung, 11);
    },
    
    testShortestPathAtoFinbound: function () {
      var query = `
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN INBOUND SHORTEST_PATH source TO target GRAPH "${graphName}" RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 3);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "C");
      assertEqual(actual[1].e.entfernung, 2);
      assertEqual(actual[2].v._key, "F");
      assertEqual(actual[2].e.entfernung, 13);
    },
    
    testShortestPathAtoFinboundEdgeCollectionRestriction: function () {
      var query = `
        WITH ${v1}, ${v2}, ${v3}
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN INBOUND SHORTEST_PATH source TO target ${e1},${e2} RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "B");
      assertEqual(actual[1].e.entfernung, 2);
      assertEqual(actual[2].v._key, "E");
      assertEqual(actual[2].e.entfernung, 6);
      assertEqual(actual[3].v._key, "F");
      assertEqual(actual[3].e.entfernung, 6);
    },
    
    testShortestPathAtoFinboundWeight: function () {
      var query = `
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN INBOUND SHORTEST_PATH source TO target GRAPH "${graphName}" OPTIONS {weightAttribute: "entfernung", defaultWeight: 100}  RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "B");
      assertEqual(actual[1].e.entfernung, 2);
      assertEqual(actual[2].v._key, "E");
      assertEqual(actual[2].e.entfernung, 6);
      assertEqual(actual[3].v._key, "F");
      assertEqual(actual[3].e.entfernung, 6);
    },
    
    testShortestPathAtoFinboundWeightEdgeCollectionRestriction: function () {
      var query = `
        WITH ${v1}, ${v2}, ${v3}
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN INBOUND SHORTEST_PATH source TO target ${e1},${e3} OPTIONS {weightAttribute: "entfernung", defaultWeight: 100}  RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 3);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "C");
      assertEqual(actual[1].e.entfernung, 2);
      assertEqual(actual[2].v._key, "F");
      assertEqual(actual[2].e.entfernung, 13);
    },
    
    testShortestPathAtoFany: function () {
      var query = `
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN ANY SHORTEST_PATH source TO target GRAPH "${graphName}" RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 3);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "C");
      assertEqual(actual[1].e.entfernung, 2);
      assertEqual(actual[2].v._key, "F");
      assertEqual(actual[2].e.entfernung, 13);
    },
    
    testShortestPathAtoFanyEdgeCollectionRestriction: function () {
      var query = `
        WITH ${v1}, ${v2}, ${v3}
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN ANY SHORTEST_PATH source TO target ${e1} RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "B");
      assertEqual(actual[1].e.entfernung, 4);
      assertTrue(actual[2].v._key === "E" || actual[2].v._key === "C");
      assertEqual(actual[3].v._key, "F");
      if(actual[2].v._key === "E") {
        assertEqual(actual[2].e.entfernung, 6);
        assertEqual(actual[3].e.entfernung, 6);
      } else if(actual[2].v._key === "C") {
        assertEqual(actual[2].e.entfernung, 5);
        assertEqual(actual[3].e.entfernung, 13);
      }
    },
    
    testShortestPathAtoFanyWeight: function () {
      var query = `
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN ANY SHORTEST_PATH source TO target GRAPH "${graphName}" OPTIONS {weightAttribute: "entfernung", defaultWeight: 100}  RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "C");
      assertEqual(actual[1].e.entfernung, 2);
      assertEqual(actual[2].v._key, "E");
      assertEqual(actual[2].e.entfernung, 3);
      assertEqual(actual[3].v._key, "F");
      assertEqual(actual[3].e.entfernung, 6);
    },
    
    testShortestPathAtoFanyWeightEdgeCollectionRestriction: function () {
      var query = `
        WITH ${v1}, ${v2}, ${v3}
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN ANY SHORTEST_PATH source TO target ${e1} OPTIONS {weightAttribute: "entfernung", defaultWeight: 100}  RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "B");
      assertEqual(actual[1].e.entfernung, 4);
      assertEqual(actual[2].v._key, "E");
      assertEqual(actual[2].e.entfernung, 6);
      assertEqual(actual[3].v._key, "F");
      assertEqual(actual[3].e.entfernung, 6);
    },
    
    testShortestPathAtoFdifferentDirections: function () {
      var query = `
        WITH ${v1}, ${v2}, ${v3}
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN OUTBOUND SHORTEST_PATH source TO target ${e1}, INBOUND ${e2},${e4} OPTIONS {weightAttribute: "entfernung", defaultWeight: 100}  RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 6);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "B");
      assertEqual(actual[1].e.entfernung, 2);
      assertEqual(actual[2].v._key, "C");
      assertEqual(actual[2].e.entfernung, 5);
      assertEqual(actual[3].v._key, "E");
      assertEqual(actual[3].e.entfernung, 3);
      assertEqual(actual[4].v._key, "D");
      assertEqual(actual[4].e.entfernung, 4);
      assertEqual(actual[5].v._key, "F");
      assertEqual(actual[5].e.entfernung, 11);
    },
    
    testShortestPathAtoFnoPath: function () {
      var query = `
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN OUTBOUND SHORTEST_PATH source TO target ${e1} RETURN {v, e}`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 0);
    },
    
   testShortestPathAtoFwithDeletedVertex: function () {
     db._collection(v2).remove({_key: "D"});
      var query = `
        LET source = "${v1}/A"
        LET target = "${v1}/F"
        FOR v, e IN OUTBOUND SHORTEST_PATH source TO target GRAPH "${graphName}" RETURN {v, e}`;
      var full = AQL_EXECUTE(query);
      var actual = full.json;
      assertEqual(actual.length, 4);
      assertEqual(actual[0].v._key, "A");
      assertEqual(actual[0].e, null);
      assertEqual(actual[1].v._key, "B");
      assertEqual(actual[1].e.entfernung, 4);
      assertEqual(actual[2].v, null);
      assertEqual(actual[2].e.entfernung, 10);
      assertEqual(actual[2].e._to, v2 + "/D");
      assertEqual(actual[3].v._key, "F");
      assertEqual(actual[3].e.entfernung, 11);
      // We expect one warning
      assertEqual(full.warnings.length, 1);
      assertEqual(full.warnings[0].code, errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
    }
    
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryGeneralCommonTestSuite);
jsunity.run(ahuacatlQueryGeneralCyclesSuite);
jsunity.run(ahuacatlQueryGeneralTraversalTestSuite);
jsunity.run(ahuacatlQueryGeneralEdgesTestSuite);
jsunity.run(ahuacatlQueryMultiCollectionMadnessTestSuite);
jsunity.run(ahuacatlQueryShortestPathTestSuite);

return jsunity.done();

