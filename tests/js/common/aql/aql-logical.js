/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertException */

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
var db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlLogicalTestSuite () {
  var vn = "UnitTestsAhuacatlVertex";
  var vertex = null;
  var d = null;
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      // this.tearDown(); should actually work as well
      db._drop(vn);
      
      vertex = db._create(vn);
      d = vertex.save({ _key: "test1" });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(vn);
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
    
    testUnaryNotNonBoolean : function () {
      assertEqual([ true ], getQueryResults("RETURN !null")); 
      assertEqual([ true ], getQueryResults("RETURN !0")); 
      assertEqual([ true ], getQueryResults("RETURN !\"\"")); 
      assertEqual([ false ], getQueryResults("RETURN !\" \"")); 
      assertEqual([ false ], getQueryResults("RETURN !\"0\"")); 
      assertEqual([ false ], getQueryResults("RETURN !\"1\"")); 
      assertEqual([ false ], getQueryResults("RETURN !\"value\"")); 
      assertEqual([ false ], getQueryResults("RETURN ![]")); 
      assertEqual([ false ], getQueryResults("RETURN !{}")); 
      assertEqual([ false ], getQueryResults("RETURN !DOCUMENT(" + vn + ", " + JSON.stringify(d._id) + ")")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNotPrecedence : function () {
      // not has higher precedence than ==
      assertEqual([ false ], getQueryResults("RETURN !1 == 0")); 
      assertEqual([ true ], getQueryResults("RETURN !1 == !1")); 
      assertEqual([ false ], getQueryResults("RETURN !1 != !1")); 
      assertEqual([ false ], getQueryResults("RETURN !1 > 7")); 
      assertEqual([ true ], getQueryResults("RETURN !1 < 7")); 
      assertEqual([ true ], getQueryResults("RETURN !1 IN [!1]")); 
      assertEqual([ false ], getQueryResults("RETURN !1 NOT IN [!1]")); 
      assertEqual([ false ], getQueryResults("RETURN !1 IN [1]")); 
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
    
    testBinaryAndNonBoolean : function () {
      assertEqual([ null ], getQueryResults("RETURN null && true"));
      assertEqual([ true ], getQueryResults("RETURN 1 && true"));
      assertEqual([ 0 ], getQueryResults("RETURN 1 && 0"));
      assertEqual([ false ], getQueryResults("RETURN 1 && false"));
      assertEqual([ 1 ], getQueryResults("RETURN 1 && 1"));
      assertEqual([ 0 ], getQueryResults("RETURN 0 && true"));
      assertEqual([ 0 ], getQueryResults("RETURN 0 && 0"));
      assertEqual([ 0 ], getQueryResults("RETURN 0 && false"));
      assertEqual([ 0 ], getQueryResults("RETURN 0 && 1"));
      assertEqual([ "" ], getQueryResults("RETURN \"\" && true"));
      assertEqual([ true ], getQueryResults("RETURN \" \" && true"));
      assertEqual([ true ], getQueryResults("RETURN \"false\" && true"));
      assertEqual([ true ], getQueryResults("RETURN [ ] && true"));
      assertEqual([ true ], getQueryResults("RETURN { } && true"));
      assertEqual([ [ ] ], getQueryResults("RETURN 23 && [ ]"));
      assertEqual([ { } ], getQueryResults("RETURN 23 && { }"));
      assertEqual([ null ], getQueryResults("RETURN true && null"));
      assertEqual([ 1 ], getQueryResults("RETURN true && 1"));
      assertEqual([ "" ], getQueryResults("RETURN true && \"\""));
      assertEqual([ "false" ], getQueryResults("RETURN true && \"false\""));
      assertEqual([ false ], getQueryResults("RETURN true && false"));
      assertEqual([ [ ] ], getQueryResults("RETURN true && [ ]"));
      assertEqual([ { } ], getQueryResults("RETURN true && { }"));
      assertEqual([ null ], getQueryResults("RETURN null && false"));
      assertEqual([ false ], getQueryResults("RETURN 1 && false"));
      assertEqual([ "" ], getQueryResults("RETURN \"\" && false"));
      assertEqual([ false ], getQueryResults("RETURN \"false\" && false"));
      assertEqual([ false ], getQueryResults("RETURN [ ] && false"));
      assertEqual([ false ], getQueryResults("RETURN { } && false"));
      assertEqual([ false ], getQueryResults("RETURN false && null"));
      assertEqual([ false ], getQueryResults("RETURN false && 1"));
      assertEqual([ false ], getQueryResults("RETURN false && \"\""));
      assertEqual([ false ], getQueryResults("RETURN false && \"false\""));
      assertEqual([ false ], getQueryResults("RETURN false && [ ]"));
      assertEqual([ false ], getQueryResults("RETURN false && { }"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAndShortCircuit1 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN false && FAIL('this will fail')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAndShortCircuit2 : function () {
      assertException(function() { getQueryResults("RETURN true && FAIL('this will fail')"); });
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
    
    testBinaryOrNonBoolean : function () {
      assertEqual([ true ], getQueryResults("RETURN null || true"));
      assertEqual([ true ], getQueryResults("RETURN 0 || true"));
      assertEqual([ true ], getQueryResults("RETURN false || true"));
      assertEqual([ 1 ], getQueryResults("RETURN 1 || true"));
      assertEqual([ true ], getQueryResults("RETURN \"\" || true"));
      assertEqual([ " " ], getQueryResults("RETURN \" \" || true"));
      assertEqual([ "0" ], getQueryResults("RETURN \"0\" || true"));
      assertEqual([ "1" ], getQueryResults("RETURN \"1\" || true"));
      assertEqual([ "false" ], getQueryResults("RETURN \"false\" || true"));
      assertEqual([ [ ] ], getQueryResults("RETURN [ ] || true"));
      assertEqual([ { } ], getQueryResults("RETURN { } || true"));
      assertEqual([ true ], getQueryResults("RETURN true || null"));
      assertEqual([ true ], getQueryResults("RETURN true || 1"));
      assertEqual([ true ], getQueryResults("RETURN true || \"\""));
      assertEqual([ true ], getQueryResults("RETURN true || \"false\""));
      assertEqual([ true ], getQueryResults("RETURN true || [ ]"));
      assertEqual([ true ], getQueryResults("RETURN true || { }"));
      assertEqual([ false ], getQueryResults("RETURN null || false"));
      assertEqual([ false ], getQueryResults("RETURN 0 || false"));
      assertEqual([ 1 ], getQueryResults("RETURN 1 || false"));
      assertEqual([ false ], getQueryResults("RETURN false || false"));
      assertEqual([ true ], getQueryResults("RETURN true || false"));
      assertEqual([ false ], getQueryResults("RETURN \"\" || false"));
      assertEqual([ " " ], getQueryResults("RETURN \" \" || false"));
      assertEqual([ "0" ], getQueryResults("RETURN \"0\" || false"));
      assertEqual([ "1" ], getQueryResults("RETURN \"1\" || false"));
      assertEqual([ "false" ], getQueryResults("RETURN \"false\" || false"));
      assertEqual([ [ ] ], getQueryResults("RETURN [ ] || false"));
      assertEqual([ { } ], getQueryResults("RETURN { } || false"));
      assertEqual([ null ], getQueryResults("RETURN false || null"));
      assertEqual([ 1 ], getQueryResults("RETURN false || 1"));
      assertEqual([ "" ], getQueryResults("RETURN false || \"\""));
      assertEqual([ " " ], getQueryResults("RETURN false || \" \""));
      assertEqual([ "0" ], getQueryResults("RETURN false || \"0\""));
      assertEqual([ "1" ], getQueryResults("RETURN false || \"1\""));
      assertEqual([ "false" ], getQueryResults("RETURN false || \"false\""));
      assertEqual([ [ ] ], getQueryResults("RETURN false || [ ]"));
      assertEqual([ { } ], getQueryResults("RETURN false || { }"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOrShortCircuit1 : function () {
      assertEqual([ true ], getQueryResults("RETURN true || FAIL('this will fail')")); 
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

