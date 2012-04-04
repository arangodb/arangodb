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
// --SECTION--                                            db.collection.document
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
    assertException(function() {
                      db[cn].document("123456");
                    });
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown document identifier
////////////////////////////////////////////////////////////////////////////////

  function testErrorHandlingUnknownDocument () {
    assertException(function() {
                      db[cn].document(db[cn]._id + "/123456");
                    });
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief cross collection
////////////////////////////////////////////////////////////////////////////////

  function testErrorHandlingCrossCollection () {
    assertException(function() {
                      db[cn].document("123456/123456");
                    });
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: reading
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
/// @brief read a document
////////////////////////////////////////////////////////////////////////////////

  function testReadDocument () {
    var id = this.collection.save({ "Hallo" : "World" });

    var doc = this.collection.document(id);

    assertEqual(id, doc._id);
    assertTypeOf("number", doc._rev);

    assertEqual(1, this.collection.count());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      db._document
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
    assertException(function() {
                      db._document("123456");
                    });
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown document identifier
////////////////////////////////////////////////////////////////////////////////

  function testErrorHandlingUnknownDocument () {
    assertException(function() {
                      db_.document(db[cn]._id + "/123456");
                    });
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: reading
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
    var id = this.collection.save({ "Hallo" : "World" });

    var doc = this_.document(id);

    assertEqual(id, doc._id);
    assertTypeOf("number", doc._rev);

    assertEqual(1, this.collection.count());
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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
