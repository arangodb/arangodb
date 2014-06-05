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

    testExtendEdgeDefinitions : function () {


      //with empty args
      assertEqual(graph._edgeDefinitions(), []);

      //with args
      var ed =graph._edgeDefinitions(
        graph._undirectedRelationDefinition("relationName", "vertexC1"),
        graph._directedRelationDefinition("relationName",
          ["vertexC1", "vertexC2"], ["vertexC3", "vertexC4"])
      );
      graph._extendEdgeDefinitions(ed,
        graph._undirectedRelationDefinition("relationName", "vertexC1")
      );
      assertEqual(ed,  [
        {
          collection: "relationName",
          from: ["vertexC1"],
          to: ["vertexC1"]
        },
        {
          collection: "relationName",
          from: ["vertexC1", "vertexC2"],
          to: ["vertexC3", "vertexC4"]
        },
        {
          collection: "relationName",
          from: ["vertexC1"],
          to: ["vertexC1"]
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
        assertEqual(err.errorMessage, "at least one edge definition is required to create a graph.");
      }
    },

    test_create_WithOut_Name : function () {
      if (db._collection("_graphs").exists(gn)) {
        db._collection("_graphs").remove(gn);
      }
      try {
        graph._create(
          "",
          graph._edgeDefinitions(
            graph._undirectedRelationDefinition("relationName", "vertexC1"),
            graph._directedRelationDefinition("relationName2",
              ["vertexC1", "vertexC2"], ["vertexC3", "vertexC4"]
            )
          )
        );
        fail();
      } catch (err) {
        assertEqual(err.errorMessage, "a graph name is required to create a graph.");
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
        assertEqual(e, "graph " + gn + "UnknownExtension" + " does not exist.");
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
/// @brief setUp: query creation for edges and vertices
////////////////////////////////////////////////////////////////////////////////

    setUp: function() {
      g = createInclExcl();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: query creation for edges and vertices
////////////////////////////////////////////////////////////////////////////////

    tearDown: function() {
      dropInclExcl();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict construct on edges
////////////////////////////////////////////////////////////////////////////////

    test_restrictOnEdges: function() {
      var query = g._edges().restrict(included);
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,{},@options_0)');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars
        .options_0
        .edgeCollectionRestriction, [included]);
      assertEqual(bindVars
        .options_0
        .direction, "outbound"
      );

      var result = query.toArray();
      assertEqual(result.length, 2);
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertTrue(findIdInResult(result, e2), "Did not include e2");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: query creation for Vertices
////////////////////////////////////////////////////////////////////////////////

    test_vertices: function() {
      var query = g._vertices(v1 + "/1");
      assertEqual(query.printQuery(), 'FOR vertices_0 IN GRAPH_VERTICES('
        + '@graphName,@vertexExample_0,@options_0)');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.vertexExample_0, {_id: v1 + "/1"});
      var result = query.toArray();
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result,  v1 + "/1"), "Did not include " +  v1 + "/1");

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict error handling
////////////////////////////////////////////////////////////////////////////////

    test_restrictErrorHandlingSingle: function() {
      try {
        g._edges(v1 + "/1").restrict([included, "unknown"]);
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
        g._edges(v1 + "/1").restrict(["failed", included, "unknown", "foxxle"]);
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
      var query = g._edges().filter({val: true});
      assertEqual(query.printQuery(), "FOR edges_0 IN GRAPH_EDGES("
        + '@graphName,{},@options_0) '
        + 'FILTER MATCHES(edges_0,[{"val":true}])');
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, graphName);
      assertEqual(bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{}]
      });
      var result = query.toArray();
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result, e1), "Did not include e1");
      assertFalse(findIdInResult(result, e2), "e2 is not excluded");
      assertFalse(findIdInResult(result, e3), "e3 is not excluded");
   },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: counting of query results
