/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTypeOf, assertNotEqual, assertTrue, assertFalse, assertNull, assertUndefined, fail */

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
var testHelper = require("@arangodb/test-helper").Helper;

var compareStringIds = function (l, r) {
  var i;
  if (l.length !== r.length) {
    return l.length - r.length < 0 ? -1 : 1;
  }

  // length is equal
  for (i = 0; i < l.length; ++i) {
    if (l[i] !== r[i]) {
      return l[i] < r[i] ? -1 : 1;
    }
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
/// @brief isSystem
////////////////////////////////////////////////////////////////////////////////

    testCreateWithUnderscoreWithIsSystem : function () {
      var cn = "_example";

      db._drop(cn);
      var c1 = db._create(cn, { isSystem: true });
      assertTrue(c1.properties().isSystem);
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
      assertTrue(r1.match(/^[0-9]+$/));

      c1.save({ a : 1 });
      var r2 = c1.revision();
      assertTypeOf("string", r2);
      assertTrue(r2.match(/^[0-9]+$/));
      assertEqual(1, compareStringIds(r2, r1));

      c1.save({ a : 2 });
      var r3 = c1.revision();
      assertTypeOf("string", r3);
      assertTrue(r3.match(/^[0-9]+$/));
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
      assertTrue(r1.match(/^[0-9]+$/));

      c1.save({ _key: "abc" });
      var r2 = c1.revision();
      assertTrue(r2.match(/^[0-9]+$/));
      assertEqual(1, compareStringIds(r2, r1));

      c1.save({ _key: "123" });
      c1.save({ _key: "456" });
      c1.save({ _key: "789" });

      var r3 = c1.revision();
      assertTrue(r3.match(/^[0-9]+$/));
      assertEqual(1, compareStringIds(r3, r2));

      c1.remove("123");
      var r4 = c1.revision();
      assertTrue(r4.match(/^[0-9]+$/));
      assertEqual(1, compareStringIds(r4, r3));

      c1.truncate();
      var r5 = c1.revision();
      assertTrue(r5.match(/^[0-9]+$/));
      assertEqual(1, compareStringIds(r5, r4));

      // unload
      c1.unload();
      c1 = null;
      internal.wait(5);

      // compare rev
      c1 = db._collection(cn);
      var r6 = c1.revision();
      assertTrue(r6.match(/^[0-9]+$/));
      assertEqual(0, compareStringIds(r6, r5));

      for (var i = 0; i < 10; ++i) {
        c1.save({ _key: "test" + i });
        assertTrue(c1.revision().match(/^[0-9]+$/));
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
      assertTrue(r7.match(/^[0-9]+$/));
      assertEqual(0, compareStringIds(r7, r6));

      c1.truncate();
      var r8 = c1.revision();

      // unload
      c1.unload();
      c1 = null;
      internal.wait(5);

      // compare rev
      c1 = db._collection(cn);
      var r9 = c1.revision();
      assertTrue(r9.match(/^[0-9]+$/));
      assertEqual(0, compareStringIds(r9, r8));

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first
////////////////////////////////////////////////////////////////////////////////

    testFirst : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertNull(c1.first());

      c1.save({ "a": 1, "_key" : "one" });
      assertEqual(1, c1.first().a);

      c1.save({ "a": 2, "_key" : "two" });
      assertEqual(1, c1.first().a);

      c1.remove("one");
      assertEqual(2, c1.first().a);

      c1.remove("two");
      assertNull(c1.first());

      c1.truncate();
      for (var i = 0; i < 1000; ++i) {
        c1.save({ "a" : i, "_key" : "test" + i });
      }

      var actual = c1.first(1);
      assertEqual(1, actual.length);
      assertEqual(0, actual[0].a);
      assertEqual("test0", actual[0]._key);

      actual = c1.first(2);
      assertEqual(2, actual.length);
      assertEqual(0, actual[0].a);
      assertEqual(1, actual[1].a);
      assertEqual("test0", actual[0]._key);
      assertEqual("test1", actual[1]._key);

      actual = c1.first(10);
      assertEqual(10, actual.length);
      assertEqual(0, actual[0].a);
      assertEqual(1, actual[1].a);
      assertEqual(9, actual[9].a);
      assertEqual("test0", actual[0]._key);
      assertEqual("test1", actual[1]._key);
      assertEqual("test9", actual[9]._key);

      c1.remove("test0");
      c1.remove("test3");

      actual = c1.first(10);
      assertEqual(10, actual.length);
      assertEqual(1, actual[0].a);
      assertEqual(2, actual[1].a);
      assertEqual(4, actual[2].a);
      assertEqual(11, actual[9].a);
      assertEqual("test1", actual[0]._key);
      assertEqual("test2", actual[1]._key);
      assertEqual("test4", actual[2]._key);
      assertEqual("test11", actual[9]._key);

      c1.truncate();

      actual = c1.first(10);
      assertEqual(0, actual.length);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last
////////////////////////////////////////////////////////////////////////////////

    testLast : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertNull(c1.last());

      c1.save({ "a": 1, "_key" : "one" });
      assertEqual(1, c1.last().a);

      c1.save({ "a": 2, "_key" : "two" });
      assertEqual(2, c1.last().a);

      c1.remove("two");
      assertEqual(1, c1.last().a);

      c1.remove("one");
      assertNull(c1.last());

      c1.truncate();
      for (var i = 0; i < 1000; ++i) {
        c1.save({ "a" : i, "_key" : "test" + i });
      }

      var actual = c1.last(1);
      assertEqual(1, actual.length);
      assertEqual(999, actual[0].a);
      assertEqual("test999", actual[0]._key);

      actual = c1.last(2);
      assertEqual(2, actual.length);
      assertEqual(999, actual[0].a);
      assertEqual(998, actual[1].a);
      assertEqual("test999", actual[0]._key);
      assertEqual("test998", actual[1]._key);

      actual = c1.last(10);
      assertEqual(10, actual.length);
      assertEqual(999, actual[0].a);
      assertEqual(998, actual[1].a);
      assertEqual(990, actual[9].a);
      assertEqual("test999", actual[0]._key);
      assertEqual("test998", actual[1]._key);
      assertEqual("test990", actual[9]._key);

      c1.remove("test999");
      c1.remove("test996");

      actual = c1.last(10);
      assertEqual(10, actual.length);
      assertEqual(998, actual[0].a);
      assertEqual(997, actual[1].a);
      assertEqual(995, actual[2].a);
      assertEqual(988, actual[9].a);
      assertEqual("test998", actual[0]._key);
      assertEqual("test997", actual[1]._key);
      assertEqual("test995", actual[2]._key);
      assertEqual("test988", actual[9]._key);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first / last after reload
////////////////////////////////////////////////////////////////////////////////

    testFirstLastAfterReload : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      for (var i = 0; i < 1000; ++i) {
        c1.save({ _key : "test" + i, "value" : i });
      }

      testHelper.waitUnload(c1);
      c1 = null;

      c1 = db._collection(cn);

      for (i = 1000; i < 2000; ++i) {
        c1.save({ _key : "test" + i, "value" : i });
      }

      var actual = c1.first(10);
      assertEqual(10, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(9, actual[9].value);
      assertEqual("test0", actual[0]._key);
      assertEqual("test1", actual[1]._key);
      assertEqual("test9", actual[9]._key);

      actual = c1.last(10);
      assertEqual(10, actual.length);
      assertEqual(1999, actual[0].value);
      assertEqual(1998, actual[1].value);
      assertEqual(1990, actual[9].value);
      assertEqual("test1999", actual[0]._key);
      assertEqual("test1998", actual[1]._key);
      assertEqual("test1990", actual[9]._key);

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

