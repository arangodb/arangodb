/*jshint globalstrict:false, strict:false, maxlen:5000 */
/*global assertEqual, assertNotEqual, assertTrue */

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
var assertQueryWarningAndFalse = helper.assertQueryWarningAndFalse;

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
/// @brief test JSON_STRINGIFY
////////////////////////////////////////////////////////////////////////////////

    testJsonStringify : function () {
      var buildQuery = function(nr, input) {
        switch (nr) {
          case 0:
            return `RETURN JSON_STRINGIFY(${input})`;
          case 1:
            return `RETURN NOOPT(JSON_STRINGIFY(${input}))`;
          case 2:
            return `RETURN NOOPT(V8(JSON_STRINGIFY(${input})))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 3; ++i) {
        assertEqual([ "null" ], getQueryResults(buildQuery(i, "null")));
        assertEqual([ "false" ], getQueryResults(buildQuery(i, "false")));
        assertEqual([ "true" ], getQueryResults(buildQuery(i, "true")));
        assertEqual([ "-19.3" ], getQueryResults(buildQuery(i, "-19.3")));
        assertEqual([ "0" ], getQueryResults(buildQuery(i, "0")));
        assertEqual([ "0" ], getQueryResults(buildQuery(i, "0.0")));
        assertEqual([ "0.1" ], getQueryResults(buildQuery(i, "0.1")));
        assertEqual([ "100" ], getQueryResults(buildQuery(i, "100")));
        assertEqual([ "10001434.2" ], getQueryResults(buildQuery(i, "10001434.2")));
        assertEqual([ "\"foo bar\"" ], getQueryResults(buildQuery(i, "\"foo bar\"")));
        assertEqual([ "\"foo \\\" bar\"" ], getQueryResults(buildQuery(i, "\"foo \\\" bar\"")));
        assertEqual([ "[]" ], getQueryResults(buildQuery(i, "[]")));
        assertEqual([ "[1,2,3,4]" ], getQueryResults(buildQuery(i, "[ 1, 2, 3, 4 ]")));
        assertEqual([ "[[1,2,3,4]]" ], getQueryResults(buildQuery(i, "[ [ 1, 2, 3, 4  ] ]")));
        assertEqual([ "{}" ], getQueryResults(buildQuery(i, "{ }")));
        assertEqual([ "{\"a\":1,\"b\":2}" ], getQueryResults(buildQuery(i, "{ a: 1, b : 2     }")));
        assertEqual([ "{\"A\":2,\"a\":1}" ], getQueryResults(buildQuery(i, "{ A: 2, a: 1 }")));
        assertEqual([ "{\"a\":1,\"b\":\"foo\",\"c\":null}" ], getQueryResults(buildQuery(i, "{ \"a\": 1, \"b\": \"foo\", c: null }")));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test JSON_PARSE
////////////////////////////////////////////////////////////////////////////////

    testJsonParse : function () {
      var buildQuery = function(nr, input) {
        switch (nr) {
          case 0:
            return `RETURN JSON_PARSE(${input})`;
          case 1:
            return `RETURN NOOPT(JSON_PARSE(${input}))`;
          case 2:
            return `RETURN NOOPT(V8(JSON_PARSE(${input})))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 3; ++i) {
        // invalid JSON
        assertEqual([ null ], getQueryResults(buildQuery(i, "null"))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "false"))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "true"))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "1234"))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"\""))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "\" \""))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"abcd\""))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "[]"))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "{}"))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"[\""))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"[]]\""))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"{\""))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"{}}\""))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"{a}\""))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"{\\\"a\\\"}\""))); 
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"{a:1}\""))); 

        // valid JSON
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"null\"")));
        assertEqual([ false ], getQueryResults(buildQuery(i, "\"false\"")));
        assertEqual([ true ], getQueryResults(buildQuery(i, "\"true\"")));
        assertEqual([ -19.3 ], getQueryResults(buildQuery(i, "\"-19.3\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"0\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"0.0\"")));
        assertEqual([ 0.1 ], getQueryResults(buildQuery(i, "\"0.1\"")));
        assertEqual([ 100 ], getQueryResults(buildQuery(i, "\"100\"")));
        assertEqual([ 10001434.2 ], getQueryResults(buildQuery(i, "\"10001434.2\"")));
        assertEqual([ "foo bar" ], getQueryResults(buildQuery(i, "\"\\\"foo bar\\\"\"")));
        assertEqual([ [] ], getQueryResults(buildQuery(i, "\"[]\"")));
        assertEqual([ [1,2,3,4] ], getQueryResults(buildQuery(i, "\"[ 1, 2, 3, 4 ]\"")));
        assertEqual([ [[1,2,3,4]] ], getQueryResults(buildQuery(i, "\"[ [ 1, 2, 3, 4  ] ]\"")));
        assertEqual([ {} ], getQueryResults(buildQuery(i, "\"{ }\"")));
        assertEqual([ { a: 1, b: 2 } ], getQueryResults(buildQuery(i, "\"{ \\\"a\\\": 1, \\\"b\\\" : 2     }\"")));
        assertEqual([ { A: 2, a: 1 } ], getQueryResults(buildQuery(i, "\"{ \\\"A\\\": 2, \\\"a\\\": 1 }\"")));
        assertEqual([ { a: 1, b: "foo", c: null } ], getQueryResults(buildQuery(i, "\"{ \\\"a\\\": 1, \\\"b\\\": \\\"foo\\\", \\\"c\\\": null }\"")));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test regex function, invalid arguments
