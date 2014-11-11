/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual, assertNull, assertTrue */
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

function ahuacatlNumericFunctionsTestSuite () {
  return {

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
      assertEqual([ 0 ], getQueryResults("RETURN FLOOR(null)"));
      assertEqual([ 1 ], getQueryResults("RETURN FLOOR(true)"));
      assertEqual([ 0 ], getQueryResults("RETURN FLOOR(\"yes\")"));
      assertEqual([ 0 ], getQueryResults("RETURN FLOOR([ ])"));
      assertEqual([ 2 ], getQueryResults("RETURN FLOOR([ 2 ])"));
      assertEqual([ 0 ], getQueryResults("RETURN FLOOR({ })"));
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
      assertEqual([ 0 ], getQueryResults("RETURN CEIL(null)"));
      assertEqual([ 1 ], getQueryResults("RETURN CEIL(true)"));
      assertEqual([ 0 ], getQueryResults("RETURN CEIL(\"yes\")"));
      assertEqual([ 0 ], getQueryResults("RETURN CEIL([ ])"));
      assertEqual([ 2 ], getQueryResults("RETURN CEIL([ 2 ])"));
      assertEqual([ 0 ], getQueryResults("RETURN CEIL({ })"));
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
      assertEqual([ 0 ], getQueryResults("RETURN ABS(null)"));
      assertEqual([ 1 ], getQueryResults("RETURN ABS(true)"));
      assertEqual([ 0 ], getQueryResults("RETURN ABS(\"yes\")"));
      assertEqual([ 0 ], getQueryResults("RETURN ABS([ ])"));
      assertEqual([ 37 ], getQueryResults("RETURN ABS([ -37 ])"));
      assertEqual([ 0 ], getQueryResults("RETURN ABS({ })"));
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
      assertEqual([ 0 ], getQueryResults("RETURN ROUND(null)"));
      assertEqual([ 1 ], getQueryResults("RETURN ROUND(true)"));
      assertEqual([ 0 ], getQueryResults("RETURN ROUND(\"yes\")"));
      assertEqual([ 0 ], getQueryResults("RETURN ROUND([ ])"));
      assertEqual([ -37 ], getQueryResults("RETURN ROUND([ -37 ])"));
      assertEqual([ 0 ], getQueryResults("RETURN ROUND({ })"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test rand function
////////////////////////////////////////////////////////////////////////////////
    
    testRand : function () {
      var actual = getQueryResults("FOR r IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 ] RETURN RAND()");
      for (var i in actual) {
        if (actual.hasOwnProperty(i)) {
          var value = actual[i];
          assertTrue(value >= 0.0 && value < 1.0);
        }
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
      assertEqual([ 0 ], getQueryResults("RETURN SQRT(false)")); 
      assertEqual([ 1 ], getQueryResults("RETURN SQRT(true)")); 
      assertEqual([ 0 ], getQueryResults("RETURN SQRT('foo')")); 
      assertEqual([ 0 ], getQueryResults("RETURN SQRT([ ])")); 
      assertEqual([ 0 ], getQueryResults("RETURN SQRT({ })")); 
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlNumericFunctionsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
