/*jshint globalstrict:false, strict:false, maxlen: 200, unused: false*/
/*global assertEqual, fail */

/* unused for functions with 'what' parameter.*/
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
var arangodb = require("@arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;

var aqlfunctions = require("@arangodb/aql/functions");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function AqlFunctionsSuite () {
  'use strict';
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
      db._useDatabase("_system");
      aqlfunctions.unregisterGroup("UnitTests::");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._useDatabase("_system");
      aqlfunctions.unregisterGroup("UnitTests::");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multiple databases
////////////////////////////////////////////////////////////////////////////////

    testDatabases1 : function () {
      aqlfunctions.register("UnitTests::tryme::foo", function (what) { return what * 2; }, true);

      try {
        db._dropDatabase("UnitTestsAhuacatl");
      }
      catch (err) {
      }

      try {
        db._createDatabase("UnitTestsAhuacatl");
        db._useDatabase("UnitTestsAhuacatl");
      }
      catch (err) {
      }

      var actual;

      aqlfunctions.register("UnitTests::tryme::foo", function (what) { return what * 4; }, true);
      assertEqual("function (what) { return what * 4; }", aqlfunctions.toArray("UnitTests")[0].code);

      actual = db._createStatement({ query: "RETURN UnitTests::tryme::foo(4)" }).execute().toArray();
      assertEqual([ 16 ], actual);

      db._useDatabase("_system");
      assertEqual("function (what) { return what * 2; }", aqlfunctions.toArray("UnitTests")[0].code);
      actual = db._createStatement({ query: "RETURN UnitTests::tryme::foo(4)" }).execute().toArray();
      assertEqual([ 8 ], actual);

      try {
        db._useDatabase("_system");
        db._dropDatabase("UnitTestsAhuacatl");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multiple databases
////////////////////////////////////////////////////////////////////////////////

    testDatabases2 : function () {
      try {
        unregister("UnitTests::tryme");
      }
      catch (err) {
      }

      try {
        db._dropDatabase("UnitTestsAhuacatl");
      }
      catch (err) {
      }

      try {
        db._createDatabase("UnitTestsAhuacatl");
        db._useDatabase("UnitTestsAhuacatl");
      }
      catch (err) {
      }

      var actual;

      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 3; }, true);
      assertEqual("function (what) { return what * 3; }", aqlfunctions.toArray("UnitTests")[0].code);

      actual = db._createStatement({ query: "RETURN UnitTests::tryme(3)" }).execute().toArray();
      assertEqual([ 9 ], actual);

      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 4; }, true);
      assertEqual("function (what) { return what * 4; }", aqlfunctions.toArray("UnitTests")[0].code);
      actual = db._createStatement({ query: "RETURN UnitTests::tryme(3)" }).execute().toArray();
      assertEqual([ 12 ], actual);

      db._useDatabase("_system");
      assertEqual(0, aqlfunctions.toArray("UnitTests").length);
      try {
        db._createStatement({ query: "RETURN UnitTests::tryme(4)" }).execute().toArray();
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_NOT_FOUND.code, err.errorNum);
      }

      try {
        db._useDatabase("_system");
        db._dropDatabase("UnitTestsAhuacatl");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief toArray
////////////////////////////////////////////////////////////////////////////////

    testToArray1 : function () {
      aqlfunctions.register("UnitTests::tryme::foo", function (what) { return what * 2; }, true);
      aqlfunctions.register("UnitTests::tryme::bar", function (what) { return what * 2; }, true);

      assertEqual([ "UnitTests::tryme::bar", "UnitTests::tryme::foo" ], aqlfunctions.toArray("UnitTests").map(function (f) { return f.name; }).sort());
      assertEqual([ "UnitTests::tryme::bar", "UnitTests::tryme::foo" ], aqlfunctions.toArray("UnitTests::").map(function (f) { return f.name; }).sort());
      assertEqual([ "UnitTests::tryme::bar", "UnitTests::tryme::foo" ], aqlfunctions.toArray("UnitTests::tryme").map(function (f) { return f.name; }).sort());
      assertEqual([ "UnitTests::tryme::bar", "UnitTests::tryme::foo" ], aqlfunctions.toArray("UnitTests::tryme::").map(function (f) { return f.name; }).sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief toArray
////////////////////////////////////////////////////////////////////////////////

    testToArray2 : function () {
      aqlfunctions.register("UnitTests::tryme::foo", function (what) { return what * 2; }, true);
      aqlfunctions.register("UnitTests::tryme::bar", function (what) { return what * 2; }, true);
      aqlfunctions.register("UnitTests58::tryme::bar", function (what) { return what * 2; }, true);
      aqlfunctions.register("UnitTests58::whyme::bar", function (what) { return what * 2; }, true);

      assertEqual([ "UnitTests::tryme::bar", "UnitTests::tryme::foo" ], aqlfunctions.toArray("UnitTests").map(function (f) { return f.name; }).sort());
      assertEqual([ "UnitTests::tryme::bar", "UnitTests::tryme::foo" ], aqlfunctions.toArray("UnitTests::").map(function (f) { return f.name; }).sort());
      assertEqual([ "UnitTests::tryme::bar", "UnitTests::tryme::foo" ], aqlfunctions.toArray("UnitTests::tryme").map(function (f) { return f.name; }).sort());
      assertEqual([ "UnitTests58::tryme::bar", "UnitTests58::whyme::bar" ], aqlfunctions.toArray("UnitTests58").map(function (f) { return f.name; }).sort());
      assertEqual([ "UnitTests58::tryme::bar", "UnitTests58::whyme::bar" ], aqlfunctions.toArray("UnitTests58::").map(function (f) { return f.name; }).sort());
      assertEqual([ "UnitTests58::tryme::bar" ], aqlfunctions.toArray("UnitTests58::tryme").map(function (f) { return f.name; }).sort());
      assertEqual([ "UnitTests58::tryme::bar" ], aqlfunctions.toArray("UnitTests58::tryme::").map(function (f) { return f.name; }).sort());

      aqlfunctions.unregister("UnitTests58::tryme::bar", function (what) { return what * 2; }, true);
      aqlfunctions.unregister("UnitTests58::whyme::bar", function (what) { return what * 2; }, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test registered code
////////////////////////////////////////////////////////////////////////////////

    testRegisterCode1 : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true);
      assertEqual("function (what) { return what * 2; }", aqlfunctions.toArray("UnitTests")[0].code);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test registered code
////////////////////////////////////////////////////////////////////////////////

    testRegisterCode2 : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", "function (what) { return what *   2; }", true);
      assertEqual("function (what) { return what *   2; }", aqlfunctions.toArray("UnitTests")[0].code);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterFunc1 : function () {
      unregister("UnitTests::tryme::foo");
      aqlfunctions.register("UnitTests::tryme::foo", function (what) { return what * 2; }, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterFunc2 : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterString1 : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", "function (what) { return what * 2; }", true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterString2 : function () {
      unregister("UnitTests::tryme::foo");
      aqlfunctions.register("UnitTests::tryme::foo", "function (what) { return what * 2; }", true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterString3 : function () {
      unregister("UnitTests::tryme::foo");
      aqlfunctions.register("UnitTests::tryme::foo", "/* this is a function! */ \n function (what) { return what * 2; }", true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief re-register a function
////////////////////////////////////////////////////////////////////////////////

    testReRegister : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true);
      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true);
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
        aqlfunctions.register("_test::foo", function (what) { return what * 2; }, true);
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
/// @brief register a function with an invalid name
////////////////////////////////////////////////////////////////////////////////

    testRegisterInvalidName5 : function () {
      try {
        aqlfunctions.register("test::", function (what) { return what * 2; }, true);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_INVALID_NAME.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function without surrounding function() 
////////////////////////////////////////////////////////////////////////////////

    testRegisterNonFunction1 : function () {
      unregister("UnitTests::tryme");
      try {
        aqlfunctions.register("UnitTests::tryme", "1 + 1", true);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_INVALID_CODE.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function without surrounding function() 
////////////////////////////////////////////////////////////////////////////////

    testRegisterNonFunction2 : function () {
      unregister("UnitTests::tryme");
      try {
        aqlfunctions.register("UnitTests::tryme", "name = 'foo'", true);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_INVALID_CODE.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function with an invalid body
////////////////////////////////////////////////////////////////////////////////

    testRegisterInvalidCode1 : function () {
      unregister("UnitTests::tryme");
      try {
        aqlfunctions.register("UnitTests::tryme", "function (what) { ", true);
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
      unregister("UnitTests::tryme");
      try {
        aqlfunctions.register("UnitTests::tryme", 1234, true);
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
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 4; }, true);
      aqlfunctions.unregister("UnitTests::tryme");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister an unknown function
////////////////////////////////////////////////////////////////////////////////

    testUnregisterUnknown : function () {
      unregister("UnitTests::tryme");

      try {
        aqlfunctions.unregister("UnitTests::tryme");
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
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests::tryme(4)" }).execute().toArray();
      assertEqual([ 8 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQuery2 : function () {
      unregister("UnitTests::tryme");
      unregister("UnitTests::foo");
      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true);
      aqlfunctions.register("UnitTests::foo", function (what) { return what * 4; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests::tryme(4) + UnitTests::foo(9)" }).execute().toArray();
      assertEqual([ 4 * 2 + 9 * 4 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testMultiple : function () {
      unregister("UnitTests::tryme");
      unregister("UnitTests::foo");
      aqlfunctions.register("UnitTests::tryme", function (what) { return "foobar" + what; }, true);
      aqlfunctions.register("UnitTests::foo", function (what) { return (what * 4) + "barbaz"; }, true);

      var actual = db._createStatement({ query: "RETURN CONCAT(UnitTests::tryme('abcdef'), UnitTests::foo(9))" }).execute().toArray();
      assertEqual([ "foobar" + "abcdef" + (9 * 4) + "barbaz" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testManyInvocations : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true);

      var actual = db._createStatement({ query: "FOR i IN 1..10000 RETURN UnitTests::tryme(i)" }).execute().toArray();
      var expected = [ ];
      for (var i = 1; i <= 10000; ++i) {
        expected.push(i * 2);
      }
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryThrow : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { throw "peng"; }, true);

      try {
        db._createStatement({ query: "RETURN UnitTests::tryme(4)" }).execute().toArray();
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_RUNTIME_ERROR.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnUndefined : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests::tryme(4)" }).execute().toArray();
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnNan : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return 1 / 0; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests::tryme(4)" }).execute().toArray();
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnNull : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return null; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests::tryme(4)" }).execute().toArray();
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnTrue : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return true; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests::tryme(4)" }).execute().toArray();
      assertEqual([ true ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnFalse : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return false; }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests::tryme(4)" }).execute().toArray();
      assertEqual([ false ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnComplex : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme",
                            function (what) {
                              return [ true,
                                       false,
                                       null,
                                       1, 2, -4,
                                       [ 5.5,
                                         {
                                           a: 1,
                                           "b": "def"
                                         }
                                       ]
                                     ];
                            },
                            true);

      var actual = db._createStatement({ query: "RETURN UnitTests::tryme()" }).execute().toArray();
      assertEqual([ [ true, false, null, 1, 2, -4, [ 5.5, { a: 1, "b": "def" } ] ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryStringWithComments : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return true; }, true);

      var actual = db._createStatement({ query: "// foo\n /* bar\nbaz\n*/ RETURN UnitTests::tryme(4) // some comment" }).execute().toArray();
      assertEqual([ true ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryFuncWithComments : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { 
        // foo 
        /* bar
        baz
        */
        return [ true, false, null, 1, 2, -4, [ 5.5, { a: 1, "b": "def" } ] ]; // some comment
      }, true);

      var actual = db._createStatement({ query: "RETURN UnitTests::tryme()" }).execute().toArray();
      assertEqual([ [ true, false, null, 1, 2, -4, [ 5.5, { a: 1, "b": "def" } ] ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test for specific problem with docs returned by user functions
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnShapedJson : function () {
      db._drop("UnitTestsFunc");
      db._create("UnitTestsFunc");

      for (var i = 0; i < 5 ; ++i) {
        db.UnitTestsFunc.save({ value: i });
      }

      unregister("UnitTests::testFunc");
      var testFunc = function() {
        var db = require("internal").db;
        var query = "FOR path IN UnitTestsFunc SORT path.value RETURN path.value";
        var bind = {};
        return db._query(query, bind).toArray()[0];
      };
      aqlfunctions.register("UnitTests::testFunc", testFunc);

      var actual = db._createStatement({ query: "RETURN UnitTests::testFunc()" }).execute().toArray();
      assertEqual([ 0 ], actual);

      db._drop("UnitTestsFunc");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test for specific problem with docs returned by user functions
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnShapedJsonFull : function () {
      db._drop("UnitTestsFunc");
      db._create("UnitTestsFunc");

      db.UnitTestsFunc.save({ value1: "abc", value2: 123, value3: null, value4: false });

      unregister("UnitTests::testFunc");
      var testFunc = function() {
        var db = require("internal").db;
        var query = "FOR path IN UnitTestsFunc RETURN path";
        var bind = {};
        return db._query(query, bind).toArray()[0];
      };
      aqlfunctions.register("UnitTests::testFunc", testFunc);

      var actual = db._createStatement({ query: "RETURN UnitTests::testFunc()" }).execute().toArray();
      assertEqual("abc", actual[0].value1);
      assertEqual(123, actual[0].value2);
      assertEqual(null, actual[0].value3);
      assertEqual(false, actual[0].value4);

      db._drop("UnitTestsFunc");
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AqlFunctionsSuite);

return jsunity.done();

