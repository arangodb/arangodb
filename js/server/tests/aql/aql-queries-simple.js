/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, simple queries
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
var errors = require("internal").errors;
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQuerySimpleTestSuite () {
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
/// @brief test filters with special characters
////////////////////////////////////////////////////////////////////////////////

    testSpecialChars1 : function () {
      var data = [ 
        "the quick\nbrown fox jumped over\r\nthe lazy dog",
        "'the \"\\quick\\\n \"brown\\\rfox' jumped",
        '"the fox"" jumped \\over the \newline \roof"'
      ];

      data.forEach(function(value) {
        var actual = getQueryResults("FOR doc IN [ { text: " + JSON.stringify(value) + " } ] FILTER doc.text == " + JSON.stringify(value) + " RETURN doc.text");
        assertEqual([ value ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test filters with special characters
////////////////////////////////////////////////////////////////////////////////

    testSpecialChars2 : function () {
      var data = [ 
        "the quick\nbrown fox jumped over\r\nthe lazy dog",
        "'the \"\\quick\\\n \"brown\\\rfox' jumped",
        '"the fox"" jumped \\over the \newline \roof"'
      ];

      data.forEach(function(value) {
        var actual = getQueryResults("FOR doc IN [ { text: @value } ] FILTER doc.text == @value RETURN doc.text", { value: value });
        assertEqual([ value ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief range
////////////////////////////////////////////////////////////////////////////////

    testRange1 : function () {
      var expected = [ [ 1, 2, 3 ] ];

      var actual = getQueryResults("RETURN 1..3");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief range
////////////////////////////////////////////////////////////////////////////////

    testRange2 : function () {
      var expected = [ [ 7, 8, 9, 10, 11, 12, 13, 14, 15 ] ];

      var actual = getQueryResults("RETURN 7..15");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief range
////////////////////////////////////////////////////////////////////////////////

    testRange3 : function () {
      var expected = [ [ -5, -6, -7, -8, -9, -10 ] ];

      var actual = getQueryResults("RETURN -5..-10");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief range
////////////////////////////////////////////////////////////////////////////////

    testRange4 : function () {
      var expected = [ [ -5, -4, -3, -2, -1, 0, 1, 2 ] ];

      var actual = getQueryResults("RETURN -5..2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief range
////////////////////////////////////////////////////////////////////////////////

    testRange5 : function () {
      var expected = [ [ 5, 4, 3, 2, 1, 0, -1, -2 ] ];

      var actual = getQueryResults("RETURN 5..-2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief range
////////////////////////////////////////////////////////////////////////////////

    testRange6 : function () {
      var expected = [ [ 0 ] ];

      var actual = getQueryResults("RETURN 0..0");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief range
////////////////////////////////////////////////////////////////////////////////

    testRange7 : function () {
      var expected = [ [ 3, 4, 5, 6, 7, 8, 9 ] ];

      var actual = getQueryResults("LET a = 3, b = 9 RETURN a..b");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief range
////////////////////////////////////////////////////////////////////////////////

    testRange8 : function () {
      var result = [ ], i ,j;
      for (i = 1; i <= 10; ++i) {
        for (j = 1; j <= 10; ++j) {
          result.push(i * j);
        }
      }

      var actual = getQueryResults("FOR i IN 1..10 FOR j IN 1..10 RETURN i * j");
      assertEqual(result, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief range
////////////////////////////////////////////////////////////////////////////////

    testRange9 : function () {
      var expected = [ [ 1, 2, 3, 4 ] ];

      var actual = AQL_EXECUTE("RETURN 1.5..4.76").json;
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi let
////////////////////////////////////////////////////////////////////////////////

    testMultiLet1 : function () {
      var expected = [ 6 ];

      var actual = getQueryResults("let a = 1, b = 2, c = 3 return a + b+ c");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi let
////////////////////////////////////////////////////////////////////////////////

    testMultiLet2 : function () {
      var expected = [ 1, 1, 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] LET a = 1, b = 2, c = 3 RETURN 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi let
////////////////////////////////////////////////////////////////////////////////

    testMultiLet3 : function () {
      var expected = [ 3, 2, 2 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] LET a = 1, b = 2, c = 3 RETURN i > a ? b : c");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi let
////////////////////////////////////////////////////////////////////////////////

    testMultiLet4 : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "LET a = 1, b = RETURN a"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi let
////////////////////////////////////////////////////////////////////////////////

    testMultiLet5 : function () {
      var expected = [ [ 1, 2, 3 ] ];

      var actual = getQueryResults("LET a = 1, b = (FOR i IN [ 1, 2, 3 ] RETURN i) RETURN b");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi let
////////////////////////////////////////////////////////////////////////////////

    testMultiLetOrder1 : function () {
      var expected = [ 14 ];

      var actual = getQueryResults("LET a = 1, b = a + 1, c = b * 7 RETURN c");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi let
////////////////////////////////////////////////////////////////////////////////

    testMultiLetOrder2 : function () {
      var expected = [ 5 ];

      var actual = getQueryResults("LET a = [ 3, 5 ], r = (FOR i IN a RETURN i) RETURN r[1]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi let
////////////////////////////////////////////////////////////////////////////////

    testMultiLetOrder3 : function () {
      var expected = [ 5 ];

      var actual = getQueryResults("LET a = [ 3, 5 ], r = (FOR i IN a RETURN i), t = r[1] RETURN t");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return null
////////////////////////////////////////////////////////////////////////////////

    testReturnOnlyQuery1 : function () {
      var expected = [ null ];

      var actual = getQueryResults("return null");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a number
////////////////////////////////////////////////////////////////////////////////

    testReturnOnlyQuery2 : function () {
      var expected = [ 0 ];

      var actual = getQueryResults("return 0");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string
////////////////////////////////////////////////////////////////////////////////

    testReturnOnlyQuery3 : function () {
      var expected = [ "a string value" ];

      var actual = getQueryResults("return \"a string value\"");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list
////////////////////////////////////////////////////////////////////////////////

    testReturnOnlyQuery4 : function () {
      var expected = [ [ 1, 2, 9 ] ];

      var actual = getQueryResults("return [ 1, 2, 9 ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return an array
////////////////////////////////////////////////////////////////////////////////

    testReturnOnlyQuery5 : function () {
      var expected = [ { "active" : false, "age" : 35, "id" : 15, "likes" : [ ], "name" : "Peter" } ];

      var actual = getQueryResults("return { \"name\" : \"Peter\", \"id\" : 15, \"age\" : 35, \"active\" : false, \"likes\" : [ ] }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of arrays
////////////////////////////////////////////////////////////////////////////////

    testReturnOnlyQuery6 : function () {
      var expected = [ [ { "name" : "Max", "id" : 1 }, { "name" : "Vanessa", "id" : 2 }, { "unrelated" : [ "some", "stuff", "goes", "here" ] } ] ];

      var actual = getQueryResults("return [ { \"name\" : \"Max\", \"id\" : 1 }, { \"name\" : \"Vanessa\", \"id\" : 2 }, { \"unrelated\" : [ \"some\", \"stuff\", \"goes\", \"here\" ] } ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a computed value
////////////////////////////////////////////////////////////////////////////////

    testReturnOnlyQuery7 : function () {
      var expected = [ 42 ];

      var actual = getQueryResults("return 1 + 40 + 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from let
////////////////////////////////////////////////////////////////////////////////

    testReturnLet1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("let x = 1 return x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from let
////////////////////////////////////////////////////////////////////////////////

    testReturnLet2 : function () {
      var expected = [ [ 1, 2, 42 ] ];

      var actual = getQueryResults("let x = [ 1, 2, 42 ] return x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from let
////////////////////////////////////////////////////////////////////////////////

    testReturnLet3 : function () {
      var expected = [ null ];

      var actual = getQueryResults("let x = null return x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from let
////////////////////////////////////////////////////////////////////////////////

    testReturnLet4 : function () {
      var expected = [ [ 1, 2, 3 ] ];

      var actual = getQueryResults("let x = (for u in [ 1, 2, 3 ] return u) return x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from let
////////////////////////////////////////////////////////////////////////////////

    testReturnLet5 : function () {
      var expected = [ [ 2, 3, 4 ] ];

      var actual = getQueryResults("let x = (for u in (for v in [ 1, 2, 3 ] return v + 1) return u) return x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from let
////////////////////////////////////////////////////////////////////////////////

    testReturnLet6 : function () {
      var expected = [ [ 1 ] ];

      var actual = getQueryResults("let x = (return 1) return x");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter1 : function () {
      var expected = [ ];

      var actual = getQueryResults("filter 1 == 0 return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter2 : function () {
      var expected = [ ];

      var actual = getQueryResults("filter 1 == 1 filter 1 == 0 return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("filter 1 == 1 return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter4 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("filter 1 == 1 filter \"true\" == \"true\" return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter5 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let x = (filter 1 == 1 return 1) return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter6 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let x = (filter 0 == 1 return 1) return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter7 : function () {
      var expected = [ ];

      var actual = getQueryResults("let x = (filter 0 == 1 return 1) filter x == [ 1 ] return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter8 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let x = (filter 1 == 1 return 1) filter x == [ 1 ] return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter9 : function () {
      var expected = [ ];

      var actual = getQueryResults("filter 1 == 0 filter 2 == 3 return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter10 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 filter a > 1 && a < 0 filter a > 2 && a < 1 return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter11 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 filter a == 1 && a == 2 && a == 3 return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter12 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a == b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter13 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a != b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter14 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a >= b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter15 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a > b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter16 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a <= b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter17 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a < b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter18 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a >= b filter a <= b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter19 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a >= b filter a < b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter20 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a >= b filter a > b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter21 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a >= b filter a >= b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter22 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a >= b filter a == b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter23 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a > b filter a <= b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter24 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a > b filter a < b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter25 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a > b filter a > b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter26 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a > b filter a >= b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter27 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a > b filter a == b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter28 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a == b filter a <= b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter29 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a == b filter a < b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter30 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a == b filter a >= b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a value from a filter
////////////////////////////////////////////////////////////////////////////////

    testReturnFilter31 : function () {
      var expected = [ ];

      var actual = getQueryResults("let a = 1 let b = 1 filter a == b filter a > b return 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief sort and return
////////////////////////////////////////////////////////////////////////////////

    testReturnSort1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("sort null return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief sort and return
////////////////////////////////////////////////////////////////////////////////

    testReturnSort2 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("sort [ 1, 2 ] return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief sort and return
////////////////////////////////////////////////////////////////////////////////

    testReturnSort3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("sort rand() return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief sort and return
////////////////////////////////////////////////////////////////////////////////

    testReturnSort4 : function () {
      var expected = [ [ 3, 2, 1 ] ];

      var actual = getQueryResults("let a=(for x in [1,2,3] sort x desc return x) return a");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief limit and return
////////////////////////////////////////////////////////////////////////////////

    testReturnLimit1 : function () {
      var expected = [ ];

      var actual = getQueryResults("limit 0 return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief limit and return
////////////////////////////////////////////////////////////////////////////////

    testReturnLimit2 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("limit 1 return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief limit and return
////////////////////////////////////////////////////////////////////////////////

    testReturnLimit3 : function () {
      var expected = [ ];

      var actual = getQueryResults("limit 0, 0 return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief limit and return
////////////////////////////////////////////////////////////////////////////////

    testReturnLimit4 : function () {
      var expected = [ ];

      var actual = getQueryResults("limit 1, 1 return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief limit and return
////////////////////////////////////////////////////////////////////////////////

    testReturnLimit5 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("limit 0, 1 return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief collect and return
////////////////////////////////////////////////////////////////////////////////

    testReturnCollect1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("collect city = 1 return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief collect and return
////////////////////////////////////////////////////////////////////////////////

    testReturnCollect2 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("collect city = 1 into g return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief collect and return
////////////////////////////////////////////////////////////////////////////////

    testReturnCollect3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("collect city = 1, nonsense = 2 return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief comments
////////////////////////////////////////////////////////////////////////////////

    testComments1: function () {
      var expected = [ 1 ];

      var actual = getQueryResults("/* return 2 */ return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief comments
////////////////////////////////////////////////////////////////////////////////

    testComments2: function () {
      var expected = [ [ 4 ] ];

      var actual = getQueryResults("/* return 1 */ return [/*1,2,3,*/4 /*5,6,7*/]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief comments
////////////////////////////////////////////////////////////////////////////////

    testComments3: function () {
      var expected = [ 1 ];

      var actual = getQueryResults("/* for u in [1,2,3] */ return 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief list index access 
////////////////////////////////////////////////////////////////////////////////

    testListIndexes: function () {
      var actual;
      
      actual = getQueryResults("LET l = [ 1, 2, 3 ] RETURN l[0]");
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("LET l = [ 1, 2, 3 ] RETURN l[1]");
      assertEqual([ 2 ], actual);
      
      actual = getQueryResults("LET l = [ 1, 2, 3 ] RETURN l[2]");
      assertEqual([ 3 ], actual);

      actual = getQueryResults("LET l = [ 1, 2, 3 ] RETURN l[3]");
      assertEqual([ null ], actual);
      
      actual = getQueryResults("LET l = [ 1, 2, 3 ] RETURN l[-1]");
      assertEqual([ 3 ], actual);
      
      actual = getQueryResults("LET l = [ 1, 2, 3 ] RETURN l[-2]");
      assertEqual([ 2 ], actual);
      
      actual = getQueryResults("LET l = [ 1, 2, 3 ] RETURN l[-3]");
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("LET l = [ 1, 2, 3 ] RETURN l[-4]");
      assertEqual([ null ], actual);
      
      actual = getQueryResults("LET l = [ 1, 2, 3 ] RETURN l['a']");
      assertEqual([ null ], actual);
  
      actual = getQueryResults("LET l = [ 1, 2, 3 ] RETURN l['0']");
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("LET l = { '1': 1, '2': 2, '3': 3 } RETURN l[0]");
      assertEqual([ null ], actual);
      
      actual = getQueryResults("LET l = { '1': 1, '2': 2, '3': 3 } RETURN l[1]");
      assertEqual([ '1' ], actual);
      
      actual = getQueryResults("LET l = { '1': 1, '2': 2, '3': 3 } RETURN l[-1]");
      assertEqual([ null ], actual);
      
      actual = getQueryResults("LET l = { '1': 1, '2': 2, '3': 3 } RETURN l['0']");
      assertEqual([ null ], actual);
      
      actual = getQueryResults("LET l = { '1': 1, '2': 2, '3': 3 } RETURN l['1']");
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("LET l = { '1': 1, '2': 2, '3': 3 } RETURN l['2']");
      assertEqual([ 2 ], actual);
      
      actual = getQueryResults("LET l = { '1': 1, '2': 2, '3': 3 } RETURN l['24']");
      assertEqual([ null ], actual);
      
      actual = getQueryResults("LET l = { '1': 1, '2': 2, '3': 3 } RETURN l['-1']");
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief naming
////////////////////////////////////////////////////////////////////////////////

    testNaming: function () {
      var actual;
      
      actual = getQueryResults("LET a = [ 1 ] RETURN a[0]");
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("LET `a` = [ 1 ] RETURN `a`[0]");
      assertEqual([ 1 ], actual);
    
      actual = getQueryResults("LET `a b` = [ 1 ] RETURN `a b`[0]");
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("LET `a b c` = [ 1 ] RETURN `a b c`[0]");
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("LET `a b c` = { `d e f`: 1 } RETURN `a b c`['d e f']");
      assertEqual([ 1 ], actual);
      
      assertQueryError(errors.ERROR_ARANGO_ILLEGAL_NAME.code, "LET a = 1 RETURN `a b c`"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief numeric overflow at compile time
////////////////////////////////////////////////////////////////////////////////

    testOverflowCompileInt: function () {
      assertEqual([ 0 ], getQueryResults("LET l = 4444444444444555555555555555555555555555555555554444333333333333333333333334444444544 RETURN l * l * l * l * l"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief numeric overflow at compile time
////////////////////////////////////////////////////////////////////////////////

    testOverflowCompileDouble: function () {
      assertEqual([ 0 ], getQueryResults("LET l = 4.0e999 RETURN l * l * l * l * l")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief numeric underflow at compile time
////////////////////////////////////////////////////////////////////////////////

    testUnderflowCompileInt: function () {
      assertEqual([ 0 ], getQueryResults("LET l = -4444444444444555555555555555555555555555555555554444333333333333333333333334444444544 RETURN l * l * l * l * l"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief numeric underflow at compile time
////////////////////////////////////////////////////////////////////////////////

    testUnderflowCompileDouble: function () {
      assertEqual([ 0 ], getQueryResults("LET l = -4.0e999 RETURN l * l * l * l * l")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief numeric overflow at execution time
////////////////////////////////////////////////////////////////////////////////

    testOverflowExecutionInt: function () {
      assertEqual([ null ], getQueryResults("FOR l IN [ 33939359949454345354858882332 ] RETURN l * l * l * l * l * l * l * l * l * l * l")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief numeric overflow at execution time
////////////////////////////////////////////////////////////////////////////////

    testOverflowExecutionDouble: function () {
      assertEqual([ 0 ], getQueryResults("FOR l IN [ 3.0e300 ] RETURN l * l * l * l * l * l * l * l * l * l * l")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief numeric underflow at execution time
////////////////////////////////////////////////////////////////////////////////

    testUnderflowExecutionInt: function () {
      assertEqual([ null ], getQueryResults("FOR l IN [ -33939359949454345354858882332 ] RETURN l * l * l * l * l * l * l * l * l * l * l")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief numeric underflow at execution time
////////////////////////////////////////////////////////////////////////////////

    testUnderflowExecutionDouble: function () {
      assertEqual([ 0 ], getQueryResults("FOR l IN [ -3.0e300 ] RETURN l * l * l * l * l * l * l * l * l * l * l")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief big integer overflow
////////////////////////////////////////////////////////////////////////////////

    testBigIntOverflow: function () {
      assertEqual([ 9223372036854776000 ], getQueryResults("RETURN 9223372036854775808")); 
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief big integer underflow
////////////////////////////////////////////////////////////////////////////////

    testBigIntUnderflow: function () {
      assertEqual([ -9223372036854776000 ], getQueryResults("RETURN -9223372036854775809")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief big integers
////////////////////////////////////////////////////////////////////////////////

    testBigInt: function () {
      var actual;

      actual = getQueryResults("FOR i IN [ 2147483647, 2147483648, -2147483648, -2147483649 /*,  9223372036854775807,*/ /*-9223372036854775808*/ ] RETURN 1");
      assertEqual([ 1, 1, 1, 1 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief negative numbers
////////////////////////////////////////////////////////////////////////////////

    testNegativeNumbers1: function () {
      var actual;

      actual = getQueryResults("FOR i IN [ -1, -2, -2.5, -5.5, -1 - 3, -5.5 - 1.5, -5.5 - (-1.5) ] RETURN i");
      assertEqual([ -1, -2, -2.5, -5.5, -4, -7, -4 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief negative numbers
////////////////////////////////////////////////////////////////////////////////
    
    testNegativeNumbers2: function () {
      var actual;

      actual = getQueryResults("FOR i IN [ 1 ] RETURN { a: -1 -3, b: 2 - 0 }");
      assertEqual([ { a: -4, b: 2 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty lists
////////////////////////////////////////////////////////////////////////////////

    testEmptyLists1: function () {
      var actual;

      actual = getQueryResults("LET l1 = [ ] LET l2 = [ ] RETURN { l1: l1, l2: l2 }");
      assertEqual([ { l1: [ ], l2: [ ] } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty lists
////////////////////////////////////////////////////////////////////////////////

    testEmptyLists2: function () {
      var actual;

      actual = getQueryResults("LET l1 = (FOR i IN [] RETURN i) LET l2 = (FOR i IN [ ] RETURN i) RETURN { l1: l1, l2: l2 }");
      assertEqual([ { l1: [ ], l2: [ ] } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty lists
////////////////////////////////////////////////////////////////////////////////

    testEmptyLists3: function () {
      var actual;

      actual = getQueryResults("LET l1 = @l1 LET l2 = @l2 RETURN { l1: l1, l2: l2 }", { l1: [ ], l2: [ ] });
      assertEqual([ { l1: [ ], l2: [ ] } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty lists
////////////////////////////////////////////////////////////////////////////////

    testEmptyLists4: function () {
      var actual;

      actual = getQueryResults("LET l1 = (FOR i IN @l1 RETURN i) LET l2 = (FOR i IN @l2 RETURN i) RETURN { l1: l1, l2: l2 }", { l1: [ ], l2: [ ] });
      assertEqual([ { l1: [ ], l2: [ ] } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constant filter
////////////////////////////////////////////////////////////////////////////////

    testConstantFilter1: function () {
      var actual;

      actual = getQueryResults("FOR i IN [1, 2, 3, 4] FILTER 1 >= 0 RETURN i");
      assertEqual([ 1, 2, 3, 4 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constant filter
////////////////////////////////////////////////////////////////////////////////

    testConstantFilter2: function () {
      var actual;

      actual = getQueryResults("FOR i IN [1, 2, 3, 4] FOR j IN [1, 2, 3, 4] FILTER 2 < 0 RETURN i");
      assertEqual([ ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constant filter
////////////////////////////////////////////////////////////////////////////////

    testConstantFilter3: function () {
      var actual;

      actual = getQueryResults("FOR i IN [1, 2, 3, 4] FILTER 2 < 0 FOR j IN [1, 2, 3, 4] FILTER 2 < 0 RETURN i");
      assertEqual([ ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constant filter
////////////////////////////////////////////////////////////////////////////////

    testConstantFilter4: function () {
      var actual;

      actual = getQueryResults("LET l1 = (FOR i IN [1, 2, 3, 4] FILTER 3 < 0 RETURN i) LET l2 = (FOR i IN [1, 2, 3, 4] FILTER 3 < 0 RETURN i) RETURN { l1: l1, l2: l2 }");
      assertEqual([ { l1: [ ], l2: [ ] } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief stable sort after COLLECT
////////////////////////////////////////////////////////////////////////////////
    
    testStableSort1: function () {
      var data = [ 
        { "city" : "london", "order" : 4 }, 
        { "city" : "paris", "order" : 4 }, 
        { "city" : "london", "order" : 2 }, 
        { "city" : "paris", "order" : 3 }, 
        { "city" : "new york", "order" : 2 }, 
        { "city" : "london", "order" : 3 }, 
        { "city" : "new york", "order" : 4 }, 
        { "city" : "new york", "order" : 3 }, 
        { "city" : "paris", "order" : 1 }, 
        { "city" : "new york", "order" : 1 }, 
        { "city" : "paris", "order" : 2 }, 
        { "city" : "london", "order" : 1 }
      ];

      var actual, expected;

      expected = [ 
        { "city" : "london", "orders" : [ 4, 3, 2, 1 ] }, 
        { "city" : "new york", "orders" : [ 4, 3, 2, 1 ] }, 
        { "city" : "paris", "orders" : [ 4, 3, 2, 1 ] } 
      ];

      actual = getQueryResults("FOR value IN " + JSON.stringify(data) + " SORT value.order DESC COLLECT city = value.city INTO orders RETURN { city: city, orders: orders[*].value.order }");
      assertEqual(expected, actual);
      
      expected = [ 
        { "city" : "london", "orders" : [ 1, 2, 3, 4 ] }, 
        { "city" : "new york", "orders" : [ 1, 2, 3, 4 ] }, 
        { "city" : "paris", "orders" : [ 1, 2, 3, 4 ] } 
      ];

      actual = getQueryResults("FOR value IN " + JSON.stringify(data) + " SORT value.order ASC COLLECT city = value.city INTO orders RETURN { city: city, orders: orders[*].value.order }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief stable sort after COLLECT
////////////////////////////////////////////////////////////////////////////////
    
    testStableSort2: function () {
      var data = [ 
        { "city" : "london", "order" : 4 }, 
        { "city" : "paris", "order" : 4 }, 
        { "city" : "london", "order" : 2 }, 
        { "city" : "paris", "order" : 3 }, 
        { "city" : "new york", "order" : 2 }, 
        { "city" : "london", "order" : 3 }, 
        { "city" : "new york", "order" : 4 }, 
        { "city" : "new york", "order" : 3 }, 
        { "city" : "paris", "order" : 1 }, 
        { "city" : "new york", "order" : 1 }, 
        { "city" : "paris", "order" : 2 }, 
        { "city" : "london", "order" : 1 }
      ];

      var actual, expected;

      expected = [ 
        { "cities" : [ "paris", "new york", "london" ], "order" : 1 }, 
        { "cities" : [ "paris", "new york", "london" ], "order" : 2 }, 
        { "cities" : [ "paris", "new york", "london" ], "order" : 3 }, 
        { "cities" : [ "paris", "new york", "london" ], "order" : 4 } 
      ];

      actual = getQueryResults("FOR value IN " + JSON.stringify(data) + " SORT value.city DESC COLLECT order = value.order INTO cities RETURN { order: order, cities: cities[*].value.city }");
      assertEqual(expected, actual);
      
      expected = [ 
        { "cities" : [ "london", "new york", "paris" ], "order" : 4 }, 
        { "cities" : [ "london", "new york", "paris" ], "order" : 3 }, 
        { "cities" : [ "london", "new york", "paris" ], "order" : 2 }, 
        { "cities" : [ "london", "new york", "paris" ], "order" : 1 } 
      ];

      actual = getQueryResults("FOR value IN " + JSON.stringify(data) + " SORT value.city ASC COLLECT order = value.order INTO cities SORT order DESC RETURN { order: order, cities: cities[*].value.city }");
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQuerySimpleTestSuite);

return jsunity.done();

