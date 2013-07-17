////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, logical operators
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
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlLogicalTestSuite () {
  var errors = internal.errors;

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
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot1 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN !true");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot2 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN !false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot3 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN !!false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot4 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN !!!false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot5 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN !(1 == 1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot6 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN !(!(1 == 1))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot7 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN !true == !!false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNotInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN !null"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN !0"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN !1"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN !\"value\""); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN ![]"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN !{}"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNotPrecedence : function () {
      // not has higher precedence than ==
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN !1 == 0"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN !1 == !1"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN !1 > 7"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN !1 IN [1]"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAnd1 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN true && true");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAnd2 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN true && false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAnd3 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN false && true");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAnd4 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN false && false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAnd5 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN true && !false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and 
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAndInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN null && true"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN 1 && true"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN \"\" && true"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN \"false\" && true"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN [ ] && true"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN { } && true"); 

      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true && null"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true && 1"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true && \"\""); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true && \"false\""); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true && [ ]"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true && { }"); 

      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN null && false"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN 1 && false"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN \"\" && false"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN \"false\" && false"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN [ ] && false"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN { } && false"); 

      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false && null"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false && 1"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false && \"\""); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false && \"false\""); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false && [ ]"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false && { }"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAndShortCircuit1 : function () {
      // TODO: FIXME                              
      // var expected = [ false ];
      // var actual = getQueryResults("RETURN false && FAIL('this will fail')");
      //assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAndShortCircuit2 : function () {
      assertException(function() { getQueryResults("RETURN false && FAIL('this will fail')"); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr1 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN true || true");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr2 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN true || false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr3 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN false || true");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr4 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN false || false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr5 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN true || !false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr6 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN false || !false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOrInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN null || true"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN 1 || true"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN \"\" || true"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN \"false\" || true"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN [ ] || true"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN { } || true"); 

      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true || null"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true || 1"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true || \"\""); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true || \"false\""); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true || [ ]"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN true || { }"); 

      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN null || false"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN 1 || false"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN \"\" || false"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN \"false\" || false"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN [ ] || false"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN { } || false"); 

      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false || null"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false || 1"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false || \"\""); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false || \"false\""); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false || [ ]"); 
      assertQueryError(errors.ERROR_QUERY_INVALID_LOGICAL_VALUE.code, "RETURN false || { }"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOrShortCircuit1 : function () {
      // TODO: FIXME                              
      // var expected = [ true ];
      // var actual = getQueryResults("RETURN true || FAIL('this will fail')");
      //assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOrShortCircuit2 : function () {
      assertException(function() { getQueryResults("RETURN false || FAIL('this will fail')"); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOrShortCircuit3 : function () {
      assertException(function() { getQueryResults("RETURN FAIL('this will fail') || true"); });
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlLogicalTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
