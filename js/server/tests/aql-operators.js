////////////////////////////////////////////////////////////////////////////////
/// @brief tests for AQL operators
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

function aqlOperatorsTestSuite () {

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
/// @brief test AQL_KEYS function
////////////////////////////////////////////////////////////////////////////////

    testKeys : function () {
      assertEqual([ ], AQL_KEYS([ ]));
      assertEqual([ 0, 1, 2 ], AQL_KEYS([ 0, 1, 2 ]));
      assertEqual([ 0, 1, 2 ], AQL_KEYS([ 1, 2, 3 ]));
      assertEqual([ 0, 1, 2 ], AQL_KEYS([ 5, 6, 9 ]));
      assertEqual([ 0, 1, 2 ], AQL_KEYS([ false, false, false ]));
      assertEqual([ 0, 1, 2 ], AQL_KEYS([ -1, -1, 'zang' ]));
      assertEqual([ 0, 1, 2, 3 ], AQL_KEYS([ 99, 99, 99, 99 ]));
      assertEqual([ 0 ], AQL_KEYS([ [ ] ]));
      assertEqual([ 0 ], AQL_KEYS([ [ 1 ] ]));
      assertEqual([ 0, 1 ], AQL_KEYS([ [ 1 ], 1 ]));
      assertEqual([ 0, 1 ], AQL_KEYS([ [ 1 ], [ 1 ] ]));
      assertEqual([ 0 ], AQL_KEYS([ [ 1 , 2 ] ]));
      assertEqual([ 0, 1, 2 ], AQL_KEYS([ [ 1 , 2 ], [ ], 3 ]));
      assertEqual([ 0, 1, 2 ], AQL_KEYS([ { }, { }, { } ]));
      assertEqual([ 0, 1, 2 ], AQL_KEYS([ { 'a' : true, 'b' : false }, { }, { } ]));
      assertEqual([ ], AQL_KEYS({ }));
      assertEqual([ '0' ], AQL_KEYS({ '0' : false }));
      assertEqual([ '0' ], AQL_KEYS({ '0' : undefined }));
      assertEqual([ 'a', 'b', 'c' ], AQL_KEYS({ 'a' : true, 'b' : true, 'c' : true }));
      assertEqual([ 'a', 'b', 'c' ], AQL_KEYS({ 'a' : true, 'c' : true, 'b' : true }));
      assertEqual([ 'a', 'b', 'c' ], AQL_KEYS({ 'b' : true, 'a' : true, 'c' : true }));
      assertEqual([ 'a', 'b', 'c' ], AQL_KEYS({ 'b' : true, 'c' : true, 'a' : true }));
      assertEqual([ 'a', 'b', 'c' ], AQL_KEYS({ 'c' : true, 'a' : true, 'b' : true }));
      assertEqual([ 'a', 'b', 'c' ], AQL_KEYS({ 'c' : true, 'b' : true, 'a' : true }));
      assertEqual([ '0', '1', '2' ], AQL_KEYS({ '0' : true, '1' : true, '2' : true }));
      assertEqual([ '0', '1', '2' ], AQL_KEYS({ '1' : true, '2' : true, '0' : true }));
      assertEqual([ 'a1', 'a2', 'a3' ], AQL_KEYS({ 'a1' : true, 'a2' : true, 'a3' : true }));
      assertEqual([ 'a1', 'a10', 'a20', 'a200', 'a21' ], AQL_KEYS({ 'a200' : true, 'a21' : true, 'a20' : true, 'a10' : false, 'a1' : null }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_UNDEFINED function
////////////////////////////////////////////////////////////////////////////////

    testIsUndefinedTrue : function () {
      assertTrue(AQL_IS_UNDEFINED(undefined));
      assertTrue(AQL_IS_UNDEFINED(NaN));
      assertTrue(AQL_IS_UNDEFINED(1.3e308 * 1.3e308));
      assertTrue(AQL_IS_UNDEFINED(AQL_ARITHMETIC_DIVIDE(1, 0)));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_UNDEFINED function
////////////////////////////////////////////////////////////////////////////////

    testIsUndefinedFalse : function () {
      assertFalse(AQL_IS_UNDEFINED(0));
      assertFalse(AQL_IS_UNDEFINED(1));
      assertFalse(AQL_IS_UNDEFINED(-1));
      assertFalse(AQL_IS_UNDEFINED(0.1));
      assertFalse(AQL_IS_UNDEFINED(-0.1));
      assertFalse(AQL_IS_UNDEFINED(false));
      assertFalse(AQL_IS_UNDEFINED(true));
      assertFalse(AQL_IS_UNDEFINED(null));
      assertFalse(AQL_IS_UNDEFINED('abc'));
      assertFalse(AQL_IS_UNDEFINED('null'));
      assertFalse(AQL_IS_UNDEFINED('false'));
      assertFalse(AQL_IS_UNDEFINED('undefined'));
      assertFalse(AQL_IS_UNDEFINED(''));
      assertFalse(AQL_IS_UNDEFINED(' '));
      assertFalse(AQL_IS_UNDEFINED([ ]));
      assertFalse(AQL_IS_UNDEFINED([ 0 ]));
      assertFalse(AQL_IS_UNDEFINED([ 0, 1 ]));
      assertFalse(AQL_IS_UNDEFINED([ 1, 2 ]));
      assertFalse(AQL_IS_UNDEFINED({ }));
      assertFalse(AQL_IS_UNDEFINED({ 'a' : 0 }));
      assertFalse(AQL_IS_UNDEFINED({ 'a' : 1 }));
      assertFalse(AQL_IS_UNDEFINED({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_NULL function
////////////////////////////////////////////////////////////////////////////////

    testIsNullTrue : function () {
      assertTrue(AQL_IS_NULL(null));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_NULL function
////////////////////////////////////////////////////////////////////////////////

    testIsNullFalse : function () {
      assertFalse(AQL_IS_NULL(0));
      assertFalse(AQL_IS_NULL(1));
      assertFalse(AQL_IS_NULL(-1));
      assertFalse(AQL_IS_NULL(0.1));
      assertFalse(AQL_IS_NULL(-0.1));
      assertFalse(AQL_IS_NULL(NaN));
      assertFalse(AQL_IS_NULL(1.3e308 * 1.3e308));
      assertFalse(AQL_IS_NULL(false));
      assertFalse(AQL_IS_NULL(true));
      assertFalse(AQL_IS_NULL(undefined));
      assertFalse(AQL_IS_NULL('abc'));
      assertFalse(AQL_IS_NULL('null'));
      assertFalse(AQL_IS_NULL('false'));
      assertFalse(AQL_IS_NULL('undefined'));
      assertFalse(AQL_IS_NULL(''));
      assertFalse(AQL_IS_NULL(' '));
      assertFalse(AQL_IS_NULL([ ]));
      assertFalse(AQL_IS_NULL([ 0 ]));
      assertFalse(AQL_IS_NULL([ 0, 1 ]));
      assertFalse(AQL_IS_NULL([ 1, 2 ]));
      assertFalse(AQL_IS_NULL({ }));
      assertFalse(AQL_IS_NULL({ 'a' : 0 }));
      assertFalse(AQL_IS_NULL({ 'a' : 1 }));
      assertFalse(AQL_IS_NULL({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_BOOL function
////////////////////////////////////////////////////////////////////////////////

    testIsBoolTrue : function () {
      assertTrue(AQL_IS_BOOL(false));
      assertTrue(AQL_IS_BOOL(true));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_BOOL function
////////////////////////////////////////////////////////////////////////////////

    testIsBoolFalse : function () {
      assertFalse(AQL_IS_BOOL(undefined));
      assertFalse(AQL_IS_BOOL(NaN));
      assertFalse(AQL_IS_BOOL(1.3e308 * 1.3e308));
      assertFalse(AQL_IS_BOOL(0));
      assertFalse(AQL_IS_BOOL(1));
      assertFalse(AQL_IS_BOOL(-1));
      assertFalse(AQL_IS_BOOL(0.1));
      assertFalse(AQL_IS_BOOL(-0.1));
      assertFalse(AQL_IS_BOOL(null));
      assertFalse(AQL_IS_BOOL('abc'));
      assertFalse(AQL_IS_BOOL('null'));
      assertFalse(AQL_IS_BOOL('false'));
      assertFalse(AQL_IS_BOOL('undefined'));
      assertFalse(AQL_IS_BOOL(''));
      assertFalse(AQL_IS_BOOL(' '));
      assertFalse(AQL_IS_BOOL([ ]));
      assertFalse(AQL_IS_BOOL([ 0 ]));
      assertFalse(AQL_IS_BOOL([ 0, 1 ]));
      assertFalse(AQL_IS_BOOL([ 1, 2 ]));
      assertFalse(AQL_IS_BOOL([ '' ]));
      assertFalse(AQL_IS_BOOL([ '0' ]));
      assertFalse(AQL_IS_BOOL([ '1' ]));
      assertFalse(AQL_IS_BOOL([ true ]));
      assertFalse(AQL_IS_BOOL([ false ]));
      assertFalse(AQL_IS_BOOL({ }));
      assertFalse(AQL_IS_BOOL({ 'a' : 0 }));
      assertFalse(AQL_IS_BOOL({ 'a' : 1 }));
      assertFalse(AQL_IS_BOOL({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_NUMBER function
////////////////////////////////////////////////////////////////////////////////

    testIsNumberTrue : function () {
      assertTrue(AQL_IS_NUMBER(0));
      assertTrue(AQL_IS_NUMBER(1));
      assertTrue(AQL_IS_NUMBER(-1));
      assertTrue(AQL_IS_NUMBER(0.1));
      assertTrue(AQL_IS_NUMBER(-0.1));
      assertTrue(AQL_IS_NUMBER(12.5356));
      assertTrue(AQL_IS_NUMBER(-235.26436));
      assertTrue(AQL_IS_NUMBER(-23.3e17));
      assertTrue(AQL_IS_NUMBER(563.44576e19));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_NUMBER function
////////////////////////////////////////////////////////////////////////////////

    testIsNumberFalse : function () {
      assertFalse(AQL_IS_NUMBER(false));
      assertFalse(AQL_IS_NUMBER(true));
      assertFalse(AQL_IS_NUMBER(undefined));
      assertFalse(AQL_IS_NUMBER(NaN));
      assertFalse(AQL_IS_NUMBER(1.3e308 * 1.3e308));
      assertFalse(AQL_IS_NUMBER(null));
      assertFalse(AQL_IS_NUMBER('abc'));
      assertFalse(AQL_IS_NUMBER('null'));
      assertFalse(AQL_IS_NUMBER('false'));
      assertFalse(AQL_IS_NUMBER('undefined'));
      assertFalse(AQL_IS_NUMBER(''));
      assertFalse(AQL_IS_NUMBER(' '));
      assertFalse(AQL_IS_NUMBER([ ]));
      assertFalse(AQL_IS_NUMBER([ 0 ]));
      assertFalse(AQL_IS_NUMBER([ 0, 1 ]));
      assertFalse(AQL_IS_NUMBER([ 1, 2 ]));
      assertFalse(AQL_IS_NUMBER([ '' ]));
      assertFalse(AQL_IS_NUMBER([ '0' ]));
      assertFalse(AQL_IS_NUMBER([ '1' ]));
      assertFalse(AQL_IS_NUMBER([ true ]));
      assertFalse(AQL_IS_NUMBER([ false ]));
      assertFalse(AQL_IS_NUMBER({ }));
      assertFalse(AQL_IS_NUMBER({ 'a' : 0 }));
      assertFalse(AQL_IS_NUMBER({ 'a' : 1 }));
      assertFalse(AQL_IS_NUMBER({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_STRING function
////////////////////////////////////////////////////////////////////////////////

    testIsStringTrue : function () {
      assertTrue(AQL_IS_STRING('abc'));
      assertTrue(AQL_IS_STRING('null'));
      assertTrue(AQL_IS_STRING('false'));
      assertTrue(AQL_IS_STRING('undefined'));
      assertTrue(AQL_IS_STRING(''));
      assertTrue(AQL_IS_STRING(' '));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_STRING function
////////////////////////////////////////////////////////////////////////////////

    testIsStringFalse : function () {
      assertFalse(AQL_IS_STRING(false));
      assertFalse(AQL_IS_STRING(true));
      assertFalse(AQL_IS_STRING(undefined));
      assertFalse(AQL_IS_STRING(NaN));
      assertFalse(AQL_IS_STRING(1.3e308 * 1.3e308));
      assertFalse(AQL_IS_STRING(0));
      assertFalse(AQL_IS_STRING(1));
      assertFalse(AQL_IS_STRING(-1));
      assertFalse(AQL_IS_STRING(0.1));
      assertFalse(AQL_IS_STRING(-0.1));
      assertFalse(AQL_IS_STRING(null));
      assertFalse(AQL_IS_STRING([ ]));
      assertFalse(AQL_IS_STRING([ 0 ]));
      assertFalse(AQL_IS_STRING([ 0, 1 ]));
      assertFalse(AQL_IS_STRING([ 1, 2 ]));
      assertFalse(AQL_IS_STRING([ '' ]));
      assertFalse(AQL_IS_STRING([ '0' ]));
      assertFalse(AQL_IS_STRING([ '1' ]));
      assertFalse(AQL_IS_STRING([ true ]));
      assertFalse(AQL_IS_STRING([ false ]));
      assertFalse(AQL_IS_STRING({ }));
      assertFalse(AQL_IS_STRING({ 'a' : 0 }));
      assertFalse(AQL_IS_STRING({ 'a' : 1 }));
      assertFalse(AQL_IS_STRING({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_ARRAY function
////////////////////////////////////////////////////////////////////////////////

    testIsArrayTrue : function () {
      assertTrue(AQL_IS_ARRAY([ ]));
      assertTrue(AQL_IS_ARRAY([ 0 ]));
      assertTrue(AQL_IS_ARRAY([ 0, 1 ]));
      assertTrue(AQL_IS_ARRAY([ 1, 2 ]));
      assertTrue(AQL_IS_ARRAY([ '' ]));
      assertTrue(AQL_IS_ARRAY([ '0' ]));
      assertTrue(AQL_IS_ARRAY([ '1' ]));
      assertTrue(AQL_IS_ARRAY([ true ]));
      assertTrue(AQL_IS_ARRAY([ false ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_ARRAY function
////////////////////////////////////////////////////////////////////////////////

    testIsArrayFalse : function () {
      assertFalse(AQL_IS_ARRAY('abc'));
      assertFalse(AQL_IS_ARRAY('null'));
      assertFalse(AQL_IS_ARRAY('false'));
      assertFalse(AQL_IS_ARRAY('undefined'));
      assertFalse(AQL_IS_ARRAY(''));
      assertFalse(AQL_IS_ARRAY(' '));
      assertFalse(AQL_IS_ARRAY(false));
      assertFalse(AQL_IS_ARRAY(true));
      assertFalse(AQL_IS_ARRAY(undefined));
      assertFalse(AQL_IS_ARRAY(NaN));
      assertFalse(AQL_IS_ARRAY(1.3e308 * 1.3e308));
      assertFalse(AQL_IS_ARRAY(0));
      assertFalse(AQL_IS_ARRAY(1));
      assertFalse(AQL_IS_ARRAY(-1));
      assertFalse(AQL_IS_ARRAY(0.1));
      assertFalse(AQL_IS_ARRAY(-0.1));
      assertFalse(AQL_IS_ARRAY(null));
      assertFalse(AQL_IS_ARRAY({ }));
      assertFalse(AQL_IS_ARRAY({ 'a' : 0 }));
      assertFalse(AQL_IS_ARRAY({ 'a' : 1 }));
      assertFalse(AQL_IS_ARRAY({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_OBJECT function
////////////////////////////////////////////////////////////////////////////////

    testIsObjectTrue : function () {
      assertTrue(AQL_IS_OBJECT({ }));
      assertTrue(AQL_IS_OBJECT({ 'a' : 0 }));
      assertTrue(AQL_IS_OBJECT({ 'a' : 1 }));
      assertTrue(AQL_IS_OBJECT({ 'a' : 0, 'b' : 1 }));
      assertTrue(AQL_IS_OBJECT({ '1' : false, 'b' : false }));
      assertTrue(AQL_IS_OBJECT({ '0' : false }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_IS_OBJECT function
////////////////////////////////////////////////////////////////////////////////

    testIsObjectFalse : function () {
      assertFalse(AQL_IS_OBJECT('abc'));
      assertFalse(AQL_IS_OBJECT('null'));
      assertFalse(AQL_IS_OBJECT('false'));
      assertFalse(AQL_IS_OBJECT('undefined'));
      assertFalse(AQL_IS_OBJECT(''));
      assertFalse(AQL_IS_OBJECT(' '));
      assertFalse(AQL_IS_OBJECT(false));
      assertFalse(AQL_IS_OBJECT(true));
      assertFalse(AQL_IS_OBJECT(undefined));
      assertFalse(AQL_IS_OBJECT(NaN));
      assertFalse(AQL_IS_OBJECT(1.3e308 * 1.3e308));
      assertFalse(AQL_IS_OBJECT(0));
      assertFalse(AQL_IS_OBJECT(1));
      assertFalse(AQL_IS_OBJECT(-1));
      assertFalse(AQL_IS_OBJECT(0.1));
      assertFalse(AQL_IS_OBJECT(-0.1));
      assertFalse(AQL_IS_OBJECT(null));
      assertFalse(AQL_IS_OBJECT([ ]));
      assertFalse(AQL_IS_OBJECT([ 0 ]));
      assertFalse(AQL_IS_OBJECT([ 0, 1 ]));
      assertFalse(AQL_IS_OBJECT([ 1, 2 ]));
      assertFalse(AQL_IS_OBJECT([ '' ]));
      assertFalse(AQL_IS_OBJECT([ '0' ]));
      assertFalse(AQL_IS_OBJECT([ '1' ]));
      assertFalse(AQL_IS_OBJECT([ true ]));
      assertFalse(AQL_IS_OBJECT([ false ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_CAST_NULL function
////////////////////////////////////////////////////////////////////////////////

    testCastNull : function () {
      assertEqual(null, AQL_CAST_NULL(undefined));
      assertEqual(null, AQL_CAST_NULL(null));
      assertEqual(null, AQL_CAST_NULL(1));
      assertEqual(null, AQL_CAST_NULL(2));
      assertEqual(null, AQL_CAST_NULL(-1));
      assertEqual(null, AQL_CAST_NULL(0));
      assertEqual(null, AQL_CAST_NULL(NaN));
      assertEqual(null, AQL_CAST_NULL(true));
      assertEqual(null, AQL_CAST_NULL(false));
      assertEqual(null, AQL_CAST_NULL(''));
      assertEqual(null, AQL_CAST_NULL(' '));
      assertEqual(null, AQL_CAST_NULL(' '));
      assertEqual(null, AQL_CAST_NULL('1'));
      assertEqual(null, AQL_CAST_NULL('0'));
      assertEqual(null, AQL_CAST_NULL('-1'));
      assertEqual(null, AQL_CAST_NULL([ ]));
      assertEqual(null, AQL_CAST_NULL([ 0 ] ));
      assertEqual(null, AQL_CAST_NULL([ 0, 1 ] ));
      assertEqual(null, AQL_CAST_NULL([ 1, 2 ] ));
      assertEqual(null, AQL_CAST_NULL({ } ));
      assertEqual(null, AQL_CAST_NULL({ 'a' : true }));
      assertEqual(null, AQL_CAST_NULL({ 'a' : true, 'b' : 0 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_CAST_UNDEFINED function
////////////////////////////////////////////////////////////////////////////////

    testCastUndefined : function () {
      assertEqual(undefined, AQL_CAST_UNDEFINED(undefined));
      assertEqual(undefined, AQL_CAST_UNDEFINED(null));
      assertEqual(undefined, AQL_CAST_UNDEFINED(1));
      assertEqual(undefined, AQL_CAST_UNDEFINED(2));
      assertEqual(undefined, AQL_CAST_UNDEFINED(-1));
      assertEqual(undefined, AQL_CAST_UNDEFINED(0));
      assertEqual(undefined, AQL_CAST_UNDEFINED(NaN));
      assertEqual(undefined, AQL_CAST_UNDEFINED(true));
      assertEqual(undefined, AQL_CAST_UNDEFINED(false));
      assertEqual(undefined, AQL_CAST_UNDEFINED(''));
      assertEqual(undefined, AQL_CAST_UNDEFINED(' '));
      assertEqual(undefined, AQL_CAST_UNDEFINED('  '));
      assertEqual(undefined, AQL_CAST_UNDEFINED('1'));
      assertEqual(undefined, AQL_CAST_UNDEFINED('1 '));
      assertEqual(undefined, AQL_CAST_UNDEFINED('0'));
      assertEqual(undefined, AQL_CAST_UNDEFINED('-1'));
      assertEqual(undefined, AQL_CAST_UNDEFINED([ ]));
      assertEqual(undefined, AQL_CAST_UNDEFINED([ 0 ] ));
      assertEqual(undefined, AQL_CAST_UNDEFINED([ 0, 1 ] ));
      assertEqual(undefined, AQL_CAST_UNDEFINED([ 1, 2 ] ));
      assertEqual(undefined, AQL_CAST_UNDEFINED([ '' ]));
      assertEqual(undefined, AQL_CAST_UNDEFINED([ false ]));
      assertEqual(undefined, AQL_CAST_UNDEFINED([ true ]));
      assertEqual(undefined, AQL_CAST_UNDEFINED({ } ));
      assertEqual(undefined, AQL_CAST_UNDEFINED({ 'a' : true }));
      assertEqual(undefined, AQL_CAST_UNDEFINED({ 'a' : true, 'b' : 0 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_CAST_BOOL function
////////////////////////////////////////////////////////////////////////////////

    testCastBoolTrue : function () {
      assertEqual(true, AQL_CAST_BOOL(true));
      assertEqual(true, AQL_CAST_BOOL(1));
      assertEqual(true, AQL_CAST_BOOL(2));
      assertEqual(true, AQL_CAST_BOOL(-1));
      assertEqual(true, AQL_CAST_BOOL(100));
      assertEqual(true, AQL_CAST_BOOL(100.01));
      assertEqual(true, AQL_CAST_BOOL(0.001));
      assertEqual(true, AQL_CAST_BOOL(-0.001));
      assertEqual(true, AQL_CAST_BOOL(' '));
      assertEqual(true, AQL_CAST_BOOL('  '));
      assertEqual(true, AQL_CAST_BOOL('1'));
      assertEqual(true, AQL_CAST_BOOL('1 '));
      assertEqual(true, AQL_CAST_BOOL('0'));
      assertEqual(true, AQL_CAST_BOOL('-1'));
      assertEqual(true, AQL_CAST_BOOL([ 0 ]));
      assertEqual(true, AQL_CAST_BOOL([ 0, 1 ]));
      assertEqual(true, AQL_CAST_BOOL([ 1, 2 ]));
      assertEqual(true, AQL_CAST_BOOL([ -1, 0 ]));
      assertEqual(true, AQL_CAST_BOOL([ '' ]));
      assertEqual(true, AQL_CAST_BOOL([ false ]));
      assertEqual(true, AQL_CAST_BOOL([ true ]));
      assertEqual(true, AQL_CAST_BOOL({ 'a' : true }));
      assertEqual(true, AQL_CAST_BOOL({ 'a' : false }));
      assertEqual(true, AQL_CAST_BOOL({ 'a' : false, 'b' : 0 }));
      assertEqual(true, AQL_CAST_BOOL({ '0' : false }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_CAST_BOOL function
////////////////////////////////////////////////////////////////////////////////

    testCastBoolFalse : function () {
      assertEqual(false, AQL_CAST_BOOL(0));
      assertEqual(false, AQL_CAST_BOOL(NaN));
      assertEqual(false, AQL_CAST_BOOL(''));
      assertEqual(false, AQL_CAST_BOOL(undefined));
      assertEqual(false, AQL_CAST_BOOL(null));
      assertEqual(false, AQL_CAST_BOOL(false));
      assertEqual(false, AQL_CAST_BOOL([ ]));
      assertEqual(false, AQL_CAST_BOOL({ }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_CAST_NUMBER function
////////////////////////////////////////////////////////////////////////////////

    testCastNumber : function () {
      assertEqual(0, AQL_CAST_NUMBER(undefined));
      assertEqual(0, AQL_CAST_NUMBER(null));
      assertEqual(0, AQL_CAST_NUMBER(false));
      assertEqual(1, AQL_CAST_NUMBER(true));
      assertEqual(1, AQL_CAST_NUMBER(1));
      assertEqual(2, AQL_CAST_NUMBER(2));
      assertEqual(-1, AQL_CAST_NUMBER(-1));
      assertEqual(0, AQL_CAST_NUMBER(0));
      assertEqual(0, AQL_CAST_NUMBER(NaN));
      assertEqual(0, AQL_CAST_NUMBER(''));
      assertEqual(0, AQL_CAST_NUMBER(' '));
      assertEqual(0, AQL_CAST_NUMBER('  '));
      assertEqual(1, AQL_CAST_NUMBER('1'));
      assertEqual(1, AQL_CAST_NUMBER('1 '));
      assertEqual(0, AQL_CAST_NUMBER('0'));
      assertEqual(-1, AQL_CAST_NUMBER('-1'));
      assertEqual(-1, AQL_CAST_NUMBER('-1 '));
      assertEqual(-1, AQL_CAST_NUMBER(' -1 '));
      assertEqual(-1, AQL_CAST_NUMBER(' -1a'));
      assertEqual(1, AQL_CAST_NUMBER(' 1a'));
      assertEqual(12335.3, AQL_CAST_NUMBER(' 12335.3 a'));
      assertEqual(0, AQL_CAST_NUMBER('a1bc'));
      assertEqual(0, AQL_CAST_NUMBER('aaaa1'));
      assertEqual(0, AQL_CAST_NUMBER('-a1'));
      assertEqual(-1.255, AQL_CAST_NUMBER('-1.255'));
      assertEqual(-1.23456, AQL_CAST_NUMBER('-1.23456'));
      assertEqual(-1.23456, AQL_CAST_NUMBER('-1.23456 '));
      assertEqual(1.23456, AQL_CAST_NUMBER('  1.23456 '));
      assertEqual(1.23456, AQL_CAST_NUMBER('   1.23456a'));
      assertEqual(0, AQL_CAST_NUMBER('--1'));
      assertEqual(1, AQL_CAST_NUMBER('+1'));
      assertEqual(12.42e32, AQL_CAST_NUMBER('12.42e32'));
      assertEqual(0, AQL_CAST_NUMBER([ ]));
      assertEqual(0, AQL_CAST_NUMBER([ 0 ]));
      assertEqual(0, AQL_CAST_NUMBER([ 0, 1 ]));
      assertEqual(0, AQL_CAST_NUMBER([ 1, 2 ]));
      assertEqual(0, AQL_CAST_NUMBER([ -1, 0 ]));
      assertEqual(0, AQL_CAST_NUMBER([ 0, 1, [ 1, 2 ], [ [ 9, 4 ] ] ]));
      assertEqual(0, AQL_CAST_NUMBER([ { } ]));
      assertEqual(0, AQL_CAST_NUMBER([ 0, 1, { } ]));
      assertEqual(0, AQL_CAST_NUMBER([ { }, { } ]));
      assertEqual(0, AQL_CAST_NUMBER([ '' ]));
      assertEqual(0, AQL_CAST_NUMBER([ false ]));
      assertEqual(0, AQL_CAST_NUMBER([ true ]));
      assertEqual(0, AQL_CAST_NUMBER({ }));
      assertEqual(0, AQL_CAST_NUMBER({ 'a' : true }));
      assertEqual(0, AQL_CAST_NUMBER({ 'a' : true, 'b' : 0 }));
      assertEqual(0, AQL_CAST_NUMBER({ 'a' : { }, 'b' : { } }));
      assertEqual(0, AQL_CAST_NUMBER({ 'a' : [ ], 'b' : [ ] }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_CAST_STRING function
////////////////////////////////////////////////////////////////////////////////

    testCastString : function () {
      assertEqual('undefined', AQL_CAST_STRING(undefined));
      assertEqual('null', AQL_CAST_STRING(null));
      assertEqual('false', AQL_CAST_STRING(false));
      assertEqual('true', AQL_CAST_STRING(true));
      assertEqual('1', AQL_CAST_STRING(1));
      assertEqual('2', AQL_CAST_STRING(2));
      assertEqual('-1', AQL_CAST_STRING(-1));
      assertEqual('0', AQL_CAST_STRING(0));
      assertEqual('undefined', AQL_CAST_STRING(NaN));
      assertEqual('', AQL_CAST_STRING(''));
      assertEqual(' ', AQL_CAST_STRING(' '));
      assertEqual('  ', AQL_CAST_STRING('  '));
      assertEqual('1', AQL_CAST_STRING('1'));
      assertEqual('1 ', AQL_CAST_STRING('1 '));
      assertEqual('0', AQL_CAST_STRING('0'));
      assertEqual('-1', AQL_CAST_STRING('-1'));
      assertEqual('', AQL_CAST_STRING([ ]));
      assertEqual('0', AQL_CAST_STRING([ 0 ]));
      assertEqual('0,1', AQL_CAST_STRING([ 0, 1 ]));
      assertEqual('1,2', AQL_CAST_STRING([ 1, 2 ]));
      assertEqual('-1,0', AQL_CAST_STRING([ -1, 0 ]));
      assertEqual('0,1,1,2,9,4', AQL_CAST_STRING([ 0, 1, [ 1, 2 ], [ [ 9, 4 ] ] ]));
      assertEqual('[object Object]', AQL_CAST_STRING([ { } ]));
      assertEqual('0,1,[object Object]', AQL_CAST_STRING([ 0, 1, { } ]));
      assertEqual('[object Object],[object Object]', AQL_CAST_STRING([ { }, { } ]));
      assertEqual('', AQL_CAST_STRING([ '' ]));
      assertEqual('false', AQL_CAST_STRING([ false ]));
      assertEqual('true', AQL_CAST_STRING([ true ]));
      assertEqual('[object Object]', AQL_CAST_STRING({ }));
      assertEqual('[object Object]', AQL_CAST_STRING({ 'a' : true }));
      assertEqual('[object Object]', AQL_CAST_STRING({ 'a' : true, 'b' : 0 }));
      assertEqual('[object Object]', AQL_CAST_STRING({ 'a' : { }, 'b' : { } }));
      assertEqual('[object Object]', AQL_CAST_STRING({ 'a' : [ ], 'b' : [ ] }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_LOGICAL_AND function
////////////////////////////////////////////////////////////////////////////////

    testLogicalAndUndefined : function () {
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, null));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, true));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, false));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, 0.0));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, 1.0));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, -1.0));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, ''));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, '0'));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, '1'));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, [ ]));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, [ 0 ]));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, [ 0, 1 ]));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, [ 1, 2 ]));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, { }));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, { 'a' : 1 }));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, { '0' : false }));
      assertEqual(undefined, AQL_LOGICAL_AND(null, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND(true, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND(false, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND(0.0, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND(1.0, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND(-1.0, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND('', undefined));
      assertEqual(undefined, AQL_LOGICAL_AND('0', undefined));
      assertEqual(undefined, AQL_LOGICAL_AND('1', undefined));
      assertEqual(undefined, AQL_LOGICAL_AND([ ], undefined));
      assertEqual(undefined, AQL_LOGICAL_AND([ 0 ], undefined));
      assertEqual(undefined, AQL_LOGICAL_AND([ 0, 1 ], undefined));
      assertEqual(undefined, AQL_LOGICAL_AND([ 1, 2 ], undefined));
      assertEqual(undefined, AQL_LOGICAL_AND({ }, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND({ 'a' : 1 }, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND({ '0' : false }, undefined));
      
      assertEqual(undefined, AQL_LOGICAL_AND(true, null));
      assertEqual(undefined, AQL_LOGICAL_AND(null, true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, ''));
      assertEqual(undefined, AQL_LOGICAL_AND('', true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, ' '));
      assertEqual(undefined, AQL_LOGICAL_AND(' ', true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, '0'));
      assertEqual(undefined, AQL_LOGICAL_AND('0', true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, '1'));
      assertEqual(undefined, AQL_LOGICAL_AND('1', true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, 'true'));
      assertEqual(undefined, AQL_LOGICAL_AND('true', true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, 'false'));
      assertEqual(undefined, AQL_LOGICAL_AND('false', true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, 0));
      assertEqual(undefined, AQL_LOGICAL_AND(0, true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, 1));
      assertEqual(undefined, AQL_LOGICAL_AND(1, true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, -1));
      assertEqual(undefined, AQL_LOGICAL_AND(-1, true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, 1.1));
      assertEqual(undefined, AQL_LOGICAL_AND(1.1, true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, [ ]));
      assertEqual(undefined, AQL_LOGICAL_AND([ ], true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, [ 0 ]));
      assertEqual(undefined, AQL_LOGICAL_AND([ 0 ], true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, [ 0, 1 ]));
      assertEqual(undefined, AQL_LOGICAL_AND([ 0, 1 ], true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, [ true ]));
      assertEqual(undefined, AQL_LOGICAL_AND([ true ], true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, [ false ]));
      assertEqual(undefined, AQL_LOGICAL_AND([ false ], true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, { }));
      assertEqual(undefined, AQL_LOGICAL_AND({ }, true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, { 'a' : true }));
      assertEqual(undefined, AQL_LOGICAL_AND({ 'a' : true }, true));
      assertEqual(undefined, AQL_LOGICAL_AND(true, { 'a' : true, 'b' : false }));
      assertEqual(undefined, AQL_LOGICAL_AND({ 'a' : true, 'b' : false }, true));
      
      assertEqual(undefined, AQL_LOGICAL_AND(false, null));
      assertEqual(undefined, AQL_LOGICAL_AND(null, false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, ''));
      assertEqual(undefined, AQL_LOGICAL_AND('', false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, ' '));
      assertEqual(undefined, AQL_LOGICAL_AND(' ', false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, '0'));
      assertEqual(undefined, AQL_LOGICAL_AND('0', false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, '1'));
      assertEqual(undefined, AQL_LOGICAL_AND('1', false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, 'true'));
      assertEqual(undefined, AQL_LOGICAL_AND('true', false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, 'false'));
      assertEqual(undefined, AQL_LOGICAL_AND('false', false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, 0));
      assertEqual(undefined, AQL_LOGICAL_AND(0, false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, 1));
      assertEqual(undefined, AQL_LOGICAL_AND(1, false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, -1));
      assertEqual(undefined, AQL_LOGICAL_AND(-1, false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, 1.1));
      assertEqual(undefined, AQL_LOGICAL_AND(1.1, false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, [ ]));
      assertEqual(undefined, AQL_LOGICAL_AND([ ], false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, [ 0 ]));
      assertEqual(undefined, AQL_LOGICAL_AND([ 0 ], false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, [ 0, 1 ]));
      assertEqual(undefined, AQL_LOGICAL_AND([ 0, 1 ], false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, [ true ]));
      assertEqual(undefined, AQL_LOGICAL_AND([ false ], true));
      assertEqual(undefined, AQL_LOGICAL_AND(false, [ false ]));
      assertEqual(undefined, AQL_LOGICAL_AND([ false ], false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, { }));
      assertEqual(undefined, AQL_LOGICAL_AND({ }, false));
      assertEqual(undefined, AQL_LOGICAL_AND(false, { 'a' : true }));
      assertEqual(undefined, AQL_LOGICAL_AND({ 'a' : false }, true));
      assertEqual(undefined, AQL_LOGICAL_AND(false, { 'a' : true, 'b' : false }));
      assertEqual(undefined, AQL_LOGICAL_AND({ 'a' : false, 'b' : false }, true));
      assertEqual(undefined, AQL_LOGICAL_AND(NaN, NaN));
      assertEqual(undefined, AQL_LOGICAL_AND(NaN, 0));
      assertEqual(undefined, AQL_LOGICAL_AND(NaN, true));
      assertEqual(undefined, AQL_LOGICAL_AND(NaN, false));
      assertEqual(undefined, AQL_LOGICAL_AND(NaN, null));
      assertEqual(undefined, AQL_LOGICAL_AND(NaN, undefined));
      assertEqual(undefined, AQL_LOGICAL_AND(NaN, ''));
      assertEqual(undefined, AQL_LOGICAL_AND(NaN, '0'));
      assertEqual(undefined, AQL_LOGICAL_AND(0, NaN));
      assertEqual(undefined, AQL_LOGICAL_AND(true, NaN));
      assertEqual(undefined, AQL_LOGICAL_AND(false, NaN));
      assertEqual(undefined, AQL_LOGICAL_AND(null, NaN));
      assertEqual(undefined, AQL_LOGICAL_AND(undefined, NaN));
      assertEqual(undefined, AQL_LOGICAL_AND('', NaN));
      assertEqual(undefined, AQL_LOGICAL_AND('0', NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_LOGICAL_AND function
////////////////////////////////////////////////////////////////////////////////

    testLogicalAndBool : function () {
      assertTrue(AQL_LOGICAL_AND(true, true));
      assertFalse(AQL_LOGICAL_AND(true, false));
      assertFalse(AQL_LOGICAL_AND(false, true));
      assertFalse(AQL_LOGICAL_AND(false, false));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_LOGICAL_OR function
////////////////////////////////////////////////////////////////////////////////

    testLogicalOrUndefined : function () {
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, null));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, true));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, false));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, 0.0));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, 1.0));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, -1.0));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, ''));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, '0'));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, '1'));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, [ ]));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, [ 0 ]));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, [ 0, 1 ]));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, [ 1, 2 ]));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, { }));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, { 'a' : 1 }));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, { '0' : false }));
      assertEqual(undefined, AQL_LOGICAL_OR(null, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR(true, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR(false, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR(0.0, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR(1.0, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR(-1.0, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR('', undefined));
      assertEqual(undefined, AQL_LOGICAL_OR('0', undefined));
      assertEqual(undefined, AQL_LOGICAL_OR('1', undefined));
      assertEqual(undefined, AQL_LOGICAL_OR([ ], undefined));
      assertEqual(undefined, AQL_LOGICAL_OR([ 0 ], undefined));
      assertEqual(undefined, AQL_LOGICAL_OR([ 0, 1 ], undefined));
      assertEqual(undefined, AQL_LOGICAL_OR([ 1, 2 ], undefined));
      assertEqual(undefined, AQL_LOGICAL_OR({ }, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR({ 'a' : 1 }, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR({ '0' : false }, undefined));
      
      assertEqual(undefined, AQL_LOGICAL_OR(true, null));
      assertEqual(undefined, AQL_LOGICAL_OR(null, true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, ''));
      assertEqual(undefined, AQL_LOGICAL_OR('', true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, ' '));
      assertEqual(undefined, AQL_LOGICAL_OR(' ', true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, '0'));
      assertEqual(undefined, AQL_LOGICAL_OR('0', true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, '1'));
      assertEqual(undefined, AQL_LOGICAL_OR('1', true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, 'true'));
      assertEqual(undefined, AQL_LOGICAL_OR('true', true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, 'false'));
      assertEqual(undefined, AQL_LOGICAL_OR('false', true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, 0));
      assertEqual(undefined, AQL_LOGICAL_OR(0, true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, 1));
      assertEqual(undefined, AQL_LOGICAL_OR(1, true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, -1));
      assertEqual(undefined, AQL_LOGICAL_OR(-1, true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, 1.1));
      assertEqual(undefined, AQL_LOGICAL_OR(1.1, true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, [ ]));
      assertEqual(undefined, AQL_LOGICAL_OR([ ], true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, [ 0 ]));
      assertEqual(undefined, AQL_LOGICAL_OR([ 0 ], true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, [ 0, 1 ]));
      assertEqual(undefined, AQL_LOGICAL_OR([ 0, 1 ], true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, [ true ]));
      assertEqual(undefined, AQL_LOGICAL_OR([ true ], true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, [ false ]));
      assertEqual(undefined, AQL_LOGICAL_OR([ false ], true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, { }));
      assertEqual(undefined, AQL_LOGICAL_OR({ }, true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, { 'a' : true }));
      assertEqual(undefined, AQL_LOGICAL_OR({ 'a' : true }, true));
      assertEqual(undefined, AQL_LOGICAL_OR(true, { 'a' : true, 'b' : false }));
      assertEqual(undefined, AQL_LOGICAL_OR({ 'a' : true, 'b' : false }, true));
      
      assertEqual(undefined, AQL_LOGICAL_OR(false, null));
      assertEqual(undefined, AQL_LOGICAL_OR(null, false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, ''));
      assertEqual(undefined, AQL_LOGICAL_OR('', false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, ' '));
      assertEqual(undefined, AQL_LOGICAL_OR(' ', false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, '0'));
      assertEqual(undefined, AQL_LOGICAL_OR('0', false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, '1'));
      assertEqual(undefined, AQL_LOGICAL_OR('1', false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, 'true'));
      assertEqual(undefined, AQL_LOGICAL_OR('true', false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, 'false'));
      assertEqual(undefined, AQL_LOGICAL_OR('false', false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, 0));
      assertEqual(undefined, AQL_LOGICAL_OR(0, false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, 1));
      assertEqual(undefined, AQL_LOGICAL_OR(1, false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, -1));
      assertEqual(undefined, AQL_LOGICAL_OR(-1, false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, 1.1));
      assertEqual(undefined, AQL_LOGICAL_OR(1.1, false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, [ ]));
      assertEqual(undefined, AQL_LOGICAL_OR([ ], false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, [ 0 ]));
      assertEqual(undefined, AQL_LOGICAL_OR([ 0 ], false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, [ 0, 1 ]));
      assertEqual(undefined, AQL_LOGICAL_OR([ 0, 1 ], false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, [ true ]));
      assertEqual(undefined, AQL_LOGICAL_OR([ false ], true));
      assertEqual(undefined, AQL_LOGICAL_OR(false, [ false ]));
      assertEqual(undefined, AQL_LOGICAL_OR([ false ], false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, { }));
      assertEqual(undefined, AQL_LOGICAL_OR({ }, false));
      assertEqual(undefined, AQL_LOGICAL_OR(false, { 'a' : true }));
      assertEqual(undefined, AQL_LOGICAL_OR({ 'a' : false }, true));
      assertEqual(undefined, AQL_LOGICAL_OR(false, { 'a' : true, 'b' : false }));
      assertEqual(undefined, AQL_LOGICAL_OR({ 'a' : false, 'b' : false }, true));
      assertEqual(undefined, AQL_LOGICAL_OR(NaN, NaN));
      assertEqual(undefined, AQL_LOGICAL_OR(NaN, 0));
      assertEqual(undefined, AQL_LOGICAL_OR(NaN, true));
      assertEqual(undefined, AQL_LOGICAL_OR(NaN, false));
      assertEqual(undefined, AQL_LOGICAL_OR(NaN, null));
      assertEqual(undefined, AQL_LOGICAL_OR(NaN, undefined));
      assertEqual(undefined, AQL_LOGICAL_OR(NaN, ''));
      assertEqual(undefined, AQL_LOGICAL_OR(NaN, '0'));
      assertEqual(undefined, AQL_LOGICAL_OR(0, NaN));
      assertEqual(undefined, AQL_LOGICAL_OR(true, NaN));
      assertEqual(undefined, AQL_LOGICAL_OR(false, NaN));
      assertEqual(undefined, AQL_LOGICAL_OR(null, NaN));
      assertEqual(undefined, AQL_LOGICAL_OR(undefined, NaN));
      assertEqual(undefined, AQL_LOGICAL_OR('', NaN));
      assertEqual(undefined, AQL_LOGICAL_OR('0', NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_LOGICAL_OR function
////////////////////////////////////////////////////////////////////////////////

    testLogicalOrBool : function () {
      assertTrue(AQL_LOGICAL_OR(true, true));
      assertTrue(AQL_LOGICAL_OR(true, false));
      assertTrue(AQL_LOGICAL_OR(false, true));
      assertFalse(AQL_LOGICAL_OR(false, false));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_LOGICAL_NOT function
////////////////////////////////////////////////////////////////////////////////

    testLogicalNotUndefined : function () {
      assertEqual(undefined, AQL_LOGICAL_NOT(undefined));
      assertEqual(undefined, AQL_LOGICAL_NOT(null));
      assertEqual(undefined, AQL_LOGICAL_NOT(0.0));
      assertEqual(undefined, AQL_LOGICAL_NOT(1.0));
      assertEqual(undefined, AQL_LOGICAL_NOT(-1.0));
      assertEqual(undefined, AQL_LOGICAL_NOT(''));
      assertEqual(undefined, AQL_LOGICAL_NOT('0'));
      assertEqual(undefined, AQL_LOGICAL_NOT('1'));
      assertEqual(undefined, AQL_LOGICAL_NOT([ ]));
      assertEqual(undefined, AQL_LOGICAL_NOT([ 0 ]));
      assertEqual(undefined, AQL_LOGICAL_NOT([ 0, 1 ]));
      assertEqual(undefined, AQL_LOGICAL_NOT([ 1, 2 ]));
      assertEqual(undefined, AQL_LOGICAL_NOT({ }));
      assertEqual(undefined, AQL_LOGICAL_NOT({ 'a' : 0 }));
      assertEqual(undefined, AQL_LOGICAL_NOT({ 'a' : 1 }));
      assertEqual(undefined, AQL_LOGICAL_NOT({ '0' : false}));
      assertEqual(undefined, AQL_LOGICAL_NOT(NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_LOGICAL_NOT function
////////////////////////////////////////////////////////////////////////////////

    testLogicalNotBool : function () {
      assertTrue(AQL_LOGICAL_NOT(false));
      assertFalse(AQL_LOGICAL_NOT(true));

      assertTrue(AQL_LOGICAL_NOT(AQL_LOGICAL_NOT(true)));
      assertFalse(AQL_LOGICAL_NOT(AQL_LOGICAL_NOT(false)));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_EQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalEqualUndefined : function () {
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, null));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, true));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, false));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, 0.0));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, 1.0));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, -1.0));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, ''));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, '0'));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, '1'));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, [ ]));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, [ 0 ]));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, [ 0, 1 ]));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, [ 1, 2 ]));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, { }));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, { 'a' : 1 }));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(undefined, { '0' : false }));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(null, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(true, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(false, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(0.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(-1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL('', undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL('0', undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL('1', undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL([ ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL([ 0 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL([ 0, 1 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL([ 1, 2 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL({ }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL({ 'a' : 1 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL({ '0' : false }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_EQUAL(NaN, NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_EQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalEqualTrue : function () {
      assertTrue(AQL_RELATIONAL_EQUAL(1, 1));
      assertTrue(AQL_RELATIONAL_EQUAL(0, 0));
      assertTrue(AQL_RELATIONAL_EQUAL(-1, -1));
      assertTrue(AQL_RELATIONAL_EQUAL(1.345, 1.345));
      assertTrue(AQL_RELATIONAL_EQUAL(1.0, 1.00));
      assertTrue(AQL_RELATIONAL_EQUAL(1.0, 1.000));
      assertTrue(AQL_RELATIONAL_EQUAL(1.1, 1.1));
      assertTrue(AQL_RELATIONAL_EQUAL(1.01, 1.01));
      assertTrue(AQL_RELATIONAL_EQUAL(1.001, 1.001));
      assertTrue(AQL_RELATIONAL_EQUAL(1.0001, 1.0001));
      assertTrue(AQL_RELATIONAL_EQUAL(1.00001, 1.00001));
      assertTrue(AQL_RELATIONAL_EQUAL(1.000001, 1.000001));
      assertTrue(AQL_RELATIONAL_EQUAL(1.245e307, 1.245e307));
      assertTrue(AQL_RELATIONAL_EQUAL(-99.43423, -99.43423));
      assertTrue(AQL_RELATIONAL_EQUAL(true, true));
      assertTrue(AQL_RELATIONAL_EQUAL(false, false));
      assertTrue(AQL_RELATIONAL_EQUAL('', ''));
      assertTrue(AQL_RELATIONAL_EQUAL(' ', ' '));
      assertTrue(AQL_RELATIONAL_EQUAL(' 1', ' 1'));
      assertTrue(AQL_RELATIONAL_EQUAL('0', '0'));
      assertTrue(AQL_RELATIONAL_EQUAL('abc', 'abc'));
      assertTrue(AQL_RELATIONAL_EQUAL('-1', '-1'));
      assertTrue(AQL_RELATIONAL_EQUAL(null, null));
      assertTrue(AQL_RELATIONAL_EQUAL('true', 'true'));
      assertTrue(AQL_RELATIONAL_EQUAL('false', 'false'));
      assertTrue(AQL_RELATIONAL_EQUAL('undefined', 'undefined'));
      assertTrue(AQL_RELATIONAL_EQUAL('null', 'null'));
      assertTrue(AQL_RELATIONAL_EQUAL([ ], [ ]));
      assertTrue(AQL_RELATIONAL_EQUAL([ 0 ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_EQUAL([ 0, 1 ], [ 0, 1 ]));
      assertTrue(AQL_RELATIONAL_EQUAL([ 0, 1, 4 ], [ 0, 1, 4 ]));
      assertTrue(AQL_RELATIONAL_EQUAL([ 3, 4, -99 ], [ 3, 4, -99 ]));
      assertTrue(AQL_RELATIONAL_EQUAL([ 'a', 4, [ 1, 'a' ], false ], [ 'a', 4, [ 1, 'a' ], false ]));
      assertTrue(AQL_RELATIONAL_EQUAL({ }, { }));
      assertTrue(AQL_RELATIONAL_EQUAL({ 'a' : true }, { 'a' : true }));
      assertTrue(AQL_RELATIONAL_EQUAL({ 'a' : true, 'b': true }, { 'a' : true, 'b': true }));
      assertTrue(AQL_RELATIONAL_EQUAL({ 'a' : true, 'b': true }, { 'b' : true, 'a': true }));
      assertTrue(AQL_RELATIONAL_EQUAL({ 'b' : true, 'a': true }, { 'b' : true, 'a': true }));
      assertTrue(AQL_RELATIONAL_EQUAL({ 'b' : true, 'a': true }, { 'a' : true, 'b': true }));
      assertTrue(AQL_RELATIONAL_EQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }));
      assertTrue(AQL_RELATIONAL_EQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'b' : [ 1, 9 ], 'a' : [ 0, 1 ] }));
      assertTrue(AQL_RELATIONAL_EQUAL({ 'f' : { 'c' : { 'd' : [ 0, 1 ], 'a' : [ 1, 9 ] }, 'a' : false }, 'a' : true }, { 'a' : true, 'f' : { 'a' : false, 'c' : { 'a' : [ 1, 9 ], 'd' : [ 0, 1 ] } } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_EQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalEqualFalse : function () {
      assertFalse(AQL_RELATIONAL_EQUAL(1, 0));
      assertFalse(AQL_RELATIONAL_EQUAL(0, 1));
      assertFalse(AQL_RELATIONAL_EQUAL(0, false));
      assertFalse(AQL_RELATIONAL_EQUAL(false, 0));
      assertFalse(AQL_RELATIONAL_EQUAL(false, 0));
      assertFalse(AQL_RELATIONAL_EQUAL(-1, 1));
      assertFalse(AQL_RELATIONAL_EQUAL(1, -1));
      assertFalse(AQL_RELATIONAL_EQUAL(-1, 0));
      assertFalse(AQL_RELATIONAL_EQUAL(0, -1));
      assertFalse(AQL_RELATIONAL_EQUAL(true, false));
      assertFalse(AQL_RELATIONAL_EQUAL(false, true));
      assertFalse(AQL_RELATIONAL_EQUAL(true, 1));
      assertFalse(AQL_RELATIONAL_EQUAL(1, true));
      assertFalse(AQL_RELATIONAL_EQUAL(0, true));
      assertFalse(AQL_RELATIONAL_EQUAL(true, 0));
      assertFalse(AQL_RELATIONAL_EQUAL(true, 'true'));
      assertFalse(AQL_RELATIONAL_EQUAL(false, 'false'));
      assertFalse(AQL_RELATIONAL_EQUAL('true', true));
      assertFalse(AQL_RELATIONAL_EQUAL('false', false));
      assertFalse(AQL_RELATIONAL_EQUAL(-1.345, 1.345));
      assertFalse(AQL_RELATIONAL_EQUAL(1.345, -1.345));
      assertFalse(AQL_RELATIONAL_EQUAL(1.345, 1.346));
      assertFalse(AQL_RELATIONAL_EQUAL(1.346, 1.345));
      assertFalse(AQL_RELATIONAL_EQUAL(1.344, 1.345));
      assertFalse(AQL_RELATIONAL_EQUAL(1.345, 1.344));
      assertFalse(AQL_RELATIONAL_EQUAL(1, 2));
      assertFalse(AQL_RELATIONAL_EQUAL(2, 1));
      assertFalse(AQL_RELATIONAL_EQUAL(1.246e307, 1.245e307));
      assertFalse(AQL_RELATIONAL_EQUAL(1.246e307, 1.247e307));
      assertFalse(AQL_RELATIONAL_EQUAL(1.246e307, 1.2467e307));
      assertFalse(AQL_RELATIONAL_EQUAL(-99.43423, -99.434233));
      assertFalse(AQL_RELATIONAL_EQUAL(1.00001, 1.000001));
      assertFalse(AQL_RELATIONAL_EQUAL(1.00001, 1.0001));
      assertFalse(AQL_RELATIONAL_EQUAL(null, 1));
      assertFalse(AQL_RELATIONAL_EQUAL(1, null));
      assertFalse(AQL_RELATIONAL_EQUAL(null, 0));
      assertFalse(AQL_RELATIONAL_EQUAL(0, null));
      assertFalse(AQL_RELATIONAL_EQUAL(null, ''));
      assertFalse(AQL_RELATIONAL_EQUAL('', null));
      assertFalse(AQL_RELATIONAL_EQUAL(null, '0'));
      assertFalse(AQL_RELATIONAL_EQUAL('0', null));
      assertFalse(AQL_RELATIONAL_EQUAL(null, false));
      assertFalse(AQL_RELATIONAL_EQUAL(false, null));
      assertFalse(AQL_RELATIONAL_EQUAL(null, true));
      assertFalse(AQL_RELATIONAL_EQUAL(true, null));
      assertFalse(AQL_RELATIONAL_EQUAL(null, 'null'));
      assertFalse(AQL_RELATIONAL_EQUAL('null', null));
      assertFalse(AQL_RELATIONAL_EQUAL(0, ''));
      assertFalse(AQL_RELATIONAL_EQUAL('', 0));
      assertFalse(AQL_RELATIONAL_EQUAL(1, ''));
      assertFalse(AQL_RELATIONAL_EQUAL('', 1));
      assertFalse(AQL_RELATIONAL_EQUAL(' ', ''));
      assertFalse(AQL_RELATIONAL_EQUAL('', ' '));
      assertFalse(AQL_RELATIONAL_EQUAL(' 1', '1'));
      assertFalse(AQL_RELATIONAL_EQUAL('1', ' 1'));
      assertFalse(AQL_RELATIONAL_EQUAL('1 ', '1'));
      assertFalse(AQL_RELATIONAL_EQUAL('1', '1 '));
      assertFalse(AQL_RELATIONAL_EQUAL('1 ', ' 1'));
      assertFalse(AQL_RELATIONAL_EQUAL(' 1', '1 '));
      assertFalse(AQL_RELATIONAL_EQUAL(' 1 ', '1'));
      assertFalse(AQL_RELATIONAL_EQUAL('0', ''));
      assertFalse(AQL_RELATIONAL_EQUAL('', ' '));
      assertFalse(AQL_RELATIONAL_EQUAL('abc', 'abcd'));
      assertFalse(AQL_RELATIONAL_EQUAL('abcd', 'abc'));
      assertFalse(AQL_RELATIONAL_EQUAL('dabc', 'abcd'));
      assertFalse(AQL_RELATIONAL_EQUAL('1', 1));
      assertFalse(AQL_RELATIONAL_EQUAL(1, '1'));
      assertFalse(AQL_RELATIONAL_EQUAL('0', 0));
      assertFalse(AQL_RELATIONAL_EQUAL('1.0', 1.0));
      assertFalse(AQL_RELATIONAL_EQUAL('1.0', 1));
      assertFalse(AQL_RELATIONAL_EQUAL('-1', -1));
      assertFalse(AQL_RELATIONAL_EQUAL('1.234', 1.234));
      assertFalse(AQL_RELATIONAL_EQUAL('NaN', NaN));
      assertFalse(AQL_RELATIONAL_EQUAL([ 0 ], [ ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ ], [ 0, 1 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 0 ], [ 0, 1 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 1, 1, 0 ], [ 1, 0, 1 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 1, 0, 0 ], [ 1, 0, 1 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 1, 0, 0 ], [ 1, 0 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 1, 0 ], [ 1, 0, 0 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 1, 0 ], [ 0, 1 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 1, 0 ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 1, 0 ], [ 1 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 1, 2, 3 ], [ 3, 2, 1 ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ [ 1 ] ], [ [ 0 ] ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 1, [ 1 , 0 ] ], [ 1, [ 0, 1 ] ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ 1, [ 1 , 0, [ ] ] ], [ 1, [ [ ], 1, 0 ] ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ '' ], false));
      assertFalse(AQL_RELATIONAL_EQUAL([ '' ], ''));
      assertFalse(AQL_RELATIONAL_EQUAL([ '' ], [ ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ true ], [ ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ true ], [ false ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ false ], [ ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ null ], [ ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ null ], [ false ]));
      assertFalse(AQL_RELATIONAL_EQUAL([ ], null));
      assertFalse(AQL_RELATIONAL_EQUAL([ ], ''));
      assertFalse(AQL_RELATIONAL_EQUAL({ }, { 'a' : false }));
      assertFalse(AQL_RELATIONAL_EQUAL({ 'a' : false }, { }));
      assertFalse(AQL_RELATIONAL_EQUAL({ 'a' : true }, { 'a' : false }));
      assertFalse(AQL_RELATIONAL_EQUAL({ 'a' : true }, { 'b' : true }));
      assertFalse(AQL_RELATIONAL_EQUAL({ 'b' : true }, { 'a' : true }));
      assertFalse(AQL_RELATIONAL_EQUAL({ 'a' : true, 'b' : [ 0, 1 ] }, { 'a' : true, 'b' : [ 1, 0 ] }));
      assertFalse(AQL_RELATIONAL_EQUAL({ 'a' : true, 'b' : { 'a' : false, 'b' : true } }, { 'a' : true, 'b' : { 'a' : true, 'b': true } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_UNEQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalUnequalUndefined : function () {
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, null));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, true));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, false));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, 0.0));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, 1.0));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, -1.0));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, ''));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, '0'));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, '1'));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, [ ]));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, [ 0 ]));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, [ 0, 1 ]));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, [ 1, 2 ]));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, { }));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, { 'a' : 1 }));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, { '0' : false }));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(null, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(true, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(false, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(0.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(-1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL('', undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL('0', undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL('1', undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL([ ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL([ 0 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL([ 0, 1 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL([ 1, 2 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL({ }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL({ 'a' : 1 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL({ '0' : false }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(NaN, false));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(NaN, true));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(NaN, ''));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(NaN, 0));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(NaN, null));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(NaN, undefined));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(false, NaN));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(true, NaN));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL('', NaN));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(0, NaN));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(null, NaN));
      assertEqual(undefined, AQL_RELATIONAL_UNEQUAL(undefined, NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_UNEQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalUnequalTrue : function () {
      assertTrue(AQL_RELATIONAL_UNEQUAL(1, 0));
      assertTrue(AQL_RELATIONAL_UNEQUAL(0, 1));
      assertTrue(AQL_RELATIONAL_UNEQUAL(0, false));
      assertTrue(AQL_RELATIONAL_UNEQUAL(false, 0));
      assertTrue(AQL_RELATIONAL_UNEQUAL(false, 0));
      assertTrue(AQL_RELATIONAL_UNEQUAL(-1, 1));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1, -1));
      assertTrue(AQL_RELATIONAL_UNEQUAL(-1, 0));
      assertTrue(AQL_RELATIONAL_UNEQUAL(0, -1));
      assertTrue(AQL_RELATIONAL_UNEQUAL(true, false));
      assertTrue(AQL_RELATIONAL_UNEQUAL(false, true));
      assertTrue(AQL_RELATIONAL_UNEQUAL(true, 1));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1, true));
      assertTrue(AQL_RELATIONAL_UNEQUAL(0, true));
      assertTrue(AQL_RELATIONAL_UNEQUAL(true, 0));
      assertTrue(AQL_RELATIONAL_UNEQUAL(true, 'true'));
      assertTrue(AQL_RELATIONAL_UNEQUAL(false, 'false'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('true', true));
      assertTrue(AQL_RELATIONAL_UNEQUAL('false', false));
      assertTrue(AQL_RELATIONAL_UNEQUAL(-1.345, 1.345));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1.345, -1.345));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1.345, 1.346));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1.346, 1.345));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1.344, 1.345));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1.345, 1.344));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1, 2));
      assertTrue(AQL_RELATIONAL_UNEQUAL(2, 1));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1.246e307, 1.245e307));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1.246e307, 1.247e307));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1.246e307, 1.2467e307));
      assertTrue(AQL_RELATIONAL_UNEQUAL(-99.43423, -99.434233));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1.00001, 1.000001));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1.00001, 1.0001));
      assertTrue(AQL_RELATIONAL_UNEQUAL(null, 1));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1, null));
      assertTrue(AQL_RELATIONAL_UNEQUAL(null, 0));
      assertTrue(AQL_RELATIONAL_UNEQUAL(0, null));
      assertTrue(AQL_RELATIONAL_UNEQUAL(null, ''));
      assertTrue(AQL_RELATIONAL_UNEQUAL('', null));
      assertTrue(AQL_RELATIONAL_UNEQUAL(null, '0'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('0', null));
      assertTrue(AQL_RELATIONAL_UNEQUAL(null, false));
      assertTrue(AQL_RELATIONAL_UNEQUAL(false, null));
      assertTrue(AQL_RELATIONAL_UNEQUAL(null, true));
      assertTrue(AQL_RELATIONAL_UNEQUAL(true, null));
      assertTrue(AQL_RELATIONAL_UNEQUAL(null, 'null'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('null', null));
      assertTrue(AQL_RELATIONAL_UNEQUAL(0, ''));
      assertTrue(AQL_RELATIONAL_UNEQUAL('', 0));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1, ''));
      assertTrue(AQL_RELATIONAL_UNEQUAL('', 1));
      assertTrue(AQL_RELATIONAL_UNEQUAL(' ', ''));
      assertTrue(AQL_RELATIONAL_UNEQUAL('', ' '));
      assertTrue(AQL_RELATIONAL_UNEQUAL(' 1', '1'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('1', ' 1'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('1 ', '1'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('1', '1 '));
      assertTrue(AQL_RELATIONAL_UNEQUAL('1 ', ' 1'));
      assertTrue(AQL_RELATIONAL_UNEQUAL(' 1', '1 '));
      assertTrue(AQL_RELATIONAL_UNEQUAL(' 1 ', '1'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('0', ''));
      assertTrue(AQL_RELATIONAL_UNEQUAL('', ' '));
      assertTrue(AQL_RELATIONAL_UNEQUAL('abc', 'abcd'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('abcd', 'abc'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('dabc', 'abcd'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('1', 1));
      assertTrue(AQL_RELATIONAL_UNEQUAL(1, '1'));
      assertTrue(AQL_RELATIONAL_UNEQUAL('0', 0));
      assertTrue(AQL_RELATIONAL_UNEQUAL('1.0', 1.0));
      assertTrue(AQL_RELATIONAL_UNEQUAL('1.0', 1));
      assertTrue(AQL_RELATIONAL_UNEQUAL('-1', -1));
      assertTrue(AQL_RELATIONAL_UNEQUAL('1.234', 1.234));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 0 ], [ ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ ], [ 0, 1 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 0 ], [ 0, 1 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 1, 1, 0 ], [ 1, 0, 1 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 1, 0, 0 ], [ 1, 0, 1 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 1, 0, 0 ], [ 1, 0 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 1, 0 ], [ 1, 0, 0 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 1, 0 ], [ 0, 1 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 1, 0 ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 1, 0 ], [ 1 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 1, 2, 3 ], [ 3, 2, 1 ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ [ 1 ] ], [ [ 0 ] ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 1, [ 1 , 0 ] ], [ 1, [ 0, 1 ] ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ 1, [ 1 , 0, [ ] ] ], [ 1, [ [ ], 1, 0 ] ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ '' ], false));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ '' ], ''));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ '' ], [ ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ true ], [ ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ true ], [ false ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ false ], [ ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ null ], [ ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ null ], [ false ]));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ ], null));
      assertTrue(AQL_RELATIONAL_UNEQUAL([ ], ''));
      assertTrue(AQL_RELATIONAL_UNEQUAL({ }, { 'a' : false }));
      assertTrue(AQL_RELATIONAL_UNEQUAL({ 'a' : false }, { }));
      assertTrue(AQL_RELATIONAL_UNEQUAL({ 'a' : true }, { 'a' : false }));
      assertTrue(AQL_RELATIONAL_UNEQUAL({ 'a' : true }, { 'b' : true }));
      assertTrue(AQL_RELATIONAL_UNEQUAL({ 'b' : true }, { 'a' : true }));
      assertTrue(AQL_RELATIONAL_UNEQUAL({ 'a' : true, 'b' : [ 0, 1 ] }, { 'a' : true, 'b' : [ 1, 0 ] }));
      assertTrue(AQL_RELATIONAL_UNEQUAL({ 'a' : true, 'b' : { 'a' : false, 'b' : true } }, { 'a' : true, 'b' : { 'a' : true, 'b': true } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_UNEQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalUnequalFalse : function () {
      assertFalse(AQL_RELATIONAL_UNEQUAL(1, 1));
      assertFalse(AQL_RELATIONAL_UNEQUAL(0, 0));
      assertFalse(AQL_RELATIONAL_UNEQUAL(-1, -1));
      assertFalse(AQL_RELATIONAL_UNEQUAL(1.345, 1.345));
      assertFalse(AQL_RELATIONAL_UNEQUAL(1.0, 1.00));
      assertFalse(AQL_RELATIONAL_UNEQUAL(1.0, 1.000));
      assertFalse(AQL_RELATIONAL_UNEQUAL(1.1, 1.1));
      assertFalse(AQL_RELATIONAL_UNEQUAL(1.01, 1.01));
      assertFalse(AQL_RELATIONAL_UNEQUAL(1.001, 1.001));
      assertFalse(AQL_RELATIONAL_UNEQUAL(1.0001, 1.0001));
      assertFalse(AQL_RELATIONAL_UNEQUAL(1.00001, 1.00001));
      assertFalse(AQL_RELATIONAL_UNEQUAL(1.000001, 1.000001));
      assertFalse(AQL_RELATIONAL_UNEQUAL(1.245e307, 1.245e307));
      assertFalse(AQL_RELATIONAL_UNEQUAL(-99.43423, -99.43423));
      assertFalse(AQL_RELATIONAL_UNEQUAL(true, true));
      assertFalse(AQL_RELATIONAL_UNEQUAL(false, false));
      assertFalse(AQL_RELATIONAL_UNEQUAL('', ''));
      assertFalse(AQL_RELATIONAL_UNEQUAL(' ', ' '));
      assertFalse(AQL_RELATIONAL_UNEQUAL(' 1', ' 1'));
      assertFalse(AQL_RELATIONAL_UNEQUAL('0', '0'));
      assertFalse(AQL_RELATIONAL_UNEQUAL('abc', 'abc'));
      assertFalse(AQL_RELATIONAL_UNEQUAL('-1', '-1'));
      assertFalse(AQL_RELATIONAL_UNEQUAL(null, null));
      assertFalse(AQL_RELATIONAL_UNEQUAL('true', 'true'));
      assertFalse(AQL_RELATIONAL_UNEQUAL('false', 'false'));
      assertFalse(AQL_RELATIONAL_UNEQUAL('undefined', 'undefined'));
      assertFalse(AQL_RELATIONAL_UNEQUAL('null', 'null'));
      assertFalse(AQL_RELATIONAL_UNEQUAL([ ], [ ]));
      assertFalse(AQL_RELATIONAL_UNEQUAL([ 0 ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_UNEQUAL([ 0, 1 ], [ 0, 1 ]));
      assertFalse(AQL_RELATIONAL_UNEQUAL([ 0, 1, 4 ], [ 0, 1, 4 ]));
      assertFalse(AQL_RELATIONAL_UNEQUAL([ 3, 4, -99 ], [ 3, 4, -99 ]));
      assertFalse(AQL_RELATIONAL_UNEQUAL([ 'a', 4, [ 1, 'a' ], false ], [ 'a', 4, [ 1, 'a' ], false ]));
      assertFalse(AQL_RELATIONAL_UNEQUAL({ }, { }));
      assertFalse(AQL_RELATIONAL_UNEQUAL({ 'a' : true }, { 'a' : true }));
      assertFalse(AQL_RELATIONAL_UNEQUAL({ 'a' : true, 'b': true }, { 'a' : true, 'b': true }));
      assertFalse(AQL_RELATIONAL_UNEQUAL({ 'a' : true, 'b': true }, { 'b' : true, 'a': true }));
      assertFalse(AQL_RELATIONAL_UNEQUAL({ 'b' : true, 'a': true }, { 'b' : true, 'a': true }));
      assertFalse(AQL_RELATIONAL_UNEQUAL({ 'b' : true, 'a': true }, { 'a' : true, 'b': true }));
      assertFalse(AQL_RELATIONAL_UNEQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }));
      assertFalse(AQL_RELATIONAL_UNEQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'b' : [ 1, 9 ], 'a' : [ 0, 1 ] }));
      assertFalse(AQL_RELATIONAL_UNEQUAL({ 'f' : { 'c' : { 'd' : [ 0, 1 ], 'a' : [ 1, 9 ] }, 'a' : false }, 'a' : true }, { 'a' : true, 'f' : { 'a' : false, 'c' : { 'a' : [ 1, 9 ], 'd' : [ 0, 1 ] } } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_LESS function
////////////////////////////////////////////////////////////////////////////////

    testRelationalLessUndefined : function () {
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, null));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, true));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, false));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, 0.0));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, 1.0));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, -1.0));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, ''));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, '0'));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, '1'));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, [ ]));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, [ 0 ]));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, [ 0, 1 ]));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, [ 1, 2 ]));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, { }));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, { 'a' : 1 }));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, { '0' : false }));
      assertEqual(undefined, AQL_RELATIONAL_LESS(null, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS(true, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS(false, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS(0.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS(1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS(-1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS('', undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS('0', undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS('1', undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS([ ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS([ 0 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS([ 0, 1 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS([ 1, 2 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS({ }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS({ 'a' : 1 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS({ '0' : false }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS(NaN, false));
      assertEqual(undefined, AQL_RELATIONAL_LESS(NaN, true));
      assertEqual(undefined, AQL_RELATIONAL_LESS(NaN, ''));
      assertEqual(undefined, AQL_RELATIONAL_LESS(NaN, 0));
      assertEqual(undefined, AQL_RELATIONAL_LESS(NaN, null));
      assertEqual(undefined, AQL_RELATIONAL_LESS(NaN, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESS(false, NaN));
      assertEqual(undefined, AQL_RELATIONAL_LESS(true, NaN));
      assertEqual(undefined, AQL_RELATIONAL_LESS('', NaN));
      assertEqual(undefined, AQL_RELATIONAL_LESS(0, NaN));
      assertEqual(undefined, AQL_RELATIONAL_LESS(null, NaN));
      assertEqual(undefined, AQL_RELATIONAL_LESS(undefined, NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_LESS function
////////////////////////////////////////////////////////////////////////////////

    testRelationalLessTrue : function () {
      assertTrue(AQL_RELATIONAL_LESS(null, false));
      assertTrue(AQL_RELATIONAL_LESS(null, true));
      assertTrue(AQL_RELATIONAL_LESS(null, 0));
      assertTrue(AQL_RELATIONAL_LESS(null, 1));
      assertTrue(AQL_RELATIONAL_LESS(null, -1));
      assertTrue(AQL_RELATIONAL_LESS(null, ''));
      assertTrue(AQL_RELATIONAL_LESS(null, ' '));
      assertTrue(AQL_RELATIONAL_LESS(null, '1'));
      assertTrue(AQL_RELATIONAL_LESS(null, '0'));
      assertTrue(AQL_RELATIONAL_LESS(null, 'abcd'));
      assertTrue(AQL_RELATIONAL_LESS(null, 'null'));
      assertTrue(AQL_RELATIONAL_LESS(null, [ ]));
      assertTrue(AQL_RELATIONAL_LESS(null, [ true ]));
      assertTrue(AQL_RELATIONAL_LESS(null, [ false ]));
      assertTrue(AQL_RELATIONAL_LESS(null, [ null ]));
      assertTrue(AQL_RELATIONAL_LESS(null, [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS(null, { }));
      assertTrue(AQL_RELATIONAL_LESS(null, { 'a' : null }));
      assertTrue(AQL_RELATIONAL_LESS(false, true));
      assertTrue(AQL_RELATIONAL_LESS(false, 0));
      assertTrue(AQL_RELATIONAL_LESS(false, 1));
      assertTrue(AQL_RELATIONAL_LESS(false, -1));
      assertTrue(AQL_RELATIONAL_LESS(false, ''));
      assertTrue(AQL_RELATIONAL_LESS(false, ' '));
      assertTrue(AQL_RELATIONAL_LESS(false, '1'));
      assertTrue(AQL_RELATIONAL_LESS(false, '0'));
      assertTrue(AQL_RELATIONAL_LESS(false, 'abcd'));
      assertTrue(AQL_RELATIONAL_LESS(false, 'true'));
      assertTrue(AQL_RELATIONAL_LESS(false, [ ]));
      assertTrue(AQL_RELATIONAL_LESS(false, [ true ]));
      assertTrue(AQL_RELATIONAL_LESS(false, [ false ]));
      assertTrue(AQL_RELATIONAL_LESS(false, [ null ]));
      assertTrue(AQL_RELATIONAL_LESS(false, [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS(false, { }));
      assertTrue(AQL_RELATIONAL_LESS(false, { 'a' : null }));
      assertTrue(AQL_RELATIONAL_LESS(true, 0));
      assertTrue(AQL_RELATIONAL_LESS(true, 1));
      assertTrue(AQL_RELATIONAL_LESS(true, -1));
      assertTrue(AQL_RELATIONAL_LESS(true, ''));
      assertTrue(AQL_RELATIONAL_LESS(true, ' '));
      assertTrue(AQL_RELATIONAL_LESS(true, '1'));
      assertTrue(AQL_RELATIONAL_LESS(true, '0'));
      assertTrue(AQL_RELATIONAL_LESS(true, 'abcd'));
      assertTrue(AQL_RELATIONAL_LESS(true, 'true'));
      assertTrue(AQL_RELATIONAL_LESS(true, [ ]));
      assertTrue(AQL_RELATIONAL_LESS(true, [ true ]));
      assertTrue(AQL_RELATIONAL_LESS(true, [ false ]));
      assertTrue(AQL_RELATIONAL_LESS(true, [ null ]));
      assertTrue(AQL_RELATIONAL_LESS(true, [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS(true, { }));
      assertTrue(AQL_RELATIONAL_LESS(true, { 'a' : null }));
      assertTrue(AQL_RELATIONAL_LESS(0, 1));
      assertTrue(AQL_RELATIONAL_LESS(1, 2));
      assertTrue(AQL_RELATIONAL_LESS(1, 100));
      assertTrue(AQL_RELATIONAL_LESS(20, 100));
      assertTrue(AQL_RELATIONAL_LESS(-100, 1));
      assertTrue(AQL_RELATIONAL_LESS(-100, -10));
      assertTrue(AQL_RELATIONAL_LESS(-11, -10));
      assertTrue(AQL_RELATIONAL_LESS(999, 1000));
      assertTrue(AQL_RELATIONAL_LESS(-1, 1));
      assertTrue(AQL_RELATIONAL_LESS(-1, 0));
      assertTrue(AQL_RELATIONAL_LESS(1.0, 1.01));
      assertTrue(AQL_RELATIONAL_LESS(1.111, 1.2));
      assertTrue(AQL_RELATIONAL_LESS(-1.111, -1.110));
      assertTrue(AQL_RELATIONAL_LESS(-1.111, -1.1109));
      assertTrue(AQL_RELATIONAL_LESS(0, ''));
      assertTrue(AQL_RELATIONAL_LESS(0, ' '));
      assertTrue(AQL_RELATIONAL_LESS(0, '0'));
      assertTrue(AQL_RELATIONAL_LESS(0, '1'));
      assertTrue(AQL_RELATIONAL_LESS(0, '-1'));
      assertTrue(AQL_RELATIONAL_LESS(0, 'true'));
      assertTrue(AQL_RELATIONAL_LESS(0, 'false'));
      assertTrue(AQL_RELATIONAL_LESS(0, 'null'));
      assertTrue(AQL_RELATIONAL_LESS(1, ''));
      assertTrue(AQL_RELATIONAL_LESS(1, ' '));
      assertTrue(AQL_RELATIONAL_LESS(1, '0'));
      assertTrue(AQL_RELATIONAL_LESS(1, '1'));
      assertTrue(AQL_RELATIONAL_LESS(1, '-1'));
      assertTrue(AQL_RELATIONAL_LESS(1, 'true'));
      assertTrue(AQL_RELATIONAL_LESS(1, 'false'));
      assertTrue(AQL_RELATIONAL_LESS(1, 'null'));
      assertTrue(AQL_RELATIONAL_LESS(0, '-1'));
      assertTrue(AQL_RELATIONAL_LESS(0, '-100'));
      assertTrue(AQL_RELATIONAL_LESS(0, '-1.1'));
      assertTrue(AQL_RELATIONAL_LESS(0, '-0.0'));
      assertTrue(AQL_RELATIONAL_LESS(1000, '-1'));
      assertTrue(AQL_RELATIONAL_LESS(1000, '10'));
      assertTrue(AQL_RELATIONAL_LESS(1000, '10000'));
      assertTrue(AQL_RELATIONAL_LESS(0, [ ]));
      assertTrue(AQL_RELATIONAL_LESS(0, [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS(10, [ ]));
      assertTrue(AQL_RELATIONAL_LESS(100, [ ]));
      assertTrue(AQL_RELATIONAL_LESS(100, [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS(100, [ 0, 1 ]));
      assertTrue(AQL_RELATIONAL_LESS(100, [ 99 ]));
      assertTrue(AQL_RELATIONAL_LESS(100, [ 100 ]));
      assertTrue(AQL_RELATIONAL_LESS(100, [ 101 ]));
      assertTrue(AQL_RELATIONAL_LESS(100, { }));
      assertTrue(AQL_RELATIONAL_LESS(100, { 'a' : 0 }));
      assertTrue(AQL_RELATIONAL_LESS(100, { 'a' : 1 }));
      assertTrue(AQL_RELATIONAL_LESS(100, { 'a' : 99 }));
      assertTrue(AQL_RELATIONAL_LESS(100, { 'a' : 100 }));
      assertTrue(AQL_RELATIONAL_LESS(100, { 'a' : 101 }));
      assertTrue(AQL_RELATIONAL_LESS(100, { 'a' : 1000 }));
      assertTrue(AQL_RELATIONAL_LESS('', ' '));
      assertTrue(AQL_RELATIONAL_LESS('0', 'a'));
      assertTrue(AQL_RELATIONAL_LESS('a', 'a '));
      assertTrue(AQL_RELATIONAL_LESS('a', 'b'));
      assertTrue(AQL_RELATIONAL_LESS('A', 'a'));
      assertTrue(AQL_RELATIONAL_LESS('AB', 'Ab'));
      assertTrue(AQL_RELATIONAL_LESS('abcd', 'bbcd'));
      assertTrue(AQL_RELATIONAL_LESS('abcd', 'abda'));
      assertTrue(AQL_RELATIONAL_LESS('abcd', 'abdd'));
      assertTrue(AQL_RELATIONAL_LESS('abcd', 'abcde'));
      assertTrue(AQL_RELATIONAL_LESS('0abcd', 'abcde'));
      assertTrue(AQL_RELATIONAL_LESS('abcd', [ ]));
      assertTrue(AQL_RELATIONAL_LESS('abcd', [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS('abcd', [ -1 ]));
      assertTrue(AQL_RELATIONAL_LESS('abcd', [ " " ]));
      assertTrue(AQL_RELATIONAL_LESS('abcd', [ "" ]));
      assertTrue(AQL_RELATIONAL_LESS('abcd', [ "abc" ]));
      assertTrue(AQL_RELATIONAL_LESS('abcd', [ "abcd" ]));
      assertTrue(AQL_RELATIONAL_LESS('abcd', { } ));
      assertTrue(AQL_RELATIONAL_LESS('abcd', { 'a' : true } ));
      assertTrue(AQL_RELATIONAL_LESS('abcd', { 'abc' : true } ));
      assertTrue(AQL_RELATIONAL_LESS('ABCD', { 'a' : true } ));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS([ 0 ], [ 1 ]));
      assertTrue(AQL_RELATIONAL_LESS([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertTrue(AQL_RELATIONAL_LESS([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertTrue(AQL_RELATIONAL_LESS([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertTrue(AQL_RELATIONAL_LESS([ 0, 1, 4 ], [ 1 ]));
      assertTrue(AQL_RELATIONAL_LESS([ 15, 99 ], [ 110 ]));
      assertTrue(AQL_RELATIONAL_LESS([ 15, 99 ], [ 15, 100 ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ undefined ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ null ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ false ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ true ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ -1 ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ '' ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ '0' ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ 'abcd' ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ [ ] ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ [ null ] ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], [ { } ]));
      assertTrue(AQL_RELATIONAL_LESS([ null ], [ false ]));
      assertTrue(AQL_RELATIONAL_LESS([ null ], [ true ]));
      assertTrue(AQL_RELATIONAL_LESS([ null ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS([ null ], [ [ ] ]));
      assertTrue(AQL_RELATIONAL_LESS([ false ], [ true ]));
      assertTrue(AQL_RELATIONAL_LESS([ false ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS([ false ], [ -1 ]));
      assertTrue(AQL_RELATIONAL_LESS([ false ], [ '' ]));
      assertTrue(AQL_RELATIONAL_LESS([ false ], [ '0' ]));
      assertTrue(AQL_RELATIONAL_LESS([ false ], [ 'abcd' ]));
      assertTrue(AQL_RELATIONAL_LESS([ false ], [ [ ] ]));
      assertTrue(AQL_RELATIONAL_LESS([ false ], [ [ false ] ]));
      assertTrue(AQL_RELATIONAL_LESS([ true ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESS([ true ], [ -1 ]));
      assertTrue(AQL_RELATIONAL_LESS([ true ], [ '' ]));
      assertTrue(AQL_RELATIONAL_LESS([ true ], [ '0' ]));
      assertTrue(AQL_RELATIONAL_LESS([ true ], [ 'abcd' ]));
      assertTrue(AQL_RELATIONAL_LESS([ true ], [ [ ] ]));
      assertTrue(AQL_RELATIONAL_LESS([ true ], [ [ false ] ]));
      assertTrue(AQL_RELATIONAL_LESS([ false, false ], [ true ]));
      assertTrue(AQL_RELATIONAL_LESS([ false, false ], [ false, true ]));
      assertTrue(AQL_RELATIONAL_LESS([ false, false ], [ false, 0 ]));
      assertTrue(AQL_RELATIONAL_LESS([ null, null ], [ null, false ]));
      assertTrue(AQL_RELATIONAL_LESS([ ], { }));
      assertTrue(AQL_RELATIONAL_LESS([ ], { 'a' : true }));
      assertTrue(AQL_RELATIONAL_LESS([ ], { 'a' : null }));
      assertTrue(AQL_RELATIONAL_LESS([ ], { 'a' : false }));
      assertTrue(AQL_RELATIONAL_LESS([ '' ], { }));
      assertTrue(AQL_RELATIONAL_LESS([ 0 ], { }));
      assertTrue(AQL_RELATIONAL_LESS([ null ], { }));
      assertTrue(AQL_RELATIONAL_LESS([ false ], { }));
      assertTrue(AQL_RELATIONAL_LESS([ false ], { 'a' : false }));
      assertTrue(AQL_RELATIONAL_LESS([ true ], { 'a' : false }));
      assertTrue(AQL_RELATIONAL_LESS([ 'abcd' ], { 'a' : false }));
      assertTrue(AQL_RELATIONAL_LESS([ 5 ], { 'a' : false }));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6 ], { 'a' : 2, 'b' : 2 }));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, 7 ], { }));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, false ], [ 5, 6, true ]));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, 0 ]));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, 999 ], [ 5, 6, '' ]));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, 'b' ]));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, 'A' ], [ 5, 6, 'a' ]));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, '' ], [ 5, 6, 'a' ]));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, 9, 9 ], [ 5, 6, [ ] ]));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, [ ] ]));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, { } ]));
      assertTrue(AQL_RELATIONAL_LESS([ 5, 6, 9, 9 ], [ 5, 6, { } ]));
      assertTrue(AQL_RELATIONAL_LESS({ }, { 'a' : 0 }));
      assertTrue(AQL_RELATIONAL_LESS({ 'a' : 1 }, { 'a' : 2 }));
      assertTrue(AQL_RELATIONAL_LESS({ 'a' : 1 }, { 'A' : 2 }));
      assertTrue(AQL_RELATIONAL_LESS({ 'a' : 2 }, { 'A' : 1 }));
      assertTrue(AQL_RELATIONAL_LESS({ 'a' : 1, 'b' : 2 }, { 'a' : 1, 'b' : 2, 'c' : null }));
      assertTrue(AQL_RELATIONAL_LESS({ 'b' : 1 }, { 'a' : 1 }));
      assertTrue(AQL_RELATIONAL_LESS({ 'b' : 2, 'a' : 1 }, { 'a' : 1, 'b' : 2, 'c' : null }));
      assertTrue(AQL_RELATIONAL_LESS({ 'a' : [ 9 ], 'b' : false }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(AQL_RELATIONAL_LESS({ 'a' : [ 9 ], 'b' : true }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(AQL_RELATIONAL_LESS({ 'a' : [ ], 'b' : true }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(AQL_RELATIONAL_LESS({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 10, 1 ] }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_LESS function
////////////////////////////////////////////////////////////////////////////////

    testRelationalLessFalse : function () {
      assertFalse(AQL_RELATIONAL_LESS(null, null));
      assertFalse(AQL_RELATIONAL_LESS(false, null));
      assertFalse(AQL_RELATIONAL_LESS(true, null));
      assertFalse(AQL_RELATIONAL_LESS(0, null));
      assertFalse(AQL_RELATIONAL_LESS(1, null));
      assertFalse(AQL_RELATIONAL_LESS(-1, null));
      assertFalse(AQL_RELATIONAL_LESS('', null));
      assertFalse(AQL_RELATIONAL_LESS(' ', null));
      assertFalse(AQL_RELATIONAL_LESS('1', null));
      assertFalse(AQL_RELATIONAL_LESS('0', null));
      assertFalse(AQL_RELATIONAL_LESS('abcd', null));
      assertFalse(AQL_RELATIONAL_LESS('null', null));
      assertFalse(AQL_RELATIONAL_LESS([ ], null));
      assertFalse(AQL_RELATIONAL_LESS([ true ], null));
      assertFalse(AQL_RELATIONAL_LESS([ false ], null));
      assertFalse(AQL_RELATIONAL_LESS([ null ], null));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], null));
      assertFalse(AQL_RELATIONAL_LESS({ }, null));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : null }, null));
      assertFalse(AQL_RELATIONAL_LESS(false, false));
      assertFalse(AQL_RELATIONAL_LESS(true, true));
      assertFalse(AQL_RELATIONAL_LESS(true, false));
      assertFalse(AQL_RELATIONAL_LESS(0, false));
      assertFalse(AQL_RELATIONAL_LESS(1, false));
      assertFalse(AQL_RELATIONAL_LESS(-1, false));
      assertFalse(AQL_RELATIONAL_LESS('', false));
      assertFalse(AQL_RELATIONAL_LESS(' ', false));
      assertFalse(AQL_RELATIONAL_LESS('1', false));
      assertFalse(AQL_RELATIONAL_LESS('0', false));
      assertFalse(AQL_RELATIONAL_LESS('abcd', false));
      assertFalse(AQL_RELATIONAL_LESS('true', false));
      assertFalse(AQL_RELATIONAL_LESS([ ], false));
      assertFalse(AQL_RELATIONAL_LESS([ true ], false));
      assertFalse(AQL_RELATIONAL_LESS([ false ], false));
      assertFalse(AQL_RELATIONAL_LESS([ null ], false));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], false));
      assertFalse(AQL_RELATIONAL_LESS({ }, false));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : null }, false));
      assertFalse(AQL_RELATIONAL_LESS(0, true));
      assertFalse(AQL_RELATIONAL_LESS(1, true));
      assertFalse(AQL_RELATIONAL_LESS(-1, true));
      assertFalse(AQL_RELATIONAL_LESS('', true));
      assertFalse(AQL_RELATIONAL_LESS(' ', true));
      assertFalse(AQL_RELATIONAL_LESS('1', true));
      assertFalse(AQL_RELATIONAL_LESS('0', true));
      assertFalse(AQL_RELATIONAL_LESS('abcd', true));
      assertFalse(AQL_RELATIONAL_LESS('true', true));
      assertFalse(AQL_RELATIONAL_LESS([ ], true));
      assertFalse(AQL_RELATIONAL_LESS([ true ], true));
      assertFalse(AQL_RELATIONAL_LESS([ false ], true));
      assertFalse(AQL_RELATIONAL_LESS([ null ], true));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], true));
      assertFalse(AQL_RELATIONAL_LESS({ }, true));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : null }, true));
      assertFalse(AQL_RELATIONAL_LESS(0, 0));
      assertFalse(AQL_RELATIONAL_LESS(1, 1));
      assertFalse(AQL_RELATIONAL_LESS(-10, -10));
      assertFalse(AQL_RELATIONAL_LESS(-100, -100));
      assertFalse(AQL_RELATIONAL_LESS(-334.5, -334.5));
      assertFalse(AQL_RELATIONAL_LESS(1, 0));
      assertFalse(AQL_RELATIONAL_LESS(2, 1));
      assertFalse(AQL_RELATIONAL_LESS(100, 1));
      assertFalse(AQL_RELATIONAL_LESS(100, 20));
      assertFalse(AQL_RELATIONAL_LESS(1, -100));
      assertFalse(AQL_RELATIONAL_LESS(-10, -100));
      assertFalse(AQL_RELATIONAL_LESS(-10, -11));
      assertFalse(AQL_RELATIONAL_LESS(1000, 999));
      assertFalse(AQL_RELATIONAL_LESS(1, -1));
      assertFalse(AQL_RELATIONAL_LESS(0, -1));
      assertFalse(AQL_RELATIONAL_LESS(1.01, 1.0));
      assertFalse(AQL_RELATIONAL_LESS(1.2, 1.111));
      assertFalse(AQL_RELATIONAL_LESS(-1.110, -1.111));
      assertFalse(AQL_RELATIONAL_LESS(-1.1109, -1.111));
      assertFalse(AQL_RELATIONAL_LESS('', 0));
      assertFalse(AQL_RELATIONAL_LESS(' ', 0));
      assertFalse(AQL_RELATIONAL_LESS('0', 0));
      assertFalse(AQL_RELATIONAL_LESS('1', 0));
      assertFalse(AQL_RELATIONAL_LESS('-1', 0));
      assertFalse(AQL_RELATIONAL_LESS('true', 0));
      assertFalse(AQL_RELATIONAL_LESS('false', 0));
      assertFalse(AQL_RELATIONAL_LESS('null', 0));
      assertFalse(AQL_RELATIONAL_LESS('', 1));
      assertFalse(AQL_RELATIONAL_LESS(' ', 1));
      assertFalse(AQL_RELATIONAL_LESS('0', 1));
      assertFalse(AQL_RELATIONAL_LESS('1', 1));
      assertFalse(AQL_RELATIONAL_LESS('-1', 1));
      assertFalse(AQL_RELATIONAL_LESS('true', 1));
      assertFalse(AQL_RELATIONAL_LESS('false', 1));
      assertFalse(AQL_RELATIONAL_LESS('null', 1));
      assertFalse(AQL_RELATIONAL_LESS('-1', 0));
      assertFalse(AQL_RELATIONAL_LESS('-100', 0));
      assertFalse(AQL_RELATIONAL_LESS('-1.1', 0));
      assertFalse(AQL_RELATIONAL_LESS('-0.0', 0));
      assertFalse(AQL_RELATIONAL_LESS('-1', 1000));
      assertFalse(AQL_RELATIONAL_LESS('10', 1000));
      assertFalse(AQL_RELATIONAL_LESS('10000', 1000));
      assertFalse(AQL_RELATIONAL_LESS([ ], 0));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], 0));
      assertFalse(AQL_RELATIONAL_LESS([ ], 10));
      assertFalse(AQL_RELATIONAL_LESS([ ], 100));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], 100));
      assertFalse(AQL_RELATIONAL_LESS([ 0, 1 ], 100));
      assertFalse(AQL_RELATIONAL_LESS([ 99 ], 100));
      assertFalse(AQL_RELATIONAL_LESS([ 100 ], 100));
      assertFalse(AQL_RELATIONAL_LESS([ 101 ], 100));
      assertFalse(AQL_RELATIONAL_LESS({ }, 100));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : 0 }, 100));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : 1 }, 100));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : 99 }, 100));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : 100 }, 100));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : 101 }, 100));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : 1000 }, 100));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : false }, 'zz'));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : 'a' }, 'zz'));
      assertFalse(AQL_RELATIONAL_LESS('', ''));
      assertFalse(AQL_RELATIONAL_LESS(' ', ' '));
      assertFalse(AQL_RELATIONAL_LESS('a', 'a'));
      assertFalse(AQL_RELATIONAL_LESS(' a', ' a'));
      assertFalse(AQL_RELATIONAL_LESS('abcd', 'abcd'));
      assertFalse(AQL_RELATIONAL_LESS(' ', ''));
      assertFalse(AQL_RELATIONAL_LESS('a', '0'));
      assertFalse(AQL_RELATIONAL_LESS('a ', 'a'));
      assertFalse(AQL_RELATIONAL_LESS('b', 'a'));
      assertFalse(AQL_RELATIONAL_LESS('a', 'A'));
      assertFalse(AQL_RELATIONAL_LESS('Ab', 'AB'));
      assertFalse(AQL_RELATIONAL_LESS('bbcd', 'abcd'));
      assertFalse(AQL_RELATIONAL_LESS('abda', 'abcd'));
      assertFalse(AQL_RELATIONAL_LESS('abdd', 'abcd'));
      assertFalse(AQL_RELATIONAL_LESS('abcde', 'abcd'));
      assertFalse(AQL_RELATIONAL_LESS('abcde', '0abcde'));
      assertFalse(AQL_RELATIONAL_LESS([ ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 1 ], [ 1 ]));
      assertFalse(AQL_RELATIONAL_LESS([ true ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESS([ false ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESS([ [ 0, 1, 2 ] ], [ [ 0, 1, 2 ] ]));
      assertFalse(AQL_RELATIONAL_LESS([ [ 1, [ "true", 0, -99 , false ] ], 4 ], [ [ 1, [ "true", 0, -99, false ] ], 4 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ 1 ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 1 ], [ 0, 1, 4 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 110 ], [ 15, 99 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 15, 100 ], [ 15, 99 ]));
      assertFalse(AQL_RELATIONAL_LESS([ undefined ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ null ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ false ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ true ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ -1 ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ '' ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ '0' ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ 'abcd' ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ [ ] ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ [ null ] ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ { } ], [ ]));
      assertFalse(AQL_RELATIONAL_LESS([ false ], [ null ]));
      assertFalse(AQL_RELATIONAL_LESS([ true ], [ null ]));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], [ null ]));
      assertFalse(AQL_RELATIONAL_LESS([ [ ] ], [ null ]));
      assertFalse(AQL_RELATIONAL_LESS([ true ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESS([ -1 ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESS([ '' ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESS([ '0' ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESS([ 'abcd' ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESS([ [ ] ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESS([ [ false ] ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESS([ 0 ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESS([ -1 ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESS([ '' ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESS([ '0' ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESS([ 'abcd' ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESS([ [ ] ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESS([ [ false] ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESS([ true ], [ false, false ]));
      assertFalse(AQL_RELATIONAL_LESS([ false, true ], [ false, false ]));
      assertFalse(AQL_RELATIONAL_LESS([ false, 0 ], [ false, false ]));
      assertFalse(AQL_RELATIONAL_LESS([ null, false ], [ null, null ]));
      assertFalse(AQL_RELATIONAL_LESS({ }, [ ]));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : true }, [ ]));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : null }, [ ]));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : false }, [ ]));
      assertFalse(AQL_RELATIONAL_LESS({ }, [ '' ]));
      assertFalse(AQL_RELATIONAL_LESS({ }, [ 0 ]));
      assertFalse(AQL_RELATIONAL_LESS({ }, [ null ]));
      assertFalse(AQL_RELATIONAL_LESS({ }, [ false ]));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : false }, [ false ]));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : false }, [ true ]));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : false }, [ 'abcd' ]));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : false }, [ 5 ]));
      assertFalse(AQL_RELATIONAL_LESS({ 'a' : 2, 'b' : 2 }, [ 5, 6 ]));
      assertFalse(AQL_RELATIONAL_LESS({ }, [ 5, 6, 7 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, false ]));
      assertFalse(AQL_RELATIONAL_LESS([ 5, 6, 0 ], [ 5, 6, true ]));
      assertFalse(AQL_RELATIONAL_LESS([ 5, 6, '' ], [ 5, 6, 999 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 5, 6, 'b' ], [ 5, 6, 'a' ]));
      assertFalse(AQL_RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, 'A' ]));
      assertFalse(AQL_RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, '' ]));
      assertFalse(AQL_RELATIONAL_LESS([ 5, 6, [ ] ], [ 5, 6, 9 ,9 ]));
      assertFalse(AQL_RELATIONAL_LESS([ 5, 6, [ ] ], [ 5, 6, true ]));
      assertFalse(AQL_RELATIONAL_LESS([ 5, 6, { } ], [ 5, 6, true ]));
      assertFalse(AQL_RELATIONAL_LESS([ 5, 6, { } ], [ 5, 6, 9, 9 ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_GREATER function
////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterUndefined : function () {
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, null));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, true));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, false));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, 0.0));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, 1.0));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, -1.0));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, ''));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, '0'));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, '1'));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, [ ]));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, [ 0 ]));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, [ 0, 1 ]));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, [ 1, 2 ]));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, { }));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, { 'a' : 1 }));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, { '0' : false }));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(null, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(true, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(false, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(0.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(-1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER('', undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER('0', undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER('1', undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER([ ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER([ 0 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER([ 0, 1 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER([ 1, 2 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER({ }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER({ 'a' : 1 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER({ '0' : false }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(NaN, false));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(NaN, true));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(NaN, ''));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(NaN, 0));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(NaN, null));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(NaN, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(false, NaN));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(true, NaN));
      assertEqual(undefined, AQL_RELATIONAL_GREATER('', NaN));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(0, NaN));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(null, NaN));
      assertEqual(undefined, AQL_RELATIONAL_GREATER(undefined, NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_GREATER function
////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterTrue : function () {
      assertTrue(AQL_RELATIONAL_GREATER(false, null));
      assertTrue(AQL_RELATIONAL_GREATER(true, null));
      assertTrue(AQL_RELATIONAL_GREATER(0, null));
      assertTrue(AQL_RELATIONAL_GREATER(1, null));
      assertTrue(AQL_RELATIONAL_GREATER(-1, null));
      assertTrue(AQL_RELATIONAL_GREATER('', null));
      assertTrue(AQL_RELATIONAL_GREATER(' ', null));
      assertTrue(AQL_RELATIONAL_GREATER('1', null));
      assertTrue(AQL_RELATIONAL_GREATER('0', null));
      assertTrue(AQL_RELATIONAL_GREATER('abcd', null));
      assertTrue(AQL_RELATIONAL_GREATER('null', null));
      assertTrue(AQL_RELATIONAL_GREATER([ ], null));
      assertTrue(AQL_RELATIONAL_GREATER([ true ], null));
      assertTrue(AQL_RELATIONAL_GREATER([ false ], null));
      assertTrue(AQL_RELATIONAL_GREATER([ null ], null));
      assertTrue(AQL_RELATIONAL_GREATER([ 0 ], null));
      assertTrue(AQL_RELATIONAL_GREATER({ }, null));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : null }, null));
      assertTrue(AQL_RELATIONAL_GREATER(true, false));
      assertTrue(AQL_RELATIONAL_GREATER(0, false));
      assertTrue(AQL_RELATIONAL_GREATER(1, false));
      assertTrue(AQL_RELATIONAL_GREATER(-1, false));
      assertTrue(AQL_RELATIONAL_GREATER('', false));
      assertTrue(AQL_RELATIONAL_GREATER(' ', false));
      assertTrue(AQL_RELATIONAL_GREATER('1', false));
      assertTrue(AQL_RELATIONAL_GREATER('0', false));
      assertTrue(AQL_RELATIONAL_GREATER('abcd', false));
      assertTrue(AQL_RELATIONAL_GREATER('true', false));
      assertTrue(AQL_RELATIONAL_GREATER([ ], false));
      assertTrue(AQL_RELATIONAL_GREATER([ true ], false));
      assertTrue(AQL_RELATIONAL_GREATER([ false ], false));
      assertTrue(AQL_RELATIONAL_GREATER([ null ], false));
      assertTrue(AQL_RELATIONAL_GREATER([ 0 ], false));
      assertTrue(AQL_RELATIONAL_GREATER({ }, false));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : null }, false));
      assertTrue(AQL_RELATIONAL_GREATER(0, true));
      assertTrue(AQL_RELATIONAL_GREATER(1, true));
      assertTrue(AQL_RELATIONAL_GREATER(-1, true));
      assertTrue(AQL_RELATIONAL_GREATER('', true));
      assertTrue(AQL_RELATIONAL_GREATER(' ', true));
      assertTrue(AQL_RELATIONAL_GREATER('1', true));
      assertTrue(AQL_RELATIONAL_GREATER('0', true));
      assertTrue(AQL_RELATIONAL_GREATER('abcd', true));
      assertTrue(AQL_RELATIONAL_GREATER('true', true));
      assertTrue(AQL_RELATIONAL_GREATER([ ], true));
      assertTrue(AQL_RELATIONAL_GREATER([ true ], true));
      assertTrue(AQL_RELATIONAL_GREATER([ false ], true));
      assertTrue(AQL_RELATIONAL_GREATER([ null ], true));
      assertTrue(AQL_RELATIONAL_GREATER([ 0 ], true));
      assertTrue(AQL_RELATIONAL_GREATER({ }, true));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : null }, true));
      assertTrue(AQL_RELATIONAL_GREATER(1, 0));
      assertTrue(AQL_RELATIONAL_GREATER(2, 1));
      assertTrue(AQL_RELATIONAL_GREATER(100, 1));
      assertTrue(AQL_RELATIONAL_GREATER(100, 20));
      assertTrue(AQL_RELATIONAL_GREATER(1, -100));
      assertTrue(AQL_RELATIONAL_GREATER(-10, -100));
      assertTrue(AQL_RELATIONAL_GREATER(-10, -11));
      assertTrue(AQL_RELATIONAL_GREATER(1000, 999));
      assertTrue(AQL_RELATIONAL_GREATER(1, -1));
      assertTrue(AQL_RELATIONAL_GREATER(0, -1));
      assertTrue(AQL_RELATIONAL_GREATER(1.01, 1.0));
      assertTrue(AQL_RELATIONAL_GREATER(1.2, 1.111));
      assertTrue(AQL_RELATIONAL_GREATER(-1.110, -1.111));
      assertTrue(AQL_RELATIONAL_GREATER(-1.1109, -1.111));
      assertTrue(AQL_RELATIONAL_GREATER('', 0));
      assertTrue(AQL_RELATIONAL_GREATER(' ', 0));
      assertTrue(AQL_RELATIONAL_GREATER('0', 0));
      assertTrue(AQL_RELATIONAL_GREATER('1', 0));
      assertTrue(AQL_RELATIONAL_GREATER('-1', 0));
      assertTrue(AQL_RELATIONAL_GREATER('true', 0));
      assertTrue(AQL_RELATIONAL_GREATER('false', 0));
      assertTrue(AQL_RELATIONAL_GREATER('null', 0));
      assertTrue(AQL_RELATIONAL_GREATER('', 1));
      assertTrue(AQL_RELATIONAL_GREATER(' ', 1));
      assertTrue(AQL_RELATIONAL_GREATER('0', 1));
      assertTrue(AQL_RELATIONAL_GREATER('1', 1));
      assertTrue(AQL_RELATIONAL_GREATER('-1', 1));
      assertTrue(AQL_RELATIONAL_GREATER('true', 1));
      assertTrue(AQL_RELATIONAL_GREATER('false', 1));
      assertTrue(AQL_RELATIONAL_GREATER('null', 1));
      assertTrue(AQL_RELATIONAL_GREATER('-1', 0));
      assertTrue(AQL_RELATIONAL_GREATER('-100', 0));
      assertTrue(AQL_RELATIONAL_GREATER('-1.1', 0));
      assertTrue(AQL_RELATIONAL_GREATER('-0.0', 0));
      assertTrue(AQL_RELATIONAL_GREATER('-1', 1000));
      assertTrue(AQL_RELATIONAL_GREATER('10', 1000));
      assertTrue(AQL_RELATIONAL_GREATER('10000', 1000));
      assertTrue(AQL_RELATIONAL_GREATER([ ], 0));
      assertTrue(AQL_RELATIONAL_GREATER([ 0 ], 0));
      assertTrue(AQL_RELATIONAL_GREATER([ ], 10));
      assertTrue(AQL_RELATIONAL_GREATER([ ], 100));
      assertTrue(AQL_RELATIONAL_GREATER([ 0 ], 100));
      assertTrue(AQL_RELATIONAL_GREATER([ 0, 1 ], 100));
      assertTrue(AQL_RELATIONAL_GREATER([ 99 ], 100));
      assertTrue(AQL_RELATIONAL_GREATER([ 100 ], 100));
      assertTrue(AQL_RELATIONAL_GREATER([ 101 ], 100));
      assertTrue(AQL_RELATIONAL_GREATER({ }, 100));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 0 }, 100));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 1 }, 100));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 99 }, 100));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 100 }, 100));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 101 }, 100));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 1000 }, 100));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : false }, 'zz'));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 'a' }, 'zz'));
      assertTrue(AQL_RELATIONAL_GREATER(' ', ''));
      assertTrue(AQL_RELATIONAL_GREATER('a', '0'));
      assertTrue(AQL_RELATIONAL_GREATER('a ', 'a'));
      assertTrue(AQL_RELATIONAL_GREATER('b', 'a'));
      assertTrue(AQL_RELATIONAL_GREATER('a', 'A'));
      assertTrue(AQL_RELATIONAL_GREATER('Ab', 'AB'));
      assertTrue(AQL_RELATIONAL_GREATER('bbcd', 'abcd'));
      assertTrue(AQL_RELATIONAL_GREATER('abda', 'abcd'));
      assertTrue(AQL_RELATIONAL_GREATER('abdd', 'abcd'));
      assertTrue(AQL_RELATIONAL_GREATER('abcde', 'abcd'));
      assertTrue(AQL_RELATIONAL_GREATER('abcde', '0abcde'));
      assertTrue(AQL_RELATIONAL_GREATER([ 0 ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 1 ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 1 ], [ 0, 1, 4 ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 110 ], [ 15, 99 ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 15, 100 ], [ 15, 99 ]));
      assertTrue(AQL_RELATIONAL_GREATER([ undefined ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ null ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ false ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ true ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 0 ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ -1 ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ '' ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ '0' ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 'abcd' ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ [ ] ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ [ null ] ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ { } ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATER([ false ], [ null ]));
      assertTrue(AQL_RELATIONAL_GREATER([ true ], [ null ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 0 ], [ null ]));
      assertTrue(AQL_RELATIONAL_GREATER([ [ ] ], [ null ]));
      assertTrue(AQL_RELATIONAL_GREATER([ true ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 0 ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ -1 ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ '' ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ '0' ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 'abcd' ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ [ ] ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ [ false ] ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 0 ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATER([ -1 ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATER([ '' ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATER([ '0' ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATER([ 'abcd' ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATER([ [ ] ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATER([ [ false] ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATER([ true ], [ false, false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ false, true ], [ false, false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ false, 0 ], [ false, false ]));
      assertTrue(AQL_RELATIONAL_GREATER([ null, false ], [ null, null ]));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 0 }, { }));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 2 }, { 'a' : 1 }));
      assertTrue(AQL_RELATIONAL_GREATER({ 'A' : 2 }, { 'a' : 1 }));
      assertTrue(AQL_RELATIONAL_GREATER({ 'A' : 1 }, { 'a' : 2 }));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 1, 'b' : 2, 'c' : null }, { 'a' : 1, 'b' : 2 }));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 1 }, { 'b' : 1 }));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : 1, 'b' : 2, 'c': null }, { 'b' : 2, 'a' : 1 }));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : false }));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : true }));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ ], 'b' : true }));
      assertTrue(AQL_RELATIONAL_GREATER({ 'a' : [ 10, 1 ] }, { 'a' : [ 10 ], 'b' : true }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_GREATER function
////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterFalse : function () {
      assertFalse(AQL_RELATIONAL_GREATER(null, null));
      assertFalse(AQL_RELATIONAL_GREATER(null, false));
      assertFalse(AQL_RELATIONAL_GREATER(null, true));
      assertFalse(AQL_RELATIONAL_GREATER(null, 0));
      assertFalse(AQL_RELATIONAL_GREATER(null, 1));
      assertFalse(AQL_RELATIONAL_GREATER(null, -1));
      assertFalse(AQL_RELATIONAL_GREATER(null, ''));
      assertFalse(AQL_RELATIONAL_GREATER(null, ' '));
      assertFalse(AQL_RELATIONAL_GREATER(null, '1'));
      assertFalse(AQL_RELATIONAL_GREATER(null, '0'));
      assertFalse(AQL_RELATIONAL_GREATER(null, 'abcd'));
      assertFalse(AQL_RELATIONAL_GREATER(null, 'null'));
      assertFalse(AQL_RELATIONAL_GREATER(null, [ ]));
      assertFalse(AQL_RELATIONAL_GREATER(null, [ true ]));
      assertFalse(AQL_RELATIONAL_GREATER(null, [ false ]));
      assertFalse(AQL_RELATIONAL_GREATER(null, [ null ]));
      assertFalse(AQL_RELATIONAL_GREATER(null, [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER(null, { }));
      assertFalse(AQL_RELATIONAL_GREATER(null, { 'a' : null }));
      assertFalse(AQL_RELATIONAL_GREATER(false, false));
      assertFalse(AQL_RELATIONAL_GREATER(true, true));
      assertFalse(AQL_RELATIONAL_GREATER(false, true));
      assertFalse(AQL_RELATIONAL_GREATER(false, 0));
      assertFalse(AQL_RELATIONAL_GREATER(false, 1));
      assertFalse(AQL_RELATIONAL_GREATER(false, -1));
      assertFalse(AQL_RELATIONAL_GREATER(false, ''));
      assertFalse(AQL_RELATIONAL_GREATER(false, ' '));
      assertFalse(AQL_RELATIONAL_GREATER(false, '1'));
      assertFalse(AQL_RELATIONAL_GREATER(false, '0'));
      assertFalse(AQL_RELATIONAL_GREATER(false, 'abcd'));
      assertFalse(AQL_RELATIONAL_GREATER(false, 'true'));
      assertFalse(AQL_RELATIONAL_GREATER(false, [ ]));
      assertFalse(AQL_RELATIONAL_GREATER(false, [ true ]));
      assertFalse(AQL_RELATIONAL_GREATER(false, [ false ]));
      assertFalse(AQL_RELATIONAL_GREATER(false, [ null ]));
      assertFalse(AQL_RELATIONAL_GREATER(false, [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER(false, { }));
      assertFalse(AQL_RELATIONAL_GREATER(false, { 'a' : null }));
      assertFalse(AQL_RELATIONAL_GREATER(true, 0));
      assertFalse(AQL_RELATIONAL_GREATER(true, 1));
      assertFalse(AQL_RELATIONAL_GREATER(true, -1));
      assertFalse(AQL_RELATIONAL_GREATER(true, ''));
      assertFalse(AQL_RELATIONAL_GREATER(true, ' '));
      assertFalse(AQL_RELATIONAL_GREATER(true, '1'));
      assertFalse(AQL_RELATIONAL_GREATER(true, '0'));
      assertFalse(AQL_RELATIONAL_GREATER(true, 'abcd'));
      assertFalse(AQL_RELATIONAL_GREATER(true, 'true'));
      assertFalse(AQL_RELATIONAL_GREATER(true, [ ]));
      assertFalse(AQL_RELATIONAL_GREATER(true, [ true ]));
      assertFalse(AQL_RELATIONAL_GREATER(true, [ false ]));
      assertFalse(AQL_RELATIONAL_GREATER(true, [ null ]));
      assertFalse(AQL_RELATIONAL_GREATER(true, [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER(true, { }));
      assertFalse(AQL_RELATIONAL_GREATER(true, { 'a' : null }));
      assertFalse(AQL_RELATIONAL_GREATER(0, 0));
      assertFalse(AQL_RELATIONAL_GREATER(1, 1));
      assertFalse(AQL_RELATIONAL_GREATER(-10, -10));
      assertFalse(AQL_RELATIONAL_GREATER(-100, -100));
      assertFalse(AQL_RELATIONAL_GREATER(-334.5, -334.5));
      assertFalse(AQL_RELATIONAL_GREATER(0, 1));
      assertFalse(AQL_RELATIONAL_GREATER(1, 2));
      assertFalse(AQL_RELATIONAL_GREATER(1, 100));
      assertFalse(AQL_RELATIONAL_GREATER(20, 100));
      assertFalse(AQL_RELATIONAL_GREATER(-100, 1));
      assertFalse(AQL_RELATIONAL_GREATER(-100, -10));
      assertFalse(AQL_RELATIONAL_GREATER(-11, -10));
      assertFalse(AQL_RELATIONAL_GREATER(999, 1000));
      assertFalse(AQL_RELATIONAL_GREATER(-1, 1));
      assertFalse(AQL_RELATIONAL_GREATER(-1, 0));
      assertFalse(AQL_RELATIONAL_GREATER(1.0, 1.01));
      assertFalse(AQL_RELATIONAL_GREATER(1.111, 1.2));
      assertFalse(AQL_RELATIONAL_GREATER(-1.111, -1.110));
      assertFalse(AQL_RELATIONAL_GREATER(-1.111, -1.1109));
      assertFalse(AQL_RELATIONAL_GREATER(0, ''));
      assertFalse(AQL_RELATIONAL_GREATER(0, ' '));
      assertFalse(AQL_RELATIONAL_GREATER(0, '0'));
      assertFalse(AQL_RELATIONAL_GREATER(0, '1'));
      assertFalse(AQL_RELATIONAL_GREATER(0, '-1'));
      assertFalse(AQL_RELATIONAL_GREATER(0, 'true'));
      assertFalse(AQL_RELATIONAL_GREATER(0, 'false'));
      assertFalse(AQL_RELATIONAL_GREATER(0, 'null'));
      assertFalse(AQL_RELATIONAL_GREATER(1, ''));
      assertFalse(AQL_RELATIONAL_GREATER(1, ' '));
      assertFalse(AQL_RELATIONAL_GREATER(1, '0'));
      assertFalse(AQL_RELATIONAL_GREATER(1, '1'));
      assertFalse(AQL_RELATIONAL_GREATER(1, '-1'));
      assertFalse(AQL_RELATIONAL_GREATER(1, 'true'));
      assertFalse(AQL_RELATIONAL_GREATER(1, 'false'));
      assertFalse(AQL_RELATIONAL_GREATER(1, 'null'));
      assertFalse(AQL_RELATIONAL_GREATER(0, '-1'));
      assertFalse(AQL_RELATIONAL_GREATER(0, '-100'));
      assertFalse(AQL_RELATIONAL_GREATER(0, '-1.1'));
      assertFalse(AQL_RELATIONAL_GREATER(0, '-0.0'));
      assertFalse(AQL_RELATIONAL_GREATER(1000, '-1'));
      assertFalse(AQL_RELATIONAL_GREATER(1000, '10'));
      assertFalse(AQL_RELATIONAL_GREATER(1000, '10000'));
      assertFalse(AQL_RELATIONAL_GREATER(0, [ ]));
      assertFalse(AQL_RELATIONAL_GREATER(0, [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER(10, [ ]));
      assertFalse(AQL_RELATIONAL_GREATER(100, [ ]));
      assertFalse(AQL_RELATIONAL_GREATER(100, [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER(100, [ 0, 1 ]));
      assertFalse(AQL_RELATIONAL_GREATER(100, [ 99 ]));
      assertFalse(AQL_RELATIONAL_GREATER(100, [ 100 ]));
      assertFalse(AQL_RELATIONAL_GREATER(100, [ 101 ]));
      assertFalse(AQL_RELATIONAL_GREATER(100, { }));
      assertFalse(AQL_RELATIONAL_GREATER(100, { 'a' : 0 }));
      assertFalse(AQL_RELATIONAL_GREATER(100, { 'a' : 1 }));
      assertFalse(AQL_RELATIONAL_GREATER(100, { 'a' : 99 }));
      assertFalse(AQL_RELATIONAL_GREATER(100, { 'a' : 100 }));
      assertFalse(AQL_RELATIONAL_GREATER(100, { 'a' : 101 }));
      assertFalse(AQL_RELATIONAL_GREATER(100, { 'a' : 1000 }));
      assertFalse(AQL_RELATIONAL_GREATER('', ''));
      assertFalse(AQL_RELATIONAL_GREATER(' ', ' '));
      assertFalse(AQL_RELATIONAL_GREATER('a', 'a'));
      assertFalse(AQL_RELATIONAL_GREATER(' a', ' a'));
      assertFalse(AQL_RELATIONAL_GREATER('abcd', 'abcd'));
      assertFalse(AQL_RELATIONAL_GREATER('', ' '));
      assertFalse(AQL_RELATIONAL_GREATER('0', 'a'));
      assertFalse(AQL_RELATIONAL_GREATER('a', 'a '));
      assertFalse(AQL_RELATIONAL_GREATER('a', 'b'));
      assertFalse(AQL_RELATIONAL_GREATER('A', 'a'));
      assertFalse(AQL_RELATIONAL_GREATER('AB', 'Ab'));
      assertFalse(AQL_RELATIONAL_GREATER('abcd', 'bbcd'));
      assertFalse(AQL_RELATIONAL_GREATER('abcd', 'abda'));
      assertFalse(AQL_RELATIONAL_GREATER('abcd', 'abdd'));
      assertFalse(AQL_RELATIONAL_GREATER('abcd', 'abcde'));
      assertFalse(AQL_RELATIONAL_GREATER('0abcd', 'abcde'));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ ]));
      assertFalse(AQL_RELATIONAL_GREATER([ 0 ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ 1 ], [ 1 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ true ], [ true ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false ], [ false ]));
      assertFalse(AQL_RELATIONAL_GREATER([ [ 0, 1, 2 ] ], [ [ 0, 1, 2 ] ]));
      assertFalse(AQL_RELATIONAL_GREATER([ [ 1, [ "true", 0, -99 , false ] ], 4 ], [ [ 1, [ "true", 0, -99, false ] ], 4 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ 0 ], [ 1 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ 0, 1, 4 ], [ 1 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ 15, 99 ], [ 110 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ 15, 99 ], [ 15, 100 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ undefined ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ null ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ false ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ true ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ -1 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ '' ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ '0' ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ 'abcd' ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ [ ] ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ [ null ] ]));
      assertFalse(AQL_RELATIONAL_GREATER([ ], [ { } ]));
      assertFalse(AQL_RELATIONAL_GREATER([ null ], [ false ]));
      assertFalse(AQL_RELATIONAL_GREATER([ null ], [ true ]));
      assertFalse(AQL_RELATIONAL_GREATER([ null ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ null ], [ [ ] ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false ], [ true ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false ], [ -1 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false ], [ '' ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false ], [ '0' ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false ], [ 'abcd' ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false ], [ [ ] ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false ], [ [ false ] ]));
      assertFalse(AQL_RELATIONAL_GREATER([ true ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ true ], [ -1 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ true ], [ '' ]));
      assertFalse(AQL_RELATIONAL_GREATER([ true ], [ '0' ]));
      assertFalse(AQL_RELATIONAL_GREATER([ true ], [ 'abcd' ]));
      assertFalse(AQL_RELATIONAL_GREATER([ true ], [ [ ] ]));
      assertFalse(AQL_RELATIONAL_GREATER([ true ], [ [ false ] ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false, false ], [ true ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false, false ], [ false, true ]));
      assertFalse(AQL_RELATIONAL_GREATER([ false, false ], [ false, 0 ]));
      assertFalse(AQL_RELATIONAL_GREATER([ null, null ], [ null, false ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_LESSEQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalLessEqualUndefined : function () {
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, null));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, true));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, false));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, 0.0));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, 1.0));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, -1.0));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, ''));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, '0'));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, '1'));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, [ ]));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, [ 0 ]));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, [ 0, 1 ]));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, [ 1, 2 ]));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, { }));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, { 'a' : 1 }));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, { '0' : false }));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(null, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(true, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(false, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(0.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(-1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL('', undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL('0', undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL('1', undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL([ ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL([ 0 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL([ 0, 1 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL([ 1, 2 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL({ }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL({ 'a' : 1 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL({ '0' : false }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(NaN, false));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(NaN, true));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(NaN, ''));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(NaN, 0));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(NaN, null));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(NaN, undefined));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(false, NaN));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(true, NaN));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL('', NaN));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(0, NaN));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(null, NaN));
      assertEqual(undefined, AQL_RELATIONAL_LESSEQUAL(undefined, NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_LESSEQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalLessEqualTrue : function () {
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, false));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, true));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, 0));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, 1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, -1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, ''));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, ' '));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, '1'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, '0'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, 'abcd'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, 'null'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, [ ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, [ true ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, [ false ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, [ null ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, { 'a' : null }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(null, null));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, true));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, 0));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, 1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, -1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, ''));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, ' '));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, '1'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, '0'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, 'abcd'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, 'true'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, [ ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, [ true ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, [ false ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, [ null ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, { 'a' : null }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(false, false));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, 0));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, 1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, -1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, ''));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, ' '));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, '1'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, '0'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, 'abcd'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, 'true'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, [ ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, [ true ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, [ false ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, [ null ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, { 'a' : null }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(true, true));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, 1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, 2));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, 100));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(20, 100));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-100, 1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-100, -10));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-11, -10));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(999, 1000));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-1, 1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-1, 0));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1.0, 1.01));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1.111, 1.2));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-1.111, -1.110));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-1.111, -1.1109));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, 0));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, 1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(2, 2));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(20, 20));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, 100));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-100, -100));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-11, -11));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(999, 999));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-1, -1));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1.0, 1.0));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1.111, 1.111));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1.2, 1.2));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(-1.111, -1.111));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, ''));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, ' '));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, '0'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, '1'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, '-1'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, 'true'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, 'false'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, 'null'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, ''));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, ' '));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, '0'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, '1'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, '-1'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, 'true'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, 'false'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1, 'null'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, '-1'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, '-100'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, '-1.1'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, '-0.0'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1000, '-1'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1000, '10'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(1000, '10000'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, [ ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(0, [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(10, [ ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, [ ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, [ 0, 1 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, [ 99 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, [ 100 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, [ 101 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, { 'a' : 0 }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, { 'a' : 1 }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, { 'a' : 99 }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, { 'a' : 100 }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, { 'a' : 101 }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(100, { 'a' : 1000 }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('', ' '));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('0', 'a'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('a', 'a '));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('a', 'b'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('A', 'a'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('AB', 'Ab'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('abcd', 'bbcd'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('abcd', 'abda'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('abcd', 'abdd'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('abcd', 'abcde'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('0abcd', 'abcde'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(' abcd', 'abcd'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('', ''));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('0', '0'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('a', 'a'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('A', 'A'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('AB', 'AB'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('Ab', 'Ab'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('abcd', 'abcd'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('0abcd', '0abcd'));
      assertTrue(AQL_RELATIONAL_LESSEQUAL(' ', ' '));
      assertTrue(AQL_RELATIONAL_LESSEQUAL('  ', '  '));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 0 ], [ 1 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 0, 1, 4 ], [ 1 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 15, 99 ], [ 110 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 15, 99 ], [ 15, 100 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ undefined ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ null ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ false ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ true ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ -1 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ '' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ '0' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ 'abcd' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ [ ] ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ [ null ] ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ { } ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], [ ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 0 ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 2 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 15, 99 ], [ 15, 99 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ null ], [ null ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ [ [ null, 1, 9 ], [ 12, "true", false ] ] , 0 ], [ [ [ null, 1, 9 ], [ 12, "true", false ] ] ,0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], [ false ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false, true ], [ false, true ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ null ], [ false ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ null ], [ true ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ null ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ null ], [ [ ] ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], [ true ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], [ -1 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], [ '' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], [ '0' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], [ 'abcd' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], [ [ ] ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], [ [ false ] ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ true ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ true ], [ -1 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ true ], [ '' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ true ], [ '0' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ true ], [ 'abcd' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ true ], [ [ ] ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ true ], [ [ false ] ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false, false ], [ true ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false, false ], [ false, true ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false, false ], [ false, 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ null, null ], [ null, false ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], { 'a' : true }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], { 'a' : null }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ ], { 'a' : false }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ '' ], { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 0 ], { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ null ], { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ false ], { 'a' : false }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ true ], { 'a' : false }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 'abcd' ], { 'a' : false }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5 ], { 'a' : false }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6 ], { 'a' : 2, 'b' : 2 }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, 7 ], { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, false ], [ 5, 6, true ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, 0 ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, 999 ], [ 5, 6, '' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, 'a' ], [ 5, 6, 'b' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, 'A' ], [ 5, 6, 'a' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, '' ], [ 5, 6, 'a' ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, 9, 9 ], [ 5, 6, [ ] ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, [ ] ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, { } ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL([ 5, 6, 9, 9 ], [ 5, 6, { } ]));
      assertTrue(AQL_RELATIONAL_LESSEQUAL({ }, { }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL({ 'A' : true }, { 'A' : true }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : false }, { 'a' : true, 'b' : false }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : false }, { 'b' : false, 'a' : true }));
      assertTrue(AQL_RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : { 'c' : 1, 'f' : 2 }, 'x' : 9 }, { 'x' : 9, 'b' : { 'f' : 2, 'c' : 1 }, 'a' : true }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_LESSEQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalLessEqualFalse : function () {
      assertFalse(AQL_RELATIONAL_LESSEQUAL(false, null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(true, null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(0, null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(1, null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(-1, null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('', null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(' ', null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('1', null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('0', null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('abcd', null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('null', null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ ], null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ true ], null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ false ], null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ null ], null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0 ], null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ }, null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : null }, null));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(true, false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(0, false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(1, false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(-1, false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('', false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(' ', false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('1', false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('0', false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('abcd', false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('true', false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ ], false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ true ], false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ false ], false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ null ], false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0 ], false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ }, false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : null }, false));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(0, true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(1, true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(-1, true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('', true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(' ', true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('1', true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('0', true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('abcd', true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('true', true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ ], true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ true ], true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ false ], true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ null ], true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0 ], true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ }, true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : null }, true));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(1, 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(2, 1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(100, 1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(100, 20));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(1, -100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(-10, -100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(-10, -11));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(1000, 999));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(1, -1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(0, -1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(1.01, 1.0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(1.2, 1.111));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(-1.110, -1.111));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(-1.1109, -1.111));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(' ', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('0', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('1', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('-1', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('true', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('false', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('null', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('', 1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(' ', 1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('0', 1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('1', 1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('-1', 1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('true', 1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('false', 1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('null', 1));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('-1', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('-100', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('-1.1', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('-0.0', 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('-1', 1000));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('10', 1000));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('10000', 1000));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ ], 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0 ], 0));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ ], 10));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ ], 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0 ], 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0, 1 ], 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 99 ], 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 100 ], 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 101 ], 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ }, 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : 0 }, 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : 1 }, 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : 99 }, 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : 100 }, 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : 101 }, 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : 1000 }, 100));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : false }, 'zz'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL({ 'a' : 'a' }, 'zz'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL(' ', ''));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('a', '0'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('a ', 'a'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('b', 'a'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('a', 'A'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('Ab', 'AB'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('bbcd', 'abcd'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('abda', 'abcd'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('abdd', 'abcd'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('abcde', 'abcd'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL('abcde', '0abcde'));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0 ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 1 ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 1 ], [ 0, 1, 4 ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 110 ], [ 15, 99 ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 15, 100 ], [ 15, 99 ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ undefined ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ null ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ false ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ true ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0 ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ -1 ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ '' ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ '0' ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 'abcd' ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ [ ] ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ [ null ] ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ { } ], [ ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ false ], [ null ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ true ], [ null ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0 ], [ null ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ [ ] ], [ null ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ true ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0 ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ -1 ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ '' ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ '0' ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 'abcd' ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ [ ] ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ [ false ] ], [ false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 0 ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ -1 ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ '' ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ '0' ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ 'abcd' ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ [ ] ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ [ false] ], [ true ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ true ], [ false, false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ false, true ], [ false, false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ false, 0 ], [ false, false ]));
      assertFalse(AQL_RELATIONAL_LESSEQUAL([ null, false ], [ null, null ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_GREATEREQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterEqualUndefined : function () {
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, null));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, true));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, false));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, 0.0));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, 1.0));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, -1.0));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, ''));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, '0'));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, '1'));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, [ ]));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, [ 0 ]));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, [ 0, 1 ]));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, [ 1, 2 ]));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, { }));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, { 'a' : 1 }));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, { '0' : false }));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(null, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(true, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(false, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(0.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(-1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL('', undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL('0', undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL('1', undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL([ ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL([ 0 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL([ 0, 1 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL([ 1, 2 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL({ }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL({ 'a' : 1 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL({ '0' : false }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(NaN, false));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(NaN, true));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(NaN, ''));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(NaN, 0));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(NaN, null));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(NaN, undefined));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(false, NaN));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(true, NaN));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL('', NaN));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(0, NaN));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(null, NaN));
      assertEqual(undefined, AQL_RELATIONAL_GREATEREQUAL(undefined, NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_GREATEREQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterEqualTrue : function () {
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(false, null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(true, null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(0, null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1, null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-1, null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('', null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(' ', null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('1', null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('0', null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('abcd', null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('null', null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ ], null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ true ], null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ false ], null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ null ], null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : null }, null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(true, false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(0, false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1, false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-1, false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('', false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(' ', false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('1', false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('0', false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('abcd', false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('true', false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ ], false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ true ], false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ false ], false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ null ], false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : null }, false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(0, true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1, true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-1, true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('', true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(' ', true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('1', true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('0', true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('abcd', true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('true', true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ ], true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ true ], true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ false ], true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ null ], true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : null }, true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1, 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(2, 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(100, 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(100, 20));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1, -100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-10, -100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-10, -11));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1000, 999));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1, -1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(0, -1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1.01, 1.0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1.2, 1.111));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-1.110, -1.111));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-1.1109, -1.111));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(' ', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('0', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('1', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('-1', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('true', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('false', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('null', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('', 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(' ', 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('0', 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('1', 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('-1', 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('true', 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('false', 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('null', 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('-1', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('-100', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('-1.1', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('-0.0', 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('-1', 1000));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('10', 1000));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('10000', 1000));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ ], 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ ], 10));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ ], 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0, 1 ], 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 99 ], 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 100 ], 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 101 ], 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 0 }, 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 1 }, 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 99 }, 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 100 }, 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 101 }, 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 1000 }, 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : false }, 'zz'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 'a' }, 'zz'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(' ', ''));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('a', '0'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('a ', 'a'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('b', 'a'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('a', 'A'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('Ab', 'AB'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('bbcd', 'abcd'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('abda', 'abcd'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('abdd', 'abcd'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('abcde', 'abcd'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('abcde', '0abcde'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 1 ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 1 ], [ 0, 1, 4 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 110 ], [ 15, 99 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 15, 100 ], [ 15, 99 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ undefined ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ null ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ false ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ true ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ -1 ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ '' ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ '0' ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 'abcd' ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ [ ] ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ [ null ] ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ { } ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ false ], [ null ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ true ], [ null ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], [ null ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ [ ] ], [ null ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ true ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ -1 ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ '' ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ '0' ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 'abcd' ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ [ ] ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ [ false ] ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ -1 ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ '' ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ '0' ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 'abcd' ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ [ ] ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ [ false] ], [ true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ true ], [ false, false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ false, true ], [ false, false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ false, 0 ], [ false, false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ null, false ], [ null, null ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(null, null));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(false, false));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(true, true));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(0, 0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1, 1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(2, 2));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(20, 20));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(100, 100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-100, -100));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-11, -11));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(999, 999));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-1, -1));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1.0, 1.0));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1.111, 1.111));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(1.2, 1.2));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(-1.111, -1.111));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('', ''));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('0', '0'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('a', 'a'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('A', 'A'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('AB', 'AB'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('Ab', 'Ab'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('abcd', 'abcd'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('0abcd', '0abcd'));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL(' ', ' '));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL('  ', '  '));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ ], [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0 ], [ 0 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 2 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 15, 99 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ null ], [ null ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ [ [ null, 1, 9 ], [ 12, "true", false ] ] , 0 ], [ [ [ null, 1, 9 ], [ 12, "true", false ] ] ,0 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ false ], [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ false, true ], [ false, true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : true }, [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : null }, [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : false }, [ ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, [ '' ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, [ 0 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, [ null ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : false }, [ false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : false }, [ true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : false }, [ 'abcd' ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : false }, [ 5 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 2, 'b' : 2 }, [ 5, 6 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, [ 5, 6, 7 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 5, 6, true ], [ 5, 6, false ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 5, 6, 0 ], [ 5, 6, true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 5, 6, '' ], [ 5, 6, 999 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 5, 6, 'b' ], [ 5, 6, 'a' ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 5, 6, 'a' ], [ 5, 6, 'A' ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 5, 6, 'a' ], [ 5, 6, '' ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 5, 6, [ ] ], [ 5, 6, 9, 9 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 5, 6, [ ] ], [ 5, 6, true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 5, 6, { } ], [ 5, 6, true ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL([ 5, 6, { } ], [ 5, 6, 9, 9 ]));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 0 }, { }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 2 }, { 'a' : 1 }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'A' : 2 }, { 'a' : 1 }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'A' : 1 }, { 'a' : 2 }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 1, 'b' : 2, 'c' : null }, { 'a' : 1, 'b' : 2 }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 1 }, { 'b' : 1 }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : 1, 'b' : 2, 'c': null }, { 'b' : 2, 'a' : 1 }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : false }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : true }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ ], 'b' : true }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : [ 10, 1 ] }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ }, { }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'A' : true }, { 'A' : true }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : false }, { 'a' : true, 'b' : false }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : false }, { 'b' : false, 'a' : true }));
      assertTrue(AQL_RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : { 'c' : 1, 'f' : 2 }, 'x' : 9 }, { 'x' : 9, 'b' : { 'f' : 2, 'c' : 1 }, 'a' : true }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_GREATEREQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterEqualFalse : function () {
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, false));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, true));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, 0));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, 1));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, -1));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, ''));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, ' '));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, '1'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, '0'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, 'abcd'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, 'null'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, [ ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, [ true ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, [ false ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, [ null ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, { }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(null, { 'a' : null }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, true));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, 0));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, 1));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, -1));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, ''));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, ' '));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, '1'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, '0'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, 'abcd'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, 'true'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, [ ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, [ true ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, [ false ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, [ null ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, { }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(false, { 'a' : null }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, 0));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, 1));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, -1));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, ''));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, ' '));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, '1'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, '0'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, 'abcd'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, 'true'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, [ ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, [ true ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, [ false ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, [ null ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, { }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(true, { 'a' : null }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, 1));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1, 2));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1, 100));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(20, 100));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(-100, 1));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(-100, -10));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(-11, -10));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(999, 1000));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(-1, 1));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(-1, 0));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1.0, 1.01));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1.111, 1.2));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(-1.111, -1.110));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(-1.111, -1.1109));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, ''));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, ' '));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, '0'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, '1'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, '-1'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, 'true'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, 'false'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, 'null'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1, ''));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1, ' '));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1, '0'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1, '1'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1, '-1'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1, 'true'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1, 'false'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1, 'null'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, '-1'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, '-100'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, '-1.1'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, '-0.0'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1000, '-1'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1000, '10'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(1000, '10000'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, [ ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(0, [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(10, [ ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, [ ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, [ 0, 1 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, [ 99 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, [ 100 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, [ 101 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, { }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, { 'a' : 0 }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, { 'a' : 1 }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, { 'a' : 99 }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, { 'a' : 100 }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, { 'a' : 101 }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(100, { 'a' : 1000 }));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('', ' '));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('0', 'a'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('a', 'a '));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('a', 'b'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('A', 'a'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('AB', 'Ab'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('abcd', 'bbcd'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('abcd', 'abda'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('abcd', 'abdd'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('abcd', 'abcde'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL('0abcd', 'abcde'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL(' abcd', 'abcd'));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ 0 ], [ 1 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ 0, 1, 4 ], [ 1 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 110 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 15, 100 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ undefined ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ null ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ false ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ true ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ -1 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ '' ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ '0' ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ 'abcd' ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ [ ] ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ [ null ] ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ ], [ { } ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ null ], [ false ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ null ], [ true ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ null ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ null ], [ [ ] ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false ], [ true ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false ], [ -1 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false ], [ '' ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false ], [ '0' ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false ], [ 'abcd' ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false ], [ [ ] ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false ], [ [ false ] ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ true ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ true ], [ -1 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ true ], [ '' ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ true ], [ '0' ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ true ], [ 'abcd' ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ true ], [ [ ] ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ true ], [ [ false ] ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false, false ], [ true ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false, false ], [ false, true ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ false, false ], [ false, 0 ]));
      assertFalse(AQL_RELATIONAL_GREATEREQUAL([ null, null ], [ null, false ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_IN function
////////////////////////////////////////////////////////////////////////////////

    testRelationalInUndefined : function () {
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, null));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, true));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, false));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, 0.0));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, 1.0));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, -1.0));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, ''));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, '0'));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, [ ]));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, [ 0 ]));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, [ 0, 1 ]));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, [ 1, 2 ]));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, { }));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, { 'a' : 1 }));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, { '0' : false }));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN(0.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN(1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN(-1.0, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN('', undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN('0', undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN('1', undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0, 1 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1, 2 ], undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN({ 'a' : 1 }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN({ '0' : false }, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN(NaN, false));
      assertEqual(undefined, AQL_RELATIONAL_IN(NaN, true));
      assertEqual(undefined, AQL_RELATIONAL_IN(NaN, ''));
      assertEqual(undefined, AQL_RELATIONAL_IN(NaN, 0));
      assertEqual(undefined, AQL_RELATIONAL_IN(NaN, null));
      assertEqual(undefined, AQL_RELATIONAL_IN(NaN, undefined));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, NaN));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, NaN));
      assertEqual(undefined, AQL_RELATIONAL_IN('', NaN));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, NaN));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, NaN));
      assertEqual(undefined, AQL_RELATIONAL_IN(undefined, NaN));
      
      assertEqual(undefined, AQL_RELATIONAL_IN(null, null));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, false));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, true));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, 0));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, 1));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, ''));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, { }));
      assertEqual(undefined, AQL_RELATIONAL_IN(null, { 'A' : true }));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, null));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, false));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, true));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, 0));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, 1));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, ''));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, { }));
      assertEqual(undefined, AQL_RELATIONAL_IN(false, { 'A' : true }));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, null));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, false));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, true));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, 0));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, 1));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, ''));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, { }));
      assertEqual(undefined, AQL_RELATIONAL_IN(true, { 'A' : true }));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, null));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, false));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, true));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, 0));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, 1));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, ''));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, { }));
      assertEqual(undefined, AQL_RELATIONAL_IN(0, { 'A' : true }));
      assertEqual(undefined, AQL_RELATIONAL_IN(1, null));
      assertEqual(undefined, AQL_RELATIONAL_IN(1, false));
      assertEqual(undefined, AQL_RELATIONAL_IN(1, true));
      assertEqual(undefined, AQL_RELATIONAL_IN(1, 0));
      assertEqual(undefined, AQL_RELATIONAL_IN(1, 1));
      assertEqual(undefined, AQL_RELATIONAL_IN(1, ''));
      assertEqual(undefined, AQL_RELATIONAL_IN(1, '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN(1, 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN(1, { }));
      assertEqual(undefined, AQL_RELATIONAL_IN(1, { 'A' : true }));
      assertEqual(undefined, AQL_RELATIONAL_IN('', null));
      assertEqual(undefined, AQL_RELATIONAL_IN('', false));
      assertEqual(undefined, AQL_RELATIONAL_IN('', true));
      assertEqual(undefined, AQL_RELATIONAL_IN('', 0));
      assertEqual(undefined, AQL_RELATIONAL_IN('', 1));
      assertEqual(undefined, AQL_RELATIONAL_IN('', ''));
      assertEqual(undefined, AQL_RELATIONAL_IN('', '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN('', 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN('', { }));
      assertEqual(undefined, AQL_RELATIONAL_IN('', { 'A' : true }));
      assertEqual(undefined, AQL_RELATIONAL_IN('a', null));
      assertEqual(undefined, AQL_RELATIONAL_IN('a', false));
      assertEqual(undefined, AQL_RELATIONAL_IN('a', true));
      assertEqual(undefined, AQL_RELATIONAL_IN('a', 0));
      assertEqual(undefined, AQL_RELATIONAL_IN('a', 1));
      assertEqual(undefined, AQL_RELATIONAL_IN('a', ''));
      assertEqual(undefined, AQL_RELATIONAL_IN('a', '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN('a', 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN('a', { }));
      assertEqual(undefined, AQL_RELATIONAL_IN('a', { 'A' : true }));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], null));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], false));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], true));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], 0));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], 1));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], ''));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], { }));
      assertEqual(undefined, AQL_RELATIONAL_IN([ ], { 'A' : true }));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], null));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], false));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], true));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], 0));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], 1));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], ''));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], { }));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 0 ], { 'A' : true }));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1 ], null));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1 ], false));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1 ], true));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1 ], 0));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1 ], 1));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1 ], ''));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1 ], '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1 ], 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1 ], { }));
      assertEqual(undefined, AQL_RELATIONAL_IN([ 1 ], { 'A' : true }));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, null));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, false));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, true));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, 0));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, 1));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, ''));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, '1'));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, 'a'));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, { }));
      assertEqual(undefined, AQL_RELATIONAL_IN({ }, { 'A' : true }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_IN function
////////////////////////////////////////////////////////////////////////////////

    testRelationalInTrue : function () {
      assertTrue(AQL_RELATIONAL_IN(null, [ null ]));
      assertTrue(AQL_RELATIONAL_IN(null, [ null, false ]));
      assertTrue(AQL_RELATIONAL_IN(null, [ false, null ]));
      assertTrue(AQL_RELATIONAL_IN(false, [ false ]));
      assertTrue(AQL_RELATIONAL_IN(false, [ true, false ]));
      assertTrue(AQL_RELATIONAL_IN(false, [ 0, false ]));
      assertTrue(AQL_RELATIONAL_IN(true, [ true ]));
      assertTrue(AQL_RELATIONAL_IN(true, [ false, true ]));
      assertTrue(AQL_RELATIONAL_IN(true, [ 0, false, true ]));
      assertTrue(AQL_RELATIONAL_IN(0, [ 0 ]));
      assertTrue(AQL_RELATIONAL_IN(1, [ 1 ]));
      assertTrue(AQL_RELATIONAL_IN(0, [ 3, 2, 1, 0 ]));
      assertTrue(AQL_RELATIONAL_IN(1, [ 3, 2, 1, 0 ]));
      assertTrue(AQL_RELATIONAL_IN(-35.5, [ 3, 2, 1, -35.5, 0 ]));
      assertTrue(AQL_RELATIONAL_IN(1.23e32, [ 3, 2, 1, 1.23e32, 35.5, 0 ]));
      assertTrue(AQL_RELATIONAL_IN('', [ '' ]));
      assertTrue(AQL_RELATIONAL_IN('', [ ' ', '' ]));
      assertTrue(AQL_RELATIONAL_IN('', [ 'a', 'b', 'c', '' ]));
      assertTrue(AQL_RELATIONAL_IN('A', [ 'c', 'b', 'a', 'A' ]));
      assertTrue(AQL_RELATIONAL_IN(' ', [ ' ' ]));
      assertTrue(AQL_RELATIONAL_IN(' a', [ ' a' ]));
      assertTrue(AQL_RELATIONAL_IN(' a ', [ ' a ' ]));
      assertTrue(AQL_RELATIONAL_IN([ ], [ [ ] ]));
      assertTrue(AQL_RELATIONAL_IN([ ], [ 1, null, 2, 3, [ ], 5 ]));
      assertTrue(AQL_RELATIONAL_IN([ null ], [ [ null ] ]));
      assertTrue(AQL_RELATIONAL_IN([ null ], [ null, [ null ], true ]));
      assertTrue(AQL_RELATIONAL_IN([ null, false ], [ [ null, false ] ]));
      assertTrue(AQL_RELATIONAL_IN([ 'a', 'A', false ], [ 'a', true, [ 'a', 'A', false ] ]));
      assertTrue(AQL_RELATIONAL_IN({ }, [ { } ]));
      assertTrue(AQL_RELATIONAL_IN({ }, [ 'a', null, false, 0, { } ]));
      assertTrue(AQL_RELATIONAL_IN({ 'a' : true }, [ 'a', null, false, 0, { 'a' : true } ]));
      assertTrue(AQL_RELATIONAL_IN({ 'a' : true, 'A': false }, [ 'a', null, false, 0, { 'A' : false, 'a' : true } ]));
      assertTrue(AQL_RELATIONAL_IN({ 'a' : { 'b' : null, 'c': 1 } }, [ { 'a' : { 'c' : 1, 'b' : null } } ]));
      assertTrue(AQL_RELATIONAL_IN({ 'a' : { 'b' : null, 'c': 1 } }, [ 'a', 'b', { 'a' : { 'c' : 1, 'b' : null } } ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_RELATIONAL_IN function
////////////////////////////////////////////////////////////////////////////////

    testRelationalInFalse : function () {
      assertFalse(AQL_RELATIONAL_IN(null, [ ]));
      assertFalse(AQL_RELATIONAL_IN(false, [ ]));
      assertFalse(AQL_RELATIONAL_IN(true, [ ]));
      assertFalse(AQL_RELATIONAL_IN(0, [ ]));
      assertFalse(AQL_RELATIONAL_IN(1, [ ]));
      assertFalse(AQL_RELATIONAL_IN('', [ ]));
      assertFalse(AQL_RELATIONAL_IN('0', [ ]));
      assertFalse(AQL_RELATIONAL_IN('1', [ ]));
      assertFalse(AQL_RELATIONAL_IN([ ], [ ]));
      assertFalse(AQL_RELATIONAL_IN([ 0 ], [ ]));
      assertFalse(AQL_RELATIONAL_IN({ }, [ ]));
      assertFalse(AQL_RELATIONAL_IN({ 'a' : true }, [ ]));
      assertFalse(AQL_RELATIONAL_IN(null, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, true, false, { }, [ ] ]));
      assertFalse(AQL_RELATIONAL_IN(true, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, false, { }, [ ] ]));
      assertFalse(AQL_RELATIONAL_IN(false, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, { }, [ ] ]));
      assertFalse(AQL_RELATIONAL_IN(0, [ '0', '1', '', 'null', 'true', 'false', -1, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AQL_RELATIONAL_IN(1, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AQL_RELATIONAL_IN(-1, [ '0', '1', '', 'null', 'true', 'false', 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AQL_RELATIONAL_IN('', [ '0', '1', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AQL_RELATIONAL_IN('0', [ '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AQL_RELATIONAL_IN('1', [ '0', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AQL_RELATIONAL_IN([ ], [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { } ]));
      assertFalse(AQL_RELATIONAL_IN({ }, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, [ ] ]));
      assertFalse(AQL_RELATIONAL_IN(null, [ [ null ]]));
      assertFalse(AQL_RELATIONAL_IN(false, [ [ false ]]));
      assertFalse(AQL_RELATIONAL_IN(true, [ [ true ]]));
      assertFalse(AQL_RELATIONAL_IN(0, [ [ 0 ]]));
      assertFalse(AQL_RELATIONAL_IN(1, [ [ 1 ]]));
      assertFalse(AQL_RELATIONAL_IN('', [ [ '' ]]));
      assertFalse(AQL_RELATIONAL_IN('1', [ [ '1' ]]));
      assertFalse(AQL_RELATIONAL_IN('', [ [ '' ]]));
      assertFalse(AQL_RELATIONAL_IN(' ', [ [ ' ' ]]));
      assertFalse(AQL_RELATIONAL_IN('a', [ 'A' ]));
      assertFalse(AQL_RELATIONAL_IN('a', [ 'b', 'c', 'd' ]));
      assertFalse(AQL_RELATIONAL_IN('', [ ' ', '  ' ]));
      assertFalse(AQL_RELATIONAL_IN(' ', [ '', '  ' ]));
      assertFalse(AQL_RELATIONAL_IN([ ], [ 0 ]));
      assertFalse(AQL_RELATIONAL_IN([ ], [ 1, 2 ]));
      assertFalse(AQL_RELATIONAL_IN([ 0 ], [ 1, 2 ]));
      assertFalse(AQL_RELATIONAL_IN([ 1 ], [ 1, 2 ]));
      assertFalse(AQL_RELATIONAL_IN([ 2 ], [ 1, 2 ]));
      assertFalse(AQL_RELATIONAL_IN([ 1, 2 ], [ 1, 2 ]));
      assertFalse(AQL_RELATIONAL_IN([ 1, 2 ], [ [ 1 ], [ 2 ] ]));
      assertFalse(AQL_RELATIONAL_IN([ 1, 2 ], [ [ 2, 1 ] ]));
      assertFalse(AQL_RELATIONAL_IN([ 1, 2 ], [ [ 1, 2, 3 ] ]));
      assertFalse(AQL_RELATIONAL_IN([ 1, 2, 3 ], [ [ 1, 2, 4 ] ]));
      assertFalse(AQL_RELATIONAL_IN([ 1, 2, 3 ], [ [ 0, 1, 2, 3 ] ]));
      assertFalse(AQL_RELATIONAL_IN({ 'a' : true }, [ { 'a' : true, 'b' : false } ]));
      assertFalse(AQL_RELATIONAL_IN({ 'a' : true }, [ { 'a' : false } ]));
      assertFalse(AQL_RELATIONAL_IN({ 'a' : true }, [ { 'b' : true } ]));
      assertFalse(AQL_RELATIONAL_IN({ 'a' : true }, [ [ { 'a' : true } ] ]));
      assertFalse(AQL_RELATIONAL_IN({ 'a' : true }, [ 1, 2, { 'a' : { 'a' : true } } ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_UNARY_PLUS function
////////////////////////////////////////////////////////////////////////////////

    testUnaryPlusUndefined : function () {
      assertEqual(undefined, AQL_UNARY_PLUS(undefined));
      assertEqual(undefined, AQL_UNARY_PLUS(null));
      assertEqual(undefined, AQL_UNARY_PLUS(false));
      assertEqual(undefined, AQL_UNARY_PLUS(true));
      assertEqual(undefined, AQL_UNARY_PLUS(' '));
      assertEqual(undefined, AQL_UNARY_PLUS('abc'));
      assertEqual(undefined, AQL_UNARY_PLUS('1abc'));
      assertEqual(undefined, AQL_UNARY_PLUS(''));
      assertEqual(undefined, AQL_UNARY_PLUS('-1'));
      assertEqual(undefined, AQL_UNARY_PLUS('0'));
      assertEqual(undefined, AQL_UNARY_PLUS('1'));
      assertEqual(undefined, AQL_UNARY_PLUS('1.5'));
      assertEqual(undefined, AQL_UNARY_PLUS([ ]));
      assertEqual(undefined, AQL_UNARY_PLUS([ 0 ]));
      assertEqual(undefined, AQL_UNARY_PLUS([ 1 ]));
      assertEqual(undefined, AQL_UNARY_PLUS({ 'a' : 1 }));
      assertEqual(undefined, AQL_UNARY_PLUS(NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_UNARY_PLUS function
////////////////////////////////////////////////////////////////////////////////

    testUnaryPlusValue : function () {
      assertEqual(0, AQL_UNARY_PLUS(0));
      assertEqual(1, AQL_UNARY_PLUS(1));
      assertEqual(-1, AQL_UNARY_PLUS(-1));
      assertEqual(0.0001, AQL_UNARY_PLUS(0.0001));
      assertEqual(-0.0001, AQL_UNARY_PLUS(-0.0001));
      assertEqual(100, AQL_UNARY_PLUS(100));
      assertEqual(1054.342, AQL_UNARY_PLUS(1054.342));
      assertEqual(-1054.342, AQL_UNARY_PLUS(-1054.342));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_UNARY_MINUS function
////////////////////////////////////////////////////////////////////////////////

    testUnaryMinusUndefined : function () {
      assertEqual(undefined, AQL_UNARY_MINUS(undefined));
      assertEqual(undefined, AQL_UNARY_MINUS(null));
      assertEqual(undefined, AQL_UNARY_MINUS(false));
      assertEqual(undefined, AQL_UNARY_MINUS(true));
      assertEqual(undefined, AQL_UNARY_MINUS(' '));
      assertEqual(undefined, AQL_UNARY_MINUS('abc'));
      assertEqual(undefined, AQL_UNARY_MINUS('1abc'));
      assertEqual(undefined, AQL_UNARY_MINUS(''));
      assertEqual(undefined, AQL_UNARY_MINUS('-1'));
      assertEqual(undefined, AQL_UNARY_MINUS('0'));
      assertEqual(undefined, AQL_UNARY_MINUS('1'));
      assertEqual(undefined, AQL_UNARY_MINUS('1.5'));
      assertEqual(undefined, AQL_UNARY_MINUS([ ]));
      assertEqual(undefined, AQL_UNARY_MINUS([ 0 ]));
      assertEqual(undefined, AQL_UNARY_MINUS([ 1 ]));
      assertEqual(undefined, AQL_UNARY_MINUS({ 'a' : 1 }));
      assertEqual(undefined, AQL_UNARY_PLUS(NaN));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_UNARY_MINUS function
////////////////////////////////////////////////////////////////////////////////
  
    testUnaryMinusValue : function () {
      assertEqual(0, AQL_UNARY_MINUS(0));
      assertEqual(1, AQL_UNARY_MINUS(-1));
      assertEqual(-1, AQL_UNARY_MINUS(1));
      assertEqual(0.0001, AQL_UNARY_MINUS(-0.0001));
      assertEqual(-0.0001, AQL_UNARY_MINUS(0.0001));
      assertEqual(100, AQL_UNARY_MINUS(-100));
      assertEqual(-100, AQL_UNARY_MINUS(100));
      assertEqual(1054.342, AQL_UNARY_MINUS(-1054.342));
      assertEqual(-1054.342, AQL_UNARY_MINUS(1054.342));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_ARITHMETIC_PLUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPlusUndefined : function () {
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, null));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, false));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, true));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, 0));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, 2));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, -1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, ''));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, '0'));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, ' '));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, 'a'));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, [ ]));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, [ 1 ]));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, { }));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(undefined, NaN));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(null, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(false, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(true, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(0, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(2, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(-1, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS('', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS('0', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(' ', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS('a', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS([ ], undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS([ 1 ], undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS({ }, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(NaN, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, NaN));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, null));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, false));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, true));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, ''));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, ' '));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, '0'));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, '1'));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, 'a'));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, [ ]));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, [ 0 ]));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, { }));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1, { 'a' : 0 }));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(NaN, 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(null, 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(false, 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(true, 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS('', 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(' ', 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS('0', 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS('1', 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS('a', 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS([ ], 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS([ 0 ], 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS({ }, 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS({ 'a' : 0 }, 1));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS('0', '0'));
      assertEqual(undefined, AQL_ARITHMETIC_PLUS(1.3e317, 1.3e317));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_ARITHMETIC_PLUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPlusValue : function () {
      assertEqual(0, AQL_ARITHMETIC_PLUS(0, 0));
      assertEqual(0, AQL_ARITHMETIC_PLUS(1, -1));
      assertEqual(0, AQL_ARITHMETIC_PLUS(-1, 1));
      assertEqual(0, AQL_ARITHMETIC_PLUS(-100, 100));
      assertEqual(0, AQL_ARITHMETIC_PLUS(100, -100));
      assertEqual(0, AQL_ARITHMETIC_PLUS(0.11, -0.11));
      assertEqual(10, AQL_ARITHMETIC_PLUS(5, 5));
      assertEqual(10, AQL_ARITHMETIC_PLUS(4, 6));
      assertEqual(10, AQL_ARITHMETIC_PLUS(1, 9));
      assertEqual(10, AQL_ARITHMETIC_PLUS(0.1, 9.9));
      assertEqual(9.8, AQL_ARITHMETIC_PLUS(-0.1, 9.9));
      assertEqual(-34.2, AQL_ARITHMETIC_PLUS(-17.1, -17.1));
      assertEqual(-2, AQL_ARITHMETIC_PLUS(-1, -1)); 
      assertEqual(2.6e307, AQL_ARITHMETIC_PLUS(1.3e307, 1.3e307));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_ARITHMETIC_MINUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticMinusUndefined : function () {
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, null));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, false));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, true));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, 0));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, 2));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, -1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, ''));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, '0'));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, ' '));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, 'a'));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, [ ]));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, [ 1 ]));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, { }));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(undefined, NaN));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(null, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(false, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(true, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(0, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(2, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(-1, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS('', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS('0', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(' ', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS('a', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS([ ], undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS([ 1 ], undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS({ }, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(NaN, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, NaN));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, null));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, false));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, true));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, ''));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, ' '));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, '0'));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, '1'));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, 'a'));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, [ ]));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, [ 0 ]));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, { }));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(1, { 'a' : 0 }));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(NaN, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(null, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(false, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(true, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS('', 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(' ', 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS('0', 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS('1', 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS('a', 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS([ ], 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS([ 0 ], 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS({ }, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS({ 'a' : 0 }, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS('0', '0'));
      assertEqual(undefined, AQL_ARITHMETIC_MINUS(-1.3e317, 1.3e317));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_ARITHMETIC_MINUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticMinusValue : function () {
      assertEqual(0, AQL_ARITHMETIC_MINUS(0, 0));
      assertEqual(-1, AQL_ARITHMETIC_MINUS(0, 1));
      assertEqual(0, AQL_ARITHMETIC_MINUS(-1, -1)); 
      assertEqual(0, AQL_ARITHMETIC_MINUS(1, 1));
      assertEqual(2, AQL_ARITHMETIC_MINUS(1, -1));
      assertEqual(-2, AQL_ARITHMETIC_MINUS(-1, 1));
      assertEqual(-200, AQL_ARITHMETIC_MINUS(-100, 100));
      assertEqual(200, AQL_ARITHMETIC_MINUS(100, -100));
      assertEqual(0.22, AQL_ARITHMETIC_MINUS(0.11, -0.11));
      assertEqual(0, AQL_ARITHMETIC_MINUS(5, 5));
      assertEqual(-2, AQL_ARITHMETIC_MINUS(4, 6));
      assertEqual(-8, AQL_ARITHMETIC_MINUS(1, 9));
      assertEqual(-9.8, AQL_ARITHMETIC_MINUS(0.1, 9.9));
      assertEqual(-10, AQL_ARITHMETIC_MINUS(-0.1, 9.9));
      assertEqual(0, AQL_ARITHMETIC_MINUS(-17.1, -17.1));
      assertEqual(0, AQL_ARITHMETIC_MINUS(1.3e307, 1.3e307));
      assertEqual(2.6e307, AQL_ARITHMETIC_MINUS(1.3e307, -1.3e307));
      assertEqual(-2.6e307, AQL_ARITHMETIC_MINUS(-1.3e307, 1.3e307));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_ARITHMETIC_TIMES function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticTimesUndefined : function () {
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, null));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, false));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, true));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, 0));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, 2));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, -1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, ''));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, '0'));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, ' '));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, 'a'));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, [ ]));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, [ 1 ]));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, { }));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(undefined, NaN));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(null, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(false, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(true, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(0, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(2, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(-1, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES('', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES('0', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(' ', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES('a', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES([ ], undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES([ 1 ], undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES({ }, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(NaN, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, NaN));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, null));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, false));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, true));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, ''));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, ' '));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, '0'));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, '1'));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, 'a'));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, [ ]));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, [ 0 ]));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, { }));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1, { 'a' : 0 }));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(NaN, 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(null, 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(false, 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(true, 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES('', 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(' ', 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES('0', 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES('1', 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES('a', 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES([ ], 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES([ 0 ], 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES({ }, 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES({ 'a' : 0 }, 1));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1.3e190, 1.3e190));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1.3e307, 1.3e307));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES(1.3e317, 1.3e317));
      assertEqual(undefined, AQL_ARITHMETIC_TIMES('0', '0'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_ARITHMETIC_TIMES function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticTimesValue : function () {
      assertEqual(0, AQL_ARITHMETIC_TIMES(0, 0));
      assertEqual(0, AQL_ARITHMETIC_TIMES(1, 0));
      assertEqual(0, AQL_ARITHMETIC_TIMES(0, 1));
      assertEqual(1, AQL_ARITHMETIC_TIMES(1, 1));
      assertEqual(2, AQL_ARITHMETIC_TIMES(2, 1));
      assertEqual(2, AQL_ARITHMETIC_TIMES(1, 2));
      assertEqual(4, AQL_ARITHMETIC_TIMES(2, 2));
      assertEqual(100, AQL_ARITHMETIC_TIMES(10, 10));
      assertEqual(1000, AQL_ARITHMETIC_TIMES(10, 100));
      assertEqual(1.44, AQL_ARITHMETIC_TIMES(1.2, 1.2));
      assertEqual(-1.44, AQL_ARITHMETIC_TIMES(1.2, -1.2));
      assertEqual(1.44, AQL_ARITHMETIC_TIMES(-1.2, -1.2));
      assertEqual(2, AQL_ARITHMETIC_TIMES(0.25, 8));
      assertEqual(2, AQL_ARITHMETIC_TIMES(8, 0.25));
      assertEqual(2.25, AQL_ARITHMETIC_TIMES(9, 0.25));
      assertEqual(-2.25, AQL_ARITHMETIC_TIMES(9, -0.25));
      assertEqual(-2.25, AQL_ARITHMETIC_TIMES(-9, 0.25));
      assertEqual(2.25, AQL_ARITHMETIC_TIMES(-9, -0.25));
      assertEqual(4, AQL_ARITHMETIC_TIMES(-2, -2));
      assertEqual(-4, AQL_ARITHMETIC_TIMES(-2, 2));
      assertEqual(-4, AQL_ARITHMETIC_TIMES(2, -2));
      assertEqual(1000000, AQL_ARITHMETIC_TIMES(1000, 1000));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_ARITHMETIC_DIVIDE function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticDivideUndefined : function () {
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, null));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, false));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, true));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, 0));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, 2));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, -1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, ''));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, '0'));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, ' '));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, 'a'));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, [ ]));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, [ 1 ]));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, { }));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(undefined, NaN));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(null, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(false, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(true, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(0, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(2, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(-1, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE('', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE('0', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(' ', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE('a', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE([ ], undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE([ 1 ], undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE({ }, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(NaN, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, NaN));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, null));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, false));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, true));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, ''));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, ' '));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, '0'));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, '1'));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, 'a'));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, [ ]));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, [ 0 ]));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, { }));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, { 'a' : 0 }));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(NaN, 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(null, 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(false, 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(true, 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE('', 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(' ', 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE('0', 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE('1', 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE('a', 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE([ ], 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE([ 0 ], 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE({ }, 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE({ 'a' : 0 }, 1));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(1, 0));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(100, 0));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(-1, 0));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(-100, 0));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE(0, 0));
      assertEqual(undefined, AQL_ARITHMETIC_DIVIDE('0', '0'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_ARITHMETIC_DIVIDE function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticDivideValue : function () {
      assertEqual(0, AQL_ARITHMETIC_DIVIDE(0, 1));
      assertEqual(0, AQL_ARITHMETIC_DIVIDE(0, 2));
      assertEqual(0, AQL_ARITHMETIC_DIVIDE(0, 10));
      assertEqual(0, AQL_ARITHMETIC_DIVIDE(0, -1));
      assertEqual(0, AQL_ARITHMETIC_DIVIDE(0, -2));
      assertEqual(0, AQL_ARITHMETIC_DIVIDE(0, -10));
      assertEqual(1, AQL_ARITHMETIC_DIVIDE(1, 1));
      assertEqual(2, AQL_ARITHMETIC_DIVIDE(2, 1));
      assertEqual(-1, AQL_ARITHMETIC_DIVIDE(-1, 1));
      assertEqual(100, AQL_ARITHMETIC_DIVIDE(100, 1));
      assertEqual(-100, AQL_ARITHMETIC_DIVIDE(-100, 1));
      assertEqual(-1, AQL_ARITHMETIC_DIVIDE(1, -1));
      assertEqual(-2, AQL_ARITHMETIC_DIVIDE(2, -1));
      assertEqual(1, AQL_ARITHMETIC_DIVIDE(-1, -1));
      assertEqual(-100, AQL_ARITHMETIC_DIVIDE(100, -1));
      assertEqual(100, AQL_ARITHMETIC_DIVIDE(-100, -1));
      assertEqual(0.5, AQL_ARITHMETIC_DIVIDE(1, 2));
      assertEqual(1, AQL_ARITHMETIC_DIVIDE(2, 2));
      assertEqual(2, AQL_ARITHMETIC_DIVIDE(4, 2));
      assertEqual(1, AQL_ARITHMETIC_DIVIDE(4, 4));
      assertEqual(5, AQL_ARITHMETIC_DIVIDE(10, 2));
      assertEqual(2, AQL_ARITHMETIC_DIVIDE(10, 5));
      assertEqual(2.5, AQL_ARITHMETIC_DIVIDE(10, 4));
      assertEqual(1, AQL_ARITHMETIC_DIVIDE(1.2, 1.2));
      assertEqual(-1, AQL_ARITHMETIC_DIVIDE(1.2, -1.2));
      assertEqual(1, AQL_ARITHMETIC_DIVIDE(-1.2, -1.2));
      assertEqual(2, AQL_ARITHMETIC_DIVIDE(1, 0.5));
      assertEqual(4, AQL_ARITHMETIC_DIVIDE(1, 0.25));
      assertEqual(16, AQL_ARITHMETIC_DIVIDE(4, 0.25));
      assertEqual(-16, AQL_ARITHMETIC_DIVIDE(4, -0.25));
      assertEqual(-16, AQL_ARITHMETIC_DIVIDE(-4, 0.25));
      assertEqual(100, AQL_ARITHMETIC_DIVIDE(25, 0.25));
      assertEqual(-100, AQL_ARITHMETIC_DIVIDE(25, -0.25));
      assertEqual(-100, AQL_ARITHMETIC_DIVIDE(-25, 0.25));
      assertEqual(1, AQL_ARITHMETIC_DIVIDE(0.22, 0.22));
      assertEqual(0.5, AQL_ARITHMETIC_DIVIDE(0.22, 0.44));
      assertEqual(2, AQL_ARITHMETIC_DIVIDE(0.22, 0.11));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_ARITHMETIC_MODULUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticModulusUndefined : function () {
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, null));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, false));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, true));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, 0));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, 2));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, -1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, ''));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, '0'));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, ' '));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, 'a'));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, [ ]));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, [ 1 ]));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, { }));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(undefined, NaN));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(null, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(false, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(true, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(0, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(2, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(-1, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS('', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS('0', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(' ', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS('a', undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS([ ], undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS([ 1 ], undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS({ }, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(NaN, undefined));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, NaN));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, null));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, false));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, true));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, ''));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, ' '));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, '0'));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, '1'));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, 'a'));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, [ ]));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, [ 0 ]));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, { }));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, { 'a' : 0 }));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(NaN, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(null, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(false, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(true, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS('', 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(' ', 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS('0', 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS('1', 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS('a', 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS([ ], 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS([ 0 ], 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS({ }, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS({ 'a' : 0 }, 1));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(1, 0));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(100, 0));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(-1, 0));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(-100, 0));
      assertEqual(undefined, AQL_ARITHMETIC_MODULUS(0, 0));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_ARITHMETIC_MODULUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticModulusValue : function () {
      assertEqual(0, AQL_ARITHMETIC_MODULUS(0, 1));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(1, 1));
      assertEqual(1, AQL_ARITHMETIC_MODULUS(1, 2));
      assertEqual(1, AQL_ARITHMETIC_MODULUS(1, 3));
      assertEqual(1, AQL_ARITHMETIC_MODULUS(1, 4));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(2, 2));
      assertEqual(2, AQL_ARITHMETIC_MODULUS(2, 3));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(3, 3));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(10, 1));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(10, 2));
      assertEqual(1, AQL_ARITHMETIC_MODULUS(10, 3));
      assertEqual(2, AQL_ARITHMETIC_MODULUS(10, 4));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(10, 5));
      assertEqual(4, AQL_ARITHMETIC_MODULUS(10, 6));
      assertEqual(3, AQL_ARITHMETIC_MODULUS(10, 7));
      assertEqual(2, AQL_ARITHMETIC_MODULUS(10, 8));
      assertEqual(1, AQL_ARITHMETIC_MODULUS(10, 9));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(10, 10));
      assertEqual(10, AQL_ARITHMETIC_MODULUS(10, 11));
      assertEqual(1, AQL_ARITHMETIC_MODULUS(4, 3));
      assertEqual(2, AQL_ARITHMETIC_MODULUS(5, 3));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(6, 3));
      assertEqual(1, AQL_ARITHMETIC_MODULUS(7, 3));
      assertEqual(2, AQL_ARITHMETIC_MODULUS(8, 3));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(9, 3));
      assertEqual(1, AQL_ARITHMETIC_MODULUS(10, 3));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(10, -1));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(10, -2));
      assertEqual(1, AQL_ARITHMETIC_MODULUS(10, -3));
      assertEqual(2, AQL_ARITHMETIC_MODULUS(10, -4));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(10, -5));
      assertEqual(4, AQL_ARITHMETIC_MODULUS(10, -6));
      assertEqual(3, AQL_ARITHMETIC_MODULUS(10, -7));
      assertEqual(2, AQL_ARITHMETIC_MODULUS(10, -8));
      assertEqual(1, AQL_ARITHMETIC_MODULUS(10, -9));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(10, -10));
      assertEqual(10, AQL_ARITHMETIC_MODULUS(10, -11));
      assertEqual(-1, AQL_ARITHMETIC_MODULUS(-1, 3));
      assertEqual(-2, AQL_ARITHMETIC_MODULUS(-2, 3));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(-3, 3));
      assertEqual(-1, AQL_ARITHMETIC_MODULUS(-4, 3));
      assertEqual(-2, AQL_ARITHMETIC_MODULUS(-5, 3));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(-6, 3));
      assertEqual(-1, AQL_ARITHMETIC_MODULUS(-7, 3));
      assertEqual(-2, AQL_ARITHMETIC_MODULUS(-8, 3));
      assertEqual(0, AQL_ARITHMETIC_MODULUS(-9, 3));
      assertEqual(-1, AQL_ARITHMETIC_MODULUS(-10, 3));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_STRING_CONCAT function
