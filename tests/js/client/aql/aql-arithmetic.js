/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, arithmetic operators
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

function ahuacatlArithmeticTestSuite () {

  return {

    testConstAttributeAccess : function () {
      var actual = getQueryResults("LET it = { a: 4, b: 5, c: 6 } RETURN [ it.a * 3, it.b * 4, it.c * 5 ]");
      assertEqual([ [ 4 * 3, 5 * 4, 6 * 5 ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlus1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN +0");
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN NOOPT(+0)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlus2 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN +1");
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN NOOPT(+1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlus3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN ++1");
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN NOOPT(++1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlus4 : function () {
      var expected = [ -5 ];
      var actual = getQueryResults("RETURN +-5");
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN NOOPT(+-5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlus5 : function () {
      var expected = [ 5.4 ];
      var actual = getQueryResults("RETURN +++5.4");
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN NOOPT(+++5.4)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlusNonNumeric : function () {
      assertEqual([ 0 ], getQueryResults("RETURN +null"));
      assertEqual([ 1 ], getQueryResults("RETURN +true"));
      assertEqual([ 0 ], getQueryResults("RETURN +false"));
      assertEqual([ 0 ], getQueryResults("RETURN +\"value\""));
      assertEqual([ 1 ], getQueryResults("RETURN +\"1\""));
      assertEqual([ -3 ], getQueryResults("RETURN +\"-3\""));
      assertEqual([ -3.4 ], getQueryResults("RETURN +\"-3.4\""));
      assertEqual([ 0 ], getQueryResults("RETURN +[ ]"));
      assertEqual([ 0 ], getQueryResults("RETURN +[ 0 ]"));
      assertEqual([ -34 ], getQueryResults("RETURN +[ -34 ]"));
      assertEqual([ 0 ], getQueryResults("RETURN +[ 1, 2 ]"));
      assertEqual([ 0 ], getQueryResults("RETURN +[ \"abc\" ]"));
      assertEqual([ 3 ], getQueryResults("RETURN +[ \"3\" ]"));
      assertEqual([ 0 ], getQueryResults("RETURN +{ }"));
      
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(+null)"));
      assertEqual([ 1 ], getQueryResults("RETURN NOOPT(+true)"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(+false)"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(+\"value\")"));
      assertEqual([ 1 ], getQueryResults("RETURN NOOPT(+\"1\")"));
      assertEqual([ -3 ], getQueryResults("RETURN NOOPT(+\"-3\")"));
      assertEqual([ -3.4 ], getQueryResults("RETURN NOOPT(+\"-3.4\")"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(+[ ])"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(+[ 0 ])"));
      assertEqual([ -34 ], getQueryResults("RETURN NOOPT(+[ -34 ])"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(+[ 1, 2 ])"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(+[ \"abc\" ])"));
      assertEqual([ 3 ], getQueryResults("RETURN NOOPT(+[ \"3\" ])"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(+{ })"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinus1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN -0");
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN NOOPT(-0)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinus2 : function () {
      var expected = [ -1 ];
      var actual = getQueryResults("RETURN -1");
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN NOOPT(-1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinus3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN --1");
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN NOOPT(--1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinus4 : function () {
      var expected = [ -5 ];
      var actual = getQueryResults("RETURN -+5");
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN NOOPT(-+5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinus5 : function () {
      var expected = [ -5.4 ];
      var actual = getQueryResults("RETURN ---5.4");
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN NOOPT(---5.4)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinusNonNumeric : function () {
      assertEqual([ 0 ], getQueryResults("RETURN -null"));
      assertEqual([ -1 ], getQueryResults("RETURN -true"));
      assertEqual([ 0 ], getQueryResults("RETURN -false"));
      assertEqual([ 0 ], getQueryResults("RETURN -\"value\""));
      assertEqual([ -1 ], getQueryResults("RETURN -\"1\""));
      assertEqual([ 3 ], getQueryResults("RETURN -\"-3\""));
      assertEqual([ 3.5 ], getQueryResults("RETURN -\"-3.5\""));
      assertEqual([ 0 ], getQueryResults("RETURN -[ ]"));
      assertEqual([ 0 ], getQueryResults("RETURN -[ 0 ]"));
      assertEqual([ 34 ], getQueryResults("RETURN -[ -34 ]"));
      assertEqual([ 0 ], getQueryResults("RETURN -[ 1, 2 ]"));
      assertEqual([ 0 ], getQueryResults("RETURN -[ \"abc\" ]"));
      assertEqual([ -3 ], getQueryResults("RETURN -[ \"3\" ]"));
      assertEqual([ 0 ], getQueryResults("RETURN -{ }"));
      
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(-null)"));
      assertEqual([ -1 ], getQueryResults("RETURN NOOPT(-true)"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(-false)"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(-\"value\")"));
      assertEqual([ -1 ], getQueryResults("RETURN NOOPT(-\"1\")"));
      assertEqual([ 3 ], getQueryResults("RETURN NOOPT(-\"-3\")"));
      assertEqual([ 3.5 ], getQueryResults("RETURN NOOPT(-\"-3.5\")"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(-[ ])"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(-[ 0 ])"));
      assertEqual([ 34 ], getQueryResults("RETURN NOOPT(-[ -34 ])"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(-[ 1, 2 ])"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(-[ \"abc\" ])"));
      assertEqual([ -3 ], getQueryResults("RETURN NOOPT(-[ \"3\" ])"));
      assertEqual([ 0 ], getQueryResults("RETURN NOOPT(-{ })"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinusPrecedence1 : function () {
      // uminus has higher precedence than <
      var expected = [ true ];
      var actual = getQueryResults("RETURN -5 < 0");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinusPrecedence2 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN -5 == -(5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinusPrecedence3 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN -5 == -10--5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary plus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryPlus1 : function () {
      var expected = [ 43.5 ];
      var actual = getQueryResults("RETURN 1 + 42.5");
      assertEqual(expected, actual);

      // Force the + to be executed at runtime
      actual = getQueryResults("RETURN NOOPT(FLOOR(1)) + 42.5");
      assertEqual(expected, actual);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary plus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryPlus2 : function () {
      var expected = [ -43.5 ];
      var actual = getQueryResults("RETURN -1 + -42.5");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FLOOR(-1)) + -42.5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary plus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryPlus3 : function () {
      var expected = [ 41.5 ];
      var actual = getQueryResults("RETURN -1 + +42.5");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FLOOR(-1)) + +42.5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary plus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryPlus4 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN -1 + +42.5 == +42.5 + -1");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FLOOR(-1)) + +42.5 == +42.5 + NOOPT(FLOOR(-1))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary plus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryPlusNonNumeric : function () {
      var buildQuery = function(nr, left, right) {
        switch (nr) {
          case 0:
            return `RETURN ${left} + ${right}`;
          case 1:
          return `RETURN NOOPT(${left}) + ${right}`;
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 2; ++i) {
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "null")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "false")));
        assertEqual([ 2 ], getQueryResults(buildQuery(i, "1", "true")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "\"0\"")));
        assertEqual([ 3 ], getQueryResults(buildQuery(i, "1", "\"2\"")));
        assertEqual([ 3.5 ], getQueryResults(buildQuery(i, "1", "\"2.5\"")));
        assertEqual([ -1.5 ], getQueryResults(buildQuery(i, "1", "\"-2.5\"")));
        assertEqual([ 43 ], getQueryResults(buildQuery(i, "1", "\"42\"")));
        assertEqual([ -16 ], getQueryResults(buildQuery(i, "1", "\"-17\"")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "[ ]")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "[ 0 ]")));
        assertEqual([ 5 ], getQueryResults(buildQuery(i, "1", "[ 4 ]")));
        assertEqual([ -3 ], getQueryResults(buildQuery(i, "1", "[ -4 ]")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "{ }")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "null", "1")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "false", "1")));
        assertEqual([ 2 ], getQueryResults(buildQuery(i, "true", "1")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "\"0\"", "1")));
        assertEqual([ 24 ], getQueryResults(buildQuery(i, "\"23\"", "1")));
        assertEqual([ -8 ], getQueryResults(buildQuery(i, "\"-9\"", "1")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "[ ]", "1")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "[ 0 ]", "1")));
        assertEqual([ 5 ], getQueryResults(buildQuery(i, "[ 4 ]", "1")));
        assertEqual([ -3 ], getQueryResults(buildQuery(i, "[ -4 ]", "1")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "{ }", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"a\"", "\"b\"")));
        assertEqual([ 3 ], getQueryResults(buildQuery(i, "\"1\"", "\"2\"")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "\"\"")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "\"\"", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"\"", "\"\"")));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary minus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMinus1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN -1 - -1");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(-1) - -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary minus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMinus2 : function () {
      var expected = [ -5.5 ];
      var actual = getQueryResults("RETURN -2.25 - 3.25");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(-2.25) - 3.25");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary minus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMinus3 : function () {
      var expected = [ -1 ];
      var actual = getQueryResults("RETURN 2.25 - 3.25");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(2.25) - 3.25");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary minus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMinusNonNumeric : function () {
      var buildQuery = function(nr, left, right) {
        switch (nr) {
          case 0:
            return `RETURN ${left} - ${right}`;
          case 1:
            return `RETURN NOOPT(${left}) - ${right}`;
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 2; ++i) {
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "null")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "false")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "1", "true")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "\"0\"")));
        assertEqual([ -41 ], getQueryResults(buildQuery(i, "1", "\"42\"")));
        assertEqual([ 18 ], getQueryResults(buildQuery(i, "1", "\"-17\"")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "\"abc\"")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "[ ]")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "[ 0 ]")));
        assertEqual([ -3 ], getQueryResults(buildQuery(i, "1", "[ 4 ]")));
        assertEqual([ 5 ], getQueryResults(buildQuery(i, "1", "[ -4 ]")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "{ }")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "null", "1")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "false", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "true", "1")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "\"0\"", "1")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "\"0\"", "\"1\"")));
        assertEqual([ 22 ], getQueryResults(buildQuery(i, "\"23\"", "1")));
        assertEqual([ -10 ], getQueryResults(buildQuery(i, "\"-9\"", "1")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "\"abc\"", "1")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "\"abc\"")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "[ ]", "1")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "[ 0 ]", "1")));
        assertEqual([ 3 ], getQueryResults(buildQuery(i, "[ 4 ]", "1")));
        assertEqual([ 3 ], getQueryResults(buildQuery(i, "[ \"4\" ]", "1")));
        assertEqual([ -8 ], getQueryResults(buildQuery(i, "[ \"4\" ]", "\"12\"")));
        assertEqual([ 4 ], getQueryResults(buildQuery(i, "[ \"4\" ]", "\"abc\"")));

        assertEqual([ -5 ], getQueryResults(buildQuery(i, "[ -4 ]", "1")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "{ }", "1")));
        assertEqual([ -1 ], getQueryResults(buildQuery(i, "{ }", "\"1\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"a\"", "\"b\"")));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN 0 * 100");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(0) * 100");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication2 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN 100 * 0");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(100) * 0");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication3 : function () {
      var expected = [ 37 ];
      var actual = getQueryResults("RETURN 1 * 37");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(1) * 37");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication4 : function () {
      var expected = [ 37 ];
      var actual = getQueryResults("RETURN 37 * 1");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(37) * 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication5 : function () {
      var expected = [ 76.5 ];
      var actual = getQueryResults("RETURN 9 * 8.5");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(9) * 8.5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication6 : function () {
      var expected = [ -14 ];
      var actual = getQueryResults("RETURN -4 * 3.5");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(-4) * 3.5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication7 : function () {
      var expected = [ 14 ];
      var actual = getQueryResults("RETURN -4 * -3.5");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(-4) * -3.5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplicationNonNumeric : function () {
      var buildQuery = function(nr, left, right) {
        switch (nr) {
          case 0:
            return `RETURN ${left} * ${right}`;
          case 1:
          return `RETURN NOOPT(${left}) * ${right}`;
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 2; ++i) {
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "1", "null")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "1", "false")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "true")));
        assertEqual([ 37 ], getQueryResults(buildQuery(i, "37", "true")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "1", "\"0\"")));
        assertEqual([ 42 ], getQueryResults(buildQuery(i, "1", "\"42\"")));
        assertEqual([ -17 ], getQueryResults(buildQuery(i, "1", "\"-17\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "1", "[ ]")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "1", "[ 0 ]")));
        assertEqual([ 4 ], getQueryResults(buildQuery(i, "1", "[ 4 ]")));
        assertEqual([ -4 ], getQueryResults(buildQuery(i, "1", "[ -4 ]")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "1", "{ }")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "null", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "false", "1")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "true", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"0\"", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"0\"", "\"1\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"abc\"", "2")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "2", "\"abc\"")));
        assertEqual([ 23 ], getQueryResults(buildQuery(i, "\"23\"", "1")));
        assertEqual([ -9 ], getQueryResults(buildQuery(i, "\"-9\"", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "[ ]", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "[ 0 ]", "1")));
        assertEqual([ 4 ], getQueryResults(buildQuery(i, "[ 4 ]", "1")));
        assertEqual([ -4 ], getQueryResults(buildQuery(i, "[ -4 ]", "1")));
        assertEqual([ 36 ], getQueryResults(buildQuery(i, "\"3\"", "12")));
        assertEqual([ 36 ], getQueryResults(buildQuery(i, "\"3\"", "\"12\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "{ }", "1")));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN 0 / 1");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(0) / 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision2 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN 1 / 1");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(1) / 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN 1.25 / 1.25");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(1.25) / 1.25");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision4 : function () {
      var expected = [ 12 ];
      var actual = getQueryResults("RETURN 144 / 12");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(144) / 12");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision5 : function () {
      var expected = [ -12 ];
      var actual = getQueryResults("RETURN -144 / 12");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(-144) / 12");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision6 : function () {
      var expected = [ -12 ];
      var actual = getQueryResults("RETURN 144 / -12");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(144) / -12");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision7 : function () {
      var expected = [ 12 ];
      var actual = getQueryResults("RETURN -144 / -12");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(-144) / -12");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision8 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN 1 / 0.5 / 2");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(1) / NOOPT(0.5) / 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivisionNonNumeric : function () {
      var buildQuery = function(nr, left, right) {
        switch (nr) {
          case 0:
            return `RETURN ${left} / ${right}`;
          case 1:
            return `RETURN NOOPT(${left}) / ${right}`;
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 2; ++i) {
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "(1 - 1)")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "0")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1","0.0")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "(1 / 0)")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "null")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "false")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "true")));
        assertEqual([ 37 ], getQueryResults(buildQuery(i, "37", "true")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "\"0\"")));
        assertEqual([ 0.25 ], getQueryResults(buildQuery(i, "1", "\"4\"")));
        assertEqual([ -2 ], getQueryResults(buildQuery(i, "34", "\"-17\"")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "[ ]")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "[ 0 ]")));
        assertEqual([ 0.25 ], getQueryResults(buildQuery(i, "1", "[ 4 ]")));
        assertEqual([ -0.25 ], getQueryResults(buildQuery(i, "1", "[ -4 ]")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "{ }")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "null", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "false", "1")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "true", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"0\"", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"0\"", "\"1\"")));
        assertEqual([ 23 ], getQueryResults(buildQuery(i, "\"23\"", "1")));
        assertEqual([ -9 ], getQueryResults(buildQuery(i, "\"-9\"", "1")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"abc\"", "\"abc\"")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "\"abc\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "[ ]", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "[ 0 ]", "1")));
        assertEqual([ 4 ], getQueryResults(buildQuery(i, "[ 4 ]", "1")));
        assertEqual([ -4 ], getQueryResults(buildQuery(i, "[ -4 ]", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "[ -4, -3 ]", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "[ \"abc\" ]", "1")));
        assertEqual([ 2 ], getQueryResults(buildQuery(i, "[ \"2\" ]", "1")));
        assertEqual([ 3 ], getQueryResults(buildQuery(i, "[ \"12\" ]", "4")));
        assertEqual([ 3 ], getQueryResults(buildQuery(i, "\"12\"", "4")));
        assertEqual([ 3 ], getQueryResults(buildQuery(i, "\"12\"", "\"4\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "{ }", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "{ }", "\"1\"")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "0")));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary modulus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryModulusInvalid : function () {
      var buildQuery = function(nr, left, right) {
        switch (nr) {
          case 0:
            return `RETURN ${left} % ${right}`;
          case 1:
          return `RETURN NOOPT(${left}) % ${right}`;
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 2; ++i) {
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "(1 - 1)")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "0")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "0.0")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "(1 / 0)")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "null")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "false")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "1", "true")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "37", "true")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "\"0\"")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "\"4\"")));
        assertEqual([ 3 ], getQueryResults(buildQuery(i, "7", "\"4\"")));
        assertEqual([ 3 ], getQueryResults(buildQuery(i, "7", "\"-4\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "34", "\"-17\"")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "35", "\"17\"")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "35", "\"-17\"")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "[ ]")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "[ 0 ]")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "13", "[ 4 ]")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "1", "[ -4 ]")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "13", "[ -4 ]")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "1", "{ }")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "null", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "false", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "true", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"0\"", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"0\"", "\"1\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"23\"", "1")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "\"23\"", "2")));
        assertEqual([ 2 ], getQueryResults(buildQuery(i, "\"23\"", "3")));
        assertEqual([ 2 ], getQueryResults(buildQuery(i, "\"23\"", "7")));
        assertEqual([ 11 ], getQueryResults(buildQuery(i, "\"23\"", "12")));
        assertEqual([ 11 ], getQueryResults(buildQuery(i, "\"23\"", "\"12\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"-9\"", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "[ ]", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "[ 0 ]", "1")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "[ 4 ]", "1")));
        assertEqual([ 1 ], getQueryResults(buildQuery(i, "[ 4 ]", "3")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "[ -4 ]", "1")));
        assertEqual([ 4 ], getQueryResults(buildQuery(i, "4", "\"5\"")));
        assertEqual([ 5 ], getQueryResults(buildQuery(i, "\"12\"", "7")));
        assertEqual([ 5 ], getQueryResults(buildQuery(i, "\"12\"", "\"7\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "\"abc\"", "2")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "\"abc\"", "\"abc\"")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "2", "\"abc\"")));
        assertEqual([ 0 ], getQueryResults(buildQuery(i, "{ }", "1")));
        assertEqual([ null ], getQueryResults(buildQuery(i, "4", "0")));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence1 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN 1 * 0 - 1 / -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence2 : function () {
      var expected = [ 328 ];
      var actual = getQueryResults("RETURN 9 * 9 * 4 - -4 * -1 / -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence3 : function () {
      var expected = [ -20 ];
      var actual = getQueryResults("RETURN -3 + -6 + 8 - 10 + -9");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence4 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN 6 * 3 * 0 / 8");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence5 : function () {
      var expected = [ 66 ];
      var actual = getQueryResults("RETURN 8 + -7 / -7 - 5 * -10 + 7");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence6 : function () {
      var expected = [ 8 ];
      var actual = getQueryResults("RETURN 7 + 7 + -5 + -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence7 : function () {
      var expected = [ 13 ];
      var actual = getQueryResults("RETURN 0 / -2 + 9 + 4");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence8 : function () {
      var expected = [ 8 ];
      var actual = getQueryResults("RETURN 6 - 0 + 5 - 3");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence9 : function () {
      var expected = [ 4 ];
      var actual = getQueryResults("RETURN 10 / 5 - -5 + 2 - 4 - 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence10 : function () {
      var expected = [ 14 ];
      var actual = getQueryResults("RETURN -4 * -4 + 4 - 4 - -3 + -5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence11 : function () {
      var expected = [ -11 ];
      var actual = getQueryResults("RETURN -9 - 8 * -6 / -4 * 1 / 6");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence12 : function () {
      var expected = [ 332 ];
      var actual = getQueryResults("RETURN -4 * -8 * 10 + 2 - -10");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence13 : function () {
      var expected = [ 270 ];
      var actual = getQueryResults("RETURN -2 * 7 * -5 + -10 * 5 * -4");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence14 : function () {
      var expected = [ -111 ];
      var actual = getQueryResults("RETURN -10 * 9 - -8 * -2 - -5 * -10 / 10");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence15 : function () {
      var expected = [ 2 ];
      var actual = getQueryResults("RETURN 7 * 0 * -9 / 10 + 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence16 : function () {
      var expected = [ 9 ];
      var actual = getQueryResults("RETURN 7 - 5 - 2 - -9");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence17 : function () {
      var expected = [ -2 ];
      var actual = getQueryResults("RETURN -9 - -9 + -8 + 6 * 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence18 : function () {
      var expected = [ -9 ];
      var actual = getQueryResults("RETURN -1 * 9 - 0 / -4");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence19 : function () {
      var expected = [ -112 ];
      var actual = getQueryResults("RETURN -4 / -1 * -4 / 1 * 7");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence20 : function () {
      var expected = [ 7 ];
      var actual = getQueryResults("RETURN 5 - 7 - -9 + 0");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence21 : function () {
      var expected = [ -34 ];
      var actual = getQueryResults("RETURN -8 + 6 + -7 * 4 - 4");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence22 : function () {
      var expected = [ -41 ];
      var actual = getQueryResults("RETURN -7 - 7 * 5 - 8 / -8");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence23 : function () {
      var expected = [ -25 ];
      var actual = getQueryResults("RETURN 6 + -1 * -10 * -4 - 1 * -9");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence24 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN 5 * -1 + -5 + -1 * -10");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence25 : function () {
      var expected = [ 30 ];
      var actual = getQueryResults("RETURN -9 * -3 - 9 / -3");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence26 : function () {
      var expected = [ -8 ];
      var actual = getQueryResults("RETURN 0 + -4 + -2 / -1 - -6 / -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence27 : function () {
      var expected = [ 198 ];
      var actual = getQueryResults("RETURN -9 * 3 * -9 - -4 * -8 - 8 - 5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence28 : function () {
      var expected = [ -1 ];
      var actual = getQueryResults("RETURN -1 * 4 * 0 + -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence29 : function () {
      var expected = [ 9 ];
      var actual = getQueryResults("RETURN -2 + -6 - -9 + 8 + 0 * -8");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence30 : function () {
      var expected = [ 65 ];
      var actual = getQueryResults("RETURN -8 * -8 - 5 - 6 / -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence31 : function () {
      var expected = [ 12 ];
      var actual = getQueryResults("RETURN 4 + -8 + -9 + 5 * 5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence32 : function () {
      var expected = [ -4 ];
      var actual = getQueryResults("RETURN 0 * 4 * -1 + -1 - 3");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence33 : function () {
      var expected = [ 293 ];
      var actual = getQueryResults("RETURN 9 * 4 * 8 - -5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence34 : function () {
      var expected = [ 5 ];
      var actual = getQueryResults("RETURN 4 * 2 + 3 * -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence35 : function () {
      var expected = [ 60 ];
      var actual = getQueryResults("RETURN 5 * 0 * 10 - 6 / 1 * -10");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence36 : function () {
      var expected = [ 600 ];
      var actual = getQueryResults("RETURN 4 * 5 - 9 * -8 * 8 - -4 - 0");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence37 : function () {
      var expected = [ 114 ];
      var actual = getQueryResults("RETURN -5 * 7 * -1 - 9 * -10 + -3 - 8");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence38 : function () {
      var expected = [ 22 ];
      var actual = getQueryResults("RETURN 6 - 10 * -2 / 10 * 8 - 0 / -8");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence39 : function () {
      var expected = [ 7 ];
      var actual = getQueryResults("RETURN -5 + -10 * -6 + 6 + -6 * 9");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence40 : function () {
      var expected = [ 54 ];
      var actual = getQueryResults("RETURN -1 - -1 + 4 - 10 * -5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence41 : function () {
      var expected = [ -33 ];
      var actual = getQueryResults("RETURN -9 - 2 + 2 - 0 + -4 * 6");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence42 : function () {
      var expected = [ 45 ];
      var actual = getQueryResults("RETURN -5 * -9 - 5 * -7 * 0 * 5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence43 : function () {
      var expected = [ -78 ];
      var actual = getQueryResults("RETURN -4 * 9 - 6 * 5 - 9 + -6 + 3");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence44 : function () {
      var expected = [ -1 ];
      var actual = getQueryResults("RETURN -10 / -2 + -8 + -4 - -6");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence45 : function () {
      var expected = [ 45 ];
      var actual = getQueryResults("RETURN 3 + 8 - 3 * -6 - -5 + 7 - -4");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence46 : function () {
      var expected = [ 68 ];
      var actual = getQueryResults("RETURN -1 - -9 * 9 + -5 - 7");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence47 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN -2 - -10 + -7 + 0 / 7");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence48 : function () {
      var expected = [ -20 ];
      var actual = getQueryResults("RETURN 10 * -5 / 5 + -8 + -8 + 6");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence49 : function () {
      var expected = [ 6 ];
      var actual = getQueryResults("RETURN 7 + -4 / -2 - 0 + 4 + -7");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence50 : function () {
      var expected = [ -140 ];
      var actual = getQueryResults("RETURN 5 * 1 * 7 * -2 * -2 * -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence51 : function () {
      var expected = [ -281 ];
      var actual = getQueryResults("RETURN 4 * 10 * -7 - 9 - -8");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence52 : function () {
      var expected = [ 34 ];
      var actual = getQueryResults("RETURN 3 * 8 - -1 - -9");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence53 : function () {
      var expected = [ 10 ];
      var actual = getQueryResults("RETURN -2 + -1 + 4 + 9");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence54 : function () {
      var expected = [ -23 ];
      var actual = getQueryResults("RETURN 1 - -6 * -5 + 6 + 0 * 3");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence55 : function () {
      var expected = [ -6 ];
      var actual = getQueryResults("RETURN 0 * 0 - 0 + -6");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence56 : function () {
      var expected = [ 23 ];
      var actual = getQueryResults("RETURN 10 - -4 * 7 - 10 - 0 + -5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence57 : function () {
      var expected = [ 11 ];
      var actual = getQueryResults("RETURN 1 - 6 - -4 - -4 * 5 + -4 - 4");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence58 : function () {
      var expected = [ 8 ];
      var actual = getQueryResults("RETURN 7 - 9 - -8 + 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPrecedence59 : function () {
      var expected = [ 40 ];
      var actual = getQueryResults("RETURN -7 - -4 - -2 + 10 * 5 - 9");
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlArithmeticTestSuite);

return jsunity.done();

