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
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function CollectionSuiteErrorHandling () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with too small journal size
////////////////////////////////////////////////////////////////////////////////

    testErrorInvalidJournalSize : function () {
      var cn = "example";

      db._drop(cn);
      try {
        db._create(cn, { waitForSync : false, journalSize : 1024 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CollectionSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief indexBuckets
////////////////////////////////////////////////////////////////////////////////

    testCreateWithDoCompact: function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, {doCompact: false});

      assertEqual(cn, c1.name());
      assertFalse(c1.properties().doCompact);

      c1.properties({doCompact: true});
      assertTrue(c1.properties().doCompact);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief indexBuckets (indexBuckets are mmfiles only)
////////////////////////////////////////////////////////////////////////////////

    testCreateWithIndexBuckets: function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, {indexBuckets: 4});

      assertEqual(cn, c1.name());
      assertEqual(4, c1.properties().indexBuckets);

      c1.properties({indexBuckets: 8});
      // adjusted number will be stored, but number of index buckets will only
      // take effect when collection is reloaded
      assertEqual(8, c1.properties().indexBuckets);

      db._drop(cn);

      c1 = db._create(cn, {indexBuckets: 6});

      assertEqual(cn, c1.name());
      assertEqual(6, c1.properties().indexBuckets);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief journalSize
////////////////////////////////////////////////////////////////////////////////

    testCreateWithJournalSize: function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, {journalSize: 4 * 1024 * 1024});

      assertEqual(cn, c1.name());
      assertEqual(4 * 1024 * 1024, c1.properties().journalSize);

      c1.properties({journalSize: 8 * 1024 * 1024});
      assertEqual(8 * 1024 * 1024, c1.properties().journalSize);

      c1.properties({journalSize: 16 * 1024 * 1024});
      assertEqual(16 * 1024 * 1024, c1.properties().journalSize);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating document collection (checking isVolatile and doCompact)
////////////////////////////////////////////////////////////////////////////////

    testCreatingDocumentCollection: function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._createDocumentCollection(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var p = c1.properties();

      assertEqual(false, p.waitForSync);
      assertEqual(false, p.isVolatile); // mmfiles related
      assertEqual(true, p.doCompact); // mmfiles related

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating edge collection (checking isVolatile and doCompact)
////////////////////////////////////////////////////////////////////////////////

    testCreatingEdgeCollection: function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._createEdgeCollection(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_EDGE, c1.type());
      assertTypeOf("number", c1.type());

      var p = c1.properties();

      assertEqual(false, p.waitForSync);
      assertEqual(false, p.isVolatile);
      assertEqual(true, p.doCompact);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with defaults testTypeDocumentCollection
// (checking isVolatile and doCompact)
////////////////////////////////////////////////////////////////////////////////

    testCreatingDefaults: function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var p = c1.properties();

      assertEqual(false, p.waitForSync);
      assertEqual(false, p.isVolatile);
      /* assertEqual(1048576, p.journalSize); */
      assertEqual(true, p.doCompact);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with properties
// (checking isVolatile, doCompact and journalSize)
////////////////////////////////////////////////////////////////////////////////

    testCreatingProperties: function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, {waitForSync: true, journalSize: 1024 * 1024});

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var p = c1.properties();

      assertEqual(true, p.waitForSync);
      assertEqual(false, p.isVolatile);
      assertEqual(1048576, p.journalSize);
      assertEqual(true, p.doCompact);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with properties (checking isVolatile and doCompact)
////////////////////////////////////////////////////////////////////////////////

    testCreatingProperties2: function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, {isVolatile: true, doCompact: false});

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var p = c1.properties();

      assertEqual(true, p.isVolatile);
      /* assertEqual(1048576, p.journalSize); */
      assertEqual(false, p.doCompact);
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test revision id (with mmfiles related compact)
////////////////////////////////////////////////////////////////////////////////

    testRevision: function () {
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

      c1.truncate({compact: false});
      var r5 = c1.revision();
      assertEqual(1, testHelper.compareStringIds(r5, r4));

      // unload
      c1.unload();
      c1 = null;
      internal.wait(5);

      // compare rev
      c1 = db._collection(cn);
      var r6 = c1.revision();

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

      db._drop(cn);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionSuiteErrorHandling);
jsunity.run(CollectionSuite);

return jsunity.done();
