/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTypeOf, assertNotEqual, assertTrue, assertFalse, assertUndefined, fail */

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
var internal = require("internal");
const testHelper = require("@arangodb/test-helper");

var ArangoCollection = arangodb.ArangoCollection;
var db = arangodb.db;
var ERRORS = arangodb.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CollectionSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief create with id
////////////////////////////////////////////////////////////////////////////////

    testCreateWithId: function () {
      var cn = "example", id = "1234567890";

      db._drop(cn);
      db._drop(id);
      var c1 = db._create(cn, {id: id});

      assertTypeOf("string", c1._id);
      assertEqual(id, c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var c2 = db._collection(cn);

      assertEqual(c1._id, c2._id);
      assertEqual(c1.name(), c2.name());
      assertEqual(c1.status(), c2.status());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with properties (isVolatile is only a valid mmfiles attr)
////////////////////////////////////////////////////////////////////////////////

    testCreatingVolatile: function () {
      var cn = "example";

      db._drop(cn);
      try {
        db._create(cn, {isVolatile: true});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with properties (journalSize is only a valid mmfiles attr)
////////////////////////////////////////////////////////////////////////////////

    testCreatingJournalSize: function () {
      var cn = "example";

      db._drop(cn);
      let c1 = db._create(cn, {journalSize: 4 * 1024 * 1024});
      assertUndefined(c1.journalSize);
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with properties (journalSize is only a valid mmfiles attr)
////////////////////////////////////////////////////////////////////////////////

    testCreatingIndexBuckets: function () {
      var cn = "example";

      db._drop(cn);
      let c1 = db._create(cn, {indexBuckets: 4});
      assertUndefined(c1.indexBuckets);
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test revision id
////////////////////////////////////////////////////////////////////////////////

    testRevision2: function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      var r1 = c1.revision();
      c1.save({_key: "abc"});
      var r2 = c1.revision();
      assertEqual(1, testHelper.compareStringIds(r2, r1));

      c1.save({_key: "123"});
      c1.save({_key: "456"});
      c1.save({_key: "789"});

      var r3 = c1.revision();
      assertEqual(1, testHelper.compareStringIds(r3, r2));

      c1.remove("123");
      var r4 = c1.revision();
      assertEqual(1, testHelper.compareStringIds(r4, r3));

      c1.truncate();
      var r5 = c1.revision();
      assertEqual(-1, testHelper.compareStringIds(r5, r4));

      // unload
      c1.unload();
      c1 = null;
      internal.wait(5);

      // compare rev
      c1 = db._collection(cn);
      var r6 = c1.revision();
      assertEqual(0, testHelper.compareStringIds(r6, r5));

      for (var i = 0; i < 10; ++i) {
        c1.save({_key: "test" + i});
        assertEqual(1, testHelper.compareStringIds(c1.revision(), r6));
        r6 = c1.revision();
      }

      // unload
      c1.unload();
      c1 = null;
      internal.wait(5);

      // compare rev
      c1 = db._collection(cn);
      var r7 = c1.revision();
      assertEqual(0, testHelper.compareStringIds(r7, r6));

      c1.truncate();
      var r8 = c1.revision();

      // unload
      c1.unload();
      c1 = null;
      internal.wait(5);

      // compare rev
      c1 = db._collection(cn);
      var r9 = c1.revision();
      assertEqual(0, testHelper.compareStringIds(r9, r8));

      db._drop(cn);
    },
  }
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
    },

    testCollectionCacheModifyProperties : function () {
      // create collection without cache
      let c = db._create(cn, {cacheEnabled:false});
      let p = c.properties();
      assertFalse(p.cacheEnabled, p);

      // enable caches
      c.properties({cacheEnabled:true});
      p = c.properties();
      assertTrue(p.cacheEnabled, p);

      // disable caches again
      c.properties({cacheEnabled:false});
      p = c.properties();
      assertFalse(p.cacheEnabled, p);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionSuite);
jsunity.run(CollectionCacheSuite);

return jsunity.done();
