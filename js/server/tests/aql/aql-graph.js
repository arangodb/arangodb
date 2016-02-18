/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
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
var db = require("@arangodb").db;
var internal = require("internal");
var errors = internal.errors;
var helper = require("@arangodb/aql-helper");
var cluster = require("@arangodb/cluster");
var getQueryResults = helper.getQueryResults;
var getRawQueryResults = helper.getRawQueryResults;
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

      vertex.save({ _key: "v1", name: "v1" });
      vertex.save({ _key: "v2", name: "v2" });
      vertex.save({ _key: "v3", name: "v3" });
      vertex.save({ _key: "v4", name: "v4" });
      vertex.save({ _key: "v5", name: "v5" });
      vertex.save({ _key: "v6", name: "v6" });
      vertex.save({ _key: "v7", name: "v7" });

      function makeEdge (from, to) {
        edge.save("UnitTestsAhuacatlVertex/" + from, "UnitTestsAhuacatlVertex/" + to, { _key: from + "" + to, what: from + "->" + to });
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
      var queries = [
        "FOR e IN EDGES('UnitTestsAhuacatlEdge', @start, 'any') SORT e.what RETURN e.what",
        "FOR e IN EDGES(UnitTestsAhuacatlEdge, @start, 'any') SORT e.what RETURN e.what",
        "FOR e IN NOOPT(EDGES(UnitTestsAhuacatlEdge, @start, 'any')) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(EDGES('UnitTestsAhuacatlEdge', @start, 'any')) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(V8(EDGES(UnitTestsAhuacatlEdge, @start, 'any'))) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(V8(EDGES('UnitTestsAhuacatlEdge', @start, 'any'))) SORT e.what RETURN e.what"
      ];
     
      queries.forEach(function (q) {
        var actual;
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v1"});
        assertEqual(actual, [ "v1->v2", "v1->v3" ]);

        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v2"});
        assertEqual(actual, [ "v1->v2", "v2->v3", "v4->v2" ]);
        
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3"});
        assertEqual(actual, [ "v1->v3", "v2->v3", "v3->v4", "v3->v6", "v3->v7", "v6->v3", "v7->v3" ]);
        
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v8"});
        assertEqual(actual, [ ]);
        
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/thefox"});
        assertEqual(actual, [ ]);
       
        assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, q, {start: "thefox/thefox"});
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesIn : function () {
      var queries = [
        "FOR e IN EDGES(UnitTestsAhuacatlEdge, @start, 'inbound') SORT e.what RETURN e.what",
        "FOR e IN EDGES('UnitTestsAhuacatlEdge', @start, 'inbound') SORT e.what RETURN e.what",
        "FOR e IN NOOPT(EDGES(UnitTestsAhuacatlEdge, @start, 'inbound')) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(EDGES('UnitTestsAhuacatlEdge', @start, 'inbound')) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(V8(EDGES(UnitTestsAhuacatlEdge, @start, 'inbound'))) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(V8(EDGES('UnitTestsAhuacatlEdge', @start, 'inbound'))) SORT e.what RETURN e.what"
      ];
     
      queries.forEach(function (q) {
        var actual;
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v1"});
        assertEqual(actual, [ ]);

        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v2"});
        assertEqual(actual, [ "v1->v2", "v4->v2" ]);
        
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3"});
        assertEqual(actual, [ "v1->v3", "v2->v3", "v6->v3", "v7->v3" ]);
        
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v8"});
        assertEqual(actual, [ ]);
        
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/thefox"});
        assertEqual(actual, [ ]);
       
        assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, q, {start: "thefox/thefox"});
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesOut : function () {
      var queries = [
        "FOR e IN EDGES(UnitTestsAhuacatlEdge, @start, 'outbound') SORT e.what RETURN e.what",
        "FOR e IN EDGES('UnitTestsAhuacatlEdge', @start, 'outbound') SORT e.what RETURN e.what",
        "FOR e IN NOOPT(EDGES(UnitTestsAhuacatlEdge, @start, 'outbound')) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(EDGES('UnitTestsAhuacatlEdge', @start, 'outbound')) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(V8(EDGES(UnitTestsAhuacatlEdge, @start, 'outbound'))) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(V8(EDGES('UnitTestsAhuacatlEdge', @start, 'outbound'))) SORT e.what RETURN e.what"
      ];
     
      queries.forEach(function (q) {
        var actual;
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v1"});
        assertEqual(actual, [ "v1->v2", "v1->v3" ]);

        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v2"});
        assertEqual(actual, [ "v2->v3" ]);
        
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3"});
        assertEqual(actual, [ "v3->v4", "v3->v6", "v3->v7" ]);
        
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v8"});
        assertEqual(actual, [ ]);
        
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/thefox"});
        assertEqual(actual, [ ]);
       
        assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, q, {start: "thefox/thefox"});
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES() with includeVertices
////////////////////////////////////////////////////////////////////////////////

    testEdgesAnyInclVertices : function () {
      "use strict";
      let actual;
      const queries = [
        "FOR e IN EDGES(@@col, @start, @dir, null, {includeVertices: true}) SORT e.edge.what RETURN e.vertex._key",
        "FOR e IN NOOPT(EDGES(@@col, @start, @dir, null, {includeVertices: true})) SORT e.edge.what RETURN e.vertex._key",
        "FOR e IN NOOPT(V8(EDGES(@@col, @start, @dir, null, {includeVertices: true}))) SORT e.edge.what RETURN e.vertex._key"
      ];

      queries.forEach(function (query) {
        let bindVars = {
          "@col": "UnitTestsAhuacatlEdge",
          "dir": "any"
        };

        bindVars.start = "UnitTestsAhuacatlVertex/v1";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, ["v2", "v3"]);

        bindVars.start = "UnitTestsAhuacatlVertex/v2";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, ["v1", "v3", "v4"]);
        
        bindVars.start = "UnitTestsAhuacatlVertex/v3";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, ["v1", "v2", "v4", "v6", "v7", "v6", "v7"]);

        bindVars.start = "UnitTestsAhuacatlVertex/v8";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, [ ]);

        bindVars.start = "UnitTestsAhuacatlVertex/thefox";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, [ ]);
       
        bindVars.start = "thefox/thefox";

        assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, query, bindVars);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES() with includeVertices
////////////////////////////////////////////////////////////////////////////////

    testEdgesInInclVertices : function () {
      "use strict";
      let actual;
      const queries = [
        "FOR e IN EDGES(@@col, @start, @dir, null, {includeVertices: true}) SORT e.edge.what RETURN e.vertex._key",
        "FOR e IN NOOPT(EDGES(@@col, @start, @dir, null, {includeVertices: true})) SORT e.edge.what RETURN e.vertex._key",
        "FOR e IN NOOPT(V8(EDGES(@@col, @start, @dir, null, {includeVertices: true}))) SORT e.edge.what RETURN e.vertex._key"
      ];

      queries.forEach(function (query) {
        let bindVars = {
          "@col": "UnitTestsAhuacatlEdge",
          "dir": "inbound"
        };

        bindVars.start = "UnitTestsAhuacatlVertex/v1";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, []);

        bindVars.start = "UnitTestsAhuacatlVertex/v2";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, ["v1", "v4"]);

        bindVars.start = "UnitTestsAhuacatlVertex/v3";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, ["v1", "v2", "v6", "v7"]);

        bindVars.start = "UnitTestsAhuacatlVertex/v8";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, [ ]);
        
        bindVars.start = "UnitTestsAhuacatlVertex/thefox";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, [ ]);
       
        bindVars.start = "thefox/thefox";
        assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, query, bindVars);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES() with includeVertices
