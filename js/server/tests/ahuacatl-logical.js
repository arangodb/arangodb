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

var jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlLogicalTestSuite () {

  ////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    return AHUACATL_RUN(query, undefined);
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
      assertException(function() { getQueryResults("RETURN !null"); });
      assertException(function() { getQueryResults("RETURN !0"); });
      assertException(function() { getQueryResults("RETURN !1"); });
      assertException(function() { getQueryResults("RETURN !\"value\""); });
      assertException(function() { getQueryResults("RETURN ![ ]"); });
      assertException(function() { getQueryResults("RETURN !{ }"); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNotPrecedence : function () {
      // not has higher precedence than ==
      assertException(function() { getQueryResults("RETURN !1 == 0"); });
      assertException(function() { getQueryResults("RETURN !1 == !1"); });
      assertException(function() { getQueryResults("RETURN !1 > 7"); });
      assertException(function() { getQueryResults("RETURN !1 in [1]"); });
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
      assertException(function() { getQueryResults("RETURN +null"); });
      assertException(function() { getQueryResults("RETURN +true"); });
      assertException(function() { getQueryResults("RETURN +false"); });
      assertException(function() { getQueryResults("RETURN +\"value\""); });
      assertException(function() { getQueryResults("RETURN +[ ]"); });
      assertException(function() { getQueryResults("RETURN +{ }"); });
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
      assertException(function() { getQueryResults("RETURN -null"); });
      assertException(function() { getQueryResults("RETURN -true"); });
      assertException(function() { getQueryResults("RETURN -false"); });
      assertException(function() { getQueryResults("RETURN -\"value\""); });
      assertException(function() { getQueryResults("RETURN -[ ]"); });
      assertException(function() { getQueryResults("RETURN -{ }"); });
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
