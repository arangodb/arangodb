/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertEqual, assertNull, assertTypeOf, assertNotEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the view interface
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
/// @author Daniel H. Larkin
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var fs = require('fs');
var ArangoView = arangodb.ArangoView;
var testHelper = require("@arangodb/test-helper").Helper;
var db = arangodb.db;
var ERRORS = arangodb.errors;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: view
////////////////////////////////////////////////////////////////////////////////

function ViewSuite () {
  'use strict';
  return {

    ////////////////////////////////////////////////////////////////////////////
    /// @brief rename (duplicate)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingRenameDuplicate : function () {
      try {
        db._createView("abc", "arangosearch", {});
        var v = db._createView("def", "arangosearch", {});
        v.rename("abc");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, err.errorNum);
        var abc = db._view("abc");
        abc.drop();
        var def = db._view("def");
        def.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief rename (illegal)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingRenameIllegal : function () {
      try {
        var v = db._createView("abc", "arangosearch", {});
        v.rename("@bc!");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        var abc = db._view("abc");
        abc.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief rename view
    ////////////////////////////////////////////////////////////////////////////
    testRename : function () {
      var abc = db._createView("abc", "arangosearch", {});
      assertEqual(abc.name(), "abc");

      abc.rename("def");
      assertEqual(abc.name(), "def");

      abc.rename("def");
      assertEqual(abc.name(), "def");

      abc.rename("abc");
      assertEqual(abc.name(), "abc");

      abc.drop();
    },

    ////////////////////////////////////////////////////////////////////////////////
    ///// @brief test a view directory deletion after a DB drop
    //////////////////////////////////////////////////////////////////////////////////

    testViewDirInFSAfterDatabaseDrop : function () {
      var serverPath = db._engine().name === "rocksdb" ? fs.join(db._path(), "databases") : "";

      db._createDatabase("testViewDirInFSAfterDatabaseDrop");
      db._useDatabase("testViewDirInFSAfterDatabaseDrop");

      var dbPath = serverPath === "" ? db._path() : fs.join(serverPath, "database-" + db._id());

      db._create("col1");
      var v1 = db._createView("view1", "arangosearch", {});
      var indexDirPath = fs.join(dbPath, "arangosearch-" + v1._id);

      assertTrue(fs.exists(indexDirPath));
      assertTrue(fs.isDirectory(indexDirPath));

      v1.properties({links: {col1: {includeAllFields: true}}});

      db._useDatabase("_system");
      db._dropDatabase("testViewDirInFSAfterDatabaseDrop");

      assertFalse(fs.exists(indexDirPath));
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ViewSuite);

return jsunity.done();
