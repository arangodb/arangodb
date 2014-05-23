/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true */
/*global require, assertEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the general-graph class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Florian Bartels, Michael Hackstein
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("org/arangodb");
var db = arangodb.db;
var graph = require("org/arangodb/general-graph");
var ERRORS = arangodb.errors;

var _ = require("underscore");

// -----------------------------------------------------------------------------
// --SECTION--                                                      graph module
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: general-graph Creation and edge definition
////////////////////////////////////////////////////////////////////////////////

function GeneralGraphCreationSuite() {

  var rn = "UnitTestRelationName";
  var rn1 = "UnitTestRelationName1";
  var vn1 = "UnitTestVerticies1";
  var vn2 = "UnitTestVerticies2";
  var vn3 = "UnitTestVerticies3";
  var vn4 = "UnitTestVerticies4";
  var gn = "UnitTestGraph";
  var edgeDef = graph._edgeDefinitions(
    graph._undirectedRelationDefinition(rn, vn1),
    graph._directedRelationDefinition(rn1,
      [vn1, vn2], [vn3, vn4]
    )
  );

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Graph Creation
////////////////////////////////////////////////////////////////////////////////

    test_undirectedRelationDefinition : function () {
      var r = graph._undirectedRelationDefinition(rn, [vn1, vn2]);

      assertEqual(r, {
        collection: rn,
        from: [vn1, vn2],
        to: [vn1, vn2]
      });

    },

    test_undirectedRelationDefinitionWithSingleCollection : function () {
      var r = graph._undirectedRelationDefinition(rn, vn1);

      assertEqual(r, {
        collection: rn,
        from: [vn1],
        to: [vn1]
      });

    },

    test_undirectedRelationDefinitionWithMissingName : function () {
      try {
        graph._undirectedRelationDefinition("", [vn1, vn2]);
        fail();
      }
      catch (err) {
        assertEqual(err, "<relationName> must be a not empty string");
      }
    },

    test_undirectedRelationDefinitionWithTooFewArgs : function () {
      try {
        graph._undirectedRelationDefinition([vn1, vn2]);
        fail();
      }
      catch (err) {
        assertEqual(err, "method _undirectedRelationDefinition expects 2 arguments");
      }
    },

    test_undirectedRelationDefinitionWithInvalidSecondArg : function () {
      try {
        var param = {};
        param[vn1] = vn2;
        graph._undirectedRelationDefinition(rn, param);
        fail();
      }
      catch (err) {
        assertEqual(err, "<vertexCollections> must be a not empty string or array");
      }
    },

    test_directedRelationDefinition : function () {
      var r = graph._directedRelationDefinition(rn,
          [vn1, vn2], [vn3, vn4]);

      assertEqual(r, {
        collection: rn,
        from: [vn1, vn2],
        to: [vn3, vn4]
      });

    },

    test_directedRelationDefinitionWithMissingName : function () {
      try {
        graph._directedRelationDefinition("",
          [vn1, vn2], [vn3, vn4]);
        fail();
      }
      catch (err) {
        assertEqual(err, "<relationName> must be a not empty string");
      }
    },

    test_directedRelationDefinitionWithTooFewArgs : function () {
      try {
        graph._directedRelationDefinition([vn1, vn2], [vn3, vn4]);
        fail();
      }
      catch (err) {
        assertEqual(err, "method _directedRelationDefinition expects 3 arguments");
      }
    },

    test_directedRelationDefinitionWithInvalidSecondArg : function () {
      try {
        var param = {};
        param[vn1] = vn2;
        graph._directedRelationDefinition(rn, param, vn3);
        fail();
      }
      catch (err) {
        assertEqual(err, "<fromVertexCollections> must be a not empty string or array");
      }


    },

    test_directedRelationDefinitionWithInvalidThirdArg : function () {
      try {
        var param = {};
        param[vn1] = vn2;
        graph._directedRelationDefinition(rn, vn3, param);
        fail();
      }
      catch (err) {
        assertEqual(err, "<toVertexCollections> must be a not empty string or array");
      }
    },

    testEdgeDefinitions : function () {

      //with empty args
      assertEqual(graph._edgeDefinitions(), []);

      //with args
      assertEqual(graph._edgeDefinitions(
        graph._undirectedRelationDefinition(rn, vn1),
        graph._directedRelationDefinition(rn1,
          [vn1, vn2], [vn3, vn4])
      ), [
        {
          collection: rn,
          from: [vn1],
          to: [vn1]
        },
        {
          collection: rn1,
          from: [vn1, vn2],
          to: [vn3, vn4]
        }
      ]);
    },

    test_create : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      var a = graph._create(
        gn,
        graph._edgeDefinitions(
          graph._undirectedRelationDefinition(rn, vn1),
          graph._directedRelationDefinition(rn1, [vn1, vn2], [vn3, vn4])
        )
      );
      assertTrue(a.__vertexCollections.hasOwnProperty(vn1));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn2));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn3));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn4));
      assertTrue(a.__edgeCollections.hasOwnProperty(rn));
      assertTrue(a.__edgeCollections.hasOwnProperty(rn1));
      assertEqual(a.__edgeDefinitions, [
        {
          "collection" : rn,
          "from" : [
            vn1
          ],
          "to" : [
            vn1
          ]
        },
        {
          "collection" : rn1,
          "from" : [
            vn1,
            vn2
          ],
          "to" : [
            vn3,
            vn4
          ]
        }
      ]
      );
    },

    test_create_WithOut_EdgeDefiniton : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      try {
        graph._create(
          gn,
          []
        );
        fail();
      } catch (err) {
        assertEqual(err, "at least one edge definition is required to create a graph.");
      }
    },

    test_create_WithOut_Name : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      try {
        graph._create(
          "",
          edgeDef
        );
        fail();
      } catch (err) {
        assertEqual(err, "a graph name is required to create a graph.");
      }
    },

    test_create_With_Already_Existing_Graph : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      graph._create(gn, edgeDef);
      try {
        graph._create(gn, edgeDef);
      } catch (err) {
        assertEqual(err, "graph " + gn + " already exists.");
      }
    },

    test_get_graph : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      graph._create(gn, edgeDef);
      var a = graph._graph(gn);

      assertTrue(a.__vertexCollections.hasOwnProperty(vn1));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn2));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn3));
      assertTrue(a.__vertexCollections.hasOwnProperty(vn4));
      assertTrue(a.__edgeCollections.hasOwnProperty(rn));
      assertTrue(a.__edgeCollections.hasOwnProperty(rn1));
      assertEqual(a.__edgeDefinitions, [
        {
          "collection" : rn,
          "from" : [
            vn1
          ],
          "to" : [
            vn1
          ]
        },
        {
          "collection" : rn1,
          "from" : [
            vn1,
            vn2
          ],
          "to" : [
            vn3,
            vn4
          ]
        }
      ]
      );
    },

    test_get_graph_without_hit : function () {
      try {
        graph._graph(gn + "UnknownExtension");
        fail();
      } catch (e) {
        assertEqual(e, "graph " + gn + "UnknownExtension" + " does not exists.");
      }
    },

    test_creationOfGraphShouldNotAffectCollections: function() {
      if(graph._exists(gn)) {
        graph._drop(gn);
      }
      var edgeDef2 = [graph._directedRelationDefinition(rn, vn1, vn2)];
      var g = graph._create(gn, edgeDef2);
      var v1 = g[vn1].save({_key: "1"})._id;
      var v2 = g[vn2].save({_key: "2"})._id;
      var v3 = g[vn1].save({_key: "3"})._id;

      g[rn].save(v1, v2, {});
      assertEqual(g[vn1].count(), 2);
      assertEqual(g[vn2].count(), 1);
      assertEqual(g[rn].count(), 1);
      try {
        g[rn].save(v2, v3, {});
        fail();
      } catch (e) {
        // This should create an error
        assertEqual(g[rn].count(), 1);
      }

      try {
        db[rn].save(v2, v3, {});
      } catch (e) {
        // This should not create an error
        fail();
      }
      assertEqual(g[rn].count(), 2);

      db[vn2].remove(v2);
      // This should not remove edges
      assertEqual(g[rn].count(), 2);

      g[vn1].remove(v1);
      // This should remove edges
      assertEqual(g[rn].count(), 1);
      graph._drop(gn, true);
    }

  };

}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Simple Queries
// -----------------------------------------------------------------------------

function GeneralGraphAQLQueriesSuite() {

  // Definition of names
  var graphName = "UnitTestsGraph";
  var included = "UnitTestIncluded";
  var excluded = "UnitTestExcluded";
  var v1 = "UnitTestV1";
  var v2 = "UnitTestV2";
  var v3 = "UnitTestV3";

  var dropInclExcl = function() {
    if (graph._exists(graphName)) {
      graph._drop(graphName);
    }
  };

  var e1, e2, e3;

  var createInclExcl = function() {
    dropInclExcl();
    var inc = graph._directedRelationDefinition(
      included, [v1], [v1, v2]
    );
    var exc = graph._directedRelationDefinition(
      excluded, [v1], [v3]
    );
    var g = graph._create(graphName, [inc, exc]);
    g[v1].save({_key: "1"});
    g[v1].save({_key: "2"});
    g[v2].save({_key: "1"});
    g[v3].save({_key: "1"});
    e1 = g[included].save(
      v1 + "/1",
      v2 + "/1",
      {
        _key: "e1",
        val: true
      }
    )._id;
    e2 = g[included].save(
      v1 + "/2",
      v1 + "/1",
      {
        _key: "e2",
        val: false
      }
    )._id;
    e3 = g[excluded].save(
      v1 + "/1",
      v3 + "/1",
      {
        _key: "e3",
        val: false
      }
    )._id;
    return g;
  };

  var findIdInResult = function(result, id) {
    return _.some(result, function(i) {
      return i._id === id; 
    });
  };

  // The testee graph object
  var g;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief setUp: query creation for edges
////////////////////////////////////////////////////////////////////////////////

    setUp: function() {
      g = createInclExcl();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: query creation for edges
////////////////////////////////////////////////////////////////////////////////

    tearDown: function() {
      dropInclExcl();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: query creation for edges
////////////////////////////////////////////////////////////////////////////////

    test_edges: function() {
      var query = g._edges(v1 + "/1");
      assertEqual(query.printQuery(), 'FOR edges_0 IN GRAPH_EDGES('
        + '@graphName,@startVertex_0,"any")');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.startVertex_0, v1 + "/1");
      var result = query.toArray();
      assertEqual(result.length, 3);
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertTrue(findIdInResult(result, e2), "Did not include e2");
      assertTrue(findIdInResult(result, e3), "Did not include e3");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: query creation for outEdges
////////////////////////////////////////////////////////////////////////////////

    test_outEdges: function() {
      var query = g._outEdges(v1 + "/1");
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,@startVertex_0,"outbound")');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.startVertex_0, v1 + "/1");
      var result = query.toArray();
      assertEqual(result.length, 2);
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertTrue(findIdInResult(result, e3), "Did not include e3");
      assertFalse(findIdInResult(result, e2), "e2 is not excluded");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: query creation for inEdges
////////////////////////////////////////////////////////////////////////////////

    test_inEdges: function() {
      var query = g._inEdges(v1 + "/1");
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,@startVertex_0,"inbound")');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.startVertex_0, v1 + "/1");
      var result = query.toArray();
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result, e2), "Did not include e2");
      assertFalse(findIdInResult(result, e1), "e1 is not excluded");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
    },

    test_restrictOnEdges: function() {
      var query = g._edges(v1 + "/1").restrict(included);
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,@startVertex_0,"any",{},@restrictions_0)');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.startVertex_0, v1 + "/1");
      assertEqual(bindVars.restrictions_0, [included]);

      var result = query.toArray();
      assertEqual(result.length, 2);
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertTrue(findIdInResult(result, e2), "Did not include e2");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict construct on inEdges
////////////////////////////////////////////////////////////////////////////////

    test_restrictOnInEdges: function() {
      var query = g._inEdges(v1 + "/1").restrict(included);
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,@startVertex_0,"inbound",{},@restrictions_0)');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.startVertex_0, v1 + "/1");
      assertEqual(bindVars.restrictions_0, [included]);
      var result = query.toArray();
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result, e2), "Did not include e2");
      assertFalse(findIdInResult(result, e1), "e1 is not excluded");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict construct on outEdges
////////////////////////////////////////////////////////////////////////////////

    test_restrictOnOutEdges: function() {
      var query = g._outEdges(v1 + "/1").restrict(included);
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,@startVertex_0,"outbound",{},@restrictions_0)');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.startVertex_0, v1 + "/1");
      assertEqual(bindVars.restrictions_0, [included]);
      var result = query.toArray();
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertFalse(findIdInResult(result, e2), "e2 is not excluded");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
   },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict error handling
////////////////////////////////////////////////////////////////////////////////

    test_restrictErrorHandlingSingle: function() {
      try {
        g._outEdges(v1 + "/1").restrict([included, "unknown"]);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
        assertEqual(err.errorMessage, "edge collections: unknown are not known to the graph");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict error handling on multiple failures
////////////////////////////////////////////////////////////////////////////////

    test_restrictErrorHandlingMultiple: function() {
      try {
        g._outEdges(v1 + "/1").restrict(["failed", included, "unknown", "foxxle"]);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
        assertEqual(err.errorMessage,
          "edge collections: failed and unknown and foxxle are not known to the graph");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: filter construct on Edges
////////////////////////////////////////////////////////////////////////////////
   
   test_filterOnEdges: function() {
      var query = g._edges(v1 + "/1").filter({val: true});
      // var query = g._edges("v1/1").filter("e.val = true");
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,@startVertex_0,"any") '
        + 'FILTER MATCHES(edges_0,[{"val":true}])');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.startVertex_0, v1 + "/1");
      var result = query.toArray();
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertFalse(findIdInResult(result, e2), "e2 is not excluded");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
   },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: filter construct on InEdges
////////////////////////////////////////////////////////////////////////////////
   
   test_filterOnInEdges: function() {
      var query = g._inEdges(v1 + "/1").filter({val: true});
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,@startVertex_0,"inbound") '
        + 'FILTER MATCHES(edges_0,[{"val":true}])');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.startVertex_0, v1 + "/1");
      var result = query.toArray();
      assertEqual(result.length, 0);
      assertFalse(findIdInResult(result, e1), "e1 is not excluded");
      assertFalse(findIdInResult(result, e2), "e2 is not excluded");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: filter construct on OutEdges
////////////////////////////////////////////////////////////////////////////////
   
   test_filterOnOutEdges: function() {
      var query = g._outEdges(v1 + "/1").filter({val: true});
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,@startVertex_0,"outbound") '
        + 'FILTER MATCHES(edges_0,[{"val":true}])');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.startVertex_0, v1 + "/1");
      var result = query.toArray();
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertFalse(findIdInResult(result, e2), "e2 is not excluded");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: counting of query results
////////////////////////////////////////////////////////////////////////////////
    test_queryCount: function() {
      var query = g._edges(v1 + "/1");
      assertEqual(query.count(), 3);
      query = g._inEdges(v1 + "/1").filter({val: true});
      assertEqual(query.count(), 0);
      query = g._outEdges(v1 + "/1").filter({val: true});
      assertEqual(query.count(), 1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Cursor iteration
////////////////////////////////////////////////////////////////////////////////
    test_cursorIteration: function() {
      var query = g._edges(v1 + "/1");
      var list = [e1, e2, e3];
      var next;
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 2);
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 1);
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 0);
      assertFalse(query.hasNext());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Cursor recreation after iteration
////////////////////////////////////////////////////////////////////////////////
    test_cursorIterationAndRecreation: function() {
      var query = g._edges(v1 + "/1");
      var list = [e1, e2, e3];
      var next;
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 2);
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 1);
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 0);
      assertFalse(query.hasNext());
      query = query.filter({val: true});
      list = [e1];
      assertTrue(query.hasNext());
      next = query.next();
      list = _.without(list, next._id);
      assertEqual(list.length, 0);
      assertFalse(query.hasNext());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Is cursor recreated after counting of query results and appending filter
////////////////////////////////////////////////////////////////////////////////
    test_cursorRecreationAfterCount: function() {
      var query = g._edges(v1 + "/1");
      assertEqual(query.count(), 3);
      query = query.filter({val: true});
      assertEqual(query.count(), 1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Is cursor recreated after to array of query results and appending filter
////////////////////////////////////////////////////////////////////////////////
    test_cursorRecreationAfterToArray: function() {
      var query = g._edges(v1 + "/1");
      var result = query.toArray();
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertTrue(findIdInResult(result, e2), "Did not include e2");
      assertTrue(findIdInResult(result, e3), "Did not include e3");
      query = query.filter({val: true});
      result = query.toArray();
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertFalse(findIdInResult(result, e2), "e2 is not excluded");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
    }

  };

}

function ChainedFluentAQLResultsSuite() {

  var gn = "UnitTestGraph";
  var g;

  var edgeDef = [];
  

  var createTestData = function() {
    g = graph._create(gn, edgeDef);

  };

  var dropData = function() {
    graph._drop(gn);
  };

  return {

    setUp: function() {

    },

    tearDown: dropData

  };


}

function EdgesAndVerticesSuite() {

  var g;
  var vertexIds = [];
  var vertexId1, vertexId2;
  var edgeId1, edgeId2;
  var unitTestGraphName = "unitTestGraph";

  fillCollections = function() {
    var ids = {};
    var vertex = g.unitTestVertexCollection1.save({first_name: "Tam"});
    ids["vId11"] = vertex._id;
    vertex = g.unitTestVertexCollection1.save({first_name: "Tem"});
    ids["vId12"] = vertex._id;
    vertex = g.unitTestVertexCollection1.save({first_name: "Tim"});
    ids["vId13"] = vertex._id;
    vertex = g.unitTestVertexCollection1.save({first_name: "Tom"});
    ids["vId14"] = vertex._id;
    vertex = g.unitTestVertexCollection1.save({first_name: "Tum"});
    ids["vId15"] = vertex._id;
    vertex = g.unitTestVertexCollection3.save({first_name: "Tam"});
    ids["vId31"] = vertex._id;
    vertex = g.unitTestVertexCollection3.save({first_name: "Tem"});
    ids["vId32"] = vertex._id;
    vertex = g.unitTestVertexCollection3.save({first_name: "Tim"});
    ids["vId33"] = vertex._id;
    vertex = g.unitTestVertexCollection3.save({first_name: "Tom"});
    ids["vId34"] = vertex._id;
    vertex = g.unitTestVertexCollection3.save({first_name: "Tum"});
    ids["vId35"] = vertex._id;

    var edge = g.unitTestEdgeCollection1.save(ids.vId11, ids.vId12, {});
    ids["eId11"] = edge._id;
    edge = g.unitTestEdgeCollection1.save(ids.vId11, ids.vId13, {});
    ids["eId12"] = edge._id;
    edge = g.unitTestEdgeCollection1.save(ids.vId11, ids.vId14, {});
    ids["eId13"] = edge._id;
    edge = g.unitTestEdgeCollection1.save(ids.vId11, ids.vId15, {});
    ids["eId14"] = edge._id;
    edge = g.unitTestEdgeCollection1.save(ids.vId12, ids.vId11, {});
    ids["eId15"] = edge._id;
    edge = g.unitTestEdgeCollection1.save(ids.vId13, ids.vId11, {});
    ids["eId16"] = edge._id;
    edge = g.unitTestEdgeCollection1.save(ids.vId14, ids.vId11, {});
    ids["eId17"] = edge._id;
    edge = g.unitTestEdgeCollection1.save(ids.vId15, ids.vId11, {});
    ids["eId18"] = edge._id;
    edge = g.unitTestEdgeCollection2.save(ids.vId11, ids.vId31, {});
    ids["eId21"] = edge._id;
    edge = g.unitTestEdgeCollection2.save(ids.vId11, ids.vId32, {});
    ids["eId22"] = edge._id;
    edge = g.unitTestEdgeCollection2.save(ids.vId11, ids.vId33, {});
    ids["eId23"] = edge._id;
    edge = g.unitTestEdgeCollection2.save(ids.vId11, ids.vId34, {});
    ids["eId24"] = edge._id;
    edge = g.unitTestEdgeCollection2.save(ids.vId11, ids.vId35, {});
    ids["eId25"] = edge._id;
    return ids;
  }

  return {

    setUp : function() {
      try {
        arangodb.db._collection("_graphs").remove("_graphs/" + unitTestGraphName)
      } catch (err) {
      }
      g = graph._create(
        unitTestGraphName,
        graph._edgeDefinitions(
          graph._undirectedRelationDefinition("unitTestEdgeCollection1", "unitTestVertexCollection1"),
          graph._directedRelationDefinition("unitTestEdgeCollection2",
            ["unitTestVertexCollection1", "unitTestVertexCollection2"], ["unitTestVertexCollection3", "unitTestVertexCollection4"]
          )
        )
      );
    },

    tearDown : function() {
      graph._drop(unitTestGraphName);
    },

    test_dropGraph : function () {
      var myGraphName = unitTestGraphName + "2";
      var myEdgeColName = "unitTestEdgeCollection1";
      var myVertexColName = "unitTestVertexCollection4711";
      graph._create(
        myGraphName,
        graph._edgeDefinitions(
          graph._undirectedRelationDefinition(myEdgeColName, myVertexColName)
        )
      );
      graph._drop(myGraphName);
      assertFalse(graph._exists(myGraphName));
      assertTrue(db._collection(myVertexColName) === null);
      assertTrue(db._collection(myEdgeColName) !== null);
    },

    test_edgeCollections : function () {

      var edgeCollections = g._edgeCollections();
      assertEqual(edgeCollections[0].name(), 'unitTestEdgeCollection1');
      assertEqual(edgeCollections[1].name(), 'unitTestEdgeCollection2');
    },

    test_vertexCollections : function () {

      var vertexCollections = g._vertexCollections();
      assertEqual(vertexCollections[0].name(), 'unitTestVertexCollection1');
      assertEqual(vertexCollections[1].name(), 'unitTestVertexCollection2');
      assertEqual(vertexCollections[2].name(), 'unitTestVertexCollection3');
      assertEqual(vertexCollections[3].name(), 'unitTestVertexCollection4');
    },

    test_vC_save : function () {
      var vertex = g.unitTestVertexCollection1.save({first_name: "Tom"});
      assertFalse(vertex.error);
      vertexId1 = vertex._id;
      var vertexObj = g.unitTestVertexCollection1.document(vertexId1);
      assertEqual(vertexObj.first_name, "Tom");
    },

    test_vC_replace : function () {
      var vertex = g.unitTestVertexCollection1.save({first_name: "Tom"});
      var vertexId = vertex._id;
      vertex = g.unitTestVertexCollection1.replace(vertexId, {first_name: "Tim"});
      assertFalse(vertex.error);
      var vertexObj = g.unitTestVertexCollection1.document(vertexId);
      assertEqual(vertexObj.first_name, "Tim");
    },

    test_vC_update : function () {
      var vertex = g.unitTestVertexCollection1.save({first_name: "Tim"});
      var vertexId = vertex._id;
      vertex = g.unitTestVertexCollection1.update(vertexId, {age: 42});
      assertFalse(vertex.error);
      var vertexObj = g.unitTestVertexCollection1.document(vertexId);
      assertEqual(vertexObj.first_name, "Tim");
      assertEqual(vertexObj.age, 42);
    },

    test_vC_remove : function () {
      var vertex = g.unitTestVertexCollection1.save({first_name: "Tim"});
      var vertexId = vertex._id;
      var result = g.unitTestVertexCollection1.remove(vertexId);
      assertTrue(result);
    },

    test_vC_removeWithEdge : function () {
      var vertex1 = g.unitTestVertexCollection1.save({first_name: "Tim"});
      var vertexId1 = vertex1._id;
      var vertex2 = g.unitTestVertexCollection1.save({first_name: "Tom"});
      var vertexId2 = vertex2._id;
      var edge = g.unitTestEdgeCollection1.save(vertexId1, vertexId2, {});
      var edgeId = edge._id;
      var result = g.unitTestVertexCollection1.remove(vertexId1);
      assertTrue(result);
      assertFalse(db.unitTestEdgeCollection1.exists(edgeId));
      result = g.unitTestVertexCollection1.remove(vertexId2);
      assertTrue(result);
    },

    test_eC_save_undirected : function() {
      var vertex1 = g.unitTestVertexCollection1.save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g.unitTestVertexCollection1.save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var edge = g.unitTestEdgeCollection1.save(vertexId1, vertexId2, {});
      assertFalse(edge.error);
      edgeId1 = edge._id;
      g.unitTestVertexCollection1.remove(vertexId1);
      g.unitTestVertexCollection1.remove(vertexId2);
    },

    test_eC_save_directed : function() {
      var vertex1 = g.unitTestVertexCollection2.save({first_name: "Tom"});
      vertexId1 = vertex1._id;
      var vertex2 = g.unitTestVertexCollection4.save({first_name: "Tim"});
      vertexId2 = vertex2._id;
      var edge = g.unitTestEdgeCollection2.save(vertexId1, vertexId2, {});
      assertFalse(edge.error);
      edgeId2 = edge._id;
      g.unitTestVertexCollection2.remove(vertexId1);
      g.unitTestVertexCollection4.remove(vertexId2);
    },

    test_eC_save_withError : function() {
      var vertex1 = g.unitTestVertexCollection1.save({first_name: "Tom"});
      vertexId1 = vertex1._id;
      var vertex2 = g.unitTestVertexCollection2.save({first_name: "Tim"});
      vertexId2 = vertex2._id;
      try {
        var edge = g.unitTestEdgeCollection1.save(vertexId1, vertexId2, {});
      } catch (e) {
        assertEqual(e, "Edge is not allowed between " + vertexId1 + " and " + vertexId2 + ".")
      }
      g.unitTestVertexCollection1.remove(vertexId1);
      g.unitTestVertexCollection2.remove(vertexId2);
    },

    test_eC_replace : function() {
      var vertex1 = g.unitTestVertexCollection1.save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g.unitTestVertexCollection1.save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var edge = g.unitTestEdgeCollection1.save(vertexId1, vertexId2, {});
      var edgeId1 = edge._id;
      edge = g.unitTestEdgeCollection1.replace(edgeId1, {label: "knows"});
      assertFalse(edge.error);
      var edgeObj = g.unitTestEdgeCollection1.document(edgeId1);
      assertEqual(edgeObj.label, "knows");
      assertEqual(edgeObj._id, edgeId1);
    },

    test_eC_update : function () {
      var vertex1 = g.unitTestVertexCollection1.save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g.unitTestVertexCollection1.save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var edge = g.unitTestEdgeCollection1.save(vertexId1, vertexId2, {});
      var edgeId1 = edge._id;
      edge = g.unitTestEdgeCollection1.replace(edgeId1, {label: "knows"});
      edge = g.unitTestEdgeCollection1.update(edgeId1, {blub: "blub"});
      assertFalse(edge.error);
      var edgeObj = g.unitTestEdgeCollection1.document(edgeId1);
      assertEqual(edgeObj.label, "knows");
      assertEqual(edgeObj.blub, "blub");
      assertEqual(edgeObj._id, edgeId1);
    },

    test_eC_remove : function () {
      var vertex1 = g.unitTestVertexCollection1.save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g.unitTestVertexCollection1.save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var edge = g.unitTestEdgeCollection1.save(vertexId1, vertexId2, {});
      var edgeId1 = edge._id;
      edge = g.unitTestEdgeCollection1.remove(edgeId1);
      assertTrue(edge);
    },

    test_eC_removeWithEdgesAsVertices : function () {
      var myGraphName = unitTestGraphName + "0815";
      var g2 = graph._create(
        myGraphName,
        graph._edgeDefinitions(
          graph._directedRelationDefinition("unitTestEdgeCollection02",
            ["unitTestEdgeCollection1"], ["unitTestVertexCollection01"]
          )
        )
      );
      var vertex1 = g.unitTestVertexCollection1.save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g.unitTestVertexCollection1.save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var vertex3 = g2.unitTestVertexCollection01.save({first_name: "Ralph"});
      var vertexId3 = vertex3._id;
      var edge = g.unitTestEdgeCollection1.save(vertexId1, vertexId2, {});
      var edge2 = g2.unitTestEdgeCollection02.save(edge._id, vertexId3, {});

      var edgeId1 = edge._id;
      edge = g.unitTestEdgeCollection1.remove(edgeId1);
      assertTrue(edge);
      assertFalse(db._exists(edge2._id));
      graph._drop(myGraphName);
      assertFalse(graph._exists(myGraphName));
    },

    test_eC_removeWithEdgesAsVerticesCircle : function () {
      var gN1 = "unitTestGraphCircle1";
      var gN2 = "unitTestGraphCircle2";
      var gN3 = "unitTestGraphCircle3";
      var gN4 = "unitTestGraphCircle4";
      var eC1 = "unitTestEdgeCollectionCircle1";
      var eC2 = "unitTestEdgeCollectionCircle2";
      var eC3 = "unitTestEdgeCollectionCircle3";
      var eC4 = "unitTestEdgeCollectionCircle4";
      var vC1 = "unitTestVertexCollectionCircle1";
      var vC2 = "unitTestVertexCollectionCircle2";
      var vC3 = "unitTestVertexCollectionCircle3";
      var vC4 = "unitTestVertexCollectionCircle4";

      db._createEdgeCollection(eC1)
      db._createEdgeCollection(eC2)
      db._createEdgeCollection(eC3)
      db._createEdgeCollection(eC4)
      db._create(vC1)
      db._create(vC2)
      db._create(vC3)
      db._create(vC4)
      var vertex1 = db[vC1].save({});
      var vertexId1 = vertex1._id;
      var vertex2 = db[vC1].save({});
      var vertexId2 = vertex2._id;
      var vertex3 = db[vC1].save({});
      var vertexId3 = vertex3._id;
      var vertex4 = db[vC1].save({});
      var vertexId4 = vertex4._id;
      var edge1 = db[eC1].save(eC4 + "/4", vertexId1, {_key: "1"});
      var edge2 = db[eC2].save(eC1 + "/1", vertexId2, {_key: "2"});
      var edge3 = db[eC3].save(eC2 + "/2", vertexId3, {_key: "3"});
      var edge4 = db[eC4].save(eC3 + "/3", vertexId4, {_key: "4"});

      var g1 = graph._create(
        gN1,
        graph._edgeDefinitions(
          graph._directedRelationDefinition(eC1, [eC4], [vC1])
        )
      );
      var g2 = graph._create(
        gN2,
        graph._edgeDefinitions(
          graph._directedRelationDefinition(eC2, [eC1], [vC2])
        )
      );
      var g3 = graph._create(
        gN3,
        graph._edgeDefinitions(
          graph._directedRelationDefinition(eC3, [eC2], [vC3])
        )
      );
      var g4 = graph._create(
        gN4,
        graph._edgeDefinitions(
          graph._directedRelationDefinition(eC4, [eC3], [vC4])
        )
      );

      assertTrue(db._exists(edge1._id));
      assertTrue(db._exists(edge2._id));
      assertTrue(db._exists(edge3._id));
      assertTrue(db._exists(edge4._id));
      assertTrue(db._exists(vertexId1));
      assertTrue(db._exists(vertexId2));
      assertTrue(db._exists(vertexId3));
      assertTrue(db._exists(vertexId4));
      var edge = g1[eC1].remove(edge1._id);
      assertFalse(db._exists(edge1._id));
      assertFalse(db._exists(edge2._id));
      assertFalse(db._exists(edge3._id));
      assertFalse(db._exists(edge4._id));
      assertTrue(db._exists(vertexId1));
      assertTrue(db._exists(vertexId2));
      assertTrue(db._exists(vertexId3));
      assertTrue(db._exists(vertexId4));

      graph._drop(gN1);
      graph._drop(gN2);
      graph._drop(gN3);
      graph._drop(gN4);
    },

    test_edges : function() {
      var ids = fillCollections();
      var result = g._edges(ids.vId11).toArray();
      assertEqual(result.length, 13)
    },

    test_inEdges : function() {
      var ids = fillCollections();
      var result = g._inEdges(ids.vId11).toArray();
      assertEqual(result.length, 4)
    },

    test_outEdges : function() {
      var ids = fillCollections();
      var result = g._outEdges(ids.vId11).toArray();
      assertEqual(result.length, 9)
    },

    test_getInVertex : function() {
      var ids = fillCollections();
      var result = g._getInVertex(ids.eId11);
      assertEqual(result._id, ids.vId11);
    },

    test_getOutVertex : function() {
      var ids = fillCollections();
      var result = g._getOutVertex(ids.eId11);
      assertEqual(result._id, ids.vId12);
      result = g._getOutVertex(ids.eId25);
      assertEqual(result._id, ids.vId35);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(EdgesAndVerticesSuite);
/*
jsunity.run(GeneralGraphCreationSuite);
jsunity.run(GeneralGraphAQLQueriesSuite);
*/

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
