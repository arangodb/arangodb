/*jshint globalstrict:false, strict:false, sub: true */
/*global fail, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief very quick test for basic functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes
////////////////////////////////////////////////////////////////////////////////

function QuickieSuite () {
  'use strict';

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief quickly create a collection and do some operations:
////////////////////////////////////////////////////////////////////////////////

    testACollection: function () {

      try {
        db._drop("UnitTestCollection");
      }
      catch (e) {
      }

      // Create a collection:
      var c = db._create("UnitTestCollection", {numberOfShards:2});

      // Do a bunch of operations:
      var r = c.insert({"Hallo":12});
      var d = c.document(r._key);
      assertEqual(12, d.Hallo);
      c.replace(r._key, {"Hallo":13});
      d = c.document(r._key);
      assertEqual(13, d.Hallo);
      c.update(r._key, {"Hallo":14});
      d = c.document(r._key);
      assertEqual(14, d.Hallo);
      var aql = db._query("FOR x IN UnitTestCollection RETURN x._key").toArray();
      assertEqual(1, aql.length);
      assertEqual(r._key, aql[0]);
      var i = c.getIndexes();
      assertEqual(1, i.length); // We have a primary index
      c.ensureIndex({type: "hash", fields: ["Hallo"]});
      i = c.getIndexes();
      assertEqual(2, i.length); // We have a primary index and a hash Index
      aql = db._query("FOR x IN UnitTestCollection FILTER x.Hallo == 14 RETURN x._key").toArray();
      assertEqual(1, aql.length);
      assertEqual(r._key, aql[0]);
      c.remove(r._key);
      try {
        d = c.document(r._key);
        fail();
      }
      catch (e) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, e.errorNum);
      }

      // Drop the collection again:
      c.drop();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief quickly create a database and a collection and do some operations:
////////////////////////////////////////////////////////////////////////////////

    testADatabase: function () {
      try {
        db._dropDatabase("UnitTestDatabase");
      }
      catch (e) {
      }

      db._createDatabase("UnitTestDatabase");
      db._useDatabase("UnitTestDatabase");

      // Create a collection:
      var c = db._create("UnitTestCollection", {numberOfShards:2});

      // Do a bunch of operations:
      var r = c.insert({"Hallo":12});
      var d = c.document(r._key);
      assertEqual(12, d.Hallo);
      c.replace(r._key, {"Hallo":13});
      d = c.document(r._key);
      assertEqual(13, d.Hallo);
      c.update(r._key, {"Hallo":14});
      var aql = db._query("FOR x IN UnitTestCollection RETURN x._key").toArray();
      assertEqual(1, aql.length);
      assertEqual(r._key, aql[0]);
      var i = c.getIndexes();
      assertEqual(1, i.length); // We have a primary index
      c.ensureIndex({type: "hash", fields: ["Hallo"]});
      i = c.getIndexes();
      assertEqual(2, i.length); // We have a primary index and a hash Index
      aql = db._query("FOR x IN UnitTestCollection FILTER x.Hallo == 14RETURN x._key").toArray();
      assertEqual(1, aql.length);
      assertEqual(r._key, aql[0]);
      d = c.document(r._key);
      assertEqual(14, d.Hallo);
      c.remove(r._key);
      try {
        d = c.document(r._key);
        fail();
      }
      catch (e) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, e.errorNum); 
      }

      // Drop the collection again:
      c.drop();

      // Drop the database again:
      db._useDatabase("_system");
      db._dropDatabase("UnitTestDatabase");
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(QuickieSuite);

return jsunity.done();

