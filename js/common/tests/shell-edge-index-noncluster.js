/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertNotNull */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the document interface
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;
var internal = require("internal");
var wait = internal.wait;
var ArangoCollection = arangodb.ArangoCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: buckets
////////////////////////////////////////////////////////////////////////////////

function EdgeIndexBucketsSuite () {
  var vn = "UnitTestsCollectionVertex";
  var vertex = null;

  var en1 = "UnitTestsCollectionEdge1";
  var edge1 = null;
  var en2 = "UnitTestsCollectionEdge2";
  var edge2 = null;
  var en3 = "UnitTestsCollectionEdge3";
  var edge3 = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(en1);
      db._drop(en2);
      db._drop(en3);
      edge1 = db._createEdgeCollection(en1, { indexBuckets: 1 });
      edge2 = db._createEdgeCollection(en2, { indexBuckets: 16 });
      edge3 = db._createEdgeCollection(en3, { indexBuckets: 128 });

      db._drop(vn);
      vertex = db._create(vn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      edge1.drop();
      edge2.drop();
      edge3.drop();
      edge1 = null;
      edge2 = null;
      edge3 = null;
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief compare results with different buckets
////////////////////////////////////////////////////////////////////////////////

    testSaveEdgeBuckets : function () {
      var i, j, n = 50;
      for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
          edge1.save(vn + "/v" + i, vn + "/v" + j, { });
          edge2.save(vn + "/v" + i, vn + "/v" + j, { });
          edge3.save(vn + "/v" + i, vn + "/v" + j, { });
        } 
      }

      // unload collections
      edge2.unload();
      edge3.unload();
      edge2 = null;
      edge3 = null;

      internal.wal.flush(true, true);
      while (db._collection(en2).status() !== ArangoCollection.STATUS_UNLOADED) {
        wait(0.5);
      }
      while (db._collection(en3).status() !== ArangoCollection.STATUS_UNLOADED) {
        wait(0.5);
      }

      edge2 = db._collection(en2);
      edge3 = db._collection(en3);

      for (i = 0; i < n; ++i) {
        assertEqual(n, edge1.outEdges(vn + "/v" + i).length);
        assertEqual(n, edge2.outEdges(vn + "/v" + i).length);
        assertEqual(n, edge3.outEdges(vn + "/v" + i).length);

        assertEqual(n, edge1.inEdges(vn + "/v" + i).length);
        assertEqual(n, edge2.inEdges(vn + "/v" + i).length);
        assertEqual(n, edge3.inEdges(vn + "/v" + i).length);

        assertEqual((n * 2) - 1, edge1.edges(vn + "/v" + i).length);
        assertEqual((n * 2) - 1, edge2.edges(vn + "/v" + i).length);
        assertEqual((n * 2) - 1, edge3.edges(vn + "/v" + i).length);
      } 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief compare results with different buckets
////////////////////////////////////////////////////////////////////////////////

    testRemoveEdgeBuckets : function () {
      var i, j, n = 40;
      for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
          edge1.save(vn + "/v" + i, vn + "/v" + j, { _key: i + "-" + j });
          edge2.save(vn + "/v" + i, vn + "/v" + j, { _key: i + "-" + j });
          edge3.save(vn + "/v" + i, vn + "/v" + j, { _key: i + "-" + j });
        } 
      }

      // remove a few random documents
      for (i = 0; i < n * 2; ++i) {
        var key = edge1.any()._key;
        edge1.remove(key);
        edge2.remove(key);
        edge3.remove(key);
      }

      // unload collections
      edge2.unload();
      edge3.unload();
      edge2 = null;
      edge3 = null;

      internal.wal.flush(true, true);
      while (db._collection(en2).status() !== ArangoCollection.STATUS_UNLOADED) {
        wait(0.5);
      }
      while (db._collection(en3).status() !== ArangoCollection.STATUS_UNLOADED) {
        wait(0.5);
      }

      edge2 = db._collection(en2);
      edge3 = db._collection(en3);

      edge1.toArray().forEach(function(doc) {
        var from = doc._from;
        var to = doc._to;
        var ref;

        ref = edge1.outEdges(from).length;
        assertEqual(ref, edge2.outEdges(from).length);
        assertEqual(ref, edge3.outEdges(from).length);

        ref = edge1.inEdges(from).length;
        assertEqual(ref, edge2.inEdges(from).length);
        assertEqual(ref, edge3.inEdges(from).length);
         
        ref = edge1.outEdges(to).length;
        assertEqual(ref, edge2.outEdges(to).length);
        assertEqual(ref, edge3.outEdges(to).length);

        ref = edge1.inEdges(to).length;
        assertEqual(ref, edge2.inEdges(to).length);
        assertEqual(ref, edge3.inEdges(to).length);
      });
    }

  };
}


