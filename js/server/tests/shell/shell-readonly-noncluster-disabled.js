/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for readonly mode of arango
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

var jsunity = require("jsunity");
var arangodb = require("@arangodb");


////////////////////////////////////////////////////////////////////////////////
/// @brief single tests when server is running in ReadOnly mode
////////////////////////////////////////////////////////////////////////////////

function databaseTestSuite () {
  'use strict';
  var db = require("internal").db;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._changeMode("Normal");
      try {
        db._dropDatabase("testDB");
      }
      catch (e) {
        // ignore any errors
      }

      db._createDatabase("testDB");
      db._changeMode("NoCreate");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._changeMode("Normal");
      try {
        db._dropDatabase("testDB");
      } 
      catch (e) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: creation of a database in read only mode should fail
////////////////////////////////////////////////////////////////////////////////

    testCreateDatabase: function () {
      try {
        db._createDatabase("xxxDB");
        fail();
      } 
      catch (e) {
        assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: dropping of a database in read only mode should be possible
////////////////////////////////////////////////////////////////////////////////

    testDropDatabase: function () {
      try {
        db._dropDatabase("testDB");
        assertEqual(-1, db._databases().indexOf("testDb"));
      }
      catch (e) {
        fail();
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief single tests when server is running in ReadOnly mode
////////////////////////////////////////////////////////////////////////////////

function operationsTestSuite () {
  'use strict';
  var db = require("internal").db;
  var collection;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop("testCol");
      collection = db._create("testCol");
      collection.save({_key: "testDocKey", a: 2 });
      db._changeMode("NoCreate");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._changeMode("Normal");
      collection.drop();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: creating a collection
////////////////////////////////////////////////////////////////////////////////

    testCreateCollection: function () {
      db._drop("abcDC");

      try {
        db._create("abcDC");
        fail();
      } 
      catch (e) {
        assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: dropping a collection
////////////////////////////////////////////////////////////////////////////////

    testDropCollection: function () {
      // should not throw any errors
      db._drop("testCol");
      assertTrue(true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: insert
////////////////////////////////////////////////////////////////////////////////

    testInsertDocument: function () {
      try {
        collection.save({a:1, b:2});  
        fail();
      } 
      catch (e) {
        assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: update
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocument: function () {
      try {
        collection.update("testDocKey", {name: "testing", a: 3});
        fail();
      } 
      catch (e) {
        assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocument: function () {
      try {
        collection.replace("testDocKey", {name: "testing", a: 3});
        fail();
      } 
      catch (e) {
        assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveDocument: function () {
      try {
        collection.remove("testDocKey");
        fail();
      } 
      catch (e) {
        assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: create index
////////////////////////////////////////////////////////////////////////////////

    testCreateHashIndex: function () {
      try {
        collection.ensureIndex({ type: "hash", fields: [ "name" ], unique: true });
        fail();
      } 
      catch (e) {
        assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: create index
////////////////////////////////////////////////////////////////////////////////

    testCreateGeoIndex: function () {
      try {
        collection.ensureIndex({ type: "geo1", fields: [ "x" ], unique: true });
        fail();
      } 
      catch (e) {
        assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: create index
////////////////////////////////////////////////////////////////////////////////

    testCreateGeo2Index: function () {
      try {
        collection.ensureIndex({ type: "geo2", fields: [ "x", "y" ], unique: true });
        fail();
      } 
      catch (e) {
        assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: create index
////////////////////////////////////////////////////////////////////////////////

    testCreateSkipListIndex: function () {
      try {
        collection.ensureIndex({ type: "skiplist", fields: [ "x" ]});
        fail();
      } 
      catch (e) {
        assertEqual(arangodb.ERROR_ARANGO_READ_ONLY, e.errorNum);
      }
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(databaseTestSuite);
jsunity.run(operationsTestSuite);

return jsunity.done();


