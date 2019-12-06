/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function rangesSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeAsResult : function () {
      var actual = AQL_EXECUTE("RETURN 1..9").json;
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9 ], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeAsResultReversed : function () {
      var actual = AQL_EXECUTE("RETURN 9..1").json;
      assertEqual([ 9, 8, 7, 6, 5, 4, 3, 2, 1 ], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeInInExpressionWrong : function () {
      for (var i = 0; i <= 10; ++i) {
        var actual = AQL_EXECUTE("RETURN 1..9 IN " + i).json;
        assertFalse(actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeInInExpression : function () {
      for (var i = 0; i <= 10; ++i) {
        var actual = AQL_EXECUTE("RETURN " + i + " IN 1..9").json;
        var expected = (i !== 0 && i !== 10);
        assertEqual(expected, actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeInNotInExpression : function () {
      for (var i = 0; i <= 10; ++i) {
        var actual = AQL_EXECUTE("RETURN " + i + " NOT IN 1..9").json;
        var expected = (i === 0 || i === 10);
        assertEqual(expected, actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeInEqualityExpression : function () {
      var actual = AQL_EXECUTE("RETURN 1..9 == 1..9").json;
      assertTrue(actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as a function parameter
////////////////////////////////////////////////////////////////////////////////

    testRangeAsFunctionParameter1 : function () {
      var actual = AQL_EXECUTE("RETURN IS_STRING(1..73)").json;
      assertFalse(actual[0]);
      
      actual = AQL_EXECUTE("RETURN NOOPT(IS_STRING(1..73))").json;
      assertFalse(actual[0]);
      
      actual = AQL_EXECUTE("RETURN IS_ARRAY(1..73)").json;
      assertTrue(actual[0]);
      
      actual = AQL_EXECUTE("RETURN NOOPT(IS_ARRAY(1..73))").json;
      assertTrue(actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as a function parameter
////////////////////////////////////////////////////////////////////////////////

    testRangeAsFunctionParameter2 : function () {
      var actual = AQL_EXECUTE("RETURN MAX(1..73)").json;
      assertEqual(73, actual[0]);

      actual = AQL_EXECUTE("RETURN NOOPT(MAX(1..73))").json;
      assertEqual(73, actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as a function parameter
////////////////////////////////////////////////////////////////////////////////

    testRangeAsFunctionParameter3 : function () {
      var actual = AQL_EXECUTE("RETURN LENGTH(1..73)").json;
      assertEqual(73, actual[0]);
      
      actual = AQL_EXECUTE("RETURN NOOPT(LENGTH(1..73))").json;
      assertEqual(73, actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as dynamic function parameter
////////////////////////////////////////////////////////////////////////////////

    testFunctionRange : function () {
      var actual = AQL_EXECUTE("RETURN FLOOR(1) .. FLOOR(10)").json;
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], actual[0]);
      
      actual = AQL_EXECUTE("RETURN NOOPT(FLOOR(1) .. FLOOR(10))").json;
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as dynamic function parameter
////////////////////////////////////////////////////////////////////////////////

    testDynamicRanges1 : function () {
      var actual = AQL_EXECUTE("LET lower = PASSTHRU(23), upper = PASSTHRU(42) RETURN [ MIN(lower..upper), MAX(lower..upper) ]").json;
      assertEqual([ 23, 42 ], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range with a dynamic bound
////////////////////////////////////////////////////////////////////////////////

    testDynamicRanges2 : function () {
      var actual = AQL_EXECUTE("FOR i IN 1..4 RETURN 1..i").json;
      assertEqual([ [ 1 ], [ 1, 2 ], [ 1, 2, 3 ], [ 1, 2, 3, 4 ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range with a dynamic bound
////////////////////////////////////////////////////////////////////////////////

    testDynamicRanges3 : function () {
      var actual = AQL_EXECUTE("LET a = 1..4 RETURN MAX(a)").json;
      assertEqual([ 4 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess1 : function () {
      var actual = AQL_EXECUTE("LET a = 1..5 FOR i IN a RETURN a[0]").json;
      assertEqual([ 1, 1, 1, 1, 1 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess2 : function () {
      var actual = AQL_EXECUTE("LET a = 1..5 FOR i IN a RETURN a[4]").json;
      assertEqual([ 5, 5, 5, 5, 5 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess3 : function () {
      var actual = AQL_EXECUTE("LET a = 1..5 FOR i IN a RETURN a[-1]").json;
      assertEqual([ 5, 5, 5, 5, 5 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess4 : function () {
      var actual = AQL_EXECUTE("LET a = 1..5 FOR i IN a RETURN a[-2]").json;
      assertEqual([ 4, 4, 4, 4, 4 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess5 : function () {
      var actual = AQL_EXECUTE("LET a = 5..1 FOR i IN a RETURN a[0]").json;
      assertEqual([ 5, 5, 5, 5, 5 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess6 : function () {
      var actual = AQL_EXECUTE("LET a = 5..1 FOR i IN a RETURN a[4]").json;
      assertEqual([ 1, 1, 1, 1, 1 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess7 : function () {
      var actual = AQL_EXECUTE("LET a = 5..1 FOR i IN a RETURN a[-1]").json;
      assertEqual([ 1, 1, 1, 1, 1 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess8 : function () {
      var actual = AQL_EXECUTE("LET a = 5..1 FOR i IN a RETURN a[-2]").json;
      assertEqual([ 2, 2, 2, 2, 2 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple ranges
////////////////////////////////////////////////////////////////////////////////

    testMultipleRanges1 : function () {
      var actual = AQL_EXECUTE("FOR i IN 1..2 FOR j IN 3..4 RETURN i").json;
      assertEqual([ 1, 1, 2, 2 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple ranges
////////////////////////////////////////////////////////////////////////////////

    testMultipleRanges2 : function () {
      var actual = AQL_EXECUTE("FOR i IN 1..2 FOR j IN 3..4 RETURN [ i, j ]").json;
      assertEqual([ [ 1, 3 ], [ 1, 4 ], [ 2, 3 ], [ 2, 4 ] ], actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(rangesSuite);

return jsunity.done();

