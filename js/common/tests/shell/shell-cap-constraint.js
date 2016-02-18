/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the cap constraint
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var testHelper = require("@arangodb/test-helper").Helper;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function CapConstraintSuite() {
  'use strict';
  var ERRORS = internal.errors;
  var cn = "UnitTestsCollectionCap";
  var collection = null;

  var nsort = function (l, r) {
    if (l !== r) {
      return (l < r) ? -1 : 1;
    }
    return 0;
  };

  var assertBadParameter = function (err) {
    assertTrue(err.errorNum === ERRORS.ERROR_BAD_PARAMETER.code ||
               err.errorNum === ERRORS.ERROR_HTTP_BAD_PARAMETER.code);
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  setUp : function () {
    internal.db._drop(cn);
    collection = internal.db._create(cn);
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  tearDown : function () {
    if (collection !== null) {
      internal.wait(0);
      collection.unload();
      collection.drop();
      collection = null;
    }
    internal.wait(0.0);
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cap with invalid count
////////////////////////////////////////////////////////////////////////////////

    testInvalidCount1 : function () {
      try {
        collection.ensureCapConstraint(0);
        fail();
      }
      catch (err) {
        assertBadParameter(err);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cap with invalid count
////////////////////////////////////////////////////////////////////////////////

    testInvalidCount2 : function () {
      try {
        collection.ensureCapConstraint(-10);
        fail();
      }
      catch (err) {
        assertBadParameter(err);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cap with invalid byteSize
////////////////////////////////////////////////////////////////////////////////

    testInvalidBytesize1 : function () {
      try {
        collection.ensureCapConstraint(0, -1024);
        fail();
      }
      catch (err) {
        assertBadParameter(err);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cap with invalid byteSize
////////////////////////////////////////////////////////////////////////////////

    testInvalidBytesize2 : function () {
      try {
        collection.ensureCapConstraint(0, 1024);
        fail();
      }
      catch (err) {
        assertBadParameter(err);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cap with invalid count
////////////////////////////////////////////////////////////////////////////////

    testInvalidCombination : function () {
      try {
        collection.ensureCapConstraint(0, 0);
        fail();
      }
      catch (err) {
        assertBadParameter(err);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cap creation
////////////////////////////////////////////////////////////////////////////////

    testCreationCap : function () {
      var idx = collection.ensureCapConstraint(10);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(10, idx.size);
      assertEqual(0, idx.byteSize);

      idx = collection.ensureCapConstraint(10);

      assertEqual(id, idx.id);
      assertEqual("cap", idx.type);
      assertEqual(false, idx.isNewlyCreated);
      assertEqual(10, idx.size);
      assertEqual(0, idx.byteSize);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation error handling
////////////////////////////////////////////////////////////////////////////////

    testCreationTwoCap : function () {
      var idx = collection.ensureCapConstraint(10);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(10, idx.size);
      assertEqual(0, idx.byteSize);

      try {
        idx = collection.ensureCapConstraint(20);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: number of documents
////////////////////////////////////////////////////////////////////////////////

    testNumberDocuments : function () {
      var idx = collection.ensureCapConstraint(10);
      var fun = function(d) { return d.n; };

      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(10, idx.size);
      assertEqual(0, idx.byteSize);

      assertEqual(0, collection.count());

      for (var i = 0;  i < 10;  ++i) {
        collection.save({ n : i });
      }

      assertEqual(10, collection.count());
      assertEqual([0,1,2,3,4,5,6,7,8,9], collection.toArray().map(fun).sort(nsort));

      collection.save({ n : 10 });
      var a = collection.save({ n : 11 });

      for (i = 12;  i < 20;  ++i) {
        collection.save({ n : i });
      }

      assertEqual(10, collection.count());
      assertEqual([10,11,12,13,14,15,16,17,18,19], collection.toArray().map(fun).sort(nsort));

      collection.replace(a._id, { n : 11, a : 1 });

      for (i = 20;  i < 29;  ++i) {
        collection.save({ n : i });
      }

      assertEqual(10, collection.count());
      assertEqual([11,20,21,22,23,24,25,26,27,28], collection.toArray().map(fun).sort(nsort));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: deletes
////////////////////////////////////////////////////////////////////////////////

    testDeletes : function () {
      var idx = collection.ensureCapConstraint(5);
      var fun = function(d) { return d.n; };

      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(5, idx.size);
      assertEqual(0, idx.byteSize);

      assertEqual(0, collection.count());

      for (var i = 0;  i < 5;  ++i) {
        try {
          collection.remove("foo");
        }
        catch (err) {
        }
      }

      assertEqual(0, collection.count());
      assertEqual([ ], collection.toArray());

      var d = collection.save({ n : 1 });
      assertEqual(1, collection.count());
      assertEqual([ 1 ], collection.toArray().map(fun).sort(nsort));

      collection.remove(d);
      assertEqual(0, collection.count());
      assertEqual([ ], collection.toArray());

      for (i = 0; i < 10; ++i) {
        collection.save({ n : i });
      }

      assertEqual(5, collection.count());
      assertEqual([ 5, 6, 7, 8, 9 ], collection.toArray().map(fun).sort(nsort));

      collection.truncate();
      assertEqual(0, collection.count());
      assertEqual([ ], collection.toArray());

      for (i = 25; i < 35; ++i) {
        collection.save({ n : i });
      }

      assertEqual(5, collection.count());
      assertEqual([ 30, 31, 32, 33, 34 ], collection.toArray().map(fun).sort(nsort));

      collection.truncate();
      assertEqual(0, collection.count());

      testHelper.waitUnload(collection, true);

      assertEqual(0, collection.count());
      assertEqual([ ], collection.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: updates
////////////////////////////////////////////////////////////////////////////////

    testUpdates1 : function () {
      var idx = collection.ensureCapConstraint(5);
      var fun = function(d) { return d.n; };

      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(5, idx.size);
      assertEqual(0, idx.byteSize);

      assertEqual(0, collection.count());

      var docs = [ ];
      for (var i = 0;  i < 6;  ++i) {
        docs[i] = collection.save({ n : i });
      }

      assertEqual(5, collection.count());
      assertEqual([1, 2, 3, 4, 5], collection.toArray().map(fun).sort(nsort));

      collection.replace(docs[1], { n: 100 });
      assertEqual(5, collection.count());
      assertEqual([2, 3, 4, 5, 100], collection.toArray().map(fun).sort(nsort));

      collection.save({ n: 99 });
      assertEqual(5, collection.count());
      assertEqual([3, 4, 5, 99, 100], collection.toArray().map(fun).sort(nsort));

      collection.remove(docs[3]);
      assertEqual(4, collection.count());
      assertEqual([4, 5, 99, 100], collection.toArray().map(fun).sort(nsort));

      collection.save({ n: 98 });
      assertEqual(5, collection.count());
      assertEqual([4, 5, 98, 99, 100], collection.toArray().map(fun).sort(nsort));

      collection.save({ n: 97 });
      assertEqual(5, collection.count());
      assertEqual([5, 97, 98, 99, 100], collection.toArray().map(fun).sort(nsort));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: updates
////////////////////////////////////////////////////////////////////////////////

    testUpdates2 : function () {
      var idx = collection.ensureCapConstraint(3);
      var fun = function(d) { return d._key; };

      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(3, idx.size);
      assertEqual(0, idx.byteSize);

      collection.save({ _key: "foo" });
      collection.save({ _key: "bar" });
      collection.save({ _key: "baz" });

      assertEqual(3, collection.count());
      assertEqual([ "bar", "baz", "foo" ], collection.toArray().map(fun).sort());

      collection.replace("foo", { a : 1 }); // bar, baz, foo
      assertEqual(3, collection.count());
      assertEqual([ "bar", "baz", "foo" ], collection.toArray().map(fun).sort());

      collection.save({ _key: "bad" });     // baz, foo, bad
      assertEqual(3, collection.count());
      assertEqual([ "bad", "baz", "foo" ], collection.toArray().map(fun).sort());

      collection.replace("baz", { a : 1 }); // foo, bad, baz
      assertEqual(3, collection.count());
      assertEqual([ "bad", "baz", "foo" ], collection.toArray().map(fun).sort());

      collection.save({ _key: "bam" });     // bad, baz, bam
      assertEqual(3, collection.count());
      assertEqual([ "bad", "bam", "baz" ], collection.toArray().map(fun).sort());

      collection.save({ _key: "abc" });     // baz, bam, abc
      assertEqual(3, collection.count());
      assertEqual([ "abc", "bam", "baz" ], collection.toArray().map(fun).sort());

      testHelper.waitUnload(collection, true);

      assertEqual(3, collection.count());
      assertEqual([ "abc", "bam", "baz" ], collection.toArray().map(fun).sort());

      collection.save({ _key: "def" });     // bam, abc, def
      assertEqual(3, collection.count());
      assertEqual([ "abc", "bam", "def" ], collection.toArray().map(fun).sort());

      collection.save({ _key: "ghi" });     // abc, def, ghi
      assertEqual(3, collection.count());
      assertEqual([ "abc", "def", "ghi" ], collection.toArray().map(fun).sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cap with byteSize
////////////////////////////////////////////////////////////////////////////////

    testBytesize : function () {
      var idx = collection.ensureCapConstraint(0, 16384);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(0, idx.size);
      assertEqual(16384, idx.byteSize);

      idx = collection.ensureCapConstraint(0, 16384);

      assertEqual(id, idx.id);
      assertEqual("cap", idx.type);
      assertEqual(false, idx.isNewlyCreated);
      assertEqual(0, idx.size);
      assertEqual(16384, idx.byteSize);

      for (var i = 0; i < 1000; ++i) {
        collection.save({ "test" : "this is a test" });
      }

      assertTrue(collection.count() < 1000);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cap with count and byteSize
////////////////////////////////////////////////////////////////////////////////

    testCombo1 : function () {
      var idx = collection.ensureCapConstraint(50, 16384);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(50, idx.size);
      assertEqual(16384, idx.byteSize);

      idx = collection.ensureCapConstraint(50, 16384);

      assertEqual(id, idx.id);
      assertEqual("cap", idx.type);
      assertEqual(false, idx.isNewlyCreated);
      assertEqual(50, idx.size);
      assertEqual(16384, idx.byteSize);

      for (var i = 0; i < 1000; ++i) {
        collection.save({ "test" : "this is a test" });
      }

      assertEqual(50, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cap with count and byteSize
////////////////////////////////////////////////////////////////////////////////

    testCombo2 : function () {
      var idx = collection.ensureCapConstraint(1000, 16384);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(1000, idx.size);
      assertEqual(16384, idx.byteSize);

      idx = collection.ensureCapConstraint(1000, 16384);

      assertEqual(id, idx.id);
      assertEqual("cap", idx.type);
      assertEqual(false, idx.isNewlyCreated);
      assertEqual(1000, idx.size);
      assertEqual(16384, idx.byteSize);

      for (var i = 0; i < 1000; ++i) {
        collection.save({ "test" : "this is a test" });
      }

      assertTrue(collection.count() < 1000);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: too large document
////////////////////////////////////////////////////////////////////////////////

    testDocumentTooLarge1 : function () {
      collection.ensureCapConstraint(1, 16384);
      var doc = { };

      for (var i = 0; i < 1000; ++i) {
        doc["test" + i] = "this is a test for the too large document";
      }

      try {
        collection.save(doc);
        fail();
      }
      catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_TOO_LARGE.code ||
                   err.errorNum === ERRORS.ERROR_INTERNAL.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: too large document
////////////////////////////////////////////////////////////////////////////////

    testDocumentTooLarge2 : function () {
      collection.ensureCapConstraint(1000, 16384);
      var doc = { };

      for (var i = 0; i < 1000; ++i) {
        doc["test" + i] = "this is a test for the too large document";
      }

      try {
        collection.save(doc);
        fail();
      }
      catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_TOO_LARGE.code ||
                   err.errorNum === ERRORS.ERROR_INTERNAL.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: modification of collection size via updates
////////////////////////////////////////////////////////////////////////////////

    testSizeModificationSimple : function () {
      collection.ensureCapConstraint(1000, 16384);

      var doc = collection.save({ a : 1 });

      // cap should not be triggered here, but we want to see whether assertions fail
      doc = collection.replace(doc, { a : 2, b : 2 });
      assertEqual(1, collection.count());
      collection.truncate();
      assertEqual(0, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: modification of collection size via updates
////////////////////////////////////////////////////////////////////////////////

    testSizeModificationsMulti : function () {
      collection.ensureCapConstraint(1000, 16384);

      var doc = collection.save({ });
      var data = { };

      for (var i = 0; i < 100; ++i) {
        data["a" + i] = i;

        // cap should not be triggered here, but we want to see whether assertions fail
        doc = collection.replace(doc, data);
      }

      assertEqual(1, collection.count());

      collection.truncate();
      doc = null;
      testHelper.waitUnload(collection);

      assertEqual(0, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: modification of collection size via updates
////////////////////////////////////////////////////////////////////////////////

    testSizeModifications : function () {
      collection.ensureCapConstraint(2, 16384);

      var doc = collection.save({ _key: "test", a: 1 });
      var rev = doc._rev;
      collection.save({ a : 2 });

      var data = { };
      for (var i = 0; i < 1000; ++i) {
        data["b" + i] = "this document will really get too big...";
      }

      try {
        collection.update(doc, data);
        fail();
      }
      catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_TOO_LARGE.code ||
                   err.errorNum === ERRORS.ERROR_INTERNAL.code);
      }

      doc = null;
      testHelper.waitUnload(collection);

      assertEqual(2, collection.count());
      assertEqual(1, collection.document("test").a);
      assertEqual(rev, collection.document("test")._rev);

      collection.truncate();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: modification of collection size via updates
////////////////////////////////////////////////////////////////////////////////

    testSizeModificationsViolations : function () {
      collection.ensureCapConstraint(1, 16384);

      var doc = collection.save({ });
      var data = { };

      for (var i = 0; i < 10; ++i) {
        data["a" + i] = i;

        doc = collection.replace(doc, data);
      }

      assertEqual(1, collection.count());
      assertEqual(collection.toArray()[0].a9, 9);

      for (i = 0; i < 1000; ++i) {
        data["b" + i] = "this document will really get too big...";
      }

      try {
        collection.save(data);
        fail();
      }
      catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_TOO_LARGE.code ||
                   err.errorNum === ERRORS.ERROR_INTERNAL.code);
      }

      assertEqual(collection.toArray()[0].a9, 9);
      assertEqual(1, collection.count());

      doc = null;
      testHelper.waitUnload(collection);

      assertEqual(collection.toArray()[0].a9, 9);
      assertEqual(1, collection.count());

      collection.truncate();
      assertEqual(0, collection.count());
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CapConstraintSuite);

return jsunity.done();

