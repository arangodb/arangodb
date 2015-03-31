/*jshint globalstrict: false, strict: false, sub: true, maxlen: 500 */
/*global require, assertEqual */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("org/arangodb").db;
var errors = require("internal").errors;
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for EDGES() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryEdgesTestSuite () {
  var vertex = null;
  var edge   = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop("UnitTestsAhuacatlVertex");
      db._drop("UnitTestsAhuacatlEdge");

      vertex = db._create("UnitTestsAhuacatlVertex");
      edge = db._createEdgeCollection("UnitTestsAhuacatlEdge");

      vertex.save({ _key: "v1" });
      vertex.save({ _key: "v2" });
      vertex.save({ _key: "v3" });
      vertex.save({ _key: "v4" });
      vertex.save({ _key: "v5" });
      vertex.save({ _key: "v6" });
      vertex.save({ _key: "v7" });

      function makeEdge (from, to) {
        edge.save("UnitTestsAhuacatlVertex/" + from, "UnitTestsAhuacatlVertex/" + to, { what: from + "->" + to });
      }

      makeEdge("v1", "v2");
      makeEdge("v1", "v3");
      makeEdge("v2", "v3");
      makeEdge("v3", "v4");
      makeEdge("v3", "v6");
      makeEdge("v3", "v7");
      makeEdge("v4", "v2");
      makeEdge("v7", "v3");
      makeEdge("v6", "v3");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsAhuacatlVertex");
      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesAny : function () {
      var actual;
     
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v1', 'any') SORT e.what RETURN e.what");
      assertEqual(actual, [ "v1->v2", "v1->v3" ]);

      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v2', 'any') SORT e.what RETURN e.what");
      assertEqual(actual, [ "v1->v2", "v2->v3", "v4->v2" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'any') SORT e.what RETURN e.what");
      assertEqual(actual, [ "v1->v3", "v2->v3", "v3->v4", "v3->v6", "v3->v7", "v6->v3", "v7->v3" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'any') SORT e.what RETURN e.what");
      assertEqual(actual, [ "v1->v3", "v2->v3", "v3->v4", "v3->v6", "v3->v7", "v6->v3", "v7->v3" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v8', 'any') SORT e.what RETURN e.what");
      assertEqual(actual, [ ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/thefox', 'any') SORT e.what RETURN e.what");
      assertEqual(actual, [ ]);
     
      assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, "FOR e IN EDGES(UnitTestsAhuacatlEdge, 'thefox/thefox', 'any') SORT e.what RETURN e.what");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesIn : function () {
      var actual;
     
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v1', 'inbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ ]);

      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v2', 'inbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ "v1->v2", "v4->v2" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'inbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ "v1->v3", "v2->v3", "v6->v3", "v7->v3" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'inbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ "v1->v3", "v2->v3", "v6->v3", "v7->v3" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v8', 'inbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/thefox', 'inbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ ]);
     
      assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, "FOR e IN EDGES(UnitTestsAhuacatlEdge, 'thefox/thefox', 'inbound') SORT e.what RETURN e.what");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testNeighborsAny : function () {
      var actual;
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v1', 'any') SORT n.vertex._key RETURN [ n.vertex._key, n.edge.what ]");
      assertEqual(actual, [ [ "v2", "v1->v2" ], [ "v3", "v1->v3" ] ]);

      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v2', 'any') SORT n.vertex._key RETURN [ n.vertex._key, n.edge.what ]");
      assertEqual(actual, [ [ "v1", "v1->v2" ], [ "v3", "v2->v3" ], [ "v4", "v4->v2" ] ]);
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'any') SORT n.vertex._key, n.edge.what RETURN [ n.vertex._key, n.edge.what ]");
      assertEqual(actual, [ [ "v1", "v1->v3"], [ "v2", "v2->v3" ], [ "v4", "v3->v4" ], [ "v6", "v3->v6" ], [ "v6", "v6->v3"], [ "v7", "v3->v7" ], [ "v7", "v7->v3" ] ]);
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v8', 'any') SORT n.vertex._key RETURN n.vertex._key");
      assertEqual(actual, [ ]);
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v5', 'any') SORT n.vertex._key RETURN n.vertex._key");
      assertEqual(actual, [ ]);

      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/thefox', 'any') SORT n.vertex._key RETURN n.vertex._key");
      assertEqual(actual, [ ]);
      
      assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, "FOR e IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'thefox/thefox', 'any') SORT n.vertex._key RETURN n.vertex._key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testNeighborsIn : function () {
      var actual;
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v1', 'inbound') SORT n.vertex._key RETURN [ n.vertex._key, n.edge.what ]");
      assertEqual(actual, [ ]);

      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v2', 'inbound') SORT n.vertex._key RETURN [ n.vertex._key, n.edge.what ]");
      assertEqual(actual, [ [ "v1", "v1->v2" ], [ "v4", "v4->v2" ] ]);
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'inbound') SORT n.vertex._key RETURN [ n.vertex._key, n.edge.what ]");
      assertEqual(actual, [ [ "v1", "v1->v3"], [ "v2", "v2->v3" ], [ "v6", "v6->v3"], [ "v7", "v7->v3" ] ]);
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v8', 'inbound') SORT n.vertex._key RETURN n.vertex._key");
      assertEqual(actual, [ ]);
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v5', 'inbound') SORT n.vertex._key RETURN n.vertex._key");
      assertEqual(actual, [ ]);

      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/thefox', 'inbound') SORT n.vertex._key RETURN n.vertex._key");
      assertEqual(actual, [ ]);
      
      assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, "FOR e IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'thefox/thefox', 'inbound') SORT n.vertex._key RETURN n.vertex._key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesOut : function () {
      var actual;
     
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v1', 'outbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ "v1->v2", "v1->v3" ]);

      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v2', 'outbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ "v2->v3" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'outbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ "v3->v4", "v3->v6", "v3->v7" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v8', 'outbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v5', 'outbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ ]);

      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/thefox', 'outbound') SORT e.what RETURN e.what");
      assertEqual(actual, [ ]);
      
      assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, "FOR e IN EDGES(UnitTestsAhuacatlEdge, 'thefox/thefox', 'outbound') SORT e.what RETURN e.what");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testNeighborsOut : function () {
      var actual;
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v1', 'outbound') SORT n.vertex._key RETURN [ n.vertex._key, n.edge.what ]");
      assertEqual(actual, [ [ "v2", "v1->v2" ], [ "v3", "v1->v3" ] ]);

      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v2', 'outbound') SORT n.vertex._key RETURN [ n.vertex._key, n.edge.what ]");
      assertEqual(actual, [ [ "v3", "v2->v3" ] ]);
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'outbound') SORT n.vertex._key RETURN n.vertex._key");
      assertEqual(actual, [ "v4", "v6", "v7" ]);
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v8', 'outbound') SORT n.vertex._key RETURN n.vertex._key");
      assertEqual(actual, [ ]);
      
      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v5', 'outbound') SORT n.vertex._key RETURN n.vertex._key");
      assertEqual(actual, [ ]);

      actual = getQueryResults("FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/thefox', 'outbound') SORT n.vertex._key RETURN n.vertex._key");
      assertEqual(actual, [ ]);
      
      assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, "FOR e IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, 'thefox/thefox', 'outbound') SORT n.vertex._key RETURN n.vertex._key");
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for PATHS() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryPathsTestSuite () {
  var users = null;
  var relations = null;
  var docs = { };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop("UnitTestsAhuacatlUsers");
      db._drop("UnitTestsAhuacatlUserRelations");

      users = db._create("UnitTestsAhuacatlUsers");
      relations = db._createEdgeCollection("UnitTestsAhuacatlUserRelations");

      docs["John"] = users.save({ "id" : 100, "name" : "John" });
      docs["Fred"] = users.save({ "id" : 101, "name" : "Fred" });
      docs["Jacob"] = users.save({ "id" : 102, "name" : "Jacob" });

      relations.save(docs["John"], docs["Fred"], { what: "John->Fred" });
      relations.save(docs["Fred"], docs["Jacob"], { what: "Fred->Jacob" });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsAhuacatlUsers");
      db._drop("UnitTestsAhuacatlUserRelations");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound1 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["John"]._id, actual[0].source._id);
      assertEqual(docs["Fred"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutboundMaxLength1 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\", { maxLength: 1 }) FILTER p.edges[0]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["John"]._id, actual[0].source._id);
      assertEqual(docs["Fred"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound2 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._from == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Fred"]._id, actual[0].source._id);
      assertEqual(docs["Jacob"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutboundMaxLength2 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\", { maxLength: 1 }) FILTER p.edges[0]._from == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Fred"]._id, actual[0].source._id);
      assertEqual(docs["Jacob"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _to, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound3 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._to == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["John"]._id, actual[0].source._id);
      assertEqual(docs["Fred"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _to, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutboundMaxLength3 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\", { maxLength: 1 }) FILTER p.edges[0]._to == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["John"]._id, actual[0].source._id);
      assertEqual(docs["Fred"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from, path length 2
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound4 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 2 && p.edges[0]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(2, actual[0].edges.length);

      assertEqual(docs["John"]._id, actual[0].source._id);
      assertEqual(docs["Jacob"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
      assertEqual(docs["Fred"]._id, actual[0].edges[1]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[1]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from, path length 2
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutboundMaxLength4 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\", { minLength: 2, maxLength: 2 }) FILTER p.edges[0]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(2, actual[0].edges.length);

      assertEqual(docs["John"]._id, actual[0].source._id);
      assertEqual(docs["Jacob"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
      assertEqual(docs["Fred"]._id, actual[0].edges[1]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[1]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound5 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 2 && p.edges[0]._from == \"" + docs["Jacob"]._id +"\" RETURN p");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutboundMaxLength5 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\", { maxLength: 2 }) FILTER p.edges[0]._from == \"" + docs["Jacob"]._id +"\" RETURN p");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _to
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound6 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 2 && p.edges[0]._to == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _to
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutboundMaxLength6 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\", { maxLength: 2 }) FILTER p.edges[0]._to == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInbound1 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Fred"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInboundMaxLength1 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\", { maxLength: 1 }) FILTER p.edges[0]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Fred"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInbound2 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._from == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Jacob"]._id, actual[0].source._id);
      assertEqual(docs["Fred"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInboundMaxLength2 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\", { maxLength: 1 }) FILTER p.edges[0]._from == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Jacob"]._id, actual[0].source._id);
      assertEqual(docs["Fred"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _to, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInbound3 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._to == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Fred"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _to, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInboundMaxLength3 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\", { maxLength: 1 }) FILTER p.edges[0]._to == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Fred"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInbound4 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\") FILTER LENGTH(p.edges) == 2 && p.edges[LENGTH(p.edges) - 1]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(2, actual[0].edges.length);

      assertEqual(docs["Jacob"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);

      assertEqual(docs["John"]._id, actual[0].edges[1]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[1]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInboundMaxLength4 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\", { minLength: 2, maxLength: 2 }) FILTER p.edges[LENGTH(p.edges) - 1]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(2, actual[0].edges.length);

      assertEqual(docs["Jacob"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);

      assertEqual(docs["John"]._id, actual[0].edges[1]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[1]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 2
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInboundMinLength1 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\", { minLength: 2, maxLength: 2 }) FILTER p.edges[LENGTH(p.edges) - 1]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(2, actual[0].edges.length);

      assertEqual(docs["Jacob"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);

      assertEqual(docs["John"]._id, actual[0].edges[1]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[1]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 2
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInboundMinLength2 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\", { minLength: 2 }) FILTER p.edges[LENGTH(p.edges) - 1]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(2, actual[0].edges.length);

      assertEqual(docs["Jacob"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);

      assertEqual(docs["John"]._id, actual[0].edges[1]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[1]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 3
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInboundMinLength3 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\", { minLength: 3 }) FILTER p.edges[LENGTH(p.edges) - 1]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks AQL inbound and outbound queries
////////////////////////////////////////////////////////////////////////////////

    testPathExamples : function () {
       users.save({ _key : "v1", "value": "v1Value" });
       users.save({ _key : "v2", "value": "v2Value" });
       users.save({ _key : "v3", "value": "v3Value" });
       users.save({ _key : "v4", "value": "v4Value" });
       
       relations.save(users.name() + "/v1", users.name() + "/v2", { "value": "v1 to v2 connection" });
       relations.save(users.name() + "/v2", users.name() + "/v3", { "value": "v2 to v3 connection" });
       relations.save(users.name() + "/v3", users.name() + "/v4", { "value": "v3 to v4 connection" });

       var actual;

       actual = db._createStatement({ "query": "FOR p IN PATHS(" + users.name() + ", " + relations.name() + ", 'inbound') FILTER p.destination._id == '" + users.name() + "/v1' && p.source._id == '" + users.name() + "/v4' RETURN p.vertices[*]._key" }).execute().toArray();
       assertEqual([ [ "v4", "v3", "v2", "v1" ] ], actual);

       actual = db._createStatement({ "query": "FOR p IN PATHS(" + users.name() + ", " + relations.name() + ", 'outbound') FILTER p.source._id == '" + users.name() + "/v1' && p.destination._id == '" + users.name() + "/v4' RETURN p.vertices[*]._key" }).execute().toArray();
       assertEqual([ [ "v1", "v2", "v3", "v4" ] ], actual);

     }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for SHORTEST_PATH() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryShortestPathTestSuite () {
  var vn = "UnitTestsTraversalVertices";
  var en = "UnitTestsTraversalEdges";
  var vertexCollection;
  var edgeCollection;

  var aqlfunctions = require("org/arangodb/aql/functions");

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(vn);
      db._drop(en);

      vertexCollection = db._create(vn);
      edgeCollection = db._createEdgeCollection(en);

      [ "A", "B", "C", "D", "E", "F", "G", "H" ].forEach(function (item) {
        vertexCollection.save({ _key: item, name: item });
      });

      [ [ "A", "B", 1 ], [ "B", "C", 5 ], [ "C", "D", 1 ], [ "A", "D", 12 ], [ "D", "C", 3 ], [ "C", "B", 2 ], [ "D", "E", 6 ], [ "B", "F", 1 ], [ "E", "G", 5 ], [ "G", "H", 2 ] ].forEach(function (item) {
        var l = item[0];
        var r = item[1];
        var w = item[2];
        edgeCollection.save(vn + "/" + l, vn + "/" + r, { _key: l + r, what : l + "->" + r, weight: w });
      });

      try {
        aqlfunctions.unregister("UnitTests::distance");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(vn);
      db._drop(en);

      vertexCollection = null;
      edgeCollection = null;

      try {
        aqlfunctions.unregister("UnitTests::distance");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path using dijkstra
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraOutbound : function () {
      var config = {
        paths: true,
        _sort: true
      };

      var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN [ p.vertex._key, p.path.vertices[*]._key, p.path.edges[*]._key ]", { "@v" : vn, "@e" : en }); 

      assertEqual([ 
        [ "A", [ "A" ], [ ] ],
        [ "D", [ "A", "D" ], [ "AD" ] ],
        [ "E", [ "A", "D", "E" ], [ "AD", "DE" ] ],
        [ "G", [ "A", "D", "E", "G" ], [ "AD", "DE", "EG" ] ],
        [ "H", [ "A", "D", "E", "G", "H" ], [ "AD", "DE", "EG", "GH" ] ]
      ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path using dijkstra
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraInbound : function () {
      var config = {
        _sort: true
      };

      var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/H', '" + vn + "/A', 'inbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "H", "G", "E", "D", "A" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path with custom distance function
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraDistance : function () {
      aqlfunctions.register("UnitTests::distance", function (config, vertex1, vertex2, edge) { 
        return edge.weight; 
      }, false);

      var config = {
        distance: "UnitTests::distance",
        _sort: true
      };

      var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "A", "B", "C", "D", "E", "G", "H" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path with vertex filter function
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraVertexFilter1 : function () {
      aqlfunctions.register("UnitTests::distance", function (config, vertex) {
        if (vertex._key === 'B' || vertex._key === 'C') {
          return [ 'exclude', 'prune' ];
        }
      }, false);

      var config = {
        filterVertices: "UnitTests::distance",
        _sort: true
      };

      var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "A", "D", "E", "G", "H" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path with vertex filter function
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraVertexFilter2 : function () {
      aqlfunctions.register("UnitTests::distance", function () {
        return [ 'exclude', 'prune' ];
      }, false);

      var config = {
        filterVertices: "UnitTests::distance",
        _sort: true
      };

      var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path with edge filter function
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraEdgeFilter : function () {
      aqlfunctions.register("UnitTests::distance", function (config, vertex, edge) {
        return edge.weight <= 6;
      }, false);

      var config = {
        followEdges: "UnitTests::distance",
        _sort: true
      };

      var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "A", "B", "C", "D", "E", "G", "H" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path, with cycles
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraCycles : function () {
      [ [ "B", "A" ], [ "C", "A" ], [ "D", "B" ] ].forEach(function (item) {
        var l = item[0];
        var r = item[1];
        edgeCollection.save(vn + "/" + l, vn + "/" + r, { _key: l + r, what : l + "->" + r });
      });

      var config = {
        paths: true,
        _sort: true
      };

      var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN [ p.vertex._key, p.path.vertices[*]._key, p.path.edges[*]._key ]", { "@v" : vn, "@e" : en }); 

      assertEqual([ 
        [ "A", [ "A" ], [ ] ],
        [ "D", [ "A", "D" ], [ "AD" ] ],
        [ "E", [ "A", "D", "E" ], [ "AD", "DE" ] ],
        [ "G", [ "A", "D", "E", "G" ], [ "AD", "DE", "EG" ] ],
        [ "H", [ "A", "D", "E", "G", "H" ], [ "AD", "DE", "EG", "GH" ] ]
      ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path, non-connected vertices
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraNotConnected : function () {
      // this item is not connected to any other
      vertexCollection.save({ _key: "J", name: "J" });

      var config = {
        paths: true,
        _sort: true
      };

      var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/J', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ ], actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TRAVERSAL() filter function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryTraversalFilterTestSuite () {
  var vn = "UnitTestsTraversalVertices";
  var en = "UnitTestsTraversalEdges";
  var vertexCollection;
  var edgeCollection;
      
  var aqlfunctions = require("org/arangodb/aql/functions");

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(vn);
      db._drop(en);

      vertexCollection = db._create(vn);
      edgeCollection = db._createEdgeCollection(en);

      [ 
        { name: "1", isDeleted: false }, 
        { name: "1-1", isDeleted: false }, 
        { name: "1-1-1", isDeleted: true },  
        { name: "1-1-2", isDeleted: false },  
        { name: "1-2", isDeleted: true }, 
        { name: "1-2-1", isDeleted: false }, 
        { name: "1-3", isDeleted: false }, 
        { name: "1-3-1", isDeleted: true }, 
        { name: "1-3-2", isDeleted: false },
        { name: "1-3-3", isDeleted: true }
      ].forEach(function (item) {
        vertexCollection.save({ _key: item.name, name: item.name, isDeleted: item.isDeleted });
      });

      [ [ "1", "1-1" ], [ "1", "1-2" ], [ "1", "1-3" ], [ "1-1", "1-1-1" ], [ "1-1", "1-1-2" ], [ "1-2", "1-2-1" ], [ "1-3", "1-3-1" ], [ "1-3", "1-3-2" ], [ "1-3", "1-3-3" ] ].forEach(function (item) {
        var l = item[0];
        var r = item[1];
        edgeCollection.save(vn + "/" + l, vn + "/" + r, { _key: l + r, what : l + "->" + r });
      });

      try {
        aqlfunctions.unregister("UnitTests::filter");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(vn);
      db._drop(en);

      try {
        aqlfunctions.unregister("UnitTests::filter");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test vertex filter with custom function
////////////////////////////////////////////////////////////////////////////////

    testTraversalVertexFilterCallback : function () {
      aqlfunctions.register("UnitTests::filter", function (config, vertex) {
        if (vertex._key.substr(0, 3) === '1-1') {
          return;
        }
        return 'exclude';
      });

      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        _sort: true,
        filterVertices: "UnitTests::filter"
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/1', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "1-1", "1-1-1", "1-1-2" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test vertex filter
////////////////////////////////////////////////////////////////////////////////

    testTraversalVertexFilterExcludeMult : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        _sort: true,
        filterVertices: [ { name: "1-1" }, { name: "1-2" }, { name: "1-3" } ],
        vertexFilterMethod : [ "exclude" ]
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/1', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "1-1", "1-2", "1-3" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test vertex filter
////////////////////////////////////////////////////////////////////////////////

    testTraversalVertexFilterPruneExclude : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        _sort: true,
        filterVertices: [ { isDeleted: false } ],
        vertexFilterMethod : [ "prune", "exclude" ]
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/1', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "1", "1-1", "1-1-2", "1-3", "1-3-2" ], actual);

      // no use the default value for filterVertices
      config.vertexFilterMethod = undefined;
      
      actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/1', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "1", "1-1", "1-1-2", "1-3", "1-3-2" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test vertex filter
////////////////////////////////////////////////////////////////////////////////

    testTraversalVertexFilterPrune : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        _sort: true,
        filterVertices: [ { isDeleted: false } ],
        vertexFilterMethod : [ "prune" ]
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/1', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "1", "1-1", "1-1-1", "1-1-2", "1-2", "1-3", "1-3-1", "1-3-2", "1-3-3" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test vertex filter
////////////////////////////////////////////////////////////////////////////////

    testTraversalVertexFilterPruneMult : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        _sort: true,
        filterVertices: [ { name: "1" }, { name: "1-1" }, { name: "1-2" }, { name: "1-2-1" } ],
        vertexFilterMethod : [ "prune", "exclude" ]
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/1', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "1", "1-1", "1-2", "1-2-1" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test vertex filter
////////////////////////////////////////////////////////////////////////////////

    testTraversalVertexFilterExclude : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        _sort: true,
        filterVertices: [ { isDeleted: false } ],
        vertexFilterMethod : [ "exclude" ]
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/1', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 
      assertEqual([ "1", "1-1", "1-1-2", "1-2-1", "1-3", "1-3-2" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test edge filter with custom function
////////////////////////////////////////////////////////////////////////////////

    testTraversalEdgeFilterCallback : function () {
      aqlfunctions.register("UnitTests::filter", function (config, vertex) {
        return ! vertex.isDeleted;
      });

      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        _sort: true,
        followEdges: "UnitTests::filter"
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/1', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "1", "1-1", "1-1-2", "1-3", "1-3-2" ], actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TRAVERSAL() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryTraversalTestSuite () {
  var vn = "UnitTestsTraversalVertices";
  var en = "UnitTestsTraversalEdges";
  var vertexCollection;
  var edgeCollection;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(vn);
      db._drop(en);

      vertexCollection = db._create(vn);
      edgeCollection = db._createEdgeCollection(en);

      [ "A", "B", "C", "D" ].forEach(function (item) {
        vertexCollection.save({ _key: item, name: item });
      });

      [ [ "A", "B" ], [ "B", "C" ], [ "A", "D" ], [ "D", "C" ], [ "C", "A" ] ].forEach(function (item) {
        var l = item[0];
        var r = item[1];
        edgeCollection.save(vn + "/" + l, vn + "/" + r, { _key: l + r, what : l + "->" + r });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(vn);
      db._drop(en);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min-depth filtering
////////////////////////////////////////////////////////////////////////////////

    testTraversalBreadthFirst : function () {
      var config = {
        strategy: "breadthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "none", 
          edges: "path"
        },
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "A", "B", "D", "C", "C", "A", "A", "D", "B", "C", "C" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min-depth filtering, track paths
////////////////////////////////////////////////////////////////////////////////

    testTraversalTrackPaths : function () {
      var config = {
        paths: true,
        minDepth: 1,
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN [ p.vertex._key, p.path.vertices[*]._key, p.path.edges[*]._key ]", { "@v" : vn, "@e" : en }); 

      assertEqual([ 
        [ "B", [ "A", "B" ], [ "AB" ] ],
        [ "C", [ "A", "B", "C" ], [ "AB", "BC" ] ],
        [ "D", [ "A", "D" ], [ "AD" ] ]
      ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min-depth filtering
////////////////////////////////////////////////////////////////////////////////

    testTraversalDepthFirstMin : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        minDepth: 1,
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "B", "C", "D" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max-depth filtering
////////////////////////////////////////////////////////////////////////////////

    testTraversalDepthFirstMax1 : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        maxDepth: 2,
        uniqueness: {
          vertices: "none", 
          edges: "none"
        },
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "A", "B", "C", "D", "C" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max-depth filtering
////////////////////////////////////////////////////////////////////////////////

    testTraversalDepthFirstMax2 : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        maxDepth: 3,
        uniqueness: {
          vertices: "none",
          edges: "none"
        },
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "A", "B", "C", "A", "D", "C", "A" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min-/max-depth filtering
////////////////////////////////////////////////////////////////////////////////

    testTraversalDepthFirstMinMax : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        minDepth: 1,
        maxDepth: 3,
        uniqueness: {
          vertices: "none", 
          edges: "none"
        },
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "B", "C", "A", "D", "C", "A" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max-depth filtering
////////////////////////////////////////////////////////////////////////////////

    testTraversalDepthFirstUniqueGlobal : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "A", "B", "C", "D" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max-depth filtering
////////////////////////////////////////////////////////////////////////////////

    testTraversalDepthFirstUniquePath : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "path", 
          edges: "none"
        },
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "A", "B", "C", "D", "C" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max-depth filtering
////////////////////////////////////////////////////////////////////////////////

    testTraversalDepthFirstUniqueEdgePath : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        uniqueness: {
          vertices: "none", 
          edges: "path"
        },
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "A", "B", "C", "A", "D", "C", "D", "C", "A", "B", "C" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max-depth filtering
////////////////////////////////////////////////////////////////////////////////

    testTraversalDepthFirstAny : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        maxDepth: 2,
        uniqueness: {
          vertices: "none", 
          edges: "none"
        },
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'any', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ "A", "B", "A", "C", "D", "A", "C", "C", "B", "A", "D" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test paths
////////////////////////////////////////////////////////////////////////////////

    testTraversalPaths : function () {
      var config = {
        strategy: "depthfirst",
        order: "preorder",
        itemOrder: "forward",
        maxDepth: 2,
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
        paths: true,
        _sort: true
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'any', " + JSON.stringify(config) + ") RETURN { p: p.path.vertices[*]._key, v: p.vertex._key }", { "@v" : vn, "@e" : en });
      assertEqual([ { p: [ "A" ], v: "A" }, { p: [ "A", "B" ], v: "B" }, { p: [ "A", "B", "C" ], v:  "C" }, { p: [ "A", "D" ], v: "D" } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-existing start vertex
////////////////////////////////////////////////////////////////////////////////

    testTraversalNonExisting : function () {
      var config = {
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/FOX', 'any', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([ ], actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TRAVERSAL_TREE() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryTraversalTreeTestSuite () {
  var vn = "UnitTestsTraversalVertices";
  var en = "UnitTestsTraversalEdges";
  var vertexCollection;
  var edgeCollection;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(vn);
      db._drop(en);

      vertexCollection = db._create(vn);
      edgeCollection = db._createEdgeCollection(en);

      [ "A", "B", "C", "D" ].forEach(function (item) {
        vertexCollection.save({ _key: item, name: item });
      });

      [ [ "A", "B" ], [ "B", "C" ], [ "A", "D" ], [ "D", "C" ], [ "C", "A" ] ].forEach(function (item) {
        var l = item[0];
        var r = item[1];
        edgeCollection.save(vn + "/" + l, vn + "/" + r, { _key: l + r, what : l + "->" + r });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(vn);
      db._drop(en);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min-depth filtering
////////////////////////////////////////////////////////////////////////////////

    testTraversalTree : function () {
      var config = {
        minDepth: 1,
        uniqueness: {
          vertices: "global", 
          edges: "none"
        },
      };

      var actual = getQueryResults("FOR p IN TRAVERSAL_TREE(@@v, @@e, '" + vn + "/A', 'outbound', 'connected', " + JSON.stringify(config) + ") RETURN p", { "@v" : vn, "@e" : en }); 
      assertEqual(1, actual.length);

      var root = actual[0];
      var nodeA = root[0];

      assertEqual("A", nodeA.name);
      assertEqual(2, nodeA.connected.length);

      var nodeB= nodeA.connected[0];
      assertEqual("B", nodeB.name);
      assertEqual(1, nodeB.connected.length);

      var nodeC = nodeB.connected[0];
      assertEqual("C", nodeC.name);
      assertEqual(undefined, nodeC.connected);

      var nodeD = nodeA.connected[1];
      assertEqual("D", nodeD.name);
      assertEqual(undefined, nodeD.connected);
    },

  };

}  

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryEdgesTestSuite);
jsunity.run(ahuacatlQueryPathsTestSuite);
jsunity.run(ahuacatlQueryShortestPathTestSuite);
jsunity.run(ahuacatlQueryTraversalFilterTestSuite);
jsunity.run(ahuacatlQueryTraversalTestSuite);
jsunity.run(ahuacatlQueryTraversalTreeTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
