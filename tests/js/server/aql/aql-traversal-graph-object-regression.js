/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
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
/// @author Markus Pfeiffer
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db, indexId;

const vertexCollectionName = "person";
const edgeCollectionName = "link";

var cleanup = function() {
  db._drop(vertexCollectionName);
  db._drop(edgeCollectionName);
};

var createBaseGraph = function () {
  const vc = db._create(vertexCollectionName);
  const ec = db._createEdgeCollection(edgeCollectionName);

  vc.save([{"_key":"287057244","name":"Gerald","age":18},
           {"_key":"287056982","name":"Chris","age":34},
           {"_key":"287056986","name":"Minerva","age":54},
           {"_key":"287056980","name":"George","age":53},
           {"_key":"287056984","name":"Ella","age":65},
           {"_key":"287056990","name":"July","age":42},
           {"_key":"287056976","name":"Peter","age":5},
           {"_key":"287056988","name":"Kay","age":4}]);

  ec.save([{"_key":"287057504","_from":"person/287056980","_to":"person/287056988","field":"dogs"},
           {"_key":"287057440","_from":"person/287057244","_to":"person/287056984","field":"home"},
           {"_key":"287057500","_from":"person/287056986","_to":"person/287056976","field":"dogs"},
           {"_key":"287057444","_from":"person/287056982","_to":"person/287056990","field":"home"}]);
};


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function pathTraversalObjectRegressionSuite() {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      cleanup();
      createBaseGraph();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      cleanup();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test object access for path object
////////////////////////////////////////////////////////////////////////////////

    testPathObjectEdgesAccessPrefixNotOK : function () {
      const query = `FOR person IN @@person
                       FOR v, e, p IN 1..1 OUTBOUND person @@links
                         FILTER p.edge[0].field == "dogs"
                         RETURN {
                           field: p.edge[0].field,
                           cond: p.edge[0].field == "dogs"
                         }`;

      var actual = db._query(query, {'@person': vertexCollectionName,
                                     '@links': edgeCollectionName});
      assertEqual(actual.toArray(), []);
    },

    testPathObjectVertexAccessPrefixNotOK : function () {
      const query = `FOR person IN @@person
                       FOR v, e, p IN 1..1 OUTBOUND person @@links
                         FILTER p.verti[0].name == "Gerald"
                         RETURN {
                           name: p.verti[0].name,
                           cond: p.verti[0].name == "Gerald"
                         }`;

      var actual = db._query(query, {'@person': vertexCollectionName,
                                     '@links': edgeCollectionName});
      assertEqual(actual.toArray(), []);
    },

    testPathObjectEdgesAccessOK : function () {
      const query = `FOR person IN @@person
                       FOR v, e, p IN 1..1 OUTBOUND person @@links
                         FILTER p.edges[0].field == "dogs"
                         RETURN {
                           field: p.edges[0].field,
                           cond: p.edges[0].field == "dogs"
                         }`;

      var actual = db._query(query, {'@person': vertexCollectionName,
                                     '@links': edgeCollectionName});
      assertEqual(actual.toArray(), [{field: "dogs", cond: true},
                                     {field: "dogs", cond: true}]);
    },

    testPathObjectVertexAccessOK : function () {
      const query = `FOR person IN @@person
                       FOR v, e, p IN 1..1 OUTBOUND person @@links
                         FILTER p.vertices[0].name == "Gerald"
                         RETURN {
                           name: p.vertices[0].name,
                           cond: p.vertices[0].name == "Gerald"
                         }`;

      var actual = db._query(query, {'@person': vertexCollectionName,
                                     '@links': edgeCollectionName});
      assertEqual(actual.toArray(), [{name: "Gerald", cond: true}]);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(pathTraversalObjectRegressionSuite);

return jsunity.done();

