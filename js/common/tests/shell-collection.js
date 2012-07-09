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
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function CollectionSuiteErrorHandling () {
  var ERRORS = internal.errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (underscore)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameUnderscore : function () {
      try {
        internal.db._create("_illegal");
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
        internal.db._create("");
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
        internal.db._create("12345");
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
        internal.db["_illegal"];
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (empty) (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameEmptyShortCut : function () {
      try {
        internal.db[""];
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (number) (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameNumberShortCut : function () {
      try {
        internal.db["12345"];
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CollectionSuite () {
  var ERRORS = require("internal").errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief read by name
////////////////////////////////////////////////////////////////////////////////

    testReadingByName : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var c2 = internal.db._collection(cn);

      assertEqual(c1._id, c2._id);
      assertEqual(c1.name(), c2.name());
      assertEqual(c1.status(), c2.status());

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read by identifier
////////////////////////////////////////////////////////////////////////////////

    testReadingByIdentifier : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var c2 = internal.db._collection(c1._id);

      assertEqual(c1._id, c2._id);
      assertEqual(c1.name(), c2.name());
      assertEqual(c1.status(), c2.status());

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read by name (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testReadingByNameShortCut : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var c2 = internal.db[cn];

      assertEqual(c1._id, c2._id);
      assertEqual(c1.name(), c2.name());
      assertEqual(c1.status(), c2.status());

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read all
////////////////////////////////////////////////////////////////////////////////

    testReadingAll : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var l = internal.db._collections();

      assertNotEqual(0, l.length);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with defaults
////////////////////////////////////////////////////////////////////////////////

    testCreatingDefaults : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var p = c1.properties();

      assertEqual(false, p.waitForSync);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with properties
////////////////////////////////////////////////////////////////////////////////

    testCreatingProperties : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { waitForSync : true, journalSize : 1024 * 1024 });

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var p = c1.properties();

      assertEqual(true, p.waitForSync);

      if (p.journalSize < 1024 * 1024) {
        fail();
      }

      if (1024 * 1025 < p.journalSize) {
        fail();
      }

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop new-born
////////////////////////////////////////////////////////////////////////////////

    testDroppingNewBorn : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.drop();

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());

      var c2 = internal.db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop loaded
////////////////////////////////////////////////////////////////////////////////

    testDroppingLoaded : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.drop();

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());

      var c2 = internal.db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop unloaded
////////////////////////////////////////////////////////////////////////////////

    testDroppingUnloaded : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      c1.save({ a : 1 });
      c1.unload();

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.drop();

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());

      var c2 = internal.db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate new-born
////////////////////////////////////////////////////////////////////////////////

    testTruncatingNewBorn : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.truncate();

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(0, c1.count());

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate loaded
////////////////////////////////////////////////////////////////////////////////

    testTruncatingLoaded : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.truncate();

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(0, c1.count());

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate unloaded
////////////////////////////////////////////////////////////////////////////////

    testTruncatingUnloaded : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      c1.save({ a : 1 });
      c1.unload();

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.truncate();

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(0, c1.count());

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief figures
////////////////////////////////////////////////////////////////////////////////

    testFigures : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      c1.load();

      var f = c1.figures();

      assertEqual(0, f.datafiles.count);
      assertEqual(0, f.alive.count);
      assertEqual(0, f.alive.size);
      assertEqual(0, f.dead.count);
      assertEqual(0, f.dead.size);
      assertEqual(0, f.dead.deletion);

      var d1 = c1.save({ hallo : 1 });

      f = c1.figures();

      assertEqual(0, f.datafiles.count);
      assertEqual(0, f.datafiles.fileSize);
      assertEqual(1, f.journals.count);
      assertTrue(f.journals.fileSize > 0);
      assertEqual(1, f.alive.count);
      assertNotEqual(0, f.alive.size);
      assertEqual(0, f.dead.count);
      assertEqual(0, f.dead.size);
      assertEqual(0, f.dead.deletion);

      var d2 = c1.save({ hallo : 2 });

      f = c1.figures();

      assertEqual(0, f.datafiles.count);
      assertEqual(0, f.datafiles.fileSize);
      assertEqual(1, f.journals.count);
      assertTrue(f.journals.fileSize > 0);
      assertEqual(2, f.alive.count);
      assertNotEqual(0, f.alive.size);
      assertEqual(0, f.dead.count);
      assertEqual(0, f.dead.size);
      assertEqual(0, f.dead.deletion);

      c1.remove(d1);

      f = c1.figures();

      assertEqual(0, f.datafiles.count);
      assertEqual(0, f.datafiles.fileSize);
      assertEqual(1, f.journals.count);
      assertTrue(f.journals.fileSize > 0);
      assertEqual(1, f.alive.count);
      assertNotEqual(0, f.alive.size);
      assertEqual(1, f.dead.count);
      assertNotEqual(0, f.dead.size);
      assertEqual(1, f.dead.deletion);

      c1.remove(d2);

      f = c1.figures();

      assertEqual(0, f.datafiles.count);
      assertEqual(0, f.datafiles.fileSize);
      assertEqual(1, f.journals.count);
      assertTrue(f.journals.fileSize > 0);
      assertEqual(0, f.alive.count);
      assertEqual(0, f.alive.size);
      assertEqual(2, f.dead.count);
      assertNotEqual(0, f.dead.size);
      assertEqual(2, f.dead.deletion);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rename loaded collection
////////////////////////////////////////////////////////////////////////////////

    testRenameLoaded : function () {
      var cn = "example";
      var nn = "example2";

      internal.db._drop(cn);
      internal.db._drop(nn);
      var c1 = internal.db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var id = c1._id;

      c1.rename(nn);

      assertEqual(id, c1._id);
      assertEqual(nn, c1.name());
      assertTypeOf("number", c1.status());

      var c2 = internal.db._collection(cn);

      assertEqual(null, c2);

      internal.db._drop(nn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rename unloaded collection
////////////////////////////////////////////////////////////////////////////////

    testRenameUnloaded : function () {
      var cn = "example";
      var nn = "example2";

      internal.db._drop(cn);
      internal.db._drop(nn);
      var c1 = internal.db._create(cn);

      c1.save({ a : 1 });
      c1.unload();

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var id = c1._id;

      c1.rename(nn);

      assertEqual(id, c1._id);
      assertEqual(nn, c1.name());
      assertTypeOf("number", c1.status());

      var c2 = internal.db._collection(cn);

      assertEqual(null, c2);

      internal.db._drop(nn);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
// --SECTION--                                                   vocbase methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CollectionDbSuite () {
  var ERRORS = require("internal").errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief drop new-born (DB)
////////////////////////////////////////////////////////////////////////////////

    testDroppingNewBornDB : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      internal.db._drop(cn);

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());

      var c2 = internal.db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop loaded (DB)
////////////////////////////////////////////////////////////////////////////////

    testDroppingLoadedDB : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      internal.db._drop(cn);

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());

      var c2 = internal.db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop unloaded (DB)
////////////////////////////////////////////////////////////////////////////////

    testDroppingUnloadedDB : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      c1.save({ a : 1 });
      c1.unload();

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      internal.db._drop(cn);

      assertEqual(ArangoCollection.STATUS_DELETED, c1.status());

      var c2 = internal.db._collection(cn);

      assertEqual(null, c2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate new-born (DB)
////////////////////////////////////////////////////////////////////////////////

    testTruncatingNewBornDB : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      internal.db._truncate(cn);

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(0, c1.count());

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate loaded (DB)
////////////////////////////////////////////////////////////////////////////////

    testTruncatingLoadedDB : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      internal.db._truncate(cn);

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(0, c1.count());

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate unloaded (DB)
////////////////////////////////////////////////////////////////////////////////

    testTruncatingUnloadedDB : function () {
      var cn = "example";

      internal.db._drop(cn);
      var c1 = internal.db._create(cn);

      c1.save({ a : 1 });
      c1.unload();

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      internal.db._truncate(cn);

      assertEqual(ArangoCollection.STATUS_LOADED, c1.status());
      assertEqual(0, c1.count());

      internal.db._drop(cn);
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionSuiteErrorHandling);
jsunity.run(CollectionSuite);
jsunity.run(CollectionDbSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

