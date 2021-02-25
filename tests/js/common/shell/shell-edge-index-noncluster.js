/* jshint globalstrict:false, strict:false */
/* global assertEqual, assertTrue, assertFalse, assertNotNull, fail */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the document interface
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Dr. Frank Celler
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var arangodb = require('@arangodb');
var db = arangodb.db;
var internal = require('internal');
var wait = internal.wait;
var ArangoCollection = arangodb.ArangoCollection;

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: buckets
// //////////////////////////////////////////////////////////////////////////////

function EdgeIndexBucketsSuite () {
  var vn = 'UnitTestsCollectionVertex';
  var vertex = null;

  var en1 = 'UnitTestsCollectionEdge1';
  var edge1 = null;
  var en2 = 'UnitTestsCollectionEdge2';
  var edge2 = null;
  var en3 = 'UnitTestsCollectionEdge3';
  var edge3 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(en1);
      db._drop(en2);
      db._drop(en3);

      var options = {};
      edge1 = db._createEdgeCollection(en1, options);
      edge2 = db._createEdgeCollection(en2, options);
      edge3 = db._createEdgeCollection(en3, options);

      db._drop(vn);
      vertex = db._create(vn);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      edge1.drop();
      edge2.drop();
      edge3.drop();
      edge1 = null;
      edge2 = null;
      edge3 = null;
      vertex.drop();
      wait(0.0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief compare results with different buckets
    // //////////////////////////////////////////////////////////////////////////////

    testSaveEdgeBuckets: function () {
      var i;
      var j;
      var n = 50;
      for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
          edge1.save(vn + '/v' + i, vn + '/v' + j, { });
          edge2.save(vn + '/v' + i, vn + '/v' + j, { });
          edge3.save(vn + '/v' + i, vn + '/v' + j, { });
        }
      }

      // unload collections
      edge2.unload();
      edge3.unload();
      edge2 = null;
      edge3 = null;

      internal.wal.flush(true, true);
      while (db._collection(en2).status() !== ArangoCollection.STATUS_UNLOADED) {
        db._collection(en2).unload();
        wait(0.5);
      }
      while (db._collection(en3).status() !== ArangoCollection.STATUS_UNLOADED) {
        db._collection(en3).unload();
        wait(0.5);
      }

      edge2 = db._collection(en2);
      edge3 = db._collection(en3);

      for (i = 0; i < n; ++i) {
        assertEqual(n, edge1.outEdges(vn + '/v' + i).length);
        assertEqual(n, edge2.outEdges(vn + '/v' + i).length);
        assertEqual(n, edge3.outEdges(vn + '/v' + i).length);

        assertEqual(n, edge1.inEdges(vn + '/v' + i).length);
        assertEqual(n, edge2.inEdges(vn + '/v' + i).length);
        assertEqual(n, edge3.inEdges(vn + '/v' + i).length);

        assertEqual((n * 2) - 1, edge1.edges(vn + '/v' + i).length);
        assertEqual((n * 2) - 1, edge2.edges(vn + '/v' + i).length);
        assertEqual((n * 2) - 1, edge3.edges(vn + '/v' + i).length);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief compare results with different buckets
    // //////////////////////////////////////////////////////////////////////////////

    testRemoveEdgeBuckets: function () {
      var i;
      var j;
      var n = 40;
      for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
          edge1.save(vn + '/v' + i, vn + '/v' + j, { _key: i + '-' + j });
          edge2.save(vn + '/v' + i, vn + '/v' + j, { _key: i + '-' + j });
          edge3.save(vn + '/v' + i, vn + '/v' + j, { _key: i + '-' + j });
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
        db._collection(en2).unload();
        wait(0.5);
      }
      while (db._collection(en3).status() !== ArangoCollection.STATUS_UNLOADED) {
        db._collection(en3).unload();
        wait(0.5);
      }

      edge2 = db._collection(en2);
      edge3 = db._collection(en3);

      edge1.toArray().forEach(function (doc) {
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
  var vn = 'UnitTestsCollectionVertex';
  var vertex = null;

  var en = 'UnitTestsCollectionEdge';
  var edge = null;

  var v1 = null;
  var v2 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(en);
      edge = db._createEdgeCollection(en);

      db._drop(vn);
      vertex = db._create(vn);

      v1 = vertex.save({ a: 1 });
      v2 = vertex.save({ a: 2 });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      edge.drop();
      vertex.drop();
      edge = null;
      vertex = null;
      wait(0.0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test batch size limit
    // //////////////////////////////////////////////////////////////////////////////

    testIndexBatchsizeLimit: function () {
      [20, 900, 1000, 1100, 2000].forEach(function (n) {
        var toKeys = [];
        for (var i = 0; i < n; ++i) {
          var to = 'b' + n + '/' + i;
          edge.insert({
            _from: 'a/' + n,
            _to: to
          });
          toKeys.push(to);
        }

        assertEqual(n, edge.byExample({
          _from: 'a/' + n
        }).toArray().length, 'compare 1');

        assertEqual(n, edge.byExample({
          _from: 'a/' + n
        }).toArray().length, 'compare 2');

        var rv = edge.byExample({ _from: 'a/' + n }).toArray();
        assertEqual(n, rv.length, 'compare 3');

        // assert equal values
        if (n <= 1001) {
          var keys = rv.map(function (x) { return x._to; });
          keys.sort();
          toKeys.sort();
          keys.forEach(function (x, i) {
            assertEqual(x, toKeys[i], 'compare exact values');
          });
        }
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test index presence
    // //////////////////////////////////////////////////////////////////////////////

    testIndexPresence: function () {
      var indexes = edge.getIndexes();
      assertEqual(2, indexes.length);
      assertEqual('edge', indexes[1].type);
      assertEqual([ '_from', '_to' ], indexes[1].fields);
      assertFalse(indexes[1].unique);
      assertFalse(indexes[1].sparse);
    },

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(EdgeIndexSuite);
jsunity.run(EdgeIndexBucketsSuite);

return jsunity.done();
