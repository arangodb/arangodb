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

var ArangoCollection = arangodb.ArangoCollection;
var db = arangodb.db;
var ERRORS = arangodb.errors;

// copied from lib/Basics/HybridLogicalClock.cpp
var decodeTable = [
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,  //   0 - 15
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,  //  16 - 31
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 0,  -1, -1,  //  32 - 47
    54, 55, 56, 57, 58, 59, 60, 61,
    62, 63, -1, -1, -1, -1, -1, -1,  //  48 - 63
    -1, 2,  3,  4,  5,  6,  7,  8,
    9,  10, 11, 12, 13, 14, 15, 16,  //  64 - 79
    17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, -1, -1, -1, -1, 1,  //  80 - 95
    -1, 28, 29, 30, 31, 32, 33, 34,
    35, 36, 37, 38, 39, 40, 41, 42,  //  96 - 111
    43, 44, 45, 46, 47, 48, 49, 50,
    51, 52, 53, -1, -1, -1, -1, -1,  // 112 - 127
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,  // 128 - 143
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,  // 144 - 159
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,  // 160 - 175
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,  // 176 - 191
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,  // 192 - 207
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,  // 208 - 223
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,  // 224 - 239
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1   // 240 - 255
];

var decode = function(value) {
  var result = 0;
  if (value !== '0') {
    for (var i = 0, n = value.length; i < n; ++i) {
      result = (result * 2 * 2 * 2 * 2 * 2 * 2) + decodeTable[value.charCodeAt(i)];
    }
  }
  return result;
};