////////////////////////////////////////////////////////////////////////////////

    testStringConcatUndefined : function () {
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, null));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, false));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, true));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, 0));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, 1));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, 2));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, -1));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, ''));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, '0'));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, ' '));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, 'a'));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, [ ]));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, [ 1 ]));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, { }));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, { 'a' : 0 }));
      assertEqual(undefined, AQL_STRING_CONCAT(undefined, NaN));
      assertEqual(undefined, AQL_STRING_CONCAT(null, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT(false, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT(true, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT(0, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT(1, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT(2, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT(-1, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT('', undefined));
      assertEqual(undefined, AQL_STRING_CONCAT('0', undefined));
      assertEqual(undefined, AQL_STRING_CONCAT(' ', undefined));
      assertEqual(undefined, AQL_STRING_CONCAT('a', undefined));
      assertEqual(undefined, AQL_STRING_CONCAT([ ], undefined));
      assertEqual(undefined, AQL_STRING_CONCAT([ 1 ], undefined));
      assertEqual(undefined, AQL_STRING_CONCAT({ }, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT({ 'a' : 0 }, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT(NaN, undefined));
      assertEqual(undefined, AQL_STRING_CONCAT(1, NaN));
      assertEqual(undefined, AQL_STRING_CONCAT(1, null));
      assertEqual(undefined, AQL_STRING_CONCAT(1, false));
      assertEqual(undefined, AQL_STRING_CONCAT(1, true));
      assertEqual(undefined, AQL_STRING_CONCAT(1, ''));
      assertEqual(undefined, AQL_STRING_CONCAT(1, ' '));
      assertEqual(undefined, AQL_STRING_CONCAT(1, '0'));
      assertEqual(undefined, AQL_STRING_CONCAT(1, '1'));
      assertEqual(undefined, AQL_STRING_CONCAT(1, 'a'));
      assertEqual(undefined, AQL_STRING_CONCAT(1, [ ]));
      assertEqual(undefined, AQL_STRING_CONCAT(1, [ 0 ]));
      assertEqual(undefined, AQL_STRING_CONCAT(1, { }));
      assertEqual(undefined, AQL_STRING_CONCAT(1, { 'a' : 0 }));
      assertEqual(undefined, AQL_STRING_CONCAT(NaN, 1));
      assertEqual(undefined, AQL_STRING_CONCAT(null, 1));
      assertEqual(undefined, AQL_STRING_CONCAT(false, 1));
      assertEqual(undefined, AQL_STRING_CONCAT(true, 1));
      assertEqual(undefined, AQL_STRING_CONCAT('', 1));
      assertEqual(undefined, AQL_STRING_CONCAT(' ', 1));
      assertEqual(undefined, AQL_STRING_CONCAT('0', 1));
      assertEqual(undefined, AQL_STRING_CONCAT('1', 1));
      assertEqual(undefined, AQL_STRING_CONCAT('a', 1));
      assertEqual(undefined, AQL_STRING_CONCAT([ ], 1));
      assertEqual(undefined, AQL_STRING_CONCAT([ 0 ], 1));
      assertEqual(undefined, AQL_STRING_CONCAT({ }, 1));
      assertEqual(undefined, AQL_STRING_CONCAT({ 'a' : 0 }, 1));
      assertEqual(undefined, AQL_STRING_CONCAT(1, 0));
      assertEqual(undefined, AQL_STRING_CONCAT(100, 0));
      assertEqual(undefined, AQL_STRING_CONCAT(-1, 0));
      assertEqual(undefined, AQL_STRING_CONCAT(-100, 0));
      assertEqual(undefined, AQL_STRING_CONCAT(0, 0));
      assertEqual(undefined, AQL_STRING_CONCAT('', null));
      assertEqual(undefined, AQL_STRING_CONCAT('', false));
      assertEqual(undefined, AQL_STRING_CONCAT('', true));
      assertEqual(undefined, AQL_STRING_CONCAT('', [ ]));
      assertEqual(undefined, AQL_STRING_CONCAT('', { }));
      assertEqual(undefined, AQL_STRING_CONCAT('a', null));
      assertEqual(undefined, AQL_STRING_CONCAT('a', false));
      assertEqual(undefined, AQL_STRING_CONCAT('a', true));
      assertEqual(undefined, AQL_STRING_CONCAT('a', [ ]));
      assertEqual(undefined, AQL_STRING_CONCAT('a', { }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL_STRING_CONCAT function
////////////////////////////////////////////////////////////////////////////////

    testStringConcatValue : function () {
      assertEqual('', AQL_STRING_CONCAT('', ''));
      assertEqual(' ', AQL_STRING_CONCAT(' ', ''));
      assertEqual(' ', AQL_STRING_CONCAT('', ' '));
      assertEqual('  ', AQL_STRING_CONCAT(' ', ' '));
      assertEqual(' a a', AQL_STRING_CONCAT(' a', ' a'));
      assertEqual(' a a ', AQL_STRING_CONCAT(' a', ' a '));
      assertEqual('a', AQL_STRING_CONCAT('a', ''));
      assertEqual('a', AQL_STRING_CONCAT('', 'a'));
      assertEqual('a ', AQL_STRING_CONCAT('', 'a '));
      assertEqual('a ', AQL_STRING_CONCAT('', 'a '));
      assertEqual('a ', AQL_STRING_CONCAT('a ', ''));
      assertEqual('ab', AQL_STRING_CONCAT('a', 'b'));
      assertEqual('ba', AQL_STRING_CONCAT('b', 'a'));
      assertEqual('AA', AQL_STRING_CONCAT('A', 'A'));
      assertEqual('AaA', AQL_STRING_CONCAT('A', 'aA'));
      assertEqual('AaA', AQL_STRING_CONCAT('Aa', 'A'));
      assertEqual('0.00', AQL_STRING_CONCAT('0.00', ''));
      assertEqual('abc', AQL_STRING_CONCAT('a', AQL_STRING_CONCAT('b', 'c')));
      assertEqual('', AQL_STRING_CONCAT('', AQL_STRING_CONCAT('', '')));
    }

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlOperatorsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
