/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual, assertNull, assertTrue, assertFalse */
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
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;
var assertQueryWarningAndNull = helper.assertQueryWarningAndNull;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlFunctionsTestSuite () {
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
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirst1 : function () {
      var expected = [ { "the fox" : "jumped" } ];
      var actual = getQueryResults("RETURN FIRST([ { \"the fox\" : \"jumped\" }, \"over\", [ \"the dog\" ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirst2 : function () {
      var expected = [ "over" ];
      var actual = getQueryResults("RETURN FIRST([ \"over\", [ \"the dog\" ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirst3 : function () {
      var expected = [ [ "the dog" ] ];
      var actual = getQueryResults("RETURN FIRST([ [ \"the dog\" ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirst4 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirstInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FIRST()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FIRST([ ], [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FIRST(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FIRST(true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FIRST(4)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FIRST(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FIRST({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLast1 : function () {
      var expected = [ [ "the dog" ] ];
      var actual = getQueryResults("RETURN LAST([ { \"the fox\" : \"jumped\" }, \"over\", [ \"the dog\" ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLast2 : function () {
      var expected = [ "over" ];
      var actual = getQueryResults("RETURN LAST([ { \"the fox\" : \"jumped\" }, \"over\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLast3 : function () {
      var expected = [ { "the fox" : "jumped" } ];
      var actual = getQueryResults("RETURN LAST([ { \"the fox\" : \"jumped\" } ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLast4 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN LAST([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLastInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LAST()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LAST([ ], [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN LAST(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN LAST(true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN LAST(4)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN LAST(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN LAST({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test position function
////////////////////////////////////////////////////////////////////////////////

    testPosition : function () {
      var list = [ "foo", "bar", null, "baz", true, 42, [ "BORK" ], { code: "foo", name: "test" }, false, 0, "" ];

      var data = [
        [ "foo", 0 ],
        [ "bar", 1 ],
        [ null, 2 ],
        [ "baz", 3 ],
        [ true, 4 ],
        [ 42, 5 ],
        [ [ "BORK" ], 6 ],
        [ { code: "foo", name: "test" }, 7 ],
        [ false, 8 ],
        [ 0, 9 ],
        [ "", 10 ]
      ];

      data.forEach(function (d) {
        var search = d[0];
        var expected = d[1];
        
        // find if element is contained in list (should be true)
        var actual = getQueryResults("RETURN POSITION(@list, @search, false)", { list: list, search: search });
        assertTrue(actual[0]);

        // find position of element in list
        actual = getQueryResults("RETURN POSITION(@list, @search, true)", { list: list, search: search });
        assertEqual(expected, actual[0]);
        
        // look up the element using the position
        actual = getQueryResults("RETURN NTH(@list, @position)", { list: list, position: actual[0] });
        assertEqual(search, actual[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test position function
////////////////////////////////////////////////////////////////////////////////

    testPositionNotThere : function () {
      var list = [ "foo", "bar", null, "baz", true, 42, [ "BORK" ], { code: "foo", name: "test" }, false, 0, "" ];

      var data = [ "foot", "barz", 43, 1, -42, { code: "foo", name: "test", bar: "baz" }, " ", " foo", "FOO", "bazt", 0.1, [ ], [ "bork" ] ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN POSITION(@list, @search, false)", { list: list, search: d });
        assertFalse(actual[0]);

        actual = getQueryResults("RETURN POSITION(@list, @search, true)", { list: list, search: d });
        assertEqual(-1, actual[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test position function
////////////////////////////////////////////////////////////////////////////////

    testPositionInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN POSITION()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN POSITION([ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN POSITION(null, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN POSITION(true, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN POSITION(4, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN POSITION(\"yes\", 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN POSITION({ }, 'foo')"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthEmpty : function () {
      var i;

      for (i = -3; i <= 3; ++i) {                         
        var actual = getQueryResults("RETURN NTH([ ], @pos)", { pos: i });
        assertNull(actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthNegative : function () {
      var i;

      for (i = -3; i <= 3; ++i) {                         
        var actual = getQueryResults("RETURN NTH([ 1, 2, 3, 4 ], @pos)", { pos: i });
        if (i < 0) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(i + 1, actual[0]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthBounds : function () {
      var i;

      for (i = 0; i <= 10; ++i) {                         
        var actual = getQueryResults("RETURN NTH([ 'a1', 'a2', 'a3', 'a4', 'a5' ], @pos)", { pos: i });
        if (i < 5) {
          assertEqual('a' + (i + 1), actual[0]);
        }
        else {
          assertNull(actual[0]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NTH()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NTH([ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN NTH(null, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN NTH(true, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN NTH(4, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN NTH(\"yes\", 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN NTH({ }, 1)"); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], null)")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], false)")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], true)")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], '')")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], '1234')")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], [ ])")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], { \"foo\": true })")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], { })")); 
      
      assertEqual([ 1 ], getQueryResults("RETURN NTH([ 1, 2, 3 ], false)")); 
      assertEqual([ 2 ], getQueryResults("RETURN NTH([ 1, 2, 3 ], true)")); 
      assertEqual([ 3 ], getQueryResults("RETURN NTH([ 1, 2, 3 ], '2')")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ 1, 2, 3 ], '3')")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
////////////////////////////////////////////////////////////////////////////////

    testReverse1 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN REVERSE([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
////////////////////////////////////////////////////////////////////////////////

    testReverse2 : function () {
      var expected = [ [ "fox" ] ];
      var actual = getQueryResults("RETURN REVERSE([ \"fox\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
////////////////////////////////////////////////////////////////////////////////

    testReverse3 : function () {
      var expected = [ [ false, [ "fox", "jumped" ], { "quick" : "brown" }, "the" ] ];
      var actual = getQueryResults("RETURN REVERSE([ \"the\", { \"quick\" : \"brown\" }, [ \"fox\", \"jumped\" ], false ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
//////////////////////////////////////////////////////////////////////////////// 

    testReverse4 : function () {
      var expected = [ [ 1, 2, 3 ] ];
      var actual = getQueryResults("LET a = (FOR i IN [ 1, 2, 3 ] RETURN i) LET b = REVERSE(a) RETURN a");
      // make sure reverse does not modify the original value
      assertEqual(expected, actual);
      
      actual = getQueryResults("LET a = (FOR i IN [ 1, 2, 3] RETURN i) LET b = REVERSE(a) RETURN b");
      assertEqual([ expected[0].reverse() ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
////////////////////////////////////////////////////////////////////////////////

    testReverseInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REVERSE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REVERSE([ ], [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN REVERSE(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN REVERSE(true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN REVERSE(4)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN REVERSE({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUnique1 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN UNIQUE([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUnique2 : function () {
      var expected = [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, false, true, null, "fox", "FOX", "Fox", "FoX", [0], [1], { "the fox" : "jumped" } ] ];
      var actual = getQueryResults("RETURN UNIQUE([ 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, false, true, null, \"fox\", \"FOX\", \"Fox\", \"FoX\", [ 0 ], [ 1 ], { \"the fox\" : \"jumped\" } ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUnique3 : function () {
      var expected = [ [ 1, 2, 3, 4, 5, 7, 9, 42, -1, -33 ] ];
      var actual = getQueryResults("RETURN UNIQUE([ 1, -1, 1, 2, 3, -1, 2, 3, 4, 5, 1, 3, 9, 2, -1, 9, -33, 42, 7 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUnique4 : function () {
      var expected = [ [ [1, 2, 3], [3, 2, 1], [2, 1, 3], [2, 3, 1], [1, 3, 2], [3, 1, 2] ] ];
      var actual = getQueryResults("RETURN UNIQUE([ [ 1, 2, 3 ], [ 3, 2, 1 ], [ 2, 1, 3 ], [ 2, 3, 1 ], [ 1, 2, 3 ], [ 1, 3, 2 ], [ 2, 3, 1 ], [ 3, 1, 2 ], [ 2 , 1, 3 ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUnique5 : function () {
      var expected = [ [ { "the fox" : "jumped" }, { "the fox" : "jumped over" }, { "over" : "the dog", "the fox" : "jumped" }]];

      var actual = getQueryResults("RETURN UNIQUE([ { \"the fox\" : \"jumped\" }, { \"the fox\" : \"jumped over\" }, { \"the fox\" : \"jumped\", \"over\" : \"the dog\" }, { \"over\" : \"the dog\", \"the fox\" : \"jumped\" } ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUniqueInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNIQUE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNIQUE([ ], [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN UNIQUE(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN UNIQUE(true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN UNIQUE(4)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN UNIQUE(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN UNIQUE({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test slice function
////////////////////////////////////////////////////////////////////////////////

    testSlice : function () {
      var actual;
      
      actual = getQueryResults("RETURN SLICE([ ], 0, 1)");
      assertEqual([ [ ] ], actual);

      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 0, 1)");
      assertEqual([ [ 1 ] ], actual);

      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 0, 2)");
      assertEqual([ [ 1, 2 ] ], actual);
      
      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 1, 2)");
      assertEqual([ [ 2, 3 ] ], actual);
      
      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 0)");
      assertEqual([ [ 1, 2, 3, 4, 5 ] ], actual);
      
      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 3)");
      assertEqual([ [ 4, 5 ] ], actual);
      
      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 0, -1)");
      assertEqual([ [ 1, 2, 3, 4 ] ], actual);
      
      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 0, -2)");
      assertEqual([ [ 1, 2, 3 ] ], actual);
      
      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 2, -1)");
      assertEqual([ [ 3, 4 ] ], actual);
      
      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 10)");
      assertEqual([ [ ] ], actual);
      
      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 1000)");
      assertEqual([ [ ] ], actual);
      
      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], -1000)");
      assertEqual([ [ 1, 2, 3, 4, 5 ] ], actual);
      
      actual = getQueryResults("RETURN SLICE([ 1, 2, 3, 4, 5 ], 1, -10)");
      assertEqual([ [ ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test slice function
////////////////////////////////////////////////////////////////////////////////

    testSliceInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SLICE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SLICE(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SLICE(1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SLICE('foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SLICE({ })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SLICE([ ])"); 
      assertEqual([ [ ] ], getQueryResults("RETURN SLICE([ ], { })")); 
      assertEqual([ [ ] ], getQueryResults("RETURN SLICE([ ], true)")); 
      assertEqual([ [ ] ], getQueryResults("RETURN SLICE([ ], 'foo')")); 
      assertEqual([ [ ] ], getQueryResults("RETURN SLICE([ ], [ ])")); 
      assertEqual([ [ ] ], getQueryResults("RETURN SLICE([ ], { })")); 
      assertEqual([ [ ] ], getQueryResults("RETURN SLICE([ ], 1, false)")); 
      assertEqual([ [ ] ], getQueryResults("RETURN SLICE([ ], 1, 'foo')")); 
      assertEqual([ [ ] ], getQueryResults("RETURN SLICE([ ], 1, [ ])")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function
////////////////////////////////////////////////////////////////////////////////

    testLength1 : function () {
      var expected = [ 0, 0, 0 ];
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ ] RETURN q)) RETURN LENGTH(quarters)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function
////////////////////////////////////////////////////////////////////////////////

    testLength2 : function () {
      var expected = [ 4, 4, 4 ];
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] RETURN q)) RETURN LENGTH(quarters)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function
////////////////////////////////////////////////////////////////////////////////

    testLength3 : function () {
      var expected = [ 3, 2, 2, 1, 0 ];
      var actual = getQueryResults("FOR test IN [ { 'a' : 1, 'b' : 2, 'c' : null }, { 'baz' : [ 1, 2, 3, 4, 5 ], 'bar' : false }, { 'boom' : { 'bang' : false }, 'kawoom' : 0.0 }, { 'meow' : { } }, { } ] RETURN LENGTH(test)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function for strings
////////////////////////////////////////////////////////////////////////////////

    testLength4 : function () {
      var expected = [ 0, 1, 3, 3, 4, 5 ];
      var actual = getQueryResults("FOR test IN [ '', ' ', 'foo', 'bar', 'meow', 'mötör' ] RETURN LENGTH(test)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function for documents
////////////////////////////////////////////////////////////////////////////////

    testLength5 : function () {
      var expected = [ 0, 1, 3, 3, 4, 5 ];
      var actual = getQueryResults("FOR test IN [ { }, { foo: true }, { foo: 1, bar: 2, baz: 3 }, { one: { }, two: [ 1, 2, 3 ], three: { one: 1, two: 2 } }, { '1': 0, '2': 0, '3': 0, '4': 0 }, { abc: false, ABC: false, aBc: false, AbC: false, ABc: true } ] RETURN LENGTH(test)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function
////////////////////////////////////////////////////////////////////////////////

    testLengthInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LENGTH()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LENGTH([ ], [ ])"); 
      assertEqual([ 4 ], getQueryResults("RETURN LENGTH(null)")); 
      assertEqual([ 4 ], getQueryResults("RETURN LENGTH(true)")); 
      assertEqual([ 1 ], getQueryResults("RETURN LENGTH(4)")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test keep function
////////////////////////////////////////////////////////////////////////////////
    
    testKeepInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN KEEP()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN KEEP({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP(null, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP(1, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP(false, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP(1, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP('bar', 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP('foo', 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP([ ], 'foo', { })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test keep function
////////////////////////////////////////////////////////////////////////////////
    
    testKeep : function () {
      var actual;

      actual = getQueryResults("FOR i IN [ { }, { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN KEEP(i, 'foo', 'bar', 'baz', [ 'meow' ], [ ])");
      assertEqual([ { }, { bar: 2, foo: 1, meow: 6 }, { foo: 0, meow: 2 }, { foo: null }, { foo: true }, { } ], actual);
      
      actual = getQueryResults("FOR i IN [ { }, { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN KEEP(i, [ 'foo', 'bar', 'baz', 'meow' ])");
      assertEqual([ { }, { bar: 2, foo: 1, meow: 6 }, { foo: 0, meow: 2 }, { foo: null }, { foo: true }, { } ], actual);
      
      actual = getQueryResults("FOR i IN [ { }, { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN KEEP(i, 'foo', 'bar', 'baz', 'meow')");
      assertEqual([ { }, { bar: 2, foo: 1, meow: 6 }, { foo: 0, meow: 2 }, { foo: null }, { foo: true }, { } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unset function
////////////////////////////////////////////////////////////////////////////////
    
    testUnsetInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSET()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSET({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET(null, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET(false, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET(1, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET(1, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET('bar', 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET('foo', 'bar')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET([ ], 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET('foo', 'foo')"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unset function
////////////////////////////////////////////////////////////////////////////////
    
    testUnset : function () {
      var expected = [ { bang: 5, goof: 4, moo: 3 }, { goof: 1 }, { }, { }, { goof: null } ];
      var actual;

      actual = getQueryResults("FOR i IN [ { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN UNSET(i, 'foo', 'bar', 'baz', [ 'meow' ], [ ])");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR i IN [ { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN UNSET(i, [ 'foo', 'bar', 'baz', 'meow' ])");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR i IN [ { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN UNSET(i, 'foo', 'bar', 'baz', 'meow')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge1 : function () {
      var expected = [ { "quarter" : 1, "year" : 2010 }, { "quarter" : 2, "year" : 2010 }, { "quarter" : 3, "year" : 2010 }, { "quarter" : 4, "year" : 2010 }, { "quarter" : 1, "year" : 2011 }, { "quarter" : 2, "year" : 2011 }, { "quarter" : 3, "year" : 2011 }, { "quarter" : 4, "year" : 2011 }, { "quarter" : 1, "year" : 2012 }, { "quarter" : 2, "year" : 2012 }, { "quarter" : 3, "year" : 2012 }, { "quarter" : 4, "year" : 2012 } ]; 
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FOR quarter IN [ 1, 2, 3, 4 ] return MERGE({ \"year\" : year }, { \"quarter\" : quarter })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge2 : function () {
      var expected = [ { "age" : 15, "isAbove18" : false, "name" : "John" }, { "age" : 19, "isAbove18" : true, "name" : "Corey" } ];
      var actual = getQueryResults("FOR u IN [ { \"name\" : \"John\", \"age\" : 15 }, { \"name\" : \"Corey\", \"age\" : 19 } ] return MERGE(u, { \"isAbove18\" : u.age >= 18 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge3 : function () {
      var expected = [ { "age" : 15, "id" : 9, "name" : "John" }, { "age" : 19, "id" : 9, "name" : "Corey" } ];
      var actual = getQueryResults("FOR u IN [ { \"id\" : 100, \"name\" : \"John\", \"age\" : 15 }, { \"id\" : 101, \"name\" : \"Corey\", \"age\" : 19 } ] return MERGE(u, { \"id\" : 9 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge4 : function () {
      var expected = [ { "age" : 15, "id" : 33, "name" : "foo" }, { "age" : 19, "id" : 33, "name" : "foo" } ];
      var actual = getQueryResults("FOR u IN [ { \"id\" : 100, \"name\" : \"John\", \"age\" : 15 }, { \"id\" : 101, \"name\" : \"Corey\", \"age\" : 19 } ] return MERGE(u, { \"id\" : 9 }, { \"name\" : \"foo\", \"id\" : 33 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////

    testMergeInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MERGE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MERGE({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(null, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(true, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(3, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(\"yes\", { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE([ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive1 : function () {
      var doc1 = "{ \"black\" : { \"enabled\" : true, \"visible\": false }, \"white\" : { \"enabled\" : true}, \"list\" : [ 1, 2, 3, 4, 5 ] }";
      var doc2 = "{ \"black\" : { \"enabled\" : false }, \"list\": [ 6, 7, 8, 9, 0 ] }";

      var expected = [ { "black" : { "enabled" : false, "visible" : false }, "list" : [ 6, 7, 8, 9, 0 ], "white" : { "enabled" : true } } ];

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive2 : function () {
      var doc1 = "{ \"black\" : { \"enabled\" : true, \"visible\": false }, \"white\" : { \"enabled\" : true}, \"list\" : [ 1, 2, 3, 4, 5 ] }";
      var doc2 = "{ \"black\" : { \"enabled\" : false }, \"list\": [ 6, 7, 8, 9, 0 ] }";

      var expected = [ { "black" : { "enabled" : true, "visible" : false }, "list" : [ 1, 2, 3, 4, 5 ], "white" : { "enabled" : true } } ];

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc2 + ", " + doc1 + ")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive3 : function () {
      var doc1 = "{ \"a\" : 1, \"b\" : 2, \"c\" : 3 }";

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc1 + ")");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc1 + ", " + doc1 + ")");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc1 + ", " + doc1 + ", " + doc1 + ")");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive4 : function () {
      var doc1 = "{ \"a\" : 1, \"b\" : 2, \"c\" : 3 }";
      var doc2 = "{ \"a\" : 2, \"b\" : 3, \"c\" : 4 }";
      var doc3 = "{ \"a\" : 3, \"b\" : 4, \"c\" : 5 }";

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ", " + doc3 + ")");
      assertEqual([ { "a" : 3, "b" : 4, "c" : 5 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc3 + ", " + doc2 + ")");
      assertEqual([ { "a" : 2, "b" : 3, "c" : 4 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc2 + ", " + doc3 + ", " + doc1 + ")");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc3 + ", " + doc1 + ", " + doc2 + ")");
      assertEqual([ { "a" : 2, "b" : 3, "c" : 4 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive5 : function () {
      var doc1 = "{ \"a\" : 1, \"b\" : 2, \"c\" : 3 }";
      var doc2 = "{ \"1\" : 7, \"b\" : 8, \"y\" : 9 }";
      var doc3 = "{ \"x\" : 4, \"y\" : 5, \"z\" : 6 }";

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ", " + doc3 + ")");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 8, "c" : 3, "x" : 4, "y": 5, "z" : 6 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc3 + ", " + doc2 + ")");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 8, "c" : 3, "x" : 4, "y": 9, "z" : 6 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc2 + ", " + doc3 + ", " + doc1 + ")");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 2, "c" : 3, "x" : 4, "y": 5, "z" : 6 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc3 + ", " + doc1 + ", " + doc2 + ")");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 8, "c" : 3, "x" : 4, "y": 9, "z" : 6 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive6 : function () {
      var doc1 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"DE\" : { \"city\" : \"Cologne\" } } } } }";
      var doc2 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"DE\" : { \"city\" : \"Frankfurt\" } } } } }";
      var doc3 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"DE\" : { \"city\" : \"Munich\" } } } } }";
      var doc4 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"UK\" : { \"city\" : \"Manchester\" } } } } }";
      var doc5 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"UK\" : { \"city\" : \"London\" } } } } }";
      var doc6 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"FR\" : { \"city\" : \"Paris\" } } } } }";
      var doc7 = "{ \"continent\" : { \"Asia\" : { \"country\" : { \"CN\" : { \"city\" : \"Beijing\" } } } } }";
      var doc8 = "{ \"continent\" : { \"Asia\" : { \"country\" : { \"CN\" : { \"city\" : \"Shanghai\" } } } } }";
      var doc9 = "{ \"continent\" : { \"Asia\" : { \"country\" : { \"JP\" : { \"city\" : \"Tokyo\" } } } } }";
      var doc10 = "{ \"continent\" : { \"Australia\" : { \"country\" : { \"AU\" : { \"city\" : \"Sydney\" } } } } }";
      var doc11 ="{ \"continent\" : { \"Australia\" : { \"country\" : { \"AU\" : { \"city\" : \"Melbourne\" } } } } }";
      var doc12 ="{ \"continent\" : { \"Africa\" : { \"country\" : { \"EG\" : { \"city\" : \"Cairo\" } } } } }";

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ", " + doc3 + ", " + doc4 + ", " + doc5 + ", " + doc6 + ", " + doc7 + ", " + doc8 + ", " + doc9 + ", " + doc10 + ", " + doc11 + ", " + doc12 + ")");

      assertEqual([ { "continent" : { 
        "Europe" : { "country" : { "DE" : { "city" : "Munich" }, "UK" : { "city" : "London" }, "FR" : { "city" : "Paris" } } }, 
        "Asia" : { "country" : { "CN" : { "city" : "Shanghai" }, "JP" : { "city" : "Tokyo" } } }, 
        "Australia" : { "country" : { "AU" : { "city" : "Melbourne" } } }, 
        "Africa" : { "country" : { "EG" : { "city" : "Cairo" } } } 
      } } ], actual);
    },

      
////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////

    testMergeRecursiveInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MERGE_RECURSIVE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MERGE_RECURSIVE({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(null, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(true, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(3, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(\"yes\", { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE([ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test translate function
////////////////////////////////////////////////////////////////////////////////
    
    testTranslate1 : function () {
      var tests = [
        { value: "foo", lookup: { }, expected: "foo" },
        { value: "foo", lookup: { foo: "bar" }, expected: "bar" },
        { value: "foo", lookup: { bar: "foo" }, expected: "foo" },
        { value: 1, lookup: { 1: "one", 2: "two" }, expected: "one" },
        { value: 1, lookup: { "1": "one", "2": "two" }, expected: "one" },
        { value: "1", lookup: { 1: "one", 2: "two" }, expected: "one" },
        { value: "1", lookup: { "1": "one", "2": "two" }, expected: "one" },
        { value: null, lookup: { foo: "bar", bar: "foo" }, expected: null },
        { value: "foobar", lookup: { foo: "bar", bar: "foo" }, expected: "foobar" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: "replaced!" }, expected: "replaced!" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: null }, expected: null },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: [ 1, 2, 3 ] }, expected: [ 1, 2, 3 ] },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: { thefoxx: "is great" } }, expected: { thefoxx: "is great" } },
        { value: "one", lookup: { one: "two", two: "three", three: "four" }, expected: "two" },
        { value: null, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: null },
        { value: false, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: false },
        { value: 0, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: 0 },
        { value: "", lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: "" },
        { value: "CGN", lookup: { "DUS" : "Duessldorf", "FRA" : "Frankfurt", "CGN" : "Cologne" }, expected: "Cologne" }
      ];

      tests.forEach(function(t) {
        var actual = getQueryResults("RETURN TRANSLATE(@value, @lookup)", { value: t.value, lookup: t.lookup });
        assertEqual(t.expected, actual[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test translate function
////////////////////////////////////////////////////////////////////////////////
    
    testTranslate2 : function () {
      var i, lookup = { };
      for (i = 0; i < 100; ++i) {
        lookup["test" + i] = "the quick brown Foxx:" + i;
      }

      for (i = 0; i < 100; ++i) {
        var actual = getQueryResults("RETURN TRANSLATE(@value, @lookup)", { value: "test" + i, lookup: lookup });
        assertEqual(lookup["test" + i], actual[0]);
        assertEqual("the quick brown Foxx:" + i, actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test translate function
////////////////////////////////////////////////////////////////////////////////
    
    testTranslate3 : function () {
      var i, lookup = { };
      for (i = 0; i < 100; ++i) {
        lookup[i] = i * i;
      }

      for (i = 0; i < 100; ++i) {
        var actual = getQueryResults("RETURN TRANSLATE(@value, @lookup, @def)", { value: i, lookup: lookup, def: "fail!" });
        assertEqual(lookup[i], actual[0]);
        assertEqual(i * i, actual[0]);
        
        actual = getQueryResults("RETURN TRANSLATE(@value, @lookup, @def)", { value: "test" + i, lookup: lookup, def: "fail" + i });
        assertEqual("fail" + i, actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test translate function
////////////////////////////////////////////////////////////////////////////////
    
    testTranslateDefault : function () {
      var tests = [
        { value: "foo", lookup: { }, expected: "bar", def: "bar" },
        { value: "foo", lookup: { foo: "bar" }, expected: "bar", def: "lol" },
        { value: "foo", lookup: { bar: "foo" }, expected: "lol", def: "lol" },
        { value: 1, lookup: { 1: "one", 2: "two" }, expected: "one", def: "foobar" },
        { value: 1, lookup: { "1": "one", "2": "two" }, expected: "one", def: "test" },
        { value: 1, lookup: { "3": "one", "2": "two" }, expected: "test", def: "test" },
        { value: "1", lookup: { 1: "one", 2: "two" }, expected: "one", def: "test" },
        { value: "1", lookup: { 3: "one", 2: "two" }, expected: "test", def: "test" },
        { value: "1", lookup: { "1": "one", "2": "two" }, expected: "one", def: "test" },
        { value: "1", lookup: { "3": "one", "2": "two" }, expected: "test", def: "test" },
        { value: null, lookup: { foo: "bar", bar: "foo" }, expected: 1, def: 1 },
        { value: null, lookup: { foo: "bar", bar: "foo" }, expected: "foobar", def: "foobar" },
        { value: "foobar", lookup: { foo: "bar", bar: "foo" }, expected: "foobart", def: "foobart" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: "replaced!" }, expected: "replaced!", def: "foobart" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: null }, expected: null, def: "foobaz" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: [ 1, 2, 3 ] }, expected: [ 1, 2, 3 ], def: null },
        { value: "foobaz", lookup: { foobar: "bar" }, expected: null, def: null },
        { value: "foobaz", lookup: { }, expected: "FOXX", def: "FOXX" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: { thefoxx: "is great" } }, expected: { thefoxx: "is great" }, def: null },
        { value: "one", lookup: { one: "two", two: "three", three: "four" }, expected: "two", def: "foo" },
        { value: null, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: "bla", def: "bla" },
        { value: false, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: true, def: true },
        { value: 0, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: 42, def: 42 },
        { value: "", lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: "three", def: "three" },
        { value: "CGN", lookup: { "DUS" : "Duessldorf", "FRA" : "Frankfurt", "MUC" : "Munich" }, expected: "Cologne", def: "Cologne" }
      ];

      tests.forEach(function(t) {
        var actual = getQueryResults("RETURN TRANSLATE(@value, @lookup, @def)", { value: t.value, lookup: t.lookup, def: t.def });
        assertEqual(t.expected, actual[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////

    testTranslateInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TRANSLATE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TRANSLATE('foo')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TRANSLATE('foo', { }, '', 'baz')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE({ }, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', '')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', [])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnion1 : function () {
      var expected = [ [ 1, 2, 3, 1, 2, 3 ] ];
      var actual = getQueryResults("RETURN UNION([ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnion2 : function () {
      var expected = [ [ 1, 2, 3, 3, 2, 1 ] ];
      var actual = getQueryResults("RETURN UNION([ 1, 2, 3 ], [ 3, 2, 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnion3 : function () {
      var expected = [ "Fred", "John", "John", "Amy" ];
      var actual = getQueryResults("FOR u IN UNION([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"]) RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////

    testUnionInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNION()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNION([ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(null, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(true, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(3, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(\"yes\", [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION({ }, [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function indexed access
////////////////////////////////////////////////////////////////////////////////
    
    testUnionIndexedAccess1 : function () {
      var expected = [ "Fred" ];
      var actual = getQueryResults("RETURN UNION([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"])[0]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function indexed access
////////////////////////////////////////////////////////////////////////////////
    
    testUnionIndexedAccess2 : function () {
      var expected = [ "John" ];
      var actual = getQueryResults("RETURN UNION([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"])[1]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function indexed access
////////////////////////////////////////////////////////////////////////////////
    
    testUnionIndexedAccess3 : function () {
      var expected = [ "bar" ];
      var actual = getQueryResults("RETURN UNION([ { title : \"foo\" } ], [ { title : \"bar\" } ])[1].title");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct1 : function () {
      var expected = [ [ 1, 2, 3, ] ];
      var actual = getQueryResults("RETURN UNION_DISTINCT([ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct2 : function () {
      var expected = [ [ 1, 2, 3 ] ];
      var actual = getQueryResults("RETURN UNION_DISTINCT([ 1, 2, 3 ], [ 3, 2, 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct3 : function () {
      var expected = [ "Fred", "John", "Amy" ];
      var actual = getQueryResults("FOR u IN UNION_DISTINCT([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"]) RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct4 : function () {
      var expected = [ [ 1, 2, 3, 4, 5, 6 ] ];
      var actual = getQueryResults("RETURN UNION_DISTINCT([ 1, 2, 3 ], [ 3, 2, 1 ], [ 4 ], [ 5, 6, 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct5 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN UNION_DISTINCT([ ], [ ], [ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct6 : function () {
      var expected = [ [ false, true ] ];
      var actual = getQueryResults("RETURN UNION_DISTINCT([ ], [ false ], [ ], [ true ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////

    testUnionDistinctInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNION_DISTINCT()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNION_DISTINCT([ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(null, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(true, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(3, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(\"yes\", [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT({ }, [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange1 : function () {
      var expected = [ [ 1 ] ];
      var actual = getQueryResults("RETURN RANGE(1, 1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange2 : function () {
      var expected = [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ];
      var actual = getQueryResults("RETURN RANGE(1, 10)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange3 : function () {
      var expected = [ [ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ];
      var actual = getQueryResults("RETURN RANGE(-1, 10)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange4 : function () {
      var expected = [ [ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1 ] ];
      var actual = getQueryResults("RETURN RANGE(10, -1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange5 : function () {
      var expected = [ [ 0 ] ];
      var actual = getQueryResults("RETURN RANGE(0, 0)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange6 : function () {
      var expected = [ [ 10 ] ];
      var actual = getQueryResults("RETURN RANGE(10, 10, 5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange7 : function () {
      var expected = [ [ 10, 15, 20, 25, 30 ] ];
      var actual = getQueryResults("RETURN RANGE(10, 30, 5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange8 : function () {
      var expected = [ [ 30, 25, 20, 15, 10 ] ];
      var actual = getQueryResults("RETURN RANGE(30, 10, -5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange9 : function () {
      var expected = [ [ 3.4, 4.6, 5.8, 7.0, 8.2 ] ];
      var actual = getQueryResults("RETURN RANGE(3.4, 8.9, 1.2)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////

    testRangeInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RANGE()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RANGE(1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RANGE(1, 2, 3, 4)"); 
      assertEqual([ [ 0, 1, 2 ] ], getQueryResults("RETURN RANGE(null, 2)"));
      assertEqual([ [ 0 ] ], getQueryResults("RETURN RANGE(null, null)"));
      assertEqual([ [ 0, 1, 2 ] ], getQueryResults("RETURN RANGE(false, 2)"));
      assertEqual([ [ 1, 2 ] ], getQueryResults("RETURN RANGE(true, 2)"));
      assertEqual([ [ 1 ] ], getQueryResults("RETURN RANGE(1, true)"));
      assertEqual([ [ 1, 0 ] ], getQueryResults("RETURN RANGE(1, [ ])"));
      assertEqual([ [ 1, 0 ] ], getQueryResults("RETURN RANGE(1, { })"));
      
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(1, 1, 0)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(-1, -1, 0)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(1, -1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(-1, 1, -1)");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus1 : function () {
      var expected = [ [ 'b', 'd' ] ];
      var actual = getQueryResults("RETURN MINUS([ 'a', 'b', 'c', 'd' ], [ 'c' ], [ 'a', 'c', 'e' ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus2 : function () {
      var expected = [ [ 'a', 'b' ] ];
      var actual = getQueryResults("RETURN MINUS([ 'a', 'b', 'c' ], [ 'c', 'd', 'e' ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus3 : function () {
      var expected = [ [ 'a', 'b', 'c' ] ];
      var actual = getQueryResults("RETURN MINUS([ 'a', 'b', 'c' ], [ 1, 2, 3 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus4 : function () {
      var expected = [ [ 'a', 'b', 'c' ] ];
      var actual = getQueryResults("RETURN MINUS([ 'a', 'b', 'c' ], [ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus5 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN MINUS([ 2 ], [ 'a', 'b', 'c' ], [ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus6 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN MINUS([ ], [ 'a', 'b', 'c' ], [ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection1 : function () {
      var expected = [ [ 1, -3 ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1, -3 ], [ -3, 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection2 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ ], [ 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection3 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1 ], [  ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection4 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1 ], [ 2, 3, 1 ], [ 4, 5, 6 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection5 : function () {
      var expected = [ [ 2, 4 ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1, 3, 2, 4 ], [ 2, 3, 1, 4 ], [ 4, 5, 6, 2 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection6 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ [ 1, 2 ] ], [ 2, 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection7 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ [ 1, 2 ] ], [ 1, 2 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection8 : function () {
      var expected = [ [ [ 1, 2 ] ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ [ 1, 2 ] ], [ [ 1, 2 ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection9 : function () {
      var expected = [ [ { foo: 'test' } ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ { foo: 'bar' }, { foo: 'test' } ], [ { foo: 'test' } ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection10 : function () {
      var expected = [ [ 2, 4, 5 ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1, 2, 3, 3, 4, 4, 5, 1 ], [ 2, 4, 5 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test flatten function
////////////////////////////////////////////////////////////////////////////////
    
    testFlattenInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FLATTEN()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FLATTEN([ ], 1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FLATTEN(1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FLATTEN(null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FLATTEN(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FLATTEN(1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FLATTEN('foo')");
      assertEqual([ [ ] ], getQueryResults("RETURN FLATTEN([ ], 'foo')"));
      assertEqual([ [ ] ], getQueryResults("RETURN FLATTEN([ ], [ ])"));
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN FLATTEN({ })");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test flatten function
////////////////////////////////////////////////////////////////////////////////
    
    testFlatten1 : function () {
      var input = [ [ 2, 4, 5 ], [ 1, 2, 3 ], [ ], [ "foo", "bar" ] ];
      var actual = getQueryResults("RETURN FLATTEN(@value)", { value: input });
      assertEqual([ [ 2, 4, 5, 1, 2, 3, "foo", "bar" ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 1)", { value: input });
      assertEqual([ [ 2, 4, 5, 1, 2, 3, "foo", "bar" ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test flatten function
////////////////////////////////////////////////////////////////////////////////
    
    testFlatten2 : function () {
      var input = [ [ 1, 1, 2, 2 ], [ 1, 1, 2, 2 ], [ null, null ], [ [ 1, 2 ], [ 3, 4 ] ] ];
      var actual = getQueryResults("RETURN FLATTEN(@value)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, null, null, [ 1, 2 ], [ 3, 4 ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 1)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, null, null, [ 1, 2 ], [ 3, 4 ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 2)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, null, null, 1, 2, 3, 4 ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test flatten_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testFlatten3 : function () {
      var input = [ [ 1, 1, 2, 2 ], [ 1, 1, 2, 2 ], [ [ 1, 2 ], [ 3, 4 ], [ [ 5, 6 ], [ 7, 8 ] ], [ 9, 10, [ 11, 12 ] ] ] ];
      var actual = getQueryResults("RETURN FLATTEN(@value, 1)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, [ 1, 2 ], [ 3, 4 ], [ [ 5, 6 ], [ 7, 8 ] ], [ 9, 10, [ 11, 12 ] ] ] ], actual);

      actual = getQueryResults("RETURN FLATTEN(@value, 2)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, 1, 2, 3, 4, [ 5, 6 ], [ 7, 8 ], 9, 10, [ 11, 12 ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 3)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test flatten_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testFlatten4 : function () {
      var input = [ 1, [ 2, [ 3, [ 4, [ 5, [ 6, [ 7, [ 8, [ 9 ] ] ] ] ] ] ] ] ];
      var actual = getQueryResults("RETURN FLATTEN(@value, 1)", { value: input });
      assertEqual([ [ 1, 2, [ 3, [ 4, [ 5, [ 6, [ 7, [ 8, [ 9 ] ] ] ] ] ] ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 2)", { value: input });
      assertEqual([ [ 1, 2, 3, [ 4, [ 5, [ 6, [ 7, [ 8, [ 9 ] ] ] ] ] ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 3)", { value: input });
      assertEqual([ [ 1, 2, 3, 4, [ 5, [ 6, [ 7, [ 8, [ 9 ] ] ] ] ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 4)", { value: input });
      assertEqual([ [ 1, 2, 3, 4, 5, [ 6, [ 7, [ 8, [ 9 ] ] ] ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 5)", { value: input });
      assertEqual([ [ 1, 2, 3, 4, 5, 6, [ 7, [ 8, [ 9 ] ] ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 6)", { value: input });
      assertEqual([ [ 1, 2, 3, 4, 5, 6, 7, [ 8, [ 9 ] ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 7)", { value: input });
      assertEqual([ [ 1, 2, 3, 4, 5, 6, 7, 8, [ 9 ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 8)", { value: input });
      assertEqual([ [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse identifier function
////////////////////////////////////////////////////////////////////////////////
    
    testParseIdentifier : function () {
      var actual;

      actual = getQueryResults("RETURN PARSE_IDENTIFIER('foo/bar')");
      assertEqual([ { collection: 'foo', key: 'bar' } ], actual);
      
      actual = getQueryResults("RETURN PARSE_IDENTIFIER('this-is-a-collection-name/and-this-is-an-id')");
      assertEqual([ { collection: 'this-is-a-collection-name', key: 'and-this-is-an-id' } ], actual);
      
      actual = getQueryResults("RETURN PARSE_IDENTIFIER('MY_COLLECTION/MY_DOC')");
      assertEqual([ { collection: 'MY_COLLECTION', key: 'MY_DOC' } ], actual);

      actual = getQueryResults("RETURN PARSE_IDENTIFIER('_users/AbC')");
      assertEqual([ { collection: '_users', key: 'AbC' } ], actual);
      
      actual = getQueryResults("RETURN PARSE_IDENTIFIER({ _id: 'foo/bar', value: 'baz' })");
      assertEqual([ { collection: 'foo', key: 'bar' } ], actual);

      actual = getQueryResults("RETURN PARSE_IDENTIFIER({ ignore: true, _id: '_system/VALUE', value: 'baz' })");
      assertEqual([ { collection: '_system', key: 'VALUE' } ], actual);

      actual = getQueryResults("RETURN PARSE_IDENTIFIER({ value: 123, _id: 'Some-Odd-Collection/THIS_IS_THE_KEY' })");
      assertEqual([ { collection: 'Some-Odd-Collection', key: 'THIS_IS_THE_KEY' } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse identifier function
////////////////////////////////////////////////////////////////////////////////
    
    testParseIdentifierCollection : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      cx.save({ "title" : "123", "value" : 456, "_key" : "foobar" });
      cx.save({ "_key" : "so-this-is-it", "title" : "nada", "value" : 123 });

      var expected, actual;
      
      expected = [ { collection: cn, key: "foobar" } ];
      actual = getQueryResults("RETURN PARSE_IDENTIFIER(DOCUMENT(CONCAT(@cn, '/', @key)))", { cn: cn, key: "foobar" });
      assertEqual(expected, actual);

      expected = [ { collection: cn, key: "foobar" } ];
      actual = getQueryResults("RETURN PARSE_IDENTIFIER(DOCUMENT(CONCAT(@cn, '/', @key)))", { cn: cn, key: "foobar" });
      assertEqual(expected, actual);

      expected = [ { collection: cn, key: "foobar" } ];
      actual = getQueryResults("RETURN PARSE_IDENTIFIER(DOCUMENT(CONCAT(@cn, '/', 'foobar')))", { cn: cn });
      assertEqual(expected, actual);

      expected = [ { collection: cn, key: "foobar" } ];
      actual = getQueryResults("RETURN PARSE_IDENTIFIER(DOCUMENT([ @key ])[0])", { key: "UnitTestsAhuacatlFunctions/foobar" });
      assertEqual(expected, actual);

      expected = [ { collection: cn, key: "so-this-is-it" } ];
      actual = getQueryResults("RETURN PARSE_IDENTIFIER(DOCUMENT([ 'UnitTestsAhuacatlFunctions/so-this-is-it' ])[0])");
      assertEqual(expected, actual);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse identifier function
////////////////////////////////////////////////////////////////////////////////

    testParseIdentifierInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PARSE_IDENTIFIER()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PARSE_IDENTIFIER('foo', 'bar')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(\"foo\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER('foo bar')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER('foo/bar/baz')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER([ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER({ foo: 'bar' })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////
    
    testMin1 : function () {
      var expected = [ null, null ]; 
      var actual = getQueryResults("FOR u IN [ [ ], [ null, null ] ] return MIN(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////
    
    testMin2 : function () {
      var expected = [ 1, 1, 1, 1, 1, 1 ];
      var actual = getQueryResults("FOR u IN [ [ 1, 2, 3 ], [ 3, 2, 1 ], [ 1, 3, 2 ], [ 2, 3, 1 ], [ 2, 1, 3 ], [ 3, 1, 2 ] ] return MIN(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////
    
    testMin3 : function () {
      var expected = [ 2, false, false, false, false, true, -1, '', 1 ];
      var actual = getQueryResults("FOR u IN [ [ 3, 2, '1' ], [ [ ], null, true, false, 1, '0' ], [ '0', 1, false, true, null, [ ] ], [ false, true ], [ 0, false ], [ true, 0 ], [ '0', -1 ], [ '', '-1' ], [ [ ], 1 ] ] return MIN(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////

    testMinInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MIN()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MIN([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MIN(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MIN(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MIN(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MIN(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MIN({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////
    
    testMax1 : function () {
      var expected = [ null, null ]; 
      var actual = getQueryResults("FOR u IN [ [ ], [ null, null ] ] return MAX(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////
    
    testMax2 : function () {
      var expected = [ 3, 3, 3, 3, 3, 3 ];
      var actual = getQueryResults("FOR u IN [ [ 1, 2, 3 ], [ 3, 2, 1 ], [ 1, 3, 2 ], [ 2, 3, 1 ], [ 2, 1, 3 ], [ 3, 1, 2 ] ] return MAX(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////
    
    testMax3 : function () {
      var expected = [ '1', [ ], [ ], true, 0, 0, '0', '-1', [ ] ];
      var actual = getQueryResults("FOR u IN [ [ 3, 2, '1' ], [ [ ], null, true, false, 1, '0' ], [ '0', 1, false, true, null, [ ] ], [ false, true ], [ 0, false ], [ true, 0 ], [ '0', -1 ], [ '', '-1' ], [ [ ], 1 ] ] return MAX(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////

    testMaxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MAX()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MAX([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MAX(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MAX(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MAX(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MAX(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MAX({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sum function
////////////////////////////////////////////////////////////////////////////////
    
    testSum : function () {
      var data = [
        [ 0, [ ] ],
        [ 0, [ null ] ],
        [ 0, [ null, null ] ],
        [ 1, [ 1, null, null ] ],
        [ 2, [ 1, null, null, 1 ] ],
        [ 15, [ 1, 2, 3, 4, 5 ] ],
        [ 15, [ 5, 4, 3, 2, 1 ] ],
        [ 15, [ null, 5, 4, null, 3, 2, 1, null ] ],
        [ 0, [ -1, 1, -1, 1, -1, 1, 0 ] ],
        [ -4, [ -1, -1, -1, -1 ] ],
        [ 1.31, [ 0.1, 0.1, 0.01, 1.1 ] ],
        [ -1.31, [ -0.1, -0.1, -0.01, -1.1 ] ],
        [ 9040346.290954, [ 45.356, 256.23, -223.6767, -14512.63, 456.00222, -0.090566, 9054325.1 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN SUM(" + JSON.stringify(value[1]) + ")");
        assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sum function
////////////////////////////////////////////////////////////////////////////////

    testSumInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUM()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUM([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN SUM(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN SUM(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN SUM(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN SUM(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN SUM({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test average function
////////////////////////////////////////////////////////////////////////////////
    
    testAverage : function () {
      var data = [
        [ null, [ ] ],
        [ null, [ null ] ],
        [ null, [ null, null ] ],
        [ 1, [ 1, null, null ] ],
        [ 1, [ 1, null, null, 1 ] ],
        [ 2.5, [ 0, 1, 2, 3, 4, 5 ] ],
        [ 3, [ 1, 2, 3, 4, 5 ] ],
        [ 3, [ 5, 4, 3, 2, 1 ] ],
        [ 3, [ 5, 4, null, null, 3, 2, 1, null ] ],
        [ 0, [ -1, 1, -1, 1, -1, 1, 0 ] ],
        [ -1, [ -1, -1, -1, -1 ] ],
        [ 0.3275, [ 0.1, 0.1, 0.01, 1.1 ] ],
        [ -0.3275, [ -0.1, -0.1, -0.01, -1.1 ] ],
        [ 1291478.0415649, [ 45.356, 256.23, -223.6767, -14512.63, 456.00222, -0.090566, 9054325.1 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN AVERAGE(" + JSON.stringify(value[1]) + ")");
        if (actual[0] === null) {
          assertNull(value[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test average function
////////////////////////////////////////////////////////////////////////////////

    testAverageInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN AVERAGE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN AVERAGE([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN AVERAGE(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN AVERAGE(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN AVERAGE(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN AVERAGE(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN AVERAGE({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test median function
////////////////////////////////////////////////////////////////////////////////
    
    testMedian : function () {
      var data = [
        [ null, [ ] ],
        [ null, [ null ] ],
        [ null, [ null, null ] ],
        [ 1, [ 1, null, null ] ],
        [ 1, [ 1, null, null, 1 ] ],
        [ 1.5, [ 1, null, null, 2 ] ],
        [ 2, [ 1, null, null, 2, 3 ] ],
        [ 2.5, [ 1, null, null, 2, 3, 4 ] ],
        [ 2.5, [ 0, 1, 2, 3, 4, 5 ] ],
        [ 0, [ 0, 0, 0, 0, 0, 0, 1 ] ],
        [ 5, [ 1, 10, 2, 9, 3, 8, 5 ] ],
        [ 5.5, [ 1, 10, 2, 9, 3, 8, 6, 5 ] ],
        [ 3, [ 1, 2, 3, 4, 5 ] ],
        [ 3, [ 5, 4, 3, 2, 1 ] ],
        [ 3.5, [ 5, 4, 4, 3, 2, 1 ] ],
        [ 3, [ 5, 4, null, null, 3, 2, 1, null ] ],
        [ 0, [ -1, 1, -1, 1, -1, 1, 0 ] ],
        [ 0.5, [ -1, 1, -1, 1, -1, 1, 1, 0 ] ],
        [ -1, [ -1, -1, -1, -1 ] ],
        [ 0.1, [ 0.1, 0.1, 0.01, 1.1 ] ],
        [ -0.1, [ -0.1, -0.1, -0.01, -1.1 ] ],
        [ 45.356, [ 45.356, 256.23, -223.6767, -14512.63, 456.00222, -0.090566, 9054325.1 ] ],
        [ 2.5, [ 1, 2, 3, 100000000000 ] ],
        [ 3, [ 1, 2, 3, 4, 100000000000 ] ],
        [ 0.005, [ -100000, 0, 0.01, 0.2 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN MEDIAN(" + JSON.stringify(value[1]) + ")");
        if (actual[0] === null) {
          assertNull(value[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test median function
////////////////////////////////////////////////////////////////////////////////

    testMedianInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MEDIAN()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MEDIAN([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MEDIAN(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MEDIAN(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MEDIAN(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MEDIAN(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN MEDIAN({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test percentile function
////////////////////////////////////////////////////////////////////////////////
 
    testPercentile : function () {
      var data = [
        [ null, [ 1, 2, 3, 4, 5 ], 0, "rank" ],
        [ null, [ 15, 20, 35, 40, 50 ], 0, "rank" ],
        [ 50, [ 15, 20, 35, 40, 50 ], 100, "rank" ],
        [ 20, [ 15, 20, 35, 40, 50 ], 30, "rank" ],
        [ 20, [ 15, 20, 35, 40, 50 ], 40, "rank" ],
        [ 35, [ 15, 20, 35, 40, 50 ], 50, "rank" ],
        [ 50, [ 15, 20, 35, 40, 50 ], 100, "rank" ],
        [ 15, [ 3, 5, 7, 8, 9, 11, 13, 15 ], 100, "rank" ],
        [ null, [ 3, 5, 7, 8, 9, 11, 13, 15 ], 0, "interpolation" ],
        [ 5.5, [ 3, 5, 7, 8, 9, 11, 13, 15 ], 25, "interpolation" ],
        [ 5, [ 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 9, 9, 9, 10, 10, 10 ], 25, "interpolation" ],
        [ 9.85, [ 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 9, 9, 9, 10, 10, 10 ], 85, "interpolation" ],
        [ 4, [ 2, 3, 5, 9 ], 50, "interpolation" ],
        [ 5, [ 2, 3, 5, 9, 11 ], 50, "interpolation" ],
        [ 11, [ 2, 3, 5, 9, 11 ], 100, "interpolation" ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN PERCENTILE(" + JSON.stringify(value[1]) + ", " + JSON.stringify(value[2]) + ", " + JSON.stringify(value[3]) + ")");
        if (actual[0] === null) {
          assertNull(value[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////
    
    testHas1 : function () {
      var expected = [ true, true, true, false, true ]; 
      var actual = getQueryResults("FOR u IN [ { \"the fox\" : true }, { \"the fox\" : false }, { \"the fox\" : null }, { \"the FOX\" : true }, { \"the FOX\" : true, \"the fox\" : false } ] return HAS(u, \"the fox\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////
    
    testHas2 : function () {
      var expected = [ false, false, false ];
      var actual = getQueryResults("FOR u IN [ { \"the foxx\" : { \"the fox\" : false } }, { \" the fox\" : false }, null ] return HAS(u, \"the fox\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////

    testHasInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN HAS()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN HAS({ })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN HAS({ }, \"fox\", true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS(false, \"fox\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS(3, \"fox\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS(\"yes\", \"fox\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS([ ], \"fox\")"); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, null)")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, false)")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, true)")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, 1)")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, [ ])")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, { })")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributes1 : function () {
      var expected = [ [ "foo", "bar", "meow", "_id" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN ATTRIBUTES(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributes2 : function () {
      var expected = [ [ "foo", "bar", "meow" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN ATTRIBUTES(u, true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributes3 : function () {
      var expected = [ [ "_id", "bar", "foo", "meow" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN ATTRIBUTES(u, false, true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributes4 : function () {
      var expected = [ [ "bar", "foo", "meow" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN ATTRIBUTES(u, true, true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test values function
////////////////////////////////////////////////////////////////////////////////
    
    testValues1 : function () {
      var expected = [ [ "bar", "baz", true, "123/456" ], [ "bar" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN VALUES(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test values function
////////////////////////////////////////////////////////////////////////////////
    
    testValues2 : function () {
      var expected = [ [ "bar", "baz", true ], [ "bar" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN VALUES(u, true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test values function
////////////////////////////////////////////////////////////////////////////////
    
    testValues3 : function () {
      var expected = [ [ "test/1123", "test/abc", "1234", "test", "", [ 1, 2, 3, 4, [ true ] ], null, { d: 42, e: null, f: [ "test!" ] } ] ];
      var actual = getQueryResults("RETURN VALUES({ _from: \"test/1123\", _to: \"test/abc\", _rev: \"1234\", _key: \"test\", void: \"\", a: [ 1, 2, 3, 4, [ true ] ], b : null, c: { d: 42, e: null, f: [ \"test!\" ] } })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test zip function
////////////////////////////////////////////////////////////////////////////////
    
    testZip : function () {
      var values = [
        [ { }, [ ], [ ] ], 
        [ { "" : true }, [ "" ], [ true ] ], 
        [ { " " : null }, [ " " ], [ null ] ], 
        [ { "MÖTÖR" : { } }, [ "MÖTÖR" ], [ { } ] ], 
        [ { "just-a-single-attribute!" : " with a senseless value " }, [ "just-a-single-attribute!" ], [ " with a senseless value " ] ], 
        [ { a: 1, b: 2, c: 3 }, [ "a", "b", "c" ], [ 1, 2, 3 ] ], 
        [ { foo: "baz", bar: "foo", baz: "bar" }, [ "foo", "bar", "baz" ], [ "baz", "foo", "bar" ] ],
        [ { a: null, b: false, c: true, d: 42, e: 2.5, f: "test", g: [], h: [ "test1", "test2" ], i : { something: "else", more: "less" } }, [ "a", "b", "c", "d", "e", "f", "g", "h", "i" ], [ null, false, true, 42, 2.5, "test", [ ], [ "test1", "test2" ], { something: "else", more: "less" } ] ],
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN ZIP(" + JSON.stringify(value[1]) + ", " + JSON.stringify(value[2]) + ")");
        assertEqual(value[0], actual[0], value);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test zip function
////////////////////////////////////////////////////////////////////////////////

    testZipInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN ZIP()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN ZIP([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN ZIP([ ], [ ], [ ])");
      
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP([ ], null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP([ ], false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP([ ], true)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP([ ], 0)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP([ ], 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP([ ], \"\")");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP([ ], { })");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP(null, [ ])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP(false, [ ])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP(true, [ ])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP(0, [ ])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP(1, [ ])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP(\"\", [ ])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP({ }, [ ])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP([ 1 ], [ ])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP([ 1 ], [ 1, 2 ])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ZIP([ ], [ 1, 2 ])");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test matches
////////////////////////////////////////////////////////////////////////////////
    
    testMatches : function () {
      var tests = [
        {
          doc: { },
          examples: [ { } ],
          flag: true,
          expected: [ 0 ]
        },
        {
          doc: { },
          examples: [ { } ],
          flag: false,
          expected: [ true ]
        },
        {
          doc: { },
          examples: [ { } ],
          flag: null,
          expected: [ true ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 1 } ],
          flag: true,
          expected: [ -1 ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 1 } ],
          flag: false,
          expected: [ false ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 2 } ],
          flag: true,
          expected: [ 0 ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 2 } ],
          flag: false,
          expected: [ true ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 3 }, { test1: 1, test2: 2 } ],
          flag: true,
          expected: [ 1 ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 3 }, { test1: 1, test2: 2 } ],
          flag: false,
          expected: [ true ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 3 }, { test1: 1, test2: 2 } ],
          flag: null,
          expected: [ true ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { } ],
          flag: true,
          expected: [ 0 ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { } ],
          flag: false,
          expected: [ true ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { fox: true }, { fox: false}, { test1: "something" }, { test99: 1, test2: 2 }, { test1: "1", test2: "2" } ],
          flag: true,
          expected: [ -1 ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { fox: true }, { fox: false}, { test1: "something" }, { test99: 1, test2: 2 }, { test1: "1", test2: "2" }, { } ],
          flag: true,
          expected: [ 5 ]
        }
      ];

      tests.forEach(function (data) {
        var query = "RETURN MATCHES(" + JSON.stringify(data.doc) + ", " + JSON.stringify(data.examples);
        if (data.flag !== null) {
          query += ", " + JSON.stringify(data.flag) + ")";
        }
        else {
          query += ")";
        }
        var actual = getQueryResults(query);
        assertEqual(data.expected, actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance function
////////////////////////////////////////////////////////////////////////////////

    testVariancePopulation : function () {
      var data = [
        [ null, [ null ] ],
        [ null, [ null, null, null ] ],
        [ null, [ ] ],
        [ 0, [ 0 ] ],
        [ 0, [ null, 0 ] ],
        [ 0, [ 0.00001 ] ],
        [ 0, [ 1 ] ],
        [ 0, [ 100 ] ],
        [ 0, [ -1 ] ],
        [ 0, [ -10000000 ] ],
        [ 0, [ -100 ] ],
        [ 0, [ null, null, null, -100 ] ],
        [ 2, [ 1, 2, 3, 4, 5 ] ],
        [ 2, [ null, 1, null, 2, 3, null, null, 4, null, 5 ] ],
        [ 0.72727272727273, [ 1, 3, 1, 3, 2, 2, 2, 1, 3, 1, 3 ] ],
        [ 0.8, [ 1, 3, 1, 3, 2, 2, 1, 3, 1, 3 ] ],
        [ 0.88888888888889, [ 1, 3, 1, 3, 2, 1, 3, 1, 3 ] ],
        [ 0.66666666666667, [ 1, 1, 1, 2, 2, 2, 3, 3, 3] ],
        [ 141789.04, [ 12,96, 13, 143, 999 ] ],
        [ 141789.04, [ 12,96, 13, 143, null, 999 ] ],
        [ 491.64405555556, [ 18, -4, 6, 35.2, 63.66, 12.4 ] ],
        [ 1998, [ 1, 10, 100 ] ],
        [ 199800, [ 10, 100, 1000 ] ],
        [ 17538018.75, [ 10, 100, 1000, 10000 ] ],
        [ 15991127264736, [ 10, 100, 1000, 10000, 10000000 ] ],
        [ 17538018.75, [ -10, -100, -1000, -10000 ] ],
        [ 1998, [ -1, -10, -100 ] ],
        [ 6.6666666666667E-7, [ 0.001, 0.002, 0.003 ] ],
        [ 49.753697, [ -0.1, 2.4, -0.004, 12.054, 12.53, -7.35 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN VARIANCE_POPULATION(" + JSON.stringify(value[1]) + ")");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance function
////////////////////////////////////////////////////////////////////////////////

    testVariancePopulationInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN VARIANCE_POPULATION()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN VARIANCE_POPULATION([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN VARIANCE_POPULATION(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN VARIANCE_POPULATION(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN VARIANCE_POPULATION(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN VARIANCE_POPULATION(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN VARIANCE_POPULATION({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance function
////////////////////////////////////////////////////////////////////////////////

    testVarianceSample : function () {
      var data = [
        [ null, [ ] ],
        [ null, [ null ] ],
        [ null, [ null, null ] ],
        [ null, [ 1 ] ],
        [ null, [ 1, null ] ],
        [ 2.5, [ 1, 2, 3, 4, 5 ] ],
        [ 2.5, [ null, null, 1, 2, 3, 4, 5 ] ],
        [ 0.8, [ 1, 3, 1, 3, 2, 2, 2, 1, 3, 1, 3 ] ],   
        [ 0.88888888888889, [ 1, 3, 1, 3, 2, 2, 1, 3, 1, 3 ] ],
        [ 1, [ 1, 3, 1, 3, 2, 1, 3, 1, 3 ] ],
        [ 1, [ 1, 3, 1, 3, 2, 1, 3, 1, 3, null ] ],
        [ 0.75, [ 1, 1, 1, 2, 2, 2, 3, 3, 3 ] ],
        [ 0.75, [ null, 1, null, 1, null, 1, null, null, 2, null, 2, null, 2, null, 3, 3, 3 ] ],
        [ 177236.3, [ 12, 96, 13, 143, 999 ] ], 
        [ 589.97286666667, [ 18, -4, 6, 35.2, 63.66, 12.4 ] ],  
        [ 2997, [ 1, 10, 100 ] ],
        [ 2997, [ 1, 10, 100, null ] ],
        [ 299700, [ 10, 100, 1000 ] ],
        [ 23384025, [ 10, 100, 1000, 10000 ] ],
        [ 19988909080920, [ 10, 100, 1000, 10000, 10000000 ] ],
        [ 23384025, [ -10, -100, -1000, -10000 ] ],  
        [ 2997, [ -1, -10, -100 ] ], 
        [ 1.0E-6, [ 0.001, 0.002, 0.003 ] ],
        [ 59.7044364, [ -0.1, 2.4, -0.004, 12.054, 12.53, -7.35 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN VARIANCE_SAMPLE(" + JSON.stringify(value[1]) + ")");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance function
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN VARIANCE_SAMPLE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN VARIANCE_SAMPLE([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN VARIANCE_SAMPLE(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN VARIANCE_SAMPLE(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN VARIANCE_SAMPLE(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN VARIANCE_SAMPLE(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN VARIANCE_SAMPLE({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev function
////////////////////////////////////////////////////////////////////////////////

    testStddevPopulation : function () {
      var data = [
        [ null, [ null ] ],
        [ null, [ null, null, null ] ],
        [ null, [ ] ],
        [ 0, [ 0 ] ],
        [ 0, [ null, 0 ] ],
        [ 0, [ 0.00001 ] ],
        [ 0, [ 1 ] ],
        [ 0, [ 100 ] ],
        [ 0, [ -1 ] ],
        [ 0, [ -10000000 ] ],
        [ 0, [ -100 ] ],
        [ 0, [ null, null, null, -100 ] ],
        [ 1.4142135623731, [ 1, 2, 3, 4, 5 ] ],
        [ 1.4142135623731, [ null, 1, null, 2, 3, null, null, 4, null, 5 ] ],
        [ 0.85280286542244, [ 1, 3, 1, 3, 2, 2, 2, 1, 3, 1, 3 ] ],
        [ 0.89442719099992, [ 1, 3, 1, 3, 2, 2, 1, 3, 1, 3 ] ],
        [ 0.94280904158206, [ 1, 3, 1, 3, 2, 1, 3, 1, 3 ] ],
        [ 0.81649658092773, [ 1, 1, 1, 2, 2, 2, 3, 3, 3] ],
        [ 376.54885473203, [ 12,96, 13, 143, 999 ] ],
        [ 376.54885473203, [ 12,96, 13, 143, null, 999 ] ],
        [ 22.173047953666, [ 18, -4, 6, 35.2, 63.66, 12.4 ] ],
        [ 44.698993277254, [ 1, 10, 100 ] ],
        [ 446.98993277254, [ 10, 100, 1000 ] ],
        [ 4187.8417770971, [ 10, 100, 1000, 10000 ] ],
        [ 3998890.7542887, [ 10, 100, 1000, 10000, 10000000 ] ],
        [ 4187.8417770971, [ -10, -100, -1000, -10000 ] ],
        [ 44.698993277254, [ -1, -10, -100 ] ],
        [ 0.00081649658092773, [ 0.001, 0.002, 0.003 ] ],
        [ 7.0536300583458, [ -0.1, 2.4, -0.004, 12.054, 12.53, -7.35 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN STDDEV_POPULATION(" + JSON.stringify(value[1]) + ")");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev function
////////////////////////////////////////////////////////////////////////////////

    testStddevPopulationInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN STDDEV_POPULATION()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN STDDEV_POPULATION([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN STDDEV_POPULATION(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN STDDEV_POPULATION(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN STDDEV_POPULATION(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN STDDEV_POPULATION(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_LIST_EXPECTED.code, "RETURN STDDEV_POPULATION({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-existing functions
////////////////////////////////////////////////////////////////////////////////

    testNonExisting : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN FOO()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN BAR()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN PENG(true)"); 
    }
  
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlFunctionsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