////////////////////////////////////////////////////////////////////////////////
    
    testRegexInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REGEX_TEST()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REGEX_TEST(\"test\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REGEX_TEST(\"test\", \"meow\", \"foo\", \"bar\")"); 
      
      assertQueryWarningAndFalse(errors.ERROR_QUERY_INVALID_REGEX.code, "RETURN REGEX_TEST(\"test\", \"[\")");
      assertQueryWarningAndFalse(errors.ERROR_QUERY_INVALID_REGEX.code, "RETURN REGEX_TEST(\"test\", \"[^\")");
      assertQueryWarningAndFalse(errors.ERROR_QUERY_INVALID_REGEX.code, "RETURN REGEX_TEST(\"test\", \"a.(\")");
      assertQueryWarningAndFalse(errors.ERROR_QUERY_INVALID_REGEX.code, "RETURN REGEX_TEST(\"test\", \"(a\")");
      assertQueryWarningAndFalse(errors.ERROR_QUERY_INVALID_REGEX.code, "RETURN REGEX_TEST(\"test\", \"(a]\")");
      assertQueryWarningAndFalse(errors.ERROR_QUERY_INVALID_REGEX.code, "RETURN REGEX_TEST(\"test\", \"**\")");
      assertQueryWarningAndFalse(errors.ERROR_QUERY_INVALID_REGEX.code, "RETURN REGEX_TEST(\"test\", \"?\")");
      assertQueryWarningAndFalse(errors.ERROR_QUERY_INVALID_REGEX.code, "RETURN REGEX_TEST(\"test\", \"*\")");
    },

    testRegex : function () {
      var values = [
        // empty values
        ["", "", true],
        ["", "^", true],
        ["", "$", true],
        ["", "^$", true],
        ["", "^.*$", true],
        ["", "^.+$", false],
        ["", ".", false],
        ["", ".?", true],
        [" ", ".?", true],
        [" ", ".+", true],
        [" ", ".$", true],
        [" ", "^.", true],
        [" ", ".$", true],
        [" ", "^.$", true],
        [" ", "^.{1,}$", true],
        [" ", "^.{2,}$", false],

        // whole words
        ["the quick brown fox", "the", true],
        ["the quick brown fox", "quick", true],
        ["the quick brown fox", "quicK", false],
        ["the quick brown fox", "quIcK", false],
        ["the quick brown fox", "brown", true],
        ["the quick brown fox", "fox", true],
        ["the quick brown fox", "The", false],
        ["the quick brown fox", "THE", false],
        ["the quick brown fox", "foxx", false],
        ["the quick brown fox", "hasi", false],
        ["the quick brown fox", "number", false],
        
        // anchored
        ["the quick brown fox", "^the", true],
        ["the quick brown fox", "^the$", false],
        ["the quick brown fox", "^the quick", true],
        ["the quick brown fox", "^the quick brown", true],
        ["the quick brown fox", "^the quick brown fo", true],
        ["the quick brown fox", "^th", true],
        ["the quick brown fox", "^t", true],
        ["the quick brown fox", "^the quick$", false],
        ["the quick brown fox", "^quick", false],
        ["the quick brown fox", "quick$", false],
        ["the quick brown fox", "^quick$", false],
        ["the quick brown fox", "^brown", false],
        ["the quick brown fox", "brown$", false],
        ["the quick brown fox", "^brown$", false],
        ["the quick brown fox", "fox", true],
        ["the quick brown fox", "fox$", true],
        ["the quick brown fox", "^fox$", false],
        ["the quick brown fox", "The", false],
        ["the quick brown fox", "^The", false],
        ["the quick brown fox", "THE", false],
        ["the quick brown fox", "^THE", false],
        ["the quick brown fox", "foxx", false],
        ["the quick brown fox", "foxx$", false],
        ["the quick brown fox", "the quick brown fox$", true],
        ["the quick brown fox", "brown fox$", true],
        ["the quick brown fox", "quick brown fox$", true],
        ["the quick brown fox", "he quick brown fox$", true],
        ["the quick brown fox", "e quick brown fox$", true],
        ["the quick brown fox", "quick brown fox$", true],
        ["the quick brown fox", "x$", true],
        ["the quick brown fox", "^", true],
        ["the quick brown fox", "$", true],
        ["the quick brown fox", "^.*$", true],
        ["the quick brown fox", ".*", true],
        ["the quick brown fox", "^.*", true],
        ["the quick brown fox", "^.*$", true],

        // partials
        ["the quick brown fox", " quick", true],
        ["the quick brown fox", " Quick", false],
        ["the quick brown fox", "the quick", true],
        ["the quick brown fox", "the slow", false],
        ["the quick brown fox", "the quick brown", true],
        ["the quick brown fox", "the quick browne", false],
        ["the quick brown fox", "the quick brownfox", false],
        ["the quick brown fox", "the quick brown fox", true],
        ["the quick brown fox", "the quick brown foxx", false],
        ["the quick brown fox", "quick brown fox", true],
        ["the quick brown fox", "a quick brown fox", false],
        ["the quick brown fox", "brown fox", true],
        ["the quick brown fox", "rown fox", true],
        ["the quick brown fox", "rown f", true],
        ["the quick brown fox", "e q", true],
        ["the quick brown fox", "f z", false],
        ["the quick brown fox", "red fo", false],
        ["the quick brown fox", "köter", false],
        ["the quick brown fox", "ö", false],
        ["the quick brown fox", "z", false],
        ["the quick brown fox", "z", false],
        ["the quick brown fox", " ", true],
        ["the quick brown fox", "  ", false],
        ["the quick brown fox", "", true],
        ["the quick brown fox", "number one", false],
        ["the quick brown fox", "123", false],

        // wildcards
        ["the quick brown fox", "the.*fox", true],
        ["the quick brown fox", "^the.*fox$", true],
        ["the quick brown fox", "^the.*dog$", false],
        ["the quick brown fox", "the (quick|slow) (red|green|brown) (dog|cat|fox)", true],
        ["the quick brown fox", "the .*(red|green|brown) (dog|cat|fox)", true],
        ["the quick brown fox", "^the .*(red|green|brown) (dog|cat|fox)", true],
        ["the quick brown fox", "the (quick|slow) (red|green|brown) (dog|cat)", false],
        ["the quick brown fox", "^the (quick|slow) (red|green|brown) (dog|cat)", false],
        ["the quick brown fox", "^the .*(red|green|brown) (dog|cat)", false],
        ["the quick brown fox", "the .*(red|green|brown) (dog|cat)", false],
        ["the quick brown fox", "the (slow|lazy) brown (fox|wolf)", false],
        ["the quick brown fox", "the.*brown (fox|wolf)", true],
        ["the quick brown fox", "^t.*(fox|wolf)", true],
        ["the quick brown fox", "^t.*(fox|wolf)$", true],
        ["the quick brown fox", "^t.*(fo|wolf)x$", true],
        ["the quick brown fox", "^t.*(fo|wolf)xx", false],
        
        // line breaks
        ["the quick\nbrown\nfox", "the.*fox", false],
        ["the quick\nbrown\nfox", "the(.|\n)*fox", true],
        ["the quick\nbrown\nfox", "^the.*fox$", false],
        ["the quick\nbrown\nfox", "^the(.|\n)*fox$", true],
        ["the quick\nbrown\nfox", "the.*fox$", false],
        ["the quick\nbrown\nfox", "the(.|\n)*fox$", true],
        ["the quick\nbrown\nfox", "^the.*fox$", false],
        ["the quick\nbrown\nfox", "^the(.|\n)*fox$", true],
        ["the quick\nbrown\nfox", "^the.*dog$", false],
        ["the quick\nbrown\nfox", "^the(.|\n)*dog$", false],
        ["the quick\nbrown\nfox", "brown fox", false],
        ["the quick\nbrown\nfox", "brown.fox", false],
        ["the quick\nbrown\nfox", "brown(.|\n)fox", true],
        ["the quick\nbrown\nfox", "brown\\nfox", true],
        ["the quick\nbrown\nfox", "quick.brown", false],
        ["the quick\nbrown\nfox", "quick(.|\n)brown", true],
        ["the quick\r\nbrown\nfox", "quick.brown", false],
        ["the quick\r\nbrown\nfox", "quick(.|\r\n)brown", true],
        ["the quick\r\nbrown\nfox", "quick\\r\\nbrown", true],
        ["the quick\r\nbrown\nfox", "quick\\r\\red", false]
      ];

      values.forEach(function(v) {
        var query = "RETURN REGEX_TEST(@what, @re)";
        assertEqual(v[2], getQueryResults(query, { what: v[0], re: v[1] })[0], v);
        
        query = "RETURN NOOPT(REGEX_TEST(@what, @re))";
        assertEqual(v[2], getQueryResults(query, { what: v[0], re: v[1] })[0], v);
        
        query = "RETURN NOOPT(V8(REGEX_TEST(@what, @re)))";
        assertEqual(v[2], getQueryResults(query, { what: v[0], re: v[1] })[0], v);
        
        query = "RETURN @what =~ @re";
        assertEqual(v[2], getQueryResults(query, { what: v[0], re: v[1] })[0], v);
        
        query = "RETURN @what !~ @re";
        assertEqual(!v[2], getQueryResults(query, { what: v[0], re: v[1] })[0], v);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test regex function, cache
////////////////////////////////////////////////////////////////////////////////
    
    testRegexCache : function () {
      var actual = getQueryResults("FOR i IN 1..100 RETURN REGEX_TEST(CONCAT('test', i), 'test')");
      assertEqual(100, actual.length);
      for (var i = 0; i < actual.length; ++i) {
        assertTrue(actual[i]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test REGEX_REPLACE, invalid arguments
////////////////////////////////////////////////////////////////////////////////

    testRegexReplaceInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REGEX_REPLACE()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REGEX_REPLACE(\"test\")");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REGEX_TEST(\"test\", \"meow\", \"woof\", \"foo\", \"bar\")");

      assertQueryWarningAndFalse(errors.ERROR_QUERY_INVALID_REGEX.code, "RETURN REGEX_REPLACE(\"test\", \"?\", \"hello\")");
      assertQueryWarningAndFalse(errors.ERROR_QUERY_INVALID_REGEX.code, "RETURN REGEX_REPLACE(\"test\", \"*\", \"hello\")");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test REGEX_REPLACE
////////////////////////////////////////////////////////////////////////////////

    testRegexReplace : function () {
      var values = [
        // whole words
        [["the quick brown fox", "the", "A"], "A quick brown fox"],
        [["the quick brown fox", "quick", "slow"], "the slow brown fox"],
        [["the quick brown fox", "quicK", "slow"], "the quick brown fox"],
        [["the quick brown fox", "brown ", "white"], "the quick whitefox"],
        [["the quick brown fox", "fox", "dog"], "the quick brown dog"],
        [["the quick brown fox", "hasi", "fasi"], "the quick brown fox"],

        // anchored
        [["the quick brown fox", "^the", "A"], "A quick brown fox"],
        [["the quick brown fox", "^the$", "A"], "the quick brown fox"],
        [["the quick brown fox", "^the quick brown fo", "fa"], "fax"],

        // // partials
        [["the quick brown fox", "ö", "i"], "the quick brown fox"],
        [["the quick brown fox", "o", "i"], "the quick briwn fix"],
        [["the quick brown fox", " ", "-"], "the-quick-brown-fox"],
        [["the quick brown fox", "  ", "-"], "the quick brown fox"],
        [["the quick brown fox", " ", ""], "thequickbrownfox"],
        [["the quick brown fox", "", "x"], "xtxhxex xqxuxixcxkx xbxrxoxwxnx xfxoxxx"],

        // // wildcards
        [["the quick brown fox", "the.*fox", "jumped over"], "jumped over"],
        [["the quick brown fox", "^the.*fox$", "jumped over"], "jumped over"],
        [["the quick brown fox", "^the.*dog$", "jumped over"], "the quick brown fox"],
        [["the quick brown fox", "the (quick|slow) (red|green|brown) (dog|cat|fox)", "jumped over"], "jumped over"],
        [["the quick brown fox", "the .*(red|green|brown) (dog|cat|fox)", "jumped over"], "jumped over"],
        [["the quick brown fox", "the (slow|lazy) brown (fox|wolf)", "jumped over"], "the quick brown fox"],
        [["the quick brown fox", "the (slow|quick) brown (fox|wolf)", "jumped over"], "jumped over"],
        
        // // line breaks
        [["the quick\nbrown\nfox", "the(.|\n)*fox", "jumped over"], "jumped over"],
        [["the quick\nbrown\nfox", "^the.*fox$", "jumped over"], "the quick\nbrown\nfox"],
      ];

      values.forEach(function(v) {
        var query;
        query = "RETURN REGEX_REPLACE(@what, @re, @with)";
        assertEqual(v[1], getQueryResults(query, { what: v[0][0], re: v[0][1], with: v[0][2] })[0], v);

        query = "RETURN NOOPT(REGEX_REPLACE(@what, @re, @with))";
        assertEqual(v[1], getQueryResults(query, { what: v[0][0], re: v[0][1], with: v[0][2] })[0], v);

        query = "RETURN NOOPT(V8(REGEX_REPLACE(@what, @re, @with)))";
        assertEqual(v[1], getQueryResults(query, { what: v[0][0], re: v[0][1], with: v[0][2] })[0], v);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test like function, invalid arguments
////////////////////////////////////////////////////////////////////////////////
    
    testLikeInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "RETURN LIKE"); 
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "RETURN \"test\" LIKE"); 
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "RETURN LIKE \"test\""); 
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "RETURN \"test\" LIKE \"meow\", \"foo\", \"bar\")"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LIKE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LIKE(\"test\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LIKE(\"test\", \"meow\", \"foo\", \"bar\")"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test like function
////////////////////////////////////////////////////////////////////////////////
    
    testLike : function () {
      // containment
      assertEqual([ false ], getQueryResults("RETURN \"this is a test string\" LIKE \"test\""));
      assertEqual([ false ], getQueryResults("RETURN \"this is a test string\" LIKE \"%test\""));
      assertEqual([ false ], getQueryResults("RETURN \"this is a test string\" LIKE \"test%\""));
      assertEqual([ true ], getQueryResults("RETURN \"this is a test string\" LIKE \"%test%\""));
      assertEqual([ true ], getQueryResults("RETURN \"this is a test string\" LIKE \"this%test%\""));
      assertEqual([ true ], getQueryResults("RETURN \"this is a test string\" LIKE \"this%is%test%\""));
      assertEqual([ true ], getQueryResults("RETURN \"this is a test string\" LIKE \"this%g\""));
      assertEqual([ false ], getQueryResults("RETURN \"this is a test string\" LIKE \"this%n\""));
      assertEqual([ false ], getQueryResults("RETURN \"this is a test string\" LIKE \"This%n\""));
      assertEqual([ false ], getQueryResults("RETURN \"this is a test string\" LIKE \"his%\""));
      assertEqual([ true ], getQueryResults("RETURN \"this is a test string\" LIKE \"%g\""));
      assertEqual([ false ], getQueryResults("RETURN \"this is a test string\" LIKE \"%G\""));
      assertEqual([ false ], getQueryResults("RETURN \"this is a test string\" LIKE \"this%test%is%\""));

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
      
      // special characters
      assertEqual([ true ], getQueryResults("RETURN \"%\" LIKE \"\\%\""));
      assertEqual([ true ], getQueryResults("RETURN \"a%c\" LIKE \"a%c\""));
      assertEqual([ false ], getQueryResults("RETURN \"a%c\" LIKE \"ac\""));
      assertEqual([ false ], getQueryResults("RETURN \"a%c\" LIKE \"a\\\\%\""));
      assertEqual([ false ], getQueryResults("RETURN \"a%c\" LIKE \"\\\\%a%\""));
      assertEqual([ false ], getQueryResults("RETURN \"a%c\" LIKE \"\\\\%\\\\%\""));
      assertEqual([ true ], getQueryResults("RETURN \"%%\" LIKE \"\\\\%\\\\%\""));
      assertEqual([ true ], getQueryResults("RETURN \"_\" LIKE \"\\\\_\""));
      assertEqual([ true ], getQueryResults("RETURN \"_\" LIKE \"\\\\_%\""));
      assertEqual([ true ], getQueryResults("RETURN \"abcd\" LIKE \"_bcd\""));
      assertEqual([ true ], getQueryResults("RETURN \"abcde\" LIKE \"_bcd%\""));
      assertEqual([ false ], getQueryResults("RETURN \"abcde\" LIKE \"\\\\_bcd%\""));
      assertEqual([ true ], getQueryResults("RETURN \"\\\\abc\" LIKE \"\\\\\\\\%\""));
      assertEqual([ true ], getQueryResults("RETURN \"\\abc\" LIKE \"\\a%\""));
      assertEqual([ true ], getQueryResults("RETURN \"[ ] ( ) % * . + -\" LIKE \"[%\""));
      assertEqual([ true ], getQueryResults("RETURN \"[ ] ( ) % * . + -\" LIKE \"[ ] ( ) \\% * . + -\""));
      assertEqual([ true ], getQueryResults("RETURN \"[ ] ( ) % * . + -\" LIKE \"%. +%\""));
      assertEqual([ true ], getQueryResults("RETURN \"abc^def$g\" LIKE \"abc^def$g\""));
      assertEqual([ true ], getQueryResults("RETURN \"abc^def$g\" LIKE \"%^%$g\""));
    
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
     
      // case-sensivity 
      assertEqual([ false ], getQueryResults("RETURN \"ABCD\" LIKE \"abcd\""));
      assertEqual([ false ], getQueryResults("RETURN \"abcd\" LIKE \"ABCD\""));
      assertEqual([ false ], getQueryResults("RETURN \"MÖterTräNenMÜtterSöhne\" LIKE \"MÖTERTRÄNENMÜTTERSÖHNE\""));
      
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
        
        actual = getQueryResults("RETURN " + JSON.stringify(value) + " LIKE " + JSON.stringify(value));
        assertEqual([ true ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test like function, cache
////////////////////////////////////////////////////////////////////////////////
    
    testLikeCache : function () {
      var actual = getQueryResults("FOR i IN 1..100 RETURN LIKE(CONCAT('test', i), 'test%')");
      assertEqual(100, actual.length);
      for (var i = 0; i < actual.length; ++i) {
        assertTrue(actual[i]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test like function, invalid arguments
////////////////////////////////////////////////////////////////////////////////
    
    testLikeInvalidCxx : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(LIKE())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(LIKE(\"test\"))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(LIKE(\"test\", \"meow\", \"foo\", \"bar\"))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test like function
////////////////////////////////////////////////////////////////////////////////
    
    testLikeCxx : function () {
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"test\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"%test\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"test%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"%test%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"this%test%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"this%is%test%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"this%g\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"this%n\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"This%n\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"his%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"%g\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"%G\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"this is a test string\", \"this%test%is%\"))"));
    
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"%\", \"\\%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"a%c\", \"a%c\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"a%c\", \"ac\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"a%c\", \"a\\\\%\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"a%c\", \"\\\\%a%\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"a%c\", \"\\\\%\\\\%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"%%\", \"\\\\%\\\\%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"_\", \"\\\\_\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"_\", \"\\\\_%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"abcd\", \"_bcd\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"abcde\", \"_bcd%\"))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"abcde\", \"\\\\_bcd%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"\\\\abc\", \"\\\\\\\\%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"\\abc\", \"\\a%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"[ ] ( ) % * . + -\", \"[%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"[ ] ( ) % * . + -\", \"[ ] ( ) \\% * . + -\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"[ ] ( ) % * . + -\", \"%. +%\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"abc^def$g\", \"abc^def$g\"))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"abc^def$g\", \"%^%$g\"))"));
      
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"ABCD\", \"abcd\", false))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"ABCD\", \"abcd\", true))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(LIKE(\"abcd\", \"ABCD\", false))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"abcd\", \"ABCD\", true))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"MÖterTräNenMÜtterSöhne\", \"MöterTräNenMütterSöhne\", true))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"MÖterTräNenMÜtterSöhne\", \"mötertränenmüttersöhne\", true))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(LIKE(\"MÖterTräNenMÜtterSöhne\", \"MÖTERTRÄNENMÜTTERSÖHNE\", true))"));
      
      assertEqual([ [ true, false, true, false ] ], getQueryResults("RETURN [ NOOPT(LIKE(\"Möter\", \"m_ter\", true)), NOOPT(LIKE(\"Möter\", \"m_ter\", false)), NOOPT(LIKE(\"Möter\", \"m_ter\", true)), NOOPT(LIKE(\"Möter\", \"m_ter\", false)) ]"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test like with special characters
////////////////////////////////////////////////////////////////////////////////

    testLikeSpecialCharsCxx : function () {
      var data = [ 
        "the quick\nbrown fox jumped over\r\nthe lazy dog",
        "'the \"\\quick\\\n \"brown\\\rfox' jumped",
        '"the fox"" jumped \\over the \newline \roof"'
      ];
        
      data.forEach(function(value) {
        var actual = getQueryResults("RETURN NOOPT(LIKE(" + JSON.stringify(value) + ", 'foobar'))");
        assertEqual([ false ], actual);
        
        actual = getQueryResults("RETURN NOOPT(LIKE(" + JSON.stringify(value) + ", " + JSON.stringify(value) + "))");
        assertEqual([ true ], actual);
      });
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test first require function / expected datatype & arg. mismatch
////////////////////////////////////////////////////////////////////////////////
    
    testContainsFirst : function () {
      var buildQuery = function(nr, input) {
        switch (nr) {
          case 0:
            return `RETURN CONTAINS(${input})`;
          case 1:
            return `RETURN NOOPT(CONTAINS(${input}))`;
          case 2:
            return `RETURN NOOPT(V8(CONTAINS(${input})))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 3; ++i) {
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "\"test\"")); 
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "\"test\", \"test\", \"test\", \"test\"")); 
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "")); 
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "\"test\", \"test2\", \"test3\""))); 
        assertEqual([ false ], getQueryResults(buildQuery(i, "null, null"))); 
        assertEqual([ true ], getQueryResults(buildQuery(i, "4, 4"))); 
        assertEqual([ true ], getQueryResults(buildQuery(i, "{}, {}"))); 
        assertEqual([ true ], getQueryResults(buildQuery(i, "{a:1,b:2}, {a:1,b:2}"))); 
        assertEqual([ true ], getQueryResults(buildQuery(i, "[], []"))); 
        assertEqual([ true ], getQueryResults(buildQuery(i, "[1,2,3], [1,2,3]"))); 
        assertEqual([ true ], getQueryResults(buildQuery(i, "[1,2], [1,2]"))); 
        assertEqual([ true ], getQueryResults(buildQuery(i, "[1,2,3], 2"))); 
        assertEqual([ false ], getQueryResults(buildQuery(i, "null, \"yes\""))); 
        assertEqual([ false ], getQueryResults(buildQuery(i, "3, null"))); 
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains true1
////////////////////////////////////////////////////////////////////////////////

    testContainsTrue1 : function () {
      var expected = [true];  
      var actual = getQueryResults("RETURN CONTAINS(\"test2\", \"test\")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(CONTAINS(\"test2\", \"test\"))");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(V8(CONTAINS(\"test2\", \"test\")))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains true2
////////////////////////////////////////////////////////////////////////////////

    testContainsTrue2 : function () {
      var expected = [true];  
      var actual = getQueryResults("RETURN CONTAINS(\"xxasdxx\", \"asd\")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(CONTAINS(\"xxasdxx\", \"asd\"))");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(V8(CONTAINS(\"xxasdxx\", \"asd\")))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains false1
////////////////////////////////////////////////////////////////////////////////

    testContainsFalse1 : function () {
      var expected = [false];  
      var actual = getQueryResults("RETURN CONTAINS(\"test\", \"test2\")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(CONTAINS(\"test\", \"test2\"))");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(V8(CONTAINS(\"test\", \"test2\")))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains false2
////////////////////////////////////////////////////////////////////////////////

    testContainsFalse2 : function () {
      var expected = [false];  
      var actual = getQueryResults("RETURN CONTAINS(\"test123\", \"\")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(CONTAINS(\"test123\", \"\"))");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(V8(CONTAINS(\"test123\", \"\")))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contanis false3
////////////////////////////////////////////////////////////////////////////////

    testContainsFalse3 : function () {
      var expected = [false];  
      var actual = getQueryResults("RETURN CONTAINS(\"\", \"test123\")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(CONTAINS(\"\", \"test123\"))");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(V8(CONTAINS(\"\", \"test123\")))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test contains indexed
////////////////////////////////////////////////////////////////////////////////

    testContainsIndexed : function () {
      var buildQuery = function(nr, input) {
        switch (nr) {
          case 0:
            return `RETURN CONTAINS(${input})`;
          case 1:
            return `RETURN NOOPT(CONTAINS(${input}))`;
          case 2:
            return `RETURN NOOPT(V8(CONTAINS(${input})))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 3; ++i) {
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"test2\", \"test\", true")));
        assertEqual([ true ], getQueryResults(buildQuery(i, "\"test2\", \"test\", false")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "\"test2\", \"est\", true")));
        assertEqual([ true ], getQueryResults(buildQuery(i, "\"test2\", \"est\", false")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "\"this is a long test\", \"this is a test\", true")));
        assertEqual([ false ], getQueryResults(buildQuery(i, "\"this is a long test\", \"this is a test\", false")));
        assertEqual([ 18 ], getQueryResults(buildQuery(i, "\"this is a test of this test\", \"this test\", true")));
        assertEqual([ true ], getQueryResults(buildQuery(i, "\"this is a test of this test\", \"this test\", false")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "\"this is a test of this test\", \"This\", true")));
        assertEqual([ false ], getQueryResults(buildQuery(i, "\"this is a test of this test\", \"This\", false")));
      }
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

        actual = getQueryResults("RETURN NOOPT(CONTAINS(" + JSON.stringify(value) + ", 'foobar', false))");
        assertEqual([ false ], actual);
        
        actual = getQueryResults("RETURN NOOPT(CONTAINS(" + JSON.stringify(value) + ", " + JSON.stringify(value) + ", false))");
        assertEqual([ true ], actual);

        actual = getQueryResults("RETURN NOOPT(V8(CONTAINS(" + JSON.stringify(value) + ", 'foobar', false)))");
        assertEqual([ false ], actual);
        
        actual = getQueryResults("RETURN NOOPT(V8(CONTAINS(" + JSON.stringify(value) + ", " + JSON.stringify(value) + ", false)))");
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
      assertEqual([ "" ], getQueryResults("RETURN LEFT(null, 1)")); 
      assertEqual([ "" ], getQueryResults("RETURN LEFT(null, 2)")); 
      assertEqual([ "tr" ], getQueryResults("RETURN LEFT(true, 2)")); 
      assertEqual([ "4" ], getQueryResults("RETURN LEFT(4, 2)")); 
      assertEqual([ "[]" ], getQueryResults("RETURN LEFT([ ], 2)")); 
      assertEqual([ "[" ], getQueryResults("RETURN LEFT([ ], 1)")); 
      assertEqual([ "{}" ], getQueryResults("RETURN LEFT({ }, 2)")); 
      assertEqual([ "{" ], getQueryResults("RETURN LEFT({ }, 1)")); 
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
      assertEqual([ "" ], getQueryResults("RETURN RIGHT(null, 1)")); 
      assertEqual([ "" ], getQueryResults("RETURN RIGHT(null, 2)")); 
      assertEqual([ "ue" ], getQueryResults("RETURN RIGHT(true, 2)")); 
      assertEqual([ "4" ], getQueryResults("RETURN RIGHT(4, 2)")); 
      assertEqual([ "[]" ], getQueryResults("RETURN RIGHT([ ], 2)")); 
      assertEqual([ "]" ], getQueryResults("RETURN RIGHT([ ], 1)")); 
      assertEqual([ "{}" ], getQueryResults("RETURN RIGHT({ }, 2)")); 
      assertEqual([ "}" ], getQueryResults("RETURN RIGHT({ }, 1)")); 
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
        [ "the quick  foxx", "the quick brown foxx", "brown" ],
        [ "the quick brown foxx", "the quick brown foxx", [ ] ],
        [ "the quick  foxx", "the quick brown foxx", [ "brown" ], [ ] ]
      ];

      values.forEach(function(value) {
        var expected = value[0], args = [ ],i, n = value.length;
        for (i = 1; i < n; ++i) {
          args.push(JSON.stringify(value[i]));
        }
        assertEqual([ expected ], getQueryResults("RETURN SUBSTITUTE(" + args.join(", ") + ")"), value);
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

      assertEqual([ "" ], getQueryResults("RETURN TRIM(null)")); 
      assertEqual([ "true" ], getQueryResults("RETURN TRIM(true)")); 
      assertEqual([ "4" ], getQueryResults("RETURN TRIM(4)")); 
      assertEqual([ "[]" ], getQueryResults("RETURN TRIM([ ])")); 
      assertEqual([ "{}" ], getQueryResults("RETURN TRIM({ })")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', null)")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', true)")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', 'bar')")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', [ ])")); 
      assertEqual([ "foo" ], getQueryResults("RETURN TRIM('foo', { })"));
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
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST([], 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST([ ], 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST({}, 'foo')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST({ }, 'foo')")); 
      assertEqual([ 0 ], getQueryResults("RETURN FIND_FIRST('foo', null)")); 
      assertEqual([ 0 ], getQueryResults("RETURN FIND_FIRST('foo', '')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', true)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', [])")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_FIRST('foo', [1,2])")); 
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
      assertEqual([ 3 ], getQueryResults("RETURN FIND_LAST('foo', null)")); 
      assertEqual([ 3 ], getQueryResults("RETURN FIND_LAST('foo', '')")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', true)")); 
      assertEqual([ -1 ], getQueryResults("RETURN FIND_LAST('foo', [ ])")); 
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
    
    testConcatList1 : function () {
      var expected = [ "theQuickBrownアボカドJumps名称について" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT([ 'the', 'Quick', '', null, 'Brown', null, 'アボカド', 'Jumps', '名称について' ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatList2 : function () {
      var expected = [ "[\"the\",\"Quick\",\"\",null,\"Brown\",null,\"アボカド\",\"Jumps\",\"名称について\"]" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT([ 'the', 'Quick', '', null, 'Brown', null, 'アボカド', 'Jumps', '名称について' ], null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////

    testConcatInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONCAT()"); 
      assertEqual([ "yestrue" ], getQueryResults("RETURN CONCAT(\"yes\", true)")); 
      assertEqual([ "yes4" ], getQueryResults("RETURN CONCAT(\"yes\", 4)")); 
      assertEqual([ "yes[]" ], getQueryResults("RETURN CONCAT(\"yes\", [ ])")); 
      assertEqual([ "yes{}" ], getQueryResults("RETURN CONCAT(\"yes\", { })")); 
      assertEqual([ "trueyes" ], getQueryResults("RETURN CONCAT(true, \"yes\")")); 
      assertEqual([ "4yes" ], getQueryResults("RETURN CONCAT(4, \"yes\")")); 
      assertEqual([ "[]yes" ], getQueryResults("RETURN CONCAT([ ], \"yes\")")); 
      assertEqual([ "{}yes" ], getQueryResults("RETURN CONCAT({ }, \"yes\")")); 
      assertEqual([ "{\"a\":\"foo\",\"b\":2}yes" ], getQueryResults("RETURN CONCAT({ a: \"foo\", b: 2 }, \"yes\")")); 
      assertEqual([ "[1,\"Quick\"][\"Brown\"]falseFox" ], getQueryResults("RETURN CONCAT([ 1, 'Quick' ], '', null, [ 'Brown' ], false, 'Fox')")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatCxx1 : function () {
      var expected = [ "theQuickBrownFoxJumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return NOOPT(CONCAT('the', 'Quick', '', null, 'Brown', null, 'Fox', 'Jumps'))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatCxx2 : function () {
      var expected = [ "theQuickBrownアボカドJumps名称について" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return NOOPT(CONCAT('the', 'Quick', '', null, 'Brown', null, 'アボカド', 'Jumps', '名称について'))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatCxxList1 : function () {
      var expected = [ "theQuickBrownアボカドJumps名称について" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return NOOPT(CONCAT([ 'the', 'Quick', '', null, 'Brown', null, 'アボカド', 'Jumps', '名称について' ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatCxxList2 : function () {
      var expected = [ "[\"the\",\"Quick\",\"\",null,\"Brown\",null,\"アボカド\",\"Jumps\",\"名称について\"]false" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return NOOPT(CONCAT([ 'the', 'Quick', '', null, 'Brown', null, 'アボカド', 'Jumps', '名称について' ], null, false))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////

    testConcatCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(CONCAT())"); 
      assertEqual([ "yestrue" ], getQueryResults("RETURN NOOPT(CONCAT(\"yes\", true))")); 
      assertEqual([ "yes4" ], getQueryResults("RETURN NOOPT(CONCAT(\"yes\", 4))")); 
      assertEqual([ "yes[]" ], getQueryResults("RETURN NOOPT(CONCAT(\"yes\", [ ]))")); 
      assertEqual([ "yes{}" ], getQueryResults("RETURN NOOPT(CONCAT(\"yes\", { }))")); 
      assertEqual([ "trueyes" ], getQueryResults("RETURN NOOPT(CONCAT(true, \"yes\"))")); 
      assertEqual([ "4yes" ], getQueryResults("RETURN NOOPT(CONCAT(4, \"yes\"))")); 
      assertEqual([ "[]yes" ], getQueryResults("RETURN NOOPT(CONCAT([ ], \"yes\"))")); 
      assertEqual([ "[1,2,3]yes" ], getQueryResults("RETURN NOOPT(CONCAT([ 1,2,3], \"yes\"))")); 
      assertEqual([ "[1,2,3]yes" ], getQueryResults("RETURN NOOPT(CONCAT([ 1 , 2, 3 ], \"yes\"))")); 
      assertEqual([ "{}yes" ], getQueryResults("RETURN NOOPT(CONCAT({ }, \"yes\"))")); 
      assertEqual([ "{\"a\":\"foo\",\"b\":2}yes" ], getQueryResults("RETURN NOOPT(CONCAT({ a: \"foo\", b: 2 }, \"yes\"))")); 
      assertEqual([ "[1,\"Quick\"][\"Brown\"]falseFox" ], getQueryResults("RETURN NOOPT(CONCAT([ 1, 'Quick' ], '', null, [ 'Brown' ], false, 'Fox'))")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparator1 : function () {
      var expected = [ "the,Quick,Brown,Fox,Jumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT_SEPARATOR(',', 'the', 'Quick', null, 'Brown', null, 'Fox', 'Jumps')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparator2 : function () {
      var expected = [ "the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorList1 : function () {
      var expected = [ "[\"the\",\"Quick\",null,\"Brown\",null,\"Fox\",\"Jumps\"],higher,[\"than\",\"you\"]" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT_SEPARATOR(',', [ 'the', 'Quick', null, 'Brown', null, 'Fox', 'Jumps' ], 'higher', [ 'than', 'you' ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorList2 : function () {
      var expected = [ "[\"the\",\"Quick\",null,\"Brown\",\"\",\"Fox\",\"Jumps\"]*/*/[]*/*/higher*/*/[\"than\",\"you\"]" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', [ 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps' ], [ ], 'higher', [ 'than', 'you' ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorList3 : function () {
      var expected = [ "the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps*/*/[]*/*/higher*/*/[\"than\",\"you\"]" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps', [ ], 'higher', [ 'than', 'you' ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorList4 : function () {
      var expected = [ "the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', [ 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps' ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorList5 : function () {
      var expected = [ "the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps*/*/[]*/*/higher*/*/[\"than\",\"you\"]" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', [ 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps', [ ], 'higher', [ 'than', 'you' ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////

    testConcatSeparatorInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONCAT_SEPARATOR()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CONCAT_SEPARATOR(\"yes\")"); 
      assertEqual([ "yesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(null, \"yes\", \"yes\")"));
      assertEqual([ "yestrueyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(true, \"yes\", \"yes\")"));
      assertEqual([ "yes4yes" ], getQueryResults("RETURN CONCAT_SEPARATOR(4, \"yes\", \"yes\")"));
      assertEqual([ "yes[]yes" ], getQueryResults("RETURN CONCAT_SEPARATOR([ ], \"yes\", \"yes\")"));
      assertEqual([ "yes{}yes" ], getQueryResults("RETURN CONCAT_SEPARATOR({ }, \"yes\", \"yes\")"));
      assertEqual([ "trueyesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", true, \"yes\")"));
      assertEqual([ "4yesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", 4, \"yes\")"));
      assertEqual([ "[]yesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", [ ], \"yes\")"));
      assertEqual([ "{}yesyes" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", { }, \"yes\")"));
      assertEqual([ "yesyestrue" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", true)"));
      assertEqual([ "yesyes4" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", 4)"));
      assertEqual([ "yesyes[]" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", [ ])"));
      assertEqual([ "yesyes[1,2,3]" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", [ 1,2,3 ])"));
      assertEqual([ "yesyes{}" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", \"yes\", { })"));
      assertEqual([ "yesyes{}" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", [ \"yes\", { } ])"));
      assertEqual([ "[\"yes\",{}]yestrue" ], getQueryResults("RETURN CONCAT_SEPARATOR(\"yes\", [ \"yes\", { } ], null, true)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorCxx1 : function () {
      var expected = [ "the,Quick,Brown,Fox,Jumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] RETURN NOOPT(CONCAT_SEPARATOR(',', 'the', 'Quick', null, 'Brown', null, 'Fox', 'Jumps'))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorCxx2 : function () {
      var expected = [ "the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] RETURN NOOPT(CONCAT_SEPARATOR('*/*/', 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps'))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorListCxx1 : function () {
      var expected = [ "[\"the\",\"Quick\",null,\"Brown\",null,\"Fox\",\"Jumps\"],higher,[\"than\",\"you\"]" ];
      var actual = getQueryResults("FOR r IN [ 1 ] RETURN NOOPT(CONCAT_SEPARATOR(',', [ 'the', 'Quick', null, 'Brown', null, 'Fox', 'Jumps' ], 'higher', [ 'than', 'you' ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorListCxx2 : function () {
      var expected = [ "[\"the\",\"Quick\",null,\"Brown\",\"\",\"Fox\",\"Jumps\"]*/*/[]*/*/higher*/*/[\"than\",\"you\"]" ];
      var actual = getQueryResults("FOR r IN [ 1 ] RETURN NOOPT(CONCAT_SEPARATOR('*/*/', [ 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps' ], [ ], 'higher', [ 'than', 'you' ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorListCxx3 : function () {
      var expected = [ "the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps*/*/[]*/*/higher*/*/[\"than\",\"you\"]" ];
      var actual = getQueryResults("FOR r IN [ 1 ] RETURN NOOPT(CONCAT_SEPARATOR('*/*/', 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps', [ ], 'higher', [ 'than', 'you' ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorListCxx4 : function () {
      var expected = [ "the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] RETURN NOOPT(CONCAT_SEPARATOR('*/*/', [ 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps' ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////
    
    testConcatSeparatorListCxx5 : function () {
      var expected = [ "the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps*/*/[]*/*/higher*/*/[\"than\",\"you\"]" ];
      var actual = getQueryResults("FOR r IN [ 1 ] RETURN NOOPT(CONCAT_SEPARATOR('*/*/', [ 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps', [ ], 'higher', [ 'than', 'you' ] ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat_separator function
////////////////////////////////////////////////////////////////////////////////

    testConcatSeparatorCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(CONCAT_SEPARATOR())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(CONCAT_SEPARATOR(\"yes\"))"); 
      assertEqual([ "yesyes" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(null, \"yes\", \"yes\"))"));
      assertEqual([ "yestrueyes" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(true, \"yes\", \"yes\"))"));
      assertEqual([ "yes4yes" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(4, \"yes\", \"yes\"))"));
      assertEqual([ "yes[]yes" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR([ ], \"yes\", \"yes\"))"));
      assertEqual([ "yes{}yes" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR({ }, \"yes\", \"yes\"))"));
      assertEqual([ "trueyesyes" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", true, \"yes\"))"));
      assertEqual([ "4yesyes" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", 4, \"yes\"))"));
      assertEqual([ "[]yesyes" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", [ ], \"yes\"))"));
      assertEqual([ "{}yesyes" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", { }, \"yes\"))"));
      assertEqual([ "yesyestrue" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", \"yes\", true))"));
      assertEqual([ "yesyes4" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", \"yes\", 4))"));
      assertEqual([ "yesyes[]" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", \"yes\", [ ]))"));
      assertEqual([ "yesyes[1,2,3]" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", \"yes\", [ 1,2,3 ]))"));
      assertEqual([ "yesyes{}" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", \"yes\", { }))"));
      assertEqual([ "yesyes{}" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", [ \"yes\", { } ]))"));
      assertEqual([ "[\"yes\",{}]yestrue" ], getQueryResults("RETURN NOOPT(CONCAT_SEPARATOR(\"yes\", [ \"yes\", { } ], null, true))"));
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test charlength function
////////////////////////////////////////////////////////////////////////////////
    
    testCharLength1 : () => {
      assertEqual([ 7 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH('äöüÄÖÜß'))"));
      assertEqual([ 7 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH('äöüÄÖÜß')))"));

      assertEqual([ 10 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH('アボカド名称について'))"));
      assertEqual([ 10 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH('アボカド名称について')))"));

      assertEqual([ 13 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH('the quick fox'))"));
      assertEqual([ 13 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH('the quick fox')))"));

      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH(null))"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH(null)))"));

      assertEqual([ 4 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH(true))"));
      assertEqual([ 4 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH(true)))"));

      assertEqual([ 5 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH(false))"));
      assertEqual([ 5 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH(false)))"));

      assertEqual([ 1 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH(3))"));
      assertEqual([ 1 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH(3)))"));

      assertEqual([ 3 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH(3.3))"));
      assertEqual([ 3 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH(3.3)))"));

      assertEqual([ 4 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH('🥑😄👻👽'))"));
      assertEqual([ 4 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH('🥑😄👻👽')))"));

      assertEqual([ 2 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH([ ]))"));
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH([ ])))"));

      assertEqual([ 7 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH([ 1, 2, 3 ]))"));
      assertEqual([ 7 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH([ 1, 2, 3 ])))"));

      assertEqual([ 2 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH({ }))"));
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH({ })))"));

      assertEqual([ 7 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH({ a:1 }))"));
      assertEqual([ 7 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH({ a:1 })))"));

      assertEqual([ 11 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH({ a: \"foo\" }))"));
      assertEqual([ 11 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH({ a: \"foo\" })))"));

      assertEqual([ 13 ], getQueryResults("RETURN NOOPT(CHAR_LENGTH({ a:1, b: 2 }))"));
      assertEqual([ 13 ], getQueryResults("RETURN NOOPT(V8(CHAR_LENGTH({ a:1, b: 2 })))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test charlength function
////////////////////////////////////////////////////////////////////////////////

    testCharLengthInvalid : () => {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(CHAR_LENGTH())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(CHAR_LENGTH()))");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(CHAR_LENGTH('yes', 'yes'))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(CHAR_LENGTH('yes', 'yes')))");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower function
////////////////////////////////////////////////////////////////////////////////

    testLower1 : () => {
      assertEqual([ 'the quick brown fox jumped' ], getQueryResults("RETURN NOOPT(LOWER('THE quick Brown foX JuMpED'))"));
      assertEqual([ 'the quick brown fox jumped' ], getQueryResults("RETURN NOOPT(V8(LOWER('THE quick Brown foX JuMpED')))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower function
////////////////////////////////////////////////////////////////////////////////

    testLower2 : () => {
      assertEqual([ "äöüäöüß アボカド名称について" ], getQueryResults("return NOOPT(LOWER('äöüÄÖÜß アボカド名称について'))"));
      assertEqual([ "äöüäöüß アボカド名称について" ], getQueryResults("return NOOPT(V8(LOWER('äöüÄÖÜß アボカド名称について')))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower function
////////////////////////////////////////////////////////////////////////////////

    testLower3 : () => {
      assertEqual([ "0123456789<>|,;.:-_#'+*@!\"$&/(){[]}?\\" ], getQueryResults("return NOOPT(LOWER('0123456789<>|,;.:-_#\\'+*@!\\\"$&/(){[]}?\\\\'))"));
      assertEqual([ "0123456789<>|,;.:-_#'+*@!\"$&/(){[]}?\\" ], getQueryResults("return NOOPT(V8(LOWER('0123456789<>|,;.:-_#\\'+*@!\\\"$&/(){[]}?\\\\')))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower function
////////////////////////////////////////////////////////////////////////////////

    testLowerInvalid : () => {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(LOWER())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(LOWER()))");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LOWER(NOOPT('yes', 'yes'))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LOWER(NOOPT(V8('yes', 'yes')))");

      assertEqual([ "" ], getQueryResults("RETURN NOOPT(LOWER(null))"));
      assertEqual([ "" ], getQueryResults("RETURN NOOPT(V8(LOWER(null)))"));

      assertEqual([ "true" ], getQueryResults("RETURN NOOPT(LOWER(true))"));
      assertEqual([ "true" ], getQueryResults("RETURN NOOPT(V8(LOWER(true)))"));

      assertEqual([ "3" ], getQueryResults("RETURN NOOPT(LOWER(3))"));
      assertEqual([ "3" ], getQueryResults("RETURN NOOPT(V8(LOWER(3)))"));

      assertEqual([ "[]" ], getQueryResults("RETURN NOOPT(LOWER([]))"));
      assertEqual([ "[]" ], getQueryResults("RETURN NOOPT(V8(LOWER([])))"));

      assertEqual([ "[1,2,3]" ], getQueryResults("RETURN NOOPT(LOWER([1,2,3]))"));
      assertEqual([ "[1,2,3]" ], getQueryResults("RETURN NOOPT(V8(LOWER([1,2,3])))"));

      assertEqual([ "{}" ], getQueryResults("RETURN NOOPT(LOWER({}))"));
      assertEqual([ "{}" ], getQueryResults("RETURN NOOPT(V8(LOWER({})))"));

      assertEqual([ "{\"a\":1,\"b\":2}" ], getQueryResults("RETURN NOOPT(LOWER({A:1,b:2}))"));
      assertEqual([ "{\"a\":1,\"b\":2}" ], getQueryResults("RETURN NOOPT(V8(LOWER({A:1,b:2})))"));

      assertEqual([ "{\"a\":1,\"a\":2,\"b\":3}" ], getQueryResults("RETURN NOOPT(LOWER({A:1,a:2,b:3}))"));
      assertEqual([ "{\"a\":1,\"a\":2,\"b\":3}" ], getQueryResults("RETURN NOOPT(LOWER(V8({A:1,a:2,b:3})))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper function
////////////////////////////////////////////////////////////////////////////////
    
    testUpper1 : () => {
      assertEqual([ 'THE QUICK BROWN FOX JUMPED' ], getQueryResults("return NOOPT(UPPER('THE quick Brown foX JuMpED'))"));
      assertEqual([ 'THE QUICK BROWN FOX JUMPED' ], getQueryResults("return NOOPT(V8(UPPER('THE quick Brown foX JuMpED')))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper function
////////////////////////////////////////////////////////////////////////////////
    
    testUpper2 : () => {
      assertEqual([ 'ÄÖÜÄÖÜSS アボカド名称について' ], getQueryResults("return NOOPT(UPPER('äöüÄÖÜß アボカド名称について'))"));
      assertEqual([ 'ÄÖÜÄÖÜSS アボカド名称について' ], getQueryResults("return NOOPT(V8(UPPER('äöüÄÖÜß アボカド名称について')))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper function
////////////////////////////////////////////////////////////////////////////////

    testUpperInvalid : () => {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN NOOPT(UPPER())');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN NOOPT(V8(UPPER()))');

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(UPPER('yes', 'yes'))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(V8(UPPER('yes', 'yes')))");

      assertEqual([ "" ], getQueryResults("RETURN NOOPT(UPPER(null))"));
      assertEqual([ "" ], getQueryResults("RETURN NOOPT(V8(UPPER(null)))"));

      assertEqual([ "TRUE" ], getQueryResults("RETURN NOOPT(UPPER(true))"));
      assertEqual([ "TRUE" ], getQueryResults("RETURN NOOPT(V8(UPPER(true)))"));

      assertEqual([ "3" ], getQueryResults("RETURN NOOPT(UPPER(3))"));
      assertEqual([ "3" ], getQueryResults("RETURN NOOPT(V8(UPPER(3)))"));

      assertEqual([ "[]" ], getQueryResults("RETURN NOOPT(UPPER([]))"));
      assertEqual([ "[]" ], getQueryResults("RETURN NOOPT(V8(UPPER([])))"));

      assertEqual([ "[1,2,3]" ], getQueryResults("RETURN NOOPT(UPPER([1,2,3]))"));
      assertEqual([ "[1,2,3]" ], getQueryResults("RETURN NOOPT(V8(UPPER([1,2,3])))"));

      assertEqual([ "{}" ], getQueryResults("RETURN NOOPT(UPPER({}))"));
      assertEqual([ "{}" ], getQueryResults("RETURN NOOPT(V8(UPPER({})))"));

      assertEqual([ "{\"A\":1,\"B\":2}" ], getQueryResults("RETURN NOOPT(UPPER({a:1, b:2}))"));
      assertEqual([ "{\"A\":1,\"B\":2}" ], getQueryResults("RETURN NOOPT(V8(UPPER({a:1, b:2})))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test substring function
////////////////////////////////////////////////////////////////////////////////
    
    testSubstring1 : function () {
      var expected = [ "the" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return SUBSTRING('the quick brown fox', 0, 3)");
      assertEqual(expected, actual);

      actual = getQueryResults("FOR r IN [ 1 ] return NOOPT(SUBSTRING('the quick brown fox', 0, 3))");
      assertEqual(expected, actual);

      actual = getQueryResults("FOR r IN [ 1 ] return NOOPT(V8(SUBSTRING('the quick brown fox', 0, 3)))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test substring function
////////////////////////////////////////////////////////////////////////////////
    
    testSubstring2 : function () {
      var expected = [ "quick" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return SUBSTRING('the quick brown fox', 4, 5)");
      assertEqual(expected, actual);

      actual = getQueryResults("FOR r IN [ 1 ] return NOOPT(SUBSTRING('the quick brown fox', 4, 5))");
      assertEqual(expected, actual);

      actual = getQueryResults("FOR r IN [ 1 ] return NOOPT(V8(SUBSTRING('the quick brown fox', 4, 5)))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test substring function
////////////////////////////////////////////////////////////////////////////////
    
    testSubstring3 : function () {
      var expected = [ "fox" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return SUBSTRING('the quick brown fox', -3)");
      assertEqual(expected, actual);

      actual = getQueryResults("FOR r IN [ 1 ] return NOOPT(SUBSTRING('the quick brown fox', -3))");
      assertEqual(expected, actual);

      actual = getQueryResults("FOR r IN [ 1 ] return NOOPT(V8(SUBSTRING('the quick brown fox', -3)))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test substring function
////////////////////////////////////////////////////////////////////////////////

    testSubstringInvalid : function () {
      var buildQuery = function (nr, input) {
        switch (nr) {
          case 0:
            return `RETURN SUBSTRING(${input})`; 
          case 1:
            return `RETURN NOOPT(SUBSTRING(${input}))`; 
          case 2:
            return `RETURN NOOPT(V8(SUBSTRING(${input})))`; 
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 3; ++i) {
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "")); 
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "\"yes\"")); 
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "\"yes\", 0, 2, \"yes\""));
        assertEqual([ "" ], getQueryResults(buildQuery(i, "null, 0")));
        assertEqual([ "" ], getQueryResults(buildQuery(i, "null, 1")));
        assertEqual([ "true" ], getQueryResults(buildQuery(i, "true, 0")));
        assertEqual([ "3" ], getQueryResults(buildQuery(i, "3, 0")));
        assertEqual([ "[]" ], getQueryResults(buildQuery(i, "[ ], 0")));
        assertEqual([ "[1,2,3]" ], getQueryResults(buildQuery(i, "[ 1, 2, 3 ], 0")));
        assertEqual([ "2,3]" ], getQueryResults(buildQuery(i, "[ 1, 2, 3 ], 3")));
        assertEqual([ "{}" ], getQueryResults(buildQuery(i, "{ }, 0")));
        assertEqual([ "" ], getQueryResults(buildQuery(i, "\"yes\", null, 0")));
        assertEqual([ "" ], getQueryResults(buildQuery(i, "\"yes\", true, 0")));
        assertEqual([ "" ], getQueryResults(buildQuery(i, "\"yes\", \"yes\", 0")));
        assertEqual([ "" ], getQueryResults(buildQuery(i, "\"yes\", [ ], 0")));
        assertEqual([ "" ], getQueryResults(buildQuery(i, "\"yes\", { }, 0")));
        assertEqual([ "" ], getQueryResults(buildQuery(i, "\"yes\", \"yes\", null")));
        assertEqual([ "y" ], getQueryResults(buildQuery(i, "\"yes\", \"yes\", true")));
        assertEqual([ "" ], getQueryResults(buildQuery(i, "\"yes\", \"yes\", [ ]")));
        assertEqual([ "" ], getQueryResults(buildQuery(i, "\"yes\", \"yes\", { }")));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test hash function
////////////////////////////////////////////////////////////////////////////////

    testHash : function () {
      var buildQuery = function (nr, input) {
        switch (nr) {
          case 0:
            return `RETURN HASH(${input})`; 
          case 1:
            return `RETURN NOOPT(HASH(${input}))`; 
          case 2:
            return `RETURN NOOPT(V8(HASH(${input})))`; 
          default:
            assertTrue(false, "Undefined state");
        }
      };
      var i;
      for (i = 0; i < 3; ++i) {
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "")); 
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "1, 2")); 
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "1, 2, 3"));
        assertEqual([ 675717317264138 ], getQueryResults(buildQuery(i, "null")));
        assertEqual([ 1217335385489389 ], getQueryResults(buildQuery(i, "false")));
        assertEqual([ 57801618404459 ], getQueryResults(buildQuery(i, "true")));
        assertEqual([ 675717317264138 ], getQueryResults(buildQuery(i, "1 / 0")));
        assertEqual([ 2964198978643 ], getQueryResults(buildQuery(i, "0")));
        assertEqual([ 2964198978643 ], getQueryResults(buildQuery(i, "0.0")));
        assertEqual([ 464020872367562 ], getQueryResults(buildQuery(i, "0.00001")));
        assertEqual([ 652971229830707 ], getQueryResults(buildQuery(i, "1")));
        assertEqual([ 510129580600084 ], getQueryResults(buildQuery(i, "-1")));
        assertEqual([ 24372339383975 ], getQueryResults(buildQuery(i, "-10.5")));
        assertEqual([ 1041198105137773 ], getQueryResults(buildQuery(i, "123452532322453")));
        assertEqual([ 876255539722551 ], getQueryResults(buildQuery(i, "123452532322454")));
        assertEqual([ 1277486662998285 ], getQueryResults(buildQuery(i, "123452532322454.434")));
        assertEqual([ 210539478145939 ], getQueryResults(buildQuery(i, "-123452532322454")));
        assertEqual([ 261745517313272 ], getQueryResults(buildQuery(i, "-9999999999999.999")));
        assertEqual([ 441814588996558 ], getQueryResults(buildQuery(i, "''")));
        assertEqual([ 1112732548475941 ], getQueryResults(buildQuery(i, "' '")));
        assertEqual([ 246233608921999 ], getQueryResults(buildQuery(i, "'  '")));
        assertEqual([ 1542381651001813 ], getQueryResults(buildQuery(i, "'a'")));
        assertEqual([ 843602980995939 ], getQueryResults(buildQuery(i, "'A'")));
        assertEqual([ 1618092585478118 ], getQueryResults(buildQuery(i, "' a'")));
        assertEqual([ 725364078947946 ], getQueryResults(buildQuery(i, "' A'")));
        assertEqual([ 736233736371291 ], getQueryResults(buildQuery(i, "' foobar'")));
        assertEqual([ 360657200843601 ], getQueryResults(buildQuery(i, "'this is a string test. please ignore.'")));
        assertEqual([ 828085160327326 ], getQueryResults(buildQuery(i, "'this is a string test. please Ignore.'")));
        assertEqual([ 2072438876063292 ], getQueryResults(buildQuery(i, "'a string is a string is a string of course. even longer strings can be hashed. isn\\'t this fantastic? let\\'s see if we can cross the short-string bounds with it...'")));
        assertEqual([ 181227890622943 ], getQueryResults(buildQuery(i, "[]")));
        assertEqual([ 346113245898278 ], getQueryResults(buildQuery(i, "[0]")));
        assertEqual([ 785599515440277 ], getQueryResults(buildQuery(i, "[1]")));
        assertEqual([ 1295855700045140 ], getQueryResults(buildQuery(i, "[1,2]")));
        assertEqual([ 1295855700045140 ], getQueryResults(buildQuery(i, "1..2")));
        assertEqual([ 1255602544875390 ], getQueryResults(buildQuery(i, "[2,1]")));
        assertEqual([ 1255602544875390 ], getQueryResults(buildQuery(i, "2..1")));
        assertEqual([ 1625466870434085 ], getQueryResults(buildQuery(i, "[1,2,3]")));
        assertEqual([ 1625466870434085 ], getQueryResults(buildQuery(i, "1..3")));
        assertEqual([ 1657598895986170 ], getQueryResults(buildQuery(i, "[1,2,3,4]")));
        assertEqual([ 1657598895986170 ], getQueryResults(buildQuery(i, "1..4")));
        assertEqual([ 1580543009747638 ], getQueryResults(buildQuery(i, "[1,2,4,3]")));
        assertEqual([ 157821093310761 ], getQueryResults(buildQuery(i, "[1,2,3,2]")));
        assertEqual([ 1032992608692014 ], getQueryResults(buildQuery(i, "[1,2,3,2,1]")));
        assertEqual([ 2051766968908771 ], getQueryResults(buildQuery(i, "1..1000")));
        assertEqual([ 1954991255293719 ], getQueryResults(buildQuery(i, "{}")));
        assertEqual([ 1294634865089389 ], getQueryResults(buildQuery(i, "{a:1}")));
        assertEqual([ 1451630758438458 ], getQueryResults(buildQuery(i, "{a:2}")));
        assertEqual([ 402003666669761 ], getQueryResults(buildQuery(i, "{a:1,b:1}")));
        assertEqual([ 529935412783457 ], getQueryResults(buildQuery(i, "{a:1,b:2}")));
        assertEqual([ 402003666669761 ], getQueryResults(buildQuery(i, "{b:1,a:1}")));
        assertEqual([ 529935412783457 ], getQueryResults(buildQuery(i, "{b:2,a:1}")));
        assertEqual([ 1363279506864914 ], getQueryResults(buildQuery(i, "{b:1,a:2}")));
        assertEqual([ 1363279506864914 ], getQueryResults(buildQuery(i, "{a:2,b:1}")));
        assertEqual([ 1685918180496814 ], getQueryResults(buildQuery(i, "{a:2,b:'1'}")));
        assertEqual([ 874128984798182 ], getQueryResults(buildQuery(i, "{a:2,b:null}")));
        assertEqual([ 991653416476703 ], getQueryResults(buildQuery(i, "{A:1,B:2}")));
        assertEqual([ 502569457877206 ], getQueryResults(buildQuery(i, "{a:'A',b:'B'}")));
        assertEqual([ 1154380811055928 ], getQueryResults(buildQuery(i, "{a:'a',b:'b'}")));
        assertEqual([ 416732334603048 ], getQueryResults(buildQuery(i, "{a:['a'],b:['b']}")));
        assertEqual([ 176300349653218 ], getQueryResults(buildQuery(i, "{a:1,b:-1}")));
        assertEqual([ 1460607510107728 ], getQueryResults(buildQuery(i, "{_id:'foo',_key:'bar',_rev:'baz'}")));
        assertEqual([ 1271501175803754 ], getQueryResults(buildQuery(i, "{_id:'foo',_key:'bar',_rev:'baz',bar:'bark'}")));
      }
      
      for (i = 0; i < 3; ++i) {
        // order does not matter
        assertEqual(getQueryResults(buildQuery(i, "{a:1,b:2}")), getQueryResults(buildQuery(i, "{b:2,a:1}")));
        assertNotEqual(getQueryResults(buildQuery(i, "{a:1,b:2}")), getQueryResults(buildQuery(i, "{a:2,b:1}")));
        // order matters
        assertNotEqual(getQueryResults(buildQuery(i, "[1,2,3]")), getQueryResults(buildQuery(i, "[3,2,1]")));
        // arrays and ranges
        assertEqual(getQueryResults(buildQuery(i, "[1,2,3]")), getQueryResults(buildQuery(i, "1..3")));
        // arrays and subqueries
        assertEqual(getQueryResults(buildQuery(i, "[1,2,3]")), getQueryResults(buildQuery(i, "FOR i IN [1,2,3] RETURN i")));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test md5 function
////////////////////////////////////////////////////////////////////////////////

    testMd5 : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MD5()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MD5(\"foo\", 2)"); 
      assertEqual([ "d41d8cd98f00b204e9800998ecf8427e" ], getQueryResults("RETURN MD5('')")); 
      assertEqual([ "7215ee9c7d9dc229d2921a40e899ec5f" ], getQueryResults("RETURN MD5(' ')")); 
      assertEqual([ "cfcd208495d565ef66e7dff9f98764da" ], getQueryResults("RETURN MD5('0')")); 
      assertEqual([ "c4ca4238a0b923820dcc509a6f75849b" ], getQueryResults("RETURN MD5('1')")); 
      assertEqual([ "6bb61e3b7bce0931da574d19d1d82c88" ], getQueryResults("RETURN MD5('-1')")); 
      assertEqual([ "0bad51c0b9b2ba77c19bf6bfbbf88dc3" ], getQueryResults("RETURN MD5(' 0')")); 
      assertEqual([ "2e5751b7cfd7f053cd29e946fb2649a4" ], getQueryResults("RETURN MD5('0 ')")); 
      assertEqual([ "acbd18db4cc2f85cedef654fccc4a4d8" ], getQueryResults("RETURN MD5('foo')")); 
      assertEqual([ "901890a8e9c8cf6d5a1a542b229febff" ], getQueryResults("RETURN MD5('FOO')")); 
      assertEqual([ "1356c67d7ad1638d816bfb822dd2c25d" ], getQueryResults("RETURN MD5('Foo')")); 
      assertEqual([ "f32a26e2a3a8aa338cd77b6e1263c535" ], getQueryResults("RETURN MD5('FooBar')")); 
      assertEqual([ "c639efc1e98762233743a75e7798dd9c" ], getQueryResults("RETURN MD5('This is a test string')")); 
      assertEqual([ "f9a70133b9fe5fa12acd30056bf4aa26" ], getQueryResults("RETURN MD5('With\r\nLinebreaks\n')")); 
      assertEqual([ "1441a7909c087dbbe7ce59881b9df8b9" ], getQueryResults("RETURN MD5('[object Object]')"));
      assertEqual([ "cfcd208495d565ef66e7dff9f98764da" ], getQueryResults("RETURN MD5(0)")); 
      assertEqual([ "c4ca4238a0b923820dcc509a6f75849b" ], getQueryResults("RETURN MD5(1)")); 
      assertEqual([ "6bb61e3b7bce0931da574d19d1d82c88" ], getQueryResults("RETURN MD5(-1)")); 
      assertEqual([ "d41d8cd98f00b204e9800998ecf8427e" ], getQueryResults("RETURN MD5(null)")); 
      assertEqual([ "d751713988987e9331980363e24189ce" ], getQueryResults("RETURN MD5([])")); 
      assertEqual([ "8d5162ca104fa7e79fe80fd92bb657fb" ], getQueryResults("RETURN MD5([0])")); 
      assertEqual([ "35dba5d75538a9bbe0b4da4422759a0e" ], getQueryResults("RETURN MD5('[1]')")); 
      assertEqual([ "f79408e5ca998cd53faf44af31e6eb45" ], getQueryResults("RETURN MD5([1,2])")); 
      assertEqual([ "99914b932bd37a50b983c5e7c90ae93b" ], getQueryResults("RETURN MD5({ })")); 
      assertEqual([ "99914b932bd37a50b983c5e7c90ae93b" ], getQueryResults("RETURN MD5({})")); 
      assertEqual([ "608de49a4600dbb5b173492759792e4a" ], getQueryResults("RETURN MD5({a:1,b:2})")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sha1 function
////////////////////////////////////////////////////////////////////////////////

    testSha1 : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SHA1()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SHA1(\"foo\", 2)"); 
      assertEqual([ "da39a3ee5e6b4b0d3255bfef95601890afd80709" ], getQueryResults("RETURN SHA1('')")); 
      assertEqual([ "b858cb282617fb0956d960215c8e84d1ccf909c6" ], getQueryResults("RETURN SHA1(' ')")); 
      assertEqual([ "b6589fc6ab0dc82cf12099d1c2d40ab994e8410c" ], getQueryResults("RETURN SHA1('0')")); 
      assertEqual([ "356a192b7913b04c54574d18c28d46e6395428ab" ], getQueryResults("RETURN SHA1('1')")); 
      assertEqual([ "7984b0a0e139cabadb5afc7756d473fb34d23819" ], getQueryResults("RETURN SHA1('-1')")); 
      assertEqual([ "7ae5a5c19b16f9ee3b00ca36fc729536fb5e7307" ], getQueryResults("RETURN SHA1(' 0')")); 
      assertEqual([ "1ee9183b1f737da4d348ea42281bd1dd682c5d52" ], getQueryResults("RETURN SHA1('0 ')")); 
      assertEqual([ "0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33" ], getQueryResults("RETURN SHA1('foo')")); 
      assertEqual([ "feab40e1fca77c7360ccca1481bb8ba5f919ce3a" ], getQueryResults("RETURN SHA1('FOO')")); 
      assertEqual([ "201a6b3053cc1422d2c3670b62616221d2290929" ], getQueryResults("RETURN SHA1('Foo')")); 
      assertEqual([ "eb8fc41f9d9ae5855c4d801355075e4ccfb22808" ], getQueryResults("RETURN SHA1('FooBar')")); 
      assertEqual([ "e2f67c772368acdeee6a2242c535c6cc28d8e0ed" ], getQueryResults("RETURN SHA1('This is a test string')")); 
      assertEqual([ "a90b947dd16a53e717451d3c536d445ece647c52" ], getQueryResults("RETURN SHA1('With\r\nLinebreaks\n')")); 
      assertEqual([ "2be88ca4242c76e8253ac62474851065032d6833" ], getQueryResults("RETURN SHA1('null')")); 
      assertEqual([ "f629ae44b7b3dcfed444d363e626edf411ec69a8" ], getQueryResults("RETURN SHA1('[1]')")); 
      assertEqual([ "c1d44ff03aff1372856c281854f454e2e1d15b7c" ], getQueryResults("RETURN SHA1('[object Object]')")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test md5 function
////////////////////////////////////////////////////////////////////////////////

    testMd5Cxx : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MD5())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MD5(\"foo\", 2))"); 
      assertEqual([ "d41d8cd98f00b204e9800998ecf8427e" ], getQueryResults("RETURN NOOPT(MD5(''))")); 
      assertEqual([ "7215ee9c7d9dc229d2921a40e899ec5f" ], getQueryResults("RETURN NOOPT(MD5(' '))")); 
      assertEqual([ "cfcd208495d565ef66e7dff9f98764da" ], getQueryResults("RETURN NOOPT(MD5('0'))")); 
      assertEqual([ "c4ca4238a0b923820dcc509a6f75849b" ], getQueryResults("RETURN NOOPT(MD5('1'))")); 
      assertEqual([ "6bb61e3b7bce0931da574d19d1d82c88" ], getQueryResults("RETURN NOOPT(MD5('-1'))")); 
      assertEqual([ "0bad51c0b9b2ba77c19bf6bfbbf88dc3" ], getQueryResults("RETURN NOOPT(MD5(' 0'))")); 
      assertEqual([ "2e5751b7cfd7f053cd29e946fb2649a4" ], getQueryResults("RETURN NOOPT(MD5('0 '))")); 
      assertEqual([ "acbd18db4cc2f85cedef654fccc4a4d8" ], getQueryResults("RETURN NOOPT(MD5('foo'))")); 
      assertEqual([ "901890a8e9c8cf6d5a1a542b229febff" ], getQueryResults("RETURN NOOPT(MD5('FOO'))")); 
      assertEqual([ "1356c67d7ad1638d816bfb822dd2c25d" ], getQueryResults("RETURN NOOPT(MD5('Foo'))")); 
      assertEqual([ "f32a26e2a3a8aa338cd77b6e1263c535" ], getQueryResults("RETURN NOOPT(MD5('FooBar'))")); 
      assertEqual([ "c639efc1e98762233743a75e7798dd9c" ], getQueryResults("RETURN NOOPT(MD5('This is a test string'))")); 
      assertEqual([ "f9a70133b9fe5fa12acd30056bf4aa26" ], getQueryResults("RETURN NOOPT(MD5('With\r\nLinebreaks\n'))")); 
      assertEqual([ "1441a7909c087dbbe7ce59881b9df8b9" ], getQueryResults("RETURN NOOPT(MD5('[object Object]'))"));
      assertEqual([ "cfcd208495d565ef66e7dff9f98764da" ], getQueryResults("RETURN NOOPT(MD5(0))")); 
      assertEqual([ "c4ca4238a0b923820dcc509a6f75849b" ], getQueryResults("RETURN NOOPT(MD5(1))")); 
      assertEqual([ "6bb61e3b7bce0931da574d19d1d82c88" ], getQueryResults("RETURN NOOPT(MD5(-1))")); 
      assertEqual([ "d41d8cd98f00b204e9800998ecf8427e" ], getQueryResults("RETURN NOOPT(MD5(null))")); 
      assertEqual([ "35dba5d75538a9bbe0b4da4422759a0e" ], getQueryResults("RETURN NOOPT(MD5('[1]'))")); 
      assertEqual([ "99914b932bd37a50b983c5e7c90ae93b" ], getQueryResults("RETURN NOOPT(MD5({}))")); 
      assertEqual([ "c7cb8c1df686c0219d540849efe3bce3" ], getQueryResults("RETURN NOOPT(MD5('[1,2,4,7,11,16,22,29,37,46,56,67,79,92,106,121,137,154,172,191,211,232,254,277,301,326,352,379,407,436,466,497,529,562,596,631,667,704,742,781,821,862,904,947,991,1036,1082,1129,1177,1226,1276,1327,1379,1432,1486,1541,1597,1654,1712,1771,1831,1892,1954,2017,2081,2146,2212,2279,2347,2416,2486,2557,2629,2702,2776,2851,2927,3004,3082,3161,3241,3322,3404,3487,3571,3656,3742,3829,3917,4006,4096,4187,4279,4372,4466,4561,4657,4754,4852,4951]'))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sha1 function
////////////////////////////////////////////////////////////////////////////////

    testSha1Cxx : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SHA1())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SHA1(\"foo\", 2))"); 
      assertEqual([ "da39a3ee5e6b4b0d3255bfef95601890afd80709" ], getQueryResults("RETURN NOOPT(SHA1(''))")); 
      assertEqual([ "b858cb282617fb0956d960215c8e84d1ccf909c6" ], getQueryResults("RETURN NOOPT(SHA1(' '))")); 
      assertEqual([ "b6589fc6ab0dc82cf12099d1c2d40ab994e8410c" ], getQueryResults("RETURN NOOPT(SHA1('0'))")); 
      assertEqual([ "356a192b7913b04c54574d18c28d46e6395428ab" ], getQueryResults("RETURN NOOPT(SHA1('1'))")); 
      assertEqual([ "7984b0a0e139cabadb5afc7756d473fb34d23819" ], getQueryResults("RETURN NOOPT(SHA1('-1'))")); 
      assertEqual([ "7ae5a5c19b16f9ee3b00ca36fc729536fb5e7307" ], getQueryResults("RETURN NOOPT(SHA1(' 0'))")); 
      assertEqual([ "1ee9183b1f737da4d348ea42281bd1dd682c5d52" ], getQueryResults("RETURN NOOPT(SHA1('0 '))")); 
      assertEqual([ "0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33" ], getQueryResults("RETURN NOOPT(SHA1('foo'))")); 
      assertEqual([ "feab40e1fca77c7360ccca1481bb8ba5f919ce3a" ], getQueryResults("RETURN NOOPT(SHA1('FOO'))")); 
      assertEqual([ "201a6b3053cc1422d2c3670b62616221d2290929" ], getQueryResults("RETURN NOOPT(SHA1('Foo'))")); 
      assertEqual([ "eb8fc41f9d9ae5855c4d801355075e4ccfb22808" ], getQueryResults("RETURN NOOPT(SHA1('FooBar'))")); 
      assertEqual([ "e2f67c772368acdeee6a2242c535c6cc28d8e0ed" ], getQueryResults("RETURN NOOPT(SHA1('This is a test string'))")); 
      assertEqual([ "a90b947dd16a53e717451d3c536d445ece647c52" ], getQueryResults("RETURN NOOPT(SHA1('With\r\nLinebreaks\n'))")); 
      assertEqual([ "2be88ca4242c76e8253ac62474851065032d6833" ], getQueryResults("RETURN NOOPT(SHA1('null'))")); 
      assertEqual([ "f629ae44b7b3dcfed444d363e626edf411ec69a8" ], getQueryResults("RETURN NOOPT(SHA1('[1]'))")); 
      assertEqual([ "c1d44ff03aff1372856c281854f454e2e1d15b7c" ], getQueryResults("RETURN NOOPT(SHA1('[object Object]'))")); 
      assertEqual([ "888227c44807b86059eb36f9fe0fc602a9b16fab" ], getQueryResults("RETURN NOOPT(SHA1('[1,2,4,7,11,16,22,29,37,46,56,67,79,92,106,121,137,154,172,191,211,232,254,277,301,326,352,379,407,436,466,497,529,562,596,631,667,704,742,781,821,862,904,947,991,1036,1082,1129,1177,1226,1276,1327,1379,1432,1486,1541,1597,1654,1712,1771,1831,1892,1954,2017,2081,2146,2212,2279,2347,2416,2486,2557,2629,2702,2776,2851,2927,3004,3082,3161,3241,3322,3404,3487,3571,3656,3742,3829,3917,4006,4096,4187,4279,4372,4466,4561,4657,4754,4852,4951]'))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test random_token function
////////////////////////////////////////////////////////////////////////////////

    testRandomToken : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RANDOM_TOKEN()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RANDOM_TOKEN(1, 2)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANDOM_TOKEN(-1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANDOM_TOKEN(-10)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANDOM_TOKEN(0)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANDOM_TOKEN(65537)"); 

      var actual = getQueryResults("FOR i IN [ 1, 10, 100, 1000 ] RETURN RANDOM_TOKEN(i)");
      assertEqual(4, actual.length);
      assertEqual(1, actual[0].length);
      assertEqual(10, actual[1].length);
      assertEqual(100, actual[2].length);
      assertEqual(1000, actual[3].length);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlStringFunctionsTestSuite);

return jsunity.done();