////////////////////////////////////////////////////////////////////////////////

    testEdgesOutInclVertices : function () {
      "use strict";
      let actual;
      const queries = [
        "FOR e IN EDGES(@@col, @start, @dir, null, {includeVertices: true}) SORT e.edge.what RETURN e.vertex._key",
        "FOR e IN NOOPT(EDGES(@@col, @start, @dir, null, {includeVertices: true})) SORT e.edge.what RETURN e.vertex._key",
        "FOR e IN NOOPT(V8(EDGES(@@col, @start, @dir, null, {includeVertices: true}))) SORT e.edge.what RETURN e.vertex._key"
      ];

      queries.forEach(function (query) {
        let bindVars = {
          "@col": "UnitTestsAhuacatlEdge",
          "dir": "outbound"
        };
       
        bindVars.start = "UnitTestsAhuacatlVertex/v1";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, ["v2", "v3"]);

        bindVars.start = "UnitTestsAhuacatlVertex/v2";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, ["v3"]);

        bindVars.start = "UnitTestsAhuacatlVertex/v3";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, ["v4", "v6", "v7"]);
        
        bindVars.start = "UnitTestsAhuacatlVertex/v8";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, [ ]);

        bindVars.start = "UnitTestsAhuacatlVertex/v5";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, [ ]);
        
        bindVars.start = "UnitTestsAhuacatlVertex/thefox";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, [ ]);
        
        bindVars.start = "thefox/thefox";
        assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, query, bindVars);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesFilterExample : function () {
      var queries = [
        "FOR e IN EDGES(UnitTestsAhuacatlEdge, @start, 'any', @examples) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(EDGES(UnitTestsAhuacatlEdge, @start, 'any', @examples)) SORT e.what RETURN e.what",
        "FOR e IN NOOPT(V8(EDGES(UnitTestsAhuacatlEdge, @start, 'any', @examples))) SORT e.what RETURN e.what"
      ];
     
      queries.forEach(function (q) {
        var actual;
        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3", examples: {what: "v1->v3"} });
        assertEqual(actual, [ "v1->v3" ]);

        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3", examples: [{what: "v1->v3"}, {what: "v3->v6"}] });
        assertEqual(actual, [ "v1->v3", "v3->v6"]);

        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3", examples: [] });
        assertEqual(actual, [ "v1->v3", "v2->v3", "v3->v4", "v3->v6", "v3->v7", "v6->v3", "v7->v3" ]);

        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3", examples: null });
        assertEqual(actual, [ "v1->v3", "v2->v3", "v3->v4", "v3->v6", "v3->v7", "v6->v3", "v7->v3" ]);

        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3", examples: {non: "matchable"} });
        assertEqual(actual, [ ]);

        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3", examples: [{what: "v1->v3"}, {non: "matchable"}] });
        assertEqual(actual, [ "v1->v3" ]);

        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3", examples: [{what: "v1->v3"}, {non: "matchable"}] });
        assertEqual(actual, [ "v1->v3" ]);

        actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3", examples: ["UnitTestsAhuacatlEdge/v1v3", {what: "v3->v6" } ] });
        assertEqual(actual, [ "v3->v6" ]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesStartVertexArray : function () {
      var queries = [
        "FOR e IN NOOPT(V8(EDGES(UnitTestsAhuacatlEdge, @start, 'outbound'))) SORT e.what RETURN e.what",
        "FOR e IN EDGES(UnitTestsAhuacatlEdge, @start, 'outbound') SORT e.what RETURN e.what",
        "FOR e IN NOOPT(EDGES(UnitTestsAhuacatlEdge, @start, 'outbound')) SORT e.what RETURN e.what"
      ];
     
      queries.forEach(function (q) {
        var actual;
        actual = getQueryResults(q, {start: [ "UnitTestsAhuacatlVertex/v1", "UnitTestsAhuacatlVertex/v2" ]});
        assertEqual(actual, [ "v1->v2", "v1->v3", "v2->v3" ]);

        actual = getQueryResults(q, {start: [ {_id: "UnitTestsAhuacatlVertex/v1"}, {_id: "UnitTestsAhuacatlVertex/v2"} ]});
        assertEqual(actual, [ "v1->v2", "v1->v3", "v2->v3" ]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesStartVertexObject : function () {
      var queries = [
        "FOR e IN NOOPT(V8(EDGES(UnitTestsAhuacatlEdge, @start, 'outbound'))) SORT e.what RETURN e.what",
        "FOR e IN EDGES(UnitTestsAhuacatlEdge, @start, 'outbound') SORT e.what RETURN e.what",
        "FOR e IN NOOPT(EDGES(UnitTestsAhuacatlEdge, @start, 'outbound')) SORT e.what RETURN e.what"
      ];
     
      queries.forEach(function (q) {
        var actual;
        actual = getQueryResults(q, {start: { _id: "UnitTestsAhuacatlVertex/v1" }});
        assertEqual(actual, [ "v1->v2", "v1->v3" ]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks EDGES()
////////////////////////////////////////////////////////////////////////////////

    testEdgesStartVertexIllegal : function () {
      var queries = [
        "FOR e IN NOOPT(V8(EDGES(UnitTestsAhuacatlEdge, @start, 'outbound'))) SORT e.what RETURN e.what",
        "FOR e IN EDGES(UnitTestsAhuacatlEdge, @start, 'outbound') SORT e.what RETURN e.what",
        "FOR e IN NOOPT(EDGES(UnitTestsAhuacatlEdge, @start, 'outbound')) SORT e.what RETURN e.what"
      ];
     
      queries.forEach(function (q) {
        var actual;

        var bindVars = {start: {_id: "v1"}}; // No collection
        assertQueryError(errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, q, bindVars);

        bindVars = {start: "UnitTestTheFuxx/v1"}; // Non existing collection
        assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, q, bindVars);

        bindVars = {start: { id: "UnitTestTheFuxx/v1" } }; // No _id attribute
        assertQueryError(errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, q, bindVars);

        bindVars = {start: [{ id: "UnitTestTheFuxx/v1" }] }; // Error in Array
        actual = getQueryResults(q, bindVars);
        // No Error thrown here
        assertEqual(actual, [ ]);

        bindVars = {start: ["UnitTestTheFuxx/v1"] }; // Error in Array
        actual = getQueryResults(q, bindVars);
        // No Error thrown here
        assertEqual(actual, [ ]);

        bindVars = {start: ["v1"] }; // Error in Array
        actual = getQueryResults(q, bindVars);
        // No Error thrown here
        assertEqual(actual, [ ]);
      });
    }
  };
}

function ahuacatlQueryNeighborsTestSuite () {
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

      vertex.save({ _key: "v1", name: "v1" });
      vertex.save({ _key: "v2", name: "v2" });
      vertex.save({ _key: "v3", name: "v3" });
      vertex.save({ _key: "v4", name: "v4" });
      vertex.save({ _key: "v5", name: "v5" });
      vertex.save({ _key: "v6", name: "v6" });
      vertex.save({ _key: "v7", name: "v7" });

      function makeEdge (from, to) {
        edge.save("UnitTestsAhuacatlVertex/" + from, "UnitTestsAhuacatlVertex/" + to, { what: from + "->" + to, _key: from + "_" + to });
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
/// @brief checks NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testNeighborsAny : function () {
      var actual;
      var v1 = "UnitTestsAhuacatlVertex/v1";
      var v2 = "UnitTestsAhuacatlVertex/v2";
      var v3 = "UnitTestsAhuacatlVertex/v3";
      var v4 = "UnitTestsAhuacatlVertex/v4";
      var v5 = "UnitTestsAhuacatlVertex/v5";
      var v6 = "UnitTestsAhuacatlVertex/v6";
      var v7 = "UnitTestsAhuacatlVertex/v7";
      var v8 = "UnitTestsAhuacatlVertex/v8";
      var theFox = "UnitTestsAhuacatlVertex/thefox";
      var queryStart = "FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, '";
      var queryEnd = "', 'any') SORT n RETURN n";
      var queryEndData = "', 'any', [], {includeData: true}) SORT n RETURN n";
      
      actual = getQueryResults(queryStart + v1 + queryEnd);
      assertEqual(actual, [ v2, v3 ]);

      actual = getQueryResults(queryStart + v2 + queryEnd);
      assertEqual(actual, [ v1, v3, v4 ]);

      // v6 and v7 are neighbors twice
      actual = getQueryResults(queryStart + v3 + queryEnd);
      assertEqual(actual, [ v1, v2, v4, v6, v7 ]);
      
      actual = getQueryResults(queryStart + v8 + queryEnd);
      assertEqual(actual, [ ]);
      
      actual = getQueryResults(queryStart + v5 + queryEnd);
      assertEqual(actual, [ ]);

      actual = getQueryResults(queryStart + theFox + queryEnd);
      assertEqual(actual, [ ]);
      
      assertQueryError(errors.ERROR_GRAPH_INVALID_PARAMETER.code, queryStart + "thefox/thefox" + queryEnd);

      // Including Data
      actual = getRawQueryResults(queryStart + v3 + queryEndData);
      actual = actual.map(function(x) {
        assertTrue(x.hasOwnProperty("_key"), "Neighbor has a _key");
        assertTrue(x.hasOwnProperty("_rev"), "Neighbor has a _rev");
        assertTrue(x.hasOwnProperty("_id"), "Neighbor has a _id");
        assertTrue(x.hasOwnProperty("name"), "Neighbor has a custom attribute");
        return x.name;
      });

      assertEqual(actual, ["v1", "v2", "v4", "v6", "v7"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testNeighborsIn : function () {
      var actual;
      var v1 = "UnitTestsAhuacatlVertex/v1";
      var v2 = "UnitTestsAhuacatlVertex/v2";
      var v3 = "UnitTestsAhuacatlVertex/v3";
      var v4 = "UnitTestsAhuacatlVertex/v4";
      var v5 = "UnitTestsAhuacatlVertex/v5";
      var v6 = "UnitTestsAhuacatlVertex/v6";
      var v7 = "UnitTestsAhuacatlVertex/v7";
      var v8 = "UnitTestsAhuacatlVertex/v8";
      var theFox = "UnitTestsAhuacatlVertex/thefox";
      var queryStart = "FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, '";
      var queryEnd = "', 'inbound') SORT n RETURN n";
      var queryEndData = "', 'inbound', [], {includeData: true}) SORT n RETURN n";
      
      actual = getQueryResults(queryStart + v1 + queryEnd);
      assertEqual(actual, [ ]);

      actual = getQueryResults(queryStart + v2 + queryEnd);
      assertEqual(actual, [v1, v4]);
      
      actual = getQueryResults(queryStart + v3 + queryEnd);
      assertEqual(actual, [v1, v2, v6, v7]);
      
      actual = getQueryResults(queryStart + v8 + queryEnd);
      assertEqual(actual, [ ]);
      
      actual = getQueryResults(queryStart + v5 + queryEnd);
      assertEqual(actual, [ ]);

      actual = getQueryResults(queryStart + theFox + queryEnd);
      assertEqual(actual, [ ]);
      
      assertQueryError(errors.ERROR_GRAPH_INVALID_PARAMETER.code, queryStart + "thefox/thefox" + queryEnd);

      // Inclunding Data
      actual = getRawQueryResults(queryStart + v3 + queryEndData);
      actual = actual.map(function(x) {
        assertTrue(x.hasOwnProperty("_key"), "Neighbor has a _key");
        assertTrue(x.hasOwnProperty("_rev"), "Neighbor has a _rev");
        assertTrue(x.hasOwnProperty("_id"), "Neighbor has a _id");
        assertTrue(x.hasOwnProperty("name"), "Neighbor has a custom attribute");
        return x.name;
      });
      assertEqual(actual, ["v1", "v2", "v6", "v7"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testNeighborsOut : function () {
      var actual;
      var v1 = "UnitTestsAhuacatlVertex/v1";
      var v2 = "UnitTestsAhuacatlVertex/v2";
      var v3 = "UnitTestsAhuacatlVertex/v3";
      var v4 = "UnitTestsAhuacatlVertex/v4";
      var v5 = "UnitTestsAhuacatlVertex/v5";
      var v6 = "UnitTestsAhuacatlVertex/v6";
      var v7 = "UnitTestsAhuacatlVertex/v7";
      var v8 = "UnitTestsAhuacatlVertex/v8";
      var theFox = "UnitTestsAhuacatlVertex/thefox";
      var queryStart = "FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, '";
      var queryEnd = "', 'outbound') SORT n RETURN n";
      var queryEndData = "', 'outbound', [], {includeData: true}) SORT n RETURN n";
      
      actual = getQueryResults(queryStart + v1 + queryEnd);
      assertEqual(actual, [ v2, v3 ]);

      actual = getQueryResults(queryStart + v2 + queryEnd);
      assertEqual(actual, [ v3 ]);
      
      actual = getQueryResults(queryStart + v3 + queryEnd);
      assertEqual(actual, [ v4, v6, v7 ]);
      
      actual = getQueryResults(queryStart + v8 + queryEnd);
      assertEqual(actual, [ ]);

      actual = getQueryResults(queryStart + v5 + queryEnd);
      assertEqual(actual, [ ]);
      
      actual = getQueryResults(queryStart + theFox + queryEnd);
      assertEqual(actual, [ ]);

      assertQueryError(errors.ERROR_GRAPH_INVALID_PARAMETER.code, queryStart + "thefox/thefox" + queryEnd);

      // Inclunding Data
      actual = getRawQueryResults(queryStart + v3 + queryEndData);
      actual = actual.map(function(x) {
        assertTrue(x.hasOwnProperty("_key"), "Neighbor has a _key");
        assertTrue(x.hasOwnProperty("_rev"), "Neighbor has a _rev");
        assertTrue(x.hasOwnProperty("_id"), "Neighbor has a _id");
        assertTrue(x.hasOwnProperty("name"), "Neighbor has a custom attribute");
        return x.name;
      });
      assertEqual(actual, ["v4", "v6", "v7"]);
    },

    testNeighborsEdgeExamples : function () {
      var actual;
      var v3 = "UnitTestsAhuacatlVertex/v3";
      var v4 = "UnitTestsAhuacatlVertex/v4";
      var v6 = "UnitTestsAhuacatlVertex/v6";
      var v7 = "UnitTestsAhuacatlVertex/v7";
      var query = "FOR n IN NEIGHBORS(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, @startId"
                + ", 'outbound', @examples) SORT n RETURN n";

      // An empty array should let all edges through
      actual = getQueryResults(query, {startId: v3, examples: []});
      assertEqual(actual, [ v4, v6, v7 ]);

      // Should be able to handle exactly one object
      actual = getQueryResults(query, {startId: v3, examples: {what: "v3->v4"}});
      assertEqual(actual, [ v4 ]);

      // Should be able to handle a list of a single object
      actual = getQueryResults(query, {startId: v3, examples: [{what: "v3->v4"}]});
      assertEqual(actual, [ v4 ]);

      // Should be able to handle a list of objects
      actual = getQueryResults(query, {startId: v3, examples: [{what: "v3->v4"}, {what: "v3->v6"}]});
      assertEqual(actual, [ v4, v6 ]);

      // Should be able to handle an id as string
      actual = getQueryResults(query, {startId: v3, examples: "UnitTestsAhuacatlEdge/v3_v6"});
      assertEqual(actual, [ v6 ]);

      // Should be able to handle a mix of id and objects
      actual = getQueryResults(query, {startId: v3, examples: ["UnitTestsAhuacatlEdge/v3_v6", {what: "v3->v4"}]});
      assertEqual(actual, [ v4, v6 ]);

      // Should be able to handle internal attributes
      actual = getQueryResults(query, {startId: v3, examples: {_to: v4}});
      assertEqual(actual, [ v4 ]);
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

  var aqlfunctions = require("@arangodb/aql/functions");

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
/// @brief shortest path using dijkstra default config
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraOutbound : function () {
      var config = {
        _sort: true
      };

      var actual = getQueryResults("RETURN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ")", { "@v" : vn, "@e" : en }); 

      assertEqual([
        {
          vertices: [
            vn + "/A",
            vn + "/D",
            vn + "/E",
            vn + "/G",
            vn + "/H"
          ],
          edges: [
            en + "/AD",
            en + "/DE",
            en + "/EG",
            en + "/GH"
          ],
          distance: 4
        }
      ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path using dijkstra with includeData: true
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraOutboundIncludeData : function () {
      var config = {
        _sort: true,
        includeData: true
      };

      var actual = getQueryResults("RETURN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ")", { "@v" : vn, "@e" : en }); 

      assertEqual(actual.length, 1);
      assertTrue(actual[0].hasOwnProperty("vertices"));
      assertTrue(actual[0].hasOwnProperty("edges"));
      var vertices = actual[0].vertices;
      var edges = actual[0].edges;
      assertEqual(vertices.length, edges.length + 1);
      var correct = ["A", "D", "E", "G", "H"];
      for (var i = 0; i < edges.length; ++i) {
        assertEqual(vertices[i]._key, correct[i]);
        assertEqual(vertices[i].name, correct[i]);
        assertEqual(vertices[i + 1].name, correct[i + 1]);
        assertEqual(edges[i]._key, correct[i] + correct[i + 1]);
        assertEqual(edges[i].what, correct[i] + "->" + correct[i + 1]);
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path using dijkstra
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraInbound : function () {
      var config = {
        _sort: true
      };

      var actual = getQueryResults("RETURN SHORTEST_PATH(@@v, @@e, '" + vn + "/H', '" + vn + "/A', 'inbound', " + JSON.stringify(config) + ").vertices", { "@v" : vn, "@e" : en }); 
      // var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

      assertEqual([[ vn + "/H", vn + "/G", vn + "/E", vn + "/D", vn + "/A" ]], actual);
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
        includeData: true,
        _sort: true
      };

      var actual = getQueryResults("LET p = SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") FOR x IN p.vertices[*]._key RETURN x", { "@v" : vn, "@e" : en }); 
      // var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

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
        includeData: true,
        _sort: true
      };

      var actual = getQueryResults("LET p = SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") FOR x IN p.vertices[*]._key RETURN x", { "@v" : vn, "@e" : en }); 
      // var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

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
        includeData: true,
        _sort: true
      };

      var actual = getQueryResults("LET p = SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") FOR x IN p.vertices[*]._key RETURN x", { "@v" : vn, "@e" : en }); 
      // var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

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
        includeData: true,
        _sort: true
      };

      var actual = getQueryResults("LET p = SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") FOR x IN p.vertices[*]._key RETURN x", { "@v" : vn, "@e" : en }); 
      // var actual = getQueryResults("FOR p IN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ") RETURN p.vertex._key", { "@v" : vn, "@e" : en }); 

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
        _sort: true
      };

      var actual = getQueryResults("RETURN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/H', 'outbound', " + JSON.stringify(config) + ")", { "@v" : vn, "@e" : en }); 
      assertEqual([
        {
          vertices: [
            vn + "/A",
            vn + "/D",
            vn + "/E",
            vn + "/G",
            vn + "/H"
          ],
          edges: [
            en + "/AD",
            en + "/DE",
            en + "/EG",
            en + "/GH"
          ],
          distance: 4
        }
      ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path, non-connected vertices
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraNotConnected : function () {
      // this item is not connected to any other
      vertexCollection.save({ _key: "J", name: "J" });

      var config = {
        _sort: true
      };

      var actual = getQueryResults("RETURN SHORTEST_PATH(@@v, @@e, '" + vn + "/A', '" + vn + "/J', 'outbound', " + JSON.stringify(config) + ")", { "@v" : vn, "@e" : en }); 

      assertEqual([ null ], actual);
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
      
  var aqlfunctions = require("@arangodb/aql/functions");

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
/// @brief test suite for Neighbors with intentional failures
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryNeighborsErrorsSuite () {
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
      internal.debugClearFailAt();

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
      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks error handling in NEIGHBORS()
////////////////////////////////////////////////////////////////////////////////

    testNeighborsDitchesOOM : function () {
      var v1 = vn + "/A";
      var v2 = vn + "/B";
      var v3 = vn + "/D";

      var queryStart = "FOR n IN NEIGHBORS(" + vn + " , " + en + ", '";
      var queryEnd = "', 'outbound') SORT n RETURN n";

      var actual = getQueryResults(queryStart + v1 + queryEnd);
      // Positive Check
      assertEqual(actual, [ v2, v3 ]);

      internal.debugClearFailAt();
      internal.debugSetFailAt("EdgeCollectionInfoOOM1");

      // Negative Check
      try {
        actual = getQueryResults(queryStart + v1 + queryEnd);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_DEBUG.code);
      }

      internal.debugClearFailAt();
      internal.debugSetFailAt("EdgeCollectionInfoOOM2");

      // Negative Check
      try {
        actual = getQueryResults(queryStart + v1 + queryEnd);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_DEBUG.code);
      }
      internal.debugClearFailAt();
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ShortestPath with intentional failures
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryShortestpathErrorsSuite () {
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
      internal.debugClearFailAt();

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
      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks error handling in SHORTEST_PATH()
////////////////////////////////////////////////////////////////////////////////

    testShortestPathOOM : function () {
      var s = vn + "/A";
      var m = vn + "/B";
      var t = vn + "/C";

      var query = "RETURN SHORTEST_PATH(" + vn + " , " + en + ", '"
                + s + "', '" + t + "', 'outbound')";

      var actual = getQueryResults(query)[0];
      // Positive Check
      assertEqual(actual.vertices, [s, m, t]);
      assertEqual(actual.distance, 2);

      internal.debugSetFailAt("TraversalOOMInitialize");

      // Negative Check
      try {
        actual = getQueryResults(query);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_DEBUG.code);
      }

      internal.debugClearFailAt();

      // Redo the positive check. Make sure the former fail is gone
      actual = getQueryResults(query)[0];
      assertEqual(actual.vertices, [s, m, t]);
      assertEqual(actual.distance, 2);

      internal.debugSetFailAt("TraversalOOMPath");
      // Negative Check
      try {
        actual = getQueryResults(query);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_DEBUG.code);
      }

    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryEdgesTestSuite);
jsunity.run(ahuacatlQueryNeighborsTestSuite);
jsunity.run(ahuacatlQueryPathsTestSuite);
jsunity.run(ahuacatlQueryShortestPathTestSuite);
jsunity.run(ahuacatlQueryTraversalFilterTestSuite);
jsunity.run(ahuacatlQueryTraversalTestSuite);
jsunity.run(ahuacatlQueryTraversalTreeTestSuite);
if (internal.debugCanUseFailAt() && ! cluster.isCluster()) {
  jsunity.run(ahuacatlQueryNeighborsErrorsSuite);
  jsunity.run(ahuacatlQueryShortestpathErrorsSuite);
}
return jsunity.done();

