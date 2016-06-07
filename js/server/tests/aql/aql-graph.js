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
/// @brief test suite for graph features
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
/// @brief checks edges ANY
////////////////////////////////////////////////////////////////////////////////

    testEdgesAny : function () {
      var q = "FOR v, e IN ANY @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what";

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
     
      actual = getQueryResults(q, {start: "thefox/thefox"});
      assertEqual(actual, [ ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges INBOUND
////////////////////////////////////////////////////////////////////////////////

    testEdgesIn : function () {
      var q = "FOR v, e IN INBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what";
     
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
     
      actual = getQueryResults(q, {start: "thefox/thefox"});
      assertEqual(actual, [ ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges OUTBOUND
////////////////////////////////////////////////////////////////////////////////

    testEdgesOut : function () {
      var q = "FOR v, e IN OUTBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what";
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
     
      actual = getQueryResults(q, {start: "thefox/thefox"});
      assertEqual(actual, [ ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges / vertex combination
////////////////////////////////////////////////////////////////////////////////

    testEdgesAnyInclVertices : function () {
      "use strict";
      let actual;
      var query = "FOR v, e IN ANY @start @@col SORT e.what RETURN v._key";

      let bindVars = {
        "@col": "UnitTestsAhuacatlEdge"
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
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, [ ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges / vertex combination
////////////////////////////////////////////////////////////////////////////////

    testEdgesInInclVertices : function () {
      "use strict";
      let actual;
      let query = "FOR v, e IN INBOUND @start @@col SORT e.what RETURN v._key";

      let bindVars = {
        "@col": "UnitTestsAhuacatlEdge",
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
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, [ ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges / vertex combination
////////////////////////////////////////////////////////////////////////////////

    testEdgesOutInclVertices : function () {
      "use strict";
      let actual;
      let query = "FOR v, e IN OUTBOUND @start @@col SORT e.what RETURN v._key";

      let bindVars = {
        "@col": "UnitTestsAhuacatlEdge",
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
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, [ ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges with filter
////////////////////////////////////////////////////////////////////////////////

    testEdgesFilterExample : function () {
      var q;
      var bindVars = {start: "UnitTestsAhuacatlVertex/v3"};
      var actual;
      q = `FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        FILTER e.what == "v1->v3"
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, [ "v1->v3" ]);

      q = `FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        FILTER e.what == "v1->v3" OR e.what == "v3->v6"
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, [ "v1->v3", "v3->v6"]);

      q = `FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, [ "v1->v3", "v2->v3", "v3->v4", "v3->v6", "v3->v7", "v6->v3", "v7->v3" ]);

      q = `FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        FILTER e.non == "matchable"
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, [ ]);

      q = `FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        FILTER e.what == "v1->v3" OR e.non == "matchable"
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, [ "v1->v3" ]);

      q = `FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        FILTER e.what == "v3->v6"
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, [ "v3->v6" ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges when starting with an array
////////////////////////////////////////////////////////////////////////////////

    testEdgesStartVertexArray : function () {
      var q = "FOR s IN @start FOR v, e IN OUTBOUND s UnitTestsAhuacatlEdge SORT e.what RETURN e.what";
     
      var actual;
      actual = getQueryResults(q, {start: [ "UnitTestsAhuacatlVertex/v1", "UnitTestsAhuacatlVertex/v2" ]});
      assertEqual(actual, [ "v1->v2", "v1->v3", "v2->v3" ]);

      actual = getQueryResults(q, {start: [ {_id: "UnitTestsAhuacatlVertex/v1"}, {_id: "UnitTestsAhuacatlVertex/v2"} ]});
      assertEqual(actual, [ "v1->v2", "v1->v3", "v2->v3" ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges when starting on an object
////////////////////////////////////////////////////////////////////////////////

    testEdgesStartVertexObject : function () {
      var q = "FOR v, e IN OUTBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what";

      var actual;
      actual = getQueryResults(q, {start: { _id: "UnitTestsAhuacatlVertex/v1" }});
      assertEqual(actual, [ "v1->v2", "v1->v3" ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges with illegal start
////////////////////////////////////////////////////////////////////////////////

    testEdgesStartVertexIllegal : function () {
      var q = "FOR v, e IN OUTBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what";
      var qArray = "FOR s IN @start FOR v, e IN OUTBOUND s UnitTestsAhuacatlEdge SORT e.what RETURN e.what";
      var actual;

      var bindVars = {start: {_id: "v1"}}; // No collection
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, [ ]);

      bindVars = {start: "UnitTestTheFuxx/v1"}; // Non existing collection
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, [ ]);

      bindVars = {start: { id: "UnitTestTheFuxx/v1"}};  // No _id attribute
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, [ ]);

      bindVars = {start: [{ id: "UnitTestTheFuxx/v1" }] }; // Error in Array
      actual = getQueryResults(qArray, bindVars);
      // No Error thrown here
      assertEqual(actual, [ ]);

      bindVars = {start: ["UnitTestTheFuxx/v1"] }; // Error in Array
      actual = getQueryResults(qArray, bindVars);
      // No Error thrown here
      assertEqual(actual, [ ]);

      bindVars = {start: ["v1"] }; // Error in Array
      actual = getQueryResults(qArray, bindVars);
      // No Error thrown here
      assertEqual(actual, [ ]);
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
/// @brief checks neighbors
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
      var queryStart = `FOR n IN ANY "`
      var queryEnd = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n._id RETURN n._id`;
      var queryEndData = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n RETURN n`;
      
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
      
      actual = getQueryResults(queryStart + "thefox/thefox" + queryEnd);
      assertEqual(actual, [ ]);

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
/// @brief checks neighbors inbound
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

      var queryStart = `FOR n IN INBOUND "`
      var queryEnd = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n._id RETURN n._id`;
      var queryEndData = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n RETURN n`;
      
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
      
      actual = getQueryResults(queryStart + "thefox/thefox" + queryEnd);
      assertEqual(actual, [ ]);

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
/// @brief checks outbound neighbors
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
      var queryStart = `FOR n IN OUTBOUND "`
      var queryEnd = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n._id RETURN n._id`;
      var queryEndData = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n RETURN n`;
      
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

      actual = getQueryResults(queryStart + "thefox/thefox" + queryEnd);
      assertEqual(actual, [ ]);

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
      var createQuery = function (start, filter) {
        return `FOR n, e IN OUTBOUND "${start}" UnitTestsAhuacatlEdge ${filter} SORT n._id RETURN n._id`;
      };

      // An empty filter should let all edges through
      actual = getQueryResults(createQuery(v3, ""));

      assertEqual(actual, [ v4, v6, v7 ]);

      // Should be able to handle exactly one object
      actual = getQueryResults(createQuery(v3, 'FILTER e.what == "v3->v4"'));
      assertEqual(actual, [ v4 ]);

      // Should be able to handle a list of objects
      actual = getQueryResults(createQuery(v3, 'FILTER e.what == "v3->v4" OR e.what == "v3->v6"'));
      assertEqual(actual, [ v4, v6 ]);

      // Should be able to handle an id as string
      actual = getQueryResults(createQuery(v3, 'FILTER e._id == "UnitTestsAhuacatlEdge/v3_v6"'));
      assertEqual(actual, [ v6 ]);

      // Should be able to handle a mix of id and objects
      actual = getQueryResults(createQuery(v3, 'FILTER e._id == "UnitTestsAhuacatlEdge/v3_v6" OR e.what == "v3->v4"'));
      assertEqual(actual, [ v4, v6 ]);

      // Should be able to handle internal attributes
      actual = getQueryResults(createQuery(v3, `FILTER e._to == "${v4}"`));
      assertEqual(actual, [ v4 ]);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for SHORTEST_PATH 
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(vn);
      db._drop(en);

      vertexCollection = null;
      edgeCollection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path using dijkstra default config
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraOutbound : function () {
      var query = `LET p = (FOR v, e IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/H" ${en} RETURN {v: v._id, e: e._id})
                   LET edges = (FOR e IN p[*].e FILTER e != null RETURN e)
                   LET vertices = p[*].v
                   LET distance = LENGTH(edges)
                   RETURN {edges, vertices, distance}`;
      var actual = getQueryResults(query);

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
      var query = `LET p = (FOR v, e IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/H" ${en} RETURN {v, e})
                   LET edges = (FOR e IN p[*].e FILTER e != null RETURN e)
                   LET vertices = p[*].v
                   LET distance = LENGTH(edges)
                   RETURN {edges, vertices, distance}`;

      var actual = getQueryResults(query);
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
      var query = `FOR v IN INBOUND SHORTEST_PATH "${vn}/H" TO "${vn}/A" ${en} RETURN v._id`;
      var actual = getQueryResults(query);
      assertEqual([ vn + "/H", vn + "/G", vn + "/E", vn + "/D", vn + "/A" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path with custom distance function
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraDistance : function () {
      var query = `FOR v IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/H" ${en} OPTIONS {weightAttribute: "weight"} RETURN v._key`;
      var actual = getQueryResults(query);
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

      var query = `FOR v IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/H" ${en} RETURN v._key`;
      var actual = getQueryResults(query);

      assertEqual(["A","D","E","G","H"], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path, non-connected vertices
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraNotConnected : function () {
      // this item is not connected to any other
      vertexCollection.save({ _key: "J", name: "J" });

      var query = `FOR v IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/J" ${en} RETURN v._key`;
      var actual = getQueryResults(query);

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
/// @brief checks error handling for neighbors
////////////////////////////////////////////////////////////////////////////////

/* CodePath does not exist any more
    testNeighborsDitchesOOM : function () {
      var v1 = vn + "/A";
      var v2 = vn + "/B";
      var v3 = vn + "/D";

      var queryStart = `FOR n IN OUTBOUND "`;
      var queryEnd = `" ${en} SORT n._id RETURN n._id`;

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
*/
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
/// @brief checks error handling in SHORTEST_PATH
////////////////////////////////////////////////////////////////////////////////

    testShortestPathOOM : function () {
      var s = vn + "/A";
      var m = vn + "/B";
      var t = vn + "/C";

      var query = `FOR v IN OUTBOUND SHORTEST_PATH "${s}" TO "${t}" ${en} RETURN v._id`;

      var actual = getQueryResults(query);
      // Positive Check
      assertEqual(actual, [s, m, t]);

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
      actual = getQueryResults(query);
      assertEqual(actual, [s, m, t]);

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
jsunity.run(ahuacatlQueryShortestPathTestSuite);
jsunity.run(ahuacatlQueryTraversalFilterTestSuite);
jsunity.run(ahuacatlQueryTraversalTestSuite);
jsunity.run(ahuacatlQueryTraversalTreeTestSuite);
if (internal.debugCanUseFailAt() && ! cluster.isCluster()) {
  jsunity.run(ahuacatlQueryNeighborsErrorsSuite);
  jsunity.run(ahuacatlQueryShortestpathErrorsSuite);
}
return jsunity.done();

