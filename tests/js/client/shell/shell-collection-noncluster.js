/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertEqual, assertTypeOf, assertNotEqual, fail, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const ArangoCollection = arangodb.ArangoCollection;
const waitForEstimatorSync = require('@arangodb/test-helper').waitForEstimatorSync;
const internal = require("internal");
const db = arangodb.db;
const ERRORS = arangodb.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CollectionSuite () {
  'use strict';
  let cn = "example";
  
  return {

    setUp: function() {
      db._drop(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },
    

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

    testCreateWithInvalidIndexes1 : function () {
      // invalid indexes will simply be ignored
      // This ignorance is only in Backwards compatibility with 3.11
      // The commented test below will be used in the future
      db._create(cn, {indexes: [{id: "1", type: "edge", fields: ["_from"]}]});
      let indexes = db[cn].indexes();
      assertEqual(1, indexes.length);
      assertEqual("primary", indexes[0].type);
      /*
      // invalid indexes will be rejected
      try {
        db._create(cn, { indexes: [{ id: "1", type: "edge", fields: ["_from"] }] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
      */
    },
    
    testCreateWithInvalidIndexes2 : function () {
      // invalid indexes will simply be ignored
      // This ignorance is only in Backwards compatibility with 3.11
      // The commented test below will be used in the future
      db._create(cn, { indexes: [{ id: "1234", type: "hash", fields: ["a"] }] });
      let indexes = db[cn].indexes();
      assertEqual(1, indexes.length);
      assertEqual("primary", indexes[0].type);
      /*
      // invalid indexes will be rejected
      try {
        db._create(cn, { indexes: [{ id: "1234", type: "hash", fields: ["a"] }] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
      */
    },

    testTruncateSelectivityEstimates : function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "hash", fields: ["value"] });
      c.ensureIndex({ type: "skiplist", fields: ["value2"] });

      // add enough docs to trigger RangeDelete in truncate
      const docs = [];
      for (let i = 0; i < 35000; ++i) {
        docs.push({value: i % 250, value2: i % 100});
      }
      c.save(docs); 

      c.truncate({ compact: false });
      assertEqual(c.count(), 0);
      assertEqual(c.all().toArray().length, 0);

      // Test Selectivity Estimates
      {
        waitForEstimatorSync();  // make sure estimates are consistent
        let indexes = c.getIndexes(true);
        for (let i of indexes) {
          switch (i.type) {
            case 'primary':
              assertEqual(i.selectivityEstimate, 1);
              break;
            case 'hash':
              assertEqual(i.selectivityEstimate, 1);
              break;
            case 'skiplist':
              assertEqual(i.selectivityEstimate, 1);
              break;
            default:
              fail();
          }
        }
      }
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
      } catch (err2) {
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, err2.errorNum);
      }

      // unload is allowed
      c.unload();
    },

    testEdgeCacheBehavior : function() {
      const cn = "UnitLoadBehavior123";
        
      let tries = 0;
      // allow ourselves to make up to 3 attempts for this test.
      // this is necessary because the edge cache is not 100%
      // deterministic (cache grow requests are executed asynchronously
      // and thus depend on timing)
      while (++tries <= 3) {
        db._drop(cn);
        let c = db._createEdgeCollection(cn);
        try {
          let docs = [];
          for (let i = 0; i < 10000; i++) {
            docs.push({_from: "c/v"+ (i / 100), _to: "c/v" + i});
            docs.push({_to: "c/v"+ (i / 100), _from: "c/v" + i});
            if (docs.length === 1000) {
              c.insert(docs);
              docs = [];
            }
          }

          // check if edge cache is present
          let idxs = c.getIndexes(true);
          assertEqual("edge", idxs[1].type, idxs);

          let initial = [];
          idxs.forEach(function(idx) {
            if (idx.figures.cacheInUse) {
              initial.push(idx.figures);
            } else {
              initial.push(-1);
            }
          });

          c.loadIndexesIntoMemory();
          // loading is async - wait a bit for the caches to be populated
          internal.wait(3);

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
              assertTrue(diff <= Math.pow(2, 23), { diff, initial: initial[i], figures: idx.figures });
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
              assertTrue(idx.figures.cacheLifeTimeHitRate > initial[i].cacheLifeTimeHitRate, { idx, initial });
            }
          });
          // success. exit while loop
          break;
        } catch (err) {
          // ignore errors in round 1 and 2
          if (tries === 3) {
            throw err;
          }
        } finally {
          db._drop(cn);
        }
      }
    }
  };
}

jsunity.run(CollectionSuite);

return jsunity.done();
