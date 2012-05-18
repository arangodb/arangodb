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

var internal = require("internal");
var jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlArithmeticTestSuite () {
  var errors = internal.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = AHUACATL_RUN(query, undefined);
    if (cursor instanceof ArangoError) {
      print(query, cursor.errorMessage);
    }
    assertFalse(cursor instanceof ArangoError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query) {
    var result = executeQuery(query).getRows();
    var results = [ ];

    for (var i in result) {
      if (!result.hasOwnProperty(i)) {
        continue;
      }

      results.push(result[i]);
    }

    return results;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the error code from a result
////////////////////////////////////////////////////////////////////////////////

  function getErrorCode (result) {
    return result.errorNum;
  }


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
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlus1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN +0");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlus2 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN +1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlus3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN ++1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlus4 : function () {
      var expected = [ -5 ];
      var actual = getQueryResults("RETURN +-5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlus5 : function () {
      var expected = [ 5.4 ];
      var actual = getQueryResults("RETURN +++5.4");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary plus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryPlusInvalid : function () {
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN +null")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN +true")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN +false")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN +\"value\"")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN +[ ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN +{ }")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinus1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN -0");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinus2 : function () {
      var expected = [ -1 ];
      var actual = getQueryResults("RETURN -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinus3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN --1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinus4 : function () {
      var expected = [ -5 ];
      var actual = getQueryResults("RETURN -+5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinus5 : function () {
      var expected = [ -5.4 ];
      var actual = getQueryResults("RETURN ---5.4");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary minus
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryMinusInvalid : function () {
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN -null")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN -true")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN -false")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN -\"value\"")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN -[ ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN -{ }")));
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary plus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryPlus2 : function () {
      var expected = [ -43.5 ];
      var actual = getQueryResults("RETURN -1 + -42.5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary plus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryPlus3 : function () {
      var expected = [ 41.5 ];
      var actual = getQueryResults("RETURN -1 + +42.5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary plus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryPlus4 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN -1 + +42.5 == +42.5 + -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary plus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryPlusInvalid : function () {
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 + null")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 + false")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 + \"0\"")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 + [ ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 + [ 0 ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 + { }")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN null + 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN false + 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN \"0\" + 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ ] + 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ 0 ] + 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN { } + 1")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary minus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMinus1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN -1 - -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary minus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMinus2 : function () {
      var expected = [ -5.5 ];
      var actual = getQueryResults("RETURN -2.25 - 3.25");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary minus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMinus3 : function () {
      var expected = [ -1 ];
      var actual = getQueryResults("RETURN 2.25 - 3.25");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary minus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMinusInvalid : function () {
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 - null")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 - false")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 - \"0\"")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 - [ ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 - [ 0 ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 - { }")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN null - 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN false - 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN \"0\" - 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ ] - 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ 0 ] - 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN { } - 1")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN 0 * 100");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication2 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN 100 * 0");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication3 : function () {
      var expected = [ 37 ];
      var actual = getQueryResults("RETURN 1 * 37");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication4 : function () {
      var expected = [ 37 ];
      var actual = getQueryResults("RETURN 37 * 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication5 : function () {
      var expected = [ 76.5 ];
      var actual = getQueryResults("RETURN 9 * 8.5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication6 : function () {
      var expected = [ -14 ];
      var actual = getQueryResults("RETURN -4 * 3.5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplication7 : function () {
      var expected = [ 14 ];
      var actual = getQueryResults("RETURN -4 * -3.5");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary multiplication
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryMultiplicationInvalid : function () {
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 * null")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 * false")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 * \"0\"")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 * [ ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 * [ 0 ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 * { }")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN null * 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN false * 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN \"0\" * 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ ] * 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ 0 ] * 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN { } * 1")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN 0 / 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision2 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN 1 / 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN 1.25 / 1.25");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision4 : function () {
      var expected = [ 12 ];
      var actual = getQueryResults("RETURN 144 / 12");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision5 : function () {
      var expected = [ -12 ];
      var actual = getQueryResults("RETURN -144 / 12");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision6 : function () {
      var expected = [ -12 ];
      var actual = getQueryResults("RETURN 144 / -12");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision7 : function () {
      var expected = [ 12 ];
      var actual = getQueryResults("RETURN -144 / -12");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivision8 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN 1 / 0.5 / 2");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary division
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryDivisionInvalid : function () {
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 / null")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 / false")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 / \"0\"")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 / [ ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 / [ 0 ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 / { }")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN null / 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN false / 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN \"0\" / 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ ] / 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ 0 ] / 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN { } / 1")));
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, getErrorCode(AHUACATL_RUN("RETURN 1 / 0")));
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, getErrorCode(AHUACATL_RUN("RETURN 1 / 0.0")));
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, getErrorCode(AHUACATL_RUN("RETURN 1 / (1 - 1)")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary modulus
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryModulusInvalid : function () {
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % null")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % false")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % \"0\"")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % [ ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % [ 0 ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % { }")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN null % 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN false % 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN \"0\" % 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ ] % 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ 0 ] % 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN { } % 1")));
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, getErrorCode(AHUACATL_RUN("RETURN 1 % 0")));
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, getErrorCode(AHUACATL_RUN("RETURN 1 % 0.0")));
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, getErrorCode(AHUACATL_RUN("RETURN 1 % (1 - 1)")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test precedence
////////////////////////////////////////////////////////////////////////////////
    
    testArithmeticPrecedence : function () {
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % null")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % false")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % \"0\"")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % [ ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % [ 0 ]")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN 1 % { }")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN null % 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN false % 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN \"0\" % 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ ] % 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN [ 0 ] % 1")));
      assertEqual(errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, getErrorCode(AHUACATL_RUN("RETURN { } % 1")));
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, getErrorCode(AHUACATL_RUN("RETURN 1 % 0")));
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, getErrorCode(AHUACATL_RUN("RETURN 1 % 0.0")));
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, getErrorCode(AHUACATL_RUN("RETURN 1 % (1 - 1)")));
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlArithmeticTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
