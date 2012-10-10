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
var internal = require("internal");
var console = require("console");

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function CollectionDocumentSuiteErrorHandling () {
  var ERRORS = internal.errors;

  var cn = "UnitTestsCollectionBasics";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { waitForSync : false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.unload();
      collection.drop();
      collection = null;
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandle : function () {
      try {
        collection.document("123456");
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
        collection.replace("123456", {});
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
        collection.remove("123456");
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
        collection.document(collection._id + "/123456");
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
        collection.document("123456/123456");
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
        collection.replace("123456/123456", {});
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
        collection.remove("123456/123456");
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
  var ERRORS = require("internal").errors;

  var cn = "UnitTestsCollectionBasics";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { waitForSync: false });

      collection.load();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      collection = null;
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document
////////////////////////////////////////////////////////////////////////////////

    testSaveDocument : function () {
      var doc = collection.save({ "Hallo" : "World" });

      assertTypeOf("string", doc._id);
      assertTypeOf("number", doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentSyncFalse : function () {
      var doc = collection.save({ "Hallo" : "World" }, false);

      assertTypeOf("string", doc._id);
      assertTypeOf("number", doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentSyncTrue : function () {
      var doc = collection.save({ "Hallo" : "World" }, true);

      assertTypeOf("string", doc._id);
      assertTypeOf("number", doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document
////////////////////////////////////////////////////////////////////////////////

    testReadDocument : function () {
      var d = collection.save({ "Hallo" : "World" });

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
      var d = collection.save({ "Hallo" : "World" });

      var doc = collection.document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      var r = collection.replace(d, { "Hallo" : "You" });

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
/// @brief replace a document
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

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
/// @brief replace a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = collection.replace(a1, { a : 2 }, true, false);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = collection.replace(a1, { a : 2 }, true, true);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

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
/// @brief delete a document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

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

      assertEqual(a3, true);

      var a4 = collection.remove(a1, true);

      assertEqual(a4, false);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = collection.update(a1, { a : 2 }, true, false);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = collection.update(a1, { a : 2 }, true, true);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = collection.remove(a1, true, false);
      assertEqual(a2, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = collection.remove(a1, true, true);
      assertEqual(a2, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a deleted document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDeletedDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      collection.remove(a1);

      try {
        collection.remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   vocbase methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function DatabaseDocumentSuiteErrorHandling () {
  var cn = "UnitTestsCollectionBasics";
  var ERRORS = require("internal").errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandle : function () {
      try {
        internal.db._document("123456");
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
        internal.db._replace("123456", {});
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
        internal.db._remove("123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown document identifier
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingUnknownDocument : function () {
      var collection = internal.db._create(cn, { waitForSync : false });

      try {
        internal.db._document(collection._id + "/123456");
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
  var cn = "UnitTestsCollectionBasics";
  var ERRORS = require("internal").errors;
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { waitForSync : false });

      collection.load();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document
////////////////////////////////////////////////////////////////////////////////

    testReadDocument : function () {
      var d = collection.save({ "Hallo" : "World" });

      var doc = internal.db._document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      doc = internal.db._document(d);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document with conflict
////////////////////////////////////////////////////////////////////////////////

    testReadDocumentConflict : function () {
      var d = collection.save({ "Hallo" : "World" });

      var doc = internal.db._document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      var r = collection.replace(d, { "Hallo" : "You" });

      doc = internal.db._document(r);

      assertEqual(d._id, doc._id);
      assertNotEqual(d._rev, doc._rev);

      try {
        internal.db._document(d);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = internal.db._replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        internal.db._replace(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = internal.db._document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = internal.db._replace(a1, { a : 4 }, true);

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = internal.db._document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = internal.db._update(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        internal.db._update(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = internal.db._document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = internal.db._update(a1, { a : 4 }, true);

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = internal.db._document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);
      
      var a5 = internal.db._update(a4, { b : 1, c : 2, d : "foo", e : null });
      assertEqual(a1._id, a5._id);
      assertNotEqual(a4._rev, a5._rev);
      
      var doc5 = internal.db._document(a1._id);
      assertEqual(a1._id, doc5._id);
      assertEqual(a5._rev, doc5._rev);
      assertEqual(4, doc5.a);
      assertEqual(1, doc5.b);
      assertEqual(2, doc5.c);
      assertEqual("foo", doc5.d);
      assertEqual(null, doc5.e);

      var a6 = internal.db._update(a5, { f : null, b : null, a : null, g : 2, c : 4 });
      assertEqual(a1._id, a6._id);
      assertNotEqual(a5._rev, a6._rev);
      
      var doc6 = internal.db._document(a1._id);
      assertEqual(a1._id, doc6._id);
      assertEqual(a6._rev, doc6._rev);
      assertEqual(null, doc6.a);
      assertEqual(null, doc6.b);
      assertEqual(4, doc6.c);
      assertEqual("foo", doc6.d);
      assertEqual(null, doc6.e);
      assertEqual(null, doc6.f);
      assertEqual(2, doc6.g);
      
      var a7 = internal.db._update(a6, { a : null, b : null, c : null, g : null }, true, false);
      assertEqual(a1._id, a7._id);
      assertNotEqual(a6._rev, a7._rev);
      
      var doc7 = internal.db._document(a1._id);
      assertEqual(a1._id, doc7._id);
      assertEqual(a7._rev, doc7._rev);
      assertEqual(undefined, doc7.a);
      assertEqual(undefined, doc7.b);
      assertEqual(undefined, doc7.c);
      assertEqual("foo", doc7.d);
      assertEqual(null, doc7.e);
      assertEqual(null, doc7.f);
      assertEqual(undefined, doc7.g);
      
      var a8 = internal.db._update(a7, { d : { "one" : 1, "two" : 2, "three" : 3 }, e : { }, f : { "one" : 1 }} );
      assertEqual(a1._id, a8._id);
      assertNotEqual(a7._rev, a8._rev);
      
      var doc8 = internal.db._document(a1._id);
      assertEqual(a1._id, doc8._id);
      assertEqual(a8._rev, doc8._rev);
      assertEqual({"one": 1, "two": 2, "three": 3}, doc8.d);
      assertEqual({}, doc8.e);
      assertEqual({"one": 1}, doc8.f);
      
      var a9 = internal.db._update(a8, { d : { "four" : 4 }, "e" : { "e1" : [ 1, 2 ], "e2" : 2 }, "f" : { "three" : 3 }} );
      assertEqual(a1._id, a9._id);
      assertNotEqual(a8._rev, a9._rev);
      
      var doc9 = internal.db._document(a1._id);
      assertEqual(a1._id, doc9._id);
      assertEqual(a9._rev, doc9._rev);
      assertEqual({"one": 1, "two": 2, "three": 3, "four": 4}, doc9.d);
      assertEqual({"e2": 2, "e1": [ 1, 2 ]}, doc9.e);
      assertEqual({"one": 1, "three": 3}, doc9.f);
      
      var a10 = internal.db._update(a9, { d : { "one" : -1, "two": null, "four" : null, "five" : 5 }, "e" : { "e1" : 1, "e2" : null, "e3" : 3 }}, true, false);
      assertEqual(a1._id, a10._id);
      assertNotEqual(a9._rev, a10._rev);
      
      var doc10 = internal.db._document(a1._id);
      assertEqual(a1._id, doc10._id);
      assertEqual(a10._rev, doc10._rev);
      assertEqual({"one": -1, "three": 3, "five": 5}, doc10.d);
      assertEqual({"e1": 1, "e3": 3}, doc10.e);
      assertEqual({"one": 1, "three": 3}, doc10.f);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = internal.db._replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        internal.db._remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var a3 = internal.db._remove(a1, true);

      assertEqual(a3, true);

      var a4 = internal.db._remove(a1, true);

      assertEqual(a4, false);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a deleted document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDeletedDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      internal.db._remove(a1);

      try {
        internal.db._remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionDocumentSuiteErrorHandling);
jsunity.run(CollectionDocumentSuite);

jsunity.run(DatabaseDocumentSuiteErrorHandling);
jsunity.run(DatabaseDocumentSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