////////////////////////////////////////////////////////////////////////////////
//
    test_queryCount: function() {
      var query = g._edges();
      assertEqual(query.count(), 3);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Cursor iteration
////////////////////////////////////////////////////////////////////////////////
    test_cursorIteration: function() {
      var query = g._edges();
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
      var query = g._edges();
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
      var query = g._edges();
      assertEqual(query.count(), 3);
      query = query.filter({val: true});
      assertEqual(query.count(), 1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Is cursor recreated after to array of query results and appending filter
////////////////////////////////////////////////////////////////////////////////
    test_cursorRecreationAfterToArray: function() {
      var query = g._edges();
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
  var user = "UnitTestUsers";
  var product = "UnitTestProducts";
  var isFriend = "UnitTestIsFriend";
  var hasBought = "UnitTestHasBought";
  var uaName = "Alice";
  var ubName = "Bob";
  var ucName = "Charly";
  var udName = "Diana";
  var p1Name = "HiFi";
  var p2Name = "Shirt";
  var p3Name = "TV";
  var pTypeElec = "Electro";
  var pTypeCloth = "Cloth";

    var ud1 = 2000;
    var ud2 = 2001;
    var ud3 = 2002;
    var ud4 = 2003;

    var d1 = 2004;
    var d2 = 2005;
    var d3 = 2006;
    var d4 = 2007;
    var d5 = 2008;

  var g;

  var edgeDef = [];
  edgeDef.push(graph._undirectedRelationDefinition(isFriend, user));
  edgeDef.push(graph._directedRelationDefinition(hasBought, user, product));


  var findBoughts = function(result, list) {
    var boughts = _.sortBy(
      _.filter(result, function(e) {
        return e._id.split("/")[0] === hasBought;
      }),
      "date"
    );
    assertEqual(list.length, boughts.length, "Did not return all expected boughts");
    _.each(list.sort(), function(v, i) {
      assertEqual(boughts[i].date, v);
    });
  };

  var findFriends = function(result, list) {
    var friends = _.sortBy(
      _.filter(result, function(e) {
        return e._id.split("/")[0] === isFriend;
      }),
      "since"
    );
    assertEqual(list.length, friends.length, "Did not return all expected friendships");
    _.each(list.sort(), function(v, i) {
      assertEqual(friends[i].since, v);
    });
  };

  var dropData = function() {
    try {
      graph._drop(gn);
    } catch(ignore) {

    }
  };

  var createTestData = function() {
    dropData();
    g = graph._create(gn, edgeDef);
    var ua = g[user].save({name: uaName})._id;
    var ub = g[user].save({name: ubName})._id;
    var uc = g[user].save({name: ucName})._id;
    var ud = g[user].save({name: udName})._id;

    var p1 = g[product].save({name: p1Name, type: pTypeElec})._id;
    var p2 = g[product].save({name: p2Name, type: pTypeCloth})._id;
    var p3 = g[product].save({name: p3Name, type: pTypeElec})._id;

    g[isFriend].save(ua, ub, {
      since: ud1
    });
    g[isFriend].save(ua, uc, {
      since: ud2
    });
    g[isFriend].save(ub, ud, {
      since: ud3
    });
    g[isFriend].save(uc, ud, {
      since: ud4
    });

    g[hasBought].save(ua, p1, {
      date: d1
    });
    g[hasBought].save(ub, p1, {
      date: d2
    });
    g[hasBought].save(ub, p3, {
      date: d3
    });

    g[hasBought].save(ud, p1, {
      date: d4
    });
    g[hasBought].save(ud, p2, {
      date: d5
    });

  };

  var plainVertexQueryStmt = function(depth) {
    return "FOR vertices_" + depth + " IN "
      + "GRAPH_VERTICES("
      + "@graphName,"
      + "@vertexExample_" + depth + ","
      + "@options_" + depth + ")";
  };

  var plainNeighborQueryStmt = function(depth, vdepth) {
    return "FOR neighbors_" + depth + " IN "
      + "GRAPH_NEIGHBORS("
      + "@graphName,"
      + "vertices_" + vdepth + ","
      + "@options_" + depth + ")";
  };

  var vertexFilterStmt = function(direction, eDepth, vDepth) {
    switch(direction) {
      case "both":
        return "FILTER edges_"
          + eDepth
          + "._from == vertices_"
          + vDepth
          + "._id || edges_"
          + eDepth
          + "._to == vertices_"
          + vDepth
          + "._id";
      case "from":
        return "FILTER edges_"
          + eDepth
          + "._from == vertices_"
          + vDepth
          + "._id";
      case "to":
        return "FILTER edges_"
          + eDepth
          + "._to == vertices_"
          + vDepth
          + "._id";
      default:
        fail("Helper function does not know direction:" + direction);
    }

  };

  var plainEdgesQueryStmt = function(depth, vDepth, type) {
    if (!type) {
      type = "vertices";
    }
    var q = "FOR edges_" + depth + " IN "
      + "GRAPH_EDGES("
      + "@graphName,";
    if(vDepth > -1) {
      q += type + "_" + vDepth;
      if (type === "neighbors") {
        q += ".vertex";
      }
      q += ",";
    } else {
      q += "{},";
    }
    q += "@options_" + depth + ")";
    return q;
  };

  return {

    setUp: createTestData,

    tearDown: dropData,

    test_getAllVerticiesResultingAQL: function() {
      var query = g._vertices();
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, {});
      assertEqual(query.bindVars.options_0, {});
    },

    test_getAllVerticies: function() {
      var result = g._vertices().toArray();
      assertEqual(result.length, 7);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, uaName);
      assertEqual(sorted[1].name, ubName);
      assertEqual(sorted[2].name, ucName);
      assertEqual(sorted[3].name, udName);
      assertEqual(sorted[4].name, p1Name);
      assertEqual(sorted[5].name, p2Name);
      assertEqual(sorted[6].name, p3Name);
    },

    test_getVertexByIdResultingAQL: function() {
      var a_id = g[user].firstExample({name: uaName})._id;
      var query = g._vertices(a_id);
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, {_id: a_id});
      assertEqual(query.bindVars.options_0, {});
    },

    test_getVertexById: function() {
      var a_id = g[user].firstExample({name: uaName})._id;
      var result = g._vertices(a_id).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0].name, uaName);
    },

    test_getVerticesByIdResultingAQL: function() {
      var a_id = g[user].firstExample({name: uaName})._id;
      var b_id = g[user].firstExample({name: ubName})._id;
      var query = g._vertices([a_id, b_id]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, [{_id: a_id}, {_id: b_id}]);
      assertEqual(query.bindVars.options_0, {});
    },

    test_getVerticiesById: function() {
      var a_id = g[user].firstExample({name: uaName})._id;
      var b_id = g[user].firstExample({name: ubName})._id;
      var result = g._vertices([a_id, b_id]).toArray();
      assertEqual(result.length, 2);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, uaName);
      assertEqual(sorted[1].name, ubName);
    },

    test_getVertexByExampleResultingAQL: function() {
      var query = g._vertices({
        name: uaName
      });
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, {name: uaName});
      assertEqual(query.bindVars.options_0, {});
    },

    test_getVertexByExample: function() {
      var result = g._vertices({
        name: uaName
      }).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0].name, uaName);
    },

    test_getVerticiesByExampleResultingAQL: function() {
      var query = g._vertices([{
        name: uaName
      },{
        name: p1Name
      }]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, [
        {name: uaName},
        {name: p1Name},
      ]);
      assertEqual(query.bindVars.options_0, {});
    },

    test_getVerticiesByExample: function() {
      var result = g._vertices([{
        name: uaName
      },{
        name: p1Name
      }]).toArray();
      assertEqual(result.length, 2);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, uaName);
      assertEqual(sorted[1].name, p1Name);
    },

    test_getVerticiesByExampleAndIdMixResultingAQL: function() {
      var b_id = g[user].firstExample({name: ubName})._id;
      var query = g._vertices([{
        name: uaName
      },
      b_id,
      {
        name: ucName
      }]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainVertexQueryStmt(0));
      assertEqual(query.bindVars.vertexExample_0, [
        {name: uaName},
        {_id: b_id},
        {name: ucName},
      ]);
      assertEqual(query.bindVars.options_0, {});
    },

    test_getVerticiesByExampleAndIdMix: function() {
      var b_id = g[user].firstExample({name: ubName})._id;
      var result = g._vertices([{
        name: uaName
      },
      b_id,
      {
        name: ucName
      }]).toArray();
      assertEqual(result.length, 3);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, uaName);
      assertEqual(sorted[1].name, ubName);
      assertEqual(sorted[2].name, ucName);
    },

    test_getAllEdgesResultingAQL: function() {
      var query = g._edges();
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{}]
      });
    },

    test_getAllEdges: function() {
      var result = g._edges().toArray();
      assertEqual(result.length, 9);
      findFriends(result, [ud1, ud2, ud3, ud4]);
      findBoughts(result, [d1, d2, d3, d4, d5]);
    },

    test_getEdgeByIdResultingAQL: function() {
      var a_id = g[hasBought].firstExample({date: d1})._id;
      var query = g._edges(a_id);
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{_id: a_id}]
      });
    },

    test_getEdgeById: function() {
      var a_id = g[hasBought].firstExample({date: d1})._id;
      var result = g._edges(a_id).toArray();
      assertEqual(result.length, 1);
      findBoughts(result, [d1]);
    },

    test_getEdgesByIdResultingAQL: function() {
      var a_id = g[hasBought].firstExample({date: d1})._id;
      var b_id = g[isFriend].firstExample({since: ud2})._id;
      var query = g._edges([a_id, b_id]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [
          {_id: a_id},
          {_id: b_id}
        ]
      });
    },

    test_getEdgesById: function() {
      var a_id = g[hasBought].firstExample({date: d1})._id;
      var b_id = g[isFriend].firstExample({since: ud2})._id;
      var result = g._edges([a_id, b_id]).toArray();
      assertEqual(result.length, 2);
      findBoughts(result, [d1]);
      findFriends(result, [ud2]);
    },

    test_getEdgeByExampleResultingAQL: function() {
      var query = g._edges({
        date: d2
      });
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [
          {date: d2}
        ]
      });
    },

    test_getEdgeByExample: function() {
      var result = g._edges({
        date: d2
      }).toArray();
      assertEqual(result.length, 1);
      findBoughts(result, [d2]);
    },

    test_getEdgesByExampleResultingAQL: function() {
      var query = g._edges([{
        since: ud3
      },{
        date: d3
      }]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [
          {since: ud3},
          {date: d3}
        ]
      });
    },

    test_getEdgesByExample: function() {
      var result = g._edges([{
        since: ud3
      },{
        date: d3
      }]).toArray();
      assertEqual(result.length, 2);
      findBoughts(result, [d3]);
      findFriends(result, [ud3]);
    },

    test_getEdgesByExampleAndIdMixResultingAQL: function() {
      var b_id = g[hasBought].firstExample({date: d1})._id;
      var query = g._edges([{
        date: d5
      },
      b_id,
      {
        since: ud1
      }]);
      var stmt = query.printQuery();
      assertEqual(stmt, plainEdgesQueryStmt(0));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [
          {date: d5},
          {_id: b_id},
          {since: ud1}
        ]
      });
    },

    test_getEdgesByExampleAndIdMix: function() {
      var b_id = g[hasBought].firstExample({date: d1})._id;
      var result = g._edges([{
        date: d5
      },
      b_id,
      {
        since: ud1
      }]).toArray();
      assertEqual(result.length, 3);
      findBoughts(result, [d1, d5]);
      findFriends(result, [ud1]);
    },

    test_getEdgesForSelectedVertexResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .edges();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainEdgesQueryStmt(1, 0));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.vertexExample_0, {name: uaName});
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        direction: "any",
        edgeExamples: [{}]
      });
    },

    test_getEdgesForSelectedVertex: function() {
      var result = g._vertices({name: uaName})
        .edges()
        .toArray();
      assertEqual(result.length, 3);
      findBoughts(result, [d1]);
      findFriends(result, [ud1, ud2]);
    },

    test_getInEdgesForSelectedVertexResultingAQL: function() {
      var query = g._vertices({name: ubName})
        .inEdges();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainEdgesQueryStmt(1, 0));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.vertexExample_0, {name: ubName});
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        direction: "inbound",
        edgeExamples: [{}]
      });
    },

    test_getInEdgesForSelectedVertex: function() {
      var result = g._vertices({name: ubName})
        .inEdges()
        .toArray();
      assertEqual(result.length, 1);
      findFriends(result, [ud1]);
    },

    test_getOutEdgesForSelectedVertexResultingAQL: function() {
      var query = g._vertices({name: ubName})
        .outEdges();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainEdgesQueryStmt(1, 0));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.vertexExample_0, {name: ubName});
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        direction: "outbound",
        edgeExamples: [{}]
      });
    },

    test_getOutEdgesForSelectedVertex: function() {
      var result = g._vertices({name: ubName})
        .outEdges()
        .toArray();
      assertEqual(result.length, 3);
      findBoughts(result, [d2, d3]);
      findFriends(result, [ud3]);
    },

    test_getVerticesForSelectedEdgeResultingAQL: function() {
      var query = g._edges({since: ud1})
        .vertices();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainEdgesQueryStmt(0));
      expected.push(plainVertexQueryStmt(1));
      expected.push(vertexFilterStmt("both", 0, 1));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{since: ud1}]
      });
      assertEqual(query.bindVars.options_1, {});
    },

    test_getVerticesForSelectedEdge: function() {
      var result = g._edges({since: ud1})
        .vertices()
        .toArray();
      assertEqual(result.length, 2);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, uaName);
      assertEqual(sorted[1].name, ubName);
    },

    test_getToVertexForSelectedEdgeResultingAQL: function() {
      var query = g._edges({since: ud1})
        .toVertices();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainEdgesQueryStmt(0));
      expected.push(plainVertexQueryStmt(1));
      expected.push(vertexFilterStmt("to", 0, 1));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{since: ud1}]
      });
      assertEqual(query.bindVars.options_1, {});
    },

    test_getToVertexForSelectedEdge: function() {
      var result = g._edges({since: ud1})
        .toVertices()
        .toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0].name, ubName);
    },

    test_getFromVertexForSelectedEdgeResultingAQL: function() {
      var query = g._edges({since: ud1})
        .fromVertices();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainEdgesQueryStmt(0));
      expected.push(plainVertexQueryStmt(1));
      expected.push(vertexFilterStmt("from", 0, 1));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {
        direction: "outbound",
        edgeExamples: [{since: ud1}]
      });
      assertEqual(query.bindVars.options_1, {});
    },

    test_getFromVertexForSelectedEdge: function() {
      var result = g._edges({since: ud1})
        .fromVertices()
        .toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0].name, uaName);
    },

    test_getAllVerticesThroughOutgoingEdgeResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .outEdges()
        .toVertices();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainEdgesQueryStmt(1, 0));
      expected.push(plainVertexQueryStmt(2));
      expected.push(vertexFilterStmt("to", 1, 2));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.vertexExample_0, {
        name: uaName
      });
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        direction: "outbound",
        edgeExamples: [{}]
      });
      assertEqual(query.bindVars.options_2, {});
    },

    test_getAllVerticesThroughOutgoingEdges: function() {
      var result = g._vertices({name: uaName})
        .outEdges()
        .toVertices()
        .toArray();
      assertEqual(result.length, 3);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, ubName);
      assertEqual(sorted[1].name, ucName);
      assertEqual(sorted[2].name, p1Name);
    },

    test_getAllVerticesThroughOutgoingEdgesWithFilterResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .outEdges([
        {since: ud1},
        {date: d1}
      ])
        .toVertices();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainEdgesQueryStmt(1, 0));
      expected.push(plainVertexQueryStmt(2));
      expected.push(vertexFilterStmt("to", 1, 2));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.vertexExample_0, {
        name: uaName
      });
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        direction: "outbound",
        edgeExamples: [
          {since: ud1},
          {date: d1}
        ]
      });
      assertEqual(query.bindVars.options_2, {});
    },

    test_getAllVerticesThroughOutgoingEdgesWithFilter: function() {
      var result = g._vertices({name: uaName})
      .outEdges([
        {since: ud1},
        {date: d1}
      ]).toVertices()
        .toArray();
      assertEqual(result.length, 2);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, ubName);
      assertEqual(sorted[1].name, p1Name);
    },

    test_getNeighborsOfSelectedVerticesResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .neighbors();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainNeighborQueryStmt(1, 0));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        neighborExamples: {}
      });
    },

    test_getNeighborsOfSelectedVertices: function() {
      var result = g._vertices({name: uaName})
        .neighbors()
        .toArray();
      assertEqual(result.length, 3);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, ubName);
      assertEqual(sorted[1].name, ucName);
      assertEqual(sorted[2].name, p1Name);
    },

    test_getExampleNeighborsOfSelectedVerticesResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .neighbors([{
          name: ubName
        },{
          name: p1Name
        }]);
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainNeighborQueryStmt(1, 0));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        neighborExamples: [{
          name: ubName
        },{
          name: p1Name
        }]
      });
    },

    test_getExampleNeighborsOfSelectedVertices: function() {
      var result = g._vertices({name: uaName})
        .neighbors([{
          name: ubName
        },{
          name: p1Name
        }])
        .toArray();
      assertEqual(result.length, 2);
      var sorted = _.sortBy(result, "name");
      assertEqual(sorted[0].name, ubName);
      assertEqual(sorted[1].name, p1Name);
    },

    test_getEdgesOfNeighborsResultingAQL: function() {
      var query = g._vertices({name: uaName})
        .neighbors()
        .outEdges();
      var stmt = query.printQuery();
      var expected = [];
      expected.push(plainVertexQueryStmt(0));
      expected.push(plainNeighborQueryStmt(1, 0));
      expected.push(plainEdgesQueryStmt(2, 1, "neighbors"));
      assertEqual(stmt, expected.join(" "));
      assertEqual(query.bindVars.options_0, {});
      assertEqual(query.bindVars.options_1, {
        neighborExamples: {}
      });
      assertEqual(query.bindVars.options_2, {
        direction: "outbound",
        edgeExamples: [{}]
      });
    },

    test_getEdgesOfNeighbors: function() {
      var result = g._vertices({name: uaName})
        .neighbors()
        .outEdges()
        .toArray();
      assertEqual(result.length, 4);
      findFriends(result, [ud3, ud4]);
      findBoughts(result, [d2, d3]);
    }

  };
}

