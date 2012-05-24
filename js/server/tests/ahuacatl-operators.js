////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, operators
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

function ahuacatlOperatorsTestSuite () {

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
/// @brief test AHUACATL_KEYS function
////////////////////////////////////////////////////////////////////////////////

    testKeys : function () {
      assertEqual([ ], AHUACATL_KEYS([ ], true));
      assertEqual([ 0, 1, 2 ], AHUACATL_KEYS([ 0, 1, 2 ], true));
      assertEqual([ 0, 1, 2 ], AHUACATL_KEYS([ 1, 2, 3 ], true));
      assertEqual([ 0, 1, 2 ], AHUACATL_KEYS([ 5, 6, 9 ], true));
      assertEqual([ 0, 1, 2 ], AHUACATL_KEYS([ false, false, false ], true));
      assertEqual([ 0, 1, 2 ], AHUACATL_KEYS([ -1, -1, 'zang' ], true));
      assertEqual([ 0, 1, 2, 3 ], AHUACATL_KEYS([ 99, 99, 99, 99 ], true));
      assertEqual([ 0 ], AHUACATL_KEYS([ [ ] ], true));
      assertEqual([ 0 ], AHUACATL_KEYS([ [ 1 ] ], true));
      assertEqual([ 0, 1 ], AHUACATL_KEYS([ [ 1 ], 1 ], true));
      assertEqual([ 0, 1 ], AHUACATL_KEYS([ [ 1 ], [ 1 ] ], true));
      assertEqual([ 0 ], AHUACATL_KEYS([ [ 1 , 2 ] ], true));
      assertEqual([ 0, 1, 2 ], AHUACATL_KEYS([ [ 1 , 2 ], [ ], 3 ], true));
      assertEqual([ 0, 1, 2 ], AHUACATL_KEYS([ { }, { }, { } ], true));
      assertEqual([ 0, 1, 2 ], AHUACATL_KEYS([ { 'a' : true, 'b' : false }, { }, { } ], true));
      assertEqual([ ], AHUACATL_KEYS({ }, true));
      assertEqual([ '0' ], AHUACATL_KEYS({ '0' : false }, true));
      assertEqual([ '0' ], AHUACATL_KEYS({ '0' : undefined }, true));
      assertEqual([ 'a', 'b', 'c' ], AHUACATL_KEYS({ 'a' : true, 'b' : true, 'c' : true }, true));
      assertEqual([ 'a', 'b', 'c' ], AHUACATL_KEYS({ 'a' : true, 'c' : true, 'b' : true }, true));
      assertEqual([ 'a', 'b', 'c' ], AHUACATL_KEYS({ 'b' : true, 'a' : true, 'c' : true }, true));
      assertEqual([ 'a', 'b', 'c' ], AHUACATL_KEYS({ 'b' : true, 'c' : true, 'a' : true }, true));
      assertEqual([ 'a', 'b', 'c' ], AHUACATL_KEYS({ 'c' : true, 'a' : true, 'b' : true }, true));
      assertEqual([ 'a', 'b', 'c' ], AHUACATL_KEYS({ 'c' : true, 'b' : true, 'a' : true }, true));
      assertEqual([ '0', '1', '2' ], AHUACATL_KEYS({ '0' : true, '1' : true, '2' : true }, true));
      assertEqual([ '0', '1', '2' ], AHUACATL_KEYS({ '1' : true, '2' : true, '0' : true }, true));
      assertEqual([ 'a1', 'a2', 'a3' ], AHUACATL_KEYS({ 'a1' : true, 'a2' : true, 'a3' : true }, true));
      assertEqual([ 'a1', 'a10', 'a20', 'a200', 'a21' ], AHUACATL_KEYS({ 'a200' : true, 'a21' : true, 'a20' : true, 'a10' : false, 'a1' : null }, true));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_NULL function
////////////////////////////////////////////////////////////////////////////////

    testIsNullTrue : function () {
      assertTrue(AHUACATL_IS_NULL(null));
      assertTrue(AHUACATL_IS_NULL(undefined));
      assertTrue(AHUACATL_IS_NULL(NaN));
      assertTrue(AHUACATL_IS_NULL(1.3e308 * 1.3e308));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_NULL function
////////////////////////////////////////////////////////////////////////////////

    testIsNullFalse : function () {
      assertFalse(AHUACATL_IS_NULL(0));
      assertFalse(AHUACATL_IS_NULL(1));
      assertFalse(AHUACATL_IS_NULL(-1));
      assertFalse(AHUACATL_IS_NULL(0.1));
      assertFalse(AHUACATL_IS_NULL(-0.1));
      assertFalse(AHUACATL_IS_NULL(false));
      assertFalse(AHUACATL_IS_NULL(true));
      assertFalse(AHUACATL_IS_NULL('abc'));
      assertFalse(AHUACATL_IS_NULL('null'));
      assertFalse(AHUACATL_IS_NULL('false'));
      assertFalse(AHUACATL_IS_NULL('undefined'));
      assertFalse(AHUACATL_IS_NULL(''));
      assertFalse(AHUACATL_IS_NULL(' '));
      assertFalse(AHUACATL_IS_NULL([ ]));
      assertFalse(AHUACATL_IS_NULL([ 0 ]));
      assertFalse(AHUACATL_IS_NULL([ 0, 1 ]));
      assertFalse(AHUACATL_IS_NULL([ 1, 2 ]));
      assertFalse(AHUACATL_IS_NULL({ }));
      assertFalse(AHUACATL_IS_NULL({ 'a' : 0 }));
      assertFalse(AHUACATL_IS_NULL({ 'a' : 1 }));
      assertFalse(AHUACATL_IS_NULL({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_BOOL function
////////////////////////////////////////////////////////////////////////////////

    testIsBoolTrue : function () {
      assertTrue(AHUACATL_IS_BOOL(false));
      assertTrue(AHUACATL_IS_BOOL(true));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_BOOL function
////////////////////////////////////////////////////////////////////////////////

    testIsBoolFalse : function () {
      assertFalse(AHUACATL_IS_BOOL(undefined));
      assertFalse(AHUACATL_IS_BOOL(NaN));
      assertFalse(AHUACATL_IS_BOOL(1.3e308 * 1.3e308));
      assertFalse(AHUACATL_IS_BOOL(0));
      assertFalse(AHUACATL_IS_BOOL(1));
      assertFalse(AHUACATL_IS_BOOL(-1));
      assertFalse(AHUACATL_IS_BOOL(0.1));
      assertFalse(AHUACATL_IS_BOOL(-0.1));
      assertFalse(AHUACATL_IS_BOOL(null));
      assertFalse(AHUACATL_IS_BOOL('abc'));
      assertFalse(AHUACATL_IS_BOOL('null'));
      assertFalse(AHUACATL_IS_BOOL('false'));
      assertFalse(AHUACATL_IS_BOOL('undefined'));
      assertFalse(AHUACATL_IS_BOOL(''));
      assertFalse(AHUACATL_IS_BOOL(' '));
      assertFalse(AHUACATL_IS_BOOL([ ]));
      assertFalse(AHUACATL_IS_BOOL([ 0 ]));
      assertFalse(AHUACATL_IS_BOOL([ 0, 1 ]));
      assertFalse(AHUACATL_IS_BOOL([ 1, 2 ]));
      assertFalse(AHUACATL_IS_BOOL([ '' ]));
      assertFalse(AHUACATL_IS_BOOL([ '0' ]));
      assertFalse(AHUACATL_IS_BOOL([ '1' ]));
      assertFalse(AHUACATL_IS_BOOL([ true ]));
      assertFalse(AHUACATL_IS_BOOL([ false ]));
      assertFalse(AHUACATL_IS_BOOL({ }));
      assertFalse(AHUACATL_IS_BOOL({ 'a' : 0 }));
      assertFalse(AHUACATL_IS_BOOL({ 'a' : 1 }));
      assertFalse(AHUACATL_IS_BOOL({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_NUMBER function
////////////////////////////////////////////////////////////////////////////////

    testIsNumberTrue : function () {
      assertTrue(AHUACATL_IS_NUMBER(0));
      assertTrue(AHUACATL_IS_NUMBER(1));
      assertTrue(AHUACATL_IS_NUMBER(-1));
      assertTrue(AHUACATL_IS_NUMBER(0.1));
      assertTrue(AHUACATL_IS_NUMBER(-0.1));
      assertTrue(AHUACATL_IS_NUMBER(12.5356));
      assertTrue(AHUACATL_IS_NUMBER(-235.26436));
      assertTrue(AHUACATL_IS_NUMBER(-23.3e17));
      assertTrue(AHUACATL_IS_NUMBER(563.44576e19));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_NUMBER function
////////////////////////////////////////////////////////////////////////////////

    testIsNumberFalse : function () {
      assertFalse(AHUACATL_IS_NUMBER(false));
      assertFalse(AHUACATL_IS_NUMBER(true));
      assertFalse(AHUACATL_IS_NUMBER(undefined));
      assertFalse(AHUACATL_IS_NUMBER(NaN));
      assertFalse(AHUACATL_IS_NUMBER(1.3e308 * 1.3e308));
      assertFalse(AHUACATL_IS_NUMBER(null));
      assertFalse(AHUACATL_IS_NUMBER('abc'));
      assertFalse(AHUACATL_IS_NUMBER('null'));
      assertFalse(AHUACATL_IS_NUMBER('false'));
      assertFalse(AHUACATL_IS_NUMBER('undefined'));
      assertFalse(AHUACATL_IS_NUMBER(''));
      assertFalse(AHUACATL_IS_NUMBER(' '));
      assertFalse(AHUACATL_IS_NUMBER([ ]));
      assertFalse(AHUACATL_IS_NUMBER([ 0 ]));
      assertFalse(AHUACATL_IS_NUMBER([ 0, 1 ]));
      assertFalse(AHUACATL_IS_NUMBER([ 1, 2 ]));
      assertFalse(AHUACATL_IS_NUMBER([ '' ]));
      assertFalse(AHUACATL_IS_NUMBER([ '0' ]));
      assertFalse(AHUACATL_IS_NUMBER([ '1' ]));
      assertFalse(AHUACATL_IS_NUMBER([ true ]));
      assertFalse(AHUACATL_IS_NUMBER([ false ]));
      assertFalse(AHUACATL_IS_NUMBER({ }));
      assertFalse(AHUACATL_IS_NUMBER({ 'a' : 0 }));
      assertFalse(AHUACATL_IS_NUMBER({ 'a' : 1 }));
      assertFalse(AHUACATL_IS_NUMBER({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_STRING function
////////////////////////////////////////////////////////////////////////////////

    testIsStringTrue : function () {
      assertTrue(AHUACATL_IS_STRING('abc'));
      assertTrue(AHUACATL_IS_STRING('null'));
      assertTrue(AHUACATL_IS_STRING('false'));
      assertTrue(AHUACATL_IS_STRING('undefined'));
      assertTrue(AHUACATL_IS_STRING(''));
      assertTrue(AHUACATL_IS_STRING(' '));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_STRING function
////////////////////////////////////////////////////////////////////////////////

    testIsStringFalse : function () {
      assertFalse(AHUACATL_IS_STRING(false));
      assertFalse(AHUACATL_IS_STRING(true));
      assertFalse(AHUACATL_IS_STRING(undefined));
      assertFalse(AHUACATL_IS_STRING(NaN));
      assertFalse(AHUACATL_IS_STRING(1.3e308 * 1.3e308));
      assertFalse(AHUACATL_IS_STRING(0));
      assertFalse(AHUACATL_IS_STRING(1));
      assertFalse(AHUACATL_IS_STRING(-1));
      assertFalse(AHUACATL_IS_STRING(0.1));
      assertFalse(AHUACATL_IS_STRING(-0.1));
      assertFalse(AHUACATL_IS_STRING(null));
      assertFalse(AHUACATL_IS_STRING([ ]));
      assertFalse(AHUACATL_IS_STRING([ 0 ]));
      assertFalse(AHUACATL_IS_STRING([ 0, 1 ]));
      assertFalse(AHUACATL_IS_STRING([ 1, 2 ]));
      assertFalse(AHUACATL_IS_STRING([ '' ]));
      assertFalse(AHUACATL_IS_STRING([ '0' ]));
      assertFalse(AHUACATL_IS_STRING([ '1' ]));
      assertFalse(AHUACATL_IS_STRING([ true ]));
      assertFalse(AHUACATL_IS_STRING([ false ]));
      assertFalse(AHUACATL_IS_STRING({ }));
      assertFalse(AHUACATL_IS_STRING({ 'a' : 0 }));
      assertFalse(AHUACATL_IS_STRING({ 'a' : 1 }));
      assertFalse(AHUACATL_IS_STRING({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_LIST function
////////////////////////////////////////////////////////////////////////////////

    testIsArrayTrue : function () {
      assertTrue(AHUACATL_IS_LIST([ ]));
      assertTrue(AHUACATL_IS_LIST([ 0 ]));
      assertTrue(AHUACATL_IS_LIST([ 0, 1 ]));
      assertTrue(AHUACATL_IS_LIST([ 1, 2 ]));
      assertTrue(AHUACATL_IS_LIST([ '' ]));
      assertTrue(AHUACATL_IS_LIST([ '0' ]));
      assertTrue(AHUACATL_IS_LIST([ '1' ]));
      assertTrue(AHUACATL_IS_LIST([ true ]));
      assertTrue(AHUACATL_IS_LIST([ false ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_LIST function
////////////////////////////////////////////////////////////////////////////////

    testIsArrayFalse : function () {
      assertFalse(AHUACATL_IS_LIST('abc'));
      assertFalse(AHUACATL_IS_LIST('null'));
      assertFalse(AHUACATL_IS_LIST('false'));
      assertFalse(AHUACATL_IS_LIST('undefined'));
      assertFalse(AHUACATL_IS_LIST(''));
      assertFalse(AHUACATL_IS_LIST(' '));
      assertFalse(AHUACATL_IS_LIST(false));
      assertFalse(AHUACATL_IS_LIST(true));
      assertFalse(AHUACATL_IS_LIST(undefined));
      assertFalse(AHUACATL_IS_LIST(NaN));
      assertFalse(AHUACATL_IS_LIST(1.3e308 * 1.3e308));
      assertFalse(AHUACATL_IS_LIST(0));
      assertFalse(AHUACATL_IS_LIST(1));
      assertFalse(AHUACATL_IS_LIST(-1));
      assertFalse(AHUACATL_IS_LIST(0.1));
      assertFalse(AHUACATL_IS_LIST(-0.1));
      assertFalse(AHUACATL_IS_LIST(null));
      assertFalse(AHUACATL_IS_LIST({ }));
      assertFalse(AHUACATL_IS_LIST({ 'a' : 0 }));
      assertFalse(AHUACATL_IS_LIST({ 'a' : 1 }));
      assertFalse(AHUACATL_IS_LIST({ 'a' : 0, 'b' : 1 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_DOCUMENT function
////////////////////////////////////////////////////////////////////////////////

    testIsObjectTrue : function () {
      assertTrue(AHUACATL_IS_DOCUMENT({ }));
      assertTrue(AHUACATL_IS_DOCUMENT({ 'a' : 0 }));
      assertTrue(AHUACATL_IS_DOCUMENT({ 'a' : 1 }));
      assertTrue(AHUACATL_IS_DOCUMENT({ 'a' : 0, 'b' : 1 }));
      assertTrue(AHUACATL_IS_DOCUMENT({ '1' : false, 'b' : false }));
      assertTrue(AHUACATL_IS_DOCUMENT({ '0' : false }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_IS_DOCUMENT function
////////////////////////////////////////////////////////////////////////////////

    testIsObjectFalse : function () {
      assertFalse(AHUACATL_IS_DOCUMENT('abc'));
      assertFalse(AHUACATL_IS_DOCUMENT('null'));
      assertFalse(AHUACATL_IS_DOCUMENT('false'));
      assertFalse(AHUACATL_IS_DOCUMENT('undefined'));
      assertFalse(AHUACATL_IS_DOCUMENT(''));
      assertFalse(AHUACATL_IS_DOCUMENT(' '));
      assertFalse(AHUACATL_IS_DOCUMENT(false));
      assertFalse(AHUACATL_IS_DOCUMENT(true));
      assertFalse(AHUACATL_IS_DOCUMENT(undefined));
      assertFalse(AHUACATL_IS_DOCUMENT(NaN));
      assertFalse(AHUACATL_IS_DOCUMENT(1.3e308 * 1.3e308));
      assertFalse(AHUACATL_IS_DOCUMENT(0));
      assertFalse(AHUACATL_IS_DOCUMENT(1));
      assertFalse(AHUACATL_IS_DOCUMENT(-1));
      assertFalse(AHUACATL_IS_DOCUMENT(0.1));
      assertFalse(AHUACATL_IS_DOCUMENT(-0.1));
      assertFalse(AHUACATL_IS_DOCUMENT(null));
      assertFalse(AHUACATL_IS_DOCUMENT([ ]));
      assertFalse(AHUACATL_IS_DOCUMENT([ 0 ]));
      assertFalse(AHUACATL_IS_DOCUMENT([ 0, 1 ]));
      assertFalse(AHUACATL_IS_DOCUMENT([ 1, 2 ]));
      assertFalse(AHUACATL_IS_DOCUMENT([ '' ]));
      assertFalse(AHUACATL_IS_DOCUMENT([ '0' ]));
      assertFalse(AHUACATL_IS_DOCUMENT([ '1' ]));
      assertFalse(AHUACATL_IS_DOCUMENT([ true ]));
      assertFalse(AHUACATL_IS_DOCUMENT([ false ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_CAST_BOOL function
////////////////////////////////////////////////////////////////////////////////

    testCastBoolTrue : function () {
      assertEqual(true, AHUACATL_CAST_BOOL(true));
      assertEqual(true, AHUACATL_CAST_BOOL(1));
      assertEqual(true, AHUACATL_CAST_BOOL(2));
      assertEqual(true, AHUACATL_CAST_BOOL(-1));
      assertEqual(true, AHUACATL_CAST_BOOL(100));
      assertEqual(true, AHUACATL_CAST_BOOL(100.01));
      assertEqual(true, AHUACATL_CAST_BOOL(0.001));
      assertEqual(true, AHUACATL_CAST_BOOL(-0.001));
      assertEqual(true, AHUACATL_CAST_BOOL(' '));
      assertEqual(true, AHUACATL_CAST_BOOL('  '));
      assertEqual(true, AHUACATL_CAST_BOOL('1'));
      assertEqual(true, AHUACATL_CAST_BOOL('1 '));
      assertEqual(true, AHUACATL_CAST_BOOL('0'));
      assertEqual(true, AHUACATL_CAST_BOOL('-1'));
      assertEqual(true, AHUACATL_CAST_BOOL([ 0 ]));
      assertEqual(true, AHUACATL_CAST_BOOL([ 0, 1 ]));
      assertEqual(true, AHUACATL_CAST_BOOL([ 1, 2 ]));
      assertEqual(true, AHUACATL_CAST_BOOL([ -1, 0 ]));
      assertEqual(true, AHUACATL_CAST_BOOL([ '' ]));
      assertEqual(true, AHUACATL_CAST_BOOL([ false ]));
      assertEqual(true, AHUACATL_CAST_BOOL([ true ]));
      assertEqual(true, AHUACATL_CAST_BOOL({ 'a' : true }));
      assertEqual(true, AHUACATL_CAST_BOOL({ 'a' : false }));
      assertEqual(true, AHUACATL_CAST_BOOL({ 'a' : false, 'b' : 0 }));
      assertEqual(true, AHUACATL_CAST_BOOL({ '0' : false }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_CAST_BOOL function
////////////////////////////////////////////////////////////////////////////////

    testCastBoolFalse : function () {
      assertEqual(false, AHUACATL_CAST_BOOL(0));
      assertEqual(false, AHUACATL_CAST_BOOL(NaN));
      assertEqual(false, AHUACATL_CAST_BOOL(''));
      assertEqual(false, AHUACATL_CAST_BOOL(undefined));
      assertEqual(false, AHUACATL_CAST_BOOL(null));
      assertEqual(false, AHUACATL_CAST_BOOL(false));
      assertEqual(false, AHUACATL_CAST_BOOL([ ]));
      assertEqual(false, AHUACATL_CAST_BOOL({ }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_CAST_NUMBER function
////////////////////////////////////////////////////////////////////////////////

    testCastNumber : function () {
      assertEqual(0, AHUACATL_CAST_NUMBER(undefined));
      assertEqual(0, AHUACATL_CAST_NUMBER(null));
      assertEqual(0, AHUACATL_CAST_NUMBER(false));
      assertEqual(1, AHUACATL_CAST_NUMBER(true));
      assertEqual(1, AHUACATL_CAST_NUMBER(1));
      assertEqual(2, AHUACATL_CAST_NUMBER(2));
      assertEqual(-1, AHUACATL_CAST_NUMBER(-1));
      assertEqual(0, AHUACATL_CAST_NUMBER(0));
      assertEqual(0, AHUACATL_CAST_NUMBER(NaN));
      assertEqual(0, AHUACATL_CAST_NUMBER(''));
      assertEqual(0, AHUACATL_CAST_NUMBER(' '));
      assertEqual(0, AHUACATL_CAST_NUMBER('  '));
      assertEqual(1, AHUACATL_CAST_NUMBER('1'));
      assertEqual(1, AHUACATL_CAST_NUMBER('1 '));
      assertEqual(0, AHUACATL_CAST_NUMBER('0'));
      assertEqual(-1, AHUACATL_CAST_NUMBER('-1'));
      assertEqual(-1, AHUACATL_CAST_NUMBER('-1 '));
      assertEqual(-1, AHUACATL_CAST_NUMBER(' -1 '));
      assertEqual(-1, AHUACATL_CAST_NUMBER(' -1a'));
      assertEqual(1, AHUACATL_CAST_NUMBER(' 1a'));
      assertEqual(12335.3, AHUACATL_CAST_NUMBER(' 12335.3 a'));
      assertEqual(0, AHUACATL_CAST_NUMBER('a1bc'));
      assertEqual(0, AHUACATL_CAST_NUMBER('aaaa1'));
      assertEqual(0, AHUACATL_CAST_NUMBER('-a1'));
      assertEqual(-1.255, AHUACATL_CAST_NUMBER('-1.255'));
      assertEqual(-1.23456, AHUACATL_CAST_NUMBER('-1.23456'));
      assertEqual(-1.23456, AHUACATL_CAST_NUMBER('-1.23456 '));
      assertEqual(1.23456, AHUACATL_CAST_NUMBER('  1.23456 '));
      assertEqual(1.23456, AHUACATL_CAST_NUMBER('   1.23456a'));
      assertEqual(0, AHUACATL_CAST_NUMBER('--1'));
      assertEqual(1, AHUACATL_CAST_NUMBER('+1'));
      assertEqual(12.42e32, AHUACATL_CAST_NUMBER('12.42e32'));
      assertEqual(0, AHUACATL_CAST_NUMBER([ ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ 0 ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ 0, 1 ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ 1, 2 ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ -1, 0 ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ 0, 1, [ 1, 2 ], [ [ 9, 4 ] ] ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ { } ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ 0, 1, { } ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ { }, { } ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ '' ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ false ]));
      assertEqual(0, AHUACATL_CAST_NUMBER([ true ]));
      assertEqual(0, AHUACATL_CAST_NUMBER({ }));
      assertEqual(0, AHUACATL_CAST_NUMBER({ 'a' : true }));
      assertEqual(0, AHUACATL_CAST_NUMBER({ 'a' : true, 'b' : 0 }));
      assertEqual(0, AHUACATL_CAST_NUMBER({ 'a' : { }, 'b' : { } }));
      assertEqual(0, AHUACATL_CAST_NUMBER({ 'a' : [ ], 'b' : [ ] }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_CAST_STRING function
////////////////////////////////////////////////////////////////////////////////

    testCastString : function () {
      assertEqual('null', AHUACATL_CAST_STRING(undefined));
      assertEqual('null', AHUACATL_CAST_STRING(null));
      assertEqual('false', AHUACATL_CAST_STRING(false));
      assertEqual('true', AHUACATL_CAST_STRING(true));
      assertEqual('1', AHUACATL_CAST_STRING(1));
      assertEqual('2', AHUACATL_CAST_STRING(2));
      assertEqual('-1', AHUACATL_CAST_STRING(-1));
      assertEqual('0', AHUACATL_CAST_STRING(0));
      assertEqual('null', AHUACATL_CAST_STRING(NaN));
      assertEqual('', AHUACATL_CAST_STRING(''));
      assertEqual(' ', AHUACATL_CAST_STRING(' '));
      assertEqual('  ', AHUACATL_CAST_STRING('  '));
      assertEqual('1', AHUACATL_CAST_STRING('1'));
      assertEqual('1 ', AHUACATL_CAST_STRING('1 '));
      assertEqual('0', AHUACATL_CAST_STRING('0'));
      assertEqual('-1', AHUACATL_CAST_STRING('-1'));
      assertEqual('', AHUACATL_CAST_STRING([ ]));
      assertEqual('0', AHUACATL_CAST_STRING([ 0 ]));
      assertEqual('0,1', AHUACATL_CAST_STRING([ 0, 1 ]));
      assertEqual('1,2', AHUACATL_CAST_STRING([ 1, 2 ]));
      assertEqual('-1,0', AHUACATL_CAST_STRING([ -1, 0 ]));
      assertEqual('0,1,1,2,9,4', AHUACATL_CAST_STRING([ 0, 1, [ 1, 2 ], [ [ 9, 4 ] ] ]));
      assertEqual('[object Object]', AHUACATL_CAST_STRING([ { } ]));
      assertEqual('0,1,[object Object]', AHUACATL_CAST_STRING([ 0, 1, { } ]));
      assertEqual('[object Object],[object Object]', AHUACATL_CAST_STRING([ { }, { } ]));
      assertEqual('', AHUACATL_CAST_STRING([ '' ]));
      assertEqual('false', AHUACATL_CAST_STRING([ false ]));
      assertEqual('true', AHUACATL_CAST_STRING([ true ]));
      assertEqual('[object Object]', AHUACATL_CAST_STRING({ }));
      assertEqual('[object Object]', AHUACATL_CAST_STRING({ 'a' : true }));
      assertEqual('[object Object]', AHUACATL_CAST_STRING({ 'a' : true, 'b' : 0 }));
      assertEqual('[object Object]', AHUACATL_CAST_STRING({ 'a' : { }, 'b' : { } }));
      assertEqual('[object Object]', AHUACATL_CAST_STRING({ 'a' : [ ], 'b' : [ ] }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_LOGICAL_AND function
////////////////////////////////////////////////////////////////////////////////

    testLogicalAndUndefined : function () {
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, null); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, false); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, 0.0); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, 1.0); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, -1.0); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, ''); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, '0'); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, '1'); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, [ ]); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, [ 0 ]); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, [ 0, 1 ]); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, [ 1, 2 ]); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, { }); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, { 'a' : 0 }); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, { 'a' : 1 }); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, { '0' : false }); });
      assertException(function() { AHUACATL_LOGICAL_AND(null, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND(0.0, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND(1.0, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND(-1.0, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND('', undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND('0', undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND('1', undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND([ ], undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND([ 0 ], undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND([ 0, 1 ], undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND([ 1, 2 ], undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND({ }, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND({ 'a' : 0 }, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND({ 'a' : 1 }, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND({ '0' : false }, undefined); });
      
      assertException(function() { AHUACATL_LOGICAL_AND(true, null); });
      assertException(function() { AHUACATL_LOGICAL_AND(null, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, ''); });
      assertException(function() { AHUACATL_LOGICAL_AND('', true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, ' '); });
      assertException(function() { AHUACATL_LOGICAL_AND(' ', true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, '0'); });
      assertException(function() { AHUACATL_LOGICAL_AND('0', true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, '1'); });
      assertException(function() { AHUACATL_LOGICAL_AND('1', true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, 'true'); });
      assertException(function() { AHUACATL_LOGICAL_AND('true', true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, 'false'); });
      assertException(function() { AHUACATL_LOGICAL_AND('false', true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, 0); });
      assertException(function() { AHUACATL_LOGICAL_AND(0, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, 1); });
      assertException(function() { AHUACATL_LOGICAL_AND(1, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, -1); });
      assertException(function() { AHUACATL_LOGICAL_AND(-1, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, 1.1); });
      assertException(function() { AHUACATL_LOGICAL_AND(1.1, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, [ ]); });
      assertException(function() { AHUACATL_LOGICAL_AND([ ], true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, [ 0 ]); });
      assertException(function() { AHUACATL_LOGICAL_AND([ 0 ], true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, [ 0, 1 ]); });
      assertException(function() { AHUACATL_LOGICAL_AND([ 0, 1 ], true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, [ true ]); });
      assertException(function() { AHUACATL_LOGICAL_AND([ true ], true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, [ false ]); });
      assertException(function() { AHUACATL_LOGICAL_AND([ false ], true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, { }); });
      assertException(function() { AHUACATL_LOGICAL_AND({ }, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, { 'a' : true }); });
      assertException(function() { AHUACATL_LOGICAL_AND({ 'a' : true }, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, { 'a' : true, 'b' : false }); });
      assertException(function() { AHUACATL_LOGICAL_AND({ 'a' : true, 'b' : false }, true); });
      
      assertException(function() { AHUACATL_LOGICAL_AND(false, null); });
      assertException(function() { AHUACATL_LOGICAL_AND(null, false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, ''); });
      assertException(function() { AHUACATL_LOGICAL_AND('', false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, ' '); });
      assertException(function() { AHUACATL_LOGICAL_AND(' ', false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, '0'); });
      assertException(function() { AHUACATL_LOGICAL_AND('0', false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, '1'); });
      assertException(function() { AHUACATL_LOGICAL_AND('1', false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, 'true'); });
      assertException(function() { AHUACATL_LOGICAL_AND('true', false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, 'false'); });
      assertException(function() { AHUACATL_LOGICAL_AND('false', false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, 0); });
      assertException(function() { AHUACATL_LOGICAL_AND(0, false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, 1); });
      assertException(function() { AHUACATL_LOGICAL_AND(1, false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, -1); });
      assertException(function() { AHUACATL_LOGICAL_AND(-1, false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, 1.1); });
      assertException(function() { AHUACATL_LOGICAL_AND(1.1, false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, [ ]); });
      assertException(function() { AHUACATL_LOGICAL_AND([ ], false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, [ 0 ]); });
      assertException(function() { AHUACATL_LOGICAL_AND([ 0 ], false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, [ 0, 1 ]); });
      assertException(function() { AHUACATL_LOGICAL_AND([ 0, 1 ], false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, [ true ]); });
      assertException(function() { AHUACATL_LOGICAL_AND([ false ], true); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, [ false ]); });
      assertException(function() { AHUACATL_LOGICAL_AND([ false ], false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, { }); });
      assertException(function() { AHUACATL_LOGICAL_AND({ }, false); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, { 'a' : true }); });
      assertException(function() { AHUACATL_LOGICAL_AND({ 'a' : false }, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, { 'a' : true, 'b' : false }); });
      assertException(function() { AHUACATL_LOGICAL_AND({ 'a' : false, 'b' : false }, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(NaN, NaN); });
      assertException(function() { AHUACATL_LOGICAL_AND(NaN, 0); });
      assertException(function() { AHUACATL_LOGICAL_AND(NaN, true); });
      assertException(function() { AHUACATL_LOGICAL_AND(NaN, false); });
      assertException(function() { AHUACATL_LOGICAL_AND(NaN, null); });
      assertException(function() { AHUACATL_LOGICAL_AND(NaN, undefined); });
      assertException(function() { AHUACATL_LOGICAL_AND(NaN, ''); });
      assertException(function() { AHUACATL_LOGICAL_AND(NaN, '0'); });
      assertException(function() { AHUACATL_LOGICAL_AND(0, NaN); });
      assertException(function() { AHUACATL_LOGICAL_AND(true, NaN); });
      assertException(function() { AHUACATL_LOGICAL_AND(false, NaN); });
      assertException(function() { AHUACATL_LOGICAL_AND(null, NaN); });
      assertException(function() { AHUACATL_LOGICAL_AND(undefined, NaN); });
      assertException(function() { AHUACATL_LOGICAL_AND('', NaN); });
      assertException(function() { AHUACATL_LOGICAL_AND('0', NaN); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_LOGICAL_AND function
////////////////////////////////////////////////////////////////////////////////

    testLogicalAndBool : function () {
      assertTrue(AHUACATL_LOGICAL_AND(true, true));
      assertFalse(AHUACATL_LOGICAL_AND(true, false));
      assertFalse(AHUACATL_LOGICAL_AND(false, true));
      assertFalse(AHUACATL_LOGICAL_AND(false, false));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_LOGICAL_OR function
////////////////////////////////////////////////////////////////////////////////

    testLogicalOrUndefined : function () {
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, null); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, false); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, 0.0); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, 1.0); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, -1.0); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, ''); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, '0'); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, '1'); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, [ ]); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, [ 0 ]); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, [ 0, 1 ]); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, [ 1, 2 ]); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, { }); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, { 'a' : 0 }); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, { 'a' : 1 }); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, { '0' : false }); });
      assertException(function() { AHUACATL_LOGICAL_OR(null, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR(0.0, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR(1.0, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR(-1.0, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR('', undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR('0', undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR('1', undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR([ ], undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR([ 0 ], undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR([ 0, 1 ], undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR([ 1, 2 ], undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR({ }, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR({ 'a' : 0 }, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR({ 'a' : 1 }, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR({ '0' : false }, undefined); });
      
      assertException(function() { AHUACATL_LOGICAL_OR(true, null); });
      assertException(function() { AHUACATL_LOGICAL_OR(null, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, ''); });
      assertException(function() { AHUACATL_LOGICAL_OR('', true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, ' '); });
      assertException(function() { AHUACATL_LOGICAL_OR(' ', true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, '0'); });
      assertException(function() { AHUACATL_LOGICAL_OR('0', true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, '1'); });
      assertException(function() { AHUACATL_LOGICAL_OR('1', true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, 'true'); });
      assertException(function() { AHUACATL_LOGICAL_OR('true', true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, 'false'); });
      assertException(function() { AHUACATL_LOGICAL_OR('false', true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, 0); });
      assertException(function() { AHUACATL_LOGICAL_OR(0, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, 1); });
      assertException(function() { AHUACATL_LOGICAL_OR(1, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, -1); });
      assertException(function() { AHUACATL_LOGICAL_OR(-1, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, 1.1); });
      assertException(function() { AHUACATL_LOGICAL_OR(1.1, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, [ ]); });
      assertException(function() { AHUACATL_LOGICAL_OR([ ], true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, [ 0 ]); });
      assertException(function() { AHUACATL_LOGICAL_OR([ 0 ], true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, [ 0, 1 ]); });
      assertException(function() { AHUACATL_LOGICAL_OR([ 0, 1 ], true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, [ true ]); });
      assertException(function() { AHUACATL_LOGICAL_OR([ true ], true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, [ false ]); });
      assertException(function() { AHUACATL_LOGICAL_OR([ false ], true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, { }); });
      assertException(function() { AHUACATL_LOGICAL_OR({ }, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, { 'a' : true }); });
      assertException(function() { AHUACATL_LOGICAL_OR({ 'a' : true }, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, { 'a' : true, 'b' : false }); });
      assertException(function() { AHUACATL_LOGICAL_OR({ 'a' : true, 'b' : false }, true); });
      
      assertException(function() { AHUACATL_LOGICAL_OR(false, null); });
      assertException(function() { AHUACATL_LOGICAL_OR(null, false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, ''); });
      assertException(function() { AHUACATL_LOGICAL_OR('', false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, ' '); });
      assertException(function() { AHUACATL_LOGICAL_OR(' ', false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, '0'); });
      assertException(function() { AHUACATL_LOGICAL_OR('0', false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, '1'); });
      assertException(function() { AHUACATL_LOGICAL_OR('1', false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, 'true'); });
      assertException(function() { AHUACATL_LOGICAL_OR('true', false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, 'false'); });
      assertException(function() { AHUACATL_LOGICAL_OR('false', false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, 0); });
      assertException(function() { AHUACATL_LOGICAL_OR(0, false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, 1); });
      assertException(function() { AHUACATL_LOGICAL_OR(1, false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, -1); });
      assertException(function() { AHUACATL_LOGICAL_OR(-1, false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, 1.1); });
      assertException(function() { AHUACATL_LOGICAL_OR(1.1, false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, [ ]); });
      assertException(function() { AHUACATL_LOGICAL_OR([ ], false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, [ 0 ]); });
      assertException(function() { AHUACATL_LOGICAL_OR([ 0 ], false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, [ 0, 1 ]); });
      assertException(function() { AHUACATL_LOGICAL_OR([ 0, 1 ], false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, [ true ]); });
      assertException(function() { AHUACATL_LOGICAL_OR([ false ], true); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, [ false ]); });
      assertException(function() { AHUACATL_LOGICAL_OR([ false ], false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, { }); });
      assertException(function() { AHUACATL_LOGICAL_OR({ }, false); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, { 'a' : true }); });
      assertException(function() { AHUACATL_LOGICAL_OR({ 'a' : false }, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, { 'a' : true, 'b' : false }); });
      assertException(function() { AHUACATL_LOGICAL_OR({ 'a' : false, 'b' : false }, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(NaN, NaN); });
      assertException(function() { AHUACATL_LOGICAL_OR(NaN, 0); });
      assertException(function() { AHUACATL_LOGICAL_OR(NaN, true); });
      assertException(function() { AHUACATL_LOGICAL_OR(NaN, false); });
      assertException(function() { AHUACATL_LOGICAL_OR(NaN, null); });
      assertException(function() { AHUACATL_LOGICAL_OR(NaN, undefined); });
      assertException(function() { AHUACATL_LOGICAL_OR(NaN, ''); });
      assertException(function() { AHUACATL_LOGICAL_OR(NaN, '0'); });
      assertException(function() { AHUACATL_LOGICAL_OR(0, NaN); });
      assertException(function() { AHUACATL_LOGICAL_OR(true, NaN); });
      assertException(function() { AHUACATL_LOGICAL_OR(false, NaN); });
      assertException(function() { AHUACATL_LOGICAL_OR(null, NaN); });
      assertException(function() { AHUACATL_LOGICAL_OR(undefined, NaN); });
      assertException(function() { AHUACATL_LOGICAL_OR('', NaN); });
      assertException(function() { AHUACATL_LOGICAL_OR('0', NaN); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_LOGICAL_OR function
////////////////////////////////////////////////////////////////////////////////

    testLogicalOrBool : function () {
      assertTrue(AHUACATL_LOGICAL_OR(true, true));
      assertTrue(AHUACATL_LOGICAL_OR(true, false));
      assertTrue(AHUACATL_LOGICAL_OR(false, true));
      assertFalse(AHUACATL_LOGICAL_OR(false, false));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_LOGICAL_NOT function
////////////////////////////////////////////////////////////////////////////////

    testLogicalNotUndefined : function () {
      assertException(function() { AHUACATL_LOGICAL_NOT(undefined); });
      assertException(function() { AHUACATL_LOGICAL_NOT(null); });
      assertException(function() { AHUACATL_LOGICAL_NOT(0.0); });
      assertException(function() { AHUACATL_LOGICAL_NOT(1.0); });
      assertException(function() { AHUACATL_LOGICAL_NOT(-1.0); });
      assertException(function() { AHUACATL_LOGICAL_NOT(''); });
      assertException(function() { AHUACATL_LOGICAL_NOT('0'); });
      assertException(function() { AHUACATL_LOGICAL_NOT('1'); });
      assertException(function() { AHUACATL_LOGICAL_NOT([ ]); });
      assertException(function() { AHUACATL_LOGICAL_NOT([ 0 ]); });
      assertException(function() { AHUACATL_LOGICAL_NOT([ 0, 1 ]); });
      assertException(function() { AHUACATL_LOGICAL_NOT([ 1, 2 ]); });
      assertException(function() { AHUACATL_LOGICAL_NOT({ }); });
      assertException(function() { AHUACATL_LOGICAL_NOT({ 'a' : 0 }); });
      assertException(function() { AHUACATL_LOGICAL_NOT({ 'a' : 1 }); });
      assertException(function() { AHUACATL_LOGICAL_NOT({ '0' : false}); });
      assertException(function() { AHUACATL_LOGICAL_NOT(NaN); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_LOGICAL_NOT function
////////////////////////////////////////////////////////////////////////////////

    testLogicalNotBool : function () {
      assertTrue(AHUACATL_LOGICAL_NOT(false));
      assertFalse(AHUACATL_LOGICAL_NOT(true));

      assertTrue(AHUACATL_LOGICAL_NOT(AHUACATL_LOGICAL_NOT(true)));
      assertFalse(AHUACATL_LOGICAL_NOT(AHUACATL_LOGICAL_NOT(false)));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_EQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalEqualTrue : function () {
      assertTrue(AHUACATL_RELATIONAL_EQUAL(undefined, undefined));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(undefined, null));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(null, undefined));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(null, null));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(NaN, NaN));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(NaN, null));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(NaN, undefined));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(undefined, NaN));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(null, NaN));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1, 1));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(0, 0));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(-1, -1));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1.345, 1.345));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1.0, 1.00));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1.0, 1.000));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1.1, 1.1));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1.01, 1.01));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1.001, 1.001));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1.0001, 1.0001));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1.00001, 1.00001));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1.000001, 1.000001));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(1.245e307, 1.245e307));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(-99.43423, -99.43423));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(true, true));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(false, false));
      assertTrue(AHUACATL_RELATIONAL_EQUAL('', ''));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(' ', ' '));
      assertTrue(AHUACATL_RELATIONAL_EQUAL(' 1', ' 1'));
      assertTrue(AHUACATL_RELATIONAL_EQUAL('0', '0'));
      assertTrue(AHUACATL_RELATIONAL_EQUAL('abc', 'abc'));
      assertTrue(AHUACATL_RELATIONAL_EQUAL('-1', '-1'));
      assertTrue(AHUACATL_RELATIONAL_EQUAL('true', 'true'));
      assertTrue(AHUACATL_RELATIONAL_EQUAL('false', 'false'));
      assertTrue(AHUACATL_RELATIONAL_EQUAL('undefined', 'undefined'));
      assertTrue(AHUACATL_RELATIONAL_EQUAL('null', 'null'));
      assertTrue(AHUACATL_RELATIONAL_EQUAL([ ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_EQUAL([ null ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_EQUAL([ ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_EQUAL([ 0 ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_EQUAL([ 0, 1 ], [ 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_EQUAL([ 0, 1, 4 ], [ 0, 1, 4 ]));
      assertTrue(AHUACATL_RELATIONAL_EQUAL([ 3, 4, -99 ], [ 3, 4, -99 ]));
      assertTrue(AHUACATL_RELATIONAL_EQUAL([ 'a', 4, [ 1, 'a' ], false ], [ 'a', 4, [ 1, 'a' ], false ]));
      assertTrue(AHUACATL_RELATIONAL_EQUAL({ }, { }));
      assertTrue(AHUACATL_RELATIONAL_EQUAL({ 'a' : true }, { 'a' : true }));
      assertTrue(AHUACATL_RELATIONAL_EQUAL({ 'a' : true, 'b': true }, { 'a' : true, 'b': true }));
      assertTrue(AHUACATL_RELATIONAL_EQUAL({ 'a' : true, 'b': true }, { 'b' : true, 'a': true }));
      assertTrue(AHUACATL_RELATIONAL_EQUAL({ 'b' : true, 'a': true }, { 'b' : true, 'a': true }));
      assertTrue(AHUACATL_RELATIONAL_EQUAL({ 'b' : true, 'a': true }, { 'a' : true, 'b': true }));
      assertTrue(AHUACATL_RELATIONAL_EQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }));
      assertTrue(AHUACATL_RELATIONAL_EQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'b' : [ 1, 9 ], 'a' : [ 0, 1 ] }));
      assertTrue(AHUACATL_RELATIONAL_EQUAL({ 'f' : { 'c' : { 'd' : [ 0, 1 ], 'a' : [ 1, 9 ] }, 'a' : false }, 'a' : true }, { 'a' : true, 'f' : { 'a' : false, 'c' : { 'a' : [ 1, 9 ], 'd' : [ 0, 1 ] } } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_EQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalEqualFalse : function () {
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, true));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, false));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, 0.0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, 1.0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, -1.0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, ''));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, '0'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, '1'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, [ ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, [ 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, [ 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, { }));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, { 'a' : 0 }));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, { 'a' : 1 }));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(undefined, { '0' : false }));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(true, undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(false, undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(0.0, undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.0, undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(-1.0, undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('', undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('0', undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('1', undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ ], undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 0 ], undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 0, 1 ], undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, 2 ], undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ ], null));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ }, undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ 'a' : 0 }, undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ 'a' : 1 }, undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ '0' : false }, undefined));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1, 0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(0, 1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(0, false));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(false, 0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(false, 0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(-1, 1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1, -1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(-1, 0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(0, -1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(true, false));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(false, true));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(true, 1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1, true));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(0, true));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(true, 0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(true, 'true'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(false, 'false'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('true', true));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('false', false));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(-1.345, 1.345));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.345, -1.345));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.345, 1.346));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.346, 1.345));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.344, 1.345));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.345, 1.344));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1, 2));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(2, 1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.246e307, 1.245e307));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.246e307, 1.247e307));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.246e307, 1.2467e307));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(-99.43423, -99.434233));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.00001, 1.000001));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1.00001, 1.0001));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(null, 1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1, null));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(null, 0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(0, null));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(null, ''));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('', null));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(null, '0'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('0', null));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(null, false));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(false, null));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(null, true));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(true, null));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(null, 'null'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('null', null));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(0, ''));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('', 0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1, ''));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('', 1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(' ', ''));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('', ' '));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(' 1', '1'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('1', ' 1'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('1 ', '1'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('1', '1 '));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('1 ', ' 1'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(' 1', '1 '));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(' 1 ', '1'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('0', ''));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('', ' '));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('abc', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('abcd', 'abc'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('dabc', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('1', 1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL(1, '1'));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('0', 0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('1.0', 1.0));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('1.0', 1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('-1', -1));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('1.234', 1.234));
      assertFalse(AHUACATL_RELATIONAL_EQUAL('NaN', NaN));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 0 ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ ], [ 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 0 ], [ 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, 1, 0 ], [ 1, 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, 0, 0 ], [ 1, 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, 0, 0 ], [ 1, 0 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, 0 ], [ 1, 0, 0 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, 0 ], [ 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, 0 ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, 0 ], [ 1 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, 2, 3 ], [ 3, 2, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ [ 1 ] ], [ [ 0 ] ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, [ 1 , 0 ] ], [ 1, [ 0, 1 ] ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ 1, [ 1 , 0, [ ] ] ], [ 1, [ [ ], 1, 0 ] ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ '' ], false));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ '' ], ''));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ '' ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ true ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ true ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ false ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ null ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_EQUAL([ ], ''));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ }, { 'a' : false }));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ 'a' : false }, { }));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ 'a' : true }, { 'a' : false }));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ 'a' : true }, { 'b' : true }));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ 'b' : true }, { 'a' : true }));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ 'a' : true, 'b' : [ 0, 1 ] }, { 'a' : true, 'b' : [ 1, 0 ] }));
      assertFalse(AHUACATL_RELATIONAL_EQUAL({ 'a' : true, 'b' : { 'a' : false, 'b' : true } }, { 'a' : true, 'b' : { 'a' : true, 'b': true } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_UNEQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalUnequalTrue : function () {
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, true));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, false));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, 0.0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, 1.0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, -1.0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, ''));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, '0'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, '1'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, [ ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, [ 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, [ 1, 2 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, { }));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, { 'a' : 0 }));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, { 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(undefined, { '0' : false }));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(true, undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(false, undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(0.0, undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.0, undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(-1.0, undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('', undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('0', undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('1', undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ ], undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 0 ], undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 0, 1 ], undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, 2 ], undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ }, undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : 0 }, undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : 1 }, undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ '0' : false }, undefined));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(NaN, false));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(NaN, true));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(NaN, ''));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(NaN, 0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(false, NaN));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(true, NaN));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('', NaN));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(0, NaN));

      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1, 0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(0, 1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(0, false));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(false, 0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(false, 0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(-1, 1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1, -1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(-1, 0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(0, -1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(true, false));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(false, true));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(true, 1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1, true));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(0, true));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(true, 0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(true, 'true'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(false, 'false'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('true', true));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('false', false));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(-1.345, 1.345));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.345, -1.345));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.345, 1.346));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.346, 1.345));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.344, 1.345));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.345, 1.344));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1, 2));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(2, 1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.246e307, 1.245e307));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.246e307, 1.247e307));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.246e307, 1.2467e307));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(-99.43423, -99.434233));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.00001, 1.000001));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1.00001, 1.0001));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(null, 1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1, null));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(null, 0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(0, null));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(null, ''));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('', null));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(null, '0'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('0', null));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(null, false));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(false, null));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(null, true));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(true, null));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(null, 'null'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('null', null));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(0, ''));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('', 0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1, ''));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('', 1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(' ', ''));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('', ' '));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(' 1', '1'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('1', ' 1'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('1 ', '1'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('1', '1 '));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('1 ', ' 1'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(' 1', '1 '));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(' 1 ', '1'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('0', ''));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('', ' '));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('abc', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('abcd', 'abc'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('dabc', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('1', 1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL(1, '1'));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('0', 0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('1.0', 1.0));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('1.0', 1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('-1', -1));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL('1.234', 1.234));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 0 ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ ], [ 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 0 ], [ 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, 1, 0 ], [ 1, 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, 0, 0 ], [ 1, 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, 0, 0 ], [ 1, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, 0 ], [ 1, 0, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, 0 ], [ 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, 0 ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, 0 ], [ 1 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, 2, 3 ], [ 3, 2, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ [ 1 ] ], [ [ 0 ] ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, [ 1 , 0 ] ], [ 1, [ 0, 1 ] ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ 1, [ 1 , 0, [ ] ] ], [ 1, [ [ ], 1, 0 ] ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ '' ], false));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ '' ], ''));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ '' ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ true ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ true ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ false ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ null ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ ], null));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL([ ], ''));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ }, { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : false }, { }));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : true }, { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : true }, { 'b' : true }));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ 'b' : true }, { 'a' : true }));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : true, 'b' : [ 0, 1 ] }, { 'a' : true, 'b' : [ 1, 0 ] }));
      assertTrue(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : true, 'b' : { 'a' : false, 'b' : true } }, { 'a' : true, 'b' : { 'a' : true, 'b': true } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_UNEQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalUnequalFalse : function () {
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1, 1));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(0, 0));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(-1, -1));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1.345, 1.345));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1.0, 1.00));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1.0, 1.000));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1.1, 1.1));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1.01, 1.01));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1.001, 1.001));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1.0001, 1.0001));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1.00001, 1.00001));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1.000001, 1.000001));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(1.245e307, 1.245e307));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(-99.43423, -99.43423));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(true, true));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(false, false));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL('', ''));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(' ', ' '));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(' 1', ' 1'));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL('0', '0'));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL('abc', 'abc'));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL('-1', '-1'));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(null, null));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(null, undefined));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(null, NaN));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(undefined, NaN));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(NaN, null));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(NaN, undefined));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL(undefined, undefined));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL('true', 'true'));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL('false', 'false'));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL('undefined', 'undefined'));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL('null', 'null'));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL([ ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL([ null ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL([ ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL([ 0 ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL([ 0, 1 ], [ 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL([ 0, 1, 4 ], [ 0, 1, 4 ]));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL([ 3, 4, -99 ], [ 3, 4, -99 ]));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL([ 'a', 4, [ 1, 'a' ], false ], [ 'a', 4, [ 1, 'a' ], false ]));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL({ }, { }));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : true }, { 'a' : true }));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : true, 'b': true }, { 'a' : true, 'b': true }));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : true, 'b': true }, { 'b' : true, 'a': true }));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL({ 'b' : true, 'a': true }, { 'b' : true, 'a': true }));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL({ 'b' : true, 'a': true }, { 'a' : true, 'b': true }));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'b' : [ 1, 9 ], 'a' : [ 0, 1 ] }));
      assertFalse(AHUACATL_RELATIONAL_UNEQUAL({ 'f' : { 'c' : { 'd' : [ 0, 1 ], 'a' : [ 1, 9 ] }, 'a' : false }, 'a' : true }, { 'a' : true, 'f' : { 'a' : false, 'c' : { 'a' : [ 1, 9 ], 'd' : [ 0, 1 ] } } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_LESS function
////////////////////////////////////////////////////////////////////////////////

    testRelationalLessTrue : function () {
      assertTrue(AHUACATL_RELATIONAL_LESS(NaN, false));
      assertTrue(AHUACATL_RELATIONAL_LESS(NaN, true));
      assertTrue(AHUACATL_RELATIONAL_LESS(NaN, ''));
      assertTrue(AHUACATL_RELATIONAL_LESS(NaN, 0));
      assertTrue(AHUACATL_RELATIONAL_LESS(NaN, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(NaN, { }));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, true));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, false));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, 0.0));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, 1.0));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, -1.0));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, ''));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, [ 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, [ 1, 2 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, { }));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, { 'a' : 0 }));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, { 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_LESS(undefined, { '0' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, false));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, true));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, 0));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, 1));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, -1));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, ''));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, ' '));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, 'null'));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, [ null ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, { }));
      assertTrue(AHUACATL_RELATIONAL_LESS(null, { 'a' : null }));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, true));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, 0));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, 1));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, -1));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, ''));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, ' '));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, 'true'));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, [ null ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, { }));
      assertTrue(AHUACATL_RELATIONAL_LESS(false, { 'a' : null }));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, 0));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, 1));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, -1));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, ''));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, ' '));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, 'true'));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, [ null ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, { }));
      assertTrue(AHUACATL_RELATIONAL_LESS(true, { 'a' : null }));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, 1));
      assertTrue(AHUACATL_RELATIONAL_LESS(1, 2));
      assertTrue(AHUACATL_RELATIONAL_LESS(1, 100));
      assertTrue(AHUACATL_RELATIONAL_LESS(20, 100));
      assertTrue(AHUACATL_RELATIONAL_LESS(-100, 1));
      assertTrue(AHUACATL_RELATIONAL_LESS(-100, -10));
      assertTrue(AHUACATL_RELATIONAL_LESS(-11, -10));
      assertTrue(AHUACATL_RELATIONAL_LESS(999, 1000));
      assertTrue(AHUACATL_RELATIONAL_LESS(-1, 1));
      assertTrue(AHUACATL_RELATIONAL_LESS(-1, 0));
      assertTrue(AHUACATL_RELATIONAL_LESS(1.0, 1.01));
      assertTrue(AHUACATL_RELATIONAL_LESS(1.111, 1.2));
      assertTrue(AHUACATL_RELATIONAL_LESS(-1.111, -1.110));
      assertTrue(AHUACATL_RELATIONAL_LESS(-1.111, -1.1109));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, ''));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, ' '));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, '-1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, 'true'));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, 'false'));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, 'null'));
      assertTrue(AHUACATL_RELATIONAL_LESS(1, ''));
      assertTrue(AHUACATL_RELATIONAL_LESS(1, ' '));
      assertTrue(AHUACATL_RELATIONAL_LESS(1, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESS(1, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(1, '-1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(1, 'true'));
      assertTrue(AHUACATL_RELATIONAL_LESS(1, 'false'));
      assertTrue(AHUACATL_RELATIONAL_LESS(1, 'null'));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, '-1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, '-100'));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, '-1.1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, '-0.0'));
      assertTrue(AHUACATL_RELATIONAL_LESS(1000, '-1'));
      assertTrue(AHUACATL_RELATIONAL_LESS(1000, '10'));
      assertTrue(AHUACATL_RELATIONAL_LESS(1000, '10000'));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(0, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(10, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, [ 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, [ 99 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, [ 100 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, [ 101 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, { }));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, { 'a' : 0 }));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, { 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, { 'a' : 99 }));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, { 'a' : 100 }));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, { 'a' : 101 }));
      assertTrue(AHUACATL_RELATIONAL_LESS(100, { 'a' : 1000 }));
      assertTrue(AHUACATL_RELATIONAL_LESS('', ' '));
      assertTrue(AHUACATL_RELATIONAL_LESS('0', 'a'));
      assertTrue(AHUACATL_RELATIONAL_LESS('a', 'a '));
      assertTrue(AHUACATL_RELATIONAL_LESS('a', 'b'));
      assertTrue(AHUACATL_RELATIONAL_LESS('A', 'a'));
      assertTrue(AHUACATL_RELATIONAL_LESS('AB', 'Ab'));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', 'bbcd'));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', 'abda'));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', 'abdd'));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', 'abcde'));
      assertTrue(AHUACATL_RELATIONAL_LESS('0abcd', 'abcde'));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', [ -1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', [ " " ]));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', [ "" ]));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', [ "abc" ]));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', [ "abcd" ]));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', { } ));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', { 'a' : true } ));
      assertTrue(AHUACATL_RELATIONAL_LESS('abcd', { 'abc' : true } ));
      assertTrue(AHUACATL_RELATIONAL_LESS('ABCD', { 'a' : true } ));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 0 ], [ 1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 0, 1, 4 ], [ 1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 15, 99 ], [ 110 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 15, 99 ], [ 15, 100 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ -1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ '' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ '0' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ 'abcd' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ [ null ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], [ { } ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ null ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ null ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ null ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ null ], [ [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false ], [ -1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false ], [ '' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false ], [ '0' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false ], [ 'abcd' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false ], [ [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false ], [ [ false ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ true ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ true ], [ -1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ true ], [ '' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ true ], [ '0' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ true ], [ 'abcd' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ true ], [ [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ true ], [ [ false ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false, false ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false, false ], [ false, true ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false, false ], [ false, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ null, null ], [ null, false ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], { 'a' : true }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], { 'a' : null }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ ], { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ '' ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 0 ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ null ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ false ], { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ true ], { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 'abcd' ], { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5 ], { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6 ], { 'a' : 2, 'b' : 2 }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, 7 ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, false ], [ 5, 6, true ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, 999 ], [ 5, 6, '' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, 'b' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, 'A' ], [ 5, 6, 'a' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, '' ], [ 5, 6, 'a' ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, 9, 9 ], [ 5, 6, [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, { } ]));
      assertTrue(AHUACATL_RELATIONAL_LESS([ 5, 6, 9, 9 ], [ 5, 6, { } ]));
      assertTrue(AHUACATL_RELATIONAL_LESS({ }, { 'a' : 0 }));
      assertTrue(AHUACATL_RELATIONAL_LESS({ 'a' : 1 }, { 'a' : 2 }));
      assertTrue(AHUACATL_RELATIONAL_LESS({ 'b' : 2 }, { 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_LESS({ 'z' : 1 }, { 'c' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_LESS({ 'a' : [ 9 ], 'b' : false }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(AHUACATL_RELATIONAL_LESS({ 'a' : [ 9 ], 'b' : true }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(AHUACATL_RELATIONAL_LESS({ 'a' : [ ], 'b' : true }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(AHUACATL_RELATIONAL_LESS({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 10, 1 ] }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_LESS function
////////////////////////////////////////////////////////////////////////////////

    testRelationalLessFalse : function () {
      assertFalse(AHUACATL_RELATIONAL_LESS(undefined, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS(undefined, null));
      assertFalse(AHUACATL_RELATIONAL_LESS(undefined, NaN));
      assertFalse(AHUACATL_RELATIONAL_LESS(null, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS(null, NaN));
      assertFalse(AHUACATL_RELATIONAL_LESS(NaN, null));
      assertFalse(AHUACATL_RELATIONAL_LESS(NaN, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS(true, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS(false, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS(0.0, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS(1.0, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS(-1.0, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS('', undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS('0', undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS('1', undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS([ ], undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS([ ], [ undefined ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0, 1 ], undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 1, 2 ], undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 0 }, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 1 }, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS({ '0' : false }, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESS(false, NaN));
      assertFalse(AHUACATL_RELATIONAL_LESS(true, NaN));
      assertFalse(AHUACATL_RELATIONAL_LESS('', NaN));
      assertFalse(AHUACATL_RELATIONAL_LESS(0, NaN));
      assertFalse(AHUACATL_RELATIONAL_LESS(null, null));
      assertFalse(AHUACATL_RELATIONAL_LESS(false, null));
      assertFalse(AHUACATL_RELATIONAL_LESS(true, null));
      assertFalse(AHUACATL_RELATIONAL_LESS(0, null));
      assertFalse(AHUACATL_RELATIONAL_LESS(1, null));
      assertFalse(AHUACATL_RELATIONAL_LESS(-1, null));
      assertFalse(AHUACATL_RELATIONAL_LESS('', null));
      assertFalse(AHUACATL_RELATIONAL_LESS(' ', null));
      assertFalse(AHUACATL_RELATIONAL_LESS('1', null));
      assertFalse(AHUACATL_RELATIONAL_LESS('0', null));
      assertFalse(AHUACATL_RELATIONAL_LESS('abcd', null));
      assertFalse(AHUACATL_RELATIONAL_LESS('null', null));
      assertFalse(AHUACATL_RELATIONAL_LESS([ ], null));
      assertFalse(AHUACATL_RELATIONAL_LESS([ true ], null));
      assertFalse(AHUACATL_RELATIONAL_LESS([ false ], null));
      assertFalse(AHUACATL_RELATIONAL_LESS([ null ], null));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], null));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, null));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : null }, null));
      assertFalse(AHUACATL_RELATIONAL_LESS(false, false));
      assertFalse(AHUACATL_RELATIONAL_LESS(true, true));
      assertFalse(AHUACATL_RELATIONAL_LESS(true, false));
      assertFalse(AHUACATL_RELATIONAL_LESS(0, false));
      assertFalse(AHUACATL_RELATIONAL_LESS(1, false));
      assertFalse(AHUACATL_RELATIONAL_LESS(-1, false));
      assertFalse(AHUACATL_RELATIONAL_LESS('', false));
      assertFalse(AHUACATL_RELATIONAL_LESS(' ', false));
      assertFalse(AHUACATL_RELATIONAL_LESS('1', false));
      assertFalse(AHUACATL_RELATIONAL_LESS('0', false));
      assertFalse(AHUACATL_RELATIONAL_LESS('abcd', false));
      assertFalse(AHUACATL_RELATIONAL_LESS('true', false));
      assertFalse(AHUACATL_RELATIONAL_LESS([ ], false));
      assertFalse(AHUACATL_RELATIONAL_LESS([ true ], false));
      assertFalse(AHUACATL_RELATIONAL_LESS([ false ], false));
      assertFalse(AHUACATL_RELATIONAL_LESS([ null ], false));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], false));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, false));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : null }, false));
      assertFalse(AHUACATL_RELATIONAL_LESS(0, true));
      assertFalse(AHUACATL_RELATIONAL_LESS(1, true));
      assertFalse(AHUACATL_RELATIONAL_LESS(-1, true));
      assertFalse(AHUACATL_RELATIONAL_LESS('', true));
      assertFalse(AHUACATL_RELATIONAL_LESS(' ', true));
      assertFalse(AHUACATL_RELATIONAL_LESS('1', true));
      assertFalse(AHUACATL_RELATIONAL_LESS('0', true));
      assertFalse(AHUACATL_RELATIONAL_LESS('abcd', true));
      assertFalse(AHUACATL_RELATIONAL_LESS('true', true));
      assertFalse(AHUACATL_RELATIONAL_LESS([ ], true));
      assertFalse(AHUACATL_RELATIONAL_LESS([ true ], true));
      assertFalse(AHUACATL_RELATIONAL_LESS([ false ], true));
      assertFalse(AHUACATL_RELATIONAL_LESS([ null ], true));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], true));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, true));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : null }, true));
      assertFalse(AHUACATL_RELATIONAL_LESS(0, 0));
      assertFalse(AHUACATL_RELATIONAL_LESS(1, 1));
      assertFalse(AHUACATL_RELATIONAL_LESS(-10, -10));
      assertFalse(AHUACATL_RELATIONAL_LESS(-100, -100));
      assertFalse(AHUACATL_RELATIONAL_LESS(-334.5, -334.5));
      assertFalse(AHUACATL_RELATIONAL_LESS(1, 0));
      assertFalse(AHUACATL_RELATIONAL_LESS(2, 1));
      assertFalse(AHUACATL_RELATIONAL_LESS(100, 1));
      assertFalse(AHUACATL_RELATIONAL_LESS(100, 20));
      assertFalse(AHUACATL_RELATIONAL_LESS(1, -100));
      assertFalse(AHUACATL_RELATIONAL_LESS(-10, -100));
      assertFalse(AHUACATL_RELATIONAL_LESS(-10, -11));
      assertFalse(AHUACATL_RELATIONAL_LESS(1000, 999));
      assertFalse(AHUACATL_RELATIONAL_LESS(1, -1));
      assertFalse(AHUACATL_RELATIONAL_LESS(0, -1));
      assertFalse(AHUACATL_RELATIONAL_LESS(1.01, 1.0));
      assertFalse(AHUACATL_RELATIONAL_LESS(1.2, 1.111));
      assertFalse(AHUACATL_RELATIONAL_LESS(-1.110, -1.111));
      assertFalse(AHUACATL_RELATIONAL_LESS(-1.1109, -1.111));
      assertFalse(AHUACATL_RELATIONAL_LESS('', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS(' ', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('0', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('1', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('-1', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('true', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('false', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('null', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('', 1));
      assertFalse(AHUACATL_RELATIONAL_LESS(' ', 1));
      assertFalse(AHUACATL_RELATIONAL_LESS('0', 1));
      assertFalse(AHUACATL_RELATIONAL_LESS('1', 1));
      assertFalse(AHUACATL_RELATIONAL_LESS('-1', 1));
      assertFalse(AHUACATL_RELATIONAL_LESS('true', 1));
      assertFalse(AHUACATL_RELATIONAL_LESS('false', 1));
      assertFalse(AHUACATL_RELATIONAL_LESS('null', 1));
      assertFalse(AHUACATL_RELATIONAL_LESS('-1', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('-100', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('-1.1', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('-0.0', 0));
      assertFalse(AHUACATL_RELATIONAL_LESS('-1', 1000));
      assertFalse(AHUACATL_RELATIONAL_LESS('10', 1000));
      assertFalse(AHUACATL_RELATIONAL_LESS('10000', 1000));
      assertFalse(AHUACATL_RELATIONAL_LESS([ ], 0));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], 0));
      assertFalse(AHUACATL_RELATIONAL_LESS([ ], 10));
      assertFalse(AHUACATL_RELATIONAL_LESS([ ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0, 1 ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 99 ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 100 ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 101 ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 0 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 1 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 99 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 100 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 101 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 1000 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : false }, 'zz'));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 'a' }, 'zz'));
      assertFalse(AHUACATL_RELATIONAL_LESS('', ''));
      assertFalse(AHUACATL_RELATIONAL_LESS(' ', ' '));
      assertFalse(AHUACATL_RELATIONAL_LESS('a', 'a'));
      assertFalse(AHUACATL_RELATIONAL_LESS(' a', ' a'));
      assertFalse(AHUACATL_RELATIONAL_LESS('abcd', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_LESS(' ', ''));
      assertFalse(AHUACATL_RELATIONAL_LESS('a', '0'));
      assertFalse(AHUACATL_RELATIONAL_LESS('a ', 'a'));
      assertFalse(AHUACATL_RELATIONAL_LESS('b', 'a'));
      assertFalse(AHUACATL_RELATIONAL_LESS('a', 'A'));
      assertFalse(AHUACATL_RELATIONAL_LESS('Ab', 'AB'));
      assertFalse(AHUACATL_RELATIONAL_LESS('bbcd', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_LESS('abda', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_LESS('abdd', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_LESS('abcde', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_LESS('abcde', '0abcde'));
      assertFalse(AHUACATL_RELATIONAL_LESS([ ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 1 ], [ 1 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ true ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ false ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ [ 0, 1, 2 ] ], [ [ 0, 1, 2 ] ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ [ 1, [ "true", 0, -99 , false ] ], 4 ], [ [ 1, [ "true", 0, -99, false ] ], 4 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 1 ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 1 ], [ 0, 1, 4 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 110 ], [ 15, 99 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 15, 100 ], [ 15, 99 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ undefined ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ null ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ false ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ true ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ -1 ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ '' ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ '0' ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 'abcd' ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ [ ] ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ [ null ] ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ { } ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ false ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ true ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ [ ] ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ true ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ -1 ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ '' ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ '0' ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 'abcd' ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ [ ] ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ [ false ] ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 0 ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ -1 ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ '' ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ '0' ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 'abcd' ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ [ ] ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ [ false] ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ true ], [ false, false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ false, true ], [ false, false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ false, 0 ], [ false, false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ null, false ], [ null, null ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : true }, [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : null }, [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : false }, [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, [ '' ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, [ null ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : false }, [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : false }, [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : false }, [ 'abcd' ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : false }, [ 5 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 2, 'b' : 2 }, [ 5, 6 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'a' : 1, 'b' : 2 }, { 'a' : 1, 'b' : 2, 'c' : null }));
      assertFalse(AHUACATL_RELATIONAL_LESS({ 'b' : 2, 'a' : 1 }, { 'a' : 1, 'b' : 2, 'c' : null }));
      assertFalse(AHUACATL_RELATIONAL_LESS({ }, [ 5, 6, 7 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, false ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 5, 6, 0 ], [ 5, 6, true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 5, 6, '' ], [ 5, 6, 999 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 5, 6, 'b' ], [ 5, 6, 'a' ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, 'A' ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, '' ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 5, 6, [ ] ], [ 5, 6, 9 ,9 ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 5, 6, [ ] ], [ 5, 6, true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 5, 6, { } ], [ 5, 6, true ]));
      assertFalse(AHUACATL_RELATIONAL_LESS([ 5, 6, { } ], [ 5, 6, 9, 9 ]));
    },
 
////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_GREATER function
////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterTrue : function () {
      assertTrue(AHUACATL_RELATIONAL_GREATER(true, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER(false, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER(0.0, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER(1.0, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER(-1.0, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER('', undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER('0', undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER('1', undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ ], undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0, 1 ], undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 1, 2 ], undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ }, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 0 }, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 1 }, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ '0' : false }, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATER(false, NaN));
      assertTrue(AHUACATL_RELATIONAL_GREATER(true, NaN));
      assertTrue(AHUACATL_RELATIONAL_GREATER('', NaN));
      assertTrue(AHUACATL_RELATIONAL_GREATER(0, NaN));

      assertTrue(AHUACATL_RELATIONAL_GREATER(false, null));
      assertTrue(AHUACATL_RELATIONAL_GREATER(true, null));
      assertTrue(AHUACATL_RELATIONAL_GREATER(0, null));
      assertTrue(AHUACATL_RELATIONAL_GREATER(1, null));
      assertTrue(AHUACATL_RELATIONAL_GREATER(-1, null));
      assertTrue(AHUACATL_RELATIONAL_GREATER('', null));
      assertTrue(AHUACATL_RELATIONAL_GREATER(' ', null));
      assertTrue(AHUACATL_RELATIONAL_GREATER('1', null));
      assertTrue(AHUACATL_RELATIONAL_GREATER('0', null));
      assertTrue(AHUACATL_RELATIONAL_GREATER('abcd', null));
      assertTrue(AHUACATL_RELATIONAL_GREATER('null', null));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ ], null));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ true ], null));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ false ], null));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ null ], null));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], null));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ }, null));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : null }, null));
      assertTrue(AHUACATL_RELATIONAL_GREATER(true, false));
      assertTrue(AHUACATL_RELATIONAL_GREATER(0, false));
      assertTrue(AHUACATL_RELATIONAL_GREATER(1, false));
      assertTrue(AHUACATL_RELATIONAL_GREATER(-1, false));
      assertTrue(AHUACATL_RELATIONAL_GREATER('', false));
      assertTrue(AHUACATL_RELATIONAL_GREATER(' ', false));
      assertTrue(AHUACATL_RELATIONAL_GREATER('1', false));
      assertTrue(AHUACATL_RELATIONAL_GREATER('0', false));
      assertTrue(AHUACATL_RELATIONAL_GREATER('abcd', false));
      assertTrue(AHUACATL_RELATIONAL_GREATER('true', false));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ ], false));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ true ], false));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ false ], false));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ null ], false));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], false));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ }, false));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : null }, false));
      assertTrue(AHUACATL_RELATIONAL_GREATER(0, true));
      assertTrue(AHUACATL_RELATIONAL_GREATER(1, true));
      assertTrue(AHUACATL_RELATIONAL_GREATER(-1, true));
      assertTrue(AHUACATL_RELATIONAL_GREATER('', true));
      assertTrue(AHUACATL_RELATIONAL_GREATER(' ', true));
      assertTrue(AHUACATL_RELATIONAL_GREATER('1', true));
      assertTrue(AHUACATL_RELATIONAL_GREATER('0', true));
      assertTrue(AHUACATL_RELATIONAL_GREATER('abcd', true));
      assertTrue(AHUACATL_RELATIONAL_GREATER('true', true));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ ], true));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ true ], true));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ false ], true));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ null ], true));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], true));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ }, true));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : null }, true));
      assertTrue(AHUACATL_RELATIONAL_GREATER(1, 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER(2, 1));
      assertTrue(AHUACATL_RELATIONAL_GREATER(100, 1));
      assertTrue(AHUACATL_RELATIONAL_GREATER(100, 20));
      assertTrue(AHUACATL_RELATIONAL_GREATER(1, -100));
      assertTrue(AHUACATL_RELATIONAL_GREATER(-10, -100));
      assertTrue(AHUACATL_RELATIONAL_GREATER(-10, -11));
      assertTrue(AHUACATL_RELATIONAL_GREATER(1000, 999));
      assertTrue(AHUACATL_RELATIONAL_GREATER(1, -1));
      assertTrue(AHUACATL_RELATIONAL_GREATER(0, -1));
      assertTrue(AHUACATL_RELATIONAL_GREATER(1.01, 1.0));
      assertTrue(AHUACATL_RELATIONAL_GREATER(1.2, 1.111));
      assertTrue(AHUACATL_RELATIONAL_GREATER(-1.110, -1.111));
      assertTrue(AHUACATL_RELATIONAL_GREATER(-1.1109, -1.111));
      assertTrue(AHUACATL_RELATIONAL_GREATER('', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER(' ', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('0', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('1', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('-1', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('true', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('false', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('null', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATER(' ', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATER('0', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATER('1', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATER('-1', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATER('true', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATER('false', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATER('null', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATER('-1', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('-100', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('-1.1', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('-0.0', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER('-1', 1000));
      assertTrue(AHUACATL_RELATIONAL_GREATER('10', 1000));
      assertTrue(AHUACATL_RELATIONAL_GREATER('10000', 1000));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ ], 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], 0));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ ], 10));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0, 1 ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 99 ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 100 ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 101 ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 0 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 1 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 99 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 100 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 101 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 1000 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : false }, 'zz'));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 'a' }, 'zz'));
      assertTrue(AHUACATL_RELATIONAL_GREATER(' ', ''));
      assertTrue(AHUACATL_RELATIONAL_GREATER('a', '0'));
      assertTrue(AHUACATL_RELATIONAL_GREATER('a ', 'a'));
      assertTrue(AHUACATL_RELATIONAL_GREATER('b', 'a'));
      assertTrue(AHUACATL_RELATIONAL_GREATER('a', 'A'));
      assertTrue(AHUACATL_RELATIONAL_GREATER('Ab', 'AB'));
      assertTrue(AHUACATL_RELATIONAL_GREATER('bbcd', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_GREATER('abda', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_GREATER('abdd', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_GREATER('abcde', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_GREATER('abcde', '0abcde'));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 1 ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 1 ], [ 0, 1, 4 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 110 ], [ 15, 99 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 15, 100 ], [ 15, 99 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ false ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ true ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ -1 ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ '' ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ '0' ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 'abcd' ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ [ ] ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ [ null ] ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ { } ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ false ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ true ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ [ ] ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ true ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ -1 ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ '' ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ '0' ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 'abcd' ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ [ ] ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ [ false ] ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 0 ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ -1 ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ '' ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ '0' ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ 'abcd' ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ [ ] ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ [ false] ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ true ], [ false, false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ false, true ], [ false, false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ false, 0 ], [ false, false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER([ null, false ], [ null, null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 0 }, { }));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 2 }, { 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'A' : 2 }, { 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'A' : 1 }, { 'a' : 2 }));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : 1 }, { 'b' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : false }));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : true }));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ ], 'b' : true }));
      assertTrue(AHUACATL_RELATIONAL_GREATER({ 'a' : [ 10, 1 ] }, { 'a' : [ 10 ], 'b' : true }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_GREATER function
////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterFalse : function () {
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, undefined));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, null));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, undefined));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, null));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, NaN));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, NaN));
      assertFalse(AHUACATL_RELATIONAL_GREATER(NaN, null));
      assertFalse(AHUACATL_RELATIONAL_GREATER(NaN, undefined));
      assertFalse(AHUACATL_RELATIONAL_GREATER(NaN, false));
      assertFalse(AHUACATL_RELATIONAL_GREATER(NaN, true));
      assertFalse(AHUACATL_RELATIONAL_GREATER(NaN, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATER(NaN, 0));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, true));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, false));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, 0.0));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, 1.0));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, -1.0));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, [ 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, [ 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, { }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, { 'a' : 0 }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, { 'a' : 1 }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(undefined, { '0' : false }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, false));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, true));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, 0));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, -1));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, 'null'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, [ null ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, { }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(null, { 'a' : null }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, false));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, true));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, true));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, 0));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, -1));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, 'true'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, [ null ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, { }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(false, { 'a' : null }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, 0));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, -1));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, 'true'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, [ null ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, { }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(true, { 'a' : null }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, 0));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATER(-10, -10));
      assertFalse(AHUACATL_RELATIONAL_GREATER(-100, -100));
      assertFalse(AHUACATL_RELATIONAL_GREATER(-334.5, -334.5));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, 2));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, 100));
      assertFalse(AHUACATL_RELATIONAL_GREATER(20, 100));
      assertFalse(AHUACATL_RELATIONAL_GREATER(-100, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATER(-100, -10));
      assertFalse(AHUACATL_RELATIONAL_GREATER(-11, -10));
      assertFalse(AHUACATL_RELATIONAL_GREATER(999, 1000));
      assertFalse(AHUACATL_RELATIONAL_GREATER(-1, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATER(-1, 0));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1.0, 1.01));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1.111, 1.2));
      assertFalse(AHUACATL_RELATIONAL_GREATER(-1.111, -1.110));
      assertFalse(AHUACATL_RELATIONAL_GREATER(-1.111, -1.1109));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, '-1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, 'true'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, 'false'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, 'null'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, '-1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, 'true'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, 'false'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1, 'null'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, '-1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, '-100'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, '-1.1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, '-0.0'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1000, '-1'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1000, '10'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(1000, '10000'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(0, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(10, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, [ 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, [ 99 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, [ 100 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, [ 101 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, { }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, { 'a' : 0 }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, { 'a' : 1 }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, { 'a' : 99 }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, { 'a' : 100 }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, { 'a' : 101 }));
      assertFalse(AHUACATL_RELATIONAL_GREATER(100, { 'a' : 1000 }));
      assertFalse(AHUACATL_RELATIONAL_GREATER('', ''));
      assertFalse(AHUACATL_RELATIONAL_GREATER(' ', ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATER('a', 'a'));
      assertFalse(AHUACATL_RELATIONAL_GREATER(' a', ' a'));
      assertFalse(AHUACATL_RELATIONAL_GREATER('abcd', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_GREATER('', ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATER('0', 'a'));
      assertFalse(AHUACATL_RELATIONAL_GREATER('a', 'a '));
      assertFalse(AHUACATL_RELATIONAL_GREATER('a', 'b'));
      assertFalse(AHUACATL_RELATIONAL_GREATER('A', 'a'));
      assertFalse(AHUACATL_RELATIONAL_GREATER('AB', 'Ab'));
      assertFalse(AHUACATL_RELATIONAL_GREATER('abcd', 'bbcd'));
      assertFalse(AHUACATL_RELATIONAL_GREATER('abcd', 'abda'));
      assertFalse(AHUACATL_RELATIONAL_GREATER('abcd', 'abdd'));
      assertFalse(AHUACATL_RELATIONAL_GREATER('abcd', 'abcde'));
      assertFalse(AHUACATL_RELATIONAL_GREATER('0abcd', 'abcde'));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ 0 ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ 1 ], [ 1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ true ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ [ 0, 1, 2 ] ], [ [ 0, 1, 2 ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ [ 1, [ "true", 0, -99 , false ] ], 4 ], [ [ 1, [ "true", 0, -99, false ] ], 4 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ 0 ], [ 1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ 0, 1, 4 ], [ 1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ 15, 99 ], [ 110 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ 15, 99 ], [ 15, 100 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ undefined ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ -1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ '' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ '0' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ 'abcd' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ [ null ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ ], [ { } ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ null ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ null ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ null ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ null ], [ [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ undefined ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ null ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false ], [ -1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false ], [ '' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false ], [ '0' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false ], [ 'abcd' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false ], [ [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false ], [ [ false ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ true ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ true ], [ -1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ true ], [ '' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ true ], [ '0' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ true ], [ 'abcd' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ true ], [ [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ true ], [ [ false ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false, false ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false, false ], [ false, true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ false, false ], [ false, 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER([ null, null ], [ null, false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATER({ 'a' : 1, 'b' : 2, 'c': null }, { 'b' : 2, 'a' : 1 }));
      assertFalse(AHUACATL_RELATIONAL_GREATER({ 'a' : 1, 'b' : 2, 'c' : null }, { 'a' : 1, 'b' : 2 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_LESSEQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalLessEqualTrue : function () {
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, undefined));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, null));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, undefined));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, true));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, false));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, 0.0));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, 1.0));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, -1.0));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, ''));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, [ 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, [ 1, 2 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, { 'a' : 0 }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, { 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, { '0' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(NaN, false));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(NaN, true));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(NaN, ''));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(NaN, 0));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(NaN, null));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(NaN, undefined));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, NaN));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(undefined, NaN));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, false));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, true));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, 0));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, 1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, -1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, ''));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, ' '));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, 'null'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, [ null ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, { 'a' : null }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(null, null));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, true));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, 0));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, 1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, -1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, ''));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, ' '));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, 'true'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, [ null ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, { 'a' : null }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(false, false));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, 0));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, 1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, -1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, ''));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, ' '));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, 'true'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, [ null ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, { 'a' : null }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(true, true));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, 1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, 2));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, 100));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(20, 100));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-100, 1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-100, -10));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-11, -10));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(999, 1000));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-1, 1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-1, 0));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1.0, 1.01));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1.111, 1.2));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-1.111, -1.110));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-1.111, -1.1109));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, 0));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, 1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(2, 2));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(20, 20));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, 100));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-100, -100));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-11, -11));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(999, 999));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-1, -1));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1.0, 1.0));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1.111, 1.111));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1.2, 1.2));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(-1.111, -1.111));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, ''));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, ' '));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, '-1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, 'true'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, 'false'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, 'null'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, ''));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, ' '));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, '0'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, '1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, '-1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, 'true'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, 'false'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1, 'null'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, '-1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, '-100'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, '-1.1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, '-0.0'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1000, '-1'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1000, '10'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(1000, '10000'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(0, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(10, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, [ 0, 1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, [ 99 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, [ 100 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, [ 101 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, { 'a' : 0 }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, { 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, { 'a' : 99 }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, { 'a' : 100 }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, { 'a' : 101 }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(100, { 'a' : 1000 }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('', ' '));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('0', 'a'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('a', 'a '));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('a', 'b'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('A', 'a'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('AB', 'Ab'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('abcd', 'bbcd'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('abcd', 'abda'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('abcd', 'abdd'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('abcd', 'abcde'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('0abcd', 'abcde'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(' abcd', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('', ''));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('0', '0'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('a', 'a'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('A', 'A'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('AB', 'AB'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('Ab', 'Ab'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('abcd', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('0abcd', '0abcd'));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL(' ', ' '));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL('  ', '  '));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], [ 1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 0, 1, 4 ], [ 1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 15, 99 ], [ 110 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 15, 99 ], [ 15, 100 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ undefined ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ -1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ '' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ '0' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ 'abcd' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ [ null ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ { } ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 2 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 15, 99 ], [ 15, 99 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ null ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ [ [ null, 1, 9 ], [ 12, "true", false ] ] , 0 ], [ [ [ null, 1, 9 ], [ 12, "true", false ] ] ,0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false, true ], [ false, true ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ null ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ null ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ null ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ null ], [ [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ -1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ '' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ '0' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ 'abcd' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ [ false ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ -1 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ '' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ '0' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ 'abcd' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ [ false ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false, false ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false, false ], [ false, true ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false, false ], [ false, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ null, null ], [ null, false ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], { 'a' : true }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], { 'a' : null }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ ], { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ '' ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ null ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ false ], { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ true ], { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 'abcd' ], { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5 ], { 'a' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6 ], { 'a' : 2, 'b' : 2 }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, 7 ], { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, false ], [ 5, 6, true ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, 999 ], [ 5, 6, '' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, 'a' ], [ 5, 6, 'b' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, 'A' ], [ 5, 6, 'a' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, '' ], [ 5, 6, 'a' ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, 9, 9 ], [ 5, 6, [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, { } ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ 5, 6, 9, 9 ], [ 5, 6, { } ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL({ }, { }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL({ 'A' : true }, { 'A' : true }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : false }, { 'a' : true, 'b' : false }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : false }, { 'b' : false, 'a' : true }));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : { 'c' : 1, 'f' : 2 }, 'x' : 9 }, { 'x' : 9, 'b' : { 'f' : 2, 'c' : 1 }, 'a' : true }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_LESSEQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalLessEqualFalse : function () {
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(true, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(false, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(0.0, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(1.0, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(-1.0, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('', undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('0', undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('1', undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ ], undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0, 1 ], undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 1, 2 ], undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ }, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : 0 }, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : 1 }, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ '0' : false }, undefined));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(false, NaN));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(true, NaN));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('', NaN));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(0, NaN));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(false, null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(true, null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(0, null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(1, null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(-1, null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('', null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(' ', null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('1', null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('0', null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('abcd', null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('null', null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ ], null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ true ], null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ false ], null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ null ], null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ }, null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : null }, null));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(true, false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(0, false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(1, false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(-1, false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('', false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(' ', false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('1', false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('0', false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('abcd', false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('true', false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ ], false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ true ], false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ false ], false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ null ], false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ }, false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : null }, false));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(0, true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(1, true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(-1, true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('', true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(' ', true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('1', true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('0', true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('abcd', true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('true', true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ ], true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ true ], true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ false ], true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ null ], true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ }, true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : null }, true));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(1, 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(2, 1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(100, 1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(100, 20));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(1, -100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(-10, -100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(-10, -11));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(1000, 999));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(1, -1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(0, -1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(1.01, 1.0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(1.2, 1.111));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(-1.110, -1.111));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(-1.1109, -1.111));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(' ', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('0', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('1', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('-1', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('true', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('false', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('null', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('', 1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(' ', 1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('0', 1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('1', 1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('-1', 1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('true', 1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('false', 1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('null', 1));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('-1', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('-100', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('-1.1', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('-0.0', 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('-1', 1000));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('10', 1000));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('10000', 1000));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ ], 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], 0));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ ], 10));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0, 1 ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 99 ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 100 ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 101 ], 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : 0 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : 1 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : 99 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : 100 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : 101 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : 1000 }, 100));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : false }, 'zz'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL({ 'a' : 'a' }, 'zz'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL(' ', ''));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('a', '0'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('a ', 'a'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('b', 'a'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('a', 'A'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('Ab', 'AB'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('bbcd', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('abda', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('abdd', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('abcde', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL('abcde', '0abcde'));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 1 ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 1 ], [ 0, 1, 4 ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 110 ], [ 15, 99 ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 15, 100 ], [ 15, 99 ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ -1 ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ '' ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ '0' ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 'abcd' ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ [ ] ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ [ null ] ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ { } ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ false ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ [ ] ], [ null ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ -1 ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ '' ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ '0' ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 'abcd' ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ [ ] ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ [ false ] ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 0 ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ -1 ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ '' ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ '0' ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ 'abcd' ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ [ ] ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ [ false] ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ true ], [ false, false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ false, true ], [ false, false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ false, 0 ], [ false, false ]));
      assertFalse(AHUACATL_RELATIONAL_LESSEQUAL([ null, false ], [ null, null ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_GREATEREQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterEqualTrue : function () {
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(null, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(NaN, null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(NaN, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(null, NaN));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, NaN));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(true, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(false, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(0.0, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1.0, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-1.0, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('', undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('0', undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('1', undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(false, NaN));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(true, NaN));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('', NaN));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(0, NaN));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ ], undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0, 1 ], undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 1, 2 ], undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 0 }, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 1 }, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ '0' : false }, undefined));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(false, null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(true, null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(0, null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1, null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-1, null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('', null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(' ', null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('1', null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('0', null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('abcd', null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('null', null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ ], null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ null ], null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : null }, null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(true, false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(0, false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1, false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-1, false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('', false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(' ', false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('1', false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('0', false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('abcd', false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('true', false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ ], false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ null ], false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : null }, false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(0, true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1, true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-1, true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('', true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(' ', true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('1', true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('0', true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('abcd', true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('true', true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ ], true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ null ], true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : null }, true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1, 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(2, 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(100, 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(100, 20));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1, -100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-10, -100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-10, -11));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1000, 999));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1, -1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(0, -1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1.01, 1.0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1.2, 1.111));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-1.110, -1.111));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-1.1109, -1.111));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(' ', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('0', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('1', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('-1', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('true', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('false', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('null', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(' ', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('0', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('1', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('-1', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('true', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('false', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('null', 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('-1', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('-100', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('-1.1', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('-0.0', 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('-1', 1000));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('10', 1000));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('10000', 1000));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ ], 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ ], 10));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0, 1 ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 99 ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 100 ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 101 ], 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 0 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 1 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 99 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 100 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 101 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 1000 }, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : false }, 'zz'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 'a' }, 'zz'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(' ', ''));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('a', '0'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('a ', 'a'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('b', 'a'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('a', 'A'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('Ab', 'AB'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('bbcd', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('abda', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('abdd', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('abcde', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('abcde', '0abcde'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 1 ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 1 ], [ 0, 1, 4 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 110 ], [ 15, 99 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 15, 100 ], [ 15, 99 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ undefined ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ null ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ -1 ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ '' ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ '0' ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 'abcd' ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ [ ] ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ [ null ] ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ { } ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ [ ] ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ -1 ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ '' ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ '0' ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 'abcd' ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ [ ] ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ [ false ] ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ -1 ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ '' ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ '0' ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 'abcd' ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ [ ] ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ [ false] ], [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ false, false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ false, true ], [ false, false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ false, 0 ], [ false, false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ null, false ], [ null, null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(null, null));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(false, false));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(true, true));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(0, 0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1, 1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(2, 2));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(20, 20));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(100, 100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-100, -100));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-11, -11));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(999, 999));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-1, -1));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1.0, 1.0));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1.111, 1.111));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(1.2, 1.2));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(-1.111, -1.111));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('', ''));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('0', '0'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('a', 'a'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('A', 'A'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('AB', 'AB'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('Ab', 'Ab'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('abcd', 'abcd'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('0abcd', '0abcd'));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL(' ', ' '));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL('  ', '  '));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 2 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 15, 99 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ null ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ [ [ null, 1, 9 ], [ 12, "true", false ] ] , 0 ], [ [ [ null, 1, 9 ], [ 12, "true", false ] ] ,0 ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ undefined ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_LESSEQUAL([ null ], [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ undefined ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ false, true ], [ false, true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : true }, [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : null }, [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : false }, [ ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, [ '' ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, [ null ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : false }, [ false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : false }, [ true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : false }, [ 'abcd' ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : false }, [ 5 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 2, 'b' : 2 }, [ 5, 6 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, [ 5, 6, 7 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 5, 6, true ], [ 5, 6, false ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 5, 6, 0 ], [ 5, 6, true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 5, 6, '' ], [ 5, 6, 999 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 5, 6, 'b' ], [ 5, 6, 'a' ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 5, 6, 'a' ], [ 5, 6, 'A' ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 5, 6, 'a' ], [ 5, 6, '' ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 5, 6, [ ] ], [ 5, 6, 9, 9 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 5, 6, [ ] ], [ 5, 6, true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 5, 6, { } ], [ 5, 6, true ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL([ 5, 6, { } ], [ 5, 6, 9, 9 ]));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 0 }, { }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 2 }, { 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'A' : 2 }, { 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'A' : 1 }, { 'a' : 2 }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 1, 'b' : 2, 'c' : null }, { 'a' : 1, 'b' : 2 }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 1 }, { 'b' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : 1, 'b' : 2, 'c': null }, { 'b' : 2, 'a' : 1 }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : false }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : true }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ ], 'b' : true }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : [ 10, 1 ] }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ }, { }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'A' : true }, { 'A' : true }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : false }, { 'a' : true, 'b' : false }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : false }, { 'b' : false, 'a' : true }));
      assertTrue(AHUACATL_RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : { 'c' : 1, 'f' : 2 }, 'x' : 9 }, { 'x' : 9, 'b' : { 'f' : 2, 'c' : 1 }, 'a' : true }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_GREATEREQUAL function
////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterEqualFalse : function () {
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, true));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, false));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, 0.0));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, 1.0));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, -1.0));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, [ 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, [ 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, { }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, { 'a' : 0 }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, { 'a' : 1 }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(undefined, { '0' : false }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(NaN, false));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(NaN, true));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(NaN, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(NaN, 0));

      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, false));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, true));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, 0));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, -1));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, 'null'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, [ null ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, { }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(null, { 'a' : null }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, true));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, 0));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, -1));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, 'true'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, [ null ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, { }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(false, { 'a' : null }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, 0));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, -1));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, 'true'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, [ null ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, { }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(true, { 'a' : null }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1, 2));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1, 100));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(20, 100));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(-100, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(-100, -10));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(-11, -10));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(999, 1000));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(-1, 1));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(-1, 0));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1.0, 1.01));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1.111, 1.2));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(-1.111, -1.110));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(-1.111, -1.1109));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, '-1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, 'true'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, 'false'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, 'null'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1, ''));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1, ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1, '0'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1, '1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1, '-1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1, 'true'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1, 'false'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1, 'null'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, '-1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, '-100'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, '-1.1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, '-0.0'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1000, '-1'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1000, '10'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(1000, '10000'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(0, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(10, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, [ ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, [ 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, [ 99 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, [ 100 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, [ 101 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, { }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, { 'a' : 0 }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, { 'a' : 1 }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, { 'a' : 99 }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, { 'a' : 100 }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, { 'a' : 101 }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(100, { 'a' : 1000 }));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('', ' '));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('0', 'a'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('a', 'a '));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('a', 'b'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('A', 'a'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('AB', 'Ab'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('abcd', 'bbcd'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('abcd', 'abda'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('abcd', 'abdd'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('abcd', 'abcde'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL('0abcd', 'abcde'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL(' abcd', 'abcd'));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ 0 ], [ 1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ 0, 1, 4 ], [ 1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 110 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 15, 100 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ -1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ '' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ '0' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ 'abcd' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ [ null ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ ], [ { } ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ null ], [ false ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ null ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ null ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ null ], [ [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ -1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ '' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ '0' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ 'abcd' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false ], [ [ false ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ -1 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ '' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ '0' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ 'abcd' ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ true ], [ [ false ] ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false, false ], [ true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false, false ], [ false, true ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ false, false ], [ false, 0 ]));
      assertFalse(AHUACATL_RELATIONAL_GREATEREQUAL([ null, null ], [ null, false ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_IN function
////////////////////////////////////////////////////////////////////////////////

    testRelationalInUndefined : function () {
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, null); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, true); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, false); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, 0.0); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, 1.0); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, -1.0); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, '0'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, { 'a' : 0 }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, { 'a' : 1 }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, { '0' : false }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0.0, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1.0, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN(-1.0, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN('0', undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN('1', undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0, 1 ], undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1, 2 ], undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ 'a' : 0 }, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ 'a' : 1 }, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ '0' : false }, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN(NaN, false); });
      assertException(function() { AHUACATL_RELATIONAL_IN(NaN, true); });
      assertException(function() { AHUACATL_RELATIONAL_IN(NaN, ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN(NaN, 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN(NaN, null); });
      assertException(function() { AHUACATL_RELATIONAL_IN(NaN, undefined); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, NaN); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, NaN); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', NaN); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, NaN); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, NaN); });
      assertException(function() { AHUACATL_RELATIONAL_IN(undefined, NaN); });
      
      assertException(function() { AHUACATL_RELATIONAL_IN(null, null); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, false); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, true); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(null, { 'A' : true }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, null); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, false); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, true); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(false, { 'A' : true }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, null); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, false); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, true); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(true, { 'A' : true }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, null); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, false); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, true); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(0, { 'A' : true }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1, null); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1, false); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1, true); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1, 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1, 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1, ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1, '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1, 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1, { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN(1, { 'A' : true }); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', null); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', false); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', true); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN('', { 'A' : true }); });
      assertException(function() { AHUACATL_RELATIONAL_IN('a', null); });
      assertException(function() { AHUACATL_RELATIONAL_IN('a', false); });
      assertException(function() { AHUACATL_RELATIONAL_IN('a', true); });
      assertException(function() { AHUACATL_RELATIONAL_IN('a', 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN('a', 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN('a', ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN('a', '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN('a', 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN('a', { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN('a', { 'A' : true }); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], null); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], false); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], true); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ ], { 'A' : true }); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], null); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], false); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], true); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 0 ], { 'A' : true }); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1 ], null); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1 ], false); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1 ], true); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1 ], 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1 ], 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1 ], ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1 ], '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1 ], 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1 ], { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN([ 1 ], { 'A' : true }); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, null); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, false); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, true); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, 0); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, 1); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, ''); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, '1'); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, 'a'); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, { }); });
      assertException(function() { AHUACATL_RELATIONAL_IN({ }, { 'A' : true }); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_IN function
////////////////////////////////////////////////////////////////////////////////

    testRelationalInTrue : function () {
      assertTrue(AHUACATL_RELATIONAL_IN(null, [ null ]));
      assertTrue(AHUACATL_RELATIONAL_IN(null, [ null, false ]));
      assertTrue(AHUACATL_RELATIONAL_IN(null, [ false, null ]));
      assertTrue(AHUACATL_RELATIONAL_IN(false, [ false ]));
      assertTrue(AHUACATL_RELATIONAL_IN(false, [ true, false ]));
      assertTrue(AHUACATL_RELATIONAL_IN(false, [ 0, false ]));
      assertTrue(AHUACATL_RELATIONAL_IN(true, [ true ]));
      assertTrue(AHUACATL_RELATIONAL_IN(true, [ false, true ]));
      assertTrue(AHUACATL_RELATIONAL_IN(true, [ 0, false, true ]));
      assertTrue(AHUACATL_RELATIONAL_IN(0, [ 0 ]));
      assertTrue(AHUACATL_RELATIONAL_IN(1, [ 1 ]));
      assertTrue(AHUACATL_RELATIONAL_IN(0, [ 3, 2, 1, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_IN(1, [ 3, 2, 1, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_IN(-35.5, [ 3, 2, 1, -35.5, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_IN(1.23e32, [ 3, 2, 1, 1.23e32, 35.5, 0 ]));
      assertTrue(AHUACATL_RELATIONAL_IN('', [ '' ]));
      assertTrue(AHUACATL_RELATIONAL_IN('', [ ' ', '' ]));
      assertTrue(AHUACATL_RELATIONAL_IN('', [ 'a', 'b', 'c', '' ]));
      assertTrue(AHUACATL_RELATIONAL_IN('A', [ 'c', 'b', 'a', 'A' ]));
      assertTrue(AHUACATL_RELATIONAL_IN(' ', [ ' ' ]));
      assertTrue(AHUACATL_RELATIONAL_IN(' a', [ ' a' ]));
      assertTrue(AHUACATL_RELATIONAL_IN(' a ', [ ' a ' ]));
      assertTrue(AHUACATL_RELATIONAL_IN([ ], [ [ ] ]));
      assertTrue(AHUACATL_RELATIONAL_IN([ ], [ 1, null, 2, 3, [ ], 5 ]));
      assertTrue(AHUACATL_RELATIONAL_IN([ null ], [ [ null ] ]));
      assertTrue(AHUACATL_RELATIONAL_IN([ null ], [ null, [ null ], true ]));
      assertTrue(AHUACATL_RELATIONAL_IN([ null, false ], [ [ null, false ] ]));
      assertTrue(AHUACATL_RELATIONAL_IN([ 'a', 'A', false ], [ 'a', true, [ 'a', 'A', false ] ]));
      assertTrue(AHUACATL_RELATIONAL_IN({ }, [ { } ]));
      assertTrue(AHUACATL_RELATIONAL_IN({ }, [ 'a', null, false, 0, { } ]));
      assertTrue(AHUACATL_RELATIONAL_IN({ 'a' : true }, [ 'a', null, false, 0, { 'a' : true } ]));
      assertTrue(AHUACATL_RELATIONAL_IN({ 'a' : true, 'A': false }, [ 'a', null, false, 0, { 'A' : false, 'a' : true } ]));
      assertTrue(AHUACATL_RELATIONAL_IN({ 'a' : { 'b' : null, 'c': 1 } }, [ { 'a' : { 'c' : 1, 'b' : null } } ]));
      assertTrue(AHUACATL_RELATIONAL_IN({ 'a' : { 'b' : null, 'c': 1 } }, [ 'a', 'b', { 'a' : { 'c' : 1, 'b' : null } } ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_RELATIONAL_IN function
////////////////////////////////////////////////////////////////////////////////

    testRelationalInFalse : function () {
      assertFalse(AHUACATL_RELATIONAL_IN(undefined, [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN(undefined, [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_IN(undefined, [ 0, 1 ]));
      assertFalse(AHUACATL_RELATIONAL_IN(undefined, [ 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_IN(null, [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN(false, [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN(true, [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN(0, [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN(1, [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN('', [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN('0', [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN('1', [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ 0 ], [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN({ }, [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN({ 'a' : true }, [ ]));
      assertFalse(AHUACATL_RELATIONAL_IN(null, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, true, false, { }, [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN(true, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, false, { }, [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN(false, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, { }, [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN(0, [ '0', '1', '', 'null', 'true', 'false', -1, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN(1, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN(-1, [ '0', '1', '', 'null', 'true', 'false', 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN('', [ '0', '1', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN('0', [ '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN('1', [ '0', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ ], [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { } ]));
      assertFalse(AHUACATL_RELATIONAL_IN({ }, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, [ ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN(null, [ [ null ]]));
      assertFalse(AHUACATL_RELATIONAL_IN(false, [ [ false ]]));
      assertFalse(AHUACATL_RELATIONAL_IN(true, [ [ true ]]));
      assertFalse(AHUACATL_RELATIONAL_IN(0, [ [ 0 ]]));
      assertFalse(AHUACATL_RELATIONAL_IN(1, [ [ 1 ]]));
      assertFalse(AHUACATL_RELATIONAL_IN('', [ [ '' ]]));
      assertFalse(AHUACATL_RELATIONAL_IN('1', [ [ '1' ]]));
      assertFalse(AHUACATL_RELATIONAL_IN('', [ [ '' ]]));
      assertFalse(AHUACATL_RELATIONAL_IN(' ', [ [ ' ' ]]));
      assertFalse(AHUACATL_RELATIONAL_IN('a', [ 'A' ]));
      assertFalse(AHUACATL_RELATIONAL_IN('a', [ 'b', 'c', 'd' ]));
      assertFalse(AHUACATL_RELATIONAL_IN('', [ ' ', '  ' ]));
      assertFalse(AHUACATL_RELATIONAL_IN(' ', [ '', '  ' ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ ], [ 0 ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ ], [ 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ 0 ], [ 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ 1 ], [ 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ 2 ], [ 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ 1, 2 ], [ 1, 2 ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ 1, 2 ], [ [ 1 ], [ 2 ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ 1, 2 ], [ [ 2, 1 ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ 1, 2 ], [ [ 1, 2, 3 ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ 1, 2, 3 ], [ [ 1, 2, 4 ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN([ 1, 2, 3 ], [ [ 0, 1, 2, 3 ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN({ 'a' : true }, [ { 'a' : true, 'b' : false } ]));
      assertFalse(AHUACATL_RELATIONAL_IN({ 'a' : true }, [ { 'a' : false } ]));
      assertFalse(AHUACATL_RELATIONAL_IN({ 'a' : true }, [ { 'b' : true } ]));
      assertFalse(AHUACATL_RELATIONAL_IN({ 'a' : true }, [ [ { 'a' : true } ] ]));
      assertFalse(AHUACATL_RELATIONAL_IN({ 'a' : true }, [ 1, 2, { 'a' : { 'a' : true } } ]));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_UNARY_PLUS function
////////////////////////////////////////////////////////////////////////////////

    testUnaryPlusUndefined : function () {
      assertException(function() { AHUACATL_UNARY_PLUS(undefined); });
      assertException(function() { AHUACATL_UNARY_PLUS(null); });
      assertException(function() { AHUACATL_UNARY_PLUS(NaN); });
      assertException(function() { AHUACATL_UNARY_PLUS(false); });
      assertException(function() { AHUACATL_UNARY_PLUS(true); });
      assertException(function() { AHUACATL_UNARY_PLUS(' '); });
      assertException(function() { AHUACATL_UNARY_PLUS('abc'); });
      assertException(function() { AHUACATL_UNARY_PLUS('1abc'); });
      assertException(function() { AHUACATL_UNARY_PLUS(''); });
      assertException(function() { AHUACATL_UNARY_PLUS('-1'); });
      assertException(function() { AHUACATL_UNARY_PLUS('0'); });
      assertException(function() { AHUACATL_UNARY_PLUS('1'); });
      assertException(function() { AHUACATL_UNARY_PLUS('1.5'); });
      assertException(function() { AHUACATL_UNARY_PLUS([ ]); });
      assertException(function() { AHUACATL_UNARY_PLUS([ 0 ]); });
      assertException(function() { AHUACATL_UNARY_PLUS([ 1 ]); });
      assertException(function() { AHUACATL_UNARY_PLUS({ 'a' : 1 }); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_UNARY_PLUS function
////////////////////////////////////////////////////////////////////////////////

    testUnaryPlusValue : function () {
      assertEqual(0, AHUACATL_UNARY_PLUS(0));
      assertEqual(1, AHUACATL_UNARY_PLUS(1));
      assertEqual(-1, AHUACATL_UNARY_PLUS(-1));
      assertEqual(0.0001, AHUACATL_UNARY_PLUS(0.0001));
      assertEqual(-0.0001, AHUACATL_UNARY_PLUS(-0.0001));
      assertEqual(100, AHUACATL_UNARY_PLUS(100));
      assertEqual(1054.342, AHUACATL_UNARY_PLUS(1054.342));
      assertEqual(-1054.342, AHUACATL_UNARY_PLUS(-1054.342));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_UNARY_MINUS function
////////////////////////////////////////////////////////////////////////////////

    testUnaryMinusUndefined : function () {
      assertException(function() { AHUACATL_UNARY_MINUS(undefined); });
      assertException(function() { AHUACATL_UNARY_MINUS(null); });
      assertException(function() { AHUACATL_UNARY_MINUS(false); });
      assertException(function() { AHUACATL_UNARY_MINUS(true); });
      assertException(function() { AHUACATL_UNARY_MINUS(' '); });
      assertException(function() { AHUACATL_UNARY_MINUS('abc'); });
      assertException(function() { AHUACATL_UNARY_MINUS('1abc'); });
      assertException(function() { AHUACATL_UNARY_MINUS(''); });
      assertException(function() { AHUACATL_UNARY_MINUS('-1'); });
      assertException(function() { AHUACATL_UNARY_MINUS('0'); });
      assertException(function() { AHUACATL_UNARY_MINUS('1'); });
      assertException(function() { AHUACATL_UNARY_MINUS('1.5'); });
      assertException(function() { AHUACATL_UNARY_MINUS([ ]); });
      assertException(function() { AHUACATL_UNARY_MINUS([ 0 ]); });
      assertException(function() { AHUACATL_UNARY_MINUS([ 1 ]); });
      assertException(function() { AHUACATL_UNARY_MINUS({ 'a' : 1 }); });
      assertException(function() { AHUACATL_UNARY_PLUS(NaN); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_UNARY_MINUS function
////////////////////////////////////////////////////////////////////////////////
  
    testUnaryMinusValue : function () {
      assertEqual(0, AHUACATL_UNARY_MINUS(0));
      assertEqual(1, AHUACATL_UNARY_MINUS(-1));
      assertEqual(-1, AHUACATL_UNARY_MINUS(1));
      assertEqual(0.0001, AHUACATL_UNARY_MINUS(-0.0001));
      assertEqual(-0.0001, AHUACATL_UNARY_MINUS(0.0001));
      assertEqual(100, AHUACATL_UNARY_MINUS(-100));
      assertEqual(-100, AHUACATL_UNARY_MINUS(100));
      assertEqual(1054.342, AHUACATL_UNARY_MINUS(-1054.342));
      assertEqual(-1054.342, AHUACATL_UNARY_MINUS(1054.342));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_ARITHMETIC_PLUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPlusUndefined : function () {
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, null); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, false); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, true); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, 2); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, -1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, ''); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, ' '); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, 'a'); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, [ ]); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, [ 1 ]); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, { }); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, { 'a' : 0 }); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(undefined, NaN); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(null, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(false, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(true, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(0, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(2, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(-1, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS('', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS('0', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(' ', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS('a', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS([ ], undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS([ 1 ], undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS({ }, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS({ 'a' : 0 }, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(NaN, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, NaN); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, null); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, false); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, true); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, ''); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, ' '); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, '1'); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, 'a'); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, [ ]); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, [ 0 ]); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, { }); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1, { 'a' : 0 }); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(NaN, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(null, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(false, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(true, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS('', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(' ', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS('0', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS('1', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS('a', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS([ ], 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS([ 0 ], 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS({ }, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS({ 'a' : 0 }, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS('0', '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_PLUS(1.3e317, 1.3e317); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_ARITHMETIC_PLUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticPlusValue : function () {
      assertEqual(0, AHUACATL_ARITHMETIC_PLUS(0, 0));
      assertEqual(0, AHUACATL_ARITHMETIC_PLUS(1, -1));
      assertEqual(0, AHUACATL_ARITHMETIC_PLUS(-1, 1));
      assertEqual(0, AHUACATL_ARITHMETIC_PLUS(-100, 100));
      assertEqual(0, AHUACATL_ARITHMETIC_PLUS(100, -100));
      assertEqual(0, AHUACATL_ARITHMETIC_PLUS(0.11, -0.11));
      assertEqual(10, AHUACATL_ARITHMETIC_PLUS(5, 5));
      assertEqual(10, AHUACATL_ARITHMETIC_PLUS(4, 6));
      assertEqual(10, AHUACATL_ARITHMETIC_PLUS(1, 9));
      assertEqual(10, AHUACATL_ARITHMETIC_PLUS(0.1, 9.9));
      assertEqual(9.8, AHUACATL_ARITHMETIC_PLUS(-0.1, 9.9));
      assertEqual(-34.2, AHUACATL_ARITHMETIC_PLUS(-17.1, -17.1));
      assertEqual(-2, AHUACATL_ARITHMETIC_PLUS(-1, -1)); 
      assertEqual(2.6e307, AHUACATL_ARITHMETIC_PLUS(1.3e307, 1.3e307));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_ARITHMETIC_MINUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticMinusUndefined : function () {
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, null); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, false); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, true); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, 2); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, -1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, ''); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, ' '); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, 'a'); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, [ ]); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, [ 1 ]); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, { }); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, { 'a' : 0 }); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(undefined, NaN); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(null, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(false, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(true, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(0, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(2, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(-1, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS('', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS('0', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(' ', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS('a', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS([ ], undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS([ 1 ], undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS({ }, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS({ 'a' : 0 }, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(NaN, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, NaN); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, null); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, false); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, true); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, ''); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, ' '); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, '1'); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, 'a'); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, [ ]); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, [ 0 ]); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, { }); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(1, { 'a' : 0 }); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(NaN, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(null, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(false, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(true, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS('', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(' ', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS('0', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS('1', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS('a', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS([ ], 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS([ 0 ], 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS({ }, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS({ 'a' : 0 }, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS('0', '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_MINUS(-1.3e317, 1.3e317); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_ARITHMETIC_MINUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticMinusValue : function () {
      assertEqual(0, AHUACATL_ARITHMETIC_MINUS(0, 0));
      assertEqual(-1, AHUACATL_ARITHMETIC_MINUS(0, 1));
      assertEqual(0, AHUACATL_ARITHMETIC_MINUS(-1, -1)); 
      assertEqual(0, AHUACATL_ARITHMETIC_MINUS(1, 1));
      assertEqual(2, AHUACATL_ARITHMETIC_MINUS(1, -1));
      assertEqual(-2, AHUACATL_ARITHMETIC_MINUS(-1, 1));
      assertEqual(-200, AHUACATL_ARITHMETIC_MINUS(-100, 100));
      assertEqual(200, AHUACATL_ARITHMETIC_MINUS(100, -100));
      assertEqual(0.22, AHUACATL_ARITHMETIC_MINUS(0.11, -0.11));
      assertEqual(0, AHUACATL_ARITHMETIC_MINUS(5, 5));
      assertEqual(-2, AHUACATL_ARITHMETIC_MINUS(4, 6));
      assertEqual(-8, AHUACATL_ARITHMETIC_MINUS(1, 9));
      assertEqual(-9.8, AHUACATL_ARITHMETIC_MINUS(0.1, 9.9));
      assertEqual(-10, AHUACATL_ARITHMETIC_MINUS(-0.1, 9.9));
      assertEqual(0, AHUACATL_ARITHMETIC_MINUS(-17.1, -17.1));
      assertEqual(0, AHUACATL_ARITHMETIC_MINUS(1.3e307, 1.3e307));
      assertEqual(2.6e307, AHUACATL_ARITHMETIC_MINUS(1.3e307, -1.3e307));
      assertEqual(-2.6e307, AHUACATL_ARITHMETIC_MINUS(-1.3e307, 1.3e307));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_ARITHMETIC_TIMES function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticTimesUndefined : function () {
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, null); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, false); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, true); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, 2); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, -1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, ''); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, ' '); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, 'a'); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, [ ]); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, [ 1 ]); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, { }); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, { 'a' : 0 }); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(undefined, NaN); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(null, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(false, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(true, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(0, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(2, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(-1, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES('', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES('0', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(' ', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES('a', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES([ ], undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES([ 1 ], undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES({ }, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES({ 'a' : 0 }, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(NaN, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, NaN); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, null); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, false); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, true); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, ''); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, ' '); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, '1'); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, 'a'); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, [ ]); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, [ 0 ]); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, { }); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1, { 'a' : 0 }); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(NaN, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(null, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(false, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(true, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES('', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(' ', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES('0', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES('1', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES('a', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES([ ], 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES([ 0 ], 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES({ }, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES({ 'a' : 0 }, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1.3e190, 1.3e190); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1.3e307, 1.3e307); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES(1.3e317, 1.3e317); });
      assertException(function() { AHUACATL_ARITHMETIC_TIMES('0', '0'); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_ARITHMETIC_TIMES function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticTimesValue : function () {
      assertEqual(0, AHUACATL_ARITHMETIC_TIMES(0, 0));
      assertEqual(0, AHUACATL_ARITHMETIC_TIMES(1, 0));
      assertEqual(0, AHUACATL_ARITHMETIC_TIMES(0, 1));
      assertEqual(1, AHUACATL_ARITHMETIC_TIMES(1, 1));
      assertEqual(2, AHUACATL_ARITHMETIC_TIMES(2, 1));
      assertEqual(2, AHUACATL_ARITHMETIC_TIMES(1, 2));
      assertEqual(4, AHUACATL_ARITHMETIC_TIMES(2, 2));
      assertEqual(100, AHUACATL_ARITHMETIC_TIMES(10, 10));
      assertEqual(1000, AHUACATL_ARITHMETIC_TIMES(10, 100));
      assertEqual(1.44, AHUACATL_ARITHMETIC_TIMES(1.2, 1.2));
      assertEqual(-1.44, AHUACATL_ARITHMETIC_TIMES(1.2, -1.2));
      assertEqual(1.44, AHUACATL_ARITHMETIC_TIMES(-1.2, -1.2));
      assertEqual(2, AHUACATL_ARITHMETIC_TIMES(0.25, 8));
      assertEqual(2, AHUACATL_ARITHMETIC_TIMES(8, 0.25));
      assertEqual(2.25, AHUACATL_ARITHMETIC_TIMES(9, 0.25));
      assertEqual(-2.25, AHUACATL_ARITHMETIC_TIMES(9, -0.25));
      assertEqual(-2.25, AHUACATL_ARITHMETIC_TIMES(-9, 0.25));
      assertEqual(2.25, AHUACATL_ARITHMETIC_TIMES(-9, -0.25));
      assertEqual(4, AHUACATL_ARITHMETIC_TIMES(-2, -2));
      assertEqual(-4, AHUACATL_ARITHMETIC_TIMES(-2, 2));
      assertEqual(-4, AHUACATL_ARITHMETIC_TIMES(2, -2));
      assertEqual(1000000, AHUACATL_ARITHMETIC_TIMES(1000, 1000));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_ARITHMETIC_DIVIDE function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticDivideUndefined : function () {
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, null); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, false); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, true); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, 2); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, -1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, ''); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, ' '); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, 'a'); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, [ ]); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, [ 1 ]); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, { }); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, { 'a' : 0 }); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(undefined, NaN); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(null, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(false, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(true, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(0, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(2, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(-1, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE('', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE('0', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(' ', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE('a', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE([ ], undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE([ 1 ], undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE({ }, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE({ 'a' : 0 }, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(NaN, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, NaN); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, null); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, false); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, true); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, ''); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, ' '); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, '1'); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, 'a'); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, [ ]); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, [ 0 ]); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, { }); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, { 'a' : 0 }); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(NaN, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(null, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(false, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(true, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE('', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(' ', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE('0', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE('1', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE('a', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE([ ], 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE([ 0 ], 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE({ }, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE({ 'a' : 0 }, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(1, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(100, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(-1, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(-100, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE(0, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_DIVIDE('0', '0'); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_ARITHMETIC_DIVIDE function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticDivideValue : function () {
      assertEqual(0, AHUACATL_ARITHMETIC_DIVIDE(0, 1));
      assertEqual(0, AHUACATL_ARITHMETIC_DIVIDE(0, 2));
      assertEqual(0, AHUACATL_ARITHMETIC_DIVIDE(0, 10));
      assertEqual(0, AHUACATL_ARITHMETIC_DIVIDE(0, -1));
      assertEqual(0, AHUACATL_ARITHMETIC_DIVIDE(0, -2));
      assertEqual(0, AHUACATL_ARITHMETIC_DIVIDE(0, -10));
      assertEqual(1, AHUACATL_ARITHMETIC_DIVIDE(1, 1));
      assertEqual(2, AHUACATL_ARITHMETIC_DIVIDE(2, 1));
      assertEqual(-1, AHUACATL_ARITHMETIC_DIVIDE(-1, 1));
      assertEqual(100, AHUACATL_ARITHMETIC_DIVIDE(100, 1));
      assertEqual(-100, AHUACATL_ARITHMETIC_DIVIDE(-100, 1));
      assertEqual(-1, AHUACATL_ARITHMETIC_DIVIDE(1, -1));
      assertEqual(-2, AHUACATL_ARITHMETIC_DIVIDE(2, -1));
      assertEqual(1, AHUACATL_ARITHMETIC_DIVIDE(-1, -1));
      assertEqual(-100, AHUACATL_ARITHMETIC_DIVIDE(100, -1));
      assertEqual(100, AHUACATL_ARITHMETIC_DIVIDE(-100, -1));
      assertEqual(0.5, AHUACATL_ARITHMETIC_DIVIDE(1, 2));
      assertEqual(1, AHUACATL_ARITHMETIC_DIVIDE(2, 2));
      assertEqual(2, AHUACATL_ARITHMETIC_DIVIDE(4, 2));
      assertEqual(1, AHUACATL_ARITHMETIC_DIVIDE(4, 4));
      assertEqual(5, AHUACATL_ARITHMETIC_DIVIDE(10, 2));
      assertEqual(2, AHUACATL_ARITHMETIC_DIVIDE(10, 5));
      assertEqual(2.5, AHUACATL_ARITHMETIC_DIVIDE(10, 4));
      assertEqual(1, AHUACATL_ARITHMETIC_DIVIDE(1.2, 1.2));
      assertEqual(-1, AHUACATL_ARITHMETIC_DIVIDE(1.2, -1.2));
      assertEqual(1, AHUACATL_ARITHMETIC_DIVIDE(-1.2, -1.2));
      assertEqual(2, AHUACATL_ARITHMETIC_DIVIDE(1, 0.5));
      assertEqual(4, AHUACATL_ARITHMETIC_DIVIDE(1, 0.25));
      assertEqual(16, AHUACATL_ARITHMETIC_DIVIDE(4, 0.25));
      assertEqual(-16, AHUACATL_ARITHMETIC_DIVIDE(4, -0.25));
      assertEqual(-16, AHUACATL_ARITHMETIC_DIVIDE(-4, 0.25));
      assertEqual(100, AHUACATL_ARITHMETIC_DIVIDE(25, 0.25));
      assertEqual(-100, AHUACATL_ARITHMETIC_DIVIDE(25, -0.25));
      assertEqual(-100, AHUACATL_ARITHMETIC_DIVIDE(-25, 0.25));
      assertEqual(1, AHUACATL_ARITHMETIC_DIVIDE(0.22, 0.22));
      assertEqual(0.5, AHUACATL_ARITHMETIC_DIVIDE(0.22, 0.44));
      assertEqual(2, AHUACATL_ARITHMETIC_DIVIDE(0.22, 0.11));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_ARITHMETIC_MODULUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticModulusUndefined : function () {
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, null); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, false); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, true); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, 2); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, -1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, ''); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, ' '); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, 'a'); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, [ ]); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, [ 1 ]); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, { }); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, { 'a' : 0 }); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(undefined, NaN); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(null, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(false, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(true, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(0, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(2, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(-1, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS('', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS('0', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(' ', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS('a', undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS([ ], undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS([ 1 ], undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS({ }, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS({ 'a' : 0 }, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(NaN, undefined); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, NaN); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, null); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, false); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, true); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, ''); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, ' '); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, '0'); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, '1'); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, 'a'); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, [ ]); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, [ 0 ]); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, { }); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, { 'a' : 0 }); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(NaN, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(null, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(false, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(true, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS('', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(' ', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS('0', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS('1', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS('a', 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS([ ], 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS([ 0 ], 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS({ }, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS({ 'a' : 0 }, 1); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(1, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(100, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(-1, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(-100, 0); });
      assertException(function() { AHUACATL_ARITHMETIC_MODULUS(0, 0); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_ARITHMETIC_MODULUS function
////////////////////////////////////////////////////////////////////////////////

    testArithmeticModulusValue : function () {
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(0, 1));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(1, 1));
      assertEqual(1, AHUACATL_ARITHMETIC_MODULUS(1, 2));
      assertEqual(1, AHUACATL_ARITHMETIC_MODULUS(1, 3));
      assertEqual(1, AHUACATL_ARITHMETIC_MODULUS(1, 4));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(2, 2));
      assertEqual(2, AHUACATL_ARITHMETIC_MODULUS(2, 3));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(3, 3));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(10, 1));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(10, 2));
      assertEqual(1, AHUACATL_ARITHMETIC_MODULUS(10, 3));
      assertEqual(2, AHUACATL_ARITHMETIC_MODULUS(10, 4));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(10, 5));
      assertEqual(4, AHUACATL_ARITHMETIC_MODULUS(10, 6));
      assertEqual(3, AHUACATL_ARITHMETIC_MODULUS(10, 7));
      assertEqual(2, AHUACATL_ARITHMETIC_MODULUS(10, 8));
      assertEqual(1, AHUACATL_ARITHMETIC_MODULUS(10, 9));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(10, 10));
      assertEqual(10, AHUACATL_ARITHMETIC_MODULUS(10, 11));
      assertEqual(1, AHUACATL_ARITHMETIC_MODULUS(4, 3));
      assertEqual(2, AHUACATL_ARITHMETIC_MODULUS(5, 3));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(6, 3));
      assertEqual(1, AHUACATL_ARITHMETIC_MODULUS(7, 3));
      assertEqual(2, AHUACATL_ARITHMETIC_MODULUS(8, 3));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(9, 3));
      assertEqual(1, AHUACATL_ARITHMETIC_MODULUS(10, 3));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(10, -1));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(10, -2));
      assertEqual(1, AHUACATL_ARITHMETIC_MODULUS(10, -3));
      assertEqual(2, AHUACATL_ARITHMETIC_MODULUS(10, -4));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(10, -5));
      assertEqual(4, AHUACATL_ARITHMETIC_MODULUS(10, -6));
      assertEqual(3, AHUACATL_ARITHMETIC_MODULUS(10, -7));
      assertEqual(2, AHUACATL_ARITHMETIC_MODULUS(10, -8));
      assertEqual(1, AHUACATL_ARITHMETIC_MODULUS(10, -9));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(10, -10));
      assertEqual(10, AHUACATL_ARITHMETIC_MODULUS(10, -11));
      assertEqual(-1, AHUACATL_ARITHMETIC_MODULUS(-1, 3));
      assertEqual(-2, AHUACATL_ARITHMETIC_MODULUS(-2, 3));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(-3, 3));
      assertEqual(-1, AHUACATL_ARITHMETIC_MODULUS(-4, 3));
      assertEqual(-2, AHUACATL_ARITHMETIC_MODULUS(-5, 3));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(-6, 3));
      assertEqual(-1, AHUACATL_ARITHMETIC_MODULUS(-7, 3));
      assertEqual(-2, AHUACATL_ARITHMETIC_MODULUS(-8, 3));
      assertEqual(0, AHUACATL_ARITHMETIC_MODULUS(-9, 3));
      assertEqual(-1, AHUACATL_ARITHMETIC_MODULUS(-10, 3));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_STRING_CONCAT function
////////////////////////////////////////////////////////////////////////////////

    testStringConcatUndefined : function () {
      assertException(function() { AHUACATL_STRING_CONCAT(undefined, false); });
      assertException(function() { AHUACATL_STRING_CONCAT(undefined, true); });
      assertException(function() { AHUACATL_STRING_CONCAT(undefined, 0); });
      assertException(function() { AHUACATL_STRING_CONCAT(undefined, 1); });
      assertException(function() { AHUACATL_STRING_CONCAT(undefined, 2); });
      assertException(function() { AHUACATL_STRING_CONCAT(undefined, -1); });
      assertException(function() { AHUACATL_STRING_CONCAT(undefined, [ ]); });
      assertException(function() { AHUACATL_STRING_CONCAT(undefined, [ 1 ]); });
      assertException(function() { AHUACATL_STRING_CONCAT(undefined, { }); });
      assertException(function() { AHUACATL_STRING_CONCAT(undefined, { 'a' : 0 }); });
      assertException(function() { AHUACATL_STRING_CONCAT(false, undefined); });
      assertException(function() { AHUACATL_STRING_CONCAT(true, undefined); });
      assertException(function() { AHUACATL_STRING_CONCAT(0, undefined); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, undefined); });
      assertException(function() { AHUACATL_STRING_CONCAT(2, undefined); });
      assertException(function() { AHUACATL_STRING_CONCAT(-1, undefined); });
      assertException(function() { AHUACATL_STRING_CONCAT([ ], undefined); });
      assertException(function() { AHUACATL_STRING_CONCAT([ 1 ], undefined); });
      assertException(function() { AHUACATL_STRING_CONCAT({ }, undefined); });
      assertException(function() { AHUACATL_STRING_CONCAT({ 'a' : 0 }, undefined); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, NaN); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, null); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, false); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, true); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, ''); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, ' '); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, '0'); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, '1'); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, 'a'); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, [ ]); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, [ 0 ]); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, { }); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, { 'a' : 0 }); });
      assertException(function() { AHUACATL_STRING_CONCAT(NaN, 1); });
      assertException(function() { AHUACATL_STRING_CONCAT(null, 1); });
      assertException(function() { AHUACATL_STRING_CONCAT(false, 1); });
      assertException(function() { AHUACATL_STRING_CONCAT(true, 1); });
      assertException(function() { AHUACATL_STRING_CONCAT('', 1); });
      assertException(function() { AHUACATL_STRING_CONCAT(' ', 1); });
      assertException(function() { AHUACATL_STRING_CONCAT('0', 1); });
      assertException(function() { AHUACATL_STRING_CONCAT('1', 1); });
      assertException(function() { AHUACATL_STRING_CONCAT('a', 1); });
      assertException(function() { AHUACATL_STRING_CONCAT([ ], 1); });
      assertException(function() { AHUACATL_STRING_CONCAT([ 0 ], 1); });
      assertException(function() { AHUACATL_STRING_CONCAT({ }, 1); });
      assertException(function() { AHUACATL_STRING_CONCAT({ 'a' : 0 }, 1); });
      assertException(function() { AHUACATL_STRING_CONCAT(1, 0); });
      assertException(function() { AHUACATL_STRING_CONCAT(100, 0); });
      assertException(function() { AHUACATL_STRING_CONCAT(-1, 0); });
      assertException(function() { AHUACATL_STRING_CONCAT(-100, 0); });
      assertException(function() { AHUACATL_STRING_CONCAT(0, 0); });
      assertException(function() { AHUACATL_STRING_CONCAT('', false); });
      assertException(function() { AHUACATL_STRING_CONCAT('', true); });
      assertException(function() { AHUACATL_STRING_CONCAT('', [ ]); });
      assertException(function() { AHUACATL_STRING_CONCAT('', { }); });
      assertException(function() { AHUACATL_STRING_CONCAT('a', false); });
      assertException(function() { AHUACATL_STRING_CONCAT('a', true); });
      assertException(function() { AHUACATL_STRING_CONCAT('a', [ ]); });
      assertException(function() { AHUACATL_STRING_CONCAT('a', { }); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_STRING_CONCAT function
////////////////////////////////////////////////////////////////////////////////

    testStringConcatValue : function () {
      assertEqual('', AHUACATL_STRING_CONCAT());
      assertEqual('', AHUACATL_STRING_CONCAT(''));
      assertEqual('a', AHUACATL_STRING_CONCAT('a'));
      assertEqual('a', AHUACATL_STRING_CONCAT('a', null));
      assertEqual('', AHUACATL_STRING_CONCAT('', null));
      assertEqual('', AHUACATL_STRING_CONCAT(undefined, ''));
      assertEqual('0', AHUACATL_STRING_CONCAT(undefined, '0'));
      assertEqual(' ', AHUACATL_STRING_CONCAT(undefined, ' '));
      assertEqual('a', AHUACATL_STRING_CONCAT(undefined, 'a'));
      assertEqual('', AHUACATL_STRING_CONCAT('', undefined));
      assertEqual('0', AHUACATL_STRING_CONCAT('0', undefined));
      assertEqual(' ', AHUACATL_STRING_CONCAT(' ', undefined));
      assertEqual('a', AHUACATL_STRING_CONCAT('a', undefined));
      assertEqual('', AHUACATL_STRING_CONCAT(undefined, NaN));
      assertEqual('', AHUACATL_STRING_CONCAT(null, undefined));
      assertEqual('', AHUACATL_STRING_CONCAT(NaN, undefined));

      assertEqual('', AHUACATL_STRING_CONCAT('', ''));
      assertEqual(' ', AHUACATL_STRING_CONCAT(' ', ''));
      assertEqual(' ', AHUACATL_STRING_CONCAT('', ' '));
      assertEqual('  ', AHUACATL_STRING_CONCAT(' ', ' '));
      assertEqual(' a a', AHUACATL_STRING_CONCAT(' a', ' a'));
      assertEqual(' a a ', AHUACATL_STRING_CONCAT(' a', ' a '));
      assertEqual('a', AHUACATL_STRING_CONCAT('a', ''));
      assertEqual('a', AHUACATL_STRING_CONCAT('', 'a'));
      assertEqual('a ', AHUACATL_STRING_CONCAT('', 'a '));
      assertEqual('a ', AHUACATL_STRING_CONCAT('', 'a '));
      assertEqual('a ', AHUACATL_STRING_CONCAT('a ', ''));
      assertEqual('ab', AHUACATL_STRING_CONCAT('a', 'b'));
      assertEqual('ba', AHUACATL_STRING_CONCAT('b', 'a'));
      assertEqual('AA', AHUACATL_STRING_CONCAT('A', 'A'));
      assertEqual('AaA', AHUACATL_STRING_CONCAT('A', 'aA'));
      assertEqual('AaA', AHUACATL_STRING_CONCAT('Aa', 'A'));
      assertEqual('0.00', AHUACATL_STRING_CONCAT('0.00', ''));
      assertEqual('abc', AHUACATL_STRING_CONCAT('a', AHUACATL_STRING_CONCAT('b', 'c')));
      assertEqual('', AHUACATL_STRING_CONCAT('', AHUACATL_STRING_CONCAT('', '')));
      assertEqual('fux', AHUACATL_STRING_CONCAT('f', 'u', 'x'));
      assertEqual('fux', AHUACATL_STRING_CONCAT('f', 'u', null, 'x'));
      assertEqual('fux', AHUACATL_STRING_CONCAT(null, 'f', null, 'u', null, 'x', null));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AHUACATL_TERNARY_OPERATOR function
////////////////////////////////////////////////////////////////////////////////

    testTernaryOperator : function () {
      assertEqual(2, AHUACATL_TERNARY_OPERATOR(true, 2, -1));
      assertEqual(-1, AHUACATL_TERNARY_OPERATOR(false, 1, -1));
      assertException(function() { AHUACATL_TERNARY_OPERATOR(0, 1, 1); } );
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlOperatorsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
