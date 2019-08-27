/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Simon Grätzer
// //////////////////////////////////////////////////////////////////////////////

// tests for streaming transactions

var jsunity = require('jsunity');
var internal = require('internal');
var arangodb = require('@arangodb');
var ERRORS = arangodb.errors;
var Graph = require('@arangodb/general-graph');
var db = arangodb.db;

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function transactionGraphSuite () {
  'use strict';
  const graph = 'UnitTestsGraph';
  const vertex = 'UnitTestsVertex';
  const edge = 'UnitTestsEdge';
  let g;

  return {
    setUp: function () {
      if (Graph._exists(graph)) {
        Graph._drop(graph, true);
      }
      g = Graph._create(graph,
        [Graph._relation(edge, vertex, vertex)]
      );
    },

    tearDown: function () {
      db._transactions().forEach(function(trx) {
        try {
          db._createTransaction(trx.id).abort();
        } catch (err) {}
      });
      Graph._drop(graph, true);
    },
    
    // / @brief test: insert vertex
    testVertexInsertUndeclared: function () {
      let trx = db._createTransaction({
        collections: {}
      });

      let headers = { "x-arango-trx-id" : trx.id() };
      let result = arango.POST("/_api/gharial/" + graph + "/vertex/" + vertex, { _key: "test", value: "test" }, headers);
      assertTrue(result.error);
      assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, result.errorNum);
    },
    
    // / @brief test: insert edge
    testEdgeInsertUndeclared: function () {
      let trx = db._createTransaction({
        collections: {}
      });

      let headers = { "x-arango-trx-id" : trx.id() };
      let result = arango.POST("/_api/gharial/" + graph + "/edge/" + edge, { _key: "test", value: "test", _from: vertex + "/1", _to: vertex + "/2" }, headers);
      assertTrue(result.error);
      assertEqual(ERRORS.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code, result.errorNum);
    },

    // / @brief test: insert vertex
    testVertexInsert: function () {
      let trx = db._createTransaction({
        collections: { write: [ vertex ] }
      });

      let headers = { "x-arango-trx-id" : trx.id() };
      let result = arango.POST("/_api/gharial/" + graph + "/vertex/" + vertex, { _key: "test", value: "test" }, headers);
      assertFalse(result.error);
      assertEqual(202, result.code);

      try {
        db._collection(vertex).document("test");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }

      trx.commit();
        
      result = db._collection(vertex).document("test");
      assertEqual("test", result.value);
    },

    // / @brief test: insert edge
    testEdgeInsert: function () {
      let trx = db._createTransaction({
        collections: { write: [ vertex, edge ] }
      });

      let headers = { "x-arango-trx-id" : trx.id() };
      arango.POST("/_api/gharial/" + graph + "/vertex/" + vertex, { _key: "1" }, headers);
      arango.POST("/_api/gharial/" + graph + "/vertex/" + vertex, { _key: "2" }, headers);
      let result = arango.POST("/_api/gharial/" + graph + "/edge/" + edge, { _key: "test", value: "test", _from: vertex + "/1", _to: vertex + "/2" }, headers);
      assertFalse(result.error);
      assertEqual(202, result.code);

      try {
        db._collection(edge).document("test");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }

      trx.commit();
        
      result = db._collection(edge).document("test");
      assertEqual("test", result.value);
      assertEqual(vertex + "/1", result._from);
      assertEqual(vertex + "/2", result._to);
    },

    // / @brief test: remove vertex
    testVertexRemove: function () {
      arango.POST("/_api/gharial/" + graph + "/vertex/" + vertex, { _key: "test", value: "test" });

      let trx = db._createTransaction({
        collections: { write: [ vertex, edge ] }
      });

      let headers = { "x-arango-trx-id" : trx.id() };
      let result = arango.DELETE("/_api/gharial/" + graph + "/vertex/" + vertex + "/test", null, headers);
      assertFalse(result.error);
      assertEqual(202, result.code);

      result = db._collection(vertex).document("test");
      assertEqual("test", result._key);
      assertEqual("test", result.value);
      
      try {
        trx.collection(vertex).document("test");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }

      trx.commit();
        
      try {
        db._collection(vertex).document("test");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }
    },
  };
}

jsunity.run(transactionGraphSuite);

return jsunity.done();
