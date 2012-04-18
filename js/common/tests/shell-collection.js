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

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function collectionSuiteErrorHandling () {
  var ERRORS = require("internal").errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (underscore)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameUnderscore : function () {
      try {
        db._create("_illegal");
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (empty)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameEmpty : function () {
      try {
        db._create("");
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (number)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameNumber : function () {
      try {
        db._create("12345");
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (underscore) (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameUnderscoreShortCut : function () {
      try {
        db["_illegal"];
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (empty) (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameEmptyShortCut : function () {
      try {
        db[""];
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad name (number) (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadNameNumberShortCut : function () {
      try {
        db["12345"];
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_ILLEGAL_NAME.code, err.errorNum);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function collectionSuite () {
  var ERRORS = require("internal").errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief read by name
////////////////////////////////////////////////////////////////////////////////

    testReadingByName : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("number", c1._id);
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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var c2 = db._collection(c1._id);

      assertEqual(c1._id, c2._id);
      assertEqual(c1.name(), c2.name());
      assertEqual(c1.status(), c2.status());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read by name (short-cut)
////////////////////////////////////////////////////////////////////////////////

    testReadingByNameShortCut : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var c2 = db[cn];

      assertEqual(c1._id, c2._id);
      assertEqual(c1.name(), c2.name());
      assertEqual(c1.status(), c2.status());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read all
////////////////////////////////////////////////////////////////////////////////

    testReadingAll : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var l = db._collections();

      assertNotEqual(0, l.length);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with defaults
////////////////////////////////////////////////////////////////////////////////

    testCreatingDefaults : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var p = c1.properties();

      assertEqual(false, p.waitForSync);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creating with properties
////////////////////////////////////////////////////////////////////////////////

    testCreatingProperties : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn, { waitForSync : true, journalSize : 1024 * 1024 });

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

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop new-born
////////////////////////////////////////////////////////////////////////////////

    testDroppingNewBorn : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.drop();

      assertEqual(AvocadoCollection.STATUS_DELETED, c1.status());

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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.drop();

      assertEqual(AvocadoCollection.STATUS_DELETED, c1.status());

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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.drop();

      assertEqual(AvocadoCollection.STATUS_DELETED, c1.status());

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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.truncate();

      assertEqual(AvocadoCollection.STATUS_LOADED, c1.status());
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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.truncate();

      assertEqual(AvocadoCollection.STATUS_LOADED, c1.status());
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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      c1.truncate();

      assertEqual(AvocadoCollection.STATUS_LOADED, c1.status());
      assertEqual(0, c1.count());

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief figures
////////////////////////////////////////////////////////////////////////////////

    testFigures : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

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

      assertEqual(1, f.datafiles.count);
      assertEqual(1, f.alive.count);
      assertNotEqual(0, f.alive.size);
      assertEqual(0, f.dead.count);
      assertEqual(0, f.dead.size);
      assertEqual(0, f.dead.deletion);

      var d2 = c1.save({ hallo : 2 });

      f = c1.figures();

      assertEqual(1, f.datafiles.count);
      assertEqual(2, f.alive.count);
      assertNotEqual(0, f.alive.size);
      assertEqual(0, f.dead.count);
      assertEqual(0, f.dead.size);
      assertEqual(0, f.dead.deletion);

      c1.remove(d1);

      f = c1.figures();

      assertEqual(1, f.datafiles.count);
      assertEqual(1, f.alive.count);
      assertNotEqual(0, f.alive.size);
      assertEqual(1, f.dead.count);
      assertNotEqual(0, f.dead.size);
      assertEqual(1, f.dead.deletion);

      c1.remove(d2);

      f = c1.figures();

      assertEqual(1, f.datafiles.count);
      assertEqual(0, f.alive.count);
      assertEqual(0, f.alive.size);
      assertEqual(2, f.dead.count);
      assertNotEqual(0, f.dead.size);
      assertEqual(2, f.dead.deletion);

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rename loaded collection
////////////////////////////////////////////////////////////////////////////////

    testRenameLoaded : function () {
      var cn = "example";
      var nn = "example2";

      db._drop(cn);
      db._drop(nn);
      var c1 = db._create(cn);

      c1.save({ a : 1 });

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      var id = c1._id;

      c1.rename(nn);

      assertEqual(id, c1._id);
      assertEqual(nn, c1.name());
      assertTypeOf("number", c1.status());

      var c2 = db._collection(cn);

      assertEqual(null, c2);

      db._drop(nn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rename unloaded collection
////////////////////////////////////////////////////////////////////////////////

    testRenameUnloaded : function () {
      var cn = "example";
      var nn = "example2";

      db._drop(cn);
      db._drop(nn);
      var c1 = db._create(cn);

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

      var c2 = db._collection(cn);

      assertEqual(null, c2);

      db._drop(nn);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
// --SECTION--                                                   vocbase methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function collectionDbSuite () {
  var ERRORS = require("internal").errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief drop new-born (DB)
////////////////////////////////////////////////////////////////////////////////

    testDroppingNewBornDB : function () {
      var cn = "example";

      db._drop(cn);
      var c1 = db._create(cn);

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      db._drop(cn);

      assertEqual(AvocadoCollection.STATUS_DELETED, c1.status());

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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      db._drop(cn);

      assertEqual(AvocadoCollection.STATUS_DELETED, c1.status());

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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      db._drop(cn);

      assertEqual(AvocadoCollection.STATUS_DELETED, c1.status());

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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      db._truncate(cn);

      assertEqual(AvocadoCollection.STATUS_LOADED, c1.status());
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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      db._truncate(cn);

      assertEqual(AvocadoCollection.STATUS_LOADED, c1.status());
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

      assertTypeOf("number", c1._id);
      assertEqual(cn, c1.name());
      assertTypeOf("number", c1.status());

      db._truncate(cn);

      assertEqual(AvocadoCollection.STATUS_LOADED, c1.status());
      assertEqual(0, c1.count());

      db._drop(cn);
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(collectionSuiteErrorHandling);
jsunity.run(collectionSuite);
jsunity.run(collectionDbSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

