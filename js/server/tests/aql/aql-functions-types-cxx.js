/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */
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
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlTypesFunctionsTestSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test typename function
////////////////////////////////////////////////////////////////////////////////
    
    testTypename : function () {
      assertEqual([ "null" ], getQueryResults("RETURN NOOPT(TYPENAME(null))"));
      assertEqual([ "bool" ], getQueryResults("RETURN NOOPT(TYPENAME(false))"));
      assertEqual([ "bool" ], getQueryResults("RETURN NOOPT(TYPENAME(true))"));
      assertEqual([ "number" ], getQueryResults("RETURN NOOPT(TYPENAME(0))"));
      assertEqual([ "number" ], getQueryResults("RETURN NOOPT(TYPENAME(1))"));
      assertEqual([ "number" ], getQueryResults("RETURN NOOPT(TYPENAME(-99999))"));
      assertEqual([ "number" ], getQueryResults("RETURN NOOPT(TYPENAME(0.005))"));
      assertEqual([ "number" ], getQueryResults("RETURN NOOPT(TYPENAME(1334540.005))"));
      assertEqual([ "number" ], getQueryResults("RETURN NOOPT(TYPENAME(3e32))"));
      assertEqual([ "array" ], getQueryResults("RETURN NOOPT(TYPENAME(1..2))"));
      assertEqual([ "array" ], getQueryResults("RETURN NOOPT(TYPENAME(99..0))"));
      assertEqual([ "array" ], getQueryResults("RETURN NOOPT(TYPENAME(FOR i IN 1..10 RETURN i))"));
      assertEqual([ "array" ], getQueryResults("RETURN NOOPT(TYPENAME([ ]))"));
      assertEqual([ "array" ], getQueryResults("RETURN NOOPT(TYPENAME([ 'foo ' ]))"));
      assertEqual([ "object" ], getQueryResults("RETURN NOOPT(TYPENAME({ }))"));
      assertEqual([ "object" ], getQueryResults("RETURN NOOPT(TYPENAME({ 'foo': 'bar' }))"));
      assertEqual([ "string" ], getQueryResults("RETURN NOOPT(TYPENAME('foo'))"));
      assertEqual([ "string" ], getQueryResults("RETURN NOOPT(TYPENAME(''))"));
      assertEqual([ "string" ], getQueryResults("RETURN NOOPT(TYPENAME(' '))"));
      assertEqual([ "string" ], getQueryResults("RETURN NOOPT(TYPENAME('0'))"));
      assertEqual([ "string" ], getQueryResults("RETURN NOOPT(TYPENAME('1'))"));
      assertEqual([ "string" ], getQueryResults("RETURN NOOPT(TYPENAME('true'))"));
      assertEqual([ "string" ], getQueryResults("RETURN NOOPT(TYPENAME('false'))"));
      assertEqual([ "number", "number" ], getQueryResults("FOR i IN 1..2 RETURN NOOPT(TYPENAME(i))"));
      assertEqual([ "string", "string", "string", "number" ], getQueryResults("FOR i IN [ 'foo', 'bar', 'baz', 42 ] RETURN NOOPT(TYPENAME(i))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test typename function, invalid arguments
////////////////////////////////////////////////////////////////////////////////
    
    testTypenameInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(TYPENAME())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(TYPENAME('a', 'b'))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test is_string function
////////////////////////////////////////////////////////////////////////////////
    
    testIsString : function () {
      assertEqual([ false ], getQueryResults("RETURN NOOPT(IS_STRING(null))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(IS_STRING(false))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(IS_STRING(true))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(IS_STRING(1))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(IS_STRING([ ]))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(IS_STRING([ 'foo ' ]))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(IS_STRING({ }))"));
      assertEqual([ false ], getQueryResults("RETURN NOOPT(IS_STRING({ 'foo': 'bar' }))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(IS_STRING('foo'))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(IS_STRING(''))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(IS_STRING('0'))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(IS_STRING('1'))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(IS_STRING('true'))"));
      assertEqual([ true ], getQueryResults("RETURN NOOPT(IS_STRING('false'))"));
      assertEqual([ false, false ], getQueryResults("FOR i IN 1..2 RETURN NOOPT(IS_STRING(i))"));
      assertEqual([ true, true, true, false ], getQueryResults("FOR i IN [ 'foo', 'bar', 'baz', 42 ] RETURN NOOPT(IS_STRING(i))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test is_string function, invalid arguments
////////////////////////////////////////////////////////////////////////////////
    
    testIsStringInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(IS_STRING())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(IS_STRING('a', 'b'))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool1 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(null))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool2 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(false))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool3 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(true))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool4 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(0))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool5 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(0.1))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool6 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(-1))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool7 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(100))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool8 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(\"\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool9 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(\" \"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool10 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(\"0\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool11 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL(\"false\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool12 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL([ ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool13 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL([ 1 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool14 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL([ false ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool15 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL([ 0 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool16 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL([ \"\" ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool17 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL([ \" \" ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool18 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL([ \"0\" ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool19 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL([ \"false\" ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool20 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL([ { } ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool21 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL({ }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool22 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL({ \"0\" : \"\" }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool23 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN NOOPT(TO_BOOL({ \"false\" : null }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER(null))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber2 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER([ ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER([ 1 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber4 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER([ -1, 1 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber5 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER({ }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber6 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER({ \"2\" : \"3\" }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber7 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER(false))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber8 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER(true))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber9 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER(\"1\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber10 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER(\"0\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber11 : function () {
      var expected = [ -435.3 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER(\"-435.3\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber12 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER(\"3553.4er6\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber13 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER(\"-wert324\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber14 : function () {
      var expected = [ 143 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER(\"  143\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber15 : function () {
      var expected = [ 0.4 ];
      var actual = getQueryResults("RETURN NOOPT(TO_NUMBER(\"  .4\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray1 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY(null))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray2 : function () {
      var expected = [ [ false ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY(false))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray3 : function () {
      var expected = [ [ true ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY(true))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray4 : function () {
      var expected = [ [ 35 ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY(35))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray5 : function () {
      var expected = -0.635;
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY(-0.635))");
      assertEqual(actual.length, 1);
      assertEqual(actual[0].length, 1);
      var value = actual[0][0];
      // Double precission
      assertEqual(expected.toPrecision(5), value.toPrecision(5));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray6 : function () {
      var expected = [ [ "" ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY(\"\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray7 : function () {
      var expected = [ [ "value" ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY(\"value\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray8 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY({ }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray9 : function () {
      var expected = [ [ 0 ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY({ \"a\" : 0 }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray10 : function () {
      var expected = [ [ null, -63, [ 1, 2 ], { "a" : "b" } ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY({ \"a\" : null, \"b\" : -63, \"c\" : [ 1, 2 ], \"d\": { \"a\" : \"b\" } }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray11 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY([ ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray12 : function () {
      var expected = [ [ 0, null, -1 ] ];
      var actual = getQueryResults("RETURN NOOPT(TO_ARRAY([ 0, null, -1 ]))");
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlTypesFunctionsTestSuite);

return jsunity.done();

