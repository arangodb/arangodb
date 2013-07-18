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
      assertEqual([ true ], getQueryResults("RETURN LIKE(\"\\abc\", \"\\\\a%\")"));
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
      var actual, expected;

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
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnion1 : function () {
      var expected = [ [ 1, 2, 3, 1, 2, 3 ], [ 1, 2, 3, 1, 2, 3 ] ];
      var actual = getQueryResults("FOR u IN [ 1, 2 ] return UNION([ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnion2 : function () {
      var expected = [ [ 1, 2, 3, 3, 2, 1 ], [ 1, 2, 3, 3, 2, 1 ] ];
      var actual = getQueryResults("FOR u IN [ 1, 2 ] return UNION([ 1, 2, 3 ], [ 3, 2, 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnion3 : function () {
      var expected = [ "Fred", "John", "John", "Amy" ];
      var actual = getQueryResults("FOR u IN UNION([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"]) return u");
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
/// @brief test document function
////////////////////////////////////////////////////////////////////////////////
    
    testDocument1 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      var d1 = cx.save({ "title" : "123", "value" : 456 });
      var d2 = cx.save({ "title" : "nada", "value" : 123 });

      var expected, actual;

      expected = [ { title: "123", value : 456 } ];
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + d1._id + "\")");
      assertEqual(expected, actual);
      
      expected = [ { title: "nada", value : 123 } ];
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + d2._id + "\")");
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

      expected = [ { title: "123", value : 456, zxy: 1 } ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : d1._id });
      assertEqual(expected, actual);
      
      expected = [ { title: "nada", value : 123, zzzz : false } ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : d2._id });
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

      cx.remove(d3);
      
      expected = [ [ { title: "nada", value : 123, zzzz : false } ] ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : [ d2._id, d3._id, "abc/def" ] }, true);
      assertEqual(expected, actual);
      
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document function
////////////////////////////////////////////////////////////////////////////////
    
    testDocumentInvalid : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);

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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS(false, \"fox\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS(3, \"fox\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS(\"yes\", \"fox\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS([ ], \"fox\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS({ }, null)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS({ }, false)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS({ }, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS({ }, 1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS({ }, [ ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN HAS({ }, { })"); 
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
