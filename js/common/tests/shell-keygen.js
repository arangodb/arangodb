////////////////////////////////////////////////////////////////////////////////
/// @brief test the key generators
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("org/arangodb");

var ArangoCollection = arangodb.ArangoCollection;
var db = arangodb.db;
var ERRORS = arangodb.errors;

// -----------------------------------------------------------------------------
// --SECTION--                                      auto-increment key generator
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: auto-increment
////////////////////////////////////////////////////////////////////////////////

function AutoIncrementSuite () {
  var cn = "UnitTestsKeyGen";

  return {

    setUp : function () {
      db._drop(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid offset
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidOffset : function () {
      try {
        db._create(cn, { createOptions: { keys: { type: "autoincrement", offset: -1 } } });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid increment
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidIncrement1 : function () {
      try {
        db._create(cn, { createOptions: { keys: { type: "autoincrement", increment: 0 } } });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid increment
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidIncrement2 : function () {
      try {
        db._create(cn, { createOptions: { keys: { type: "autoincrement", increment: -1 } } });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid increment
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidIncrement2 : function () {
      try {
        db._create(cn, { createOptions: { keys: { type: "autoincrement", increment: 9999999999999 } } });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk1 : function () {
      var c = db._create(cn, { createOptions: { keys: { type: "autoincrement", offset: 12345678901234567 } } });

      var options = c.properties().createOptions;
      assertEqual("autoincrement", options.keys.type);
      assertEqual(12345678901234567, options.keys.offset);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk2 : function () {
      var c = db._create(cn, { createOptions: { keys: { type: "autoincrement" } } });

      var options = c.properties().createOptions;
      assertEqual("autoincrement", options.keys.type);
    },

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AutoIncrementSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