function EdgeIndexSuite () {
  var vn = "UnitTestsCollectionVertex";
  var vertex = null;

  var en = "UnitTestsCollectionEdge";
  var edge = null;

  var v1 = null;
  var v2 = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(en);
      edge = db._createEdgeCollection(en);

      db._drop(vn);
      vertex = db._create(vn);

      v1 = vertex.save({ a : 1 });
      v2 = vertex.save({ a : 2 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      edge.drop();
      vertex.drop();
      edge = null;
      vertex = null;
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index presence
////////////////////////////////////////////////////////////////////////////////

    testIndexPresence : function () {
      var indexes = edge.getIndexes();
      assertEqual(2, indexes.length);
      assertEqual("edge", indexes[1].type);
      assertEqual([ "_from", "_to" ], indexes[1].fields);
      assertFalse(indexes[1].unique);
      assertFalse(indexes[1].sparse);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityEmpty : function () {
      var edgeIndex = edge.getIndexes()[1];
      assertTrue(edgeIndex.hasOwnProperty("selectivityEstimate"));
      assertEqual(1, edgeIndex.selectivityEstimate);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityOneDoc : function () {
      edge.save(v1, v2, { });
      var edgeIndex = edge.getIndexes()[1];
      assertTrue(edgeIndex.hasOwnProperty("selectivityEstimate"));
      assertEqual(1, edgeIndex.selectivityEstimate);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityDuplicateDocs : function () {
      var i, c, edgeIndex, expectedSelectivity;

      for (i = 0; i < 1000; ++i) {
        edge.save(v1, v2, { });
        edgeIndex = edge.getIndexes()[1];
        expectedSelectivity = 1 / (i + 1);
        // allow for some floating-point deviations
        assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
      }

      var n = edge.count();
      assertEqual(1000, n);

      for (i = 0; i < n; ++i) {
        var doc = edge.any();
        assertNotNull(doc);
        edge.remove(doc._key);

        edgeIndex = edge.getIndexes()[1];
        c = 1000 - (i + 1);
        expectedSelectivity = (c === 0 ? 1 : 1 / c);
        // allow for some floating-point deviations
        assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
      } 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityUniqueDocs : function () {
      for (var i = 0; i < 1000; ++i) {
        edge.save(vn + "/from" + i, vn + "/to" + i, { });
        var edgeIndex = edge.getIndexes()[1];
        assertTrue(1, edgeIndex.selectivityEstimate);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityUniqueDocsFrom : function () {
      for (var i = 0; i < 1000; ++i) {
        edge.save(vn + "/from" + i, vn + "/1", { });
        var edgeIndex = edge.getIndexes()[1];
        var expectedSelectivity = (1 + (1 / (i + 1))) * 0.5; 
        assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityRepeatingDocs : function () {
      for (var i = 0; i < 1000; ++i) {
        if (i > 0) {
          var edgeIndex = edge.getIndexes()[1];
          var expectedSelectivity = (1 + (Math.min(i, 20) / i)) * 0.5; 
          assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
        }
        edge.save(vn + "/from" + (i % 20), vn + "/to" + i, { });
      }
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(EdgeIndexBucketsSuite);
jsunity.run(EdgeIndexSuite);

return jsunity.done();