var compareStringIds = function (l, r) {
  if (l.length === r.length) {
    // strip common prefix because the accuracy of JS numbers is limited
    var prefixLength = 0;
    for (var i = 0; i < l.length; ++i) {
      if (l[i] !== r[i]) {
        break;
      }
      ++prefixLength;
    }
    if (prefixLength > 0) {
      l = l.substr(prefixLength);
      r = r.substr(prefixLength);
    }
  }

  l = decode(l);
  r = decode(r);
  if (l !== r) {
    return l < r ? -1 : 1;
  }
  return 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function CollectionSuiteErrorHandling () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (too long)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameTooLong : function () {
      try {
        // one char too long
        db._create("a1234567890123456789012345678901234567890123456789012345678901234");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (system name)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameSystem : function () {
      try {
        db._create("1234");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (underscore)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameUnderscore : function () {
      try {
        db._create("_illegal");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (empty)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameEmpty : function () {
      try {
        db._create("");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (number)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameNumber : function () {
      try {
        db._create("12345");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (underscore) (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameUnderscoreShortCut : function () {
      try {
        var a = db._illegal;
        assertUndefined(a);
      }
      catch (err) {
        assertTrue(false);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (empty) (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameEmptyShortCut : function () {
      try {
        var a = db[""];
        assertUndefined(a);
      }
      catch (err) {
        assertTrue(false);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (number) (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameNumberShortCut : function () {
      try {
        var a = db["12345"];
        assertUndefined(a);
      }
      catch (err) {
        assertTrue(false);
      }
    },

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
    },

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

    testCreateWithDoCompact : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, { doCompact: false });

      assertEqual(cn, c1.name());
      assertFalse(c1.properties().doCompact);

      c1.properties({ doCompact: true });
      assertTrue(c1.properties().doCompact);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief indexBuckets
////////////////////////////////////////////////////////////////////////////////

    testCreateWithIndexBuckets : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, { indexBuckets: 4 });

      assertEqual(cn, c1.name());
      assertEqual(4, c1.properties().indexBuckets);

      c1.properties({ indexBuckets: 8 });
      // adjusted number will be stored, but number of index buckets will only
      // take effect when collection is reloaded
      assertEqual(8, c1.properties().indexBuckets);

      db._drop(cn);

      c1 = db._create(cn, { indexBuckets: 6 });

      assertEqual(cn, c1.name());
      assertEqual(6, c1.properties().indexBuckets);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief journalSize
////////////////////////////////////////////////////////////////////////////////

    testCreateWithJournalSize : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, { journalSize: 4 * 1024 * 1024 });

      assertEqual(cn, c1.name());
      assertEqual(4 * 1024 * 1024, c1.properties().journalSize);

      c1.properties({ journalSize: 8 * 1024 * 1024 });
      assertEqual(8 * 1024 * 1024, c1.properties().journalSize);

      c1.properties({ journalSize: 16 * 1024 * 1024 });
      assertEqual(16 * 1024 * 1024, c1.properties().journalSize);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with id
////////////////////////////////////////////////////////////////////////////////

    testCreateWithId : function () {
      var cn = "example", id = "1234567890";

      db._drop(cn);
      db._drop(id);
      var c1 = db._create(cn, { id: id });

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
/// @brief isSystem
////////////////////////////////////////////////////////////////////////////////

    testCreateWithIsSystem : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, { isSystem: true });

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertFalse(c1.properties().isSystem);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief isSystem
////////////////////////////////////////////////////////////////////////////////

    testCreateWithUnderscoreNoIsSystem : function () {
      var cn = "_example";

      db._drop(cn);
      try {
        db._create(cn, { isSystem: false });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief long name
////////////////////////////////////////////////////////////////////////////////

    testCreateLongName : function () {
      var cn = "a123456789012345678901234567890123456789012345678901234567890123";

      db._drop(cn);
      var c1 = db._create(cn);
      assertEqual(cn, c1.name());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read by name
////////////////////////////////////////////////////////////////////////////////

    testReadingByName : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var c2 = db._collection(cn);

      assertEqual(c1._id, c2._id);
      assertEqual(c1.name(), c2.name());
      assertEqual(c1.status(), c2.status());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read by identifier
////////////////////////////////////////////////////////////////////////////////

    testReadingByIdentifier : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var c2 = db._collection(c1._id);

      assertEqual(c1._id, c2._id);
      assertEqual(c1.name(), c2.name());
      assertEqual(c1.status(), c2.status());
      assertEqual(c1.type(), c2.type());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read by name (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testReadingByNameShortCut : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var c2 = db[cn];

      assertEqual(c1._id, c2._id);
      assertEqual(c1.name(), c2.name());
      assertEqual(c1.status(), c2.status());
      assertEqual(c1.type(), c2.type());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read all
////////////////////////////////////////////////////////////////////////////////

    testReadingAll : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      var l = db._collections();

      assertNotEqual(0, l.length);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating document collection
////////////////////////////////////////////////////////////////////////////////

    testCreatingDocumentCollection : function () {
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
      assertEqual(false, p.isVolatile);
      assertEqual(true, p.doCompact);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating edge collection
////////////////////////////////////////////////////////////////////////////////

    testCreatingEdgeCollection : function () {
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
/// @brief check type of document collection after unload
////////////////////////////////////////////////////////////////////////////////

    testTypeDocumentCollection : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._createDocumentCollection(cn);
      c1.unload();
      c1 = null;

      var c2 = db[cn];
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c2.type());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check type of edge collection after unload
////////////////////////////////////////////////////////////////////////////////

    testTypeEdgeCollection : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._createEdgeCollection(cn);
      c1.unload();
      c1 = null;

      var c2 = db[cn];
      assertEqual(ArangoCollection.TYPE_EDGE, c2.type());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with defaults
////////////////////////////////////////////////////////////////////////////////

    testCreatingDefaults : function () {
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
////////////////////////////////////////////////////////////////////////////////

    testCreatingProperties : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, { waitForSync : true, journalSize : 1024 * 1024 });

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
/// @brief creating with properties
////////////////////////////////////////////////////////////////////////////////

    testCreatingProperties2 : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, { isVolatile : true, doCompact: false });

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
/// @brief creating with type
////////////////////////////////////////////////////////////////////////////////

    testCreatingTypeDocument : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, { }, "document");

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with type
////////////////////////////////////////////////////////////////////////////////

    testCreatingTypeEdge : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, { }, "edge");

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertEqual(ArangoCollection.TYPE_EDGE, c1.type());
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with type
////////////////////////////////////////////////////////////////////////////////

    testCreatingTypeInvalid : function () {
      var cn = "example";

      db._drop(cn);
      // invalid type defaults to type "document"
      var c1 = db._create(cn, { }, "foobar");

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop new-born
////////////////////////////////////////////////////////////////////////////////

    testDroppingNewBorn : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      c1.drop();

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());

      var c2 = db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop loaded
////////////////////////////////////////////////////////////////////////////////

    testDroppingLoaded : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      c1.drop();

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());

      var c2 = db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop unloaded
////////////////////////////////////////////////////////////////////////////////

    testDroppingUnloaded : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });
      c1.unload();

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      c1.drop();

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());

      var c2 = db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate new-born
////////////////////////////////////////////////////////////////////////////////

    testTruncatingNewBorn : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      c1.truncate();

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertEqual(0, c1.count());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate loaded
////////////////////////////////////////////////////////////////////////////////

    testTruncatingLoaded : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      c1.truncate();

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertEqual(0, c1.count());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate unloaded
////////////////////////////////////////////////////////////////////////////////

    testTruncatingUnloaded : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });
      c1.unload();

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      c1.truncate();

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertEqual(0, c1.count());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test revision id
////////////////////////////////////////////////////////////////////////////////

    testRevision1 : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      var r1 = c1.revision();
      assertTypeOf("string", r1);

      c1.save({ a : 1 });
      var r2 = c1.revision();
      assertTypeOf("string", r2);
      assertEqual(1, compareStringIds(r2, r1));

      c1.save({ a : 2 });
      var r3 = c1.revision();
      assertTypeOf("string", r3);
      assertEqual(1, compareStringIds(r3, r2));

      // unload
      c1.unload();
      c1 = null;
      internal.wait(5);
      c1 = db._collection(cn);

      var r4 = c1.revision();
      assertTypeOf("string", r4);
      assertEqual(0, compareStringIds(r3, r4));

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test revision id
////////////////////////////////////////////////////////////////////////////////

    testRevision2 : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      var r1 = c1.revision();
      c1.save({ _key: "abc" });
      var r2 = c1.revision();
      assertEqual(1, compareStringIds(r2, r1));

      c1.save({ _key: "123" });
      c1.save({ _key: "456" });
      c1.save({ _key: "789" });

      var r3 = c1.revision();
      assertEqual(1, compareStringIds(r3, r2));

      c1.remove("123");
      var r4 = c1.revision();
      assertEqual(1, compareStringIds(r4, r3));

      c1.truncate();
      var r5 = c1.revision();
      assertEqual(1, compareStringIds(r5, r4));

      // unload
      c1.unload();
      c1 = null;
      internal.wait(5);

      // compare rev
      c1 = db._collection(cn);
      var r6 = c1.revision();
      assertEqual(0, compareStringIds(r6, r5));

      for (var i = 0; i < 10; ++i) {
        c1.save({ _key: "test" + i });
        assertEqual(1, compareStringIds(c1.revision(), r6));
        r6 = c1.revision();
      }

      // unload
      c1.unload();
      c1 = null;
      internal.wait(5);

      // compare rev
      c1 = db._collection(cn);
      var r7 = c1.revision();
      assertEqual(0, compareStringIds(r7, r6));

      db._drop(cn);
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
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CollectionDbSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief drop new-born (DB)
////////////////////////////////////////////////////////////////////////////////

    testCollections : function () {
      var cn1 = "example1";
      var cn2 = "example2";

      db._drop(cn1);
      db._drop(cn2);
      db._createDocumentCollection(cn1);
      db._createEdgeCollection(cn2);

      var collections = db._collections();
      for (var i in collections) {
        if (collections.hasOwnProperty[i]) {
          var c = collections[i];

          assertTypeOf("string", c.name());
          assertTypeOf("number", c.status());
          assertTypeOf("number", c.type());

          if (c.name() === cn1) {
            assertEqual(ArangoCollection.TYPE_DOCUMENT, c.type());
          }
          else if (c.name() === cn2) {
            assertEqual(ArangoCollection.TYPE_EDGE, c.type());
          }
        }
      }
      db._drop(cn1);
      db._drop(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop new-born (DB)
////////////////////////////////////////////////////////////////////////////////

    testDroppingNewBornDB : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      db._drop(cn);

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());

      var c2 = db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop loaded (DB)
////////////////////////////////////////////////////////////////////////////////

    testDroppingLoadedDB : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      db._drop(cn);

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());

      var c2 = db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop unloaded (DB)
////////////////////////////////////////////////////////////////////////////////

    testDroppingUnloadedDB : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });
      c1.unload();

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      db._drop(cn);

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());

      var c2 = db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate new-born (DB)
////////////////////////////////////////////////////////////////////////////////

    testTruncatingNewBornDB : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      db._truncate(cn);

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertEqual(0, c1.count());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate loaded (DB)
////////////////////////////////////////////////////////////////////////////////

    testTruncatingLoadedDB : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      db._truncate(cn);

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertEqual(0, c1.count());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate unloaded (DB)
////////////////////////////////////////////////////////////////////////////////

    testTruncatingUnloadedDB : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });
      c1.unload();

      assertTypeOf("string", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertTypeOf("number", c1.type());

      db._truncate(cn);

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(ArangoCollection.TYPE_DOCUMENT, c1.type());
      assertEqual(0, c1.count());

      db._drop(cn);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionSuiteErrorHandling);
jsunity.run(CollectionSuite);
jsunity.run(CollectionDbSuite);

return jsunity.done();
