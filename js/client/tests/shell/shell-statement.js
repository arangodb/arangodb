/*jshint globalstrict:false, strict:false, maxlen:1000*/
/*global assertEqual, assertTrue, assertFalse, fail, more */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the statement class
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("@arangodb");
var db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: statements
////////////////////////////////////////////////////////////////////////////////

function StatementSuite () {
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
/// @brief test cursor
////////////////////////////////////////////////////////////////////////////////

    testCursor : function () {
      var stmt = db._createStatement({ query: "FOR i IN 1..11 RETURN i", count: true });
      var cursor = stmt.execute();

      assertEqual(11, cursor.count());
      for (var i = 1; i <= 11; ++i) {
        assertEqual(i, cursor.next());
        assertEqual(i !== 11, cursor.hasNext());
      }

      assertFalse(cursor.hasNext());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to string
////////////////////////////////////////////////////////////////////////////////

    testToString : function () {
      var stmt = db._createStatement({ query: "FOR i IN 1..11 RETURN i", count: true });
      var cursor = stmt.execute();

      assertEqual(11, cursor.count());
      assertTrue(cursor.hasNext());

      // print it. this should not modify the cursor apart from when it's accessed for printing
      cursor.toString();
      assertTrue(more === cursor);

      for (var i = 1; i <= 11; ++i) {
        assertEqual(i, cursor.next());
        assertEqual(i !== 11, cursor.hasNext());
      }

      assertFalse(cursor.hasNext());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test more
////////////////////////////////////////////////////////////////////////////////

    testMore : function () {
      var stmt = db._createStatement({ query: "FOR i IN 1..11 RETURN i", count: true });
      var cursor = stmt.execute();

      assertEqual(11, cursor.count());
      assertTrue(cursor.hasNext());

      // print it. this should not modify the cursor apart from when it's accessed for printing
      cursor.toString();
      assertTrue(more === cursor);

      cursor.toString();
      for (var i = 1; i <= 11; ++i) {
        assertEqual(i, cursor.next());
        assertEqual(i !== 11, cursor.hasNext());
      }

      assertFalse(cursor.hasNext());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test more
////////////////////////////////////////////////////////////////////////////////

    testMoreEof : function () {
      var stmt = db._createStatement({ query: "FOR i IN 1..11 RETURN i", count: true });
      var cursor = stmt.execute();

      assertEqual(11, cursor.count());
      assertTrue(cursor.hasNext());

      // print it. this should not modify the cursor apart from when it's accessed for printing
      cursor.toString();
      assertTrue(more === cursor);

      cursor.toString();
      try {
        cursor.toString();
        fail();
      } catch (err) {
        // we're expecting the cursor to be at the end
      }
      
      assertTrue(cursor.hasNext());
      for (var i = 1; i <= 11; ++i) {
        assertEqual(i, cursor.next());
        assertEqual(i !== 11, cursor.hasNext());
      }

      assertFalse(cursor.hasNext());
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(StatementSuite);
return jsunity.done();

