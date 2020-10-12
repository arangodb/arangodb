/*jshint globalstrict:false, strict:false, maxlen: 200, unused: false*/
/*global assertEqual, assertTrue, assertFalse, fail, AQL_WARNING */

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
  let unregister = function (name) {
    try {
      aqlfunctions.unregister(name);
    } catch (err) {
    }
  };

  return {

    setUp : function () {
      db._useDatabase("_system");
      aqlfunctions.unregisterGroup("UnitTests::");
    },

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
      } catch (err) {
      }

      db._createDatabase("UnitTestsAhuacatl");
      try {
        db._useDatabase("UnitTestsAhuacatl");

        let result = aqlfunctions.register("UnitTests::tryme::foo", function (what) { return what * 4; }, true);

        assertEqual("function (what) { return what * 4; }", aqlfunctions.toArray("UnitTests")[0].code);

        let actual = db._query({ query: "RETURN UnitTests::tryme::foo(4)" }).toArray();
        assertEqual([ 16 ], actual);

        db._useDatabase("_system");
        assertEqual("function (what) { return what * 2; }", aqlfunctions.toArray("UnitTests")[0].code);
        actual = db._query({ query: "RETURN UnitTests::tryme::foo(4)" }).toArray();
        assertEqual([ 8 ], actual);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase("UnitTestsAhuacatl");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multiple databases
////////////////////////////////////////////////////////////////////////////////

    testDatabases2 : function () {
      try {
        unregister("UnitTests::tryme");
      } catch (err) {
      }

      try {
        db._dropDatabase("UnitTestsAhuacatl");
      } catch (err) {
      }

      db._createDatabase("UnitTestsAhuacatl");
      try {
        db._useDatabase("UnitTestsAhuacatl");

        aqlfunctions.register("UnitTests::tryme", function (what) { return what * 3; }, true);
        assertEqual("function (what) { return what * 3; }", aqlfunctions.toArray("UnitTests")[0].code);

        let actual = db._query({ query: "RETURN UnitTests::tryme(3)" }).toArray();
        assertEqual([ 9 ], actual);

        aqlfunctions.register("UnitTests::tryme", function (what) { return what * 4; }, true);
        assertEqual("function (what) { return what * 4; }", aqlfunctions.toArray("UnitTests")[0].code);
        actual = db._query({ query: "RETURN UnitTests::tryme(3)" }).toArray();
        assertEqual([ 12 ], actual);

        db._useDatabase("_system");

        assertEqual(0, aqlfunctions.toArray("UnitTests").length);
        try {
          db._query({ query: "RETURN UnitTests::tryme(4)" }).toArray();
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_QUERY_FUNCTION_NOT_FOUND.code, err.errorNum);
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase("UnitTestsAhuacatl");
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

      aqlfunctions.unregister("UnitTests58::tryme::bar");
      aqlfunctions.unregister("UnitTests58::whyme::bar");
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
      assertFalse(aqlfunctions.register("UnitTests::tryme", "function (what) { return what * 2; }", true));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterString2 : function () {
      unregister("UnitTests::tryme::foo");
      assertFalse(aqlfunctions.register("UnitTests::tryme::foo", "function (what) { return what * 2; }", true));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function
////////////////////////////////////////////////////////////////////////////////

    testRegisterString3 : function () {
      unregister("UnitTests::tryme::foo");
      assertFalse(aqlfunctions.register("UnitTests::tryme::foo", "/* this is a function! */ \n function (what) { return what * 2; }", true));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief re-register a function
////////////////////////////////////////////////////////////////////////////////

    testReRegister : function () {
      unregister("UnitTests::tryme");
      assertFalse(aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true));
      assertTrue(aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function with an invalid name
////////////////////////////////////////////////////////////////////////////////

    testRegisterInvalidName1 : function () {
      try {
        aqlfunctions.register("foo", function (what) { return what * 2; }, true);
        fail();
      } catch (err) {
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
      } catch (err) {
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
      } catch (err) {
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
      } catch (err) {
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
      } catch (err) {
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
      } catch (err) {
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
      } catch (err) {
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
      } catch (err) {
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
      } catch (err) {
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
      } catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQuery1 : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true);

      let actual = db._query({ query: "RETURN UnitTests::tryme(4)" }).toArray();
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

      let actual = db._query({ query: "RETURN UnitTests::tryme(4) + UnitTests::foo(9)" }).toArray();
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

      let actual = db._query({ query: "RETURN CONCAT(UnitTests::tryme('abcdef'), UnitTests::foo(9))" }).toArray();
      assertEqual([ "foobar" + "abcdef" + (9 * 4) + "barbaz" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testManyInvocations : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return what * 2; }, true);

      let actual = db._query({ query: "FOR i IN 1..10000 RETURN UnitTests::tryme(i)" }).toArray();
      let expected = [ ];
      for (let i = 1; i <= 10000; ++i) {
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
        db._query({ query: "RETURN UnitTests::tryme(4)" }).toArray();
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_QUERY_FUNCTION_RUNTIME_ERROR.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnUndefined : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { }, true);

      let actual = db._query({ query: "RETURN UnitTests::tryme(4)" }).toArray();
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnNan : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return 1 / 0; }, true);

      let actual = db._query({ query: "RETURN UnitTests::tryme(4)" }).toArray();
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnNull : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return null; }, true);

      let actual = db._query({ query: "RETURN UnitTests::tryme(4)" }).toArray();
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnTrue : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return true; }, true);

      let actual = db._query({ query: "RETURN UnitTests::tryme(4)" }).toArray();
      assertEqual([ true ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnFalse : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return false; }, true);

      let actual = db._query({ query: "RETURN UnitTests::tryme(4)" }).toArray();
      assertEqual([ false ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnError : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) {
        const error = require("@arangodb").errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH;
        AQL_WARNING(error.code,'ALL THY ERRORS ARE BELONG TO US!');
      },
                            true);

      let actual = db._query({ query: "RETURN UnitTests::tryme(4)" });
      let extra = actual.getExtra();
      assertTrue(extra.hasOwnProperty('warnings'));
      assertTrue(Array.isArray(extra.warnings));
      assertEqual(extra.warnings.length, 1);
      let warn = extra.warnings[0];
      assertTrue(warn.hasOwnProperty('code'));
      assertTrue(warn.hasOwnProperty('message'));
      assertEqual(warn.code, require("@arangodb").errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code);
      assertEqual(warn.message, 'ALL THY ERRORS ARE BELONG TO US!');
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

      let actual = db._query({ query: "RETURN UnitTests::tryme()" }).toArray();
      assertEqual([ [ true, false, null, 1, 2, -4, [ 5.5, { a: 1, "b": "def" } ] ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function and run a query
////////////////////////////////////////////////////////////////////////////////

    testQueryStringWithComments : function () {
      unregister("UnitTests::tryme");
      aqlfunctions.register("UnitTests::tryme", function (what) { return true; }, true);

      let actual = db._query({ query: "// foo\n /* bar\nbaz\n*/ RETURN UnitTests::tryme(4) // some comment" }).toArray();
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

      let actual = db._query({ query: "RETURN UnitTests::tryme()" }).toArray();
      assertEqual([ [ true, false, null, 1, 2, -4, [ 5.5, { a: 1, "b": "def" } ] ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test for specific problem with docs returned by user functions
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnShapedJson : function () {
      db._drop("UnitTestsFunc");
      db._create("UnitTestsFunc");

      try {
        for (let i = 0; i < 5 ; ++i) {
          db.UnitTestsFunc.save({ value: i });
        }

        unregister("UnitTests::testFunc");
        let testFunc = function() {
          let db = require("internal").db;
          let query = "FOR path IN UnitTestsFunc SORT path.value RETURN path.value";
          let bind = {};
          return db._query(query, bind).toArray()[0];
        };
        aqlfunctions.register("UnitTests::testFunc", testFunc);

        let actual = db._query({ query: "RETURN UnitTests::testFunc()" }).toArray();
        assertEqual([ 0 ], actual);
      } finally {
        db._drop("UnitTestsFunc");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test for specific problem with docs returned by user functions
////////////////////////////////////////////////////////////////////////////////

    testQueryReturnShapedJsonFull : function () {
      db._drop("UnitTestsFunc");
      db._create("UnitTestsFunc");

      try {
        db.UnitTestsFunc.save({ value1: "abc", value2: 123, value3: null, value4: false });

        unregister("UnitTests::testFunc");
        let testFunc = function() {
          let db = require("internal").db;
          let query = "FOR path IN UnitTestsFunc RETURN path";
          let bind = {};
          return db._query(query, bind).toArray()[0];
        };
        aqlfunctions.register("UnitTests::testFunc", testFunc);

        let actual = db._query({ query: "RETURN UnitTests::testFunc()" }).toArray();
        assertEqual("abc", actual[0].value1);
        assertEqual(123, actual[0].value2);
        assertEqual(null, actual[0].value3);
        assertEqual(false, actual[0].value4);
      } finally {
        db._drop("UnitTestsFunc");
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test for specific problem with docs returned by user functions
    ////////////////////////////////////////////////////////////////////////////////

    testQueryUsesUDFWithNestedQuery : function () {
      db._drop("UnitTestsFunc");
      db._create("UnitTestsFunc", {numberOfShards: 2, shardKeys: ["productId"]});
      try {
        db._create("UnitTestsFunc2", {distributeShardsLike: "UnitTestsFunc"});

        try {
          let docs = [];
          for (let i = 0; i < 100; i++) {
            docs.push({ productId: "abc" });
          }
          db.UnitTestsFunc.save(docs);
          db.UnitTestsFunc2.save({ _key: "abc", value: "hello" });

          unregister("UnitTests::testFunc");
          let testFunc = function(d) {
            let db = require("internal").db;
            let query = "FOR d IN UnitTestsFunc2 RETURN d";
            return db._query(query).toArray()[0];
          };
          aqlfunctions.register("UnitTests::testFunc", testFunc);

          let actual = db._query({ query: "FOR doc in UnitTestsFunc RETURN UnitTests::testFunc(doc)" }).toArray();
          assertEqual(100, actual.length);
          assertEqual("hello", actual[0].value);
        } finally {
          db._drop("UnitTestsFunc2");
        }
      } finally {
        db._drop("UnitTestsFunc");
      }
    }

  };
}

jsunity.run(AqlFunctionsSuite);

return jsunity.done();
