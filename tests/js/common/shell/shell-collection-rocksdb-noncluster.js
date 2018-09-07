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
        assertEqual(ERRORS.ERROR_INTERNAL.code, err.errorNum);
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
/// @brief test checksum
////////////////////////////////////////////////////////////////////////////////

    testChecksum : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      // empty collection, checksum should be 0
      var r1 = c1.checksum(true);
      assertTypeOf("string", r1.revision);
      assertTrue(r1.revision !== "");
      assertTypeOf("string", r1.checksum);
      assertEqual("0", r1.checksum);

      // inserting a doc, checksum should change
      c1.save({ a : 1 });
      var r2 = c1.checksum(true);
      assertNotEqual(r1.revision, r2.revision);
      assertTypeOf("string", r2.revision);
      assertTrue(r2.revision !== "");
      assertTypeOf("string", r2.checksum);
      assertNotEqual("0", r2.checksum);

      // inserting another doc, checksum should change
      c1.save({ a : 2 });
      var r3 = c1.checksum(true);
      assertNotEqual(r1.revision, r3.revision);
      assertNotEqual(r2.revision, r3.revision);
      assertTypeOf("string", r3.revision);
      assertTrue(r3.revision !== "");
      assertTypeOf("string", r3.checksum);
      assertNotEqual("0", r3.checksum);
      assertNotEqual(r2.checksum, r3.checksum);

      // test after unloading
      c1.unload();
      var r4 = c1.checksum(true);
      assertTypeOf("string", r4.revision);
      assertEqual(r3.revision, r4.revision);
      assertTypeOf("string", r4.checksum);
      assertNotEqual("0", r4.checksum);
      assertEqual(r3.checksum, r4.checksum);

      // test withData
      var r5 = c1.checksum(true, true);
      assertTypeOf("string", r5.revision);
      assertEqual(r4.revision, r5.revision);
      assertTypeOf("string", r5.checksum);
      assertNotEqual("0", r5.checksum);
      assertNotEqual(r4.checksum, r5.checksum);

      // test after truncation
      c1.truncate();
      var r6 = c1.checksum(true);
      assertTypeOf("string", r6.revision);
      assertTypeOf("string", r6.checksum);
      assertEqual("0", r6.checksum);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test checksum
