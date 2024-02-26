/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
const db = require('internal').db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function rangesSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeAsResult : function () {
      var actual = db._query("RETURN 1..9").toArray();
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9 ], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeAsResultReversed : function () {
      var actual = db._query("RETURN 9..1").toArray();
      assertEqual([ 9, 8, 7, 6, 5, 4, 3, 2, 1 ], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeInInExpressionWrong : function () {
      for (var i = 0; i <= 10; ++i) {
        var actual = db._query("RETURN 1..9 IN " + i).toArray();
        assertFalse(actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeInInExpression : function () {
      for (var i = 0; i <= 10; ++i) {
        var actual = db._query("RETURN " + i + " IN 1..9").toArray();
        var expected = (i !== 0 && i !== 10);
        assertEqual(expected, actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeInNotInExpression : function () {
      for (var i = 0; i <= 10; ++i) {
        var actual = db._query("RETURN " + i + " NOT IN 1..9").toArray();
        var expected = (i === 0 || i === 10);
        assertEqual(expected, actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as result
////////////////////////////////////////////////////////////////////////////////

    testRangeInEqualityExpression : function () {
      var actual = db._query("RETURN 1..9 == 1..9").toArray();
      assertTrue(actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as a function parameter
////////////////////////////////////////////////////////////////////////////////

    testRangeAsFunctionParameter1 : function () {
      var actual = db._query("RETURN IS_STRING(1..73)").toArray();
      assertFalse(actual[0]);
      
      actual = db._query("RETURN NOOPT(IS_STRING(1..73))").toArray();
      assertFalse(actual[0]);
      
      actual = db._query("RETURN IS_ARRAY(1..73)").toArray();
      assertTrue(actual[0]);
      
      actual = db._query("RETURN NOOPT(IS_ARRAY(1..73))").toArray();
      assertTrue(actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as a function parameter
////////////////////////////////////////////////////////////////////////////////

    testRangeAsFunctionParameter2 : function () {
      var actual = db._query("RETURN MAX(1..73)").toArray();
      assertEqual(73, actual[0]);

      actual = db._query("RETURN NOOPT(MAX(1..73))").toArray();
      assertEqual(73, actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as a function parameter
////////////////////////////////////////////////////////////////////////////////

    testRangeAsFunctionParameter3 : function () {
      var actual = db._query("RETURN LENGTH(1..73)").toArray();
      assertEqual(73, actual[0]);
      
      actual = db._query("RETURN NOOPT(LENGTH(1..73))").toArray();
      assertEqual(73, actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as dynamic function parameter
////////////////////////////////////////////////////////////////////////////////

    testFunctionRange : function () {
      var actual = db._query("RETURN FLOOR(1) .. FLOOR(10)").toArray();
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], actual[0]);
      
      actual = db._query("RETURN NOOPT(FLOOR(1) .. FLOOR(10))").toArray();
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range as dynamic function parameter
////////////////////////////////////////////////////////////////////////////////

    testDynamicRanges1 : function () {
      var actual = db._query("LET lower = PASSTHRU(23), upper = PASSTHRU(42) RETURN [ MIN(lower..upper), MAX(lower..upper) ]").toArray();
      assertEqual([ 23, 42 ], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range with a dynamic bound
////////////////////////////////////////////////////////////////////////////////

    testDynamicRanges2 : function () {
      var actual = db._query("FOR i IN 1..4 RETURN 1..i").toArray();
      assertEqual([ [ 1 ], [ 1, 2 ], [ 1, 2, 3 ], [ 1, 2, 3, 4 ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range with a dynamic bound
////////////////////////////////////////////////////////////////////////////////

    testDynamicRanges3 : function () {
      var actual = db._query("LET a = 1..4 RETURN MAX(a)").toArray();
      assertEqual([ 4 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess1 : function () {
      var actual = db._query("LET a = 1..5 FOR i IN a RETURN a[0]").toArray();
      assertEqual([ 1, 1, 1, 1, 1 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess2 : function () {
      var actual = db._query("LET a = 1..5 FOR i IN a RETURN a[4]").toArray();
      assertEqual([ 5, 5, 5, 5, 5 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess3 : function () {
      var actual = db._query("LET a = 1..5 FOR i IN a RETURN a[-1]").toArray();
      assertEqual([ 5, 5, 5, 5, 5 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess4 : function () {
      var actual = db._query("LET a = 1..5 FOR i IN a RETURN a[-2]").toArray();
      assertEqual([ 4, 4, 4, 4, 4 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess5 : function () {
      var actual = db._query("LET a = 5..1 FOR i IN a RETURN a[0]").toArray();
      assertEqual([ 5, 5, 5, 5, 5 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess6 : function () {
      var actual = db._query("LET a = 5..1 FOR i IN a RETURN a[4]").toArray();
      assertEqual([ 1, 1, 1, 1, 1 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess7 : function () {
      var actual = db._query("LET a = 5..1 FOR i IN a RETURN a[-1]").toArray();
      assertEqual([ 1, 1, 1, 1, 1 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range access
////////////////////////////////////////////////////////////////////////////////

    testRangesAccess8 : function () {
      var actual = db._query("LET a = 5..1 FOR i IN a RETURN a[-2]").toArray();
      assertEqual([ 2, 2, 2, 2, 2 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple ranges
////////////////////////////////////////////////////////////////////////////////

    testMultipleRanges1 : function () {
      var actual = db._query("FOR i IN 1..2 FOR j IN 3..4 RETURN i").toArray();
      assertEqual([ 1, 1, 2, 2 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple ranges
////////////////////////////////////////////////////////////////////////////////

    testMultipleRanges2 : function () {
      var actual = db._query("FOR i IN 1..2 FOR j IN 3..4 RETURN [ i, j ]").toArray();
      assertEqual([ [ 1, 3 ], [ 1, 4 ], [ 2, 3 ], [ 2, 4 ] ], actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(rangesSuite);

return jsunity.done();

