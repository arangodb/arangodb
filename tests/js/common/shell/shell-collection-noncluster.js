/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertEqual, assertTypeOf, assertNotEqual, fail, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
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
var ArangoCollection = arangodb.ArangoCollection;
var testHelper = require("@arangodb/test-helper").Helper;
const internal = require("internal");
var db = arangodb.db;
var ERRORS = arangodb.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CollectionSuite () {
  'use strict';
  return {

    testShards : function () {
      var cn = "example";

      db._drop(cn);
      var c = db._create(cn);
      try {
        c.shards();
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_NOT_IMPLEMENTED.code, err.errorNum);
      }
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rename loaded collection
////////////////////////////////////////////////////////////////////////////////

    testRenameLoaded : function () {
      var cn = "example";
      var nn = "example2";

      db._drop(cn);
      db._drop(nn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var id = c1._id;

      c1.rename(nn);

      assertEqual(id, c1._id);
      assertEqual(nn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var c2 = db._collection(cn);

      assertEqual(null, c2);

      db._drop(nn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rename unloaded collection
////////////////////////////////////////////////////////////////////////////////

    testRenameUnloaded : function () {
      var cn = "example";
      var nn = "example2";

      db._drop(cn);
      db._drop(nn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });
      c1.unload();

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var id = c1._id;

      c1.rename(nn);

      assertEqual(id, c1._id);
      assertEqual(nn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var c2 = db._collection(cn);

      assertEqual(null, c2);

      db._drop(nn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rename a collection to an already existing collection
////////////////////////////////////////////////////////////////////////////////

    testRenameExisting : function () {
      var cn1 = "example";
      var cn2 = "example2";

      db._drop(cn1);
      db._drop(cn2);
      var c1 = db._create(cn1);
      db._create(cn2);

      try {
        c1.rename(cn2);
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, err.errorNum);
      }
      db._drop(cn1);
      db._drop(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test system collection dropping / renaming / unloading
////////////////////////////////////////////////////////////////////////////////

    testSystemSpecial : function () {
      var cn = "_users";
      var c = db._collection(cn);

      // drop
      try {
        c.drop();
        fail();
      }
      catch (err1) {
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, err1.errorNum);
      }

      // rename
      var cn1 = "example";
      db._drop(cn1);

      try {
        c.rename(cn1);
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, err2.errorNum);
      }

      // unload is allowed
      c.unload();
    },

    testEdgeCacheBehavior : function() {
      var cn = "UnitLoadBehavior123";
      db._drop(cn);
      var c = db._createEdgeCollection(cn);
      try {
        c.load();
        for (let i = 0; i < 10000; i++) {
          c.insert({_from:"c/v"+ (i / 100), _to:"c/v" + i});
          c.insert({_to:"c/v"+ (i / 100), _from:"c/v" + i});
        }

        // check if edge cache is present
        var idxs = c.getIndexes(true);
        assertEqual("edge", idxs[1].type, idxs);

        var initial = [];
        idxs.forEach(function(idx) {
          if (idx.figures.cacheInUse) {
            initial.push(idx.figures);
          } else {
            initial.push(-1);
          }
        });

        c.loadIndexesIntoMemory();

        // checking if edge cache grew
        idxs = c.getIndexes(true);
        idxs.forEach(function(idx, i) {
          if (idx.figures.cacheInUse) {
            assertTrue(idx.figures.cacheSize >= initial[i].cacheSize, idx);
            assertEqual(idx.figures.cacheLifeTimeHitRate, 0, idx);
            initial[i] = idx.figures;
          }
        });

        for (let i = 0; i < 10000; i++) {
          c.outEdges("c/v" + (i / 100));
          c.inEdges("c/v" + (i / 100));
        }
        idxs = c.getIndexes(true);
        // cache was filled with same queries, hit rate must now increase
        idxs.forEach(function(idx, i) {
          if (idx.figures.cacheInUse) {
            let diff = Math.abs(initial[i].cacheSize - idx.figures.cacheSize);
            assertTrue(diff <= Math.pow(2, 21), idx);
            // this assumption is simply not safe
            //assertTrue(idx.figures.cacheLifeTimeHitRate > 15, idx);
            initial[i] = idx.figures;
          }
        });
        for (let i = 0; i < 10000; i++) {
          c.outEdges("c/v" + (i / 100));
          c.inEdges("c/v" + (i / 100));
        }
        idxs = c.getIndexes(true);
        // cache was filled with same queries, hit rate must be higher
        idxs.forEach(function(idx, i) {
          if (idx.figures.cacheInUse) {
            assertTrue(Math.abs(initial[i].cacheSize - idx.figures.cacheSize) < 1024);
            assertTrue(idx.figures.cacheLifeTimeHitRate > initial[i].cacheLifeTimeHitRate, idx, { idx, initial });
          }
        });

        // cache usage should be 0 after unload
        c.unload(); // destorys cache
        idxs = c.getIndexes(true); // loads collection
        idxs.forEach(function(idx, i) {
          if (idx.figures.cacheInUse) {
            assertTrue(idx.figures.cacheSize > 0);
            assertEqual(idx.figures.cacheUsage, 0);
          }
        });

        // lets do some reads
        for (let i = 0; i < 10000; i++) {
          c.outEdges("c/v" + (i / 100));
          c.inEdges("c/v" + (i / 100));
        }
        idxs = c.getIndexes(true);
        // cache was empty, hit rate should be 0
        idxs.forEach(function(idx) {
          if (idx.figures.cacheInUse) {
            assertTrue(idx.figures.cacheSize > 0, idx);
            assertTrue(idx.figures.cacheUsage > 0, idx);
            assertEqual(idx.figures.cacheLifeTimeHitRate, 0, idx);
          }
        });

        for(let i = 0; i < 10000; i++) {
          c.outEdges("c/v" + (i / 100));
          c.inEdges("c/v" + (i / 100));
        }
        idxs = c.getIndexes(true);
        // cache was partially filled same queries, lifetime hit rate
        // should be about 50 %
        // but actual lifetime hit rate is unpredictable due to varying
        // load and memory constraints during tests
        idxs.forEach(function(idx) {
          if (idx.figures.cacheInUse) {
            assertTrue(idx.figures.cacheLifeTimeHitRate > 15, idx);
          }
        });
      } finally {
        db._drop(cn);
      }
    }
  };
}

jsunity.run(CollectionSuite);

return jsunity.done();