////////////////////////////////////////////////////////////////////////////////

    testChecksumEdge : function () {
      var cn = "example";
      var vn = "example2";

      db._drop(cn);
      db._drop(vn);
      db._create(vn);
      var c1 = db._createEdgeCollection(cn);

      var r1 = c1.checksum(true);
      assertTypeOf("string", r1.revision);
      assertTrue(r1.revision !== "");
      assertTypeOf("string", r1.checksum);
      assertEqual("0", r1.checksum);

      c1.save(vn + "/1", vn + "/2", { a : 1 });
      var r2 = c1.checksum(true);
      assertNotEqual(r1.revision, r2.revision);
      assertTypeOf("string", r2.revision);
      assertTrue(r2.revision !== "");
      assertTypeOf("string", r2.checksum);
      assertNotEqual("0", r2.checksum);

      c1.save(vn + "/1", vn + "/2", { a : 2 });
      var r3 = c1.checksum(true);
      assertNotEqual(r1.revision, r3.revision);
      assertNotEqual(r2.revision, r3.revision);
      assertTypeOf("string", r3.revision);
      assertTrue(r3.revision !== "");
      assertTypeOf("string", r3.checksum);
      assertNotEqual("0", r3.checksum);
      assertNotEqual(r2.checksum, r3.checksum);

      c1.unload();
      var r4 = c1.checksum(true);
      assertTypeOf("string", r4.revision);
      assertEqual(r3.revision, r4.revision);
      assertTypeOf("string", r4.checksum);
      assertEqual(r3.checksum, r4.checksum);

      // test withData
      var r5 = c1.checksum(true, true);
      assertTypeOf("string", r5.revision);
      assertEqual(r4.revision, r5.revision);
      assertTypeOf("string", r5.checksum);
      assertNotEqual("0", r5.checksum);
      assertNotEqual(r4.checksum, r5.checksum);

      // test after truncation
      c1.truncate();
      var r6 = c1.checksum(true);
      assertTypeOf("string", r6.revision);
      assertTypeOf("string", r6.checksum);
      assertEqual("0", r6.checksum);

      db._drop(cn);
      db._drop(vn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test checksum two different collections
////////////////////////////////////////////////////////////////////////////////

    testChecksumDifferent : function () {
      var cn1 = "example";
      var cn2 = "example2";

      db._drop(cn1);
      db._drop(cn2);
      var c1 = db._create(cn1);
      var c2 = db._create(cn2);

      // collections are empty, checksums are identical
      var cs1 = c1.checksum().checksum;
      var cs2 = c2.checksum().checksum;

      assertEqual(cs1, cs2);

      c1.save({ _key: "foobar", value: 123 });
      c2.save({ _key: "foobar", value: 123 });

      // keys are the same
      cs1 = c1.checksum().checksum;
      cs2 = c2.checksum().checksum;

      assertEqual(cs1, cs2);

      // data is the same
      cs1 = c1.checksum(false, true).checksum;
      cs2 = c2.checksum(false, true).checksum;

      assertEqual(cs1, cs2);

      // revisions are different
      cs1 = c1.checksum(true, false).checksum;
      cs2 = c2.checksum(true, false).checksum;

      assertNotEqual(cs1, cs2);

      // revisions are still different
      cs1 = c1.checksum(true, true).checksum;
      cs2 = c2.checksum(true, true).checksum;

      assertNotEqual(cs1, cs2);

      // update document in c1, keep data
      c1.replace("foobar", { value: 123 });

      // keys are still the same
      cs1 = c1.checksum().checksum;
      cs2 = c2.checksum().checksum;

      assertEqual(cs1, cs2);

      // data is still the same
      cs1 = c1.checksum(false, true).checksum;
      cs2 = c2.checksum(false, true).checksum;

      assertEqual(cs1, cs2);

      // revisions are still different
      cs1 = c1.checksum(true, false).checksum;
      cs2 = c2.checksum(true, false).checksum;

      // update document in c1, changing data
      c1.replace("foobar", { value: 124 });

      // keys are still the same
      cs1 = c1.checksum().checksum;
      cs2 = c2.checksum().checksum;

      assertEqual(cs1, cs2);

      // data is not the same
      cs1 = c1.checksum(false, true).checksum;
      cs2 = c2.checksum(false, true).checksum;

      assertNotEqual(cs1, cs2);

      // revisions are still different
      cs1 = c1.checksum(true, false).checksum;
      cs2 = c2.checksum(true, false).checksum;

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

    testEdgeCacheBehaviour : function() {

      var cn = "UnitLoadBehaviour123";
      db._drop(cn);
      var c = db._createEdgeCollection(cn);
      c.load();
      for(let i=0;i<10000;i++) {
        c.insert({_from:"c/v"+(i/100), _to:"c/v"+i});
        c.insert({_to:"c/v"+(i/100), _from:"c/v"+i});
      }

      // check if edge cache is present
      var idxs = c.getIndexes(true);
      assertEqual("edge", idxs[1].type, idxs);

      var inital = [];
      idxs.forEach(function(idx) {
        if (idx.figures.cacheInUse) {
          inital.push(idx.figures);
        } else {
          inital.push(-1);
        }
      });

      c.loadIndexesIntoMemory();

      // checking if edge cach grew
      idxs = c.getIndexes(true);
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertTrue(idx.figures.cacheSize > inital[i].cacheSize, idx);
          assertEqual(idx.figures.cacheLifeTimeHitRate, 0, idx);
          inital[i] = idx.figures;
        }
      });

      for(let i=0;i<10000;i++) {
        c.outEdges("c/v"+(i/100));
        c.inEdges("c/v"+(i/100));
      }
      idxs = c.getIndexes(true);
      // cache was filled same queries, hit rate must be about 
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          let diff = Math.abs(inital[i].cacheSize - idx.figures.cacheSize);
          assertTrue(diff <= Math.pow(2, 21), idx);
          assertTrue(idx.figures.cacheLifeTimeHitRate > 15, idx);
          inital[i] = idx.figures;
        }
      });
      for(let i = 0; i < 10000; i++) {
        c.outEdges("c/v" + (i / 100));
        c.inEdges("c/v" + (i / 100));
      }
      idxs = c.getIndexes(true);
      // cache was filled with same queries, hit rate must be higher
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertTrue(Math.abs(inital[i].cacheSize - idx.figures.cacheSize) < 1024);
          assertTrue(idx.figures.cacheLifeTimeHitRate > 30, idx);
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
      for(let i = 0; i < 10000; i++) {
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
      idxs.forEach(function(idx) {
        if (idx.figures.cacheInUse) {
          assertTrue(idx.figures.cacheLifeTimeHitRate > 15, idx);
        }
      });

      db._drop(cn);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection caches
////////////////////////////////////////////////////////////////////////////////

function CollectionCacheSuite () {
  const cn = "UnitTestsClusterCache";
  return {

    tearDown : function () {
      try {
        db._drop(cn);
      }
      catch (err) {
      }
    },
    
    testCollectionCache : function () {
      let c = db._create(cn, {cacheEnabled:true});
      let p = c.properties();
      assertTrue(p.cacheEnabled, p);

      let f = c.figures();
      assertTrue(f.cacheInUse, f);
      assertTrue(f.cacheSize > 0);
      assertEqual(f.cacheUsage, 0);
      assertEqual(f.cacheLifeTimeHitRate, 0);

      let idxs = c.getIndexes(true);
      idxs.forEach(function(idx) {
        if (idx.type === 'primary') {
          assertTrue(idx.figures.cacheInUse, idx);
          assertEqual(idx.figures.cacheUsage, 0);
        }
      });
    },

    testCollectionCacheModifyProperties : function () {
      // create collection without cache
      let c = db._create(cn, {cacheEnabled:false});
      let p = c.properties();
      assertFalse(p.cacheEnabled, p);
      let f = c.figures();
      assertFalse(f.cacheInUse, f);
      let idxs = c.getIndexes(true);
      idxs.forEach(function(idx) {
        if (idx.type === 'primary') {
          assertFalse(idx.figures.cacheInUse);
          assertEqual(idx.figures.cacheUsage, 0);
        }
      });

      // enable caches
      c.properties({cacheEnabled:true});
      p = c.properties();
      assertTrue(p.cacheEnabled, p);
      f = c.figures();
      assertTrue(f.cacheInUse, f);
      assertEqual(f.cacheLifeTimeHitRate, 0);
      idxs = c.getIndexes(true);
      idxs.forEach(function(idx) {
        if (idx.type === 'primary') {
          assertTrue(idx.figures.cacheInUse);
          assertEqual(idx.figures.cacheUsage, 0);
        }
      });

      // disable caches again
      c.properties({cacheEnabled:false});
      p = c.properties();
      assertFalse(p.cacheEnabled, p);
      f = c.figures();
      assertFalse(f.cacheInUse, f);
      idxs = c.getIndexes(true);
      idxs.forEach(function(idx) {
        if (idx.type === 'primary') {
          assertFalse(idx.figures.cacheInUse);
          assertEqual(idx.figures.cacheUsage, 0);
        }
      });
    },

    testCollectionCacheBehaviour : function () {
      let c = db._create(cn, {cacheEnabled:true});
      let p = c.properties();
      assertTrue(p.cacheEnabled, p);

      for(let i = 0; i < 10000; i++) {
        c.insert({_key:String(i), value : i});
      }

      let f = c.figures();
      assertTrue(f.cacheInUse, f);
      assertTrue(f.cacheSize > 0);
      assertEqual(f.cacheUsage, 0);
      assertEqual(f.cacheLifeTimeHitRate, 0);

      let idxs = c.getIndexes(true);
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertTrue(idx.figures.cacheSize > 0);
          assertEqual(idx.figures.cacheUsage, 0);
          assertEqual(idx.figures.cacheLifeTimeHitRate, 0);
        }
      });

      // hit 25% 
      for(let i = 0; i < 2500; i++) {
        let doc = c.document(String(i));
        assertTrue(doc !== undefined);
      }

      f = c.figures();
      assertTrue(f.cacheInUse, f);
      assertTrue(f.cacheSize > 0);
      assertTrue(f.cacheUsage > 0);
      assertEqual(f.cacheLifeTimeHitRate, 0);

      idxs = c.getIndexes(true);
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertTrue(idx.figures.cacheSize > 0);
          assertTrue(idx.figures.cacheUsage > 0);
          assertEqual(idx.figures.cacheLifeTimeHitRate, 0);
        }
      });

      // hit same 25% 
      for(let i = 0; i < 2500; i++) {
        let doc = c.document(String(i));
        assertTrue(doc !== undefined);
      }

      // no new documents were hit
      let f2 = c.figures();
      assertTrue(f2.cacheInUse);
      // anyway, the cache usage may be different, because there is
      // no guarantee any document makes it into the cache
      assertEqual(f.cacheSize, f2.cacheSize);
      /*
      assertTrue(Math.abs(f.cacheUsage - f2.cacheUsage) < 2048, 
                 Math.abs(f.cacheUsage - f2.cacheUsage));
      // first no hits, second time all hits ~= 50% hit-rate
      assertTrue(Math.abs(f2.cacheLifeTimeHitRate - 50) < 11, f2.cacheLifeTimeHitRate);
      */
      let idxs2 = c.getIndexes(true);
      idxs2.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertEqual(idx.figures.cacheSize, idxs[i].figures.cacheSize);
          assertTrue(Math.abs(idx.figures.cacheUsage - idxs[i].figures.cacheUsage) < 2048, 
                     Math.abs(idx.figures.cacheUsage - idxs[i].figures.cacheUsage));
          assertTrue(Math.abs(idx.figures.cacheLifeTimeHitRate - 50) < 11);
        }
      });

      // hit different 25% 
      for(let i = 5000; i < 7500; i++) {
        let doc = c.document(String(i));
        assertTrue(doc !== undefined);
      }

      let f3 = c.figures();
      assertTrue(f3.cacheInUse);
      assertTrue(f2.cacheSize <= f3.cacheSize);
      assertTrue(f2.cacheUsage < f3.cacheUsage);
      assertTrue(f3.cacheLifeTimeHitRate < f2.cacheLifeTimeHitRate);

      idxs = c.getIndexes(true);
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertTrue(idxs2[i].figures.cacheSize < idx.figures.cacheSize);
          assertTrue(idxs2[i].figures.cacheUsage < idx.figures.cacheUsage);
          assertTrue(idx.figures.cacheLifeTimeHitRate < idxs2[i].figures.cacheLifeTimeHitRate);
        }
      });

      c.unload(); // should destroy cache
      f = c.figures(); // will load collection
      assertTrue(f.cacheSize > 0);
      assertEqual(f.cacheUsage, 0);
      assertEqual(f.cacheLifeTimeHitRate, 0);

      idxs = c.getIndexes(true);
      idxs.forEach(function(idx, i) {
        if (idx.figures.cacheInUse) {
          assertTrue(idx.figures.cacheSize > 0);
          assertEqual(idx.figures.cacheUsage, 0);
          assertEqual(idx.figures.cacheLifeTimeHitRate, 0);
        }
      });
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionSuite);
jsunity.run(CollectionCacheSuite);

return jsunity.done();
