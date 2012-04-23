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

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function collectionDocumentSuiteErrorHandling () {
  var ERRORS = require("internal").errors;

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
      collection.drop();
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
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
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
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
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
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
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
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_NOT_FOUND.code, err.errorNum);
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
        assertEqual(ERRORS.ERROR_AVOCADO_CROSS_COLLECTION_REQUEST.code, err.errorNum);
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
        assertEqual(ERRORS.ERROR_AVOCADO_CROSS_COLLECTION_REQUEST.code, err.errorNum);
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
        assertEqual(ERRORS.ERROR_AVOCADO_CROSS_COLLECTION_REQUEST.code, err.errorNum);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function collectionDocumentSuite () {
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
        assertEqual(ERRORS.ERROR_AVOCADO_CONFLICT.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief udpate a document
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocument : function () {
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
        assertEqual(ERRORS.ERROR_AVOCADO_CONFLICT.code, err.errorNum);
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
        assertEqual(ERRORS.ERROR_AVOCADO_CONFLICT.code, err.errorNum);
      }

      var a3 = collection.remove(a1, true);

      assertEqual(a3, true);

      var a4 = collection.remove(a1, true);

      assertEqual(a4, false);
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
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_NOT_FOUND.code, err.errorNum);
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

function databaseDocumentSuiteErrorHandling () {
  var cn = "UnitTestsCollectionBasics";
  var ERRORS = require("internal").errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandle : function () {
      try {
        db._document("123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle replace
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleReplace : function () {
      try {
        db._replace("123456", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle delete
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleDelete : function () {
      try {
        db._remove("123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown document identifier
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingUnknownDocument : function () {
      var collection = db._create(cn, { waitForSync : false });

      try {
        db._document(collection._id + "/123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }

      collection.drop();
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function databaseDocumentSuite () {
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document
////////////////////////////////////////////////////////////////////////////////

    testReadDocument : function () {
      var d = collection.save({ "Hallo" : "World" });

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

    testReadDocumentConflict : function () {
      var d = collection.save({ "Hallo" : "World" });

      var doc = db._document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      var r = collection.replace(d, { "Hallo" : "You" });

      doc = db._document(r);

      assertEqual(d._id, doc._id);
      assertNotEqual(d._rev, doc._rev);

      try {
        db._document(d);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_CONFLICT.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief udpate a document
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = db._replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._replace(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_CONFLICT.code, err.errorNum);
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
/// @brief delete a document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      var a2 = db._replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_CONFLICT.code, err.errorNum);
      }

      var a3 = db._remove(a1, true);

      assertEqual(a3, true);

      var a4 = db._remove(a1, true);

      assertEqual(a4, false);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a deleted document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDeletedDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("number", a1._rev);

      db._remove(a1);

      try {
        db._remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_NOT_FOUND.code, err.errorNum);
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

jsunity.run(collectionDocumentSuiteErrorHandling);
jsunity.run(collectionDocumentSuite);

jsunity.run(databaseDocumentSuiteErrorHandling);
jsunity.run(databaseDocumentSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
