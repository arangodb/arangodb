////////////////////////////////////////////////////////////////////////////////
/// @brief tests for routing
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
/// @author Esteban Lombeyda
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var jsunity = require("jsunity");
var arangodb = require("org/arangodb");

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief single tests when server is running in ReadOnly mode
////////////////////////////////////////////////////////////////////////////////
function dropDatabaseTestSuite () {
  var db;
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db = require("internal").db;
      db._changeMode("Normal");
      db._createDatabase("testDB");
      db._changeMode("ReadOnly");
    },
          tearDown : function () {
            try {
              db._changeMode("Normal");
              db._dropDatabase("testDB");
            } catch (e) {
            }
          },

          ////////////////////////////////////////////////////////////////////////////////
          /// @brief test: creation of a data base in read only modes should faill
          ////////////////////////////////////////////////////////////////////////////////

          testCreateDatabase: function () {
            var not_modified = false;
            try {
              db._createDatabase("xxxDB");
            }catch (e) {
              not_modified = true;
              assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
            }
            assertTrue(not_modified);
          },

          ////////////////////////////////////////////////////////////////////////////////
          /// @brief test: droping of a data base in read only mode should be posible
          ////////////////////////////////////////////////////////////////////////////////

          testDropDatabase: function () {
            var modified = false;
            try {
              db._dropDatabase("testDB");
              modified = true;
            }catch (e) {
              not_modified = false;
            }
            assertTrue(modified);
          },
  };
}
////////////////////////////////////////////////////////////////////////////////
/// @brief single tests when server is running in ReadOnly mode
////////////////////////////////////////////////////////////////////////////////
function readOnlyDatabaseSuite () {
  var db;
  var collection;
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db = require("internal").db;
      collection = db._createDocumentCollection("testCol");
      collection.save({_key: "testDocKey", a: 2 });
      db._changeMode("ReadOnly");
    },
          tearDown : function () {
            try {
              db._changeMode("Normal");
              collection.drop("testCol");
            } catch (e) {
             print(e); 
            }
          },

          ////////////////////////////////////////////////////////////////////////////////
          /// @brief test: simple routing (prefix)
          ////////////////////////////////////////////////////////////////////////////////

          testCreateDocumentCollection: function () {
            var not_modified = false;
            try {
              db._createDocumentCollection("abcDC");
            }catch (e) {
              not_modified = true;
              assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
            }
            assertTrue(not_modified);
          },

          ////////////////////////////////////////////////////////////////////////////////
          /// @brief test: simple routing (parameter)
          ////////////////////////////////////////////////////////////////////////////////

          testDropCollection: function () {
            var not_modified = false;
            try {
              db._createDocumentCollection("xxxDC");
              not_modified = true;
            }catch (e) {
            }
            assertFalse(not_modified);
          },

          ////////////////////////////////////////////////////////////////////////////////
          /// @brief test: simple routing (optional)
          ////////////////////////////////////////////////////////////////////////////////

          testInsertDocument: function () {
            var modified = false;
            try {
            collection.save({a:1, b:2});  
              modified = true;
            }catch (e) {
              assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
            }
            assertFalse(modified);
          },

          ////////////////////////////////////////////////////////////////////////////////
          /// @brief test: simple routing (optional)
          ////////////////////////////////////////////////////////////////////////////////

          testUpdateDocument: function () {
            var modified = false;
            try {
            collection.update("testDocKey", {name: "testing", a: 3});
              modified = true;
            }catch (e) {
              assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
            }
            assertFalse(modified);
          },

          ////////////////////////////////////////////////////////////////////////////////
          /// @brief test: simple routing (prefix vs non-prefix)
          ////////////////////////////////////////////////////////////////////////////////

          testRemoveDocument: function () {
            var modified = false;
            try {
            var removed = collection.remove("testDocKey");
            assertFalse(removed);
              modified = true;
            }catch (e) {
              assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
            }
            assertFalse(modified);
          },

          ////////////////////////////////////////////////////////////////////////////////
          /// @brief test: content string
          ////////////////////////////////////////////////////////////////////////////////

          testCreateBitArrayIndex: function () {
            var modified = false;
            try {
            collection.ensureBitarray("a", [1,2,3,4]);
            modified = true;
            }catch (e) {
              assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
            }
            assertFalse(modified);
          },
          testCreateHashIndex: function () {
            var modified = false;
            try {
              collection.ensureIndex({ type: "hash", fields: [ "name" ], unique: true });
              modified = true;
            }catch (e) {
              assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
            }
            assertFalse(modified);
          },
          testCreateGeoIndex: function () {
            var modified = false;
            try {
              collection.ensureIndex({ type: "geo1", fields: [ "x" ], unique: true });
              modified = true;
            }catch (e) {
              assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
            }
            assertFalse(modified);
          },
          testCreateGeo2Index: function () {
            var modified = false;
            try {
              collection.ensureIndex({ type: "geo2", fields: [ "x", "y" ], unique: true });
              modified = true;
            }catch (e) {
              assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
            }
            assertFalse(modified);
          },
          testCreateSkipListIndex: function () {
            var modified = false;
            try {
              collection.ensureIndex({ type: "skiplist", fields: [ "x" ]});
              modified = true;
            }catch (e) {
              assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
            }
            assertFalse(modified);
          },

  };
}


// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(dropDatabaseTestSuite);
jsunity.run(readOnlyDatabaseSuite);

return jsunity.done();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
