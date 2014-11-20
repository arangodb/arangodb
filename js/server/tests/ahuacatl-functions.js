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
var assertQueryError = helper.assertQueryError;

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
/// @brief test like function, invalid arguments
////////////////////////////////////////////////////////////////////////////////
    
    testLikeInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LIKE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LIKE(\"test\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LIKE(\"test\", \"meow\", \"foo\", \"bar\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONTAINS(\"test\", 1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONTAINS(1, \"test\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONTAINS(\"test\", \"test\", 1)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test like function
////////////////////////////////////////////////////////////////////////////////
    
    testLike : function () {
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"this is a test string\", \"test\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"this is a test string\", \"%test\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"this is a test string\", \"test%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"this is a test string\", \"%test%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"this is a test string\", \"this%test%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"this is a test string\", \"this%is%test%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"this is a test string\", \"this%g\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"this is a test string\", \"this%n\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"this is a test string\", \"This%n\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"this is a test string\", \"his%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"this is a test string\", \"%g\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"this is a test string\", \"%G\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"this is a test string\", \"this%test%is%\")"));
    
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"%\", \"\\%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"a%c\", \"a%c\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"a%c\", \"ac\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"a%c\", \"a\\\\%\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"a%c\", \"\\\\%a%\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"a%c\", \"\\\\%\\\\%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"%%\", \"\\\\%\\\\%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"_\", \"\\\\_\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"_\", \"\\\\_%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"abcd\", \"_bcd\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"abcde\", \"_bcd%\")"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"abcde\", \"\\\\_bcd%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"\\\\abc\", \"\\\\\\\\%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"\\abc\", \"\\a%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"[ ] ( ) % * . + -\", \"[%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"[ ] ( ) % * . + -\", \"[ ] ( ) \\% * . + -\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"[ ] ( ) % * . + -\", \"%. +%\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"abc^def$g\", \"abc^def$g\")"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"abc^def$g\", \"%^%$g\")"));
      
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"ABCD\", \"abcd\", false)"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"ABCD\", \"abcd\", true)"));
      assertEqual([ false ], getQueryResults("RETURN LIKE(\"abcd\", \"ABCD\", false)"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"abcd\", \"ABCD\", true)"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"MÖterTräNenMÜtterSöhne\", \"MöterTräNenMütterSöhne\", true)"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"MÖterTräNenMÜtterSöhne\", \"mötertränenmüttersöhne\", true)"));
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"MÖterTräNenMÜtterSöhne\", \"MÖTERTRÄNENMÜTTERSÖHNE\", true)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test like with special characters
////////////////////////////////////////////////////////////////////////////////

    testLikeSpecialChars : function () {
      var data = [ 
        "the quick\nbrown fox jumped over\r\nthe lazy dog",
        "'the \"\\quick\\\n \"brown\\\rfox' jumped",
        '"the fox"" jumped \\over the \newline \roof"'
      ];
        
      data.forEach(function(value) {
        var actual = getQueryResults("RETURN LIKE(" + JSON.stringify(value) + ", 'foobar')");
        assertEqual([ false ], actual);
        
        actual = getQueryResults("RETURN LIKE(" + JSON.stringify(value) + ", " + JSON.stringify(value) + ")");
        assertEqual([ true ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first require function / expected datatype & arg. mismatch
////////////////////////////////////////////////////////////////////////////////
    
    testContainsFirst : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONTAINS(\"test\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONTAINS(\"test\", \"test\", \"test\", \"test\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONTAINS()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONTAINS(\"test\", \"test2\", \"test3\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONTAINS(null, null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONTAINS(4, 4)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONTAINS({ }, { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONTAINS([ ], [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONTAINS(null, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONTAINS(3, null)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear contains true1
////////////////////////////////////////////////////////////////////////////////

    testContainsTrue1 : function () {
      var expected = [true];  
      var actual = getQueryResults("RETURN CONTAINS(\"test2\", \"test\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains true2
////////////////////////////////////////////////////////////////////////////////

    testContainsTrue2 : function () {
      var expected = [true];  
      var actual = getQueryResults("RETURN CONTAINS(\"xxasdxx\", \"asd\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains false1
////////////////////////////////////////////////////////////////////////////////

    testContainsFalse1 : function () {
      var expected = [false];  
      var actual = getQueryResults("RETURN CONTAINS(\"test\", \"test2\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains false2
////////////////////////////////////////////////////////////////////////////////

    testContainsFalse2 : function () {
      var expected = [false];  
      var actual = getQueryResults("RETURN CONTAINS(\"test123\", \"\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contanis false3
////////////////////////////////////////////////////////////////////////////////

    testContainsFalse3 : function () {
      var expected = [false];  
      var actual = getQueryResults("RETURN CONTAINS(\"\", \"test123\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear contains indexed
////////////////////////////////////////////////////////////////////////////////

    testContainsIndexed : function () {
      assertEqual([ 0 ], getQueryResults("RETURN CONTAINS(\"test2\", \"test\", true)"));
      assertEqual([ true ], getQueryResults("RETURN CONTAINS(\"test2\", \"test\", false)"));
      assertEqual([ 1 ], getQueryResults("RETURN CONTAINS(\"test2\", \"est\", true)"));
      assertEqual([ true ], getQueryResults("RETURN CONTAINS(\"test2\", \"est\", false)"));
      assertEqual([ -1 ], getQueryResults("RETURN CONTAINS(\"this is a long test\", \"this is a test\", true)"));
      assertEqual([ false ], getQueryResults("RETURN CONTAINS(\"this is a long test\", \"this is a test\", false)"));
      assertEqual([ 18 ], getQueryResults("RETURN CONTAINS(\"this is a test of this test\", \"this test\", true)"));
      assertEqual([ true ], getQueryResults("RETURN CONTAINS(\"this is a test of this test\", \"this test\", false)"));
      assertEqual([ -1 ], getQueryResults("RETURN CONTAINS(\"this is a test of this test\", \"This\", true)"));
      assertEqual([ false ], getQueryResults("RETURN CONTAINS(\"this is a test of this test\", \"This\", false)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains with special characters
////////////////////////////////////////////////////////////////////////////////

    testContainsSpecialChars : function () {
      var data = [ 
        "the quick\nbrown fox jumped over\r\nthe lazy dog",
        "'the \"\\quick\\\n \"brown\\\rfox' jumped",
        '"the fox"" jumped \\over the \newline \roof"'
      ];
        
      data.forEach(function(value) {
        var actual = getQueryResults("RETURN CONTAINS(" + JSON.stringify(value) + ", 'foobar', false)");
        assertEqual([ false ], actual);
        
        actual = getQueryResults("RETURN CONTAINS(" + JSON.stringify(value) + ", " + JSON.stringify(value) + ", false)");
        assertEqual([ true ], actual);
      });
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FIRST(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FIRST(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FIRST(4)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FIRST(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FIRST({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testlast1 : function () {
      var expected = [ [ "the dog" ] ];
      var actual = getQueryResults("RETURN LAST([ { \"the fox\" : \"jumped\" }, \"over\", [ \"the dog\" ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testlast2 : function () {
      var expected = [ "over" ];
      var actual = getQueryResults("RETURN LAST([ { \"the fox\" : \"jumped\" }, \"over\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testlast3 : function () {
      var expected = [ { "the fox" : "jumped" } ];
      var actual = getQueryResults("RETURN LAST([ { \"the fox\" : \"jumped\" } ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testlast4 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN LAST([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testlastInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LAST()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LAST([ ], [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LAST(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LAST(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LAST(4)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LAST(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LAST({ })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN POSITION(null, 'foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN POSITION(true, 'foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN POSITION(4, 'foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN POSITION(\"yes\", 'foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN POSITION({ }, 'foo')"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH(null, 1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH(true, 1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH(4, 1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH(\"yes\", 1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH({ }, 1)"); 
      
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH([ ], null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH([ ], false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH([ ], true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH([ ], '')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH([ ], '1234')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH([ ], [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH([ ], { \"foo\": true})"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NTH([ ], { })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN REVERSE(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN REVERSE(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN REVERSE(4)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN REVERSE({ })"); 
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
      var expected = [ [ { "the fox" : "jumped" }, { "the fox" : "jumped over" }, { "over" : "the dog", "the fox" : "jumped" }]]

      var actual = getQueryResults("RETURN UNIQUE([ { \"the fox\" : \"jumped\" }, { \"the fox\" : \"jumped over\" }, { \"the fox\" : \"jumped\", \"over\" : \"the dog\" }, { \"over\" : \"the dog\", \"the fox\" : \"jumped\" } ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUniqueInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNIQUE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNIQUE([ ], [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNIQUE(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNIQUE(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNIQUE(4)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNIQUE(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNIQUE({ })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SLICE([ ], { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SLICE([ ], true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SLICE([ ], 'foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SLICE([ ], [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SLICE([ ], { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SLICE([ ], 1, false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SLICE([ ], 1, 'foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SLICE([ ], 1, [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test left function
////////////////////////////////////////////////////////////////////////////////

    testLeft : function () {
      var expected = [ 'fo', 'f', '', 'foo', 'foo', '', '', '', 'mö', 'mötö' ];
      var actual = getQueryResults("FOR t IN [ [ 'foo', 2 ], [ 'foo', 1 ], [ 'foo', 0 ], [ 'foo', 4 ], [ 'foo', 999999999 ], [ '', 0 ], [ '', 1 ], [ '', 2 ], [ 'mötör', 2 ], [ 'mötör', 4 ] ] RETURN LEFT(t[0], t[1])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test left function
////////////////////////////////////////////////////////////////////////////////

    testLeftInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LEFT()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LEFT('foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LEFT('foo', 2, 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT(null, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT(true, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT(4, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT([ ], 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT({ }, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT('foo', null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT('foo', true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT('foo', 'bar')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT('foo', [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT('foo', { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT('foo', -1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LEFT('foo', -1.5)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test right function
////////////////////////////////////////////////////////////////////////////////

    testRight : function () {
      var expected = [ 'oo', 'o', '', 'foo', 'foo', '', '', '', 'ör', 'ötör' ];
      var actual = getQueryResults("FOR t IN [ [ 'foo', 2 ], [ 'foo', 1 ], [ 'foo', 0 ], [ 'foo', 4 ], [ 'foo', 999999999 ], [ '', 0 ], [ '', 1 ], [ '', 2 ], [ 'mötör', 2 ], [ 'mötör', 4 ] ] RETURN RIGHT(t[0], t[1])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test left function
////////////////////////////////////////////////////////////////////////////////

    testRightInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RIGHT()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RIGHT('foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RIGHT('foo', 2, 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT(null, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT(true, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT(4, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT([ ], 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT({ }, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT('foo', null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT('foo', true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT('foo', 'bar')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT('foo', [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT('foo', { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT('foo', -1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RIGHT('foo', -1.5)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test trim function
////////////////////////////////////////////////////////////////////////////////

    testTrim : function () {
      var expected = [ 'foo', 'foo  ', '  foo', '', '', '', 'abc', 'abc\n\r\t', '\t\r\nabc', 'a\rb\nc', 'a\rb\nc ', '\ta\rb\nc' ];
      var actual = getQueryResults("FOR t IN [ [ '  foo  ', 0 ], [ '  foo  ', 1 ], [ '  foo  ', 2 ], [ '', 0 ], [ '', 1 ], [ '', 2 ], [ '\t\r\nabc\n\r\t', 0 ], [ '\t\r\nabc\n\r\t', 1 ], [ '\t\r\nabc\t\r\n', 2 ], [ '\ta\rb\nc ', 0 ], [ '\ta\rb\nc ', 1 ], [ '\ta\rb\nc ', 2 ] ] RETURN TRIM(t[0], t[1])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test trim function
////////////////////////////////////////////////////////////////////////////////

    testTrimInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TRIM()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TRIM('foo', 2, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM(4)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM({ })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM('foo', null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM('foo', true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM('foo', 'bar')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM('foo', [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM('foo', { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM('foo', -1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM('foo', -1.5)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRIM('foo', 3)"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LENGTH(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LENGTH(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LENGTH(4)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////
    
    testConcat1 : function () {
      var expected = [ "theQuickBrownFoxJumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT('the', 'Quick', '', null, 'Brown', null, 'Fox', 'Jumps')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////
    
    testConcat2 : function () {
      var expected = [ "theQuickBrownアボカドJumps名称について" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT('the', 'Quick', '', null, 'Brown', null, 'アボカド', 'Jumps', '名称について')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////

    testConcatInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONCAT()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONCAT(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT(\"yes\", true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT(\"yes\", 4)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT(\"yes\", [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT(\"yes\", { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT(true, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT(4, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT([ ], \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT({ }, \"yes\")"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concatseparator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparator1 : function () {
      var expected = [ "the,Quick,Brown,Fox,Jumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT_SEPARATOR(',', 'the', 'Quick', null, 'Brown', null, 'Fox', 'Jumps')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concatseparator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparator2 : function () {
      var expected = [ "the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concatseparator function
////////////////////////////////////////////////////////////////////////////////

    testConcatSeparatorInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONCAT_SEPARATOR()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\", \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(null, \"yes\", \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(true, \"yes\", \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(4, \"yes\", \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR([ ], \"yes\", \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR({ }, \"yes\", \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\", true, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\", 4, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\", [ ], \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\", { }, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", 4)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", { })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test charlength function
////////////////////////////////////////////////////////////////////////////////
    
    testCharLength1 : function () {
      var expected = [ 13 ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CHAR_LENGTH('the quick fox')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test charlength function
////////////////////////////////////////////////////////////////////////////////
    
    testCharLength2 : function () {
      var expected = [ 7 ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CHAR_LENGTH('äöüÄÖÜß')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test charlength function
////////////////////////////////////////////////////////////////////////////////
    
    testCharLength3 : function () {
      var expected = [ 10 ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CHAR_LENGTH('アボカド名称について')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test charlength function
////////////////////////////////////////////////////////////////////////////////

    testCharLengthInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CHAR_LENGTH()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CHAR_LENGTH(\"yes\", \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CHAR_LENGTH(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CHAR_LENGTH(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CHAR_LENGTH(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CHAR_LENGTH([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CHAR_LENGTH({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower function
////////////////////////////////////////////////////////////////////////////////
    
    testLower1 : function () {
      var expected = [ "the quick brown fox jumped" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return LOWER('THE quick Brown foX JuMpED')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower function
////////////////////////////////////////////////////////////////////////////////
    
    testLower2 : function () {
      var expected = [ "äöüäöüß アボカド名称について" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return LOWER('äöüÄÖÜß アボカド名称について')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower function
////////////////////////////////////////////////////////////////////////////////
    
    testLower3 : function () {
      var expected = [ "0123456789<>|,;.:-_#'+*@!\"$&/(){[]}?\\" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return LOWER('0123456789<>|,;.:-_#\\'+*@!\\\"$&/(){[]}?\\\\')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower function
////////////////////////////////////////////////////////////////////////////////

    testLowerInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LOWER()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LOWER(\"yes\", \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LOWER(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LOWER(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LOWER(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LOWER([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN LOWER({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper function
////////////////////////////////////////////////////////////////////////////////
    
    testUpper1 : function () {
      var expected = [ "THE QUICK BROWN FOX JUMPED" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return UPPER('THE quick Brown foX JuMpED')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper function
////////////////////////////////////////////////////////////////////////////////
    
    testUpper2 : function () {
      var expected = [ "ÄÖÜÄÖÜSS アボカド名称について" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return UPPER('äöüÄÖÜß アボカド名称について')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper function
////////////////////////////////////////////////////////////////////////////////

    testUpperInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UPPER()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UPPER(\"yes\", \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UPPER(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UPPER(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UPPER(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UPPER([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UPPER({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test substring function
////////////////////////////////////////////////////////////////////////////////
    
    testSubstring1 : function () {
      var expected = [ "the" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return SUBSTRING('the quick brown fox', 0, 3)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test substring function
////////////////////////////////////////////////////////////////////////////////
    
    testSubstring2 : function () {
      var expected = [ "quick" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return SUBSTRING('the quick brown fox', 4, 5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test substring function
////////////////////////////////////////////////////////////////////////////////
    
    testSubstring3 : function () {
      var expected = [ "fox" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return SUBSTRING('the quick brown fox', -3)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test substring function
////////////////////////////////////////////////////////////////////////////////

    testSubstringInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUBSTRING()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUBSTRING(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUBSTRING(\"yes\", 0, 2, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(null, 0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(true, 0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(3, 0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING([ ], 0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING({ }, 0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(\"yes\", null, 0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(\"yes\", true, 0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(\"yes\", \"yes\", 0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(\"yes\", [ ], 0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(\"yes\", { }, 0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(\"yes\", \"yes\", null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(\"yes\", \"yes\", true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(\"yes\", \"yes\", [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUBSTRING(\"yes\", \"yes\", { })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test floor function
////////////////////////////////////////////////////////////////////////////////
    
    testFloor : function () {
      var expected = [ -100, -3, -3, -3, -2, -2, -2, -2, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 99 ];
      var actual = getQueryResults("FOR r IN [ -99.999, -3, -2.1, -2.01, -2, -1.99, -1.1, -1.01, -1, -0.9, -0.6, -0.5, -0.4, -0.1, -0.01, 0, 0.01, 0.1, 0.4, 0.5, 0.6, 0.9, 1, 1.01, 1.1, 1.99, 2, 2.01, 2.1, 3, 99.999 ] return FLOOR(r)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test floor function
////////////////////////////////////////////////////////////////////////////////

    testFloorInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FLOOR()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FLOOR(1, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLOOR(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLOOR(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLOOR(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLOOR([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLOOR({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ceil function
////////////////////////////////////////////////////////////////////////////////
    
    testCeil : function () {
      var expected = [ -99, -3, -2, -2, -2, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 100 ];
      var actual = getQueryResults("FOR r IN [ -99.999, -3, -2.1, -2.01, -2, -1.99, -1.1, -1.01, -1, -0.9, -0.6, -0.5, -0.4, -0.1, -0.01, 0, 0.01, 0.1, 0.4, 0.5, 0.6, 0.9, 1, 1.01, 1.1, 1.99, 2, 2.01, 2.1, 3, 99.999 ] return CEIL(r)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ceil function
////////////////////////////////////////////////////////////////////////////////

    testCeilInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CEIL()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CEIL(1,2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CEIL(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CEIL(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CEIL(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CEIL([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CEIL({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test abs function
////////////////////////////////////////////////////////////////////////////////
    
    testAbs : function () {
      var expected = [ 99.999, 3, 2.1, 2.01, 2, 1.99, 1.1, 1.01, 1, 0.9, 0.6, 0.5, 0.4, 0.1, 0.01, 0, 0.01, 0.1, 0.4, 0.5, 0.6, 0.9, 1, 1.01, 1.1, 1.99, 2, 2.01, 2.1, 3, 99.999 ];
      var actual = getQueryResults("FOR r IN [ -99.999, -3, -2.1, -2.01, -2, -1.99, -1.1, -1.01, -1, -0.9, -0.6, -0.5, -0.4, -0.1, -0.01, 0, 0.01, 0.1, 0.4, 0.5, 0.6, 0.9, 1, 1.01, 1.1, 1.99, 2, 2.01, 2.1, 3, 99.999 ] return ABS(r)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test abs function
////////////////////////////////////////////////////////////////////////////////

    testAbsInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN ABS()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN ABS(1,2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ABS(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ABS(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ABS(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ABS([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ABS({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test round function
////////////////////////////////////////////////////////////////////////////////
    
    testRound : function () {
      var expected = [ -100, -3, -2, -2, -2, -2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 100 ];
      var actual = getQueryResults("FOR r IN [ -99.999, -3, -2.1, -2.01, -2, -1.99, -1.1, -1.01, -1, -0.9, -0.6, -0.5, -0.4, -0.1, -0.01, 0, 0.01, 0.1, 0.4, 0.5, 0.6, 0.9, 1, 1.01, 1.1, 1.99, 2, 2.01, 2.1, 3, 99.999 ] return ROUND(r)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test round function
////////////////////////////////////////////////////////////////////////////////

    testRoundInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN ROUND()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN ROUND(1,2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ROUND(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ROUND(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ROUND(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ROUND([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN ROUND({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test rand function
////////////////////////////////////////////////////////////////////////////////
    
    testRand : function () {
      var actual = getQueryResults("FOR r IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 ] RETURN RAND()");
      for (var i in actual) {
        if (! actual.hasOwnProperty(i)) {
          continue;
        }
        var value = actual[i];
        assertTrue(value >= 0.0 && value < 1.0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test rand function
////////////////////////////////////////////////////////////////////////////////

    testRandInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RAND(1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RAND(2)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sqrt function
////////////////////////////////////////////////////////////////////////////////
    
    testSqrt : function () {
      var data = [
        [ 0, 0 ],
        [ 0.1, 0.31622776601684 ],
        [ 0.01, 0.1 ],
        [ 0.001, 0.031622776601684 ],
        [ 0.002, 0.044721359549996 ],
        [ 0.0004, 0.02 ],
        [ 9.0E-5, 0.0094868329805051 ],
        [ 9.0E-6, 0.003 ],
        [ 0.1212121415, 0.34815534104764 ],
        [ 1, 1 ],
        [ 2, 1.4142135623731 ],
        [ 2.25, 1.5 ],
        [ 3, 1.7320508075689 ],
        [ 4, 2 ],
        [ 5, 2.2360679774998 ],
        [ 6, 2.4494897427832 ],
        [ 9, 3 ],
        [ 12, 3.4641016151378 ],
        [ 14, 3.7416573867739 ],
        [ 16, 4 ],
        [ 20, 4.4721359549996 ],
        [ 25, 5 ],
        [ 36, 6 ],
        [ 37, 6.0827625302982 ],
        [ 99, 9.9498743710662 ],
        [ 100, 10 ],
        [ 100000, 316.22776601684 ],
        [ 1000000, 1000 ],
        [ 10000000, 3162.2776601684 ],
        [ 1000000000, 31622.776601684 ],
        [ -0.1, null ],
        [ -0.01, null ],
        [ -1.0E-5, null ],
        [ -1, null ],
        [ -2, null ],
        [ -3, null ],
        [ -5, null ],
        [ -10, null ],
        [ -13, null ],
        [ -16, null ],
        [ -25, null ],
        [ -27, null ],
        [ -100, null ],
        [ -1000, null ],
        [ -10000, null ],
        [ -100000, null ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN SQRT(" + JSON.stringify(value[0]) + ")");
        if (value[1] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[1].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sqrt function
////////////////////////////////////////////////////////////////////////////////

    testSqrtInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SQRT()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SQRT(2, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SQRT(2, 2, 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SQRT(true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SQRT('foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SQRT([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SQRT({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test keep function
////////////////////////////////////////////////////////////////////////////////
    
    testKeepInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP({ }, 1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP({ }, { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP({ }, 'foo', { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP('foo', 'foo')"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN KEEP()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN KEEP({ })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET({ }, 1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET({ }, { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET({ }, 'foo', { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET('foo', 'foo')"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSET()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSET({ })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(null, { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(true, { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(3, { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(\"yes\", { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE([ ], { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, [ ])"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(null, { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(true, { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(3, { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(\"yes\", { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE([ ], { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, [ ])"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE({ }, 'foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', 1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', '')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', [])"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(null, [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(true, [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(3, [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(\"yes\", [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION({ }, [ ])"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], 3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], \"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], { })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(null, [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(true, [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(3, [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(\"yes\", [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT({ }, [ ])"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(null, 2)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(null, null)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(false, 2)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(true, 2)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(1, true)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(1, [ ])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(1, { })");
      
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(1, 1, 0)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(-1, -1, 0)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(1, -1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(-1, 1, -1)");
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLATTEN(1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLATTEN(null)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLATTEN(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLATTEN(1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLATTEN('foo')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLATTEN([ ], 'foo')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLATTEN([ ], [ ])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN FLATTEN({ })");
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

    testParseIdentifier : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PARSE_IDENTIFIER()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PARSE_IDENTIFIER('foo', 'bar')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(\"foo\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER('foo bar')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER('foo/bar/baz')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER([ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER({ })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER({ foo: 'bar' })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document function
////////////////////////////////////////////////////////////////////////////////
    
    testDocument1 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      var d1 = cx.save({ "title" : "123", "value" : 456 });
      var d2 = cx.save({ "title" : "nada", "value" : 123 });

      var expected, actual;

      // test with two parameters
      expected = [ { title: "123", value : 456 } ];
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + d1._id + "\")");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + d1._key + "\")");
      assertEqual(expected, actual);
      
      expected = [ { title: "nada", value : 123 } ];
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + d2._id + "\")");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + d2._key + "\")");
      assertEqual(expected, actual);
      
      // test with one parameter
      expected = [ { title: "nada", value : 123 } ];
      actual = getQueryResults("RETURN DOCUMENT(\"" + d2._id + "\")");
      assertEqual(expected, actual);
      
      // test with function result parameter
      actual = getQueryResults("RETURN DOCUMENT(CONCAT(\"foo\", \"bar\"))");
      assertEqual([ null ], actual);
      actual = getQueryResults("RETURN DOCUMENT(CONCAT(\"" + cn + "\", \"bart\"))");
      assertEqual([ null ], actual);

      cx.save({ _key: "foo", value: "bar" });
      expected = [ { value: "bar" } ];
      actual = getQueryResults("RETURN DOCUMENT(CONCAT(\"" + cn + "\", \"foo\"))");
      assertEqual([ null ], actual);
      actual = getQueryResults("RETURN DOCUMENT(CONCAT(@c, \"/\", @k))", { c: cn, k: "foo" });
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN DOCUMENT(CONCAT(\"" + cn + "\", \"/\", @k))", { k: "foo" });
      assertEqual(expected, actual);
      
      // test with bind parameter
      expected = [ { title: "nada", value : 123 } ];
      actual = getQueryResults("RETURN DOCUMENT(@id)", { id: d2._id });
      assertEqual(expected, actual);
      
      // test dynamic parameter
      expected = [ { title: "nada", value : 123 }, { title: "123", value: 456 }, { value: "bar" } ];
      actual = getQueryResults("FOR d IN @@cn SORT d.value RETURN DOCUMENT(d._id)", { "@cn" : cn });
      assertEqual(expected, actual);
      
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document function
////////////////////////////////////////////////////////////////////////////////
    
    testDocument2 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      var d1 = cx.save({ "title" : "123", "value" : 456, "zxy" : 1 });
      var d2 = cx.save({ "title" : "nada", "value" : 123, "zzzz" : false });

      var expected, actual;

      // test with two parameters
      expected = [ { title: "123", value : 456, zxy: 1 } ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : d1._id });
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : d1._key });
      assertEqual(expected, actual);
      
      expected = [ { title: "nada", value : 123, zzzz : false } ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : d2._id });
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : d2._key });
      assertEqual(expected, actual);
      
      // test with one parameter
      expected = [ { title: "123", value : 456, zxy: 1 } ];
      actual = getQueryResults("RETURN DOCUMENT(@id)", { "id" : d1._id });
      assertEqual(expected, actual);
      
      expected = [ { title: "nada", value : 123, zzzz : false } ];
      actual = getQueryResults("RETURN DOCUMENT(@id)", { "id" : d2._id });
      assertEqual(expected, actual);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document function
////////////////////////////////////////////////////////////////////////////////
    
    testDocumentMulti1 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      var d1 = cx.save({ "title" : "123", "value" : 456, "zxy" : 1 });
      var d2 = cx.save({ "title" : "nada", "value" : 123, "zzzz" : false });
      var d3 = cx.save({ "title" : "boom", "value" : 3321, "zzzz" : null });

      var expected, actual;

      // test with two parameters
      expected = [ [
        { title: "123", value : 456, zxy : 1 },
        { title: "nada", value : 123, zzzz : false },
        { title: "boom", value : 3321, zzzz : null }
      ] ];

      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : [ d1._id, d2._id, d3._id ] }, true);
      assertEqual(expected, actual);
     
      expected = [ [ { title: "nada", value : 123, zzzz : false } ] ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : [ d2._id ] }, true);
      assertEqual(expected, actual);
      
      // test with one parameter
      expected = [ [ { title: "nada", value : 123, zzzz : false } ] ];
      actual = getQueryResults("RETURN DOCUMENT(@id)", { "id" : [ d2._id ] }, true);
      assertEqual(expected, actual);


      cx.remove(d3);
      
      expected = [ [ { title: "nada", value : 123, zzzz : false } ] ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : [ d2._id, d3._id, "abc/def" ] }, true);
      assertEqual(expected, actual);
      
      expected = [ [ { title: "nada", value : 123, zzzz : false } ] ];
      actual = getQueryResults("RETURN DOCUMENT(@id)", { "id" : [ d2._id, d3._id, "abc/def" ] }, true);
      assertEqual(expected, actual);
      
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document function
////////////////////////////////////////////////////////////////////////////////
    
    testDocumentInvalid : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      internal.db._create(cn);

      var expected, actual;

      // test with non-existing document
      expected = [ null ];
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + cn + "/99999999999\")");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"thefoxdoesnotexist/99999999999\")");
      assertEqual(expected, actual);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document indexed access function
////////////////////////////////////////////////////////////////////////////////
    
    testDocumentIndexedAccess : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      var d1 = cx.save({ "title" : "123", "value" : 456 });

      var expected, actual;

      expected = [ "123" ];
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", [ \"" + d1._id + "\" ])[0].title");
      assertEqual(expected, actual);
      
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test skiplist function
////////////////////////////////////////////////////////////////////////////////
    
    testSkiplist1 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      cx.ensureSkiplist("created");

      var i;
      for (i = 0; i < 1000; ++i) {
        cx.save({ created: i });
      }
      
      expected = [ { created: 0 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 0 ]] }, 0, 1) RETURN x"); 
      assertEqual(expected, actual);
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 0 ], [ '<', 1 ]] }) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 1 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>', 0 ]] }, 0, 1) RETURN x"); 
      assertEqual(expected, actual);
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>', 0 ], [ '<=', 1 ]] }) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 0 }, { created: 1 }, { created: 2 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 0 ]] }, 0, 3) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 5 }, { created: 6 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 0 ]] }, 5, 2) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 5 }, { created: 6 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 5 ], [ '<=', 6 ]] }, 0, 5) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 5 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 5 ], [ '<=', 6 ]] }, 0, 1) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 2 }, { created: 3 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '<', 4 ]] }, 2, 10) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>', 0 ]] }, 10000, 10) RETURN x"); 
      assertEqual(expected, actual);
      
      assertQueryError(errors.ERROR_ARANGO_NO_INDEX.code, "RETURN SKIPLIST(" + cn + ", { a: [[ '==', 1 ]] })"); 
      
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test skiplist function
////////////////////////////////////////////////////////////////////////////////
    
    testSkiplist2 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      
      assertQueryError(errors.ERROR_ARANGO_NO_INDEX.code, "RETURN SKIPLIST(" + cn + ", { a: [[ '==', 1 ]], b: [[ '==', 0 ]] })"); 

      cx.ensureSkiplist("a", "b");

      var i;
      for (i = 0; i < 1000; ++i) {
        cx.save({ a: i, b: i + 1 });
      }
      
      expected = [ { a: 1, b: 2 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { a: [[ '==', 1 ]], b: [[ '>=', 2 ], [ '<=', 3 ]] }) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { a: 1, b: 2 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { a: [[ '==', 1 ]], b: [[ '==', 2 ]] }) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { a: [[ '==', 2 ]], b: [[ '==', 1 ]] }, 1, 1) RETURN x"); 
      assertEqual(expected, actual);
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { a: [[ '==', 99 ]], b: [[ '==', 17 ]] }, 1, 1) RETURN x"); 
      assertEqual(expected, actual);
      
      assertQueryError(errors.ERROR_ARANGO_NO_INDEX.code, "RETURN SKIPLIST(" + cn + ", { a: [[ '==', 1 ]] })"); 
      assertQueryError(errors.ERROR_ARANGO_NO_INDEX.code, "RETURN SKIPLIST(" + cn + ", { b: [[ '==', 1 ]] })"); 
      assertQueryError(errors.ERROR_ARANGO_NO_INDEX.code, "RETURN SKIPLIST(" + cn + ", { c: [[ '==', 1 ]] })"); 
      
      internal.db._drop(cn);
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MIN(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MIN(false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MIN(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MIN(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MIN({ })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MAX(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MAX(false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MAX(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MAX(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MAX({ })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUM(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUM(false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUM(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUM(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN SUM({ })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN AVERAGE(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN AVERAGE(false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN AVERAGE(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN AVERAGE(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN AVERAGE({ })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MEDIAN(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MEDIAN(false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MEDIAN(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MEDIAN(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MEDIAN({ })"); 
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
      assertEqual([ false ], getQueryResults("RETURN HAS(false, \"fox\")")); 
      assertEqual([ false ], getQueryResults("RETURN HAS(3, \"fox\")")); 
      assertEqual([ false ], getQueryResults("RETURN HAS(\"yes\", \"fox\")")); 
      assertEqual([ false ], getQueryResults("RETURN HAS([ ], \"fox\")")); 
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
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull1 : function () {
      var expected = [ 6 ];
      var actual = getQueryResults("RETURN NOT_NULL(null, 2 + 4)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull2 : function () {
      var expected = [ 6 ];
      var actual = getQueryResults("RETURN NOT_NULL(2 + 4, 2 + 5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull3 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN NOT_NULL(null, null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull4 : function () {
      var expected = [ 2 ];
      var actual = getQueryResults("RETURN NOT_NULL(2, null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull5 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN NOT_NULL(false, true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull6 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN NOT_NULL(null, null, null, null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull7 : function () {
      var expected = [ -6 ];
      var actual = getQueryResults("RETURN NOT_NULL(null, null, null, null, -6, -7)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull8 : function () {
      var expected = [ 12 ];
      var actual = getQueryResults("RETURN NOT_NULL(null, null, null, null, 12, null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull9 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN NOT_NULL(null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull10 : function () {
      var expected = [ 23 ];
      var actual = getQueryResults("RETURN NOT_NULL(23)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList1 : function () {
      var expected = [ [ 1, 2 ] ];
      var actual = getQueryResults("RETURN FIRST_LIST(null, [ 1, 2 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList2 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_LIST(null, \"not a list!\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList3 : function () {
      var expected = [ [ 1, 4 ] ];
      var actual = getQueryResults("RETURN FIRST_LIST([ 1, 4 ], [ 1, 5 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList4 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN FIRST_LIST([ ], [ 1, 5 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList5 : function () {
      var expected = [ [ false ] ];
      var actual = getQueryResults("RETURN FIRST_LIST([ false ], [ 1, 5 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList6 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_LIST(false, 7)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList7 : function () {
      var expected = [ [ 0 ] ];
      var actual = getQueryResults("RETURN FIRST_LIST(1, 2, 3, 4, [ 0 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList8 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_LIST(1, 2, 3, 4, { })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList9 : function () {
      var expected = [ [ 7 ] ];
      var actual = getQueryResults("RETURN FIRST_LIST([ 7 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList10 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_LIST(99)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument1 : function () {
      var expected = [ { a : 1 } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(null, { a : 1 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument2 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(null, \"not a doc!\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument3 : function () {
      var expected = [ { a : 1 } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT({ a : 1 }, { b : 2 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument4 : function () {
      var expected = [ { } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT({ }, { b : 2 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument5 : function () {
      var expected = [ { a : null, b : false } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT({ a : null, b : false }, { a : 1000, b : 1000 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument6 : function () {
      var expected = [ { czz : 7 } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(false, { czz : 7 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument7 : function () {
      var expected = [ { } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(1, 2, 3, 4, { })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument8 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(1, 2, 3, 4, [ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument9 : function () {
      var expected = [ { c : 4 } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT({ c : 4 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument10 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(false)");
      assertEqual(expected, actual);
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
      ]

      tests.forEach(function (data) {
        var query = "RETURN MATCHES(" + JSON.stringify(data.doc) + ", " + JSON.stringify(data.examples);
        if (data.flag != null) {
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
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool1 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN TO_BOOL(null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool2 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN TO_BOOL(false)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool3 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool4 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN TO_BOOL(0)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool5 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(0.1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool6 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(-1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool7 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(100)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool8 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN TO_BOOL(\"\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool9 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(\" \")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool10 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(\"0\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool11 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(\"false\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool12 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN TO_BOOL([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool13 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool14 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ false ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool15 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ 0 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool16 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ \"\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool17 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ \" \" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool18 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ \"0\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool19 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ \"false\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool20 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ { } ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool21 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN TO_BOOL({ })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool22 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL({ \"0\" : \"\" })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool23 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL({ \"false\" : null })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER(null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber2 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber3 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER([ 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber4 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER([ -1, 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber5 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER({ })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber6 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER({ \"2\" : \"3\" })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber7 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER(false)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber8 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN TO_NUMBER(true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber9 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"1\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber10 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"0\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber11 : function () {
      var expected = [ -435.3 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"-435.3\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber12 : function () {
      var expected = [ 3553.4 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"3553.4er6\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber13 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"-wert324\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber14 : function () {
      var expected = [ 143 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"  143\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber15 : function () {
      var expected = [ 0.4 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"  .4\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList1 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN TO_LIST(null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList2 : function () {
      var expected = [ [ false ] ];
      var actual = getQueryResults("RETURN TO_LIST(false)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList3 : function () {
      var expected = [ [ true ] ];
      var actual = getQueryResults("RETURN TO_LIST(true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList4 : function () {
      var expected = [ [ 35 ] ];
      var actual = getQueryResults("RETURN TO_LIST(35)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList5 : function () {
      var expected = [ [ -0.635 ] ];
      var actual = getQueryResults("RETURN TO_LIST(-0.635)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList6 : function () {
      var expected = [ [ "" ] ];
      var actual = getQueryResults("RETURN TO_LIST(\"\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList7 : function () {
      var expected = [ [ "value" ] ];
      var actual = getQueryResults("RETURN TO_LIST(\"value\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList8 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN TO_LIST({ })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList9 : function () {
      var expected = [ [ 0 ] ];
      var actual = getQueryResults("RETURN TO_LIST({ \"a\" : 0 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList10 : function () {
      var expected = [ [ null, -63, [ 1, 2 ], { "a" : "b" } ] ];
      var actual = getQueryResults("RETURN TO_LIST({ \"a\" : null, \"b\" : -63, \"c\" : [ 1, 2 ], \"d\": { \"a\" : \"b\" } })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList11 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN TO_LIST([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList12 : function () {
      var expected = [ [ 0, null, -1 ] ];
      var actual = getQueryResults("RETURN TO_LIST([ 0, null, -1 ])");
      assertEqual(expected, actual);
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN VARIANCE_POPULATION(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN VARIANCE_POPULATION(false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN VARIANCE_POPULATION(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN VARIANCE_POPULATION(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN VARIANCE_POPULATION({ })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN VARIANCE_SAMPLE(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN VARIANCE_SAMPLE(false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN VARIANCE_SAMPLE(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN VARIANCE_SAMPLE(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN VARIANCE_SAMPLE({ })"); 
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN STDDEV_POPULATION(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN STDDEV_POPULATION(false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN STDDEV_POPULATION(3)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN STDDEV_POPULATION(\"yes\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN STDDEV_POPULATION({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test current user function
////////////////////////////////////////////////////////////////////////////////

    testCurrentUser : function () {
      var actual = getQueryResults("RETURN CURRENT_USER()");
      // there is no current user in the non-request context
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test current database function
////////////////////////////////////////////////////////////////////////////////

    testCurrentDatabase : function () {
      var actual = getQueryResults("RETURN CURRENT_DATABASE()");
      assertEqual([ "_system" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sleep function
////////////////////////////////////////////////////////////////////////////////

    testSleep : function () {
      var start = require("internal").time();
      var actual = getQueryResults("LET a = SLEEP(2) RETURN 1");

      var diff = Math.round(require("internal").time() - start, 1);

      assertEqual([ 1 ], actual);

      // allow some tolerance for the time diff
      assertTrue(diff >= 1.5 && diff <= 2.5);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_now function
////////////////////////////////////////////////////////////////////////////////

    testDateNowInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_NOW(1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_NOW(1, 1)");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_now function
////////////////////////////////////////////////////////////////////////////////

    testDateNow : function () {
      var time = require("internal").time() * 1000;
      var actual = getQueryResults("RETURN DATE_NOW()")[0];

      // allow for some small tolerance
      assertTrue(Math.abs(time - actual) <= 1500);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_dayofweek function
////////////////////////////////////////////////////////////////////////////////

    testDateDayOfWeekInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYOFWEEK()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYOFWEEK(1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK([])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_dayofweek function
////////////////////////////////////////////////////////////////////////////////

    testDateDayOfWeek : function () {
      var values = [
        [ "2000-04-29", 6 ],
        [ "2000-04-29Z", 6 ],
        [ "2012-02-12 13:24:12", 0 ],
        [ "2012-02-12 13:24:12Z", 0 ],
        [ "2012-02-12 23:59:59.991", 0 ],
        [ "2012-02-12 23:59:59.991Z", 0 ],
        [ "2012-02-12", 0 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-02-12T13:24:12Z", 0 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-2-12Z", 0 ],
        [ "1910-01-02T03:04:05Z", 0 ],
        [ "1910-01-02 03:04:05Z", 0 ],
        [ "1910-01-02", 0 ],
        [ "1910-01-02Z", 0 ],
        [ "1970-01-01T01:05:27", 4 ],
        [ "1970-01-01T01:05:27Z", 4 ],
        [ "1970-01-01 01:05:27Z", 4 ],
        [ "1970-1-1Z", 4 ],
        [ "1221-02-28T23:59:59Z", 0 ],
        [ "1221-02-28 23:59:59Z", 0 ],
        [ "1221-02-28Z", 0 ],
        [ "1221-2-28Z", 0 ],
        [ "1000-12-24T04:12:00Z", 3 ],
        [ "1000-12-24Z", 3 ],
        [ "1000-12-24 04:12:00Z", 3 ],
        [ "6789-12-31T23:59:58.99Z", 0 ],
        [ "6789-12-31Z", 0 ],
        [ "9999-12-31T23:59:59.999Z", 5 ],
        [ "9999-12-31Z", 5 ],
        [ "9999-12-31z", 5 ],
        [ "9999-12-31", 5 ],
        [ "2012Z", 0 ],
        [ "2012z", 0 ],
        [ "2012", 0 ],
        [ "2012-1Z", 0 ],
        [ "2012-1z", 0 ],
        [ "2012-1-1z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "  2012-01-01Z", 0 ],
        [ "  2012-01-01z", 0 ],
        [ 1399395674000, 2 ],
        [ 60123, 4 ],
        [ 1, 4 ],
        [ 0, 4 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_DAYOFWEEK(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_year function
////////////////////////////////////////////////////////////////////////////////

    testDateDayOfWeekInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_YEAR()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_YEAR(1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR([])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_year function
////////////////////////////////////////////////////////////////////////////////

    testDateYear : function () {
      var values = [
        [ "2000-04-29Z", 2000 ],
        [ "2012-02-12 13:24:12Z", 2012 ],
        [ "2012-02-12 23:59:59.991Z", 2012 ],
        [ "2012-02-12Z", 2012 ],
        [ "2012-02-12T13:24:12Z", 2012 ],
        [ "2012-02-12Z", 2012 ],
        [ "2012-2-12Z", 2012 ],
        [ "1910-01-02T03:04:05Z", 1910 ],
        [ "1910-01-02 03:04:05Z", 1910 ],
        [ "1910-01-02Z", 1910 ],
        [ "1970-01-01T01:05:27Z", 1970 ],
        [ "1970-01-01 01:05:27Z", 1970 ],
        [ "1970-1-1Z", 1970 ],
        [ "1221-02-28T23:59:59Z", 1221 ],
        [ "1221-02-28 23:59:59Z", 1221 ],
        [ "1221-02-28Z", 1221 ],
        [ "1221-2-28Z", 1221 ],
        [ "1000-12-24T04:12:00Z", 1000 ],
        [ "1000-12-24Z", 1000 ],
        [ "1000-12-24 04:12:00Z", 1000 ],
        [ "6789-12-31T23:59:58.99Z", 6789 ],
        [ "6789-12-31Z", 6789 ],
        [ "9999-12-31T23:59:59.999Z", 9999 ],
        [ "9999-12-31Z", 9999 ],
        [ "9999-12-31z", 9999 ],
        [ "9999-12-31", 9999 ],
        [ "2012Z", 2012 ],
        [ "2012z", 2012 ],
        [ "2012", 2012 ],
        [ "2012-1Z", 2012 ],
        [ "2012-1z", 2012 ],
        [ "2012-1-1z", 2012 ],
        [ "2012-01-01Z", 2012 ],
        [ "2012-01-01Z", 2012 ],
        [ "  2012-01-01Z", 2012 ],
        [ "  2012-01-01z", 2012 ],
        [ 1399395674000, 2014 ],
        [ 60123, 1970 ],
        [ 1, 1970 ],
        [ 0, 1970 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_YEAR(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_month function
////////////////////////////////////////////////////////////////////////////////

    testDateMonthInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MONTH()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MONTH(1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH([])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_month function
////////////////////////////////////////////////////////////////////////////////

    testDateMonth : function () {
      var values = [
        [ "2000-04-29Z", 4 ],
        [ "2012-02-12 13:24:12Z", 2 ],
        [ "2012-02-12 23:59:59.991Z", 2 ],
        [ "2012-02-12Z", 2 ],
        [ "2012-02-12T13:24:12Z", 2 ],
        [ "2012-02-12Z", 2 ],
        [ "2012-2-12Z", 2 ],
        [ "1910-01-02T03:04:05Z", 1 ],
        [ "1910-01-02 03:04:05Z", 1 ],
        [ "1910-01-02Z", 1 ],
        [ "1970-01-01T01:05:27Z", 1 ],
        [ "1970-01-01 01:05:27Z", 1 ],
        [ "1970-1-1Z", 1 ],
        [ "1221-02-28T23:59:59Z", 2 ],
        [ "1221-02-28 23:59:59Z", 2 ],
        [ "1221-02-28Z", 2 ],
        [ "1221-2-28Z", 2 ],
        [ "1000-12-24T04:12:00Z", 12 ],
        [ "1000-12-24Z", 12 ],
        [ "1000-12-24 04:12:00Z", 12 ],
        [ "6789-12-31T23:59:58.99Z", 12 ],
        [ "6789-12-31Z", 12 ],
        [ "9999-12-31T23:59:59.999Z", 12 ],
        [ "9999-12-31Z", 12 ],
        [ "9999-12-31z", 12 ],
        [ "9999-12-31", 12 ],
        [ "2012Z", 1 ],
        [ "2012z", 1 ],
        [ "2012", 1 ],
        [ "2012-2Z", 2 ],
        [ "2012-1Z", 1 ],
        [ "2012-1z", 1 ],
        [ "2012-1-1z", 1 ],
        [ "2012-01-01Z", 1 ],
        [ "2012-01-01Z", 1 ],
        [ "  2012-01-01Z", 1 ],
        [ "  2012-01-01z", 1 ],
        [ 1399395674000, 5 ],
        [ 60123, 1 ],
        [ 1, 1 ],
        [ 0, 1 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_MONTH(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_day function
////////////////////////////////////////////////////////////////////////////////

    testDateDayInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAY()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAY(1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY([])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_day function
////////////////////////////////////////////////////////////////////////////////

    testDateDay : function () {
      var values = [
        [ "2000-04-29Z", 29 ],
        [ "2012-02-12 13:24:12Z", 12 ],
        [ "2012-02-12 23:59:59.991Z", 12 ],
        [ "2012-02-12Z", 12 ],
        [ "2012-02-12T13:24:12Z", 12 ],
        [ "2012-02-12Z", 12 ],
        [ "2012-2-12Z", 12 ],
        [ "1910-01-02T03:04:05Z", 2 ],
        [ "1910-01-02 03:04:05Z", 2 ],
        [ "1910-01-02Z", 2 ],
        [ "1970-01-01T01:05:27Z", 1 ],
        [ "1970-01-01 01:05:27Z", 1 ],
        [ "1970-1-1Z", 1 ],
        [ "1221-02-28T23:59:59Z", 28 ],
        [ "1221-02-28 23:59:59Z", 28 ],
        [ "1221-02-28Z", 28 ],
        [ "1221-2-28Z", 28 ],
        [ "1000-12-24T04:12:00Z", 24 ],
        [ "1000-12-24Z", 24 ],
        [ "1000-12-24 04:12:00Z", 24 ],
        [ "6789-12-31T23:59:58.99Z", 31 ],
        [ "6789-12-31Z", 31 ],
        [ "9999-12-31T23:59:59.999Z", 31 ],
        [ "9999-12-31Z", 31 ],
        [ "9999-12-31z", 31 ],
        [ "9999-12-31", 31 ],
        [ "2012Z", 1 ],
        [ "2012z", 1 ],
        [ "2012", 1 ],
        [ "2012-2Z", 1 ],
        [ "2012-1Z", 1 ],
        [ "2012-1z", 1 ],
        [ "2012-1-1z", 1 ],
        [ "2012-01-01Z", 1 ],
        [ "2012-01-01Z", 1 ],
        [ "2012-01-02Z", 2 ],
        [ "2012-01-2Z", 2 ],
        [ "  2012-01-01Z", 1 ],
        [ "  2012-01-01z", 1 ],
        [ 1399395674000, 6 ],
        [ 60123, 1 ],
        [ 1, 1 ],
        [ 0, 1 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_DAY(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_hour function
////////////////////////////////////////////////////////////////////////////////

    testDateHourInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_HOUR()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_HOUR(1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR([])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_hour function
////////////////////////////////////////////////////////////////////////////////

    testDateHour : function () {
      var values = [
        [ "2000-04-29", 0 ],
        [ "2000-04-29Z", 0 ],
        [ "2012-02-12 13:24:12", 13 ],
        [ "2012-02-12 13:24:12Z", 13 ],
        [ "2012-02-12 23:59:59.991Z", 23 ],
        [ "2012-02-12", 0 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-02-12T13:24:12", 13 ],
        [ "2012-02-12T13:24:12Z", 13 ],
        [ "2012-02-12", 0 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-2-12Z", 0 ],
        [ "1910-01-02T03:04:05", 3 ],
        [ "1910-01-02T03:04:05Z", 3 ],
        [ "1910-01-02 03:04:05Z", 3 ],
        [ "1910-01-02Z", 0 ],
        [ "1970-01-01T01:05:27Z", 1 ],
        [ "1970-01-01 01:05:27Z", 1 ],
        [ "1970-01-01T12:05:27Z", 12 ],
        [ "1970-01-01 12:05:27Z", 12 ],
        [ "1970-1-1Z", 0 ],
        [ "1221-02-28T23:59:59Z", 23 ],
        [ "1221-02-28 23:59:59Z", 23 ],
        [ "1221-02-28Z", 0 ],
        [ "1221-2-28Z", 0 ],
        [ "1000-12-24T04:12:00Z", 4 ],
        [ "1000-12-24Z", 0 ],
        [ "1000-12-24 04:12:00Z", 4 ],
        [ "6789-12-31T23:59:58.99Z", 23 ],
        [ "6789-12-31Z", 0 ],
        [ "9999-12-31T23:59:59.999Z", 23 ],
        [ "9999-12-31Z", 0 ],
        [ "9999-12-31z", 0 ],
        [ "9999-12-31", 0 ],
        [ "2012Z", 0 ],
        [ "2012z", 0 ],
        [ "2012", 0 ],
        [ "2012-2Z", 0 ],
        [ "2012-1Z", 0 ],
        [ "2012-1z", 0 ],
        [ "2012-1-1z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-02Z", 0 ],
        [ "2012-01-2Z", 0 ],
        [ "  2012-01-01Z", 0 ],
        [ "  2012-01-01z", 0 ],
        [ 1399395674000, 17 ],
        [ 3600000, 1 ],
        [ 7200000, 2 ],
        [ 8200000, 2 ],
        [ 60123, 0 ],
        [ 1, 0 ],
        [ 0, 0 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_HOUR(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_minute function
////////////////////////////////////////////////////////////////////////////////

    testDateMinuteInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MINUTE()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MINUTE(1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE([])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_minute function
////////////////////////////////////////////////////////////////////////////////

    testDateMinute : function () {
      var values = [
        [ "2000-04-29Z", 0 ],
        [ "2012-02-12 13:24:12Z", 24 ],
        [ "2012-02-12 23:59:59.991Z", 59 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-02-12T13:24:12Z", 24 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-2-12Z", 0 ],
        [ "1910-01-02T03:04:05Z", 4 ],
        [ "1910-01-02 03:04:05Z", 4 ],
        [ "1910-01-02Z", 0 ],
        [ "1970-01-01T01:05:27Z", 5 ],
        [ "1970-01-01 01:05:27Z", 5 ],
        [ "1970-01-01T12:05:27Z", 5 ],
        [ "1970-01-01 12:05:27Z", 5 ],
        [ "1970-1-1Z", 0 ],
        [ "1221-02-28T23:59:59Z", 59 ],
        [ "1221-02-28 23:59:59Z", 59 ],
        [ "1221-02-28Z", 0 ],
        [ "1221-2-28Z", 0 ],
        [ "1000-12-24T04:12:00Z", 12 ],
        [ "1000-12-24Z", 0 ],
        [ "1000-12-24 04:12:00Z", 12 ],
        [ "6789-12-31T23:59:58.99Z", 59 ],
        [ "6789-12-31Z", 0 ],
        [ "9999-12-31T23:59:59.999Z", 59 ],
        [ "9999-12-31Z", 0 ],
        [ "9999-12-31z", 0 ],
        [ "9999-12-31", 0 ],
        [ "2012Z", 0 ],
        [ "2012z", 0 ],
        [ "2012", 0 ],
        [ "2012-2Z", 0 ],
        [ "2012-1Z", 0 ],
        [ "2012-1z", 0 ],
        [ "2012-1-1z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-02Z", 0 ],
        [ "2012-01-2Z", 0 ],
        [ "  2012-01-01Z", 0 ],
        [ "  2012-01-01z", 0 ],
        [ 1399395674000, 1 ],
        [ 3600000, 0 ],
        [ 7200000, 0 ],
        [ 8200000, 16 ],
        [ 60123, 1 ],
        [ 1, 0 ],
        [ 0, 0 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_MINUTE(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_second function
////////////////////////////////////////////////////////////////////////////////

    testDateSecondInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_SECOND()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_SECOND(1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SECOND(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SECOND(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SECOND([])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SECOND({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_second function
////////////////////////////////////////////////////////////////////////////////

    testDateSecond : function () {
      var values = [
        [ "2000-04-29Z", 0 ],
        [ "2012-02-12 13:24:12Z", 12 ],
        [ "2012-02-12 23:59:59.991Z", 59 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-02-12T13:24:12Z", 12 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-2-12Z", 0 ],
        [ "1910-01-02T03:04:05Z", 5 ],
        [ "1910-01-02 03:04:05Z", 5 ],
        [ "1910-01-02Z", 0 ],
        [ "1970-01-01T01:05:27Z", 27 ],
        [ "1970-01-01 01:05:27Z", 27 ],
        [ "1970-01-01T12:05:27Z", 27 ],
        [ "1970-01-01 12:05:27Z", 27 ],
        [ "1970-1-1Z", 0 ],
        [ "1221-02-28T23:59:59Z", 59 ],
        [ "1221-02-28 23:59:59Z", 59 ],
        [ "1221-02-28Z", 0 ],
        [ "1221-2-28Z", 0 ],
        [ "1000-12-24T04:12:00Z", 0 ],
        [ "1000-12-24Z", 0 ],
        [ "1000-12-24 04:12:00Z", 0 ],
        [ "6789-12-31T23:59:58.99Z", 58 ],
        [ "6789-12-31Z", 0 ],
        [ "9999-12-31T23:59:59.999Z", 59 ],
        [ "9999-12-31Z", 0 ],
        [ "9999-12-31z", 0 ],
        [ "9999-12-31", 0 ],
        [ "2012Z", 0 ],
        [ "2012z", 0 ],
        [ "2012", 0 ],
        [ "2012-2Z", 0 ],
        [ "2012-1Z", 0 ],
        [ "2012-1z", 0 ],
        [ "2012-1-1z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-02Z", 0 ],
        [ "2012-01-2Z", 0 ],
        [ "  2012-01-01Z", 0 ],
        [ "  2012-01-01z", 0 ],
        [ 1399395674000, 14 ],
        [ 3600000, 0 ],
        [ 7200000, 0 ],
        [ 8200000, 40 ],
        [ 61123, 1 ],
        [ 60123, 0 ],
        [ 2001, 2 ],
        [ 2000, 2 ],
        [ 1000, 1 ],
        [ 1, 0 ],
        [ 0, 0 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_SECOND(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_millisecond function
////////////////////////////////////////////////////////////////////////////////

    testDateMillisecondInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MILLISECOND()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MILLISECOND(1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MILLISECOND(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MILLISECOND(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MILLISECOND([])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MILLISECOND({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_millisecond function
////////////////////////////////////////////////////////////////////////////////

    testDateMillisecond : function () {
      var values = [
        [ "2000-04-29Z", 0 ],
        [ "2012-02-12 13:24:12Z", 0 ],
        [ "2012-02-12 23:59:59.991Z", 991 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-02-12T13:24:12Z", 0 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-2-12Z", 0 ],
        [ "1910-01-02T03:04:05Z", 0 ],
        [ "1910-01-02 03:04:05Z", 0 ],
        [ "1910-01-02Z", 0 ],
        [ "1970-01-01T01:05:27Z", 0 ],
        [ "1970-01-01 01:05:27Z", 0 ],
        [ "1970-01-01T12:05:27Z", 0 ],
        [ "1970-01-01 12:05:27Z", 0 ],
        [ "1970-1-1Z", 0 ],
        [ "1221-02-28T23:59:59Z", 0 ],
        [ "1221-02-28 23:59:59Z", 0 ],
        [ "1221-02-28Z", 0 ],
        [ "1221-2-28Z", 0 ],
        [ "1000-12-24T04:12:00Z", 0 ],
        [ "1000-12-24Z", 0 ],
        [ "1000-12-24 04:12:00Z", 0 ],
        [ "6789-12-31T23:59:58.99Z", 990 ],
        [ "6789-12-31Z", 0 ],
        [ "9999-12-31T23:59:59.999Z", 999 ],
        [ "9999-12-31Z", 0 ],
        [ "9999-12-31z", 0 ],
        [ "9999-12-31", 0 ],
        [ "2012Z", 0 ],
        [ "2012z", 0 ],
        [ "2012", 0 ],
        [ "2012-2Z", 0 ],
        [ "2012-1Z", 0 ],
        [ "2012-1z", 0 ],
        [ "2012-1-1z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-02Z", 0 ],
        [ "2012-01-2Z", 0 ],
        [ "  2012-01-01Z", 0 ],
        [ "  2012-01-01z", 0 ],
        [ 1399395674000, 0 ],
        [ 1399395674123, 123 ],
        [ 3600000, 0 ],
        [ 7200000, 0 ],
        [ 8200000, 0 ],
        [ 8200040, 40 ],
        [ 61123, 123 ],
        [ 60123, 123 ],
        [ 2001, 1 ],
        [ 2000, 0 ],
        [ 1000, 0 ],
        [ 753, 753 ],
        [ 53, 53 ],
        [ 1, 1 ],
        [ 0, 0 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_MILLISECOND(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_timestamp function
////////////////////////////////////////////////////////////////////////////////

    testDateTimestampInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_TIMESTAMP()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_TIMESTAMP(1, 1, 1, 1, 1, 1, 1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TIMESTAMP(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TIMESTAMP(1, null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TIMESTAMP(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TIMESTAMP([])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TIMESTAMP({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_timestamp function
////////////////////////////////////////////////////////////////////////////////

    testDateTimestamp : function () {
      var values = [
        [ "2000-04-29", 956966400000 ],
        [ "2000-04-29Z", 956966400000 ],
        [ "2012-02-12 13:24:12", 1329053052000 ],
        [ "2012-02-12 13:24:12Z", 1329053052000 ],
        [ "2012-02-12 23:59:59.991", 1329091199991 ],
        [ "2012-02-12 23:59:59.991Z", 1329091199991 ],
        [ "2012-02-12", 1329004800000 ],
        [ "2012-02-12Z", 1329004800000 ],
        [ "2012-02-12T13:24:12", 1329053052000 ],
        [ "2012-02-12T13:24:12Z", 1329053052000 ],
        [ "2012-02-12Z", 1329004800000 ],
        [ "2012-2-12Z", 1329004800000 ],
        [ "1910-01-02T03:04:05", -1893358555000 ],
        [ "1910-01-02T03:04:05Z", -1893358555000 ],
        [ "1910-01-02 03:04:05Z", -1893358555000 ],
        [ "1910-01-02", -1893369600000 ],
        [ "1910-01-02Z", -1893369600000 ],
        [ "1970-01-01T01:05:27Z", 3927000 ],
        [ "1970-01-01 01:05:27Z", 3927000 ],
        [ "1970-01-01T12:05:27Z", 43527000 ],
        [ "1970-01-01 12:05:27Z", 43527000 ],
        [ "1970-1-1Z", 0 ],
        [ "1221-02-28T23:59:59Z", -23631004801000 ],
        [ "1221-02-28 23:59:59Z", -23631004801000 ],
        [ "1221-02-28Z", -23631091200000 ],
        [ "1221-2-28Z", -23631091200000 ],
        [ "1000-12-24T04:12:00Z", -30579364080000 ],
        [ "1000-12-24Z", -30579379200000 ],
        [ "1000-12-24 04:12:00Z", -30579364080000 ],
        [ "6789-12-31T23:59:58.99Z", 152104521598990 ],
        [ "6789-12-31Z", 152104435200000 ],
        [ "9999-12-31T23:59:59.999Z", 253402300799999 ],
        [ "9999-12-31Z", 253402214400000 ],
        [ "9999-12-31z", 253402214400000 ],
        [ "9999-12-31", 253402214400000 ],
        [ "2012Z", 1325376000000 ],
        [ "2012z", 1325376000000 ],
        [ "2012", 1325376000000 ],
        [ "2012-2Z", 1328054400000 ],
        [ "2012-1Z", 1325376000000 ],
        [ "2012-1z", 1325376000000 ],
        [ "2012-1-1z", 1325376000000 ],
        [ "2012-01-01Z", 1325376000000 ],
        [ "2012-01-01Z", 1325376000000 ],
        [ "2012-01-02Z", 1325462400000 ],
        [ "2012-01-2Z", 1325462400000 ],
        [ "  2012-01-01Z", 1325376000000 ],
        [ "  2012-01-01z", 1325376000000 ],
        [ 1399395674000, 1399395674000 ],
        [ 3600000, 3600000 ],
        [ 7200000, 7200000 ],
        [ 8200000, 8200000 ],
        [ 61123, 61123 ],
        [ 60123, 60123 ],
        [ 2001, 2001 ],
        [ 2000, 2000 ],
        [ 1000, 1000 ],
        [ 121, 121 ],
        [ 1, 1 ],
        [ 0, 0 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_TIMESTAMP(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_iso8601 function
////////////////////////////////////////////////////////////////////////////////

    testDateIso8601Invalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISO8601()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISO8601(1, 1, 1, 1, 1, 1, 1, 1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISO8601(1, null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISO8601(null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISO8601(false)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISO8601([])");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISO8601({})");

      var values = [ 
        "foobar", 
        "2012fjh", 
        "  2012tjjgg", 
        "", 
        " ", 
        "abc2012-01-01", 
        "2000-00-00", 
        "2001-02-11 24:00:00",
        "2001-02-11 24:01:01",
        "2001-02-11 25:01:01",
        "2001-02-11 23:60:00",
        "2001-02-11 23:01:60",
        "2001-02-11 23:01:99",
        "2001-00-11",
        "2001-0-11",
        "2001-13-11",
        "2001-13-32",
        "2001-01-0",
        "2001-01-00",
        "2001-01-32",
        "2001-1-32"
      ];
        
      values.forEach(function(value) {
        assertQueryError(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(@value)", { value: value });
      });  
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_iso8601 function
////////////////////////////////////////////////////////////////////////////////

    testDateIso8601 : function () {
      var values = [
        [ "2000-04-29", "2000-04-29T00:00:00.000Z" ],
        [ "2000-04-29Z", "2000-04-29T00:00:00.000Z" ],
        [ "2012-02-12 13:24:12", "2012-02-12T13:24:12.000Z" ],
        [ "2012-02-12 13:24:12Z", "2012-02-12T13:24:12.000Z" ],
        [ "2012-02-12 23:59:59.991", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12 23:59:59.991Z", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12", "2012-02-12T00:00:00.000Z" ],
        [ "2012-02-12Z", "2012-02-12T00:00:00.000Z" ],
        [ "2012-02-12T13:24:12", "2012-02-12T13:24:12.000Z" ],
        [ "2012-02-12T13:24:12Z", "2012-02-12T13:24:12.000Z" ],
        [ "2012-02-12Z", "2012-02-12T00:00:00.000Z" ],
        [ "2012-2-12Z", "2012-02-12T00:00:00.000Z" ],
        [ "1910-01-02T03:04:05", "1910-01-02T03:04:05.000Z" ],
        [ "1910-01-02T03:04:05Z", "1910-01-02T03:04:05.000Z" ],
        [ "1910-01-02 03:04:05", "1910-01-02T03:04:05.000Z" ],
        [ "1910-01-02 03:04:05Z", "1910-01-02T03:04:05.000Z" ],
        [ "1910-01-02", "1910-01-02T00:00:00.000Z" ],
        [ "1910-01-02Z", "1910-01-02T00:00:00.000Z" ],
        [ "1970-01-01T01:05:27+01:00", "1970-01-01T00:05:27.000Z" ],
        [ "1970-01-01T01:05:27-01:00", "1970-01-01T02:05:27.000Z" ],
        [ "1970-01-01T01:05:27", "1970-01-01T01:05:27.000Z" ],
        [ "1970-01-01T01:05:27Z", "1970-01-01T01:05:27.000Z" ],
        [ "1970-01-01 01:05:27Z", "1970-01-01T01:05:27.000Z" ],
        [ "1970-01-01T12:05:27Z", "1970-01-01T12:05:27.000Z" ],
        [ "1970-01-01 12:05:27Z", "1970-01-01T12:05:27.000Z" ],
        [ "1970-1-1Z", "1970-01-01T00:00:00.000Z" ],
        [ "1221-02-28T23:59:59", "1221-02-28T23:59:59.000Z" ],
        [ "1221-02-28T23:59:59Z", "1221-02-28T23:59:59.000Z" ],
        [ "1221-02-28 23:59:59Z", "1221-02-28T23:59:59.000Z" ],
        [ "1221-02-28", "1221-02-28T00:00:00.000Z" ],
        [ "1221-02-28Z", "1221-02-28T00:00:00.000Z" ],
        [ "1221-2-28Z", "1221-02-28T00:00:00.000Z" ],
        [ "1000-12-24T04:12:00Z", "1000-12-24T04:12:00.000Z" ],
        [ "1000-12-24Z", "1000-12-24T00:00:00.000Z" ],
        [ "1000-12-24 04:12:00Z", "1000-12-24T04:12:00.000Z" ],
        [ "6789-12-31T23:59:58.99Z", "6789-12-31T23:59:58.990Z" ],
        [ "6789-12-31Z", "6789-12-31T00:00:00.000Z" ],
        [ "9999-12-31T23:59:59.999Z", "9999-12-31T23:59:59.999Z" ],
        [ "9999-12-31Z", "9999-12-31T00:00:00.000Z" ],
        [ "9999-12-31z", "9999-12-31T00:00:00.000Z" ],
        [ "9999-12-31", "9999-12-31T00:00:00.000Z" ],
        [ "2012Z", "2012-01-01T00:00:00.000Z" ],
        [ "2012z", "2012-01-01T00:00:00.000Z" ],
        [ "2012", "2012-01-01T00:00:00.000Z" ],
        [ "2012-2Z", "2012-02-01T00:00:00.000Z" ],
        [ "2012-1Z", "2012-01-01T00:00:00.000Z" ],
        [ "2012-1z", "2012-01-01T00:00:00.000Z" ],
        [ "2012-1-1z", "2012-01-01T00:00:00.000Z" ],
        [ "2012-01-01", "2012-01-01T00:00:00.000Z" ],
        [ "2012-01-01Z", "2012-01-01T00:00:00.000Z" ],
        [ "2012-01-01Z", "2012-01-01T00:00:00.000Z" ],
        [ "2012-01-02Z", "2012-01-02T00:00:00.000Z" ],
        [ "2012-01-2Z", "2012-01-02T00:00:00.000Z" ],
        [ "  2012-01-01Z", "2012-01-01T00:00:00.000Z" ],
        [ "  2012-01-01z", "2012-01-01T00:00:00.000Z" ],
        [ 1399395674000, "2014-05-06T17:01:14.000Z" ],
        [ 3600000, "1970-01-01T01:00:00.000Z" ],
        [ 7200000, "1970-01-01T02:00:00.000Z" ],
        [ 8200000, "1970-01-01T02:16:40.000Z" ],
        [ 61123, "1970-01-01T00:01:01.123Z" ],
        [ 60123, "1970-01-01T00:01:00.123Z" ],
        [ 2001, "1970-01-01T00:00:02.001Z" ],
        [ 2000, "1970-01-01T00:00:02.000Z" ],
        [ 1000, "1970-01-01T00:00:01.000Z" ],
        [ 121, "1970-01-01T00:00:00.121Z" ],
        [ 1, "1970-01-01T00:00:00.001Z" ],
        [ 0, "1970-01-01T00:00:00.000Z" ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_ISO8601(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_iso8601 function
////////////////////////////////////////////////////////////////////////////////

    testDateIso8601Alternative : function () {
      var values = [
        [ [ 1000, 1, 1, 0, 0, 0, 0 ], "1000-01-01T00:00:00.000Z" ],
        [ [ 9999, 12, 31, 23, 59, 59, 999 ], "9999-12-31T23:59:59.999Z" ],
        [ [ 2012, 1, 1, 13, 12, 14, 95 ], "2012-01-01T13:12:14.095Z" ],
        [ [ 1234, 12, 31, 23, 59, 59, 477 ], "1234-12-31T23:59:59.477Z" ],
        [ [ 2014, 5, 7, 12, 6 ], "2014-05-07T12:06:00.000Z" ],
        [ [ 2014, 5, 7, 12 ], "2014-05-07T12:00:00.000Z" ],
        [ [ 2014, 5, 7 ], "2014-05-07T00:00:00.000Z" ],
        [ [ 2014, 5, 7 ], "2014-05-07T00:00:00.000Z" ],
        [ [ 1000, 1, 1 ], "1000-01-01T00:00:00.000Z" ],
        [ [ "1000", "01", "01", "00", "00", "00", "00" ], "1000-01-01T00:00:00.000Z" ],
        [ [ "1000", "1", "1", "0", "0", "0", "0" ], "1000-01-01T00:00:00.000Z" ],
        [ [ "9999", "12", "31", "23", "59", "59", "999" ], "9999-12-31T23:59:59.999Z" ],
        [ [ "2012", "1", "1", "13", "12", "14", "95" ], "2012-01-01T13:12:14.095Z" ],
        [ [ "1234", "12", "31", "23", "59", "59", "477" ], "1234-12-31T23:59:59.477Z" ],
        [ [ "2014", "05", "07", "12", "06" ], "2014-05-07T12:06:00.000Z" ],
        [ [ "2014", "5", "7", "12", "6" ], "2014-05-07T12:06:00.000Z" ],
        [ [ "2014", "5", "7", "12" ], "2014-05-07T12:00:00.000Z" ],
        [ [ "2014", "5", "7" ], "2014-05-07T00:00:00.000Z" ],
        [ [ "2014", "5", "7" ], "2014-05-07T00:00:00.000Z" ],
        [ [ "1000", "01", "01" ], "1000-01-01T00:00:00.000Z" ],
        [ [ "1000", "1", "1" ], "1000-01-01T00:00:00.000Z" ],
      ];
      
      values.forEach(function (value) {
        var query = "RETURN DATE_ISO8601(" + 
            value[0].map(function(v) {
              return JSON.stringify(v)
            }).join(", ") + ")";
       
        var actual = getQueryResults(query);
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date transformations
////////////////////////////////////////////////////////////////////////////////

    testDateTransformations1 : function () {
      var dt = "2014-05-07T15:23:21.446";
      var actual;
      
      actual = getQueryResults("RETURN DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value))", { value: dt });
      assertEqual([ new Date(dt).getTime() ], actual); 
      
      actual = getQueryResults("RETURN DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value))", { value: dt + "Z" });
      assertEqual([ new Date(dt).getTime() ], actual); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date transformations
////////////////////////////////////////////////////////////////////////////////

    testDateTransformations2 : function () {
      var dt = "2014-05-07T15:23:21.446";
      var actual;
      
      actual = getQueryResults("RETURN DATE_ISO8601(DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value)))", { value: dt });
      assertEqual([ dt + "Z" ], actual); 

      actual = getQueryResults("RETURN DATE_ISO8601(DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value)))", { value: dt + "Z" });
      assertEqual([ dt + "Z" ], actual); 
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
