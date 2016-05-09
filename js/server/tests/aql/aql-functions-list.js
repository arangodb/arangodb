/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */
////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, functions
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

var internal = require("internal");
var errors = internal.errors;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlListTestSuite () {
  var collectionName = "UnitTestList";
  var collection; 

  return {

    setUp : function () {
      internal.db._drop(collectionName);
      collection = internal.db._create(collectionName);
    },

    tearDown: function () {
      internal.db._drop(collectionName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test push function
////////////////////////////////////////////////////////////////////////////////

    testPush : function () {
      var data = [
        [ [ 1, 2, 3, 4 ], [ [ 1, 2, 3 ], 4 ] ],
        [ [ 1, 2, 3, "foo" ], [ [ 1, 2, 3 ], "foo" ] ],
        [ [ "foo", "bar", "baz", "foo" ], [ [ "foo", "bar", "baz", ], "foo" ] ],
        [ [ "foo" ], [ null, "foo" ] ],
        [ null, [ false, "foo" ] ],
        [ null, [ 1, "foo" ] ],
        [ [ "foo" ], [ [ ], "foo" ] ],
        [ null, [ "foo", "foo" ] ],
        [ null, [ { }, "foo" ] ],
        [ [ "" ], [ [ ], "" ] ],
        [ [ false, false ], [ [ false ], false ] ],
        [ [ false, null ], [ [ false ], null ] ],
        [ [ 0 ], [ [ ], 0, true ] ],
        [ [ true, 1 ], [ [ true ], 1, true ] ],
        [ [ true ], [ [ true ], true, true ] ],
        [ [ true, true ], [ [ true ], true, false ] ],
        [ [ "foo", "bar", "foo" ], [ [ "foo", "bar", "foo" ], "foo", true ] ],
        [ [ "foo", [ ] ], [ [ "foo" ], [ ], true ] ],
        [ [ "foo", [ ] ], [ [ "foo", [ ] ], [ ], true ] ],
        [ [ "foo", [ ], [ ] ], [ [ "foo", [ ] ], [ ], false ] ],
        [ [ { a: 1 }, { a: 2 }, { b: 1 } ], [ [ { a: 1 }, { a: 2 }, { b: 1 } ], { a: 1 }, true ] ],
        [ [ { a: 1 }, { a: 2 }, { b: 1 }, { a: 1 } ], [ [ { a: 1 }, { a: 2 }, { b: 1 } ], { a: 1 }, false ] ],
        [ [ { a: 1 }, { a: 2 }, { b: 1 }, { b: 2 } ], [ [ { a: 1 }, { a: 2 }, { b: 1 } ], { b: 2 }, true ] ]
      ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN PUSH(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(PUSH(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + "))");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(V8(PUSH(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")))");
        assertEqual(d[0], actual[0], d);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test push function
////////////////////////////////////////////////////////////////////////////////

    testPushBig : function () {
      var l = [];
      for (var i = 0; i < 2000; i += 2) {
        l.push(i);
      }
      var actual = getQueryResults("RETURN PUSH(" + JSON.stringify(l) + ", 1000, true)");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN PUSH(" + JSON.stringify(l) + ", 1000, true)");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN PUSH(" + JSON.stringify(l) + ", 1000, false)");
      l.push(1000);
      assertEqual(l, actual[0]);
      assertEqual(1001, actual[0].length);

      actual = getQueryResults("RETURN PUSH(" + JSON.stringify(l) + ", 1001, true)");
      l.push(1001);
      assertEqual(l, actual[0]);
      assertEqual(1002, actual[0].length);

      // Reset to default
      l.pop();
      l.pop();

      actual = getQueryResults("RETURN NOOPT(PUSH(" + JSON.stringify(l) + ", 1000, true))");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(PUSH(" + JSON.stringify(l) + ", 1000, true))");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(PUSH(" + JSON.stringify(l) + ", 1000, false))");
      l.push(1000);
      assertEqual(l, actual[0]);
      assertEqual(1001, actual[0].length);

      assertEqual(l.indexOf(1001), -1);
      actual = getQueryResults("RETURN NOOPT(PUSH(" + JSON.stringify(l) + ", 1001, true))");
      l.push(1001);
      assertEqual(l, actual[0]);
      assertEqual(1002, actual[0].length);

      // Reset to default
      l.pop();
      l.pop();

      actual = getQueryResults("RETURN NOOPT(V8(PUSH(" + JSON.stringify(l) + ", 1000, true)))");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(PUSH(" + JSON.stringify(l) + ", 1000, true)))");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(PUSH(" + JSON.stringify(l) + ", 1000, false)))");
      l.push(1000);
      assertEqual(l, actual[0]);
      assertEqual(1001, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(PUSH(" + JSON.stringify(l) + ", 1001, true)))");
      l.push(1001);
      assertEqual(l, actual[0]);
      assertEqual(1002, actual[0].length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test push function
////////////////////////////////////////////////////////////////////////////////

    testPushInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PUSH()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PUSH([ ])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PUSH([ ], 1, 2, 3)");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(PUSH())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(PUSH([ ]))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(PUSH([ ], 1, 2, 3))");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(PUSH()))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(PUSH([ ])))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(PUSH([ ], 1, 2, 3)))");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unshift function
////////////////////////////////////////////////////////////////////////////////

    testUnshift : function () {
      var data = [
        [ [ 4, 1, 2, 3 ], [ [ 1, 2, 3 ], 4 ] ],
        [ [ "foo", 1, 2, 3 ], [ [ 1, 2, 3 ], "foo" ] ],
        [ [ "foo", "foo", "bar", "baz" ], [ [ "foo", "bar", "baz", ], "foo" ] ],
        [ [ "foo" ], [ null, "foo" ] ],
        [ null, [ false, "foo" ] ],
        [ null, [ 1, "foo" ] ],
        [ [ "foo" ], [ [ ], "foo" ] ],
        [ null, [ "foo", "foo" ] ],
        [ null, [ { }, "foo" ] ],
        [ [ "" ], [ [ ], "" ] ],
        [ [ false, false ], [ [ false ], false ] ],
        [ [ null, false ], [ [ false ], null ] ],
        [ [ 0 ], [ [ ], 0, true ] ],
        [ [ 1, true ], [ [ true ], 1, true ] ],
        [ [ true ], [ [ true ], true, true ] ],
        [ [ true, true ], [ [ true ], true, false ] ],
        [ [ "foo", "bar", "foo" ], [ [ "foo", "bar", "foo" ], "foo", true ] ],
        [ [ "baz", "foo", "bar", "foo" ], [ [ "foo", "bar", "foo" ], "baz", true ] ],
        [ [ [ ], "foo" ], [ [ "foo" ], [ ], true ] ],
        [ [ "foo", [ ] ], [ [ "foo", [ ] ], [ ], true ] ],
        [ [ [ ], "foo" ], [ [ [ ], "foo" ], [ ], true ] ],
        [ [ [ ], "foo", [ ] ], [ [ "foo", [ ] ], [ ], false ] ],
        [ [ [ ], [ ], "foo" ], [ [ [ ], "foo" ], [ ], false ] ],
        [ [ { a: 1 }, { a: 2 }, { b: 1 } ], [ [ { a: 1 }, { a: 2 }, { b: 1 } ], { a: 1 }, true ] ],
        [ [ { a: 1 }, { a: 1 }, { a: 2 }, { b: 1 } ], [ [ { a: 1 }, { a: 2 }, { b: 1 } ], { a: 1 }, false ] ],
        [ [ { b: 2 }, { a: 1 }, { a: 2 }, { b: 1 } ], [ [ { a: 1 }, { a: 2 }, { b: 1 } ], { b: 2 }, true ] ]
      ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN UNSHIFT(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(UNSHIFT(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + "))");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(V8(UNSHIFT(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")))");
        assertEqual(d[0], actual[0], d);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unshift function
////////////////////////////////////////////////////////////////////////////////

    testUnshiftBig : function () {
      var l = [];
      for (var i = 0; i < 2000; i += 2) {
        l.push(i);
      }

      var actual = getQueryResults("RETURN UNSHIFT(" + JSON.stringify(l) + ", 1000, true)");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN UNSHIFT(" + JSON.stringify(l) + ", 1000, true)");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN UNSHIFT(" + JSON.stringify(l) + ", 1000, false)");
      l.unshift(1000);
      assertEqual(l, actual[0]);
      assertEqual(1001, actual[0].length);

      actual = getQueryResults("RETURN UNSHIFT(" + JSON.stringify(l) + ", 1001, true)");
      l.unshift(1001);
      assertEqual(l, actual[0]);
      assertEqual(1002, actual[0].length);

      // Reset to start
      l.shift();
      l.shift();

      actual = getQueryResults("RETURN NOOPT(UNSHIFT(" + JSON.stringify(l) + ", 1000, true))");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(UNSHIFT(" + JSON.stringify(l) + ", 1000, true))");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(UNSHIFT(" + JSON.stringify(l) + ", 1000, false))");
      l.unshift(1000);
      assertEqual(l, actual[0]);
      assertEqual(1001, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(UNSHIFT(" + JSON.stringify(l) + ", 1001, true))");
      l.unshift(1001);
      assertEqual(l, actual[0]);
      assertEqual(1002, actual[0].length);

      // Reset to start
      l.shift();
      l.shift();

      actual = getQueryResults("RETURN NOOPT(V8(UNSHIFT(" + JSON.stringify(l) + ", 1000, true)))");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(UNSHIFT(" + JSON.stringify(l) + ", 1000, true)))");
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(UNSHIFT(" + JSON.stringify(l) + ", 1000, false)))");
      l.unshift(1000);
      assertEqual(l, actual[0]);
      assertEqual(1001, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(UNSHIFT(" + JSON.stringify(l) + ", 1001, true)))");
      l.unshift(1001);
      assertEqual(l, actual[0]);
      assertEqual(1002, actual[0].length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unshift function
////////////////////////////////////////////////////////////////////////////////

    testUnshiftInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSHIFT()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSHIFT([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSHIFT([ ], 1, 2, 3)"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(UNSHIFT())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(UNSHIFT([ ]))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(UNSHIFT([ ], 1, 2, 3))"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(UNSHIFT()))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(UNSHIFT([ ])))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(UNSHIFT([ ], 1, 2, 3)))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test pop function
////////////////////////////////////////////////////////////////////////////////

    testPop : function () {
      var data = [
        [ null, null ],
        [ null, false ],
        [ null, true ],
        [ null, 23 ],
        [ null, "foo" ],
        [ null, { } ],
        [ [ ], [ ] ],
        [ [ 1, 2 ], [ 1, 2, 3 ] ],
        [ [ 1, 2, 3, "foo" ], [ 1, 2, 3, "foo", 4 ] ],
        [ [ 1, 2, 3 ], [ 1, 2, 3, "foo" ] ],
        [ [ "foo", "bar", "baz" ], [ "foo", "bar", "baz", "foo" ] ],
        [ [ null ], [ null, "foo" ] ],
        [ [ false ], [ false, "foo" ] ],
        [ [ 1 ], [ 1, "foo" ] ],
        [ [ [ ] ], [ [ ], "foo" ] ],
        [ [ "foo" ], [ "foo", "foo" ] ],
        [ [ "foo" ], [ "foo", "bar" ] ],
        [ [ { } ], [ { }, "foo" ] ],
        [ [ [ ] ], [ [ ], "" ] ],
        [ [ [ false ] ], [ [ false ], false ] ],
        [ [ [ false ] ], [ [ false ], null ] ],
        [ [ [ ], 0 ], [ [ ], 0, true ] ],
        [ [ true, 1 ], [ true, 1, true ] ],
        [ [ true, true ], [ true, true, true ] ],
        [ [ true, false ], [ true, false, true ] ],
        [ [ true, false ], [ true, false, false ] ],
        [ [ [ true ], true ], [ [ true ], true, false ] ],
        [ [ [ "foo", "bar", "foo" ], "foo" ], [ [ "foo", "bar", "foo" ], "foo", true ] ],
        [ [ "foo", [ ] ], [ "foo", [ ], true ] ],
        [ [ "foo", [ ], [ ] ], [ "foo", [ ], [ ], true ] ],
        [ [ "foo", [ ], [ ] ], [ "foo", [ ], [ ], false ] ],
        [ [ { a: 1 }, { a: 2 } ], [ { a: 1 }, { a: 2 }, { b: 1 } ] ],
        [ [ { a: 1 }, { a: 2 }, { b: 1 } ], [ { a: 1 }, { a: 2 }, { b: 1 }, { a: 1 } ] ],
        [ [ { a: 1 }, { a: 2 }, { b: 1 } ], [ { a: 1 }, { a: 2 }, { b: 1 }, { b: 2 } ] ]
      ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN POP(" + JSON.stringify(d[1]) + ")");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(POP(" + JSON.stringify(d[1]) + "))");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(V8(POP(" + JSON.stringify(d[1]) + ")))");
        assertEqual(d[0], actual[0], d);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test pop function
////////////////////////////////////////////////////////////////////////////////

    testPopBig : function () {
      var l = [];
      for (var i = 0; i < 2000; i += 2) {
        l.push(i);
      }
      var actual = getQueryResults("RETURN POP(" + JSON.stringify(l) + ")");
      l.pop();
      assertEqual(l, actual[0]);
      assertEqual(999, actual[0].length);

      actual = getQueryResults("RETURN POP(" + JSON.stringify(l) + ")");
      l.pop();
      assertEqual(l, actual[0]);
      assertEqual(998, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(POP(" + JSON.stringify(l) + "))");
      l.pop();
      assertEqual(l, actual[0]);
      assertEqual(997, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(POP(" + JSON.stringify(l) + "))");
      l.pop();
      assertEqual(l, actual[0]);
      assertEqual(996, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(POP(" + JSON.stringify(l) + ")))");
      l.pop();
      assertEqual(l, actual[0]);
      assertEqual(995, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(POP(" + JSON.stringify(l) + ")))");
      l.pop();
      assertEqual(l, actual[0]);
      assertEqual(994, actual[0].length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test pop function
////////////////////////////////////////////////////////////////////////////////

    testPopInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN POP()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN POP([ ], 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN POP([ ], 1, 2)");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(POP())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(POP([ ], 1))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(POP([ ], 1, 2))");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(POP()))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(POP([ ], 1)))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(POP([ ], 1, 2)))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test shift function
////////////////////////////////////////////////////////////////////////////////

    testShift : function () {
      var data = [
        [ null, null ],
        [ null, false ],
        [ null, true ],
        [ null, 23 ],
        [ null, "foo" ],
        [ null, { } ],
        [ [ ], [ ] ],
        [ [ 2, 3 ], [ 1, 2, 3 ] ],
        [ [ 2, 3, "foo", 4 ], [ 1, 2, 3, "foo", 4 ] ],
        [ [ 2, 3, "foo" ], [ 1, 2, 3, "foo" ] ],
        [ [ "bar", "baz", "foo" ], [ "foo", "bar", "baz", "foo" ] ],
        [ [ "foo" ], [ null, "foo" ] ],
        [ [ "foo" ], [ false, "foo" ] ],
        [ [ "foo" ], [ 1, "foo" ] ],
        [ [ "foo" ], [ [ ], "foo" ] ],
        [ [ "foo" ], [ "foo", "foo" ] ],
        [ [ "bar" ], [ "foo", "bar" ] ],
        [ [ "foo" ], [ { }, "foo" ] ],
        [ [ { } ], [ "foo", { } ] ],
        [ [ "" ], [ [ ], "" ] ],
        [ [ [ ] ], [ "", [ ] ] ],
        [ [ false ], [ [ false ], false ] ],
        [ [ null ], [ [ false ], null ] ],
        [ [ 0, true ], [ [ ], 0, true ] ],
        [ [ 1, true ], [ true, 1, true ] ],
        [ [ true, true ], [ true, true, true ] ],
        [ [ false, true ], [ true, false, true ] ],
        [ [ false, false ], [ true, false, false ] ],
        [ [ true, false ], [ [ true ], true, false ] ],
        [ [ "foo", true ], [ [ "foo", "bar", "foo" ], "foo", true ] ],
        [ [ [ ], true ], [ "foo", [ ], true ] ],
        [ [ [ ], [ ], true ], [ "foo", [ ], [ ], true ] ],
        [ [ [ ], [ ], false ], [ "foo", [ ], [ ], false ] ],
        [ [ { a: 2 }, { b: 1 } ], [ { a: 1 }, { a: 2 }, { b: 1 } ] ],
        [ [ { a: 2 }, { b: 1 }, { a: 1 } ], [ { a: 1 }, { a: 2 }, { b: 1 }, { a: 1 } ] ],
        [ [ { a: 2 }, { b: 1 }, { b: 2 } ], [ { a: 1 }, { a: 2 }, { b: 1 }, { b: 2 } ] ]
      ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN SHIFT(" + JSON.stringify(d[1]) + ")");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(SHIFT(" + JSON.stringify(d[1]) + "))");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(V8(SHIFT(" + JSON.stringify(d[1]) + ")))");
        assertEqual(d[0], actual[0], d);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test shift function
////////////////////////////////////////////////////////////////////////////////

    testShiftBig : function () {
      var l = [];
      for (var i = 0; i < 2000; i += 2) {
        l.push(i);
      }
      var actual = getQueryResults("RETURN SHIFT(" + JSON.stringify(l) + ")");
      var first = l.shift();
      assertEqual(l, actual[0]);
      assertEqual(999, actual[0].length);

      actual = getQueryResults("RETURN SHIFT(" + JSON.stringify(l) + ")");
      var second = l.shift();
      assertEqual(l, actual[0]);
      assertEqual(998, actual[0].length);

      l.unshift(second);
      l.unshift(first);

      actual = getQueryResults("RETURN NOOPT(SHIFT(" + JSON.stringify(l) + "))");
      first = l.shift();
      assertEqual(l, actual[0]);
      assertEqual(999, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(SHIFT(" + JSON.stringify(l) + "))");
      second = l.shift();
      assertEqual(l, actual[0]);
      assertEqual(998, actual[0].length);

      l.unshift(second);
      l.unshift(first);

      actual = getQueryResults("RETURN NOOPT(V8(SHIFT(" + JSON.stringify(l) + ")))");
      l.shift();
      assertEqual(l, actual[0]);
      assertEqual(999, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(SHIFT(" + JSON.stringify(l) + ")))");
      l.shift();
      assertEqual(l, actual[0]);
      assertEqual(998, actual[0].length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test shift function
////////////////////////////////////////////////////////////////////////////////

    testShiftInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SHIFT()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SHIFT([ ], 1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SHIFT([ ], 1, 2)"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SHIFT())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SHIFT([ ], 1))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SHIFT([ ], 1, 2))"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(SHIFT()))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(SHIFT([ ], 1)))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(SHIFT([ ], 1, 2)))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test push/pop function
////////////////////////////////////////////////////////////////////////////////

    testPushPop : function () {
      var l = [], i, actual;
      for (i = 0; i < 1000; ++i) {
        actual = getQueryResults("RETURN PUSH(" + JSON.stringify(l) + ", " + JSON.stringify(i) + ")");
        l.push(i);
        assertEqual(l, actual[0]);
      }

      for (i = 0; i < 1000; ++i) {
        actual = getQueryResults("RETURN POP(" + JSON.stringify(l) + ")");
        l.pop();
        assertEqual(l, actual[0]);
      }

      assertEqual(l.length, 0);

      for (i = 0; i < 1000; ++i) {
        actual = getQueryResults("RETURN NOOPT(PUSH(" + JSON.stringify(l) + ", " + JSON.stringify(i) + "))");
        l.push(i);
        assertEqual(l, actual[0]);
      }

      for (i = 0; i < 1000; ++i) {
        actual = getQueryResults("RETURN NOOPT(POP(" + JSON.stringify(l) + "))");
        l.pop();
        assertEqual(l, actual[0]);
      }

      assertEqual(l.length, 0);

      for (i = 0; i < 1000; ++i) {
        actual = getQueryResults("RETURN NOOPT(V8(PUSH(" + JSON.stringify(l) + ", " + JSON.stringify(i) + ")))");
        l.push(i);
        assertEqual(l, actual[0]);
      }

      for (i = 0; i < 1000; ++i) {
        actual = getQueryResults("RETURN NOOPT(V8(POP(" + JSON.stringify(l) + ")))");
        l.pop();
        assertEqual(l, actual[0]);
      }

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unshift/shift function
////////////////////////////////////////////////////////////////////////////////

    testUnshiftShift :  function () {
      var l = [], i, actual;
      for (i = 0; i < 1000; ++i) {
        actual = getQueryResults("RETURN UNSHIFT(" + JSON.stringify(l) + ", " + JSON.stringify(i) + ")");
        l.unshift(i);
        assertEqual(l, actual[0]);
      }

      for (i = 0; i < 1000; ++i) {
        actual = getQueryResults("RETURN SHIFT(" + JSON.stringify(l) + ")");
        l.shift();
        assertEqual(l, actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test append function
////////////////////////////////////////////////////////////////////////////////

    testAppend : function () {
      var data = [
        [ [ ], [ [ ], [ ] ] ],
        [ [ "foo", "bar", "baz" ], [ [ "foo" ], [ "bar", "baz" ] ] ],
        [ [ "foo", "foo", "bar", "baz" ], [ [ "foo" ], [ "foo", "bar", "baz" ] ] ],
        [ [ "foo", "bar", "baz" ], [ [ "foo" ], [ "foo", "bar", "baz" ], true ] ],
        [ [ "foo", "bar", "baz" ], [ [ "foo" ], [ "foo", "bar", "baz", "foo" ], true ] ],
        [ [ "foo", "bar", "baz" ], [ [ "foo", "bar" ], [ "baz" ] ] ],
        [ [ "foo", "bar", "baz" ], [ null, [ "foo", "bar", "baz" ] ] ],
        [ [ "foo", "bar", "baz" ], [ [ "foo", "bar", "baz" ], null ] ],
        [ [ "foo", "bar", "baz" ], [ [ ], [ "foo", "bar", "baz" ] ] ],
        [ [ "foo", "bar", "baz" ], [ [ "foo", "bar", "baz" ], [ ] ] ],
        [ [ "foo", "bar", "baz", "one", "two", "three" ], [ [ "foo", "bar", "baz" ], [ "one", "two", "three" ] ] ],
        [ [ "foo", "bar", "baz", "one", "two", null, "three" ], [ [ "foo", "bar", "baz" ], [ "one", "two", null, "three" ] ] ],
        [ [ "two", "one", null, "three", "one", "two", null, "three" ], [ [ "two", "one", null, "three" ], [ "one", "two", null, "three" ] ] ],
        [ [ "two", "one", null, "three" ], [ [ "two", "one", null, "three" ], [ "one", "two", null, "three" ], true ] ],
        [ [ "two", "one", null, "three", "four" ], [ [ "two", "one", null, "three" ], [ "one", "two", "four", null, "three" ], true ] ]
      ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN APPEND(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(APPEND(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + "))");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(V8(APPEND(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")))");
        assertEqual(d[0], actual[0], d);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test append function
////////////////////////////////////////////////////////////////////////////////

    testAppendBig : function () {
      var l1 = [], l2 = [ ];
      for (var i = 0; i < 2000; i += 4) {
        l1.push(i);
        l2.push(i + 1);
      }
      var actual = getQueryResults("RETURN APPEND(" + JSON.stringify(l1) + ", " + JSON.stringify(l2) + ")");
      var l = l1.concat(l2);
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN APPEND(" + JSON.stringify(l) + ", " + JSON.stringify(l1) + ")");
      l = l.concat(l1);
      assertEqual(l, actual[0]);
      assertEqual(1500, actual[0].length);

      actual = getQueryResults("RETURN APPEND(" + JSON.stringify(l) + ", " + JSON.stringify(l1) + ", true)");
      assertEqual(l, actual[0]);
      assertEqual(1500, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(APPEND(" + JSON.stringify(l1) + ", " + JSON.stringify(l2) + "))");
      l = l1.concat(l2);
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(APPEND(" + JSON.stringify(l) + ", " + JSON.stringify(l1) + "))");
      l = l.concat(l1);
      assertEqual(l, actual[0]);
      assertEqual(1500, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(APPEND(" + JSON.stringify(l) + ", " + JSON.stringify(l1) + ", true))");
      assertEqual(l, actual[0]);
      assertEqual(1500, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(APPEND(" + JSON.stringify(l1) + ", " + JSON.stringify(l2) + ")))");
      l = l1.concat(l2);
      assertEqual(l, actual[0]);
      assertEqual(1000, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(APPEND(" + JSON.stringify(l) + ", " + JSON.stringify(l1) + ")))");
      l = l.concat(l1);
      assertEqual(l, actual[0]);
      assertEqual(1500, actual[0].length);

      actual = getQueryResults("RETURN NOOPT(V8(APPEND(" + JSON.stringify(l) + ", " + JSON.stringify(l1) + ", true)))");
      assertEqual(l, actual[0]);
      assertEqual(1500, actual[0].length);
    },

    testAppendDocuments : function () {
      for (var i = 0; i < 10; ++i) {
        collection.save({_key: "test" + i});
      }
      var bindVars = {"@collection" : collectionName};
      var actual = getQueryResults("LET tmp = (FOR x IN @@collection RETURN x) RETURN APPEND([], tmp)", bindVars);
      assertEqual(actual.length, 1);
      actual = actual[0];
      assertEqual(actual.length, 10);
      actual = actual.sort(function(l, r) { if (l._key < r._key) { return -1;} else if (l._key > r._key) { return 1;} return 0; });
      for (i = 0; i < 10; ++i) {
        assertEqual(actual[i]._key, "test"+i);
      }
    },

    testAppendDocuments2 : function () {
      for (var i = 0; i < 10; ++i) {
        collection.save({_key: "test" + i});
      }
      var bindVars = {"@collection" : collectionName};
      var actual = getQueryResults("LET tmp = (FOR x IN @@collection RETURN x) RETURN APPEND('stringvalue', tmp)", bindVars);
      assertEqual(actual.length, 1);
      actual = actual[0];
      assertEqual(actual.length, 10);
      actual = actual.sort(function(l, r) { if (l._key < r._key) { return -1;} else if (l._key > r._key) { return 1;} return 0; });
      for (i = 0; i < 10; ++i) {
        assertEqual(actual[i]._key, "test"+i);
      }
    },

    testAppendDocuments3 : function () {
      for (var i = 0; i < 10; ++i) {
        collection.save({_key: "test" + i});
      }
      var bindVars = {"@collection" : collectionName};
      var actual = getQueryResults("LET tmp = (FOR x IN @@collection RETURN x._id) RETURN APPEND('stringvalue', tmp)", bindVars);
      assertEqual(actual.length, 1);
      actual = actual[0];
      assertEqual(actual.length, 10);
      actual = actual.sort(function(l, r) { if (l < r) { return -1;} else if (l > r) { return 1;} return 0; });
      for (i = 0; i < 10; ++i) {
        assertEqual(actual[i], collectionName + "/test"+i);
      }
    },

    testAppendSecondUnique : function () {
      var actual = getQueryResults("RETURN APPEND('stringvalue', [1,1,1,1,1,1], true)");
      assertEqual(actual.length, 1);
      actual = actual[0];
      assertEqual(actual, null);
    },



////////////////////////////////////////////////////////////////////////////////
/// @brief test append function
////////////////////////////////////////////////////////////////////////////////

    testAppendInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN APPEND()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN APPEND([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN APPEND([ ], [ ], false, [ ])"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(APPEND())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(APPEND([ ]))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(APPEND([ ], [ ], false, [ ]))"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(APPEND()))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(APPEND([ ])))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(APPEND([ ], [ ], false, [ ])))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeValues function
////////////////////////////////////////////////////////////////////////////////

    testRemoveValues : function () {
      var data = [
        [ [ ], [ [ ], [ ] ] ],
        [ [ ], [ [ ], [ "1", "2" ] ] ],
        [ [ "1", "2" ], [ [ "1", "2" ], [ ] ] ],
        [ [ "1", "2" ], [ [ "1", "2" ], [ 1, 2 ] ] ],
        [ [ 1, 2 ], [ [ 1, 2 ], [ "1", "2" ] ] ],
        [ [ 2 ], [ [ 1, 2 ], [ "1", "2", 1 ] ] ],
        [ [ "1", "2" ], [ [ "1", "2" ], [ "foo", 1] ] ],
        [ [ "foo" ], [ [ "foo" ], [ "bar", "baz" ] ] ],
        [ [ ], [ [ "foo" ], [ "foo", "bar", "baz" ] ] ],
        [ [ ], [ [ "foo" ], [ "foo", "bar", "baz" ] ] ],
        [ [ ], [ [ "foo" ], [ "foo", "bar", "baz", "foo" ] ] ],
        [ [ "foo", "bar" ], [ [ "foo", "bar" ], [ "baz" ] ] ],
        [ [ ], [ null, [ "foo", "bar", "baz" ] ] ],
        [ [ "foo", "bar", "baz" ], [ [ "foo", "bar", "baz" ], null ] ],
        [ [ ], [ [ ], [ "foo", "bar", "baz" ] ] ],
        [ [ "foo", "bar", "baz" ], [ [ "foo", "bar", "baz" ], [ ] ] ],
        [ [ "foo", "bar", "baz" ], [ [ "foo", "bar", "baz" ], [ "one", "two", "three" ] ] ],
        [ [ "foo", "bar", "baz" ], [ [ "foo", null, "bar", "baz" ], [ "one", "two", null, "three" ] ] ],
        [ [ ], [ [ "two", "one", null, "three" ], [ "one", "two", null, "three" ] ] ],
        [ [ null ], [ [ "two", "one", null, "three" ], [ "one", "two", "three" ] ] ],
        [ [ "four", "five" ], [ [ "two", "four", "one", null, "three", "five" ], [ "one", "two", null, "three" ] ] ],
        [ [ ], [ [ "two", "one", null, "three" ], [ "one", "two", "four", null, "three" ] ] ]
      ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN REMOVE_VALUES(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(REMOVE_VALUES(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + "))");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(V8(REMOVE_VALUES(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")))");
        assertEqual(d[0], actual[0], d);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeValues function
////////////////////////////////////////////////////////////////////////////////

    testRemoveValuesInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REMOVE_VALUES()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REMOVE_VALUES([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REMOVE_VALUES([ ], [ ], true)"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(REMOVE_VALUES())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(REMOVE_VALUES([ ]))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(REMOVE_VALUES([ ], [ ], true))"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(REMOVE_VALUES()))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(REMOVE_VALUES([ ])))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(REMOVE_VALUES([ ], [ ], true)))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeValue function
////////////////////////////////////////////////////////////////////////////////

    testRemoveValue : function () {
      var data = [
        [ [ ], [ [ ], null ] ],
        [ [ ], [ [ ], false ] ],
        [ [ ], [ [ ], 1 ] ],
        [ [ ], [ [ ], "1" ] ],
        [ [ ], [ [ ], [ ] ] ],
        [ [ ], [ [ ], [ "1", "2" ] ] ],
        [ [ "1", "2" ], [ [ "1", "2" ], [ ] ] ],
        [ [ "1", "2" ], [ [ "1", "2" ], 1 ] ],
        [ [ "1", "2" ], [ [ "1", "2" ], 2 ] ],
        [ [ "2" ], [ [ "1", "2" ], "1" ] ],
        [ [ "1" ], [ [ "1", "2" ], "2" ] ],
        [ [ "1", "1", "1", "3" ], [ [ "1", "1", "1", "2", "2", "3" ], "2" ] ],
        [ [ "1", "1", "1", "2", "3" ], [ [ "1", "1", "1", "2", "2", "3" ], "2", 1 ] ],
        [ [ ], [ [ "foo" ], "foo" ] ],
        [ [ "bar" ], [ [ "bar" ], "foo" ] ],
        [ [ "foo" ], [ [ "foo" ], "bar" ] ],
        [ [ ], [ [ "foo", "foo", "foo" ], "foo" ] ],
        [ [ "foo", "foo" ], [ [ "foo", "foo", "foo" ], "foo", 1 ] ],
        [ [ "foo" ], [ [ "foo", "foo", "foo" ], "foo", 2 ] ],
        [ [ ], [ [ "foo", "foo", "foo" ], "foo", 3 ] ],
        [ [ ], [ [ "foo", "foo", "foo" ], "foo", 496 ] ],
        [ [ "bar", "foo", "bam", "foo", "baz" ], [ [ "bar", "foo", "foo", "bam", "foo", "baz" ], "foo", 1 ] ],
        [ [ "bar", "bam", "baz", "foo" ], [ [ "bar", "bam", "baz", "foo", "foo" ], "foo", 1 ] ],
        [ [ "bar", "bam", "baz" ], [ [ "bar", "bam", "baz", "foo", "foo" ], "foo", 2 ] ],
        [ [ [ 1, 2, 3 ] ], [ [ [ 1, 2, 3 ] ], [ 1, 2 ] ] ],
        [ [ [ 1, 2, 3 ] ], [ [ [ 1, 2, 3 ] ], [ 1, 3, 2 ] ] ],
        [ [ ], [ [ [ 1, 2, 3 ] ], [ 1, 2, 3] ] ]
      ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN REMOVE_VALUE(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(REMOVE_VALUE(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + "))");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(V8(REMOVE_VALUE(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")))");
        assertEqual(d[0], actual[0], d);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeValue function
////////////////////////////////////////////////////////////////////////////////

    testRemoveValueInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REMOVE_VALUE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REMOVE_VALUE([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REMOVE_VALUE([ ], [ ], true, true)"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(REMOVE_VALUE())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(REMOVE_VALUE([ ]))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(REMOVE_VALUE([ ], [ ], true, true))"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(REMOVE_VALUE()))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(REMOVE_VALUE([ ])))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(REMOVE_VALUE([ ], [ ], true, true)))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeNth function
////////////////////////////////////////////////////////////////////////////////

    testRemoveNth : function () {
      var data = [
        [ [ ], [ [ ], 0 ] ],
        [ [ ], [ [ ], -1 ] ],
        [ [ ], [ [ ], -2 ] ],
        [ [ ], [ [ ], 99 ] ],
        [ [ ], [ [ ], null ] ],
        [ [ "2" ], [ [ "1", "2" ], 0 ] ],
        [ [ "1" ], [ [ "1", "2" ], 1 ] ],
        [ [ "1", "2" ], [ [ "1", "2" ], 2 ] ],
        [ [ "1", "2" ], [ [ "1", "2" ], -3 ] ],
        [ [ "2" ], [ [ "1", "2" ], -2 ] ],
        [ [ "1" ], [ [ "1", "2" ], -1 ] ],
        [ [ "1b", "1c", "2a", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], 0 ] ],
        [ [ "1a", "1c", "2a", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], 1 ] ],
        [ [ "1a", "1b", "2a", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], 2 ] ],
        [ [ "1a", "1b", "2a", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], 2.2 ] ],
        [ [ "1a", "1b", "2a", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], 2.8 ] ],
        [ [ "1a", "1b", "1c", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], 3 ] ],
        [ [ "1a", "1b", "1c", "2a", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], 4 ] ],
        [ [ "1a", "1b", "1c", "2a", "2b" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], 5 ] ],
        [ [ "1a", "1b", "1c", "2a", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], 6 ] ],
        [ [ "1a", "1b", "1c", "2a", "2b" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], -1 ] ],
        [ [ "1a", "1b", "1c", "2a", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], -2 ] ],
        [ [ "1a", "1b", "1c", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], -2.2 ] ],
        [ [ "1a", "1b", "1c", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], -2.8 ] ],
        [ [ "1a", "1b", "1c", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], -3 ] ],
        [ [ "1a", "1b", "2a", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], -4 ] ],
        [ [ "1a", "1c", "2a", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], -5 ] ],
        [ [ "1b", "1c", "2a", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], -6 ] ],
        [ [ "1a", "1b", "1c", "2a", "2b", "3" ], [ [ "1a", "1b", "1c", "2a", "2b", "3" ], -7 ] ]
      ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN REMOVE_NTH(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(REMOVE_NTH(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + "))");
        assertEqual(d[0], actual[0], d);

        actual = getQueryResults("RETURN NOOPT(V8(REMOVE_NTH(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")))");
        assertEqual(d[0], actual[0], d);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeNth function
////////////////////////////////////////////////////////////////////////////////

    testRemoveNthInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REMOVE_NTH()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REMOVE_NTH([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REMOVE_NTH([ ], 1, true)"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(REMOVE_NTH())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(REMOVE_NTH([ ]))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(REMOVE_NTH([ ], 1, true))");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(REMOVE_NTH()))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(REMOVE_NTH([ ])))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(REMOVE_NTH([ ], 1, true)))");
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlListTestSuite);

return jsunity.done();