function EdgesAndVerticesSuite() {

  var g;
  var vertexId1, vertexId2;
  var unitTestGraphName = "unitTestGraph";

  var ec1 = "unitTestEdgeCollection1";
  var ec2 = "unitTestEdgeCollection2";
  var vc1 = "unitTestVertexCollection1";
  var vc2 = "unitTestVertexCollection2";
  var vc3 = "unitTestVertexCollection3";
  var vc4 = "unitTestVertexCollection4";


  var fillCollections = function() {
    var ids = {};
    var vertex = g[vc1].save({first_name: "Tam"});
    ids.vId11 = vertex._id;
    vertex = g[vc1].save({first_name: "Tem"});
    ids.vId12 = vertex._id;
    vertex = g[vc1].save({first_name: "Tim"});
    ids.vId13 = vertex._id;
    vertex = g[vc1].save({first_name: "Tom"});
    ids.vId14 = vertex._id;
    vertex = g[vc1].save({first_name: "Tum"});
    ids.vId15 = vertex._id;
    vertex = g[vc3].save({first_name: "Tam"});
    ids.vId31 = vertex._id;
    vertex = g[vc3].save({first_name: "Tem"});
    ids.vId32 = vertex._id;
    vertex = g[vc3].save({first_name: "Tim"});
    ids.vId33 = vertex._id;
    vertex = g[vc3].save({first_name: "Tom"});
    ids.vId34 = vertex._id;
    vertex = g[vc3].save({first_name: "Tum"});
    ids.vId35 = vertex._id;

    var edge = g[ec1].save(ids.vId11, ids.vId12, {});
    ids.eId11 = edge._id;
    edge = g[ec1].save(ids.vId11, ids.vId13, {});
    ids.eId12 = edge._id;
    edge = g[ec1].save(ids.vId11, ids.vId14, {});
    ids.eId13 = edge._id;
    edge = g[ec1].save(ids.vId11, ids.vId15, {});
    ids.eId14 = edge._id;
    edge = g[ec1].save(ids.vId12, ids.vId11, {});
    ids.eId15 = edge._id;
    edge = g[ec1].save(ids.vId13, ids.vId11, {});
    ids.eId16 = edge._id;
    edge = g[ec1].save(ids.vId14, ids.vId11, {});
    ids.eId17 = edge._id;
    edge = g[ec1].save(ids.vId15, ids.vId11, {});
    ids.eId18 = edge._id;
    edge = g[ec2].save(ids.vId11, ids.vId31, {});
    ids.eId21 = edge._id;
    edge = g[ec2].save(ids.vId11, ids.vId32, {});
    ids.eId22 = edge._id;
    edge = g[ec2].save(ids.vId11, ids.vId33, {});
    ids.eId23 = edge._id;
    edge = g[ec2].save(ids.vId11, ids.vId34, {});
    ids.eId24 = edge._id;
    edge = g[ec2].save(ids.vId11, ids.vId35, {});
    ids.eId25 = edge._id;
    return ids;
  };

  return {

    setUp : function() {
      try {
        arangodb.db._collection("_graphs").remove(unitTestGraphName);
      } catch (ignore) {
      }
      g = graph._create(
        unitTestGraphName,
        graph._edgeDefinitions(
          graph._undirectedRelationDefinition(ec1, vc1),
          graph._directedRelationDefinition(ec2,
            [vc1, vc2], [vc3, vc4]
          )
        )
      );
    },

    tearDown : function() {
      graph._drop(unitTestGraphName);
    },

    test_dropGraph : function () {
      var myGraphName = unitTestGraphName + "2";
      var myEdgeColName = "unitTestEdgeCollection4711";
      var myVertexColName = vc1;
      graph._create(
        myGraphName,
        graph._edgeDefinitions(
          graph._undirectedRelationDefinition(myEdgeColName, myVertexColName)
        )
      );
      graph._drop(myGraphName);
      assertFalse(graph._exists(myGraphName));
      assertTrue(db._collection(myVertexColName) !== null);
      assertTrue(db._collection(myEdgeColName) === null);
    },

    test_createGraphWithCollectionDuplicateOK : function () {
      var myGraphName = unitTestGraphName + "2";
      graph._create(
        myGraphName,
        graph._edgeDefinitions(
          graph._undirectedRelationDefinition(ec1, vc1)
        )
      );
      assertTrue(graph._exists(myGraphName));
      graph._drop(myGraphName);
      assertFalse(graph._exists(myGraphName));
      assertTrue(db._collection(vc1) !== null);
      assertTrue(db._collection(ec1) !== null);
    },

    test_createGraphWithCollectionDuplicateNOK1 : function () {
      var myGraphName = unitTestGraphName + "2";
      try {
        graph._create(
          myGraphName,
          graph._edgeDefinitions(
            graph._undirectedRelationDefinition(ec1, vc2)
          )
        );
      } catch (e) {
        assertEqual(
          e.errorMessage,
          ec1 + arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message
        );
      }
      assertFalse(graph._exists(myGraphName));
      assertTrue(db._collection(vc2) !== null);
      assertTrue(db._collection(ec1) !== null);
    },

    test_createGraphWithCollectionDuplicateNOK2 : function () {
      var myGraphName = unitTestGraphName + "2";
      var myED = "unitTestEdgeCollection4711";
      var myVD1 = "unitTestVertexCollection4711";
      var myVD2 = "unitTestVertexCollection4712";
      try {
        graph._create(
          myGraphName,
          graph._edgeDefinitions(
            graph._undirectedRelationDefinition(myED, myVD1),
            graph._undirectedRelationDefinition(myED, myVD2)
          )
        );
      } catch (e) {
        assertEqual(
          e.errorMessage,
          arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.message
        );
      }
      assertFalse(graph._exists(myGraphName));
      assertTrue(db._collection(myVD1) === null);
      assertTrue(db._collection(myVD2) === null);
      assertTrue(db._collection(myED) === null);
    },

    test_edgeCollections : function () {

      var edgeCollections = g._edgeCollections();
      assertEqual(edgeCollections[0].name(), ec1);
      assertEqual(edgeCollections[1].name(), ec2);
    },

    test_vertexCollections : function () {

      var vertexCollections = g._vertexCollections();
      assertEqual(vertexCollections[0].name(), vc1);
      assertEqual(vertexCollections[1].name(), vc2);
      assertEqual(vertexCollections[2].name(), vc3);
      assertEqual(vertexCollections[3].name(), vc4);
    },

    test_vC_save : function () {
      var vertex = g[vc1].save({first_name: "Tom"});
      assertFalse(vertex.error);
      vertexId1 = vertex._id;
      var vertexObj = g[vc1].document(vertexId1);
      assertEqual(vertexObj.first_name, "Tom");
    },

    test_vC_replace : function () {
      var vertex = g[vc1].save({first_name: "Tom"});
      var vertexId = vertex._id;
      vertex = g[vc1].replace(vertexId, {first_name: "Tim"});
      assertFalse(vertex.error);
      var vertexObj = g[vc1].document(vertexId);
      assertEqual(vertexObj.first_name, "Tim");
    },

    test_vC_update : function () {
      var vertex = g[vc1].save({first_name: "Tim"});
      var vertexId = vertex._id;
      vertex = g[vc1].update(vertexId, {age: 42});
      assertFalse(vertex.error);
      var vertexObj = g[vc1].document(vertexId);
      assertEqual(vertexObj.first_name, "Tim");
      assertEqual(vertexObj.age, 42);
    },

    test_vC_remove : function () {
      var vertex = g[vc1].save({first_name: "Tim"});
      var vertexId = vertex._id;
      var result = g[vc1].remove(vertexId);
      assertTrue(result);
    },

    test_vC_removeWithEdge : function () {
      var vertex1 = g[vc1].save({first_name: "Tim"});
      var vId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tom"});
      var vId2 = vertex2._id;
      var edge = g[ec1].save(vId1, vId2, {});
      var edgeId = edge._id;
      var result = g[vc1].remove(vId1);
      assertTrue(result);
      assertFalse(db[ec1].exists(edgeId));
      result = g[vc1].remove(vId2);
      assertTrue(result);
    },

    test_eC_save_undirected : function() {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vId2 = vertex2._id;
      var edge = g[ec1].save(vertexId1, vId2, {});
      assertFalse(edge.error);
      g[vc1].remove(vId1);
      g[vc1].remove(vId2);
    },

    test_eC_save_directed : function() {
      var vertex1 = g[vc2].save({first_name: "Tom"});
      vertexId1 = vertex1._id;
      var vertex2 = g[vc4].save({first_name: "Tim"});
      vertexId2 = vertex2._id;
      var edge = g[ec2].save(vertexId1, vertexId2, {});
      assertFalse(edge.error);
      g[vc2].remove(vertexId1);
      g[vc4].remove(vertexId2);
    },

    test_eC_save_withError : function() {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      vertexId1 = vertex1._id;
      var vertex2 = g[vc2].save({first_name: "Tim"});
      vertexId2 = vertex2._id;
      try {
        g[ec1].save(vertexId1, vertexId2, {});
        fail();
      } catch (e) {
        assertEqual(e, "Edge is not allowed between " + vertexId1 + " and " + vertexId2 + ".");
      }
      g[vc1].remove(vertexId1);
      g[vc2].remove(vertexId2);
    },

    test_eC_replace : function() {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var edge = g[ec1].save(vertexId1, vertexId2, {});
      var edgeId1 = edge._id;
      edge = g[ec1].replace(edgeId1, {label: "knows"});
      assertFalse(edge.error);
      var edgeObj = g[ec1].document(edgeId1);
      assertEqual(edgeObj.label, "knows");
      assertEqual(edgeObj._id, edgeId1);
    },

    test_eC_update : function () {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var edge = g[ec1].save(vertexId1, vertexId2, {});
      var edgeId1 = edge._id;
      edge = g[ec1].replace(edgeId1, {label: "knows"});
      edge = g[ec1].update(edgeId1, {blub: "blub"});
      assertFalse(edge.error);
      var edgeObj = g[ec1].document(edgeId1);
      assertEqual(edgeObj.label, "knows");
      assertEqual(edgeObj.blub, "blub");
      assertEqual(edgeObj._id, edgeId1);
    },

    test_eC_remove : function () {
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var edge = g[ec1].save(vertexId1, vertexId2, {});
      var edgeId1 = edge._id;
      edge = g[ec1].remove(edgeId1);
      assertTrue(edge);
    },


    test_eC_removeWithEdgesAsVertices : function () {

      var myGraphName = unitTestGraphName + "0815";
      var myEC02 = "unitTestEdgeCollection02";
      var myVC01 = "unitTestVertexCollection01";
      try {
        graph._drop(myGraphName);
        db._drop(myEC02);
        db._drop(myVC01);
      } catch (ignore) {
      }
      var g2 = graph._create(
        myGraphName,
        graph._edgeDefinitions(
          graph._directedRelationDefinition(myEC02,
            [ec1], [myVC01]
          )
        )
      );
      var vertex1 = g[vc1].save({first_name: "Tom"});
      var vertexId1 = vertex1._id;
      var vertex2 = g[vc1].save({first_name: "Tim"});
      var vertexId2 = vertex2._id;
      var vertex3 = g2.unitTestVertexCollection01.save({first_name: "Ralph"});
      var vertexId3 = vertex3._id;
      var edge = g[ec1].save(vertexId1, vertexId2, {});
      var edge2 = g2.unitTestEdgeCollection02.save(edge._id, vertexId3, {});

      var edgeId1 = edge._id;
      edge = g[ec1].remove(edgeId1);
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
      try {
        graph._drop(gN1);
        graph._drop(gN2);
        graph._drop(gN3);
        graph._drop(gN4);
        db._drop(eC1);
        db._drop(eC2);
        db._drop(eC3);
        db._drop(eC4);
        db._drop(vC1);
        db._drop(vC2);
        db._drop(vC3);
        db._drop(vC4);
      } catch (ignore) {
      }

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



function GeneralGraphCommonNeighborsSuite() {
  var testGraph, actual;

  var v1ColName = "UnitTestsAhuacatlVertex1";
  var v2ColName = "UnitTestsAhuacatlVertex2";
  var eColName = "UnitTestsAhuacatlEdge1";
  var v1;
  var v2;
  var v3;
  var v4;
  var v5;
  var v6;
  var v7;
  var v8;

  var createKeyValueObject = function(key, value) {
    var res = {};
    res[key] = value;
    return res;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(v1ColName);
      db._drop(v2ColName);
      db._drop(eColName);

      var vertex1 = db._create(v1ColName);
      var vertex2 = db._create(v2ColName);
      var edge1 = db._createEdgeCollection(eColName);

      v1 = vertex1.save({ _key: "v1" , hugo : true})._id;
      v2 = vertex1.save({ _key: "v2" ,hugo : true})._id;
      v3 = vertex1.save({ _key: "v3" , heinz : 1})._id;
      v4 = vertex1.save({ _key: "v4" , harald : "meier"})._id;
      v5 = vertex2.save({ _key: "v5" , ageing : true})._id;
      v6 = vertex2.save({ _key: "v6" , harald : "meier", ageing : true})._id;
      v7 = vertex2.save({ _key: "v7" ,harald : "meier"})._id;
      v8 = vertex2.save({ _key: "v8" ,heinz : 1, harald : "meier"})._id;

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
      testGraph = graph._create(
        "bla3",
        graph._edgeDefinitions(
          graph._directedRelationDefinition(eColName,
            [v1ColName, v2ColName],
            [v1ColName, v2ColName]
          )
        )
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop(v1ColName);
      db._drop(v2ColName);
      db._drop(eColName);
      try {
        db._collection("_graphs").remove("_graphs/bla3");
      } catch (ignore) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks GRAPH_COMMON_NEIGHBORS() and GRAPH_COMMON_PROPERTIES()
////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsAny: function () {
      actual = testGraph._listCommonNeighbors(v3 , v6,  {direction : 'any'});
      assertEqual(actual[0][v3][v6][0]._id  , v2);
      assertEqual(actual[0][v3][v6][1]._id  , v7);
      actual = testGraph._amountCommonNeighbors(v3 , v6,  {direction : 'any'});
      assertEqual(actual[0][v3][0][v6] , 2);
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief checks GRAPH_COMMON_NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsIn: function () {
      actual = testGraph._listCommonNeighbors({} , {},  {direction : 'inbound'});
      assertEqual(actual[0][v3][v6][0]._id, v2);
      assertEqual(actual[1][v5][v8][0]._id, v3);
      assertEqual(actual[1][v5][v7][0]._id, v3);
      assertEqual(actual[2][v6][v3][0]._id, v2);
      assertEqual(actual[3][v7][v5][0]._id, v3);
      assertEqual(actual[3][v7][v8][0]._id, v3);
      assertEqual(actual[4][v8][v5][0]._id, v3);
      assertEqual(actual[4][v8][v7][0]._id, v3);

      actual = testGraph._amountCommonNeighbors({} , {},  {direction : 'inbound'});
      assertEqual(actual[0][v3][0][v6] , 1);
      assertEqual(actual[1][v5][0][v8] , 1);
      assertEqual(actual[1][v5][1][v7] , 1);
      assertEqual(actual[2][v6][0][v3] , 1);
      assertEqual(actual[3][v7][0][v5] , 1);
      assertEqual(actual[3][v7][1][v8] , 1);

      assertEqual(actual[4][v8][0][v5] , 1);
      assertEqual(actual[4][v8][1][v7] , 1);

    },


////////////////////////////////////////////////////////////////////////////////
/// @brief checks GRAPH_COMMON_NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsOut: function () {
      actual = testGraph._listCommonNeighbors(
        {hugo: true}, {heinz: 1},
        {direction: 'outbound', minDepth: 1, maxDepth: 3}
      );
      assertEqual(Object.keys(actual[1])[0], v2);
      assertEqual(Object.keys(actual[1][Object.keys(actual[1])[0]]), [v8, v3]);

      assertEqual(actual[1][Object.keys(actual[1])[0]][v8].length, 3);
      assertEqual(actual[1][Object.keys(actual[1])[0]][v3].length, 4);

      assertEqual(Object.keys(actual[0])[0], v1);
      assertEqual(Object.keys(actual[0][Object.keys(actual[0])[0]]), [v3, v8]);

      assertEqual(actual[0][Object.keys(actual[0])[0]][v3].length, 4);
      assertEqual(actual[0][Object.keys(actual[0])[0]][v8].length, 3);

      actual = testGraph._amountCommonNeighbors(
        {hugo: true }, {heinz: 1},
        {direction: 'outbound', minDepth: 1, maxDepth: 3}
      );
      assertEqual(actual[0][v1][0][v3], 4);
      assertEqual(actual[0][v1][1][v8], 3);
      assertEqual(actual[1][v2][0][v8], 3);
      assertEqual(actual[1][v2][1][v3], 4);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks GRAPH_COMMON_PROPERTIES()
////////////////////////////////////////////////////////////////////////////////

    testCommonProperties: function () {
      actual = testGraph._listCommonProperties({} ,{} ,{});
      assertEqual(actual[0][v1][0]._id  , v2);
      assertEqual(actual[1][v2][0]._id  , v1);
      assertEqual(actual[2][v3][0]._id  , v8);
      assertEqual(actual[3][v4][0]._id  , v6);
      assertEqual(actual[3][v4][1]._id  , v8);
      assertEqual(actual[3][v4][2]._id  , v7);
      assertEqual(actual[4][v5][0]._id  , v6);
      assertEqual(actual[5][v6][0]._id  , v4);
      assertEqual(actual[5][v6][1]._id  , v5);
      assertEqual(actual[5][v6][2]._id  , v8);
      assertEqual(actual[5][v6][3]._id  , v7);
      assertEqual(actual[6][v7][0]._id  , v4);
      assertEqual(actual[6][v7][1]._id  , v6);
      assertEqual(actual[6][v7][2]._id  , v8);
      assertEqual(actual[7][v8][0]._id  , v3);
      assertEqual(actual[7][v8][1]._id  , v4);
      assertEqual(actual[7][v8][2]._id  , v6);
      assertEqual(actual[7][v8][3]._id  , v7);

      actual = testGraph._amountCommonProperties({} ,{} ,{});
      assertEqual(actual, [
        createKeyValueObject(v1, 1),
        createKeyValueObject(v2, 1),
        createKeyValueObject(v3, 1),
        createKeyValueObject(v4, 3),
        createKeyValueObject(v5, 1),
        createKeyValueObject(v6, 4),
        createKeyValueObject(v7, 3),
        createKeyValueObject(v8, 4)
      ]);
    },

    testCommonPropertiesWithFilters: function () {
      actual = testGraph._listCommonProperties({ageing : true} , {harald : 'meier'},  {});
      assertEqual(actual[0][v5][0]._id  , v6);
      assertEqual(actual[1][v6][0]._id  , v4);
      assertEqual(actual[1][v6][1]._id  , v8);
      assertEqual(actual[1][v6][2]._id  , v7);

      actual = testGraph._amountCommonProperties({ageing : true} , {harald : 'meier'},  {});
      assertEqual(actual, [
        createKeyValueObject(v5, 1),
        createKeyValueObject(v6, 3)
      ]);

    },

    testCommonPropertiesWithFiltersAndIgnoringKeyHarald: function () {
      actual = testGraph._listCommonProperties( {} , {},  {ignoreProperties : 'harald'});

      assertEqual(actual[0][v1][0]._id  , v2);
      assertEqual(actual[1][v2][0]._id  , v1);
      assertEqual(actual[2][v3][0]._id  , v8);
      assertEqual(actual[3][v5][0]._id  , v6);
      assertEqual(actual[4][v6][0]._id  , v5);
      assertEqual(actual[5][v8][0]._id  , v3);

      actual = testGraph._amountCommonProperties({} , {},  {ignoreProperties : 'harald'});
      assertEqual(actual, [
        createKeyValueObject(v1, 1),
        createKeyValueObject(v2, 1),
        createKeyValueObject(v3, 1),
        createKeyValueObject(v5, 1),
        createKeyValueObject(v6, 1),
        createKeyValueObject(v8, 1)
      ]);
    }
  };
}


// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(GeneralGraphCommonNeighborsSuite);
jsunity.run(GeneralGraphAQLQueriesSuite);
jsunity.run(EdgesAndVerticesSuite);
jsunity.run(GeneralGraphCreationSuite);
jsunity.run(ChainedFluentAQLResultsSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
