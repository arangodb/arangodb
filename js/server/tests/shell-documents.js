////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, simple queries
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

var jsUnity = require("jsunity").jsUnity;

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
      assertEqual(1205, err.errorNum);
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
      assertEqual(1202, err.errorNum);
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
      assertEqual(1213, err.errorNum);
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
    var id = this.collection.save({ "Hallo" : "World" })._id;

    var doc = this.collection.document(id);

    assertEqual(id, doc._id);
    assertTypeOf("number", doc._rev);
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
      assertEqual(1200, err.errorNum);
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
      assertEqual(1205, err.errorNum);
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
      assertEqual(1202, err.errorNum);
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
    var id = this.collection.save({ "Hallo" : "World" })._id;

    var doc = db._document(id);

    assertEqual(id, doc._id);
    assertTypeOf("number", doc._rev);

    assertEqual(1, this.collection.count());
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
      assertEqual(1200, err.errorNum);
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

jsUnity.run(readCollectionDocumentSuiteErrorHandling);
jsUnity.run(readCollectionDocumentSuiteReadDocument);

jsUnity.run(readDocumentSuiteErrorHandling);
jsUnity.run(readDocumentSuiteReadDocument);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
