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
var ArangoError = require("org/arangodb/arango-error").ArangoError; 
var QUERY = require("internal").AQL_QUERY;
var errors = require("internal").errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for EDGES() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryEdgesTestSuite () {
  var vertex = null;
  var edge = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = QUERY(query, undefined);
    if (cursor instanceof ArangoError) {
      print(query, cursor.errorMessage);
    }
    assertFalse(cursor instanceof ArangoError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, isFlat) {
    var result = executeQuery(query).getRows();
    var results = [ ];

    for (var i in result) {
      if (! result.hasOwnProperty(i)) {
        continue;
      }

      var row = result[i];
      if (isFlat) {
        results.push(row);
      } 
      else {
        var keys = [ ];
        for (var k in row) {
          if (row.hasOwnProperty(k) && k != '_id' && k != '_rev' && k != '_key') {
            keys.push(k);
          }
        }
       
        keys.sort();
        var resultRow = { };
        for (var k in keys) {
          if (keys.hasOwnProperty(k)) {
            resultRow[keys[k]] = row[keys[k]];
          }
        }
        results.push(resultRow);
      }
    }

    return results;
  }


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
     
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v1', 'any') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v1->v2", "v1->v3" ]);

      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v2', 'any') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v1->v2", "v2->v3", "v4->v2" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'any') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v1->v3", "v2->v3", "v3->v4", "v3->v6", "v3->v7", "v6->v3", "v7->v3" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'any') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v1->v3", "v2->v3", "v3->v4", "v3->v6", "v3->v7", "v6->v3", "v7->v3" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v8', 'any') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/thefox', 'any') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ ]);
     
      try { 
        actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'thefox/thefox', 'any') SORT e.what RETURN e.what", true);
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, err.errorNum);
      }
      
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesIn : function () {
      var actual;
     
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v1', 'inbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ ]);

      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v2', 'inbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v1->v2", "v4->v2" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'inbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v1->v3", "v2->v3", "v6->v3", "v7->v3" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'inbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v1->v3", "v2->v3", "v6->v3", "v7->v3" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v8', 'inbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/thefox', 'inbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ ]);
     
      try { 
        actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'thefox/thefox', 'inbound') SORT e.what RETURN e.what", true);
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesOut : function () {
      var actual;
     
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v1', 'outbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v1->v2", "v1->v3" ]);

      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v2', 'outbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v2->v3" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'outbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v3->v4", "v3->v6", "v3->v7" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v3', 'outbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ "v3->v4", "v3->v6", "v3->v7" ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v8', 'outbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ ]);
      
      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/v5', 'outbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ ]);

      actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'UnitTestsAhuacatlVertex/thefox', 'outbound') SORT e.what RETURN e.what", true);
      assertEqual(actual, [ ]);
      
      try {
        actual = getQueryResults("FOR e IN EDGES(UnitTestsAhuacatlEdge, 'thefox/thefox', 'outbound') SORT e.what RETURN e.what", true);
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, err.errorNum);
      }
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

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = QUERY(query, undefined);
    if (cursor instanceof ArangoError) {
      print(query, cursor.errorMessage);
    }
    assertFalse(cursor instanceof ArangoError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, isFlat) {
    var result = executeQuery(query).getRows();
    var results = [ ];

    for (var i in result) {
      if (! result.hasOwnProperty(i)) {
        continue;
      }

      var row = result[i];
      if (isFlat) {
        results.push(row);
      } 
      else {
        var keys = [ ];
        for (var k in row) {
          if (row.hasOwnProperty(k) && k != '_id' && k != '_rev' && k != '_key') {
            keys.push(k);
          }
        }
       
        keys.sort();
        var resultRow = { };
        for (var k in keys) {
          if (keys.hasOwnProperty(k)) {
            resultRow[keys[k]] = row[keys[k]];
          }
        }
        results.push(resultRow);
      }
    }

    return results;
  }


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
/// @brief checks an AQL outbound query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound5 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 2 && p.edges[0]._from == \"" + docs["Jacob"]._id +"\" RETURN p");
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
/// @brief test suite for TRAVERSAL() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryTraversalTestSuite () {
  var vn = "UnitTestsTraverseVertices";
  var en = "UnitTestsTraverseEdges";

  var vertices;
  var edges;

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query, params) {
    var cursor = QUERY(query, params);
    if (cursor instanceof ArangoError) {
      print(query, cursor.errorMessage);
    }
    assertFalse(cursor instanceof ArangoError);
    return cursor;
  }

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

      var actual = executeQuery("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }).getRows(); 

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

      var actual = executeQuery("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }).getRows(); 

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

      var actual = executeQuery("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }).getRows(); 

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

      var actual = executeQuery("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }).getRows(); 

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

      var actual = executeQuery("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }).getRows(); 

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

      var actual = executeQuery("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }).getRows(); 

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

      var actual = executeQuery("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }).getRows(); 

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

      var actual = executeQuery("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'any', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }).getRows(); 

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

      var actual = executeQuery("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/A', 'any', " + JSON.stringify(config) + ") RETURN { p: p.path.vertices[*]._key, v: p.vertex._key }", { "@v" : vn, "@e" : en }).getRows();
      assertEqual([ { p: [ "A" ], v: "A" }, { p: [ "A", "B" ], v: "B" }, { p: [ "A", "B", "C" ], v:  "C" }, { p: [ "A", "D" ], v: "D" } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-existing start vertex
////////////////////////////////////////////////////////////////////////////////

    testTraversalNonExisting : function () {
      var config = {
      };

      var actual = executeQuery("FOR p IN TRAVERSAL(@@v, @@e, '" + vn + "/FOX', 'any', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }).getRows(); 

      assertEqual([ ], actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TRAVERSAL_TREE() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryTraversalTreeTestSuite () {
  var vn = "UnitTestsTraverseVertices";
  var en = "UnitTestsTraverseEdges";

  var vertices;
  var edges;

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query, params) {
    var cursor = QUERY(query, params);
    if (cursor instanceof ArangoError) {
      print(query, cursor.errorMessage);
    }
    assertFalse(cursor instanceof ArangoError);
    return cursor;
  }

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

      var actual = executeQuery("FOR p IN TRAVERSAL_TREE(@@v, @@e, '" + vn + "/A', 'outbound', 'connected', " + JSON.stringify(config) + ") RETURN p", { "@v" : vn, "@e" : en }).getRows(); 
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
jsunity.run(ahuacatlQueryTraversalTestSuite);
jsunity.run(ahuacatlQueryTraversalTreeTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
