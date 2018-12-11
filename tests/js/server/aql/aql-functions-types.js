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
var db = require("@arangodb").db;
var errors = internal.errors;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlTypesFunctionsTestSuite () {
  var vn = "UnitTestsAhuacatlVertex";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      db._drop(vn);

      var vertex = db._create(vn);
      vertex.save({ _key: "test1" });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(vn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test typename function
////////////////////////////////////////////////////////////////////////////////
    
    testTypename : function () {
      assertEqual([ "null" ], getQueryResults("RETURN TYPENAME(null)"));
      assertEqual([ "bool" ], getQueryResults("RETURN TYPENAME(false)"));
      assertEqual([ "bool" ], getQueryResults("RETURN TYPENAME(true)"));
      assertEqual([ "number" ], getQueryResults("RETURN TYPENAME(0)"));
      assertEqual([ "number" ], getQueryResults("RETURN TYPENAME(1)"));
      assertEqual([ "number" ], getQueryResults("RETURN TYPENAME(-99999)"));
      assertEqual([ "number" ], getQueryResults("RETURN TYPENAME(0.005)"));
      assertEqual([ "number" ], getQueryResults("RETURN TYPENAME(1334540.005)"));
      assertEqual([ "number" ], getQueryResults("RETURN TYPENAME(3e32)"));
      assertEqual([ "array" ], getQueryResults("RETURN TYPENAME(1..2)"));
      assertEqual([ "array" ], getQueryResults("RETURN TYPENAME(99..0)"));
      assertEqual([ "array" ], getQueryResults("RETURN TYPENAME(FOR i IN 1..10 RETURN i)"));
      assertEqual([ "array" ], getQueryResults("RETURN TYPENAME([ ])"));
      assertEqual([ "array" ], getQueryResults("RETURN TYPENAME([ 'foo ' ])"));
      assertEqual([ "object" ], getQueryResults("RETURN TYPENAME({ })"));
      assertEqual([ "object" ], getQueryResults("RETURN TYPENAME({ 'foo': 'bar' })"));
      assertEqual([ "string" ], getQueryResults("RETURN TYPENAME('foo')"));
      assertEqual([ "string" ], getQueryResults("RETURN TYPENAME('')"));
      assertEqual([ "string" ], getQueryResults("RETURN TYPENAME(' ')"));
      assertEqual([ "string" ], getQueryResults("RETURN TYPENAME('0')"));
      assertEqual([ "string" ], getQueryResults("RETURN TYPENAME('1')"));
      assertEqual([ "string" ], getQueryResults("RETURN TYPENAME('true')"));
      assertEqual([ "string" ], getQueryResults("RETURN TYPENAME('false')"));
      assertEqual([ "number", "number" ], getQueryResults("FOR i IN 1..2 RETURN TYPENAME(i)"));
      assertEqual([ "string", "string", "string", "number" ], getQueryResults("FOR i IN [ 'foo', 'bar', 'baz', 42 ] RETURN TYPENAME(i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test typename function, invalid arguments
////////////////////////////////////////////////////////////////////////////////
    
    testTypenameInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TYPENAME()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TYPENAME('a', 'b')"); 
    },

    testIsKey : function () {
      var tests = [
        [ null, false ],
        [ false, false ],
        [ true, false ],
        [ 0, false ],
        [ 1, false ],
        [ -1, false ],
        [ 1000, false ],
        [ 1.234, false ],
        [ [ ], false ],
        [ [ "key" ], false ],
        [ [ "key", "bar" ], false ],
        [ [ 1, 2, 3 ], false ],
        [ { key: true }, false ],
        [ { key: "key" }, false ],
        [ "", false ],
        [ " ", false ],
        [ " a", false ],
        [ " 1", false ],
        [ "1 ", false ],
        [ "a ", false ],
        [ "1", true ],
        [ "12345", true ],
        [ "a", true ],
        [ "A", true ],
        [ "A1B2C3", true ],
        [ "aaAABBcc", true ],
        [ "a string", false ],
        [ "1234567", true ],
        [ "1234567-12345", true ],
        [ "1234567/1234", false ],
        [ "/", false ],
        [ "1/", false ],
        [ "/1", false ],
        [ "tröt", false ],
        [ "AAA", true ],
        [ ":", true ],
        [ "@", true ],
        [ "_", true ],
        [ "-", true ],
        [ ":", true ],
        [ ".", true ],
        [ "(", true ],
        [ ")", true ],
        [ "+", true ],
        [ ",", true ],
        [ "=", true ],
        [ ";", true ],
        [ "$", true ],
        [ "!", true ],
        [ "*", true ],
        [ "'", true ],
        [ "%", true ],
        [ "1-2", true ],
        [ "foo@bar", true ],
        [ "foo@bar.com", true ],
        [ "foo_bar", true ],
        [ Array(253 + 1).join("a"), true ],
        [ Array(254 + 1).join("a"), true ],
        [ Array(255 + 1).join("a"), false ],
      ];

      tests.forEach(function(test) {
        assertEqual([ test[1] ], getQueryResults("RETURN IS_KEY(" + JSON.stringify(test[0]) + ")"), test);
        assertEqual([ test[1] ], getQueryResults("RETURN NOOPT(IS_KEY(" + JSON.stringify(test[0]) + "))"), test);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test is_string function
////////////////////////////////////////////////////////////////////////////////
    
    testIsString : function () {
      assertEqual([ false ], getQueryResults("RETURN IS_STRING(null)"));
      assertEqual([ false ], getQueryResults("RETURN IS_STRING(false)"));
      assertEqual([ false ], getQueryResults("RETURN IS_STRING(true)"));
      assertEqual([ false ], getQueryResults("RETURN IS_STRING(1)"));
      assertEqual([ false ], getQueryResults("RETURN IS_STRING([ ])"));
      assertEqual([ false ], getQueryResults("RETURN IS_STRING([ 'foo ' ])"));
      assertEqual([ false ], getQueryResults("RETURN IS_STRING({ })"));
      assertEqual([ false ], getQueryResults("RETURN IS_STRING({ 'foo': 'bar' })"));
      assertEqual([ true ], getQueryResults("RETURN IS_STRING('foo')"));
      assertEqual([ true ], getQueryResults("RETURN IS_STRING('')"));
      assertEqual([ true ], getQueryResults("RETURN IS_STRING('0')"));
      assertEqual([ true ], getQueryResults("RETURN IS_STRING('1')"));
      assertEqual([ true ], getQueryResults("RETURN IS_STRING('true')"));
      assertEqual([ true ], getQueryResults("RETURN IS_STRING('false')"));
      assertEqual([ false, false ], getQueryResults("FOR i IN 1..2 RETURN IS_STRING(i)"));
      assertEqual([ true, true, true, false ], getQueryResults("FOR i IN [ 'foo', 'bar', 'baz', 42 ] RETURN IS_STRING(i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test is_string function, invalid arguments
////////////////////////////////////////////////////////////////////////////////
    
    testIsStringInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_STRING()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_STRING('a', 'b')"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull1 : function () {
      var expected = [ 6 ];
      var actual = getQueryResults("RETURN NOT_NULL(null, 2 + 4)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(NOT_NULL(null, 2 + 4))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull2 : function () {
      var expected = [ 6 ];
      var actual = getQueryResults("RETURN NOT_NULL(2 + 4, 2 + 5)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(NOT_NULL(2 + 4, 2 + 5))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull3 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN NOT_NULL(null, null)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(NOT_NULL(null, null))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull4 : function () {
      var expected = [ 2 ];
      var actual = getQueryResults("RETURN NOT_NULL(2, null)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(NOT_NULL(2, null))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull5 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN NOT_NULL(false, true)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(NOT_NULL(false, true))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull6 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN NOT_NULL(null, null, null, null)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(NOT_NULL(null, null, null, null))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull7 : function () {
      var expected = [ -6 ];
      var actual = getQueryResults("RETURN NOT_NULL(null, null, null, null, -6, -7)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(NOT_NULL(null, null, null, null, -6, -7))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull8 : function () {
      var expected = [ 12 ];
      var actual = getQueryResults("RETURN NOT_NULL(null, null, null, null, 12, null)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(NOT_NULL(null, null, null, null, 12, null))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull9 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN NOT_NULL(null)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(NOT_NULL(null))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not_null
////////////////////////////////////////////////////////////////////////////////
    
    testNotNull10 : function () {
      var expected = [ 23 ];
      var actual = getQueryResults("RETURN NOT_NULL(23)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(NOT_NULL(23))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList1 : function () {
      var expected = [ [ 1, 2 ] ];
      var actual = getQueryResults("RETURN FIRST_LIST(null, [ 1, 2 ])");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_LIST(null, [ 1, 2 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList2 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_LIST(null, \"not a list!\")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_LIST(null, \"not a list!\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList3 : function () {
      var expected = [ [ 1, 4 ] ];
      var actual = getQueryResults("RETURN FIRST_LIST([ 1, 4 ], [ 1, 5 ])");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_LIST([ 1, 4 ], [ 1, 5 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList4 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN FIRST_LIST([ ], [ 1, 5 ])");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_LIST([ ], [ 1, 5 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList5 : function () {
      var expected = [ [ false ] ];
      var actual = getQueryResults("RETURN FIRST_LIST([ false ], [ 1, 5 ])");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_LIST([ false ], [ 1, 5 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList6 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_LIST(false, 7)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_LIST(false, 7))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList7 : function () {
      var expected = [ [ 0 ] ];
      var actual = getQueryResults("RETURN FIRST_LIST(1, 2, 3, 4, [ 0 ])");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_LIST(1, 2, 3, 4, [ 0 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList8 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_LIST(1, 2, 3, 4, { })");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_LIST(1, 2, 3, 4, { }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList9 : function () {
      var expected = [ [ 7 ] ];
      var actual = getQueryResults("RETURN FIRST_LIST([ 7 ])");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_LIST([ 7 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_list
////////////////////////////////////////////////////////////////////////////////
    
    testFirstList10 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_LIST(99)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_LIST(99))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument1 : function () {
      var expected = [ { a : 1 } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(null, { a : 1 })");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_DOCUMENT(null, { a : 1 }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument2 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(null, \"not a doc!\")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_DOCUMENT(null, \"not a doc!\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument3 : function () {
      var expected = [ { a : 1 } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT({ a : 1 }, { b : 2 })");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_DOCUMENT({ a : 1 }, { b : 2 }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument4 : function () {
      var expected = [ { } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT({ }, { b : 2 })");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_DOCUMENT({ }, { b : 2 }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument5 : function () {
      var expected = [ { a : null, b : false } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT({ a : null, b : false }, { a : 1000, b : 1000 })");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_DOCUMENT({ a : null, b : false }, { a : 1000, b : 1000 }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument6 : function () {
      var expected = [ { czz : 7 } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(false, { czz : 7 })");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_DOCUMENT(false, { czz : 7 }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument7 : function () {
      var expected = [ { } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(1, 2, 3, 4, { })");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_DOCUMENT(1, 2, 3, 4, { }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument8 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(1, 2, 3, 4, [ ])");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_DOCUMENT(1, 2, 3, 4, [ ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument9 : function () {
      var expected = [ { c : 4 } ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT({ c : 4 })");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_DOCUMENT({ c : 4 }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first_document
////////////////////////////////////////////////////////////////////////////////
    
    testFirstDocument10 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST_DOCUMENT(false)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(FIRST_DOCUMENT(false))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool1 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN TO_BOOL(null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool2 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN TO_BOOL(false)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool3 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool4 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN TO_BOOL(0)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool5 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(0.1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool6 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(-1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool7 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(100)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool8 : function () {
      var expected = [ false ];
      var actual = getQueryResults("RETURN TO_BOOL(\"\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool9 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(\" \")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool10 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(\"0\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool11 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(\"false\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool12 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool13 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool14 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ false ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool15 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ 0 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool16 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ \"\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool17 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ \" \" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool18 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ \"0\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool19 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ \"false\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool20 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL([ { } ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool21 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL({ })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool22 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL({ \"0\" : \"\" })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool23 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL({ \"false\" : null })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_bool
////////////////////////////////////////////////////////////////////////////////
    
    testToBool24 : function () {
      var expected = [ true ];
      var actual = getQueryResults("RETURN TO_BOOL(DOCUMENT(" + vn + ", " + JSON.stringify(vn + "/test1") + "))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber1 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER(null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber2 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN TO_NUMBER([ 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber4 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER([ -1, 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber5 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER({ })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber6 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER({ \"2\" : \"3\" })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber7 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER(false)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber8 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN TO_NUMBER(true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber9 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"1\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber10 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"0\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber11 : function () {
      var expected = [ -435.3 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"-435.3\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber12 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"3553.4er6\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber13 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"-wert324\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber14 : function () {
      var expected = [ 143 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"  143\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_number
////////////////////////////////////////////////////////////////////////////////
    
    testToNumber15 : function () {
      var expected = [ 0.4 ];
      var actual = getQueryResults("RETURN TO_NUMBER(\"  .4\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray1 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN TO_ARRAY(null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray2 : function () {
      var expected = [ [ false ] ];
      var actual = getQueryResults("RETURN TO_ARRAY(false)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray3 : function () {
      var expected = [ [ true ] ];
      var actual = getQueryResults("RETURN TO_ARRAY(true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray4 : function () {
      var expected = [ [ 35 ] ];
      var actual = getQueryResults("RETURN TO_ARRAY(35)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray5 : function () {
      var expected = [ [ -0.635 ] ];
      var actual = getQueryResults("RETURN TO_ARRAY(-0.635)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray6 : function () {
      var expected = [ [ "" ] ];
      var actual = getQueryResults("RETURN TO_ARRAY(\"\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray7 : function () {
      var expected = [ [ "value" ] ];
      var actual = getQueryResults("RETURN TO_ARRAY(\"value\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray8 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN TO_ARRAY({ })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray9 : function () {
      var expected = [ [ 0 ] ];
      var actual = getQueryResults("RETURN TO_ARRAY({ \"a\" : 0 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray10 : function () {
      var expected = [ [ null, -63, [ 1, 2 ], { "a" : "b" } ] ];
      var actual = getQueryResults("RETURN TO_ARRAY({ \"a\" : null, \"b\" : -63, \"c\" : [ 1, 2 ], \"d\": { \"a\" : \"b\" } })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray11 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN TO_ARRAY([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_array
////////////////////////////////////////////////////////////////////////////////
    
    testToArray12 : function () {
      var expected = [ [ 0, null, -1 ] ];
      var actual = getQueryResults("RETURN TO_ARRAY([ 0, null, -1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList1 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN TO_LIST(null)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList2 : function () {
      var expected = [ [ false ] ];
      var actual = getQueryResults("RETURN TO_LIST(false)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList3 : function () {
      var expected = [ [ true ] ];
      var actual = getQueryResults("RETURN TO_LIST(true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList4 : function () {
      var expected = [ [ 35 ] ];
      var actual = getQueryResults("RETURN TO_LIST(35)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList5 : function () {
      var expected = [ [ -0.635 ] ];
      var actual = getQueryResults("RETURN TO_LIST(-0.635)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList6 : function () {
      var expected = [ [ "" ] ];
      var actual = getQueryResults("RETURN TO_LIST(\"\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList7 : function () {
      var expected = [ [ "value" ] ];
      var actual = getQueryResults("RETURN TO_LIST(\"value\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList8 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN TO_LIST({ })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList9 : function () {
      var expected = [ [ 0 ] ];
      var actual = getQueryResults("RETURN TO_LIST({ \"a\" : 0 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList10 : function () {
      var expected = [ [ null, -63, [ 1, 2 ], { "a" : "b" } ] ];
      var actual = getQueryResults("RETURN TO_LIST({ \"a\" : null, \"b\" : -63, \"c\" : [ 1, 2 ], \"d\": { \"a\" : \"b\" } })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList11 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN TO_LIST([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to_list
////////////////////////////////////////////////////////////////////////////////
    
    testToList12 : function () {
      var expected = [ [ 0, null, -1 ] ];
      var actual = getQueryResults("RETURN TO_LIST([ 0, null, -1 ])");
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlTypesFunctionsTestSuite);

return jsunity.done();

