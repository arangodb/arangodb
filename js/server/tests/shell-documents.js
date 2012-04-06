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

function readCollectionDocumentSuiteErrorHandling () {
  var cn = "UnitTestsCollectionBasics";

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

  function testErrorHandlingBadHandle () {
    try {
      db[cn].document("123456");
      fail();
    }
    catch (err) {
      assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown document identifier
////////////////////////////////////////////////////////////////////////////////

  function testErrorHandlingUnknownDocument () {
    try {
      db[cn].document(db[cn]._id + "/123456");
      fail();
    }
    catch (err) {
      assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_NOT_FOUND.code, err.errorNum);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief cross collection
////////////////////////////////////////////////////////////////////////////////

  function testErrorHandlingCrossCollection () {
    try {
      db[cn].document("123456/123456");
      fail();
    }
    catch (err) {
      assertEqual(ERRORS.ERROR_AVOCADO_CROSS_COLLECTION_REQUEST.code, err.errorNum);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function readCollectionDocumentSuiteReadDocument () {
  var cn = "UnitTestsCollectionBasics";
  var collection = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  function setUp () {
    db._drop(cn);
    this.collection = db[cn];

    this.collection.load();
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  function tearDown () {
    this.collection.drop();
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document
////////////////////////////////////////////////////////////////////////////////

  function testSaveDocument () {
    var doc = this.collection.save({ "Hallo" : "World" });

    assertTypeOf("string", doc._id);
    assertTypeOf("number", doc._rev);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document
////////////////////////////////////////////////////////////////////////////////

  function testReadDocument () {
    var d = this.collection.save({ "Hallo" : "World" });

    var doc = this.collection.document(d._id);

    assertEqual(d._id, doc._id);
    assertEqual(d._rev, doc._rev);

    doc = this.collection.document(d);

    assertEqual(d._id, doc._id);
    assertEqual(d._rev, doc._rev);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document with conflict
////////////////////////////////////////////////////////////////////////////////

  function testReadDocumentConflict () {
    var d = this.collection.save({ "Hallo" : "World" });

    var doc = this.collection.document(d._id);

    assertEqual(d._id, doc._id);
    assertEqual(d._rev, doc._rev);

    var r = this.collection.replace(d, { "Hallo" : "You" });

    doc = this.collection.document(r);

    assertEqual(d._id, doc._id);
    assertNotEqual(d._rev, doc._rev);

    try {
      this.collection.document(d);
      fail();
    }
    catch (err) {
      assertEqual(ERRORS.ERROR_AVOCADO_CONFLICT.code, err.errorNum);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief udpate a document
////////////////////////////////////////////////////////////////////////////////

  function testUpdateDocument () {
    var a1 = this.collection.save({ a : 1});

    assertTypeOf("string", a1._id);
    assertTypeOf("number", a1._rev);

    var a2 = this.collection.replace(a1, { a : 2 });

    assertEqual(a1._id, a2._id);
    assertNotEqual(a1._rev, a2._rev);

    try {
      this.collection.replace(a1, { a : 3 });
      fail();
    }
    catch (err) {
      assertEqual(ERRORS.ERROR_AVOCADO_CONFLICT.code, err.errorNum);
    }

    var doc2 = this.collection.document(a1._id);

    assertEqual(a1._id, doc2._id);
    assertEqual(a2._rev, doc2._rev);
    assertEqual(2, doc2.a);

    var a4 = this.collection.replace(a2, { a : 4 }, true);

    assertEqual(a1._id, a4._id);
    assertNotEqual(a1._rev, a4._rev);
    assertNotEqual(a2._rev, a4._rev);

    var doc4 = this.collection.document(a1._id);

    assertEqual(a1._id, doc4._id);
    assertEqual(a4._rev, doc4._rev);
    assertEqual(4, doc4.a);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document
////////////////////////////////////////////////////////////////////////////////

  function testDeleteDocument () {
    var a1 = this.collection.save({ a : 1});

    assertTypeOf("string", a1._id);
    assertTypeOf("number", a1._rev);

    var a2 = this.collection.delete(a1);

    assertEqual(a2, true);

    try {
      this.collection.delete(a1);
      fail();
    }
    catch (err) {
      assertEqual(ERRORS.ERROR_AVOCADO_CONFLICT.code, err.errorNum);
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   vocbase methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function readDocumentSuiteErrorHandling () {
  var cn = "UnitTestsCollectionBasics";

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

  function testErrorHandlingBadHandle () {
    try {
      db._document("123456");
      fail();
    }
    catch (err) {
      assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown document identifier
////////////////////////////////////////////////////////////////////////////////

  function testErrorHandlingUnknownDocument () {
    try {
      db._document(db[cn]._id + "/123456");
      fail();
    }
    catch (err) {
      assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_NOT_FOUND.code, err.errorNum);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function readDocumentSuiteReadDocument () {
  var cn = "UnitTestsCollectionBasics";
  var collection = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  function setUp () {
    db._drop(cn);
    this.collection = db[cn];

    this.collection.load();
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  function tearDown () {
    this.collection.drop();
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document
////////////////////////////////////////////////////////////////////////////////

  function testReadDocument () {
    var d = this.collection.save({ "Hallo" : "World" });

    var doc = db._document(d._id);

    assertEqual(d._id, doc._id);
    assertEqual(d._rev, doc._rev);

    doc = db._document(d);

    assertEqual(d._id, doc._id);
    assertEqual(d._rev, doc._rev);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document with conflict
////////////////////////////////////////////////////////////////////////////////

  function testReadDocumentConflict () {
    var d = this.collection.save({ "Hallo" : "World" });

    var doc = db._document(d._id);

    assertEqual(d._id, doc._id);
    assertEqual(d._rev, doc._rev);

    var r = this.collection.replace(d, { "Hallo" : "You" });

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
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief udpate a document
////////////////////////////////////////////////////////////////////////////////

  function testUpdateDocument () {
    var a1 = this.collection.save({ a : 1});

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

    var a4 = db._replace(a2, { a : 4 }, true);

    assertEqual(a1._id, a4._id);
    assertNotEqual(a1._rev, a4._rev);
    assertNotEqual(a2._rev, a4._rev);

    var doc4 = db._document(a1._id);

    assertEqual(a1._id, doc4._id);
    assertEqual(a4._rev, doc4._rev);
    assertEqual(4, doc4.a);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

var context = {
  ERRORS : require("internal").errors
};

jsunity.run(readCollectionDocumentSuiteErrorHandling, context);
jsunity.run(readCollectionDocumentSuiteReadDocument, context);

jsunity.run(readDocumentSuiteErrorHandling, context);
jsunity.run(readDocumentSuiteReadDocument, context);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
