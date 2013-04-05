/*jslint indent: 2, nomen: true, maxlen: 80 */
/*global require, assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the AQL user functions management
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
var arangodb = require("org/arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;

var aqlfunctions = require("org/arangodb/aql/functions");

// -----------------------------------------------------------------------------
// --SECTION--                                          AQL user functions tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function AqlFunctionsSuite () {

  var unregister = function (name) {
    try {
      aqlfunctions.unregister(name);
    }
    catch (err) {
    }
  };

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
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterFunc1 : function () {
      unregister("UnitTests:tryme:foo");
      aqlfunctions.register("UnitTests:tryme:foo", function (what) { return what * 2; }, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterFunc2 : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", function (what) { return what * 2; }, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterString1 : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", "function (what) { return what * 2; }", true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterString2 : function () {
      unregister("UnitTests:tryme:foo");
      aqlfunctions.register("UnitTests:tryme:foo", "function (what) { return what * 2; }", true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief re-register a function
////////////////////////////////////////////////////////////////////////////////

    testReRegister : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", function (what) { return what * 2; }, true);
      aqlfunctions.register("UnitTests:tryme", function (what) { return what * 2; }, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function with an invalid name
////////////////////////////////////////////////////////////////////////////////

    testRegisterInvalidName1 : function () {
      try {
        aqlfunctions.register("foo", function (what) { return what * 2; }, true);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_INVALID_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function with an invalid name
////////////////////////////////////////////////////////////////////////////////

    testRegisterInvalidName2 : function () {
      try {
        aqlfunctions.register("_test", function (what) { return what * 2; }, true);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_INVALID_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function with an invalid name
////////////////////////////////////////////////////////////////////////////////

    testRegisterInvalidName3 : function () {
      try {
        aqlfunctions.register("_test:foo", function (what) { return what * 2; }, true);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_INVALID_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function with an invalid name
////////////////////////////////////////////////////////////////////////////////

    testRegisterInvalidName4 : function () {
      try {
        aqlfunctions.register("test:", function (what) { return what * 2; }, true);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_INVALID_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function with an invalid body
////////////////////////////////////////////////////////////////////////////////

    testRegisterInvalidCode1 : function () {
      unregister("UnitTests:tryme");
      try {
        aqlfunctions.register("UnitTests:tryme", "function (what) { ", true);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_INVALID_CODE.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function with an invalid body
////////////////////////////////////////////////////////////////////////////////

    testRegisterInvalidCode2 : function () {
      unregister("UnitTests:tryme");
      try {
        aqlfunctions.register("UnitTests:tryme", 1234, true);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_INVALID_CODE.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and unregister it
////////////////////////////////////////////////////////////////////////////////

    testUnregister : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", function (what) { return what * 4; }, true);
      aqlfunctions.unregister("UnitTests:tryme");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister an unknown function
////////////////////////////////////////////////////////////////////////////////

    testUnregisterUnknown : function () {
      unregister("UnitTests:tryme");

      try {
        aqlfunctions.unregister("UnitTests:tryme");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQuery1 : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", function (what) { return what * 2; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests:tryme(4)" }).execute().toArray();
      assertEqual([ 8 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQuery2 : function () {
      unregister("UnitTests:tryme");
      unregister("UnitTests:foo");
      aqlfunctions.register("UnitTests:tryme", function (what) { return what * 2; }, true);
      aqlfunctions.register("UnitTests:foo", function (what) { return what * 4; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests:tryme(4) + UnitTests:foo(9)" }).execute().toArray();
      assertEqual([ 4 * 2 + 9 * 4 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnUndefined : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", function (what) { }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests:tryme(4)" }).execute().toArray();
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnNan : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", function (what) { return 1 / 0; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests:tryme(4)" }).execute().toArray();
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnNull : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", function (what) { return null; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests:tryme(4)" }).execute().toArray();
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnTrue : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", function (what) { return true; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests:tryme(4)" }).execute().toArray();
      assertEqual([ true ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnFalse : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", function (what) { return false; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests:tryme(4)" }).execute().toArray();
      assertEqual([ false ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnComplex : function () {
      unregister("UnitTests:tryme");
      aqlfunctions.register("UnitTests:tryme", function (what) { return [ true, false, null, 1, 2, -4, [ 5.5, { a: 1, "b": "def" } ] ] }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests:tryme()" }).execute().toArray();
      assertEqual([ [ true, false, null, 1, 2, -4, [ 5.5, { a: 1, "b": "def" } ] ] ], actual);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AqlFunctionsSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
