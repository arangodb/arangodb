/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual */
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

function ahuacatlStringFunctionsTestSuite () {
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
      assertEqual([ -1 ], getQueryResults("RETURN CONTAINS(\"test\", \"test2\", \"test3\")")); 
      assertEqual([ true ], getQueryResults("RETURN CONTAINS(null, null)")); 
      assertEqual([ true ], getQueryResults("RETURN CONTAINS(4, 4)")); 
      assertEqual([ true ], getQueryResults("RETURN CONTAINS({ }, { })")); 
      assertEqual([ false ], getQueryResults("RETURN CONTAINS([ ], [ ])")); 
      assertEqual([ false ], getQueryResults("RETURN CONTAINS(null, \"yes\")")); 
      assertEqual([ false ], getQueryResults("RETURN CONTAINS(3, null)")); 
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
      assertEqual([ "nu" ], getQueryResults("RETURN LEFT(null, 2)")); 
      assertEqual([ "tr" ], getQueryResults("RETURN LEFT(true, 2)")); 
      assertEqual([ "4" ], getQueryResults("RETURN LEFT(4, 2)")); 
      assertEqual([ "" ], getQueryResults("RETURN LEFT([ ], 2)")); 
      assertEqual([ "[o" ], getQueryResults("RETURN LEFT({ }, 2)")); 
      assertEqual([ "" ], getQueryResults("RETURN LEFT('foo', null)")); 
      assertEqual([ "f" ], getQueryResults("RETURN LEFT('foo', true)")); 
      assertEqual([ "" ], getQueryResults("RETURN LEFT('foo', 'bar')")); 
      assertEqual([ "" ], getQueryResults("RETURN LEFT('foo', [ ])")); 
      assertEqual([ "" ], getQueryResults("RETURN LEFT('foo', { })")); 
      assertEqual([ "" ], getQueryResults("RETURN LEFT('foo', -1)")); 
      assertEqual([ "" ], getQueryResults("RETURN LEFT('foo', -1.5)")); 
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
      assertEqual([ "ll" ], getQueryResults("RETURN RIGHT(null, 2)")); 
      assertEqual([ "ue" ], getQueryResults("RETURN RIGHT(true, 2)")); 
      assertEqual([ "4" ], getQueryResults("RETURN RIGHT(4, 2)")); 
      assertEqual([ "" ], getQueryResults("RETURN RIGHT([ ], 2)")); 
      assertEqual([ "t]" ], getQueryResults("RETURN RIGHT({ }, 2)")); 
      assertEqual([ "" ], getQueryResults("RETURN RIGHT('foo', null)")); 
      assertEqual([ "o" ], getQueryResults("RETURN RIGHT('foo', true)")); 
      assertEqual([ "" ], getQueryResults("RETURN RIGHT('foo', 'bar')")); 
      assertEqual([ "" ], getQueryResults("RETURN RIGHT('foo', [ ])")); 
      assertEqual([ "" ], getQueryResults("RETURN RIGHT('foo', { })")); 
      assertEqual([ "" ], getQueryResults("RETURN RIGHT('foo', -1)")); 
      assertEqual([ "" ], getQueryResults("RETURN RIGHT('foo', -1.5)")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test substitute function
////////////////////////////////////////////////////////////////////////////////

    testSubstitute : function () {
      var values = [
        [ "the quick brown dog jumped over the lazy foxx", "the quick brown foxx jumped over the lazy dog", [ "foxx", "dog" ], [ "dog", "foxx" ] ],
        [ "the quick brown foxx jumped over the lazy foxx", "the quick brown foxx jumped over the lazy dog", [ "foxx", "dog" ], [ "foxx", "foxx" ] ],
        [ "the quick brown dog jumped over the lazy foxx", "the quick brown foxx jumped over the lazy dog", { "foxx": "dog", "dog": "foxx" } ],
        [ "the quick brown foxx jumped over the lazy foxx", "the quick brown foxx jumped over the lazy dog", { "foxx": "foxx", "dog": "foxx" } ],
        [ "the unspecified unspecified foxx jumped over the unspecified dog", "the quick brown foxx jumped over the lazy dog", [ "quick", "brown", "lazy" ], "unspecified" ],
        [ "the slow yellow foxx jumped over the eager dog", "the quick brown foxx jumped over the lazy dog", [ "quick", "brown", "lazy" ], [ "slow", "yellow", "eager" ] ],
        [ "the empty  foxx jumped over the  dog", "the quick brown foxx jumped over the lazy dog", [ "quick", "brown", "lazy" ], [ "empty" ] ],
        [ "the empty empty foxx jumped over the empty dog", "the quick brown foxx jumped over the lazy dog", [ "quick", "brown", "lazy" ], "empty" ],
        [ "the quick brown foxx jumped over the empty. dog", "the quick brown foxx jumped over the lazy dog", [ "dog", "lazy" ], "empty.", 1 ],
        [ "the.quick.brown.foxx.jumped.over.the.lazy\tdog", "the\r\nquick\r\nbrown\r\nfoxx\r\njumped\r\nover\r\nthe\r\nlazy\tdog", "\r\n", "." ],
        [ "the.quick.brown.foxx.jumped\r\nover\r\nthe\r\nlazy dog", "the\r\nquick\r\nbrown\r\nfoxx\r\njumped\r\nover\r\nthe\r\nlazy dog", "\r\n", ".", 4 ],
        [ "A capital foxx escaped!", "the quick brown foxx jumped over the lazy dog", [ "the quick brown", "jumped over the lazy dog" ], [ "A capital", "escaped!" ] ],
        [ "a quick brown foxx jumped over a lazy dog", "the quick brown foxx jumped over the lazy dog", "the", "a" ],
        [ "a quick brown foxx jumped over the lazy dog", "the quick brown foxx jumped over the lazy dog", "the", "a", 1 ],
        [ "the quick brown foxx jumped over the lazy dog", "the quick brown foxx jumped over the lazy dog", "the", "a", 0 ],
        [ "a quick brown foxx jumped over a lazy dog", "the quick brown foxx jumped over the lazy dog", [ "the" ], [ "a" ] ],
        [ "a quick brown foxx jumped over the lazy dog", "the quick brown foxx jumped over the lazy dog", [ "the" ], [ "a" ], 1 ],
        [ "mötör quick brown mötör jumped over the lazy dog", "the quick brown foxx jumped over the lazy dog", [ "over", "the", "foxx" ], "mötör", 2 ],
        [ "AbCdEF", "aBcDef", { a: "A", B: "b", c: "C", D: "d", e: "E", f: "F" } ],
        [ "AbcDef", "aBcDef", { a: "A", B: "b", c: "C", D: "d", e: "E", f: "F" }, 2 ],
        [ "aBcDef", "aBcDef", { a: "A", B: "b", c: "C", D: "d", e: "E", f: "F" }, 0 ],
        [ "xxxxyyyyzzz", "aaaabbbbccc", [ "a", "b", "c" ], [ "x", "y", "z" ] ],
        [ "xxaabbbbccc", "aaaabbbbccc", [ "a", "b", "c" ], [ "x", "y", "z" ], 2 ],
        [ "xxxxyybbccc", "aaaabbbbccc", [ "a", "b", "c" ], [ "x", "y", "z" ], 6 ],
        [ "aaaayyybccc", "aaaabbbbccc", [ "A", "b", "c" ], [ "x", "y", "z" ], 3 ],

      ];

      values.forEach(function(value) {
        var expected = value[0], args = [ ],i, n = value.length;
        for (i = 1; i < n; ++i) {
          args.push(JSON.stringify(value[i]));
        }
require("internal").print(args.join(", "));
        assertEqual([ expected ], getQueryResults("RETURN SUBSTITUTE(" + args.join(", ") + ")"));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test substitute function
////////////////////////////////////////////////////////////////////////////////

    testSubstituteInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUBSTITUTE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUBSTITUTE('foo', 'bar', 'baz', 2, 2)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test split function
////////////////////////////////////////////////////////////////////////////////
      
    testSplit : function () {
      var values = [
        [ [ "" ], "", "\n" ],
        [ [ "" ], "", "foobar" ],
        [ [ ], "", "" ],
        [ [ ], "", [ "" ] ],
        [ [ "" ], "", [ "a", "b", "c" ] ],
        [ [ "this\nis\na\nt", "st" ], "this\nis\na\ntest", "e" ],
        [ [ "th", "s\n", "s\na\nt", "st" ], "this\nis\na\ntest", [ "e", "i" ] ],
        [ [ "th", "\n", "\na\ntest" ], "this\nis\na\ntest", "is" ],
        [ [ "this", "is", "a", "test" ], "this\nis\na\ntest", "\n" ],
        [ [ "this", "is", "a", "test" ], "this\nis\na\ntest", [ "\n" ] ],
        [ [ "this", "is", "a", "test" ], "this\nis\na\ntest", [ "\n", "\r" ] ],
        [ [ "this", "is", "a", "test" ], "this\ris\ra\rtest", [ "\n", "\r" ] ],
        [ [ "this", "is", "a", "test" ], "this\tis\ta\ttest", [ "\t" ] ],
        [ [ "this", "is", "a", "test" ], "this\tis\ta\ttest", "\t" ],
        [ [ "this", "is", "a", "test" ], "this\nis\ra\ttest", [ "\n", "\r", "\t" ] ],
        [ [ "this", "is", "a", "test" ], "this is a test", [ " " ] ],
        [ [ "this", "is", "a", "test" ], "this is a test", " " ],
        [ [ "this", "is", "a", "test" ], "this/SEP/is/SEP/a/SEP/test", "/SEP/" ],
        [ [ "this", "is", "a", "test" ], "this/SEP/is/SEP/a/SEP/test", [ "/SEP/" ] ],
        [ [ "this", "is", "a", "test" ], "this/SEP1/is/SEP2/a/SEP3/test", [ "/SEP1/", "/SEP2/", "/SEP3/" ] ],
        [ [ "the", "quick", "brown", "foxx" ], "the quick brown foxx", " " ],
        [ [ "the quick ", " foxx" ], "the quick brown foxx", "brown" ],
        [ [ "t", "h", "e", " ", "q", "u", "i", "c", "k", " ", "b", "r", "o", "w", "n", " ", "f", "o", "x", "x" ], "the quick brown foxx", "" ]
      ];

      values.forEach(function(value) {
        var expected = value[0], text = value[1], separator = value[2];
        assertEqual([ expected ], getQueryResults("RETURN SPLIT(" + JSON.stringify(text) + ", " + JSON.stringify(separator) + ")"), value);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test split function
////////////////////////////////////////////////////////////////////////////////
      
    testSplitMaxLength : function () {
      var values = [
        [ null, "foobar", "", -1 ],
        [ null, "foobar", "", -10 ],
        [ [ ], "foobar", "", 0 ],
        [ [ "f" ], "foobar", "", 1 ],
        [ [ ], "this\nis\na\ntest", "\n", 0 ],
        [ [ "this" ], "this\nis\na\ntest", "\n", 1 ],
        [ [ "this", "is", "a" ], "this\nis\na\ntest", "\n", 3 ],
        [ [ "this", "is", "a", "test" ], "this\nis\na\ntest", "\n", 5 ],
        [ [ "this", "is", "a", "test" ], "this\nis\na\ntest", "\n", 500 ],
        [ [ "t", "h", "i", "s", " " ], "this is a test", "", 5 ]
      ];

      values.forEach(function(value) {
        var expected = value[0], text = value[1], separator = value[2], limit = value[3];
        assertEqual([ expected ], getQueryResults("RETURN SPLIT(" + JSON.stringify(text) + ", " + JSON.stringify(separator) + ", " + JSON.stringify(limit) + ")"));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test split function
////////////////////////////////////////////////////////////////////////////////
    
    testSplitEmpty : function () {
      assertEqual([ [ "the foxx" ] ], getQueryResults("RETURN SPLIT('the foxx')")); 
      assertEqual([ [ "" ] ], getQueryResults("RETURN SPLIT('')")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test split function
////////////////////////////////////////////////////////////////////////////////
    
    testSplitInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SPLIT()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SPLIT('foo', '', 10, '')"); 
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

    testTrimSpecial : function () {
      var expected = [ 'foo', '  foo  ', '', 'abc', '\t\r\nabc\n\r\t', '\r\nabc\t\r',  'a\rb\n', '\rb\n', '\ta\rb' ];
      var actual = getQueryResults("FOR t IN [ [ '  foo  ', '\r\n\t ' ], [ '  foo  ', '\r\n\t' ], [ '', '\r\n\t' ], [ '\t\r\nabc\n\r\t', '\r\n\t ' ], [ '\t\r\nabc\n\r\t', '\r\n ' ], [ '\t\r\nabc\t\r\n', '\t\n' ], [ '\ta\rb\nc', '\tc' ], [ '\ta\rb\nc', '\tac' ], [ '\ta\rb\nc', '\nc' ] ] RETURN TRIM(t[0], t[1])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test trim function
////////////////////////////////////////////////////////////////////////////////

    testTrimInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TRIM()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TRIM('foo', 2, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LTRIM('foo', 2, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LTRIM()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RTRIM('foo', 2, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RTRIM()"); 

      assertEqual([ "null" ], getQueryResults("RETURN TRIM(null)")); 
      assertEqual([ "true" ], getQueryResults("RETURN TRIM(true)")); 
      assertEqual([ "4" ], getQueryResults("RETURN TRIM(4)")); 
      assertEqual([ "" ], getQueryResults("RETURN TRIM([ ])")); 
      assertEqual([ "[object Object]" ], getQueryResults("RETURN TRIM({ })")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', null)")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', true)")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', 'bar')")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', [ ])")); 
      assertEqual([ "f" ], getQueryResults("RETURN TRIM('foo', { })")); // { } = "[object Object]" 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', -1)")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', -1.5)")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', 3)")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ltrim function
////////////////////////////////////////////////////////////////////////////////

    testLtrim : function () {
      var expected = [ 'foo  ', 'abc\n\r\t', 'a\rb\nc ', 'This\nis\r\na\ttest\r\n' ];
      var actual = getQueryResults("FOR t IN [ '  foo  ', '\t\r\nabc\n\r\t', '\ta\rb\nc ', '\r\nThis\nis\r\na\ttest\r\n' ] RETURN LTRIM(t)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ltrim function
////////////////////////////////////////////////////////////////////////////////

    testLtrimSpecial1 : function () {
      var expected = [ 'foo  ', '\t\r\nabc\n\r\t', '\ta\rb\nc ', 'This\nis\r\na\ttest\r\n' ];
      var actual = getQueryResults("FOR t IN [ '  foo  ', '\t\r\nabc\n\r\t', '\ta\rb\nc ', '\r\nThis\nis\r\na\ttest\r\n' ] RETURN LTRIM(t, '\r \n')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ltrim function
////////////////////////////////////////////////////////////////////////////////

    testLtrimSpecial2 : function () {
      var expected = [ '  foo  ', 'a,b,c,d,,e,f,,', 'foo,bar,baz\r\n' ];
      var actual = getQueryResults("FOR t IN [ '  foo  ', ',,,a,b,c,d,,e,f,,', 'foo,bar,baz\r\n' ] RETURN LTRIM(t, ',\n')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test rtrim function
////////////////////////////////////////////////////////////////////////////////

    testRtrim : function () {
      var expected = [ '  foo', '\t\r\nabc', '\ta\rb\nc', '\r\nThis\nis\r\na\ttest' ];
      var actual = getQueryResults("FOR t IN [ '  foo  ', '\t\r\nabc\n\r\t', '\ta\rb\nc ', '\r\nThis\nis\r\na\ttest\r\n' ] RETURN RTRIM(t)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ltrim function
////////////////////////////////////////////////////////////////////////////////

    testRtrimSpecial1 : function () {
      var expected = [ '  foo', '\t\r\nabc\n\r\t', '\ta\rb\nc', '\r\nThis\nis\r\na\ttest' ];
      var actual = getQueryResults("FOR t IN [ '  foo  ', '\t\r\nabc\n\r\t', '\ta\rb\nc ', '\r\nThis\nis\r\na\ttest\r\n' ] RETURN RTRIM(t, '\r \n')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ltrim function
////////////////////////////////////////////////////////////////////////////////

    testRtrimSpecial2 : function () {
      var expected = [ '  foo  ', ',,,a,b,c,d,,e,f', 'foo,bar,baz\r' ];
      var actual = getQueryResults("FOR t IN [ '  foo  ', ',,,a,b,c,d,,e,f,,', 'foo,bar,baz\r\n' ] RETURN RTRIM(t, ',\n')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test find_first function
////////////////////////////////////////////////////////////////////////////////

    testFindFirstEmpty1 : function () {
      [ 'foo', 'bar', 'baz', 'FOO', 'BAR', 'true', ' ' ].forEach(function(v) {
        var actual = getQueryResults("RETURN FIND_FIRST('', " + JSON.stringify(v) + ")");
        assertEqual([ -1 ], actual);
      });

      var actual = getQueryResults("RETURN FIND_FIRST('', '')");
      assertEqual([ 0 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test find_first function
////////////////////////////////////////////////////////////////////////////////

    testFindFirstEmpty2 : function () {
      [ 'foo', 'bar', 'baz', 'FOO', 'BAR', 'true', ' ', '' ].forEach(function(v) {
        var actual = getQueryResults("RETURN FIND_FIRST(" + JSON.stringify(v) + ", '')");
        assertEqual([ 0 ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test find_first function
////////////////////////////////////////////////////////////////////////////////

    testFindFirst : function () {
      [ 
        [ -1, 'foo', 'bar' ],
        [ 3, 'foobar', 'bar' ],
        [ 3, 'Foobar', 'bar' ],
        [ -1, 'foobar', 'Bar' ],
        [ 16, 'the quick brown bar jumped over the lazy dog', 'bar' ],
        [ 16, 'the quick brown bar jumped over the lazy dog bar', 'bar' ],
        [ 3, 'FOOBAR', 'BAR' ],
        [ -1, 'FOOBAR', 'bar' ],
        [ -1, 'the quick brown foxx', 'the foxx' ],
        [ 0, 'the quick brown foxx', 'the quick' ],
        [ -1, 'the quick brown foxx', 'the quick brown foxx j' ],
        [ 4, 'the quick brown foxx', 'quick brown' ],
        [ 35, 'the quick brown foxx jumped over a\nnewline', 'newline' ],
        [ 14, 'some linebreak\r\ngoes here', '\r\n' ],
        [ 12, 'foo BAR foo bar', 'bar' ],
        [ 4, 'foo bar foo bar', 'bar' ]
      ].forEach(function(v) {
        var actual = getQueryResults("RETURN FIND_FIRST(" + JSON.stringify(v[1]) + ", " + JSON.stringify(v[2]) + ")");
        assertEqual([ v[0] ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test find_first function
////////////////////////////////////////////////////////////////////////////////

    testFindFirstStartEnd : function () {
      [ 
        [ 3, 'foobar', 'bar', 2 ],
        [ 3, 'foobar', 'bar', 3 ],
        [ -1, 'foobar', 'bar', 4 ],
        [ 3, 'foobar', 'bar', 1, 5 ],
        [ -1, 'foobar', 'bar', 4, 5 ],
        [ -1, 'foobar', 'bar', 1, 4 ],
        [ 3, 'foobar', 'bar', 3 ],
        [ -1, 'foobar', 'bar', 0, 4 ],
        [ 3, 'foobar', 'bar', 0, 5 ],
        [ 3, 'foobar', 'bar', 0, 999 ],
        [ 0, 'the quick brown bar jumped over the lazy dog', 'the', 0 ],
        [ 32, 'the quick brown bar jumped over the lazy dog', 'the', 1 ],
        [ 4, 'the quick brown bar jumped over the lazy dog', 'q', 1 ],
        [ 4, 'the quick brown bar jumped over the lazy dog', 'q', 3 ],
        [ 4, 'the quick brown bar jumped over the lazy dog', 'q', 4 ],
        [ -1, 'the quick brown bar jumped over the lazy dog', 'q', 5 ]
      ].forEach(function(v) {
        var actual = getQueryResults("RETURN FIND_FIRST(" + JSON.stringify(v[1]) + ", " + JSON.stringify(v[2]) + ", " + v[3] + ", " + (v[4] === undefined ? null : v[4]) + ")");
        assertEqual([ v[0] ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test find first function
////////////////////////////////////////////////////////////////////////////////

    testFindFirstInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FIND_FIRST()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FIND_FIRST('foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FIND_FIRST('foo', 'bar', 2, 2, 2)"); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST(null, 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST(true, 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST(4, 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST([ ], 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST({ }, 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', null)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', true)")); 
      assertEqual([ 0 ], getQueryResults("RETURN FIND_FIRST('foo', [ ])")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', { })")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', -1)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', -1.5)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', 3)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', 'bar', 'baz')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', 'bar', 1, 'bar')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', 'bar', -1)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', 'bar', 1, -1)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', 'bar', 1, 0)")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test find_last function
////////////////////////////////////////////////////////////////////////////////

    testFindLastEmpty1 : function () {
      [ 'foo', 'bar', 'baz', 'FOO', 'BAR', 'true', ' ' ].forEach(function(v) {
        var actual = getQueryResults("RETURN FIND_LAST('', " + JSON.stringify(v) + ")");
        assertEqual([ -1 ], actual);
      });

      var actual = getQueryResults("RETURN FIND_LAST('', '')");
      assertEqual([ 0 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test find_last function
////////////////////////////////////////////////////////////////////////////////

    testFindLastEmpty2 : function () {
      [ 'foo', 'bar', 'baz', 'FOO', 'BAR', 'true', ' ', '' ].forEach(function(v) {
        var actual = getQueryResults("RETURN FIND_LAST(" + JSON.stringify(v) + ", '')");
        assertEqual([ v.length ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test find_last function
////////////////////////////////////////////////////////////////////////////////

    testFindLast : function () {
      [ 
        [ -1, 'foo', 'bar' ],
        [ 3, 'foobar', 'bar' ],
        [ 3, 'Foobar', 'bar' ],
        [ -1, 'foobar', 'Bar' ],
        [ 16, 'the quick brown bar jumped over the lazy dog', 'bar' ],
        [ 3, 'FOOBAR', 'BAR' ],
        [ -1, 'FOOBAR', 'bar' ],
        [ -1, 'the quick brown foxx', 'the foxx' ],
        [ 0, 'the quick brown foxx', 'the quick' ],
        [ -1, 'the quick brown foxx', 'the quick brown foxx j' ],
        [ 4, 'the quick brown foxx', 'quick brown' ],
        [ 35, 'the quick brown foxx jumped over a\nnewline', 'newline' ],
        [ 14, 'some linebreak\r\ngoes here', '\r\n' ]
      ].forEach(function(v) {
        var actual = getQueryResults("RETURN FIND_LAST(" + JSON.stringify(v[1]) + ", " + JSON.stringify(v[2]) + ")");
        assertEqual([ v[0] ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test find_last function
////////////////////////////////////////////////////////////////////////////////

    testFindLastStartEnd : function () {
      [ 
        [ 3, 'foobar', 'bar', 0 ],
        [ 3, 'foobar', 'bar', 1 ],
        [ 3, 'foobar', 'bar', 2 ],
        [ 3, 'foobar', 'bar', 3 ],
        [ -1, 'foobar', 'bar', 4 ],
        [ 3, 'foobar', 'bar', 1, 5 ],
        [ 3, 'foobar', 'bar', 2, 5 ],
        [ 3, 'foobar', 'bar', 3, 5 ],
        [ -1, 'foobar', 'bar', 4, 6 ],
        [ -1, 'foobar', 'bar', 4, 5 ],
        [ -1, 'foobar', 'bar', 1, 4 ],
        [ -1, 'foobar', 'bar', 0, 4 ],
        [ 3, 'foobar', 'bar', 0, 5 ],
        [ 3, 'foobar', 'bar', 0, 999 ],
        [ 32, 'the quick brown bar jumped over the lazy dog', 'the', 0 ],
        [ 32, 'the quick brown bar jumped over the lazy dog', 'the', 10 ],
        [ 32, 'the quick brown bar jumped over the lazy dog', 'the', 1 ]
      ].forEach(function(v) {
        var actual = getQueryResults("RETURN FIND_LAST(" + JSON.stringify(v[1]) + ", " + JSON.stringify(v[2]) + ", " + v[3] + ", " + (v[4] === undefined ? null : v[4]) + ")");
        assertEqual([ v[0] ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test find_last function
////////////////////////////////////////////////////////////////////////////////

    testFindLastInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FIND_LAST()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FIND_LAST('foo')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN FIND_LAST('foo', 'bar', 2, 2, 2)"); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST(null, 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST(true, 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST(4, 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST([ ], 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST({ }, 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', null)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', true)")); 
      assertEqual([ 3 ], getQueryResults("RETURN FIND_LAST('foo', [ ])")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', { })")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', -1)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', -1.5)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', 3)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', 'bar', 'baz')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', 'bar', 1, 'bar')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', 'bar', -1)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', 'bar', 1, -1)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', 'bar', 1, 0)")); 
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
      assertEqual([ "yestrue" ], getQueryResults("RETURN CONCAT(\"yes\", true)")); 
      assertEqual([ "yes4" ], getQueryResults("RETURN CONCAT(\"yes\", 4)")); 
      assertEqual([ "yes" ], getQueryResults("RETURN CONCAT(\"yes\", [ ])")); 
      assertEqual([ "yes[object Object]" ], getQueryResults("RETURN CONCAT(\"yes\", { })")); 
      assertEqual([ "trueyes" ], getQueryResults("RETURN CONCAT(true, \"yes\")")); 
      assertEqual([ "4yes" ], getQueryResults("RETURN CONCAT(4, \"yes\")")); 
      assertEqual([ "yes" ], getQueryResults("RETURN CONCAT([ ], \"yes\")")); 
      assertEqual([ "[object Object]yes" ], getQueryResults("RETURN CONCAT({ }, \"yes\")")); 
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
      assertEqual([ "yesnullyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(null, \"yes\", \"yes\")"));
      assertEqual([ "yestrueyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(true, \"yes\", \"yes\")"));
      assertEqual([ "yes4yes" ], getQueryResults("RETURN CONCAT_SEPARATOR(4, \"yes\", \"yes\")"));
      assertEqual([ "yesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR([ ], \"yes\", \"yes\")"));
      assertEqual([ "yes[object Object]yes" ], getQueryResults("RETURN CONCAT_SEPARATOR({ }, \"yes\", \"yes\")"));
      assertEqual([ "trueyesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", true, \"yes\")"));
      assertEqual([ "4yesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", 4, \"yes\")"));
      assertEqual([ "yesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", [ ], \"yes\")"));
      assertEqual([ "[object Object]yesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", { }, \"yes\")"));
      assertEqual([ "yesyestrue" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", true)"));
      assertEqual([ "yesyes4" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", 4)"));
      assertEqual([ "yesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", [ ])"));
      assertEqual([ "yesyes[object Object]" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", { })"));
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
      assertEqual([ 4 ], getQueryResults("RETURN CHAR_LENGTH(null)"));
      assertEqual([ 4 ], getQueryResults("RETURN CHAR_LENGTH(true)"));
      assertEqual([ 1 ], getQueryResults("RETURN CHAR_LENGTH(3)"));
      assertEqual([ 0 ], getQueryResults("RETURN CHAR_LENGTH([ ])"));
      assertEqual([ 15 ], getQueryResults("RETURN CHAR_LENGTH({ })"));
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
      assertEqual([ "null" ], getQueryResults("RETURN LOWER(null)"));
      assertEqual([ "true" ], getQueryResults("RETURN LOWER(true)"));
      assertEqual([ "3" ], getQueryResults("RETURN LOWER(3)"));
      assertEqual([ "" ], getQueryResults("RETURN LOWER([])"));
      assertEqual([ "[object object]" ], getQueryResults("RETURN LOWER({})"));
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
      assertEqual([ "NULL" ], getQueryResults("RETURN UPPER(null)"));
      assertEqual([ "TRUE" ], getQueryResults("RETURN UPPER(true)"));
      assertEqual([ "3" ], getQueryResults("RETURN UPPER(3)"));
      assertEqual([ "" ], getQueryResults("RETURN UPPER([])"));
      assertEqual([ "[OBJECT OBJECT]" ], getQueryResults("RETURN UPPER({})"));
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
      assertEqual([ "null" ], getQueryResults("RETURN SUBSTRING(null, 0)"));
      assertEqual([ "true" ], getQueryResults("RETURN SUBSTRING(true, 0)"));
      assertEqual([ "3" ], getQueryResults("RETURN SUBSTRING(3, 0)"));
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING([ ], 0)"));
      assertEqual([ "[object Object]" ], getQueryResults("RETURN SUBSTRING({ }, 0)"));
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING(\"yes\", null, 0)"));
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING(\"yes\", true, 0)"));
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING(\"yes\", \"yes\", 0)"));
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING(\"yes\", [ ], 0)"));
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING(\"yes\", { }, 0)"));
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING(\"yes\", \"yes\", null)"));
      assertEqual([ "y" ], getQueryResults("RETURN SUBSTRING(\"yes\", \"yes\", true)"));
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING(\"yes\", \"yes\", [ ])"));
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING(\"yes\", \"yes\", { })"));
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlStringFunctionsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
