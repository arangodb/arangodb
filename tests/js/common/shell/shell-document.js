/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global fail, assertTrue, assertFalse, assertEqual, assertNotEqual, assertTypeOf */

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
var ERRORS = arangodb.errors;
var db = arangodb.db;
var internal = require("internal");
var wait = internal.wait;
var testHelper = require("@arangodb/test-helper").Helper;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function CollectionDocumentSuiteErrorHandling () {
  'use strict';
  var cn = "UnitTestsCollectionBasics";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      collection = db._create(cn, { waitForSync : false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (collection) {
        collection.unload();
        collection.drop();
        collection = null;
      }
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleDatabase : function () {
      try {
        db._document("123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleCollection : function () {
      try {
        collection.document("");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle replace
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleReplace : function () {
      try {
        db._replace("123456  ", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle delete
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleDelete : function () {
      try {
        collection.remove("123/45/6");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown document identifier
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingUnknownDocument: function () {
      try {
        collection.document(collection.name() + "/123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cross collection
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingCrossCollection : function () {
      try {
        collection.document("test123456/123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cross collection replace
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingCrossCollectionReplace : function () {
      try {
        collection.replace("test123456/123456", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cross collection delete
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingCrossCollectionDelete : function () {
      try {
        collection.remove("test123456/123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code, err.errorNum);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function CollectionDocumentSuite () {
  'use strict';
  var ERRORS = require("internal").errors;

  var cn = "UnitTestsCollectionBasics";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      collection = db._create(cn, { waitForSync: false });

      collection.load();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      collection = null;
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ invalid type
////////////////////////////////////////////////////////////////////////////////

    testSaveInvalidDocumentType : function () {
      [ 1, 2, 3, false, true, null ].forEach(function (doc) {
        try {
          collection.save(doc);
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document w/ invalid type
////////////////////////////////////////////////////////////////////////////////

    testUpdateInvalidDocumentType : function () {
      var d = collection.save({ _key: "test" });

      [ 1, 2, 3, false, true, null, [ ] ].forEach(function (doc) {
        try {
          collection.update(d, doc);
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document w/ invalid type
////////////////////////////////////////////////////////////////////////////////

    testReplaceInvalidDocumentType : function () {
      var d = collection.save({ _key: "test" });

      [ 1, 2, 3, false, true, null, [ ] ].forEach(function (doc) {
        try {
          collection.replace(d, doc);
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ invalid primary key data types
////////////////////////////////////////////////////////////////////////////////

    testSaveInvalidDocumentKeyType : function () {
      [ 1, 2, 3, false, true, null, [ ], { } ].forEach(function (key) {
        try {
          collection.save({ _key: key });
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ invalid primary key values
////////////////////////////////////////////////////////////////////////////////

    testSaveInvalidDocumentKeyValue : function () {
      [ "", " ", "  ", " a", "a ", "/", "|", "#", "a/a", "\0", "\r", "\n", "\t", "\"", "[", "]", "{", "}", "\\" ].forEach(function (key) {
        try {
          collection.save({ _key: key });
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ special characters
////////////////////////////////////////////////////////////////////////////////

    testSaveSpecialCharsDocumentKey : function () {
      [ ":", "-", "_", "@", ".", "..", "...", "a@b", "a@b.c", "a-b-c", "_a", "@a", "@a-b", ":80", ":_", "@:_",
        "0", "1", "123456", "0123456", "true", "false", "a", "A", "a1", "A1", "01ab01", "01AB01",
        "abcd-efgh", "abcd_efgh", "Abcd_Efgh", "@@", "abc@foo.bar", "@..abc-@-foo__bar",
        ".foobar", "-foobar", "_foobar", "@foobar", "(valid)", "%valid", "$valid",
        "$$bill,y'all", "'valid", "'a-key-is-a-key-is-a-key'", "m+ller", ";valid", ",valid", "!valid!",
        ":::", ":-:-:", ";", ";;;;;;;;;;", "(", ")", "()xoxo()", "%",
        "%-%-%-%", ":-)", "!", "!!!!", "'", "''''", "this-key's-valid.", "=",
        "==================================================", "-=-=-=___xoxox-",
        "*", "(*)", "****", "--", "__" ].forEach(function (key) {
        var doc1 = collection.save({ _key: key, value: key });
        assertEqual(key, doc1._key);
        assertEqual(cn + "/" + key, doc1._id);

        assertTrue(collection.exists(key));
        assertTrue(db._exists(cn + "/" + key));

        var doc2 = collection.document(key);
        assertEqual(key, doc2._key);
        assertEqual(cn + "/" + key, doc2._id);
        assertEqual(key, doc2.value);

        var doc3 = collection.document(cn + "/" + key);
        assertEqual(key, doc3._key);
        assertEqual(cn + "/" + key, doc3._id);
        assertEqual(key, doc3.value);

        var doc4 = db._document(cn + "/" + key);
        assertEqual(key, doc4._key);
        assertEqual(cn + "/" + key, doc4._id);
        assertEqual(key, doc4.value);

        collection.remove(key);

        assertFalse(collection.exists(key));
        assertFalse(db._exists(cn + "/" + key));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ primary key violation
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicate : function () {
      var d1, d2, doc;

      try {
        d1 = collection.save({ _key: "test", value: 1 });
        d2 = collection.save({ _key: "test", value: 2 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertTypeOf("string", d1._id);
      assertTypeOf("string", d1._key);
      assertTypeOf("string", d1._rev);
      assertEqual("UnitTestsCollectionBasics/test", d1._id);
      assertEqual("test", d1._key);

      doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(1, doc.value);

      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ secondary key violation
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicateSecondary : function () {
      var d1, d2, doc;

      collection.ensureUniqueConstraint("value1");

      try {
        d1 = collection.save({ _key: "test1", value1: 1, value2: 1 });
        d2 = collection.save({ _key: "test2", value1: 1, value2: 2 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertTypeOf("string", d1._id);
      assertTypeOf("string", d1._key);
      assertTypeOf("string", d1._rev);
      assertEqual("UnitTestsCollectionBasics/test1", d1._id);
      assertEqual("test1", d1._key);

      doc = collection.document("test1");
      assertEqual("UnitTestsCollectionBasics/test1", doc._id);
      assertEqual("test1", doc._key);
      assertEqual(1, doc.value1);
      assertEqual(1, doc.value2);

      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ primary key violation, then unload/reload
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicateUnloadReload : function () {
      var d1, d2, doc;

      try {
        d1 = collection.save({ _key: "test", value: 1 });
        d2 = collection.save({ _key: "test", value: 2 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      d1 = null;
      d2 = null;

      testHelper.waitUnload(collection);

      collection.load();

      doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(1, doc.value);

      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ secondary key violation, then unload/reload
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicateSecondaryUnloadReload : function () {
      var d1, d2, doc;

      collection.ensureUniqueConstraint("value1");

      try {
        d1 = collection.save({ _key: "test1", value1: 1, value2: 1 });
        d2 = collection.save({ _key: "test1", value1: 1, value2: 2 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      d1 = null;
      d2 = null;

      testHelper.waitUnload(collection);

      collection.load();

      doc = collection.document("test1");
      assertEqual("UnitTestsCollectionBasics/test1", doc._id);
      assertEqual("test1", doc._key);
      assertEqual(1, doc.value1);
      assertEqual(1, doc.value2);

      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document insert/delete w/ reload
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicates : function () {
      var d, i;

      for (i = 0; i < 10; ++i) {
        d = collection.save({ _key: "test", value: i });
        collection.remove(d);
      }
      d = null;

      assertEqual(0, collection.count());

      testHelper.waitUnload(collection);

      collection.load();

      assertEqual(0, collection.count());

      d = collection.save({ _key: "test", value: 200 });
      assertTypeOf("string", d._id);
      assertTypeOf("string", d._key);
      assertTypeOf("string", d._rev);
      assertEqual("UnitTestsCollectionBasics/test", d._id);
      assertEqual("test", d._key);

      var doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(200, doc.value);

      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document insert/delete w/ reload
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicatesSurvive : function () {
      var i;

      for (i = 0; i < 10; ++i) {
        var d = collection.save({ _key: "test", value: i });
        collection.remove(d);
      }
      collection.save({ _key: "test", value: 99 });

      assertEqual(1, collection.count());

      testHelper.waitUnload(collection);

      collection.load();

      assertEqual(1, collection.count());

      var doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(99, doc.value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document insert/delete w/ reload
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicatesViolationSurvive : function () {
      var i;

      try {
        collection.remove("test");
        fail("whoops");
      }
      catch (e1) {
      }

      for (i = 0; i < 10; ++i) {
        try {
          collection.save({ _key: "test", value: i });
        }
        catch (e2) {
        }
      }

      assertEqual(1, collection.count());
      var doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(0, doc.value);
      doc = null;

      testHelper.waitUnload(collection);

      collection.load();

      assertEqual(1, collection.count());

      doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(0, doc.value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document
////////////////////////////////////////////////////////////////////////////////

    testSaveDocument : function () {
      var doc = collection.save({ "Hello" : "World" });

      assertTypeOf("string", doc._id);
      assertTypeOf("string", doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentSyncFalse : function () {
      var doc = collection.save({ "Hello" : "World" }, false);

      assertTypeOf("string", doc._id);
      assertTypeOf("string", doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentSyncTrue : function () {
      var doc = collection.save({ "Hello" : "World" }, true);

      assertTypeOf("string", doc._id);
      assertTypeOf("string", doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document
////////////////////////////////////////////////////////////////////////////////

    testReadDocument : function () {
      var d = collection.save({ "Hello" : "World" });

      var doc = collection.document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      doc = collection.document(d);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document with conflict
////////////////////////////////////////////////////////////////////////////////

    testReadDocumentConflict : function () {
      var d = collection.save({ "Hello" : "World" });

      var doc = collection.document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      var r = collection.replace(d, { "Hello" : "You" });

      doc = collection.document(r);

      assertEqual(d._id, doc._id);
      assertNotEqual(d._rev, doc._rev);

      try {
        collection.document(d);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief exists
////////////////////////////////////////////////////////////////////////////////

    testExistsDocument : function () {
      var d1 = collection.save({ _key : "baz" });

      // string keys
      assertFalse(collection.exists("foo"));
      assertFalse(collection.exists("bar"));
      var r = collection.exists("baz");
      assertTypeOf("object", r);
      assertTypeOf("string", r._id);
      assertTypeOf("string", r._key);
      assertTypeOf("string", r._rev);

      assertFalse(collection.exists(cn + "/foo"));
      assertFalse(collection.exists(cn + "/bar"));
      r = collection.exists(cn + "/baz");
      assertTypeOf("object", r);
      assertTypeOf("string", r._id);
      assertTypeOf("string", r._key);
      assertTypeOf("string", r._rev);

      // object key
      r = collection.exists(d1);
      assertTypeOf("object", r);
      assertTypeOf("string", r._id);
      assertTypeOf("string", r._key);
      assertTypeOf("string", r._rev);

      var d2 = collection.save({ _key : "2" });
      // string keys
      assertFalse(collection.exists("1"));
      r = collection.exists("2");
      assertTypeOf("object", r);
      assertTypeOf("string", r._id);
      assertTypeOf("string", r._key);
      assertTypeOf("string", r._rev);

      assertFalse(collection.exists(cn + "/1"));
      r = collection.exists(cn + "/2");
      assertTypeOf("object", r);
      assertTypeOf("string", r._id);
      assertTypeOf("string", r._key);
      assertTypeOf("string", r._rev);

      // object key
      r = collection.exists(d2);
      assertTypeOf("object", r);
      assertTypeOf("string", r._id);
      assertTypeOf("string", r._key);
      assertTypeOf("string", r._rev);

      // check with revision
      var d3 = collection.replace("2", { });
      // d2 is gone now...
      try {
        collection.exists(d2);
        fail();
      }
      catch (err0) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err0.errorNum);
      }

      // d3 is still there
      r = collection.exists(d3);
      assertTypeOf("object", r);
      assertTypeOf("string", r._id);
      assertTypeOf("string", r._key);
      assertTypeOf("string", r._rev);

      // cross-collection query
      try {
        collection.exists("UnitTestsNonExistingCollection/1");
        fail();
      }
      catch (err1) {
        assertEqual(ERRORS.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code, err1.errorNum);
      }

      // invalid key pattern
      try {
        collection.exists("foo bar");
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err2.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        collection.replace(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = collection.document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = collection.replace(a1, { a : 4 }, true);

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = collection.document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tests the replace function with the new signature
////////////////////////////////////////////////////////////////////////////////

    testReplaceWithNewSignatureDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);
      // important test, the server has to compute the overwrite policy in correct wise
      var a2 = collection.replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        collection.replace(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = collection.document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);
      // new signature
      var a4 = collection.replace(a1, { a : 4 }, {"overwrite" : true});

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = collection.document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 }, true, false);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief tests the replace function with new signature
////////////////////////////////////////////////////////////////////////////////

    testReplaceWithNewSignatureDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 }, {"overwrite": true, "waitForSync": false});

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 }, true, true);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tests the replace function with new signature
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocumentSyncTrue2 : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 }, {"overwrite": true, "waitForSync": true});

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.update(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        collection.update(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = collection.document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = collection.update(a1, { a : 4 }, true);

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = collection.document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);

      var a5 = collection.update(a4, { b : 1, c : 2, d : "foo", e : null });
      assertEqual(a1._id, a5._id);
      assertNotEqual(a4._rev, a5._rev);

      var doc5 = collection.document(a1._id);
      assertEqual(a1._id, doc5._id);
      assertEqual(a5._rev, doc5._rev);
      assertEqual(4, doc5.a);
      assertEqual(1, doc5.b);
      assertEqual(2, doc5.c);
      assertEqual("foo", doc5.d);
      assertEqual(null, doc5.e);

      var a6 = collection.update(a5, { f : null, b : null, a : null, g : 2, c : 4 });
      assertEqual(a1._id, a6._id);
      assertNotEqual(a5._rev, a6._rev);

      var doc6 = collection.document(a1._id);
      assertEqual(a1._id, doc6._id);
      assertEqual(a6._rev, doc6._rev);
      assertEqual(null, doc6.a);
      assertEqual(null, doc6.b);
      assertEqual(4, doc6.c);
      assertEqual("foo", doc6.d);
      assertEqual(null, doc6.e);
      assertEqual(null, doc6.f);
      assertEqual(2, doc6.g);

      var a7 = collection.update(a6, { a : null, b : null, c : null, g : null }, true, false);
      assertEqual(a1._id, a7._id);
      assertNotEqual(a6._rev, a7._rev);

      var doc7 = collection.document(a1._id);
      assertEqual(a1._id, doc7._id);
      assertEqual(a7._rev, doc7._rev);
      assertEqual(undefined, doc7.a);
      assertEqual(undefined, doc7.b);
      assertEqual(undefined, doc7.c);
      assertEqual("foo", doc7.d);
      assertEqual(null, doc7.e);
      assertEqual(null, doc7.f);
      assertEqual(undefined, doc7.g);

      var a8 = collection.update(a7, { d : { "one" : 1, "two" : 2, "three" : 3 }, e : { }, f : { "one" : 1 }} );
      assertEqual(a1._id, a8._id);
      assertNotEqual(a7._rev, a8._rev);

      var doc8 = collection.document(a1._id);
      assertEqual(a1._id, doc8._id);
      assertEqual(a8._rev, doc8._rev);
      assertEqual({"one": 1, "two": 2, "three": 3}, doc8.d);
      assertEqual({}, doc8.e);
      assertEqual({"one": 1}, doc8.f);

      var a9 = collection.update(a8, { d : { "four" : 4 }, "e" : { "e1" : [ 1, 2 ], "e2" : 2 }, "f" : { "three" : 3 }} );
      assertEqual(a1._id, a9._id);
      assertNotEqual(a8._rev, a9._rev);

      var doc9 = collection.document(a1._id);
      assertEqual(a1._id, doc9._id);
      assertEqual(a9._rev, doc9._rev);
      assertEqual({"one": 1, "two": 2, "three": 3, "four": 4}, doc9.d);
      assertEqual({"e2": 2, "e1": [ 1, 2 ]}, doc9.e);
      assertEqual({"one": 1, "three": 3}, doc9.f);

      var a10 = collection.update(a9, { d : { "one" : -1, "two": null, "four" : null, "five" : 5 }, "e" : { "e1" : 1, "e2" : null, "e3" : 3 }}, true, false);
      assertEqual(a1._id, a10._id);
      assertNotEqual(a9._rev, a10._rev);

      var doc10 = collection.document(a1._id);
      assertEqual(a1._id, doc10._id);
      assertEqual(a10._rev, doc10._rev);
      assertEqual({"one": -1, "three": 3, "five": 5}, doc10.d);
      assertEqual({"e1": 1, "e3": 3}, doc10.e);
      assertEqual({"one": 1, "three": 3}, doc10.f);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document
////////////////////////////////////////////////////////////////////////////////

    testNewSignatureUpdateDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.update(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        collection.update(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = collection.document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = collection.update(a1, { a : 4 }, {"overwrite": true});

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = collection.document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);

      var a5 = collection.update(a4, { b : 1, c : 2, d : "foo", e : null });
      assertEqual(a1._id, a5._id);
      assertNotEqual(a4._rev, a5._rev);

      var doc5 = collection.document(a1._id);
      assertEqual(a1._id, doc5._id);
      assertEqual(a5._rev, doc5._rev);
      assertEqual(4, doc5.a);
      assertEqual(1, doc5.b);
      assertEqual(2, doc5.c);
      assertEqual("foo", doc5.d);
      assertEqual(null, doc5.e);

      var a6 = collection.update(a5, { f : null, b : null, a : null, g : 2, c : 4 });
      assertEqual(a1._id, a6._id);
      assertNotEqual(a5._rev, a6._rev);

      var doc6 = collection.document(a1._id);
      assertEqual(a1._id, doc6._id);
      assertEqual(a6._rev, doc6._rev);
      assertEqual(null, doc6.a);
      assertEqual(null, doc6.b);
      assertEqual(4, doc6.c);
      assertEqual("foo", doc6.d);
      assertEqual(null, doc6.e);
      assertEqual(null, doc6.f);
      assertEqual(2, doc6.g);

      var a7 = collection.update(a6, { a : null, b : null, c : null, g : null }, {"overwrite": true, "keepNull": false});
      assertEqual(a1._id, a7._id);
      assertNotEqual(a6._rev, a7._rev);

      var doc7 = collection.document(a1._id);
      assertEqual(a1._id, doc7._id);
      assertEqual(a7._rev, doc7._rev);
      assertEqual(undefined, doc7.a);
      assertEqual(undefined, doc7.b);
      assertEqual(undefined, doc7.c);
      assertEqual("foo", doc7.d);
      assertEqual(null, doc7.e);
      assertEqual(null, doc7.f);
      assertEqual(undefined, doc7.g);

      var a8 = collection.update(a7, { d : { "one" : 1, "two" : 2, "three" : 3 }, e : { }, f : { "one" : 1 }} );
      assertEqual(a1._id, a8._id);
      assertNotEqual(a7._rev, a8._rev);

      var doc8 = collection.document(a1._id);
      assertEqual(a1._id, doc8._id);
      assertEqual(a8._rev, doc8._rev);
      assertEqual({"one": 1, "two": 2, "three": 3}, doc8.d);
      assertEqual({}, doc8.e);
      assertEqual({"one": 1}, doc8.f);

      var a9 = collection.update(a8, { d : { "four" : 4 }, "e" : { "e1" : [ 1, 2 ], "e2" : 2 }, "f" : { "three" : 3 }} );
      assertEqual(a1._id, a9._id);
      assertNotEqual(a8._rev, a9._rev);

      var doc9 = collection.document(a1._id);
      assertEqual(a1._id, doc9._id);
      assertEqual(a9._rev, doc9._rev);
      assertEqual({"one": 1, "two": 2, "three": 3, "four": 4}, doc9.d);
      assertEqual({"e2": 2, "e1": [ 1, 2 ]}, doc9.e);
      assertEqual({"one": 1, "three": 3}, doc9.f);

      var a10 = collection.update(a9, { d : { "one" : -1, "two": null, "four" : null, "five" : 5 }, "e" : { "e1" : 1, "e2" : null, "e3" : 3 }}, {"overwrite": true, "keepNull": false});
      assertEqual(a1._id, a10._id);
      assertNotEqual(a9._rev, a10._rev);

      var doc10 = collection.document(a1._id);
      assertEqual(a1._id, doc10._id);
      assertEqual(a10._rev, doc10._rev);
      assertEqual({"one": -1, "three": 3, "five": 5}, doc10.d);
      assertEqual({"e1": 1, "e3": 3}, doc10.e);
      assertEqual({"one": 1, "three": 3}, doc10.f);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test mergeObjects
////////////////////////////////////////////////////////////////////////////////

    testUpdateMergeObjects : function () {
      var doc1 = collection.save({ name: { first: "foo", last: "bar" }, owns: { evilCellPhone: [ 1 ] } });

      // do not specify mergeObjects. its default value is true
      collection.update(doc1, { name: { middle: "baz" }, owns: { schabernack: true, pileOfBones: null } }, { });

      var doc = collection.document(doc1._key);
      assertEqual({ first: "foo", last: "bar", middle: "baz" }, doc.name);
      assertEqual({ evilCellPhone: [ 1 ], schabernack: true, pileOfBones: null }, doc.owns);

      // explicitly specifiy mergeObjects
      var doc2 = collection.save({ name: { first: "foo", last: "bar" }, owns: { evilCellPhone: [ 1 ] } });
      collection.update(doc2, { name: { middle: "baz" }, owns: { schabernack: true, pileOfBones: null } }, { mergeObjects: true });

      doc = collection.document(doc2._key);
      assertEqual({ first: "foo", last: "bar", middle: "baz" }, doc.name);
      assertEqual({ evilCellPhone: [ 1 ], schabernack: true, pileOfBones: null }, doc.owns);

      // disable mergeObjects
      var doc3 = collection.save({ name: { first: "foo", last: "bar" }, owns: { evilCellPhone: [ 1 ] } });
      collection.update(doc3, { name: { middle: "baz" }, owns: { schabernack: true, pileOfBones: null } }, { mergeObjects: false });

      doc = collection.document(doc3._key);
      assertEqual({ middle: "baz" }, doc.name);
      assertEqual({ schabernack: true, pileOfBones: null }, doc.owns);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        collection.remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var a3 = collection.remove(a1, true);

      assertTypeOf("object", a3);
      assertTypeOf("string", a3._id);
      assertTypeOf("string", a3._rev);
      assertTypeOf("string", a3._key);

      try {
        collection.remove(a1, true);
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err2.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document using new signature
////////////////////////////////////////////////////////////////////////////////

    testDeleteWithNewSignatureDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        collection.remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var a3 = collection.remove(a1, {"overwrite": true});
      assertTypeOf("object", a3);
      assertTypeOf("string", a3._id);
      assertTypeOf("string", a3._rev);
      assertTypeOf("string", a3._key);

      try {
        collection.remove(a1, {"overwrite": true});
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err2.errorNum);
      }

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);
                               //document, data, overwrite, keepNull, waitForSync
      var a2 = collection.update(a1, { a : 2 }, true, true, false);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief update a document, waitForSync=false; using new signature
////////////////////////////////////////////////////////////////////////////////

    testUpdateWithNewSignatureDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.update(a1, { a : 2 }, {"overwrite": true, "waitForSync" : false});

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

                               //document, data, overwrite, keepNull, waitForSync
      var a2 = collection.update(a1, { a : 2 }, true, true, true);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief update a document, waitForSync=true,new signature
////////////////////////////////////////////////////////////////////////////////

    testUpdateWithNewSignatureDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.update(a1, { a : 2 }, {"overwrite": true, "waitForSync" : true});

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.remove(a1, true, false);
      assertTypeOf("object", a2);
      assertTypeOf("string", a2._id);
      assertTypeOf("string", a2._rev);
      assertTypeOf("string", a2._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document, waitForSync=false , using new signature
////////////////////////////////////////////////////////////////////////////////

    testDeleteWithNewSignatureDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.remove(a1, {"overwrite": true, "waitForSync": false});
      assertTypeOf("object", a2);
      assertTypeOf("string", a2._id);
      assertTypeOf("string", a2._rev);
      assertTypeOf("string", a2._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.remove(a1, true, true);
      assertTypeOf("object", a2);
      assertTypeOf("string", a2._id);
      assertTypeOf("string", a2._rev);
      assertTypeOf("string", a2._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document, waitForSync=true , using new signature
////////////////////////////////////////////////////////////////////////////////

    testDeleteWithNewSignatureDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.remove(a1, {"overwrite": true, "waitForSync": true});
      assertTypeOf("object", a2);
      assertTypeOf("string", a2._id);
      assertTypeOf("string", a2._rev);
      assertTypeOf("string", a2._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a deleted document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDeletedDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      collection.remove(a1);

      try {
        collection.remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test saving and searching for special characters
////////////////////////////////////////////////////////////////////////////////

    testSpecialChars : function () {
      [ "the quick\nbrown fox jumped over\r\nthe lazy dog",
        "'the \"\\quick\\\n \"brown\\\rfox' jumped",
        '"the fox"" jumped \\over the \newline \roof"' ].forEach(function(value) {
        collection.save({ text: value });

        var result = collection.byExample({ text: value }).toArray();
        assertEqual(1, result.length);
        assertEqual(value, result[0].text);
      });
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function DatabaseDocumentSuiteErrorHandling () {
  'use strict';
  var cn = "UnitTestsCollectionBasics";
  var ERRORS = require("internal").errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testDBErrorHandlingBadHandle : function () {
      try {
        db._document("  123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle replace
////////////////////////////////////////////////////////////////////////////////

    testDBErrorHandlingBadHandleReplace : function () {
      try {
        db._replace("123456  ", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle delete
////////////////////////////////////////////////////////////////////////////////

    testDBErrorHandlingBadHandleDelete : function () {
      try {
        db._remove("123/45/6");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown document identifier
////////////////////////////////////////////////////////////////////////////////

    testDBErrorHandlingUnknownDocument : function () {
      var collection = db._create(cn, { waitForSync : false });

      try {
        db._document(collection.name() + "/123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }

      collection.drop();
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function DatabaseDocumentSuite () {
  'use strict';
  var cn = "UnitTestsCollectionBasics";
  var ERRORS = require("internal").errors;
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      collection = db._create(cn, { waitForSync : false });

      collection.load();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document
////////////////////////////////////////////////////////////////////////////////

    testDBReadDocument : function () {
      var d = collection.save({ "Hello" : "World" });

      var doc = db._document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      doc = db._document(d);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document with conflict
////////////////////////////////////////////////////////////////////////////////

    testDBReadDocumentConflict : function () {
      var d = collection.save({ "Hello" : "World" });

      var doc = db._document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      var r = collection.replace(d, { "Hello" : "You" });

      doc = db._document(r);

      assertEqual(d._id, doc._id);
      assertNotEqual(d._rev, doc._rev);

      try {
        db._document(d);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief exists
////////////////////////////////////////////////////////////////////////////////

    testDBExistsDocument : function () {
      var d1 = collection.save({ _key : "baz" });

      // string keys
      assertFalse(db._exists(cn + "/foo"));
      assertFalse(db._exists(cn + "/bar"));
      assertTrue(db._exists(cn + "/baz"));
      // object key
      assertTrue(db._exists(d1));

      var d2 = collection.save({ _key : "2" });
      // string keys
      assertFalse(db._exists(cn + "/1"));
      assertTrue(db._exists(cn + "/2"));
      // object key
      assertTrue(db._exists(d2));

      // check with revision
      var d3 = collection.replace("2", { });
      // d2 is gone now...
      try {
        db._exists(d2);
      }
      catch (err0) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err0.errorNum);
      }

      // d3 is still there
      assertTrue(db._exists(d3));


      // non existing collection
      try {
        db._exists("UnitTestsNonExistingCollection/1");
      }
      catch (err1) {
        assertEqual(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err1.errorNum);
      }

      // invalid key pattern
      try {
        db._exists(cn + "/foo bar");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document
////////////////////////////////////////////////////////////////////////////////

    testDBReplaceDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = db._replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._replace(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = db._document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = db._replace(a1, { a : 4 }, true);

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = db._document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tests the _replace function with new signature
////////////////////////////////////////////////////////////////////////////////

    testDBReplaceWithNewSignatureDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = db._replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._replace(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = db._document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = db._replace(a1, { a : 4 }, {"overwrite": true});

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = db._document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document
////////////////////////////////////////////////////////////////////////////////

    testDBUpdateDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = db._update(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._update(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = db._document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = db._update(a1, { a : 4 }, true);

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = db._document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);

      var a5 = db._update(a4, { b : 1, c : 2, d : "foo", e : null });
      assertEqual(a1._id, a5._id);
      assertNotEqual(a4._rev, a5._rev);

      var doc5 = db._document(a1._id);
      assertEqual(a1._id, doc5._id);
      assertEqual(a5._rev, doc5._rev);
      assertEqual(4, doc5.a);
      assertEqual(1, doc5.b);
      assertEqual(2, doc5.c);
      assertEqual("foo", doc5.d);
      assertEqual(null, doc5.e);

      var a6 = db._update(a5, { f : null, b : null, a : null, g : 2, c : 4 });
      assertEqual(a1._id, a6._id);
      assertNotEqual(a5._rev, a6._rev);

      var doc6 = db._document(a1._id);
      assertEqual(a1._id, doc6._id);
      assertEqual(a6._rev, doc6._rev);
      assertEqual(null, doc6.a);
      assertEqual(null, doc6.b);
      assertEqual(4, doc6.c);
      assertEqual("foo", doc6.d);
      assertEqual(null, doc6.e);
      assertEqual(null, doc6.f);
      assertEqual(2, doc6.g);

      var a7 = db._update(a6, { a : null, b : null, c : null, g : null }, true, false);
      assertEqual(a1._id, a7._id);
      assertNotEqual(a6._rev, a7._rev);

      var doc7 = db._document(a1._id);
      assertEqual(a1._id, doc7._id);
      assertEqual(a7._rev, doc7._rev);
      assertEqual(undefined, doc7.a);
      assertEqual(undefined, doc7.b);
      assertEqual(undefined, doc7.c);
      assertEqual("foo", doc7.d);
      assertEqual(null, doc7.e);
      assertEqual(null, doc7.f);
      assertEqual(undefined, doc7.g);

      var a8 = db._update(a7, { d : { "one" : 1, "two" : 2, "three" : 3 }, e : { }, f : { "one" : 1 }} );
      assertEqual(a1._id, a8._id);
      assertNotEqual(a7._rev, a8._rev);

      var doc8 = db._document(a1._id);
      assertEqual(a1._id, doc8._id);
      assertEqual(a8._rev, doc8._rev);
      assertEqual({"one": 1, "two": 2, "three": 3}, doc8.d);
      assertEqual({}, doc8.e);
      assertEqual({"one": 1}, doc8.f);

      var a9 = db._update(a8, { d : { "four" : 4 }, "e" : { "e1" : [ 1, 2 ], "e2" : 2 }, "f" : { "three" : 3 }} );
      assertEqual(a1._id, a9._id);
      assertNotEqual(a8._rev, a9._rev);

      var doc9 = db._document(a1._id);
      assertEqual(a1._id, doc9._id);
      assertEqual(a9._rev, doc9._rev);
      assertEqual({"one": 1, "two": 2, "three": 3, "four": 4}, doc9.d);
      assertEqual({"e2": 2, "e1": [ 1, 2 ]}, doc9.e);
      assertEqual({"one": 1, "three": 3}, doc9.f);

      var a10 = db._update(a9, { d : { "one" : -1, "two": null, "four" : null, "five" : 5 }, "e" : { "e1" : 1, "e2" : null, "e3" : 3 }}, true, false);
      assertEqual(a1._id, a10._id);
      assertNotEqual(a9._rev, a10._rev);

      var doc10 = db._document(a1._id);
      assertEqual(a1._id, doc10._id);
      assertEqual(a10._rev, doc10._rev);
      assertEqual({"one": -1, "three": 3, "five": 5}, doc10.d);
      assertEqual({"e1": 1, "e3": 3}, doc10.e);
      assertEqual({"one": 1, "three": 3}, doc10.f);
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief tests update function with new signature
////////////////////////////////////////////////////////////////////////////////

    testDBNewsignatureOf_UpdateDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = db._update(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._update(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = db._document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = db._update(a1, { a : 4 }, {"overwrite": true});

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = db._document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);

      var a5 = db._update(a4, { b : 1, c : 2, d : "foo", e : null });
      assertEqual(a1._id, a5._id);
      assertNotEqual(a4._rev, a5._rev);

      var doc5 = db._document(a1._id);
      assertEqual(a1._id, doc5._id);
      assertEqual(a5._rev, doc5._rev);
      assertEqual(4, doc5.a);
      assertEqual(1, doc5.b);
      assertEqual(2, doc5.c);
      assertEqual("foo", doc5.d);
      assertEqual(null, doc5.e);

      var a6 = db._update(a5, { f : null, b : null, a : null, g : 2, c : 4 });
      assertEqual(a1._id, a6._id);
      assertNotEqual(a5._rev, a6._rev);

      var doc6 = db._document(a1._id);
      assertEqual(a1._id, doc6._id);
      assertEqual(a6._rev, doc6._rev);
      assertEqual(null, doc6.a);
      assertEqual(null, doc6.b);
      assertEqual(4, doc6.c);
      assertEqual("foo", doc6.d);
      assertEqual(null, doc6.e);
      assertEqual(null, doc6.f);
      assertEqual(2, doc6.g);

      var a7 = db._update(a6, { a : null, b : null, c : null, g : null }, {"overwrite": true, "keepNull": false});
      assertEqual(a1._id, a7._id);
      assertNotEqual(a6._rev, a7._rev);

      var doc7 = db._document(a1._id);
      assertEqual(a1._id, doc7._id);
      assertEqual(a7._rev, doc7._rev);
      assertEqual(undefined, doc7.a);
      assertEqual(undefined, doc7.b);
      assertEqual(undefined, doc7.c);
      assertEqual("foo", doc7.d);
      assertEqual(null, doc7.e);
      assertEqual(null, doc7.f);
      assertEqual(undefined, doc7.g);

      var a8 = db._update(a7, { d : { "one" : 1, "two" : 2, "three" : 3 }, e : { }, f : { "one" : 1 }} );
      assertEqual(a1._id, a8._id);
      assertNotEqual(a7._rev, a8._rev);

      var doc8 = db._document(a1._id);
      assertEqual(a1._id, doc8._id);
      assertEqual(a8._rev, doc8._rev);
      assertEqual({"one": 1, "two": 2, "three": 3}, doc8.d);
      assertEqual({}, doc8.e);
      assertEqual({"one": 1}, doc8.f);

      var a9 = db._update(a8, { d : { "four" : 4 }, "e" : { "e1" : [ 1, 2 ], "e2" : 2 }, "f" : { "three" : 3 }} );
      assertEqual(a1._id, a9._id);
      assertNotEqual(a8._rev, a9._rev);

      var doc9 = db._document(a1._id);
      assertEqual(a1._id, doc9._id);
      assertEqual(a9._rev, doc9._rev);
      assertEqual({"one": 1, "two": 2, "three": 3, "four": 4}, doc9.d);
      assertEqual({"e2": 2, "e1": [ 1, 2 ]}, doc9.e);
      assertEqual({"one": 1, "three": 3}, doc9.f);

      var a10 = db._update(a9, { d : { "one" : -1, "two": null, "four" : null, "five" : 5 }, "e" : { "e1" : 1, "e2" : null, "e3" : 3 }}, {"overwrite": true, "keepNull": false});
      assertEqual(a1._id, a10._id);
      assertNotEqual(a9._rev, a10._rev);

      var doc10 = db._document(a1._id);
      assertEqual(a1._id, doc10._id);
      assertEqual(a10._rev, doc10._rev);
      assertEqual({"one": -1, "three": 3, "five": 5}, doc10.d);
      assertEqual({"e1": 1, "e3": 3}, doc10.e);
      assertEqual({"one": 1, "three": 3}, doc10.f);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document
////////////////////////////////////////////////////////////////////////////////

    testDBDeleteDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = db._replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertEqual(a1._key, a2._key);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var a3 = db._remove(a1, {"overwrite" : true});
      assertTypeOf("object", a3);
      assertTypeOf("string", a3._id);
      assertTypeOf("string", a3._rev);
      assertTypeOf("string", a3._key);

      try {
        db._remove(a1, {"overwrite" : true});
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err2.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document using new signature of the remove function
///       of the remove function
////////////////////////////////////////////////////////////////////////////////

    testDBDeleteWithNewSignatureDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = db._replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var a3 = db._remove(a1, {"overwrite" : true});

      assertTypeOf("object", a3);
      assertTypeOf("string", a3._id);
      assertTypeOf("string", a3._rev);
      assertTypeOf("string", a3._key);

      try {
        db._remove(a1, {"overwrite" : true});
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err2.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a deleted document
////////////////////////////////////////////////////////////////////////////////

    testDBDeleteDeletedDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      db._remove(a1);

      try {
        db._remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a very big document
////////////////////////////////////////////////////////////////////////////////

    testDBDocumentVeryLarge : function () {
      // create a very big and silly document, just to blow up the datafiles
      var doc = { };
      for (var i = 0; i < 60000; ++i) {
        var val = "thequickbrownfoxjumpsoverthelazydog" + i;
        doc[val] = val;
      }

      assertEqual(0, collection.count());
      collection.save(doc);
      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document bigger than shape file size
////////////////////////////////////////////////////////////////////////////////

    testDBBigShape : function () {
      // create a very big and silly document, just to blow up the datafiles
      var doc = { _key : "mydoc" };
      for (var i = 0; i < 50000; ++i) {
        doc["thequickbrownfox" + i] = 1;
      }

      assertEqual(0, collection.count());
      collection.save(doc);
      assertEqual(1, collection.count());
      assertEqual("mydoc", collection.document("mydoc")._key);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: returnNew and returnOld options
////////////////////////////////////////////////////////////////////////////////

function DatabaseDocumentSuiteReturnStuff () {
  'use strict';
  var cn = "UnitTestsCollectionBasics";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      collection = db._create(cn, { waitForSync : false });

      collection.load();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with and without returnNew
////////////////////////////////////////////////////////////////////////////////

    testCreateReturnNew : function () {
      var res = collection.insert({"Hallo":12});
      assertTypeOf("object", res);
      assertEqual(3, Object.keys(res).length);
      assertTypeOf("string", res._id);
      assertTypeOf("string", res._key);
      assertTypeOf("string", res._rev);

      // Now with returnNew: true
      res = collection.insert({"Hallo":12}, {returnNew: true});
      assertTypeOf("object", res);
      assertEqual(4, Object.keys(res).length);
      assertTypeOf("string", res._id);
      assertTypeOf("string", res._key);
      assertTypeOf("string", res._rev);
      assertTypeOf("object", res["new"]);
      assertEqual(12, res["new"].Hallo);
      assertEqual(4, Object.keys(res["new"]).length);

      // Now with returnNew: false
      res = collection.insert({"Hallo":12}, {returnNew: false});
      assertTypeOf("object", res);
      assertEqual(3, Object.keys(res).length);
      assertTypeOf("string", res._id);
      assertTypeOf("string", res._key);
      assertTypeOf("string", res._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief remove with and without returnOld
////////////////////////////////////////////////////////////////////////////////

    testRemoveReturnOld : function () {
      var res = collection.insert({"Hallo":12});
      var res2 = collection.remove(res._key);

      assertTypeOf("object", res2);
      assertEqual(3, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);

      // Now with returnOld: true
      res = collection.insert({"Hallo":12});
      res2 = collection.remove(res._key, {returnOld: true});
      assertTypeOf("object", res2);
      assertEqual(4, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);
      assertTypeOf("object", res2.old);
      assertEqual(12, res2.old.Hallo);
      assertEqual(4, Object.keys(res2.old).length);

      // Now with returnOld: false
      res = collection.insert({"Hallo":12});
      res2 = collection.remove(res._key, {returnOld: false});
      assertTypeOf("object", res2);
      assertEqual(3, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace with and without returnOld and returnNew
////////////////////////////////////////////////////////////////////////////////

    testReplaceReturnOldNew : function () {
      var res = collection.insert({"Hallo":12});
      var res2 = collection.replace(res._key,{"Hallo":13});

      assertTypeOf("object", res2);
      assertEqual(4, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);

      // Now with returnOld: true
      res = collection.insert({"Hallo":12});
      res2 = collection.replace(res._key, {"Hallo":13}, {returnOld: true});
      assertTypeOf("object", res2);
      assertEqual(5, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);
      assertTypeOf("object", res2.old);
      assertEqual(12, res2.old.Hallo);
      assertEqual(4, Object.keys(res2.old).length);

      // Now with returnOld: false
      res = collection.insert({"Hallo":12});
      res2 = collection.replace(res._key, {"Hallo":14}, {returnOld: false});
      assertTypeOf("object", res2);
      assertEqual(4, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);

      // Now with returnNew: true
      res = collection.insert({"Hallo":12});
      res2 = collection.replace(res._key, {"Hallo":14}, {returnNew: true});
      assertTypeOf("object", res2);
      assertEqual(5, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);
      assertTypeOf("object", res2["new"]);
      assertEqual(14, res2["new"].Hallo);
      assertEqual(4, Object.keys(res2["new"]).length);

      // Now with returnOld: false
      res = collection.insert({"Hallo":12});
      res2 = collection.replace(res._key, {"Hallo":15}, {returnNew: false});
      assertTypeOf("object", res2);
      assertEqual(4, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);

      // Now with returnNew: true and returnOld:true
      res = collection.insert({"Hallo":12});
      res2 = collection.replace(res._key, {"Hallo":16},
                                {returnNew: true, returnOld: true});
      assertTypeOf("object", res2);
      assertEqual(6, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);
      assertTypeOf("object", res2.old);
      assertTypeOf("object", res2["new"]);
      assertEqual(16, res2["new"].Hallo);
      assertEqual(12, res2.old.Hallo);
      assertEqual(4, Object.keys(res2["new"]).length);
      assertEqual(4, Object.keys(res2.old).length);

      // Now with returnOld: false and returnNew: false
      res = collection.insert({"Hallo":12});
      res2 = collection.replace(res._key, {"Hallo":15},
                                {returnNew: false, returnOld: false});
      assertTypeOf("object", res2);
      assertEqual(4, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update with and without returnOld and returnNew
////////////////////////////////////////////////////////////////////////////////

    testUpdateReturnOldNew : function () {
      var res = collection.insert({"Hallo":12});
      var res2 = collection.update(res._key,{"Hallo":13});

      assertTypeOf("object", res2);
      assertEqual(4, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);

      // Now with returnOld: true
      res = collection.insert({"Hallo":12});
      res2 = collection.update(res._key, {"Hallo":13}, {returnOld: true});
      assertTypeOf("object", res2);
      assertEqual(5, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);
      assertTypeOf("object", res2.old);
      assertEqual(12, res2.old.Hallo);
      assertEqual(4, Object.keys(res2.old).length);

      // Now with returnOld: false
      res = collection.insert({"Hallo":12});
      res2 = collection.update(res._key, {"Hallo":14}, {returnOld: false});
      assertTypeOf("object", res2);
      assertEqual(4, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);

      // Now with returnNew: true
      res = collection.insert({"Hallo":12});
      res2 = collection.update(res._key, {"Hallo":14}, {returnNew: true});
      assertTypeOf("object", res2);
      assertEqual(5, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);
      assertTypeOf("object", res2["new"]);
      assertEqual(14, res2["new"].Hallo);
      assertEqual(4, Object.keys(res2["new"]).length);

      // Now with returnOld: false
      res = collection.insert({"Hallo":12});
      res2 = collection.update(res._key, {"Hallo":15}, {returnNew: false});
      assertTypeOf("object", res2);
      assertEqual(4, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);

      // Now with returnNew: true and returnOld:true
      res = collection.insert({"Hallo":12});
      res2 = collection.update(res._key, {"Hallo":16},
                                {returnNew: true, returnOld: true});
      assertTypeOf("object", res2);
      assertEqual(6, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);
      assertTypeOf("object", res2.old);
      assertTypeOf("object", res2["new"]);
      assertEqual(16, res2["new"].Hallo);
      assertEqual(12, res2.old.Hallo);
      assertEqual(4, Object.keys(res2["new"]).length);
      assertEqual(4, Object.keys(res2.old).length);

      // Now with returnOld: false and returnNew: false
      res = collection.insert({"Hallo":12});
      res2 = collection.update(res._key, {"Hallo":15},
                                {returnNew: false, returnOld: false});
      assertTypeOf("object", res2);
      assertEqual(4, Object.keys(res2).length);
      assertTypeOf("string", res2._id);
      assertTypeOf("string", res2._key);
      assertTypeOf("string", res2._rev);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief use overwrite option
////////////////////////////////////////////////////////////////////////////////

    testInsertOverwrite : function () {
      var docHandle = collection.insert({ a : 1});
      var key = docHandle._key;

      // normal insert with same key must fail!
      try{
        var res = collection.insert({a : 2, _key : key});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      // overwrite with same key must work
      var rv = collection.insert({c : 3, _key: key},{overwrite: true, returnOld: true, returnNew: true});
      var arr = collection.toArray();
      assertEqual(arr.length, 1);
      assertEqual(rv.new.c, 3);
      assertFalse(rv.new.hasOwnProperty('a'));
      assertEqual(rv.old.a, 1);

      // overwrite (babies) with same key must work
      collection.insert({b : 2, _key: key},{overwrite:true});
      arr = collection.toArray();
      assertEqual(arr.length, 1);
      assertEqual(arr[0].b, 2);

      // overwrite (babies) with same key must work
      collection.insert([{a : 3, _key: key}, {a : 4, _key: key}, {a : 5, _key: key}], {overwrite: true});
      arr = collection.toArray();
      assertEqual(arr.length, 1);
      assertEqual(arr[0].a, 5);

      rv = collection.insert({x : 3},{overwriteMode: "replace", returnNew: true});
      assertEqual(rv.new.x, 3);
      assertTypeOf("string", rv._id);
      assertTypeOf("string", rv._key);

    },

    testInsertOverwriteMode : function () {
      let doc = { _key : "Harry",
                  type : "hunk",
                  color: "black",
                  hobbies : { "1" : "sleeping" }
                };

      let update = { _key : "Harry",
                     color: "grey",
                     age: 11
                   };

      let merged = {...doc, ...update};

      collection.insert(doc);
      var rv = collection.insert(update,{overwrite: true, overwriteMode: "update", returnOld: true, returnNew: true});
      Object.keys(merged).forEach((key) => {
        assertEqual(merged[key], rv.new[key]);
      });

      update.age = null;
      merged.age = null;
      rv = collection.insert(update,{overwrite:true, overwriteMode: "update", returnOld: true, returnNew: true, keepNull: true});
      Object.keys(merged).forEach((key) => {
        assertEqual(merged[key], rv.new[key]);
      });

      update.age = null;
      delete merged.age;
      rv = collection.insert(update,{overwrite:true, overwriteMode: "update", returnOld: true, returnNew: true, keepNull: false});
      Object.keys(merged).forEach((key) => {
        assertEqual(merged[key], rv.new[key]);
      });

      update.hobbies = { "2" : "eating" };
      merged.hobbies = {...update.hobbies, ...merged.hobbies };
      rv = collection.insert(update,{overwriteMode: "update", returnOld: true, returnNew: true, keepNull: false});
      Object.keys(merged).forEach((key) => {
        assertEqual(merged[key], rv.new[key]);
      });

      update.hobbies = { "2" : "eating" };
      merged.hobbies = update.hobbies;
      rv = collection.insert(update,{overwriteMode: "update", returnOld: true, returnNew: true, mergeObjects: false});
      Object.keys(merged).forEach((key) => {
        assertEqual(merged[key], rv.new[key]);
      });
    }, // testInsertOverwriteModeUpdate

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: document keys
////////////////////////////////////////////////////////////////////////////////

function CollectionDocumentSuiteIdFromKey () {
  'use strict';
  var cn = "UnitTestsCollectionBasics";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      collection = db._create(cn, { waitForSync : false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (collection) {
        collection.unload();
        collection.drop();
        collection = null;
      }
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document key qualification
////////////////////////////////////////////////////////////////////////////////

    testDocumentIdThrowsIfInvalid () {
      let error;
      for (const key of ["/", "", undefined, "{invalid}"]) {
        try {
          collection.documentId(key);
        } catch (e) {
          error = e;
        }
        assertTrue(error);
        assertEqual(error.errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document key qualification
////////////////////////////////////////////////////////////////////////////////

    testDocumentId () {
      for (const key of ["yolo", "__special__", "my-key", "-_!$%'()*+,.:;=@"]) {
        assertEqual(collection.documentId(key), `${collection.name()}/${key}`);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionDocumentSuiteErrorHandling);
jsunity.run(CollectionDocumentSuite);

jsunity.run(DatabaseDocumentSuiteErrorHandling);
jsunity.run(DatabaseDocumentSuite);

jsunity.run(DatabaseDocumentSuiteReturnStuff);

jsunity.run(CollectionDocumentSuiteIdFromKey);

return jsunity.done();
