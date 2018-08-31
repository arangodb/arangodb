/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertException */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, simple non-collection-based queries
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
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryNonCollectionTestSuite () {

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
/// @brief multiple subqueries
////////////////////////////////////////////////////////////////////////////////

    testMultipleSubqueries1 : function () {
      var expected = [ [ [ 2, 4 ], [ 6, 8 ] ] ];
      var actual = getQueryResults("LET a = (FOR i IN [ 1, 2 ] RETURN i * 2), b = (FOR i IN [ 3, 4 ] RETURN i * 2) RETURN [ a, b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multiple subqueries
////////////////////////////////////////////////////////////////////////////////

    testMultipleSubqueries2 : function () {
      var expected = [ [ [ 2, 4 ], [ 6, 8 ] ] ];
      var actual = getQueryResults("LET a = (FOR i IN [ 1, 2 ] RETURN i * 2) LET b = (FOR i IN [ 3, 4 ] RETURN i * 2) RETURN [ a, b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test deferred evaluation
////////////////////////////////////////////////////////////////////////////////

    testDeferredEval1 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FILTER year > 2013 && FAIL() RETURN year");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test deferred evaluation
////////////////////////////////////////////////////////////////////////////////

    testDeferredEval2 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FILTER year > 2013 RETURN FAIL()");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test deferred evaluation
////////////////////////////////////////////////////////////////////////////////

    testDeferredEval3 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FILTER year > 2013 && FAIL() RETURN FAIL()");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test deferred evaluation
////////////////////////////////////////////////////////////////////////////////

    testDeferredEval4 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FILTER year > 2020 LET x = (FOR i IN [ 1, 2 ] RETURN FAIL()) RETURN year");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test deferred evaluation
////////////////////////////////////////////////////////////////////////////////

    testDeferredEval5 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FILTER year > 2020 LET x = (FOR i IN [ 1, 2 ] RETURN FAIL()) RETURN FAIL()");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test deferred evaluation
////////////////////////////////////////////////////////////////////////////////

    testDeferredEval6 : function () {
      assertException(function() { getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FILTER year < 2020 RETURN FAIL()"); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test deferred evaluation
////////////////////////////////////////////////////////////////////////////////

    testDeferredEval7 : function () {
      assertException(function() { getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FILTER year < 2020 LET x = (FOR i IN [ 1, 2 ] RETURN FAIL()) RETURN year"); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single value from a list
////////////////////////////////////////////////////////////////////////////////

    testListValueQuery : function () {
      var expected = [ 2010, 2011, 2012 ];

      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] return year");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return two values from a list
////////////////////////////////////////////////////////////////////////////////

    testListDocumentQuery : function () {
      var expected = [ { "days" : 365, "year" : 2010 }, { "days" : 365, "year" : 2011 }, { "days" : 366, "year" : 2012 } ];

      var actual = getQueryResults("FOR year IN [ { \"year\" : 2010, \"days\" : 365 } , { \"year\" : 2011, \"days\" : 365 }, { \"year\" : 2012, \"days\" : 366 } ] return year");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single value from a sorted list
////////////////////////////////////////////////////////////////////////////////
    
    testListValueQuerySort : function () {
      var expected = [ 2015, 2014, 2013, 2012, 2011, 2010 ];

      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012, 2013, 2014, 2015 ] SORT year DESC return year");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single value from a sorted limited list
////////////////////////////////////////////////////////////////////////////////

    testListValueQuerySortLimit : function () {
      var expected = [ 2013, 2012, 2011 ];

      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012, 2013, 2014, 2015 ] SORT year DESC LIMIT 2,3 return year");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a calculated from a list
////////////////////////////////////////////////////////////////////////////////

    testListCalculated : function () {
      var expected = [ { "anno" : 2010, "isLeapYear" : false }, { "anno" : 2011, "isLeapYear" : false }, { "anno" : 2012, "isLeapYear" : true }, { "anno" : 2013, "isLeapYear" : false }, { "anno" : 2014, "isLeapYear" : false } ];
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012, 2013, 2014 ] return { \"anno\" : year, \"isLeapYear\" : (year%4==0 && year%100!=0 || year%400==1) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return values from nested for loops
////////////////////////////////////////////////////////////////////////////////

    testNestedFor1 : function () {
      var expected = [ { "q" : 1, "y": 2010 }, { "q" : 1, "y": 2011 }, { "q" : 1, "y": 2012 }, { "q" : 2, "y": 2010 }, { "q" : 2, "y": 2011 }, { "q" : 2, "y": 2012 }, { "q" : 3, "y": 2010 }, { "q" : 3, "y": 2011 }, { "q" : 3, "y": 2012 }, { "q" : 4, "y": 2010 }, { "q" : 4, "y": 2011 }, { "q" : 4, "y": 2012 } ];
      var actual = getQueryResults("FOR quarter IN [ 1, 2, 3, 4 ] FOR year IN [ 2010, 2011, 2012 ] return { \"q\" : quarter, \"y\" : year }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return values from nested for loops
////////////////////////////////////////////////////////////////////////////////

    testNestedFor2 : function () {
      var expected = [ { "q" : 1, "y": 2010 }, { "q" : 2, "y": 2010 }, { "q" : 3, "y": 2010 }, { "q" : 4, "y": 2010 }, { "q" : 1, "y": 2011 }, { "q" : 2, "y": 2011 }, { "q" : 3, "y": 2011 }, { "q" : 4, "y": 2011 }, { "q" : 1, "y": 2012 }, { "q" : 2, "y": 2012 }, { "q" : 3, "y": 2012 }, { "q" : 4, "y": 2012 } ];
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] for quarter IN [ 1, 2, 3, 4 ] return { \"q\" : quarter, \"y\" : year }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return values from nested subqueries
////////////////////////////////////////////////////////////////////////////////

    testSubqueryFor1 : function () {
      var expected = [ 4, 5, 6 ];
      var actual = getQueryResults("FOR i IN ((FOR j IN ((FOR k IN ((FOR l IN [1,2,3] RETURN l+1)) RETURN k+1)) RETURN j+1)) RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return data from nested subqueries
////////////////////////////////////////////////////////////////////////////////

    testSubqueryFor2 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN (FOR j IN [1,2,3] RETURN j) RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return values from let
////////////////////////////////////////////////////////////////////////////////

    testNestedLet1 : function () {
      var expected = [ 2010, 2011, 2012 ];
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] let quarters = ((for quarter IN [ 1, 2, 3, 4 ] return quarter)) RETURN year");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return values from let
////////////////////////////////////////////////////////////////////////////////

    testNestedLet2 : function () {
      var expected = [ [ 1, 2, 3, 4 ], [ 1, 2, 3, 4 ], [ 1, 2, 3, 4 ] ];
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] let quarters = ((for quarter IN [ 1, 2, 3, 4 ] return quarter)) RETURN quarters");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return values from let
////////////////////////////////////////////////////////////////////////////////

    testNestedLet3 : function () {
      var expected = [{ "a" : 1, "b" : [4, 5, 6], "c" : [7, 8, 9] }, { "a" : 2, "b" : [5, 6, 7], "c" : [8, 9, 10] }, { "a" : 3, "b" : [6, 7, 8], "c" : [9, 10, 11] }];
      var actual = getQueryResults("FOR i IN [1,2,3] LET j=(FOR x IN [3,4,5] RETURN x+i) LET k=(FOR y IN [6,7,8] RETURN y+i) RETURN { \"a\": i, \"b\" : j, \"c\" : k }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return values from let
////////////////////////////////////////////////////////////////////////////////

    testNestedLet4 : function () {
      var expected = [[[9, 8, 7], [9, 8, 7]], [[9, 8, 7], [9, 8, 7]], [[9, 8, 7], [9, 8, 7]]];
      var actual = getQueryResults("FOR i IN [1,2,3] LET j=(FOR x IN [2010,2011] LET k=(FOR y IN [9,8,7] RETURN y) RETURN k) RETURN j");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return values from simple let
////////////////////////////////////////////////////////////////////////////////

    testSimpleLet1 : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("FOR i IN [1,2,3] LET j=i RETURN j");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return values from simple let
////////////////////////////////////////////////////////////////////////////////

    testSimpleLet2 : function () {
      var expected = [ "the fox", "the fox", "the fox" ];
      var actual = getQueryResults("FOR i IN [1,2,3] LET j=\"the fox\" RETURN j");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return asterisk
////////////////////////////////////////////////////////////////////////////////

    testAsterisk : function () {
      var expected = [ { "r" : 2010, "x" : [ [ { "name" : "a" }, { "name" : "b" } ], [ { "name" : "c" }, { "name" : "d" } ] ] }, { "r" : 2011, "x" : [ [ { "name" : "a" }, { "name" : "b" } ], [ { "name" : "c" }, { "name" : "d" } ] ] }, { "r" : 2011, "x" : [ [ { "name" : "a" }, { "name" : "b" } ], [ { "name" : "c" }, { "name" : "d" } ] ] } ];
      var actual = getQueryResults("FOR r IN [ 2010, 2011, 2011] LET x = ((FOR f IN [ { \"names\" : [ { \"name\" : \"a\" }, { \"name\" : \"b\" } ] }, { \"names\": [ { \"name\" : \"c\" }, { \"name\": \"d\" } ] } ] return f)) return { \"r\" : r, \"x\" : x[*].names }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nesting (issue #97)
////////////////////////////////////////////////////////////////////////////////

    testMultiNesting : function () {
      var expected = [ 1 ]; // we're only interested in whether the below query can be parsed properly

      var actual = getQueryResults("FOR r IN [ 1 ] LET f = (FOR x IN [ 1 ] FILTER 1 == 1 FOR y IN [ 1 ] FOR z IN [ 1 ] RETURN 1) RETURN 1");
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryNonCollectionTestSuite);

return jsunity.done();

