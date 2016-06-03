/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, AQL_EXECUTE */

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
var aql = require("@arangodb/aql");
var db = require("internal").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlOperatorsTestSuite () {
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function() {},

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function() {},

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.KEYS function
    ////////////////////////////////////////////////////////////////////////////////

    testKeys: function() {
      assertEqual([ ], aql.KEYS([ ], true));
      assertEqual([ 0, 1, 2 ], aql.KEYS([ 0, 1, 2 ], true));
      assertEqual([ 0, 1, 2 ], aql.KEYS([ 1, 2, 3 ], true));
      assertEqual([ 0, 1, 2 ], aql.KEYS([ 5, 6, 9 ], true));
      assertEqual([ 0, 1, 2 ], aql.KEYS([ false, false, false ], true));
      assertEqual([ 0, 1, 2 ], aql.KEYS([ -1, -1, 'zang' ], true));
      assertEqual([ 0, 1, 2, 3 ], aql.KEYS([ 99, 99, 99, 99 ], true));
      assertEqual([ 0 ], aql.KEYS([ [ ] ], true));
      assertEqual([ 0 ], aql.KEYS([ [ 1 ] ], true));
      assertEqual([ 0, 1 ], aql.KEYS([ [ 1 ], 1 ], true));
      assertEqual([ 0, 1 ], aql.KEYS([ [ 1 ], [ 1 ] ], true));
      assertEqual([ 0 ], aql.KEYS([ [ 1 , 2 ] ], true));
      assertEqual([ 0, 1, 2 ], aql.KEYS([ [ 1 , 2 ], [ ], 3 ], true));
      assertEqual([ 0, 1, 2 ], aql.KEYS([ { }, { }, { } ], true));
      assertEqual([ 0, 1, 2 ], aql.KEYS([ { 'a' : true, 'b' : false }, { }, { } ], true));
      assertEqual([ ], aql.KEYS({ }, true));
      assertEqual([ '0' ], aql.KEYS({ '0' : false }, true));
      assertEqual([ '0' ], aql.KEYS({ '0' : undefined }, true));
      assertEqual([ 'a', 'b', 'c' ], aql.KEYS({ 'a' : true, 'b' : true, 'c' : true }, true));
      assertEqual([ 'a', 'b', 'c' ], aql.KEYS({ 'a' : true, 'c' : true, 'b' : true }, true));
      assertEqual([ 'a', 'b', 'c' ], aql.KEYS({ 'b' : true, 'a' : true, 'c' : true }, true));
      assertEqual([ 'a', 'b', 'c' ], aql.KEYS({ 'b' : true, 'c' : true, 'a' : true }, true));
      assertEqual([ 'a', 'b', 'c' ], aql.KEYS({ 'c' : true, 'a' : true, 'b' : true }, true));
      assertEqual([ 'a', 'b', 'c' ], aql.KEYS({ 'c' : true, 'b' : true, 'a' : true }, true));
      assertEqual([ '0', '1', '2' ], aql.KEYS({ '0' : true, '1' : true, '2' : true }, true));
      assertEqual([ '0', '1', '2' ], aql.KEYS({ '1' : true, '2' : true, '0' : true }, true));
      assertEqual([ 'a1', 'a2', 'a3' ], aql.KEYS({ 'a1' : true, 'a2' : true, 'a3' : true }, true));
      assertEqual([ 'a1', 'a10', 'a20', 'a200', 'a21' ], aql.KEYS({ 'a200' : true, 'a21' : true, 'a20' : true, 'a10' : false, 'a1' : null }, true));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_NULL function
    ////////////////////////////////////////////////////////////////////////////////

    testIsNullTrue: function() {
      assertTrue(aql.AQL_IS_NULL(undefined));
      assertTrue(aql.AQL_IS_NULL(NaN));
      assertTrue(aql.AQL_IS_NULL(1.3e308 * 1.3e308));
      var values = [ null ];
      values.forEach(function (v) {
        assertTrue(db._query(`RETURN IS_NULL(${v})`).next());
        assertTrue(db._query(`RETURN NOOPT(IS_NULL(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_NULL function
    ////////////////////////////////////////////////////////////////////////////////

    testIsNullFalse: function() {
      var values = [
        0,
        1,
        -1,
        0.1,
        -0.1,
        false,
        true,
        '"abc"',
        '"null"',
        '"false"',
        '"undefined"',
        '""',
        '" "',
        "[ ]",
        "[ 0 ]",
        "[ 0, 1 ]",
        "[ 1, 2 ]",
        "{ }",
        "{ 'a' : 0 }",
        "{ 'a' : 1 }",
        "{ 'a' : 0, 'b' :1 }"
      ];
      values.forEach(function (v) {
        assertFalse(db._query(`RETURN IS_NULL(${v})`).next());
        assertFalse(db._query(`RETURN NOOPT(IS_NULL(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_BOOL function
    ////////////////////////////////////////////////////////////////////////////////

    testIsBoolTrue: function() {
      var values = [ false, true ];
      values.forEach(function (v) {
        assertTrue(db._query(`RETURN IS_BOOL(${v})`).next());
        assertTrue(db._query(`RETURN NOOPT(IS_BOOL(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_BOOL function
    ////////////////////////////////////////////////////////////////////////////////

    testIsBoolFalse: function() {
      assertFalse(aql.AQL_IS_BOOL(undefined));
      assertFalse(aql.AQL_IS_BOOL(NaN));
      assertFalse(aql.AQL_IS_BOOL(1.3e308 * 1.3e308));

      var values = [
        0,
        1,
        -1,
        0.1,
        -0.1,
        null,
        '"abc"',
        '"null"',
        '"false"',
        '"undefined"',
        '""',
        '" "',
        "[ ]",
        "[ 0 ]",
        "[ 0, 1 ]",
        "[ 1, 2 ]",
        "[ '' ]",
        "[ '0' ]",
        "[ '1' ]",
        "[ true ]",
        "[ false ]",
        "{ }",
        "{ 'a' : 0 }",
        "{ 'a' : 1 }",
        "{ 'a' : 0, 'b' :1 }"
      ];

      values.forEach(function (v) {
        assertFalse(db._query(`RETURN IS_BOOL(${v})`).next());
        assertFalse(db._query(`RETURN NOOPT(IS_BOOL(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_NUMBER function
    ////////////////////////////////////////////////////////////////////////////////

    testIsNumberTrue: function() {
      var values = [0, 1, -1, 0.1, 12.5356, -235.26436, -23.3e17, 563.44576e19];

      values.forEach(function (v) {
        assertTrue(db._query(`RETURN IS_NUMBER(${v})`).next());
        assertTrue(db._query(`RETURN NOOPT(IS_NUMBER(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_NUMBER function
    ////////////////////////////////////////////////////////////////////////////////

    testIsNumberFalse: function() {
      assertFalse(aql.AQL_IS_NUMBER(undefined));
      assertFalse(aql.AQL_IS_NUMBER(NaN));
      assertFalse(aql.AQL_IS_NUMBER(1.3e308 * 1.3e308));

      var values = [
        false,
        true,
        null,
        '"abc"',
        '"null"',
        '"false"',
        '"undefined"',
        '""',
        '" "',
        "[ ]",
        "[ 0 ]",
        "[ 0, 1 ]",
        "[ 1, 2 ]",
        "[ '' ]",
        "[ '0' ]",
        "[ '1' ]",
        "[ true ]",
        "[ false ]",
        "{ }",
        "{ 'a' : 0 }",
        "{ 'a' : 1 }",
        "{ 'a' : 0, 'b' :1 }"
      ];

      values.forEach(function (v) {
        assertFalse(db._query(`RETURN IS_NUMBER(${v})`).next());
        assertFalse(db._query(`RETURN NOOPT(IS_NUMBER(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_STRING function
    ////////////////////////////////////////////////////////////////////////////////

    testIsStringTrue: function() {
      var values = ["'abc'", "'null'", "'false'", "'undefined'", "''", "' '"];
      values.forEach(function (v) {
        assertTrue(db._query(`RETURN IS_STRING(${v})`).next());
        assertTrue(db._query(`RETURN NOOPT(IS_STRING(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_STRING function
    ////////////////////////////////////////////////////////////////////////////////

    testIsStringFalse: function() {
      assertFalse(aql.AQL_IS_STRING(undefined));
      assertFalse(aql.AQL_IS_STRING(NaN));
      assertFalse(aql.AQL_IS_STRING(1.3e308 * 1.3e308));

      var values = [
        false,
        true,
        0,
        1,
        -1,
        0.1,
        -0.1,
        null,
        "[ ]",
        "[ 0 ]",
        "[ 0, 1 ]",
        "[ 1, 2 ]",
        "[ '' ]",
        "[ '0' ]",
        "[ '1' ]",
        "[ true ]",
        "[ false ]",
        "{ }",
        "{ 'a' : 0 }",
        "{ 'a' : 1 }",
        "{ 'a' : 0, 'b' :1 }"
      ];

      values.forEach(function (v) {
        assertFalse(db._query(`RETURN IS_STRING(${v})`).next());
        assertFalse(db._query(`RETURN NOOPT(IS_STRING(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_LIST function
    ////////////////////////////////////////////////////////////////////////////////

    testIsListTrue: function() {
      var values = [
        "[ ]",
        "[ 0 ]",
        "[ 0, 1 ]",
        "[ 1, 2 ]",
        "[ '' ]",
        "[ '0' ]",
        "[ '1' ]",
        "[ true ]",
        "[ false ]",
      ];
      values.forEach(function (v) {
        assertTrue(db._query(`RETURN IS_LIST(${v})`).next());
        assertTrue(db._query(`RETURN NOOPT(IS_LIST(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_LIST function
    ////////////////////////////////////////////////////////////////////////////////

    testIsListFalse: function() {
      assertFalse(aql.AQL_IS_LIST(undefined));
      assertFalse(aql.AQL_IS_LIST(NaN));
      assertFalse(aql.AQL_IS_LIST(1.3e308 * 1.3e308));

      var values = [
        0,
        1,
        -1,
        0.1,
        -0.1,
        null,
        false,
        true,
        '"abc"',
        '"null"',
        '"false"',
        '"undefined"',
        '""',
        '" "',
        "{ }",
        "{ 'a' : 0 }",
        "{ 'a' : 1 }",
        "{ 'a' : 0, 'b' :1 }"
      ];
      values.forEach(function (v) {
        assertFalse(db._query(`RETURN IS_LIST(${v})`).next());
        assertFalse(db._query(`RETURN NOOPT(IS_LIST(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_ARRAY function
    ////////////////////////////////////////////////////////////////////////////////

    testIsArrayTrue: function() {
      var values = [
        "[ ]",
        "[ 0 ]",
        "[ 0, 1 ]",
        "[ 1, 2 ]",
        "[ '' ]",
        "[ '0' ]",
        "[ '1' ]",
        "[ true ]",
        "[ false ]",
      ];
      values.forEach(function (v) {
        assertTrue(db._query(`RETURN IS_ARRAY(${v})`).next());
        assertTrue(db._query(`RETURN NOOPT(IS_ARRAY(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_ARRAY function
    ////////////////////////////////////////////////////////////////////////////////

    testIsArrayFalse: function() {
      assertFalse(aql.AQL_IS_ARRAY(undefined));
      assertFalse(aql.AQL_IS_ARRAY(NaN));
      assertFalse(aql.AQL_IS_ARRAY(1.3e308 * 1.3e308));

      var values = [
        0,
        1,
        -1,
        0.1,
        -0.1,
        null,
        false,
        true,
        '"abc"',
        '"null"',
        '"false"',
        '"undefined"',
        '""',
        '" "',
        "{ }",
        "{ 'a' : 0 }",
        "{ 'a' : 1 }",
        "{ 'a' : 0, 'b' :1 }"
      ];
      values.forEach(function (v) {
        assertFalse(db._query(`RETURN IS_ARRAY(${v})`).next());
        assertFalse(db._query(`RETURN NOOPT(IS_ARRAY(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_DOCUMENT function
    ////////////////////////////////////////////////////////////////////////////////

    testIsDocumentTrue: function() {
      var values = [
        "{ }",
        "{ 'a' : 0 }",
        "{ 'a' : 1 }",
        "{ 'a' : 0, 'b' : 1 }",
        "{ '1' : false, 'b' : false }",
        "{ '0' : false }",
      ];
      values.forEach(function(v) {
        assertTrue(db._query(`RETURN IS_DOCUMENT(${v})`).next());
        assertTrue(db._query(`RETURN NOOPT(IS_DOCUMENT(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_DOCUMENT function
    ////////////////////////////////////////////////////////////////////////////////

    testIsDocumentFalse: function() {
      assertFalse(aql.AQL_IS_DOCUMENT(undefined));
      assertFalse(aql.AQL_IS_DOCUMENT(NaN));
      assertFalse(aql.AQL_IS_DOCUMENT(1.3e308 * 1.3e308));

      var values = [
        0,
        1,
        -1,
        0.1,
        -0.1,
        null,
        false,
        true,
        '"abc"',
        '"null"',
        '"false"',
        '"undefined"',
        '""',
        '" "',
        "[ ]",
        "[ 0 ]",
        "[ 0, 1 ]",
        "[ 1, 2 ]",
        "[ '' ]",
        "[ '0' ]",
        "[ '1' ]",
        "[ true ]",
        "[ false ]",
      ];
      values.forEach(function(v) {
        assertFalse(db._query(`RETURN IS_DOCUMENT(${v})`).next());
        assertFalse(db._query(`RETURN NOOPT(IS_DOCUMENT(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_OBJECT function
    ////////////////////////////////////////////////////////////////////////////////

    testIsObjectTrue: function() {
      var values = [
        "{ }",
        "{ 'a' : 0 }",
        "{ 'a' : 1 }",
        "{ 'a' : 0, 'b' : 1 }",
        "{ '1' : false, 'b' : false }",
        "{ '0' : false }",
      ];
      values.forEach(function(v) {
        assertTrue(db._query(`RETURN IS_OBJECT(${v})`).next());
        assertTrue(db._query(`RETURN NOOPT(IS_OBJECT(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_OBJECT function
    ////////////////////////////////////////////////////////////////////////////////

    testIsObjecttFalse: function() {
      assertFalse(aql.AQL_IS_OBJECT(undefined));
      assertFalse(aql.AQL_IS_OBJECT(NaN));
      assertFalse(aql.AQL_IS_OBJECT(1.3e308 * 1.3e308));

      var values = [
        0,
        1,
        -1,
        0.1,
        -0.1,
        null,
        false,
        true,
        '"abc"',
        '"null"',
        '"false"',
        '"undefined"',
        '""',
        '" "',
        "[ ]",
        "[ 0 ]",
        "[ 0, 1 ]",
        "[ 1, 2 ]",
        "[ '' ]",
        "[ '0' ]",
        "[ '1' ]",
        "[ true ]",
        "[ false ]",
      ];
      values.forEach(function(v) {
        assertFalse(db._query(`RETURN IS_OBJECT(${v})`).next());
        assertFalse(db._query(`RETURN NOOPT(IS_OBJECT(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.TO_BOOL function
    ////////////////////////////////////////////////////////////////////////////////

    testCastBoolTrue: function() {
      var values = [
        1,
        2,
        -1,
        100,
        100.01,
        0.001,
        -0.001,
        true,
        '"abc"',
        '"null"',
        '"false"',
        '"undefined"',
        '" "',
        '"  "',
        '"1"',
        '"1 "',
        '"0"',
        '"-1"',
        "[ ]",
        "[ 0 ]",
        "[ 0, 1 ]",
        "[ 1, 2 ]",
        "[ -1, 0 ]",
        "[ '' ]",
        "[ true ]",
        "[ false ]",
        "{ }",
        "{ 'a' : true }",
        "{ 'a' : false }",
        "{ 'a' : false, 'b' : 0 }",
        "{ '0' : false }"
      ];
      values.forEach(function(v) {
        assertEqual(true, db._query(`RETURN TO_BOOL(${v})`).next());
        assertEqual(true, db._query(`RETURN NOOPT(TO_BOOL(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.TO_BOOL function
    ////////////////////////////////////////////////////////////////////////////////

    testCastBoolFalse: function() {
      assertEqual(false, aql.AQL_TO_BOOL(NaN));
      assertEqual(false, aql.AQL_TO_BOOL(undefined));

      var values = [0, "''", null, false];
      values.forEach(function(v) {
        assertEqual(false, db._query(`RETURN TO_BOOL(${v})`).next());
        assertEqual(false, db._query(`RETURN NOOPT(TO_BOOL(${v}))`).next());
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.TO_NUMBER function
    ////////////////////////////////////////////////////////////////////////////////

    testCastNumber: function() {
      assertEqual(0, aql.AQL_TO_NUMBER(undefined));
      assertEqual(0, aql.AQL_TO_NUMBER(NaN));

      var values = [
        {ex: 0, val: null},
        {ex: 0, val: false},
        {ex: 1, val: true},
        {ex: 1, val: 1},
        {ex: 2, val: 2},
        {ex: -1, val: -1},
        {ex: 0, val: 0},
        {ex: 0, val: "''"},
        {ex: 0, val: "' '"},
        {ex: 0, val: "'  '"},
        {ex: 1, val: "'1'"},
        {ex: 1, val: "'1 '"},
        {ex: 1, val: "' 1 '"},
        {ex: 0, val: "'0'"},
        {ex: -1, val: "'-1'"},
        {ex: -1, val: "'-1 '"},
        {ex: -1, val: "' -1 '"},
        {ex: null, val: "' -1a '"},
        {ex: null, val: "' 1a '"},
        {ex: null, val: "' 12335.3 a '"},
        {ex: null, val: "'a1bc'"},
        {ex: null, val: "'aaa1'"},
        {ex: null, val: "'-a1'"},
        {ex: -1.255, val: "'-1.255'"},
        {ex: -1.23456, val: "'-1.23456'"},
        {ex: -1.23456, val: "'-1.23456 '"},
        {ex: 1.23456, val: "' 1.23456 '"},
        {ex: null, val: "'  1.23456a'"},
        {ex: null, val: "'--1'"},
        {ex: 1, val: "'+1'"},
        {ex: 12.42e32, val: "'12.42e32'"},
        {ex: 0, val: "[]"},
        {ex: 0, val: "[ 0 ]"},
        {ex: -17, val: "[ -17 ]"},
        {ex: null, val: "[ 0, 1 ]"},
        {ex: null, val: "[ 1, 2 ]"},
        {ex: null, val: "[ -1, 0 ]"},
        {ex: null, val: "[ 0, 1, [ 1, 2 ], [ [ 9, 4 ] ] ]"},
        {ex: null, val: "[ { } ]"},
        {ex: null, val: "[ 0, 1, { } ]"},
        {ex: null, val: "[ { }, { } ]"},
        {ex: 0, val: "[ '' ]"},
        {ex: 0, val: "[ false ]"},
        {ex: 1, val: "[ true ]"},
        {ex: null, val: "{ }"},
        {ex: null, val: "{ 'a' : true }"},
        {ex: null, val: "{ 'a' : true, 'b' : 0 }"},
        {ex: null, val: "{ 'a' : { }, 'b' : { } }"},
        {ex: null, val: "{ 'a' : [ ], 'b' : [ ] }"}
      ];
      values.forEach(function(v) {
        // double precission check for e=0.0001
        var q = `RETURN TO_NUMBER(${v.val})`;
        var diff = db._query(q).next() - v.ex;
        assertTrue(diff < 0.0001, q);
        q = `RETURN NOOPT(TO_NUMBER(${v.val}))`;
        diff = db._query(q).next() - v.ex;
        assertTrue(diff < 0.0001, q);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.TO_STRING function
    ////////////////////////////////////////////////////////////////////////////////

    testCastString: function() {
      assertEqual('', aql.AQL_TO_STRING(undefined));
      assertEqual('', aql.AQL_TO_STRING(NaN));
      assertEqual('', aql.AQL_TO_STRING(null));
      assertEqual('', aql.AQL_TO_STRING(1 / 0));
      var values = [
        {ex: "", val: null},
        {ex: "false", val: false},
        {ex: "true", val: true},
        {ex: "1", val: 1},
        {ex: "2", val: 2},
        {ex: "-1", val: -1},
        {ex: "0", val: 0},
        {ex: "", val: "''"},
        {ex: " ", val: "' '"},
        {ex: "  ", val: "'  '"},
        {ex: "1", val: "'1'"},
        {ex: "1 ", val: "'1 '"},
        {ex: "0", val: "'0'"},
        {ex: "-1", val: "'-1'"},
        {ex: "[]", val: "[ ]"},
        {ex: "[0]", val: "[ 0 ]"},
        {ex: "[0,1]", val: "[ 0, 1 ]"},
        {ex: "[1,2]", val: "[ 1, 2 ]"},
        {ex: "[-1,0]", val: "[ -1, 0 ]"},
        {ex: "[0,1,[1,2],[[9,4]]]", val: "[ 0, 1, [1, 2], [ [ 9, 4 ] ] ]"},
        {ex: "[{}]", val: "[ { } ]"},
        {ex: "[0,1,{}]", val: "[ 0, 1, { } ]"},
        {ex: "[{},{}]", val: "[ { }, { } ]"},
        {ex: "[\"\"]", val: "['']"},
        {ex: "[false]", val: "[ false ]"},
        {ex: "[true]", val: "[ true ]"},
        {ex: "{}", val: "{ }"},
        {ex: "{\"a\":true}", val: "{ 'a' : true }"},
        {ex: "{\"a\":true,\"b\":0}", val: "{ 'a' : true, 'b' : 0 }"},
        {ex: "{\"a\":{},\"b\":{}}", val: "{ 'a' : { }, 'b' : { } }"},
        {ex: "{\"a\":[],\"b\":[]}", val: "{ 'a' : [ ], 'b' : [ ] }"}
      ];
      values.forEach(function(v) {
        var q = `RETURN TO_STRING(${v.val})`;
        assertEqual(v.ex, db._query(q).next(), q);
        q = `RETURN NOOPT(TO_STRING(${v.val}))`;
        assertEqual(v.ex, db._query(q).next(), q);
        q = `RETURN NOOPT(V8(TO_STRING(${v.val})))`;
        assertEqual(v.ex, db._query(q).next(), q);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_AND function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalAndUndefined: function() {
      var LOGICAL_AND = function (a, b) {
        return aql.LOGICAL_AND(function () { return a; }, function () { return b; });
      };

      assertEqual(undefined, LOGICAL_AND(undefined, undefined));
      assertEqual(undefined, LOGICAL_AND(undefined, null));
      assertEqual(undefined, LOGICAL_AND(undefined, true));
      assertEqual(undefined, LOGICAL_AND(undefined, false));
      assertEqual(undefined, LOGICAL_AND(undefined, 0.0));
      assertEqual(undefined, LOGICAL_AND(undefined, 1.0));
      assertEqual(undefined, LOGICAL_AND(undefined, -1.0));
      assertEqual(undefined, LOGICAL_AND(undefined, ''));
      assertEqual(undefined, LOGICAL_AND(undefined, '0'));
      assertEqual(undefined, LOGICAL_AND(undefined, '1'));
      assertEqual(undefined, LOGICAL_AND(undefined, [ ]));
      assertEqual(undefined, LOGICAL_AND(undefined, [ 0 ]));
      assertEqual(undefined, LOGICAL_AND(undefined, [ 0, 1 ]));
      assertEqual(undefined, LOGICAL_AND(undefined, [ 1, 2 ]));
      assertEqual(undefined, LOGICAL_AND(undefined, { }));
      assertEqual(undefined, LOGICAL_AND(undefined, { 'a' : 0 }));
      assertEqual(undefined, LOGICAL_AND(undefined, { 'a' : 1 }));
      assertEqual(undefined, LOGICAL_AND(undefined, { '0' : false }));
      assertEqual(null, LOGICAL_AND(null, undefined));
      assertEqual(undefined, LOGICAL_AND(true, undefined));
      assertEqual(false, LOGICAL_AND(false, undefined));
      assertEqual(0.0, LOGICAL_AND(0.0, undefined));
      assertEqual(undefined, LOGICAL_AND(1.0, undefined));
      assertEqual(undefined, LOGICAL_AND(-1.0, undefined));
      assertEqual('', LOGICAL_AND('', undefined));
      assertEqual(undefined, LOGICAL_AND('0', undefined));
      assertEqual(undefined, LOGICAL_AND('1', undefined));
      assertEqual(undefined, LOGICAL_AND([ ], undefined));
      assertEqual(undefined, LOGICAL_AND([ 0 ], undefined));
      assertEqual(undefined, LOGICAL_AND([ 0, 1 ], undefined));
      assertEqual(undefined, LOGICAL_AND([ 1, 2 ], undefined));
      assertEqual(undefined, LOGICAL_AND({ }, undefined));
      assertEqual(undefined, LOGICAL_AND({ 'a' : 0 }, undefined));
      assertEqual(undefined, LOGICAL_AND({ 'a' : 1 }, undefined));
      assertEqual(undefined, LOGICAL_AND({ '0' : false }, undefined));
      
      assertEqual(null, LOGICAL_AND(true, null));
      assertEqual(null, LOGICAL_AND(null, true));
      assertEqual('', LOGICAL_AND(true, ''));
      assertEqual('', LOGICAL_AND('', true));
      assertEqual(' ', LOGICAL_AND(true, ' '));
      assertEqual(true, LOGICAL_AND(' ', true));
      assertEqual('0', LOGICAL_AND(true, '0'));
      assertEqual(true, LOGICAL_AND('0', true));
      assertEqual('1', LOGICAL_AND(true, '1'));
      assertEqual(true, LOGICAL_AND('1', true));
      assertEqual('true', LOGICAL_AND(true, 'true'));
      assertEqual(true, LOGICAL_AND('true', true));
      assertEqual('false', LOGICAL_AND(true, 'false'));
      assertEqual(true, LOGICAL_AND('false', true));
      assertEqual(0, LOGICAL_AND(true, 0));
      assertEqual(0, LOGICAL_AND(0, true));
      assertEqual(1, LOGICAL_AND(true, 1));
      assertEqual(true, LOGICAL_AND(1, true));
      assertEqual(-1, LOGICAL_AND(true, -1));
      assertEqual(true, LOGICAL_AND(-1, true));
      assertEqual(1.1, LOGICAL_AND(true, 1.1));
      assertEqual(true, LOGICAL_AND(1.1, true));
      assertEqual([ ], LOGICAL_AND(true, [ ]));
      assertEqual(true, LOGICAL_AND([ ], true));
      assertEqual([ 0 ], LOGICAL_AND(true, [ 0 ]));
      assertEqual(true, LOGICAL_AND([ 0 ], true));
      assertEqual([ 0, 1 ], LOGICAL_AND(true, [ 0, 1 ]));
      assertEqual(true, LOGICAL_AND([ 0, 1 ], true));
      assertEqual([ true ], LOGICAL_AND(true, [ true ]));
      assertEqual(true, LOGICAL_AND([ true ], true));
      assertEqual([ false ], LOGICAL_AND(true, [ false ]));
      assertEqual(true, LOGICAL_AND([ false ], true));
      assertEqual({ }, LOGICAL_AND(true, { }));
      assertEqual(true, LOGICAL_AND({ }, true));
      assertEqual({ 'a' : true }, LOGICAL_AND(true, { 'a' : true }));
      assertEqual(true, LOGICAL_AND({ 'a' : true }, true));
      assertEqual({ 'a' : true, 'b' : false }, LOGICAL_AND(true, { 'a' : true, 'b' : false }));
      assertEqual(true, LOGICAL_AND({ 'a' : true, 'b' : false }, true));
      
      assertEqual(false, LOGICAL_AND(false, null));
      assertEqual(null, LOGICAL_AND(null, false));
      assertEqual(false, LOGICAL_AND(false, ''));
      assertEqual('', LOGICAL_AND('', false));
      assertEqual(false, LOGICAL_AND(false, ' '));
      assertEqual(false, LOGICAL_AND(' ', false));
      assertEqual(false, LOGICAL_AND(false, '0'));
      assertEqual(false, LOGICAL_AND('0', false));
      assertEqual(false, LOGICAL_AND(false, '1'));
      assertEqual(false, LOGICAL_AND('1', false));
      assertEqual(false, LOGICAL_AND(false, 'true'));
      assertEqual(false, LOGICAL_AND('true', false));
      assertEqual(false, LOGICAL_AND(false, 'false'));
      assertEqual(false, LOGICAL_AND('false', false));
      assertEqual(false, LOGICAL_AND(false, 0));
      assertEqual(0, LOGICAL_AND(0, false));
      assertEqual(false, LOGICAL_AND(false, 1));
      assertEqual(false, LOGICAL_AND(1, false));
      assertEqual(false, LOGICAL_AND(false, -1));
      assertEqual(false, LOGICAL_AND(-1, false));
      assertEqual(false, LOGICAL_AND(false, 1.1));
      assertEqual(false, LOGICAL_AND(1.1, false));
      assertEqual(false, LOGICAL_AND(false, [ ]));
      assertEqual(false, LOGICAL_AND([ ], false));
      assertEqual(false, LOGICAL_AND(false, [ 0 ]));
      assertEqual(false, LOGICAL_AND([ 0 ], false));
      assertEqual(false, LOGICAL_AND(false, [ 0, 1 ]));
      assertEqual(false, LOGICAL_AND([ 0, 1 ], false));
      assertEqual(false, LOGICAL_AND(false, [ true ]));
      assertEqual(true, LOGICAL_AND([ false ], true));
      assertEqual(false, LOGICAL_AND(false, [ false ]));
      assertEqual(false, LOGICAL_AND([ false ], false));
      assertEqual(false, LOGICAL_AND(false, { }));
      assertEqual(false, LOGICAL_AND({ }, false));
      assertEqual(false, LOGICAL_AND(false, { 'a' : true }));
      assertEqual(true, LOGICAL_AND({ 'a' : false }, true));
      assertEqual(false, LOGICAL_AND(false, { 'a' : true, 'b' : false }));
      assertEqual(true, LOGICAL_AND({ 'a' : false, 'b' : false }, true));
      assertEqual(NaN, LOGICAL_AND(NaN, NaN));
      assertEqual(NaN, LOGICAL_AND(NaN, 0));
      assertEqual(NaN, LOGICAL_AND(NaN, true));
      assertEqual(NaN, LOGICAL_AND(NaN, false));
      assertEqual(NaN, LOGICAL_AND(NaN, null));
      assertEqual(NaN, LOGICAL_AND(NaN, undefined));
      assertEqual(NaN, LOGICAL_AND(NaN, ''));
      assertEqual(NaN, LOGICAL_AND(NaN, '0'));
      assertEqual(0, LOGICAL_AND(0, NaN));
      assertEqual(NaN, LOGICAL_AND(true, NaN));
      assertEqual(false, LOGICAL_AND(false, NaN));
      assertEqual(null, LOGICAL_AND(null, NaN));
      assertEqual(undefined, LOGICAL_AND(undefined, NaN));
      assertEqual('', LOGICAL_AND('', NaN));
      assertEqual(NaN, LOGICAL_AND('0', NaN));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_AND function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalAndBool: function() {
      assertTrue(aql.LOGICAL_AND(function () { return true; }, function () { return true; }));
      assertFalse(aql.LOGICAL_AND(function () { return true; }, function () { return false; }));
      assertFalse(aql.LOGICAL_AND(function () { return false; }, function () { return true; }));
      assertFalse(aql.LOGICAL_AND(function () { return false; }, function () { return false; }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_OR function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalOrUndefined: function() {
      var LOGICAL_OR = function (a, b) {
        return aql.LOGICAL_OR(function () { return a; }, function () { return b; });
      };

      assertEqual(undefined, LOGICAL_OR(undefined, undefined));
      assertEqual(null, LOGICAL_OR(undefined, null));
      assertEqual(true, LOGICAL_OR(undefined, true));
      assertEqual(false, LOGICAL_OR(undefined, false));
      assertEqual(0.0, LOGICAL_OR(undefined, 0.0));
      assertEqual(1.0, LOGICAL_OR(undefined, 1.0));
      assertEqual(-1.0, LOGICAL_OR(undefined, -1.0));
      assertEqual('', LOGICAL_OR(undefined, ''));
      assertEqual('0', LOGICAL_OR(undefined, '0'));
      assertEqual('1', LOGICAL_OR(undefined, '1'));
      assertEqual([ ], LOGICAL_OR(undefined, [ ]));
      assertEqual([ 0 ], LOGICAL_OR(undefined, [ 0 ]));
      assertEqual([ 0, 1 ], LOGICAL_OR(undefined, [ 0, 1 ]));
      assertEqual([ 1, 2 ], LOGICAL_OR(undefined, [ 1, 2 ]));
      assertEqual({ }, LOGICAL_OR(undefined, { }));
      assertEqual({ 'a' : 0 }, LOGICAL_OR(undefined, { 'a' : 0 }));
      assertEqual({ 'a' : 1 }, LOGICAL_OR(undefined, { 'a' : 1 }));
      assertEqual({ '0' : false }, LOGICAL_OR(undefined, { '0' : false }));
      assertEqual(undefined, LOGICAL_OR(null, undefined));
      assertEqual(true, LOGICAL_OR(true, undefined));
      assertEqual(undefined, LOGICAL_OR(false, undefined));
      assertEqual(undefined, LOGICAL_OR(0.0, undefined));
      assertEqual(1.0, LOGICAL_OR(1.0, undefined));
      assertEqual(-1.0, LOGICAL_OR(-1.0, undefined));
      assertEqual(undefined, LOGICAL_OR('', undefined));
      assertEqual('0', LOGICAL_OR('0', undefined));
      assertEqual('1', LOGICAL_OR('1', undefined));
      assertEqual([ ], LOGICAL_OR([ ], undefined));
      assertEqual([ 0 ], LOGICAL_OR([ 0 ], undefined));
      assertEqual([ 0, 1 ], LOGICAL_OR([ 0, 1 ], undefined));
      assertEqual([ 1, 2 ], LOGICAL_OR([ 1, 2 ], undefined));
      assertEqual({ }, LOGICAL_OR({ }, undefined));
      assertEqual({ 'a' : 0 }, LOGICAL_OR({ 'a' : 0 }, undefined));
      assertEqual({ 'a' : 1 }, LOGICAL_OR({ 'a' : 1 }, undefined));
      assertEqual({ '0' : false }, LOGICAL_OR({ '0' : false }, undefined));
      
      assertEqual(true, LOGICAL_OR(true, null));
      assertEqual(true, LOGICAL_OR(null, true));
      assertEqual(true, LOGICAL_OR(true, ''));
      assertEqual(true, LOGICAL_OR('', true));
      assertEqual(true, LOGICAL_OR(true, ' '));
      assertEqual(' ', LOGICAL_OR(' ', true));
      assertEqual(true, LOGICAL_OR(true, '0'));
      assertEqual('0', LOGICAL_OR('0', true));
      assertEqual(true, LOGICAL_OR(true, '1'));
      assertEqual('1', LOGICAL_OR('1', true));
      assertEqual(true, LOGICAL_OR(true, 'true'));
      assertEqual('true', LOGICAL_OR('true', true));
      assertEqual(true, LOGICAL_OR(true, 'false'));
      assertEqual('false', LOGICAL_OR('false', true));
      assertEqual(true, LOGICAL_OR(true, 0));
      assertEqual(true, LOGICAL_OR(0, true));
      assertEqual(true, LOGICAL_OR(true, 1));
      assertEqual(1, LOGICAL_OR(1, true));
      assertEqual(true, LOGICAL_OR(true, -1));
      assertEqual(-1, LOGICAL_OR(-1, true));
      assertEqual(true, LOGICAL_OR(true, 1.1));
      assertEqual(1.1, LOGICAL_OR(1.1, true));
      assertEqual(true, LOGICAL_OR(true, [ ]));
      assertEqual([ ], LOGICAL_OR([ ], true));
      assertEqual(true, LOGICAL_OR(true, [ 0 ]));
      assertEqual([ 0 ], LOGICAL_OR([ 0 ], true));
      assertEqual(true, LOGICAL_OR(true, [ 0, 1 ]));
      assertEqual([ 0, 1 ], LOGICAL_OR([ 0, 1 ], true));
      assertEqual(true, LOGICAL_OR(true, [ true ]));
      assertEqual([ true ], LOGICAL_OR([ true ], true));
      assertEqual(true, LOGICAL_OR(true, [ false ]));
      assertEqual([ false ], LOGICAL_OR([ false ], true));
      assertEqual(true, LOGICAL_OR(true, { }));
      assertEqual({ }, LOGICAL_OR({ }, true));
      assertEqual(true, LOGICAL_OR(true, { 'a' : true }));
      assertEqual({ 'a' : true }, LOGICAL_OR({ 'a' : true }, true));
      assertEqual(true, LOGICAL_OR(true, { 'a' : true, 'b' : false }));
      assertEqual({ 'a' : true, 'b' : false }, LOGICAL_OR({ 'a' : true, 'b' : false }, true));
      
      assertEqual(null, LOGICAL_OR(false, null));
      assertEqual(false, LOGICAL_OR(null, false));
      assertEqual('', LOGICAL_OR(false, ''));
      assertEqual(false, LOGICAL_OR('', false));
      assertEqual(' ', LOGICAL_OR(false, ' '));
      assertEqual(' ', LOGICAL_OR(' ', false));
      assertEqual('0', LOGICAL_OR(false, '0'));
      assertEqual('0', LOGICAL_OR('0', false));
      assertEqual('1', LOGICAL_OR(false, '1'));
      assertEqual('1', LOGICAL_OR('1', false));
      assertEqual('true', LOGICAL_OR(false, 'true'));
      assertEqual('true', LOGICAL_OR('true', false));
      assertEqual('false', LOGICAL_OR(false, 'false'));
      assertEqual('false', LOGICAL_OR('false', false));
      assertEqual(0, LOGICAL_OR(false, 0));
      assertEqual(false, LOGICAL_OR(0, false));
      assertEqual(1, LOGICAL_OR(false, 1));
      assertEqual(1, LOGICAL_OR(1, false));
      assertEqual(-1, LOGICAL_OR(false, -1));
      assertEqual(-1, LOGICAL_OR(-1, false));
      assertEqual(1.1, LOGICAL_OR(false, 1.1));
      assertEqual(1.1, LOGICAL_OR(1.1, false));
      assertEqual([ ], LOGICAL_OR(false, [ ]));
      assertEqual([ ], LOGICAL_OR([ ], false));
      assertEqual([ 0 ], LOGICAL_OR(false, [ 0 ]));
      assertEqual([ 0 ], LOGICAL_OR([ 0 ], false));
      assertEqual([ 0, 1 ], LOGICAL_OR(false, [ 0, 1 ]));
      assertEqual([ 0, 1 ], LOGICAL_OR([ 0, 1 ], false));
      assertEqual([ true ], LOGICAL_OR(false, [ true ]));
      assertEqual([ false ], LOGICAL_OR([ false ], true));
      assertEqual([ false ], LOGICAL_OR(false, [ false ]));
      assertEqual([ false ], LOGICAL_OR([ false ], false));
      assertEqual({ }, LOGICAL_OR(false, { }));
      assertEqual({ }, LOGICAL_OR({ }, false));
      assertEqual({ 'a' : true }, LOGICAL_OR(false, { 'a' : true }));
      assertEqual({ 'a' : false }, LOGICAL_OR({ 'a' : false }, true));
      assertEqual({ 'a' : true, 'b' : false }, LOGICAL_OR(false, { 'a' : true, 'b' : false }));
      assertEqual({ 'a' : false, 'b' : false }, LOGICAL_OR({ 'a' : false, 'b' : false }, true));
      assertEqual(NaN, LOGICAL_OR(NaN, NaN));
      assertEqual(0, LOGICAL_OR(NaN, 0));
      assertEqual(true, LOGICAL_OR(NaN, true));
      assertEqual(false, LOGICAL_OR(NaN, false));
      assertEqual(null, LOGICAL_OR(NaN, null));
      assertEqual(undefined, LOGICAL_OR(NaN, undefined));
      assertEqual('', LOGICAL_OR(NaN, ''));
      assertEqual('0', LOGICAL_OR(NaN, '0'));
      assertEqual(NaN, LOGICAL_OR(0, NaN));
      assertEqual(true, LOGICAL_OR(true, NaN));
      assertEqual(NaN, LOGICAL_OR(false, NaN));
      assertEqual(NaN, LOGICAL_OR(null, NaN));
      assertEqual(NaN, LOGICAL_OR(undefined, NaN));
      assertEqual(NaN, LOGICAL_OR('', NaN));
      assertEqual('0', LOGICAL_OR('0', NaN));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_OR function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalOrBool: function() {
      assertTrue(aql.LOGICAL_OR(function () { return true; }, function () { return true; }));
      assertTrue(aql.LOGICAL_OR(function () { return true; }, function () { return false; }));
      assertTrue(aql.LOGICAL_OR(function () { return false; }, function () { return true; }));
      assertFalse(aql.LOGICAL_OR(function () { return false; }, function () { return false; }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_NOT function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalNotUndefined: function() {
      assertEqual(true, aql.LOGICAL_NOT(undefined));
      assertEqual(true, aql.LOGICAL_NOT(null));
      assertEqual(true, aql.LOGICAL_NOT(false));
      assertEqual(false, aql.LOGICAL_NOT(true));
      assertEqual(true, aql.LOGICAL_NOT(0.0));
      assertEqual(false, aql.LOGICAL_NOT(1.0));
      assertEqual(false, aql.LOGICAL_NOT(-1.0));
      assertEqual(true, aql.LOGICAL_NOT(''));
      assertEqual(false, aql.LOGICAL_NOT('0'));
      assertEqual(false, aql.LOGICAL_NOT('1'));
      assertEqual(false, aql.LOGICAL_NOT([ ]));
      assertEqual(false, aql.LOGICAL_NOT([ 0 ]));
      assertEqual(false, aql.LOGICAL_NOT([ 0, 1 ]));
      assertEqual(false, aql.LOGICAL_NOT([ 1, 2 ]));
      assertEqual(false, aql.LOGICAL_NOT({ }));
      assertEqual(false, aql.LOGICAL_NOT({ 'a' : 0 }));
      assertEqual(false, aql.LOGICAL_NOT({ 'a' : 1 }));
      assertEqual(false, aql.LOGICAL_NOT({ '0' : false}));
      assertEqual(true, aql.LOGICAL_NOT(NaN));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_NOT function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalNotBool: function() {
      assertTrue(aql.LOGICAL_NOT(false));
      assertFalse(aql.LOGICAL_NOT(true));

      assertTrue(aql.LOGICAL_NOT(aql.LOGICAL_NOT(true)));
      assertFalse(aql.LOGICAL_NOT(aql.LOGICAL_NOT(false)));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_EQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalEqualTrue: function() {
      assertTrue(aql.RELATIONAL_EQUAL(undefined, undefined));
      assertTrue(aql.RELATIONAL_EQUAL(undefined, null));
      assertTrue(aql.RELATIONAL_EQUAL(null, undefined));
      assertTrue(aql.RELATIONAL_EQUAL(null, null));
      assertTrue(aql.RELATIONAL_EQUAL(NaN, NaN));
      assertTrue(aql.RELATIONAL_EQUAL(NaN, null));
      assertTrue(aql.RELATIONAL_EQUAL(NaN, undefined));
      assertTrue(aql.RELATIONAL_EQUAL(undefined, NaN));
      assertTrue(aql.RELATIONAL_EQUAL(null, NaN));
      assertTrue(aql.RELATIONAL_EQUAL(1, 1));
      assertTrue(aql.RELATIONAL_EQUAL(0, 0));
      assertTrue(aql.RELATIONAL_EQUAL(-1, -1));
      assertTrue(aql.RELATIONAL_EQUAL(1.345, 1.345));
      assertTrue(aql.RELATIONAL_EQUAL(1.0, 1.00));
      assertTrue(aql.RELATIONAL_EQUAL(1.0, 1.000));
      assertTrue(aql.RELATIONAL_EQUAL(1.1, 1.1));
      assertTrue(aql.RELATIONAL_EQUAL(1.01, 1.01));
      assertTrue(aql.RELATIONAL_EQUAL(1.001, 1.001));
      assertTrue(aql.RELATIONAL_EQUAL(1.0001, 1.0001));
      assertTrue(aql.RELATIONAL_EQUAL(1.00001, 1.00001));
      assertTrue(aql.RELATIONAL_EQUAL(1.000001, 1.000001));
      assertTrue(aql.RELATIONAL_EQUAL(1.245e307, 1.245e307));
      assertTrue(aql.RELATIONAL_EQUAL(-99.43423, -99.43423));
      assertTrue(aql.RELATIONAL_EQUAL(true, true));
      assertTrue(aql.RELATIONAL_EQUAL(false, false));
      assertTrue(aql.RELATIONAL_EQUAL('', ''));
      assertTrue(aql.RELATIONAL_EQUAL(' ', ' '));
      assertTrue(aql.RELATIONAL_EQUAL(' 1', ' 1'));
      assertTrue(aql.RELATIONAL_EQUAL('0', '0'));
      assertTrue(aql.RELATIONAL_EQUAL('abc', 'abc'));
      assertTrue(aql.RELATIONAL_EQUAL('-1', '-1'));
      assertTrue(aql.RELATIONAL_EQUAL('true', 'true'));
      assertTrue(aql.RELATIONAL_EQUAL('false', 'false'));
      assertTrue(aql.RELATIONAL_EQUAL('undefined', 'undefined'));
      assertTrue(aql.RELATIONAL_EQUAL('null', 'null'));
      assertTrue(aql.RELATIONAL_EQUAL([ ], [ ]));
      assertTrue(aql.RELATIONAL_EQUAL([ null ], [ ]));
      assertTrue(aql.RELATIONAL_EQUAL([ ], [ null ]));
      assertTrue(aql.RELATIONAL_EQUAL([ 0 ], [ 0 ]));
      assertTrue(aql.RELATIONAL_EQUAL([ 0, 1 ], [ 0, 1 ]));
      assertTrue(aql.RELATIONAL_EQUAL([ 0, 1, 4 ], [ 0, 1, 4 ]));
      assertTrue(aql.RELATIONAL_EQUAL([ 3, 4, -99 ], [ 3, 4, -99 ]));
      assertTrue(aql.RELATIONAL_EQUAL([ 'a', 4, [ 1, 'a' ], false ], [ 'a', 4, [ 1, 'a' ], false ]));
      assertTrue(aql.RELATIONAL_EQUAL({ }, { }));
      assertTrue(aql.RELATIONAL_EQUAL({ 'a' : true }, { 'a' : true }));
      assertTrue(aql.RELATIONAL_EQUAL({ 'a' : true, 'b': true }, { 'a' : true, 'b': true }));
      assertTrue(aql.RELATIONAL_EQUAL({ 'a' : true, 'b': true }, { 'b' : true, 'a': true }));
      assertTrue(aql.RELATIONAL_EQUAL({ 'b' : true, 'a': true }, { 'b' : true, 'a': true }));
      assertTrue(aql.RELATIONAL_EQUAL({ 'b' : true, 'a': true }, { 'a' : true, 'b': true }));
      assertTrue(aql.RELATIONAL_EQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }));
      assertTrue(aql.RELATIONAL_EQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'b' : [ 1, 9 ], 'a' : [ 0, 1 ] }));
      assertTrue(aql.RELATIONAL_EQUAL({ 'f' : { 'c' : { 'd' : [ 0, 1 ], 'a' : [ 1, 9 ] }, 'a' : false }, 'a' : true }, { 'a' : true, 'f' : { 'a' : false, 'c' : { 'a' : [ 1, 9 ], 'd' : [ 0, 1 ] } } }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_EQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalEqualFalse: function() {
      assertFalse(aql.RELATIONAL_EQUAL(undefined, true));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, false));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, 0.0));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, 1.0));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, -1.0));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, ''));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, '0'));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, '1'));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, [ ]));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, [ 0 ]));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, [ 0, 1 ]));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, [ 1, 2 ]));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, { }));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, { 'a' : 0 }));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, { 'a' : 1 }));
      assertFalse(aql.RELATIONAL_EQUAL(undefined, { '0' : false }));
      assertFalse(aql.RELATIONAL_EQUAL(true, undefined));
      assertFalse(aql.RELATIONAL_EQUAL(false, undefined));
      assertFalse(aql.RELATIONAL_EQUAL(0.0, undefined));
      assertFalse(aql.RELATIONAL_EQUAL(1.0, undefined));
      assertFalse(aql.RELATIONAL_EQUAL(-1.0, undefined));
      assertFalse(aql.RELATIONAL_EQUAL('', undefined));
      assertFalse(aql.RELATIONAL_EQUAL('0', undefined));
      assertFalse(aql.RELATIONAL_EQUAL('1', undefined));
      assertFalse(aql.RELATIONAL_EQUAL([ ], undefined));
      assertFalse(aql.RELATIONAL_EQUAL([ 0 ], undefined));
      assertFalse(aql.RELATIONAL_EQUAL([ 0, 1 ], undefined));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, 2 ], undefined));
      assertFalse(aql.RELATIONAL_EQUAL([ ], null));
      assertFalse(aql.RELATIONAL_EQUAL({ }, undefined));
      assertFalse(aql.RELATIONAL_EQUAL({ 'a' : 0 }, undefined));
      assertFalse(aql.RELATIONAL_EQUAL({ 'a' : 1 }, undefined));
      assertFalse(aql.RELATIONAL_EQUAL({ '0' : false }, undefined));
      assertFalse(aql.RELATIONAL_EQUAL(1, 0));
      assertFalse(aql.RELATIONAL_EQUAL(0, 1));
      assertFalse(aql.RELATIONAL_EQUAL(0, false));
      assertFalse(aql.RELATIONAL_EQUAL(false, 0));
      assertFalse(aql.RELATIONAL_EQUAL(false, 0));
      assertFalse(aql.RELATIONAL_EQUAL(-1, 1));
      assertFalse(aql.RELATIONAL_EQUAL(1, -1));
      assertFalse(aql.RELATIONAL_EQUAL(-1, 0));
      assertFalse(aql.RELATIONAL_EQUAL(0, -1));
      assertFalse(aql.RELATIONAL_EQUAL(true, false));
      assertFalse(aql.RELATIONAL_EQUAL(false, true));
      assertFalse(aql.RELATIONAL_EQUAL(true, 1));
      assertFalse(aql.RELATIONAL_EQUAL(1, true));
      assertFalse(aql.RELATIONAL_EQUAL(0, true));
      assertFalse(aql.RELATIONAL_EQUAL(true, 0));
      assertFalse(aql.RELATIONAL_EQUAL(true, 'true'));
      assertFalse(aql.RELATIONAL_EQUAL(false, 'false'));
      assertFalse(aql.RELATIONAL_EQUAL('true', true));
      assertFalse(aql.RELATIONAL_EQUAL('false', false));
      assertFalse(aql.RELATIONAL_EQUAL(-1.345, 1.345));
      assertFalse(aql.RELATIONAL_EQUAL(1.345, -1.345));
      assertFalse(aql.RELATIONAL_EQUAL(1.345, 1.346));
      assertFalse(aql.RELATIONAL_EQUAL(1.346, 1.345));
      assertFalse(aql.RELATIONAL_EQUAL(1.344, 1.345));
      assertFalse(aql.RELATIONAL_EQUAL(1.345, 1.344));
      assertFalse(aql.RELATIONAL_EQUAL(1, 2));
      assertFalse(aql.RELATIONAL_EQUAL(2, 1));
      assertFalse(aql.RELATIONAL_EQUAL(1.246e307, 1.245e307));
      assertFalse(aql.RELATIONAL_EQUAL(1.246e307, 1.247e307));
      assertFalse(aql.RELATIONAL_EQUAL(1.246e307, 1.2467e307));
      assertFalse(aql.RELATIONAL_EQUAL(-99.43423, -99.434233));
      assertFalse(aql.RELATIONAL_EQUAL(1.00001, 1.000001));
      assertFalse(aql.RELATIONAL_EQUAL(1.00001, 1.0001));
      assertFalse(aql.RELATIONAL_EQUAL(null, 1));
      assertFalse(aql.RELATIONAL_EQUAL(1, null));
      assertFalse(aql.RELATIONAL_EQUAL(null, 0));
      assertFalse(aql.RELATIONAL_EQUAL(0, null));
      assertFalse(aql.RELATIONAL_EQUAL(null, ''));
      assertFalse(aql.RELATIONAL_EQUAL('', null));
      assertFalse(aql.RELATIONAL_EQUAL(null, '0'));
      assertFalse(aql.RELATIONAL_EQUAL('0', null));
      assertFalse(aql.RELATIONAL_EQUAL(null, false));
      assertFalse(aql.RELATIONAL_EQUAL(false, null));
      assertFalse(aql.RELATIONAL_EQUAL(null, true));
      assertFalse(aql.RELATIONAL_EQUAL(true, null));
      assertFalse(aql.RELATIONAL_EQUAL(null, 'null'));
      assertFalse(aql.RELATIONAL_EQUAL('null', null));
      assertFalse(aql.RELATIONAL_EQUAL(0, ''));
      assertFalse(aql.RELATIONAL_EQUAL('', 0));
      assertFalse(aql.RELATIONAL_EQUAL(1, ''));
      assertFalse(aql.RELATIONAL_EQUAL('', 1));
      assertFalse(aql.RELATIONAL_EQUAL(' ', ''));
      assertFalse(aql.RELATIONAL_EQUAL('', ' '));
      assertFalse(aql.RELATIONAL_EQUAL(' 1', '1'));
      assertFalse(aql.RELATIONAL_EQUAL('1', ' 1'));
      assertFalse(aql.RELATIONAL_EQUAL('1 ', '1'));
      assertFalse(aql.RELATIONAL_EQUAL('1', '1 '));
      assertFalse(aql.RELATIONAL_EQUAL('1 ', ' 1'));
      assertFalse(aql.RELATIONAL_EQUAL(' 1', '1 '));
      assertFalse(aql.RELATIONAL_EQUAL(' 1 ', '1'));
      assertFalse(aql.RELATIONAL_EQUAL('0', ''));
      assertFalse(aql.RELATIONAL_EQUAL('', ' '));
      assertFalse(aql.RELATIONAL_EQUAL('abc', 'abcd'));
      assertFalse(aql.RELATIONAL_EQUAL('abcd', 'abc'));
      assertFalse(aql.RELATIONAL_EQUAL('dabc', 'abcd'));
      assertFalse(aql.RELATIONAL_EQUAL('1', 1));
      assertFalse(aql.RELATIONAL_EQUAL(1, '1'));
      assertFalse(aql.RELATIONAL_EQUAL('0', 0));
      assertFalse(aql.RELATIONAL_EQUAL('1.0', 1.0));
      assertFalse(aql.RELATIONAL_EQUAL('1.0', 1));
      assertFalse(aql.RELATIONAL_EQUAL('-1', -1));
      assertFalse(aql.RELATIONAL_EQUAL('1.234', 1.234));
      assertFalse(aql.RELATIONAL_EQUAL('NaN', NaN));
      assertFalse(aql.RELATIONAL_EQUAL([ 0 ], [ ]));
      assertFalse(aql.RELATIONAL_EQUAL([ ], [ 0 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ ], [ 0, 1 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 0 ], [ 0, 1 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, 1, 0 ], [ 1, 0, 1 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, 0, 0 ], [ 1, 0, 1 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, 0, 0 ], [ 1, 0 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, 0 ], [ 1, 0, 0 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, 0 ], [ 0, 1 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, 0 ], [ 0 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, 0 ], [ 1 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, 2, 3 ], [ 3, 2, 1 ]));
      assertFalse(aql.RELATIONAL_EQUAL([ [ 1 ] ], [ [ 0 ] ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, [ 1 , 0 ] ], [ 1, [ 0, 1 ] ]));
      assertFalse(aql.RELATIONAL_EQUAL([ 1, [ 1 , 0, [ ] ] ], [ 1, [ [ ], 1, 0 ] ]));
      assertFalse(aql.RELATIONAL_EQUAL([ '' ], false));
      assertFalse(aql.RELATIONAL_EQUAL([ '' ], ''));
      assertFalse(aql.RELATIONAL_EQUAL([ '' ], [ ]));
      assertFalse(aql.RELATIONAL_EQUAL([ true ], [ ]));
      assertFalse(aql.RELATIONAL_EQUAL([ true ], [ false ]));
      assertFalse(aql.RELATIONAL_EQUAL([ false ], [ ]));
      assertFalse(aql.RELATIONAL_EQUAL([ null ], [ false ]));
      assertFalse(aql.RELATIONAL_EQUAL([ ], ''));
      assertFalse(aql.RELATIONAL_EQUAL({ }, { 'a' : false }));
      assertFalse(aql.RELATIONAL_EQUAL({ 'a' : false }, { }));
      assertFalse(aql.RELATIONAL_EQUAL({ 'a' : true }, { 'a' : false }));
      assertFalse(aql.RELATIONAL_EQUAL({ 'a' : true }, { 'b' : true }));
      assertFalse(aql.RELATIONAL_EQUAL({ 'b' : true }, { 'a' : true }));
      assertFalse(aql.RELATIONAL_EQUAL({ 'a' : true, 'b' : [ 0, 1 ] }, { 'a' : true, 'b' : [ 1, 0 ] }));
      assertFalse(aql.RELATIONAL_EQUAL({ 'a' : true, 'b' : { 'a' : false, 'b' : true } }, { 'a' : true, 'b' : { 'a' : true, 'b': true } }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_UNEQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalUnequalTrue: function() {
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, true));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, false));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, 0.0));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, 1.0));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, -1.0));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, ''));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, '0'));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, '1'));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, [ ]));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, [ 0 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, [ 0, 1 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, [ 1, 2 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, { }));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, { 'a' : 0 }));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, { 'a' : 1 }));
      assertTrue(aql.RELATIONAL_UNEQUAL(undefined, { '0' : false }));
      assertTrue(aql.RELATIONAL_UNEQUAL(true, undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL(false, undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL(0.0, undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.0, undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL(-1.0, undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL('', undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL('0', undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL('1', undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL([ ], undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 0 ], undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 0, 1 ], undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, 2 ], undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL({ }, undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL({ 'a' : 0 }, undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL({ 'a' : 1 }, undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL({ '0' : false }, undefined));
      assertTrue(aql.RELATIONAL_UNEQUAL(NaN, false));
      assertTrue(aql.RELATIONAL_UNEQUAL(NaN, true));
      assertTrue(aql.RELATIONAL_UNEQUAL(NaN, ''));
      assertTrue(aql.RELATIONAL_UNEQUAL(NaN, 0));
      assertTrue(aql.RELATIONAL_UNEQUAL(false, NaN));
      assertTrue(aql.RELATIONAL_UNEQUAL(true, NaN));
      assertTrue(aql.RELATIONAL_UNEQUAL('', NaN));
      assertTrue(aql.RELATIONAL_UNEQUAL(0, NaN));

      assertTrue(aql.RELATIONAL_UNEQUAL(1, 0));
      assertTrue(aql.RELATIONAL_UNEQUAL(0, 1));
      assertTrue(aql.RELATIONAL_UNEQUAL(0, false));
      assertTrue(aql.RELATIONAL_UNEQUAL(false, 0));
      assertTrue(aql.RELATIONAL_UNEQUAL(false, 0));
      assertTrue(aql.RELATIONAL_UNEQUAL(-1, 1));
      assertTrue(aql.RELATIONAL_UNEQUAL(1, -1));
      assertTrue(aql.RELATIONAL_UNEQUAL(-1, 0));
      assertTrue(aql.RELATIONAL_UNEQUAL(0, -1));
      assertTrue(aql.RELATIONAL_UNEQUAL(true, false));
      assertTrue(aql.RELATIONAL_UNEQUAL(false, true));
      assertTrue(aql.RELATIONAL_UNEQUAL(true, 1));
      assertTrue(aql.RELATIONAL_UNEQUAL(1, true));
      assertTrue(aql.RELATIONAL_UNEQUAL(0, true));
      assertTrue(aql.RELATIONAL_UNEQUAL(true, 0));
      assertTrue(aql.RELATIONAL_UNEQUAL(true, 'true'));
      assertTrue(aql.RELATIONAL_UNEQUAL(false, 'false'));
      assertTrue(aql.RELATIONAL_UNEQUAL('true', true));
      assertTrue(aql.RELATIONAL_UNEQUAL('false', false));
      assertTrue(aql.RELATIONAL_UNEQUAL(-1.345, 1.345));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.345, -1.345));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.345, 1.346));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.346, 1.345));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.344, 1.345));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.345, 1.344));
      assertTrue(aql.RELATIONAL_UNEQUAL(1, 2));
      assertTrue(aql.RELATIONAL_UNEQUAL(2, 1));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.246e307, 1.245e307));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.246e307, 1.247e307));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.246e307, 1.2467e307));
      assertTrue(aql.RELATIONAL_UNEQUAL(-99.43423, -99.434233));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.00001, 1.000001));
      assertTrue(aql.RELATIONAL_UNEQUAL(1.00001, 1.0001));
      assertTrue(aql.RELATIONAL_UNEQUAL(null, 1));
      assertTrue(aql.RELATIONAL_UNEQUAL(1, null));
      assertTrue(aql.RELATIONAL_UNEQUAL(null, 0));
      assertTrue(aql.RELATIONAL_UNEQUAL(0, null));
      assertTrue(aql.RELATIONAL_UNEQUAL(null, ''));
      assertTrue(aql.RELATIONAL_UNEQUAL('', null));
      assertTrue(aql.RELATIONAL_UNEQUAL(null, '0'));
      assertTrue(aql.RELATIONAL_UNEQUAL('0', null));
      assertTrue(aql.RELATIONAL_UNEQUAL(null, false));
      assertTrue(aql.RELATIONAL_UNEQUAL(false, null));
      assertTrue(aql.RELATIONAL_UNEQUAL(null, true));
      assertTrue(aql.RELATIONAL_UNEQUAL(true, null));
      assertTrue(aql.RELATIONAL_UNEQUAL(null, 'null'));
      assertTrue(aql.RELATIONAL_UNEQUAL('null', null));
      assertTrue(aql.RELATIONAL_UNEQUAL(0, ''));
      assertTrue(aql.RELATIONAL_UNEQUAL('', 0));
      assertTrue(aql.RELATIONAL_UNEQUAL(1, ''));
      assertTrue(aql.RELATIONAL_UNEQUAL('', 1));
      assertTrue(aql.RELATIONAL_UNEQUAL(' ', ''));
      assertTrue(aql.RELATIONAL_UNEQUAL('', ' '));
      assertTrue(aql.RELATIONAL_UNEQUAL(' 1', '1'));
      assertTrue(aql.RELATIONAL_UNEQUAL('1', ' 1'));
      assertTrue(aql.RELATIONAL_UNEQUAL('1 ', '1'));
      assertTrue(aql.RELATIONAL_UNEQUAL('1', '1 '));
      assertTrue(aql.RELATIONAL_UNEQUAL('1 ', ' 1'));
      assertTrue(aql.RELATIONAL_UNEQUAL(' 1', '1 '));
      assertTrue(aql.RELATIONAL_UNEQUAL(' 1 ', '1'));
      assertTrue(aql.RELATIONAL_UNEQUAL('0', ''));
      assertTrue(aql.RELATIONAL_UNEQUAL('', ' '));
      assertTrue(aql.RELATIONAL_UNEQUAL('abc', 'abcd'));
      assertTrue(aql.RELATIONAL_UNEQUAL('abcd', 'abc'));
      assertTrue(aql.RELATIONAL_UNEQUAL('dabc', 'abcd'));
      assertTrue(aql.RELATIONAL_UNEQUAL('1', 1));
      assertTrue(aql.RELATIONAL_UNEQUAL(1, '1'));
      assertTrue(aql.RELATIONAL_UNEQUAL('0', 0));
      assertTrue(aql.RELATIONAL_UNEQUAL('1.0', 1.0));
      assertTrue(aql.RELATIONAL_UNEQUAL('1.0', 1));
      assertTrue(aql.RELATIONAL_UNEQUAL('-1', -1));
      assertTrue(aql.RELATIONAL_UNEQUAL('1.234', 1.234));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 0 ], [ ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ ], [ 0 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ ], [ 0, 1 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 0 ], [ 0, 1 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, 1, 0 ], [ 1, 0, 1 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, 0, 0 ], [ 1, 0, 1 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, 0, 0 ], [ 1, 0 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, 0 ], [ 1, 0, 0 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, 0 ], [ 0, 1 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, 0 ], [ 0 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, 0 ], [ 1 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, 2, 3 ], [ 3, 2, 1 ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ [ 1 ] ], [ [ 0 ] ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, [ 1 , 0 ] ], [ 1, [ 0, 1 ] ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ 1, [ 1 , 0, [ ] ] ], [ 1, [ [ ], 1, 0 ] ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ '' ], false));
      assertTrue(aql.RELATIONAL_UNEQUAL([ '' ], ''));
      assertTrue(aql.RELATIONAL_UNEQUAL([ '' ], [ ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ true ], [ ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ true ], [ false ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ false ], [ ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ null ], [ false ]));
      assertTrue(aql.RELATIONAL_UNEQUAL([ ], null));
      assertTrue(aql.RELATIONAL_UNEQUAL([ ], ''));
      assertTrue(aql.RELATIONAL_UNEQUAL({ }, { 'a' : false }));
      assertTrue(aql.RELATIONAL_UNEQUAL({ 'a' : false }, { }));
      assertTrue(aql.RELATIONAL_UNEQUAL({ 'a' : true }, { 'a' : false }));
      assertTrue(aql.RELATIONAL_UNEQUAL({ 'a' : true }, { 'b' : true }));
      assertTrue(aql.RELATIONAL_UNEQUAL({ 'b' : true }, { 'a' : true }));
      assertTrue(aql.RELATIONAL_UNEQUAL({ 'a' : true, 'b' : [ 0, 1 ] }, { 'a' : true, 'b' : [ 1, 0 ] }));
      assertTrue(aql.RELATIONAL_UNEQUAL({ 'a' : true, 'b' : { 'a' : false, 'b' : true } }, { 'a' : true, 'b' : { 'a' : true, 'b': true } }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_UNEQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalUnequalFalse: function() {
      assertFalse(aql.RELATIONAL_UNEQUAL(1, 1));
      assertFalse(aql.RELATIONAL_UNEQUAL(0, 0));
      assertFalse(aql.RELATIONAL_UNEQUAL(-1, -1));
      assertFalse(aql.RELATIONAL_UNEQUAL(1.345, 1.345));
      assertFalse(aql.RELATIONAL_UNEQUAL(1.0, 1.00));
      assertFalse(aql.RELATIONAL_UNEQUAL(1.0, 1.000));
      assertFalse(aql.RELATIONAL_UNEQUAL(1.1, 1.1));
      assertFalse(aql.RELATIONAL_UNEQUAL(1.01, 1.01));
      assertFalse(aql.RELATIONAL_UNEQUAL(1.001, 1.001));
      assertFalse(aql.RELATIONAL_UNEQUAL(1.0001, 1.0001));
      assertFalse(aql.RELATIONAL_UNEQUAL(1.00001, 1.00001));
      assertFalse(aql.RELATIONAL_UNEQUAL(1.000001, 1.000001));
      assertFalse(aql.RELATIONAL_UNEQUAL(1.245e307, 1.245e307));
      assertFalse(aql.RELATIONAL_UNEQUAL(-99.43423, -99.43423));
      assertFalse(aql.RELATIONAL_UNEQUAL(true, true));
      assertFalse(aql.RELATIONAL_UNEQUAL(false, false));
      assertFalse(aql.RELATIONAL_UNEQUAL('', ''));
      assertFalse(aql.RELATIONAL_UNEQUAL(' ', ' '));
      assertFalse(aql.RELATIONAL_UNEQUAL(' 1', ' 1'));
      assertFalse(aql.RELATIONAL_UNEQUAL('0', '0'));
      assertFalse(aql.RELATIONAL_UNEQUAL('abc', 'abc'));
      assertFalse(aql.RELATIONAL_UNEQUAL('-1', '-1'));
      assertFalse(aql.RELATIONAL_UNEQUAL(null, null));
      assertFalse(aql.RELATIONAL_UNEQUAL(null, undefined));
      assertFalse(aql.RELATIONAL_UNEQUAL(null, NaN));
      assertFalse(aql.RELATIONAL_UNEQUAL(undefined, NaN));
      assertFalse(aql.RELATIONAL_UNEQUAL(NaN, null));
      assertFalse(aql.RELATIONAL_UNEQUAL(NaN, undefined));
      assertFalse(aql.RELATIONAL_UNEQUAL(undefined, undefined));
      assertFalse(aql.RELATIONAL_UNEQUAL('true', 'true'));
      assertFalse(aql.RELATIONAL_UNEQUAL('false', 'false'));
      assertFalse(aql.RELATIONAL_UNEQUAL('undefined', 'undefined'));
      assertFalse(aql.RELATIONAL_UNEQUAL('null', 'null'));
      assertFalse(aql.RELATIONAL_UNEQUAL([ ], [ ]));
      assertFalse(aql.RELATIONAL_UNEQUAL([ null ], [ ]));
      assertFalse(aql.RELATIONAL_UNEQUAL([ ], [ null ]));
      assertFalse(aql.RELATIONAL_UNEQUAL([ 0 ], [ 0 ]));
      assertFalse(aql.RELATIONAL_UNEQUAL([ 0, 1 ], [ 0, 1 ]));
      assertFalse(aql.RELATIONAL_UNEQUAL([ 0, 1, 4 ], [ 0, 1, 4 ]));
      assertFalse(aql.RELATIONAL_UNEQUAL([ 3, 4, -99 ], [ 3, 4, -99 ]));
      assertFalse(aql.RELATIONAL_UNEQUAL([ 'a', 4, [ 1, 'a' ], false ], [ 'a', 4, [ 1, 'a' ], false ]));
      assertFalse(aql.RELATIONAL_UNEQUAL({ }, { }));
      assertFalse(aql.RELATIONAL_UNEQUAL({ 'a' : true }, { 'a' : true }));
      assertFalse(aql.RELATIONAL_UNEQUAL({ 'a' : true, 'b': true }, { 'a' : true, 'b': true }));
      assertFalse(aql.RELATIONAL_UNEQUAL({ 'a' : true, 'b': true }, { 'b' : true, 'a': true }));
      assertFalse(aql.RELATIONAL_UNEQUAL({ 'b' : true, 'a': true }, { 'b' : true, 'a': true }));
      assertFalse(aql.RELATIONAL_UNEQUAL({ 'b' : true, 'a': true }, { 'a' : true, 'b': true }));
      assertFalse(aql.RELATIONAL_UNEQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }));
      assertFalse(aql.RELATIONAL_UNEQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'b' : [ 1, 9 ], 'a' : [ 0, 1 ] }));
      assertFalse(aql.RELATIONAL_UNEQUAL({ 'f' : { 'c' : { 'd' : [ 0, 1 ], 'a' : [ 1, 9 ] }, 'a' : false }, 'a' : true }, { 'a' : true, 'f' : { 'a' : false, 'c' : { 'a' : [ 1, 9 ], 'd' : [ 0, 1 ] } } }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_LESS function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalLessTrue: function() {
      assertTrue(aql.RELATIONAL_LESS(NaN, false));
      assertTrue(aql.RELATIONAL_LESS(NaN, true));
      assertTrue(aql.RELATIONAL_LESS(NaN, ''));
      assertTrue(aql.RELATIONAL_LESS(NaN, 0));
      assertTrue(aql.RELATIONAL_LESS(NaN, [ ]));
      assertTrue(aql.RELATIONAL_LESS(NaN, { }));
      assertTrue(aql.RELATIONAL_LESS(undefined, true));
      assertTrue(aql.RELATIONAL_LESS(undefined, false));
      assertTrue(aql.RELATIONAL_LESS(undefined, 0.0));
      assertTrue(aql.RELATIONAL_LESS(undefined, 1.0));
      assertTrue(aql.RELATIONAL_LESS(undefined, -1.0));
      assertTrue(aql.RELATIONAL_LESS(undefined, ''));
      assertTrue(aql.RELATIONAL_LESS(undefined, '0'));
      assertTrue(aql.RELATIONAL_LESS(undefined, '1'));
      assertTrue(aql.RELATIONAL_LESS(undefined, [ ]));
      assertTrue(aql.RELATIONAL_LESS(undefined, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS(undefined, [ 0, 1 ]));
      assertTrue(aql.RELATIONAL_LESS(undefined, [ 1, 2 ]));
      assertTrue(aql.RELATIONAL_LESS(undefined, { }));
      assertTrue(aql.RELATIONAL_LESS(undefined, { 'a' : 0 }));
      assertTrue(aql.RELATIONAL_LESS(undefined, { 'a' : 1 }));
      assertTrue(aql.RELATIONAL_LESS(undefined, { '0' : false }));
      assertTrue(aql.RELATIONAL_LESS(null, false));
      assertTrue(aql.RELATIONAL_LESS(null, true));
      assertTrue(aql.RELATIONAL_LESS(null, 0));
      assertTrue(aql.RELATIONAL_LESS(null, 1));
      assertTrue(aql.RELATIONAL_LESS(null, -1));
      assertTrue(aql.RELATIONAL_LESS(null, ''));
      assertTrue(aql.RELATIONAL_LESS(null, ' '));
      assertTrue(aql.RELATIONAL_LESS(null, '1'));
      assertTrue(aql.RELATIONAL_LESS(null, '0'));
      assertTrue(aql.RELATIONAL_LESS(null, 'abcd'));
      assertTrue(aql.RELATIONAL_LESS(null, 'null'));
      assertTrue(aql.RELATIONAL_LESS(null, [ ]));
      assertTrue(aql.RELATIONAL_LESS(null, [ true ]));
      assertTrue(aql.RELATIONAL_LESS(null, [ false ]));
      assertTrue(aql.RELATIONAL_LESS(null, [ null ]));
      assertTrue(aql.RELATIONAL_LESS(null, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS(null, { }));
      assertTrue(aql.RELATIONAL_LESS(null, { 'a' : null }));
      assertTrue(aql.RELATIONAL_LESS(false, true));
      assertTrue(aql.RELATIONAL_LESS(false, 0));
      assertTrue(aql.RELATIONAL_LESS(false, 1));
      assertTrue(aql.RELATIONAL_LESS(false, -1));
      assertTrue(aql.RELATIONAL_LESS(false, ''));
      assertTrue(aql.RELATIONAL_LESS(false, ' '));
      assertTrue(aql.RELATIONAL_LESS(false, '1'));
      assertTrue(aql.RELATIONAL_LESS(false, '0'));
      assertTrue(aql.RELATIONAL_LESS(false, 'abcd'));
      assertTrue(aql.RELATIONAL_LESS(false, 'true'));
      assertTrue(aql.RELATIONAL_LESS(false, [ ]));
      assertTrue(aql.RELATIONAL_LESS(false, [ true ]));
      assertTrue(aql.RELATIONAL_LESS(false, [ false ]));
      assertTrue(aql.RELATIONAL_LESS(false, [ null ]));
      assertTrue(aql.RELATIONAL_LESS(false, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS(false, { }));
      assertTrue(aql.RELATIONAL_LESS(false, { 'a' : null }));
      assertTrue(aql.RELATIONAL_LESS(true, 0));
      assertTrue(aql.RELATIONAL_LESS(true, 1));
      assertTrue(aql.RELATIONAL_LESS(true, -1));
      assertTrue(aql.RELATIONAL_LESS(true, ''));
      assertTrue(aql.RELATIONAL_LESS(true, ' '));
      assertTrue(aql.RELATIONAL_LESS(true, '1'));
      assertTrue(aql.RELATIONAL_LESS(true, '0'));
      assertTrue(aql.RELATIONAL_LESS(true, 'abcd'));
      assertTrue(aql.RELATIONAL_LESS(true, 'true'));
      assertTrue(aql.RELATIONAL_LESS(true, [ ]));
      assertTrue(aql.RELATIONAL_LESS(true, [ true ]));
      assertTrue(aql.RELATIONAL_LESS(true, [ false ]));
      assertTrue(aql.RELATIONAL_LESS(true, [ null ]));
      assertTrue(aql.RELATIONAL_LESS(true, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS(true, { }));
      assertTrue(aql.RELATIONAL_LESS(true, { 'a' : null }));
      assertTrue(aql.RELATIONAL_LESS(0, 1));
      assertTrue(aql.RELATIONAL_LESS(1, 2));
      assertTrue(aql.RELATIONAL_LESS(1, 100));
      assertTrue(aql.RELATIONAL_LESS(20, 100));
      assertTrue(aql.RELATIONAL_LESS(-100, 1));
      assertTrue(aql.RELATIONAL_LESS(-100, -10));
      assertTrue(aql.RELATIONAL_LESS(-11, -10));
      assertTrue(aql.RELATIONAL_LESS(999, 1000));
      assertTrue(aql.RELATIONAL_LESS(-1, 1));
      assertTrue(aql.RELATIONAL_LESS(-1, 0));
      assertTrue(aql.RELATIONAL_LESS(1.0, 1.01));
      assertTrue(aql.RELATIONAL_LESS(1.111, 1.2));
      assertTrue(aql.RELATIONAL_LESS(-1.111, -1.110));
      assertTrue(aql.RELATIONAL_LESS(-1.111, -1.1109));
      assertTrue(aql.RELATIONAL_LESS(0, ''));
      assertTrue(aql.RELATIONAL_LESS(0, ' '));
      assertTrue(aql.RELATIONAL_LESS(0, '0'));
      assertTrue(aql.RELATIONAL_LESS(0, '1'));
      assertTrue(aql.RELATIONAL_LESS(0, '-1'));
      assertTrue(aql.RELATIONAL_LESS(0, 'true'));
      assertTrue(aql.RELATIONAL_LESS(0, 'false'));
      assertTrue(aql.RELATIONAL_LESS(0, 'null'));
      assertTrue(aql.RELATIONAL_LESS(1, ''));
      assertTrue(aql.RELATIONAL_LESS(1, ' '));
      assertTrue(aql.RELATIONAL_LESS(1, '0'));
      assertTrue(aql.RELATIONAL_LESS(1, '1'));
      assertTrue(aql.RELATIONAL_LESS(1, '-1'));
      assertTrue(aql.RELATIONAL_LESS(1, 'true'));
      assertTrue(aql.RELATIONAL_LESS(1, 'false'));
      assertTrue(aql.RELATIONAL_LESS(1, 'null'));
      assertTrue(aql.RELATIONAL_LESS(0, '-1'));
      assertTrue(aql.RELATIONAL_LESS(0, '-100'));
      assertTrue(aql.RELATIONAL_LESS(0, '-1.1'));
      assertTrue(aql.RELATIONAL_LESS(0, '-0.0'));
      assertTrue(aql.RELATIONAL_LESS(1000, '-1'));
      assertTrue(aql.RELATIONAL_LESS(1000, '10'));
      assertTrue(aql.RELATIONAL_LESS(1000, '10000'));
      assertTrue(aql.RELATIONAL_LESS(0, [ ]));
      assertTrue(aql.RELATIONAL_LESS(0, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS(10, [ ]));
      assertTrue(aql.RELATIONAL_LESS(100, [ ]));
      assertTrue(aql.RELATIONAL_LESS(100, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS(100, [ 0, 1 ]));
      assertTrue(aql.RELATIONAL_LESS(100, [ 99 ]));
      assertTrue(aql.RELATIONAL_LESS(100, [ 100 ]));
      assertTrue(aql.RELATIONAL_LESS(100, [ 101 ]));
      assertTrue(aql.RELATIONAL_LESS(100, { }));
      assertTrue(aql.RELATIONAL_LESS(100, { 'a' : 0 }));
      assertTrue(aql.RELATIONAL_LESS(100, { 'a' : 1 }));
      assertTrue(aql.RELATIONAL_LESS(100, { 'a' : 99 }));
      assertTrue(aql.RELATIONAL_LESS(100, { 'a' : 100 }));
      assertTrue(aql.RELATIONAL_LESS(100, { 'a' : 101 }));
      assertTrue(aql.RELATIONAL_LESS(100, { 'a' : 1000 }));
      assertTrue(aql.RELATIONAL_LESS('', ' '));
      assertTrue(aql.RELATIONAL_LESS('0', 'a'));
      assertTrue(aql.RELATIONAL_LESS('a', 'a '));
      assertTrue(aql.RELATIONAL_LESS('a', 'b'));
      assertTrue(aql.RELATIONAL_LESS('A', 'a'));
      assertTrue(aql.RELATIONAL_LESS('AB', 'Ab'));
      assertTrue(aql.RELATIONAL_LESS('abcd', 'bbcd'));
      assertTrue(aql.RELATIONAL_LESS('abcd', 'abda'));
      assertTrue(aql.RELATIONAL_LESS('abcd', 'abdd'));
      assertTrue(aql.RELATIONAL_LESS('abcd', 'abcde'));
      assertTrue(aql.RELATIONAL_LESS('0abcd', 'abcde'));
      assertTrue(aql.RELATIONAL_LESS('abcd', [ ]));
      assertTrue(aql.RELATIONAL_LESS('abcd', [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS('abcd', [ -1 ]));
      assertTrue(aql.RELATIONAL_LESS('abcd', [ " " ]));
      assertTrue(aql.RELATIONAL_LESS('abcd', [ "" ]));
      assertTrue(aql.RELATIONAL_LESS('abcd', [ "abc" ]));
      assertTrue(aql.RELATIONAL_LESS('abcd', [ "abcd" ]));
      assertTrue(aql.RELATIONAL_LESS('abcd', { } ));
      assertTrue(aql.RELATIONAL_LESS('abcd', { 'a' : true } ));
      assertTrue(aql.RELATIONAL_LESS('abcd', { 'abc' : true } ));
      assertTrue(aql.RELATIONAL_LESS('ABCD', { 'a' : true } ));
      assertTrue(aql.RELATIONAL_LESS([ ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS([ 0 ], [ 1 ]));
      assertTrue(aql.RELATIONAL_LESS([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertTrue(aql.RELATIONAL_LESS([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertTrue(aql.RELATIONAL_LESS([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertTrue(aql.RELATIONAL_LESS([ 0, 1, 4 ], [ 1 ]));
      assertTrue(aql.RELATIONAL_LESS([ 15, 99 ], [ 110 ]));
      assertTrue(aql.RELATIONAL_LESS([ 15, 99 ], [ 15, 100 ]));
      assertTrue(aql.RELATIONAL_LESS([ ], [ false ]));
      assertTrue(aql.RELATIONAL_LESS([ ], [ true ]));
      assertTrue(aql.RELATIONAL_LESS([ ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS([ ], [ -1 ]));
      assertTrue(aql.RELATIONAL_LESS([ ], [ '' ]));
      assertTrue(aql.RELATIONAL_LESS([ ], [ '0' ]));
      assertTrue(aql.RELATIONAL_LESS([ ], [ 'abcd' ]));
      assertTrue(aql.RELATIONAL_LESS([ ], [ [ ] ]));
      assertTrue(aql.RELATIONAL_LESS([ ], [ [ null ] ]));
      assertTrue(aql.RELATIONAL_LESS([ ], [ { } ]));
      assertTrue(aql.RELATIONAL_LESS([ null ], [ false ]));
      assertTrue(aql.RELATIONAL_LESS([ null ], [ true ]));
      assertTrue(aql.RELATIONAL_LESS([ null ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS([ null ], [ [ ] ]));
      assertTrue(aql.RELATIONAL_LESS([ false ], [ true ]));
      assertTrue(aql.RELATIONAL_LESS([ false ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS([ false ], [ -1 ]));
      assertTrue(aql.RELATIONAL_LESS([ false ], [ '' ]));
      assertTrue(aql.RELATIONAL_LESS([ false ], [ '0' ]));
      assertTrue(aql.RELATIONAL_LESS([ false ], [ 'abcd' ]));
      assertTrue(aql.RELATIONAL_LESS([ false ], [ [ ] ]));
      assertTrue(aql.RELATIONAL_LESS([ false ], [ [ false ] ]));
      assertTrue(aql.RELATIONAL_LESS([ true ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESS([ true ], [ -1 ]));
      assertTrue(aql.RELATIONAL_LESS([ true ], [ '' ]));
      assertTrue(aql.RELATIONAL_LESS([ true ], [ '0' ]));
      assertTrue(aql.RELATIONAL_LESS([ true ], [ 'abcd' ]));
      assertTrue(aql.RELATIONAL_LESS([ true ], [ [ ] ]));
      assertTrue(aql.RELATIONAL_LESS([ true ], [ [ false ] ]));
      assertTrue(aql.RELATIONAL_LESS([ false, false ], [ true ]));
      assertTrue(aql.RELATIONAL_LESS([ false, false ], [ false, true ]));
      assertTrue(aql.RELATIONAL_LESS([ false, false ], [ false, 0 ]));
      assertTrue(aql.RELATIONAL_LESS([ null, null ], [ null, false ]));
      assertTrue(aql.RELATIONAL_LESS([ ], { }));
      assertTrue(aql.RELATIONAL_LESS([ ], { 'a' : true }));
      assertTrue(aql.RELATIONAL_LESS([ ], { 'a' : null }));
      assertTrue(aql.RELATIONAL_LESS([ ], { 'a' : false }));
      assertTrue(aql.RELATIONAL_LESS([ '' ], { }));
      assertTrue(aql.RELATIONAL_LESS([ 0 ], { }));
      assertTrue(aql.RELATIONAL_LESS([ null ], { }));
      assertTrue(aql.RELATIONAL_LESS([ false ], { }));
      assertTrue(aql.RELATIONAL_LESS([ false ], { 'a' : false }));
      assertTrue(aql.RELATIONAL_LESS([ true ], { 'a' : false }));
      assertTrue(aql.RELATIONAL_LESS([ 'abcd' ], { 'a' : false }));
      assertTrue(aql.RELATIONAL_LESS([ 5 ], { 'a' : false }));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6 ], { 'a' : 2, 'b' : 2 }));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, 7 ], { }));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, false ], [ 5, 6, true ]));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, 0 ]));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, 999 ], [ 5, 6, '' ]));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, 'b' ]));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, 'A' ], [ 5, 6, 'a' ]));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, '' ], [ 5, 6, 'a' ]));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, 9, 9 ], [ 5, 6, [ ] ]));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, [ ] ]));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, { } ]));
      assertTrue(aql.RELATIONAL_LESS([ 5, 6, 9, 9 ], [ 5, 6, { } ]));
      assertTrue(aql.RELATIONAL_LESS({ }, { 'a' : 0 }));
      assertTrue(aql.RELATIONAL_LESS({ 'a' : 1 }, { 'a' : 2 }));
      assertTrue(aql.RELATIONAL_LESS({ 'b' : 2 }, { 'a' : 1 }));
      assertTrue(aql.RELATIONAL_LESS({ 'z' : 1 }, { 'c' : 1 }));
      assertTrue(aql.RELATIONAL_LESS({ 'a' : [ 9 ], 'b' : false }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(aql.RELATIONAL_LESS({ 'a' : [ 9 ], 'b' : true }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(aql.RELATIONAL_LESS({ 'a' : [ ], 'b' : true }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(aql.RELATIONAL_LESS({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 10, 1 ] }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_LESS function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalLessFalse: function() {
      assertFalse(aql.RELATIONAL_LESS(undefined, undefined));
      assertFalse(aql.RELATIONAL_LESS(undefined, null));
      assertFalse(aql.RELATIONAL_LESS(undefined, NaN));
      assertFalse(aql.RELATIONAL_LESS(null, undefined));
      assertFalse(aql.RELATIONAL_LESS(null, NaN));
      assertFalse(aql.RELATIONAL_LESS(NaN, null));
      assertFalse(aql.RELATIONAL_LESS(NaN, undefined));
      assertFalse(aql.RELATIONAL_LESS(true, undefined));
      assertFalse(aql.RELATIONAL_LESS(false, undefined));
      assertFalse(aql.RELATIONAL_LESS(0.0, undefined));
      assertFalse(aql.RELATIONAL_LESS(1.0, undefined));
      assertFalse(aql.RELATIONAL_LESS(-1.0, undefined));
      assertFalse(aql.RELATIONAL_LESS('', undefined));
      assertFalse(aql.RELATIONAL_LESS('0', undefined));
      assertFalse(aql.RELATIONAL_LESS('1', undefined));
      assertFalse(aql.RELATIONAL_LESS([ ], undefined));
      assertFalse(aql.RELATIONAL_LESS([ ], [ undefined ]));
      assertFalse(aql.RELATIONAL_LESS([ ], [ null ]));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], undefined));
      assertFalse(aql.RELATIONAL_LESS([ 0, 1 ], undefined));
      assertFalse(aql.RELATIONAL_LESS([ 1, 2 ], undefined));
      assertFalse(aql.RELATIONAL_LESS({ }, undefined));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 0 }, undefined));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 1 }, undefined));
      assertFalse(aql.RELATIONAL_LESS({ '0' : false }, undefined));
      assertFalse(aql.RELATIONAL_LESS(false, NaN));
      assertFalse(aql.RELATIONAL_LESS(true, NaN));
      assertFalse(aql.RELATIONAL_LESS('', NaN));
      assertFalse(aql.RELATIONAL_LESS(0, NaN));
      assertFalse(aql.RELATIONAL_LESS(null, null));
      assertFalse(aql.RELATIONAL_LESS(false, null));
      assertFalse(aql.RELATIONAL_LESS(true, null));
      assertFalse(aql.RELATIONAL_LESS(0, null));
      assertFalse(aql.RELATIONAL_LESS(1, null));
      assertFalse(aql.RELATIONAL_LESS(-1, null));
      assertFalse(aql.RELATIONAL_LESS('', null));
      assertFalse(aql.RELATIONAL_LESS(' ', null));
      assertFalse(aql.RELATIONAL_LESS('1', null));
      assertFalse(aql.RELATIONAL_LESS('0', null));
      assertFalse(aql.RELATIONAL_LESS('abcd', null));
      assertFalse(aql.RELATIONAL_LESS('null', null));
      assertFalse(aql.RELATIONAL_LESS([ ], null));
      assertFalse(aql.RELATIONAL_LESS([ true ], null));
      assertFalse(aql.RELATIONAL_LESS([ false ], null));
      assertFalse(aql.RELATIONAL_LESS([ null ], null));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], null));
      assertFalse(aql.RELATIONAL_LESS({ }, null));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : null }, null));
      assertFalse(aql.RELATIONAL_LESS(false, false));
      assertFalse(aql.RELATIONAL_LESS(true, true));
      assertFalse(aql.RELATIONAL_LESS(true, false));
      assertFalse(aql.RELATIONAL_LESS(0, false));
      assertFalse(aql.RELATIONAL_LESS(1, false));
      assertFalse(aql.RELATIONAL_LESS(-1, false));
      assertFalse(aql.RELATIONAL_LESS('', false));
      assertFalse(aql.RELATIONAL_LESS(' ', false));
      assertFalse(aql.RELATIONAL_LESS('1', false));
      assertFalse(aql.RELATIONAL_LESS('0', false));
      assertFalse(aql.RELATIONAL_LESS('abcd', false));
      assertFalse(aql.RELATIONAL_LESS('true', false));
      assertFalse(aql.RELATIONAL_LESS([ ], false));
      assertFalse(aql.RELATIONAL_LESS([ true ], false));
      assertFalse(aql.RELATIONAL_LESS([ false ], false));
      assertFalse(aql.RELATIONAL_LESS([ null ], false));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], false));
      assertFalse(aql.RELATIONAL_LESS({ }, false));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : null }, false));
      assertFalse(aql.RELATIONAL_LESS(0, true));
      assertFalse(aql.RELATIONAL_LESS(1, true));
      assertFalse(aql.RELATIONAL_LESS(-1, true));
      assertFalse(aql.RELATIONAL_LESS('', true));
      assertFalse(aql.RELATIONAL_LESS(' ', true));
      assertFalse(aql.RELATIONAL_LESS('1', true));
      assertFalse(aql.RELATIONAL_LESS('0', true));
      assertFalse(aql.RELATIONAL_LESS('abcd', true));
      assertFalse(aql.RELATIONAL_LESS('true', true));
      assertFalse(aql.RELATIONAL_LESS([ ], true));
      assertFalse(aql.RELATIONAL_LESS([ true ], true));
      assertFalse(aql.RELATIONAL_LESS([ false ], true));
      assertFalse(aql.RELATIONAL_LESS([ null ], true));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], true));
      assertFalse(aql.RELATIONAL_LESS({ }, true));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : null }, true));
      assertFalse(aql.RELATIONAL_LESS(0, 0));
      assertFalse(aql.RELATIONAL_LESS(1, 1));
      assertFalse(aql.RELATIONAL_LESS(-10, -10));
      assertFalse(aql.RELATIONAL_LESS(-100, -100));
      assertFalse(aql.RELATIONAL_LESS(-334.5, -334.5));
      assertFalse(aql.RELATIONAL_LESS(1, 0));
      assertFalse(aql.RELATIONAL_LESS(2, 1));
      assertFalse(aql.RELATIONAL_LESS(100, 1));
      assertFalse(aql.RELATIONAL_LESS(100, 20));
      assertFalse(aql.RELATIONAL_LESS(1, -100));
      assertFalse(aql.RELATIONAL_LESS(-10, -100));
      assertFalse(aql.RELATIONAL_LESS(-10, -11));
      assertFalse(aql.RELATIONAL_LESS(1000, 999));
      assertFalse(aql.RELATIONAL_LESS(1, -1));
      assertFalse(aql.RELATIONAL_LESS(0, -1));
      assertFalse(aql.RELATIONAL_LESS(1.01, 1.0));
      assertFalse(aql.RELATIONAL_LESS(1.2, 1.111));
      assertFalse(aql.RELATIONAL_LESS(-1.110, -1.111));
      assertFalse(aql.RELATIONAL_LESS(-1.1109, -1.111));
      assertFalse(aql.RELATIONAL_LESS('', 0));
      assertFalse(aql.RELATIONAL_LESS(' ', 0));
      assertFalse(aql.RELATIONAL_LESS('0', 0));
      assertFalse(aql.RELATIONAL_LESS('1', 0));
      assertFalse(aql.RELATIONAL_LESS('-1', 0));
      assertFalse(aql.RELATIONAL_LESS('true', 0));
      assertFalse(aql.RELATIONAL_LESS('false', 0));
      assertFalse(aql.RELATIONAL_LESS('null', 0));
      assertFalse(aql.RELATIONAL_LESS('', 1));
      assertFalse(aql.RELATIONAL_LESS(' ', 1));
      assertFalse(aql.RELATIONAL_LESS('0', 1));
      assertFalse(aql.RELATIONAL_LESS('1', 1));
      assertFalse(aql.RELATIONAL_LESS('-1', 1));
      assertFalse(aql.RELATIONAL_LESS('true', 1));
      assertFalse(aql.RELATIONAL_LESS('false', 1));
      assertFalse(aql.RELATIONAL_LESS('null', 1));
      assertFalse(aql.RELATIONAL_LESS('-1', 0));
      assertFalse(aql.RELATIONAL_LESS('-100', 0));
      assertFalse(aql.RELATIONAL_LESS('-1.1', 0));
      assertFalse(aql.RELATIONAL_LESS('-0.0', 0));
      assertFalse(aql.RELATIONAL_LESS('-1', 1000));
      assertFalse(aql.RELATIONAL_LESS('10', 1000));
      assertFalse(aql.RELATIONAL_LESS('10000', 1000));
      assertFalse(aql.RELATIONAL_LESS([ ], 0));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], 0));
      assertFalse(aql.RELATIONAL_LESS([ ], 10));
      assertFalse(aql.RELATIONAL_LESS([ ], 100));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], 100));
      assertFalse(aql.RELATIONAL_LESS([ 0, 1 ], 100));
      assertFalse(aql.RELATIONAL_LESS([ 99 ], 100));
      assertFalse(aql.RELATIONAL_LESS([ 100 ], 100));
      assertFalse(aql.RELATIONAL_LESS([ 101 ], 100));
      assertFalse(aql.RELATIONAL_LESS({ }, 100));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 0 }, 100));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 1 }, 100));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 99 }, 100));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 100 }, 100));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 101 }, 100));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 1000 }, 100));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : false }, 'zz'));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 'a' }, 'zz'));
      assertFalse(aql.RELATIONAL_LESS('', ''));
      assertFalse(aql.RELATIONAL_LESS(' ', ' '));
      assertFalse(aql.RELATIONAL_LESS('a', 'a'));
      assertFalse(aql.RELATIONAL_LESS(' a', ' a'));
      assertFalse(aql.RELATIONAL_LESS('abcd', 'abcd'));
      assertFalse(aql.RELATIONAL_LESS(' ', ''));
      assertFalse(aql.RELATIONAL_LESS('a', '0'));
      assertFalse(aql.RELATIONAL_LESS('a ', 'a'));
      assertFalse(aql.RELATIONAL_LESS('b', 'a'));
      assertFalse(aql.RELATIONAL_LESS('a', 'A'));
      assertFalse(aql.RELATIONAL_LESS('Ab', 'AB'));
      assertFalse(aql.RELATIONAL_LESS('bbcd', 'abcd'));
      assertFalse(aql.RELATIONAL_LESS('abda', 'abcd'));
      assertFalse(aql.RELATIONAL_LESS('abdd', 'abcd'));
      assertFalse(aql.RELATIONAL_LESS('abcde', 'abcd'));
      assertFalse(aql.RELATIONAL_LESS('abcde', '0abcde'));
      assertFalse(aql.RELATIONAL_LESS([ ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], [ 0 ]));
      assertFalse(aql.RELATIONAL_LESS([ 1 ], [ 1 ]));
      assertFalse(aql.RELATIONAL_LESS([ true ], [ true ]));
      assertFalse(aql.RELATIONAL_LESS([ false ], [ false ]));
      assertFalse(aql.RELATIONAL_LESS([ [ 0, 1, 2 ] ], [ [ 0, 1, 2 ] ]));
      assertFalse(aql.RELATIONAL_LESS([ [ 1, [ "true", 0, -99 , false ] ], 4 ], [ [ 1, [ "true", 0, -99, false ] ], 4 ]));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ 1 ], [ 0 ]));
      assertFalse(aql.RELATIONAL_LESS([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertFalse(aql.RELATIONAL_LESS([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertFalse(aql.RELATIONAL_LESS([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertFalse(aql.RELATIONAL_LESS([ 1 ], [ 0, 1, 4 ]));
      assertFalse(aql.RELATIONAL_LESS([ 110 ], [ 15, 99 ]));
      assertFalse(aql.RELATIONAL_LESS([ 15, 100 ], [ 15, 99 ]));
      assertFalse(aql.RELATIONAL_LESS([ undefined ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ null ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ false ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ true ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ -1 ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ '' ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ '0' ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ 'abcd' ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ [ ] ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ [ null ] ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ { } ], [ ]));
      assertFalse(aql.RELATIONAL_LESS([ false ], [ null ]));
      assertFalse(aql.RELATIONAL_LESS([ true ], [ null ]));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], [ null ]));
      assertFalse(aql.RELATIONAL_LESS([ [ ] ], [ null ]));
      assertFalse(aql.RELATIONAL_LESS([ true ], [ false ]));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], [ false ]));
      assertFalse(aql.RELATIONAL_LESS([ -1 ], [ false ]));
      assertFalse(aql.RELATIONAL_LESS([ '' ], [ false ]));
      assertFalse(aql.RELATIONAL_LESS([ '0' ], [ false ]));
      assertFalse(aql.RELATIONAL_LESS([ 'abcd' ], [ false ]));
      assertFalse(aql.RELATIONAL_LESS([ [ ] ], [ false ]));
      assertFalse(aql.RELATIONAL_LESS([ [ false ] ], [ false ]));
      assertFalse(aql.RELATIONAL_LESS([ 0 ], [ true ]));
      assertFalse(aql.RELATIONAL_LESS([ -1 ], [ true ]));
      assertFalse(aql.RELATIONAL_LESS([ '' ], [ true ]));
      assertFalse(aql.RELATIONAL_LESS([ '0' ], [ true ]));
      assertFalse(aql.RELATIONAL_LESS([ 'abcd' ], [ true ]));
      assertFalse(aql.RELATIONAL_LESS([ [ ] ], [ true ]));
      assertFalse(aql.RELATIONAL_LESS([ [ false] ], [ true ]));
      assertFalse(aql.RELATIONAL_LESS([ true ], [ false, false ]));
      assertFalse(aql.RELATIONAL_LESS([ false, true ], [ false, false ]));
      assertFalse(aql.RELATIONAL_LESS([ false, 0 ], [ false, false ]));
      assertFalse(aql.RELATIONAL_LESS([ null, false ], [ null, null ]));
      assertFalse(aql.RELATIONAL_LESS({ }, [ ]));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : true }, [ ]));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : null }, [ ]));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : false }, [ ]));
      assertFalse(aql.RELATIONAL_LESS({ }, [ '' ]));
      assertFalse(aql.RELATIONAL_LESS({ }, [ 0 ]));
      assertFalse(aql.RELATIONAL_LESS({ }, [ null ]));
      assertFalse(aql.RELATIONAL_LESS({ }, [ false ]));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : false }, [ false ]));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : false }, [ true ]));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : false }, [ 'abcd' ]));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : false }, [ 5 ]));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 2, 'b' : 2 }, [ 5, 6 ]));
      assertFalse(aql.RELATIONAL_LESS({ 'a' : 1, 'b' : 2 }, { 'a' : 1, 'b' : 2, 'c' : null }));
      assertFalse(aql.RELATIONAL_LESS({ 'b' : 2, 'a' : 1 }, { 'a' : 1, 'b' : 2, 'c' : null }));
      assertFalse(aql.RELATIONAL_LESS({ }, [ 5, 6, 7 ]));
      assertFalse(aql.RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, false ]));
      assertFalse(aql.RELATIONAL_LESS([ 5, 6, 0 ], [ 5, 6, true ]));
      assertFalse(aql.RELATIONAL_LESS([ 5, 6, '' ], [ 5, 6, 999 ]));
      assertFalse(aql.RELATIONAL_LESS([ 5, 6, 'b' ], [ 5, 6, 'a' ]));
      assertFalse(aql.RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, 'A' ]));
      assertFalse(aql.RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, '' ]));
      assertFalse(aql.RELATIONAL_LESS([ 5, 6, [ ] ], [ 5, 6, 9 ,9 ]));
      assertFalse(aql.RELATIONAL_LESS([ 5, 6, [ ] ], [ 5, 6, true ]));
      assertFalse(aql.RELATIONAL_LESS([ 5, 6, { } ], [ 5, 6, true ]));
      assertFalse(aql.RELATIONAL_LESS([ 5, 6, { } ], [ 5, 6, 9, 9 ]));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_GREATER function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterTrue: function() {
      assertTrue(aql.RELATIONAL_GREATER(true, undefined));
      assertTrue(aql.RELATIONAL_GREATER(false, undefined));
      assertTrue(aql.RELATIONAL_GREATER(0.0, undefined));
      assertTrue(aql.RELATIONAL_GREATER(1.0, undefined));
      assertTrue(aql.RELATIONAL_GREATER(-1.0, undefined));
      assertTrue(aql.RELATIONAL_GREATER('', undefined));
      assertTrue(aql.RELATIONAL_GREATER('0', undefined));
      assertTrue(aql.RELATIONAL_GREATER('1', undefined));
      assertTrue(aql.RELATIONAL_GREATER([ ], undefined));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], undefined));
      assertTrue(aql.RELATIONAL_GREATER([ 0, 1 ], undefined));
      assertTrue(aql.RELATIONAL_GREATER([ 1, 2 ], undefined));
      assertTrue(aql.RELATIONAL_GREATER({ }, undefined));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 0 }, undefined));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 1 }, undefined));
      assertTrue(aql.RELATIONAL_GREATER({ '0' : false }, undefined));
      assertTrue(aql.RELATIONAL_GREATER(false, NaN));
      assertTrue(aql.RELATIONAL_GREATER(true, NaN));
      assertTrue(aql.RELATIONAL_GREATER('', NaN));
      assertTrue(aql.RELATIONAL_GREATER(0, NaN));

      assertTrue(aql.RELATIONAL_GREATER(false, null));
      assertTrue(aql.RELATIONAL_GREATER(true, null));
      assertTrue(aql.RELATIONAL_GREATER(0, null));
      assertTrue(aql.RELATIONAL_GREATER(1, null));
      assertTrue(aql.RELATIONAL_GREATER(-1, null));
      assertTrue(aql.RELATIONAL_GREATER('', null));
      assertTrue(aql.RELATIONAL_GREATER(' ', null));
      assertTrue(aql.RELATIONAL_GREATER('1', null));
      assertTrue(aql.RELATIONAL_GREATER('0', null));
      assertTrue(aql.RELATIONAL_GREATER('abcd', null));
      assertTrue(aql.RELATIONAL_GREATER('null', null));
      assertTrue(aql.RELATIONAL_GREATER([ ], null));
      assertTrue(aql.RELATIONAL_GREATER([ true ], null));
      assertTrue(aql.RELATIONAL_GREATER([ false ], null));
      assertTrue(aql.RELATIONAL_GREATER([ null ], null));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], null));
      assertTrue(aql.RELATIONAL_GREATER({ }, null));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : null }, null));
      assertTrue(aql.RELATIONAL_GREATER(true, false));
      assertTrue(aql.RELATIONAL_GREATER(0, false));
      assertTrue(aql.RELATIONAL_GREATER(1, false));
      assertTrue(aql.RELATIONAL_GREATER(-1, false));
      assertTrue(aql.RELATIONAL_GREATER('', false));
      assertTrue(aql.RELATIONAL_GREATER(' ', false));
      assertTrue(aql.RELATIONAL_GREATER('1', false));
      assertTrue(aql.RELATIONAL_GREATER('0', false));
      assertTrue(aql.RELATIONAL_GREATER('abcd', false));
      assertTrue(aql.RELATIONAL_GREATER('true', false));
      assertTrue(aql.RELATIONAL_GREATER([ ], false));
      assertTrue(aql.RELATIONAL_GREATER([ true ], false));
      assertTrue(aql.RELATIONAL_GREATER([ false ], false));
      assertTrue(aql.RELATIONAL_GREATER([ null ], false));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], false));
      assertTrue(aql.RELATIONAL_GREATER({ }, false));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : null }, false));
      assertTrue(aql.RELATIONAL_GREATER(0, true));
      assertTrue(aql.RELATIONAL_GREATER(1, true));
      assertTrue(aql.RELATIONAL_GREATER(-1, true));
      assertTrue(aql.RELATIONAL_GREATER('', true));
      assertTrue(aql.RELATIONAL_GREATER(' ', true));
      assertTrue(aql.RELATIONAL_GREATER('1', true));
      assertTrue(aql.RELATIONAL_GREATER('0', true));
      assertTrue(aql.RELATIONAL_GREATER('abcd', true));
      assertTrue(aql.RELATIONAL_GREATER('true', true));
      assertTrue(aql.RELATIONAL_GREATER([ ], true));
      assertTrue(aql.RELATIONAL_GREATER([ true ], true));
      assertTrue(aql.RELATIONAL_GREATER([ false ], true));
      assertTrue(aql.RELATIONAL_GREATER([ null ], true));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], true));
      assertTrue(aql.RELATIONAL_GREATER({ }, true));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : null }, true));
      assertTrue(aql.RELATIONAL_GREATER(1, 0));
      assertTrue(aql.RELATIONAL_GREATER(2, 1));
      assertTrue(aql.RELATIONAL_GREATER(100, 1));
      assertTrue(aql.RELATIONAL_GREATER(100, 20));
      assertTrue(aql.RELATIONAL_GREATER(1, -100));
      assertTrue(aql.RELATIONAL_GREATER(-10, -100));
      assertTrue(aql.RELATIONAL_GREATER(-10, -11));
      assertTrue(aql.RELATIONAL_GREATER(1000, 999));
      assertTrue(aql.RELATIONAL_GREATER(1, -1));
      assertTrue(aql.RELATIONAL_GREATER(0, -1));
      assertTrue(aql.RELATIONAL_GREATER(1.01, 1.0));
      assertTrue(aql.RELATIONAL_GREATER(1.2, 1.111));
      assertTrue(aql.RELATIONAL_GREATER(-1.110, -1.111));
      assertTrue(aql.RELATIONAL_GREATER(-1.1109, -1.111));
      assertTrue(aql.RELATIONAL_GREATER('', 0));
      assertTrue(aql.RELATIONAL_GREATER(' ', 0));
      assertTrue(aql.RELATIONAL_GREATER('0', 0));
      assertTrue(aql.RELATIONAL_GREATER('1', 0));
      assertTrue(aql.RELATIONAL_GREATER('-1', 0));
      assertTrue(aql.RELATIONAL_GREATER('true', 0));
      assertTrue(aql.RELATIONAL_GREATER('false', 0));
      assertTrue(aql.RELATIONAL_GREATER('null', 0));
      assertTrue(aql.RELATIONAL_GREATER('', 1));
      assertTrue(aql.RELATIONAL_GREATER(' ', 1));
      assertTrue(aql.RELATIONAL_GREATER('0', 1));
      assertTrue(aql.RELATIONAL_GREATER('1', 1));
      assertTrue(aql.RELATIONAL_GREATER('-1', 1));
      assertTrue(aql.RELATIONAL_GREATER('true', 1));
      assertTrue(aql.RELATIONAL_GREATER('false', 1));
      assertTrue(aql.RELATIONAL_GREATER('null', 1));
      assertTrue(aql.RELATIONAL_GREATER('-1', 0));
      assertTrue(aql.RELATIONAL_GREATER('-100', 0));
      assertTrue(aql.RELATIONAL_GREATER('-1.1', 0));
      assertTrue(aql.RELATIONAL_GREATER('-0.0', 0));
      assertTrue(aql.RELATIONAL_GREATER('-1', 1000));
      assertTrue(aql.RELATIONAL_GREATER('10', 1000));
      assertTrue(aql.RELATIONAL_GREATER('10000', 1000));
      assertTrue(aql.RELATIONAL_GREATER([ ], 0));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], 0));
      assertTrue(aql.RELATIONAL_GREATER([ ], 10));
      assertTrue(aql.RELATIONAL_GREATER([ ], 100));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], 100));
      assertTrue(aql.RELATIONAL_GREATER([ 0, 1 ], 100));
      assertTrue(aql.RELATIONAL_GREATER([ 99 ], 100));
      assertTrue(aql.RELATIONAL_GREATER([ 100 ], 100));
      assertTrue(aql.RELATIONAL_GREATER([ 101 ], 100));
      assertTrue(aql.RELATIONAL_GREATER({ }, 100));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 0 }, 100));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 1 }, 100));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 99 }, 100));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 100 }, 100));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 101 }, 100));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 1000 }, 100));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : false }, 'zz'));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 'a' }, 'zz'));
      assertTrue(aql.RELATIONAL_GREATER(' ', ''));
      assertTrue(aql.RELATIONAL_GREATER('a', '0'));
      assertTrue(aql.RELATIONAL_GREATER('a ', 'a'));
      assertTrue(aql.RELATIONAL_GREATER('b', 'a'));
      assertTrue(aql.RELATIONAL_GREATER('a', 'A'));
      assertTrue(aql.RELATIONAL_GREATER('Ab', 'AB'));
      assertTrue(aql.RELATIONAL_GREATER('bbcd', 'abcd'));
      assertTrue(aql.RELATIONAL_GREATER('abda', 'abcd'));
      assertTrue(aql.RELATIONAL_GREATER('abdd', 'abcd'));
      assertTrue(aql.RELATIONAL_GREATER('abcde', 'abcd'));
      assertTrue(aql.RELATIONAL_GREATER('abcde', '0abcde'));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ 1 ], [ 0 ]));
      assertTrue(aql.RELATIONAL_GREATER([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertTrue(aql.RELATIONAL_GREATER([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertTrue(aql.RELATIONAL_GREATER([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertTrue(aql.RELATIONAL_GREATER([ 1 ], [ 0, 1, 4 ]));
      assertTrue(aql.RELATIONAL_GREATER([ 110 ], [ 15, 99 ]));
      assertTrue(aql.RELATIONAL_GREATER([ 15, 100 ], [ 15, 99 ]));
      assertTrue(aql.RELATIONAL_GREATER([ false ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ true ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ -1 ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ '' ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ '0' ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ 'abcd' ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ [ ] ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ [ null ] ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ { } ], [ ]));
      assertTrue(aql.RELATIONAL_GREATER([ false ], [ null ]));
      assertTrue(aql.RELATIONAL_GREATER([ true ], [ null ]));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], [ null ]));
      assertTrue(aql.RELATIONAL_GREATER([ [ ] ], [ null ]));
      assertTrue(aql.RELATIONAL_GREATER([ true ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATER([ -1 ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATER([ '' ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATER([ '0' ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATER([ 'abcd' ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATER([ [ ] ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATER([ [ false ] ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATER([ 0 ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATER([ -1 ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATER([ '' ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATER([ '0' ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATER([ 'abcd' ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATER([ [ ] ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATER([ [ false] ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATER([ true ], [ false, false ]));
      assertTrue(aql.RELATIONAL_GREATER([ false, true ], [ false, false ]));
      assertTrue(aql.RELATIONAL_GREATER([ false, 0 ], [ false, false ]));
      assertTrue(aql.RELATIONAL_GREATER([ null, false ], [ null, null ]));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 0 }, { }));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 2 }, { 'a' : 1 }));
      assertTrue(aql.RELATIONAL_GREATER({ 'A' : 2 }, { 'a' : 1 }));
      assertTrue(aql.RELATIONAL_GREATER({ 'A' : 1 }, { 'a' : 2 }));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : 1 }, { 'b' : 1 }));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : false }));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : true }));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ ], 'b' : true }));
      assertTrue(aql.RELATIONAL_GREATER({ 'a' : [ 10, 1 ] }, { 'a' : [ 10 ], 'b' : true }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_GREATER function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterFalse: function() {
      assertFalse(aql.RELATIONAL_GREATER(undefined, undefined));
      assertFalse(aql.RELATIONAL_GREATER(undefined, null));
      assertFalse(aql.RELATIONAL_GREATER(null, undefined));
      assertFalse(aql.RELATIONAL_GREATER(null, null));
      assertFalse(aql.RELATIONAL_GREATER(null, NaN));
      assertFalse(aql.RELATIONAL_GREATER(undefined, NaN));
      assertFalse(aql.RELATIONAL_GREATER(NaN, null));
      assertFalse(aql.RELATIONAL_GREATER(NaN, undefined));
      assertFalse(aql.RELATIONAL_GREATER(NaN, false));
      assertFalse(aql.RELATIONAL_GREATER(NaN, true));
      assertFalse(aql.RELATIONAL_GREATER(NaN, ''));
      assertFalse(aql.RELATIONAL_GREATER(NaN, 0));
      assertFalse(aql.RELATIONAL_GREATER(undefined, true));
      assertFalse(aql.RELATIONAL_GREATER(undefined, false));
      assertFalse(aql.RELATIONAL_GREATER(undefined, 0.0));
      assertFalse(aql.RELATIONAL_GREATER(undefined, 1.0));
      assertFalse(aql.RELATIONAL_GREATER(undefined, -1.0));
      assertFalse(aql.RELATIONAL_GREATER(undefined, ''));
      assertFalse(aql.RELATIONAL_GREATER(undefined, '0'));
      assertFalse(aql.RELATIONAL_GREATER(undefined, '1'));
      assertFalse(aql.RELATIONAL_GREATER(undefined, [ ]));
      assertFalse(aql.RELATIONAL_GREATER(undefined, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER(undefined, [ 0, 1 ]));
      assertFalse(aql.RELATIONAL_GREATER(undefined, [ 1, 2 ]));
      assertFalse(aql.RELATIONAL_GREATER(undefined, { }));
      assertFalse(aql.RELATIONAL_GREATER(undefined, { 'a' : 0 }));
      assertFalse(aql.RELATIONAL_GREATER(undefined, { 'a' : 1 }));
      assertFalse(aql.RELATIONAL_GREATER(undefined, { '0' : false }));
      assertFalse(aql.RELATIONAL_GREATER(null, false));
      assertFalse(aql.RELATIONAL_GREATER(null, true));
      assertFalse(aql.RELATIONAL_GREATER(null, 0));
      assertFalse(aql.RELATIONAL_GREATER(null, 1));
      assertFalse(aql.RELATIONAL_GREATER(null, -1));
      assertFalse(aql.RELATIONAL_GREATER(null, ''));
      assertFalse(aql.RELATIONAL_GREATER(null, ' '));
      assertFalse(aql.RELATIONAL_GREATER(null, '1'));
      assertFalse(aql.RELATIONAL_GREATER(null, '0'));
      assertFalse(aql.RELATIONAL_GREATER(null, 'abcd'));
      assertFalse(aql.RELATIONAL_GREATER(null, 'null'));
      assertFalse(aql.RELATIONAL_GREATER(null, [ ]));
      assertFalse(aql.RELATIONAL_GREATER(null, [ true ]));
      assertFalse(aql.RELATIONAL_GREATER(null, [ false ]));
      assertFalse(aql.RELATIONAL_GREATER(null, [ null ]));
      assertFalse(aql.RELATIONAL_GREATER(null, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER(null, { }));
      assertFalse(aql.RELATIONAL_GREATER(null, { 'a' : null }));
      assertFalse(aql.RELATIONAL_GREATER(false, false));
      assertFalse(aql.RELATIONAL_GREATER(true, true));
      assertFalse(aql.RELATIONAL_GREATER(false, true));
      assertFalse(aql.RELATIONAL_GREATER(false, 0));
      assertFalse(aql.RELATIONAL_GREATER(false, 1));
      assertFalse(aql.RELATIONAL_GREATER(false, -1));
      assertFalse(aql.RELATIONAL_GREATER(false, ''));
      assertFalse(aql.RELATIONAL_GREATER(false, ' '));
      assertFalse(aql.RELATIONAL_GREATER(false, '1'));
      assertFalse(aql.RELATIONAL_GREATER(false, '0'));
      assertFalse(aql.RELATIONAL_GREATER(false, 'abcd'));
      assertFalse(aql.RELATIONAL_GREATER(false, 'true'));
      assertFalse(aql.RELATIONAL_GREATER(false, [ ]));
      assertFalse(aql.RELATIONAL_GREATER(false, [ true ]));
      assertFalse(aql.RELATIONAL_GREATER(false, [ false ]));
      assertFalse(aql.RELATIONAL_GREATER(false, [ null ]));
      assertFalse(aql.RELATIONAL_GREATER(false, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER(false, { }));
      assertFalse(aql.RELATIONAL_GREATER(false, { 'a' : null }));
      assertFalse(aql.RELATIONAL_GREATER(true, 0));
      assertFalse(aql.RELATIONAL_GREATER(true, 1));
      assertFalse(aql.RELATIONAL_GREATER(true, -1));
      assertFalse(aql.RELATIONAL_GREATER(true, ''));
      assertFalse(aql.RELATIONAL_GREATER(true, ' '));
      assertFalse(aql.RELATIONAL_GREATER(true, '1'));
      assertFalse(aql.RELATIONAL_GREATER(true, '0'));
      assertFalse(aql.RELATIONAL_GREATER(true, 'abcd'));
      assertFalse(aql.RELATIONAL_GREATER(true, 'true'));
      assertFalse(aql.RELATIONAL_GREATER(true, [ ]));
      assertFalse(aql.RELATIONAL_GREATER(true, [ true ]));
      assertFalse(aql.RELATIONAL_GREATER(true, [ false ]));
      assertFalse(aql.RELATIONAL_GREATER(true, [ null ]));
      assertFalse(aql.RELATIONAL_GREATER(true, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER(true, { }));
      assertFalse(aql.RELATIONAL_GREATER(true, { 'a' : null }));
      assertFalse(aql.RELATIONAL_GREATER(0, 0));
      assertFalse(aql.RELATIONAL_GREATER(1, 1));
      assertFalse(aql.RELATIONAL_GREATER(-10, -10));
      assertFalse(aql.RELATIONAL_GREATER(-100, -100));
      assertFalse(aql.RELATIONAL_GREATER(-334.5, -334.5));
      assertFalse(aql.RELATIONAL_GREATER(0, 1));
      assertFalse(aql.RELATIONAL_GREATER(1, 2));
      assertFalse(aql.RELATIONAL_GREATER(1, 100));
      assertFalse(aql.RELATIONAL_GREATER(20, 100));
      assertFalse(aql.RELATIONAL_GREATER(-100, 1));
      assertFalse(aql.RELATIONAL_GREATER(-100, -10));
      assertFalse(aql.RELATIONAL_GREATER(-11, -10));
      assertFalse(aql.RELATIONAL_GREATER(999, 1000));
      assertFalse(aql.RELATIONAL_GREATER(-1, 1));
      assertFalse(aql.RELATIONAL_GREATER(-1, 0));
      assertFalse(aql.RELATIONAL_GREATER(1.0, 1.01));
      assertFalse(aql.RELATIONAL_GREATER(1.111, 1.2));
      assertFalse(aql.RELATIONAL_GREATER(-1.111, -1.110));
      assertFalse(aql.RELATIONAL_GREATER(-1.111, -1.1109));
      assertFalse(aql.RELATIONAL_GREATER(0, ''));
      assertFalse(aql.RELATIONAL_GREATER(0, ' '));
      assertFalse(aql.RELATIONAL_GREATER(0, '0'));
      assertFalse(aql.RELATIONAL_GREATER(0, '1'));
      assertFalse(aql.RELATIONAL_GREATER(0, '-1'));
      assertFalse(aql.RELATIONAL_GREATER(0, 'true'));
      assertFalse(aql.RELATIONAL_GREATER(0, 'false'));
      assertFalse(aql.RELATIONAL_GREATER(0, 'null'));
      assertFalse(aql.RELATIONAL_GREATER(1, ''));
      assertFalse(aql.RELATIONAL_GREATER(1, ' '));
      assertFalse(aql.RELATIONAL_GREATER(1, '0'));
      assertFalse(aql.RELATIONAL_GREATER(1, '1'));
      assertFalse(aql.RELATIONAL_GREATER(1, '-1'));
      assertFalse(aql.RELATIONAL_GREATER(1, 'true'));
      assertFalse(aql.RELATIONAL_GREATER(1, 'false'));
      assertFalse(aql.RELATIONAL_GREATER(1, 'null'));
      assertFalse(aql.RELATIONAL_GREATER(0, '-1'));
      assertFalse(aql.RELATIONAL_GREATER(0, '-100'));
      assertFalse(aql.RELATIONAL_GREATER(0, '-1.1'));
      assertFalse(aql.RELATIONAL_GREATER(0, '-0.0'));
      assertFalse(aql.RELATIONAL_GREATER(1000, '-1'));
      assertFalse(aql.RELATIONAL_GREATER(1000, '10'));
      assertFalse(aql.RELATIONAL_GREATER(1000, '10000'));
      assertFalse(aql.RELATIONAL_GREATER(0, [ ]));
      assertFalse(aql.RELATIONAL_GREATER(0, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER(10, [ ]));
      assertFalse(aql.RELATIONAL_GREATER(100, [ ]));
      assertFalse(aql.RELATIONAL_GREATER(100, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER(100, [ 0, 1 ]));
      assertFalse(aql.RELATIONAL_GREATER(100, [ 99 ]));
      assertFalse(aql.RELATIONAL_GREATER(100, [ 100 ]));
      assertFalse(aql.RELATIONAL_GREATER(100, [ 101 ]));
      assertFalse(aql.RELATIONAL_GREATER(100, { }));
      assertFalse(aql.RELATIONAL_GREATER(100, { 'a' : 0 }));
      assertFalse(aql.RELATIONAL_GREATER(100, { 'a' : 1 }));
      assertFalse(aql.RELATIONAL_GREATER(100, { 'a' : 99 }));
      assertFalse(aql.RELATIONAL_GREATER(100, { 'a' : 100 }));
      assertFalse(aql.RELATIONAL_GREATER(100, { 'a' : 101 }));
      assertFalse(aql.RELATIONAL_GREATER(100, { 'a' : 1000 }));
      assertFalse(aql.RELATIONAL_GREATER('', ''));
      assertFalse(aql.RELATIONAL_GREATER(' ', ' '));
      assertFalse(aql.RELATIONAL_GREATER('a', 'a'));
      assertFalse(aql.RELATIONAL_GREATER(' a', ' a'));
      assertFalse(aql.RELATIONAL_GREATER('abcd', 'abcd'));
      assertFalse(aql.RELATIONAL_GREATER('', ' '));
      assertFalse(aql.RELATIONAL_GREATER('0', 'a'));
      assertFalse(aql.RELATIONAL_GREATER('a', 'a '));
      assertFalse(aql.RELATIONAL_GREATER('a', 'b'));
      assertFalse(aql.RELATIONAL_GREATER('A', 'a'));
      assertFalse(aql.RELATIONAL_GREATER('AB', 'Ab'));
      assertFalse(aql.RELATIONAL_GREATER('abcd', 'bbcd'));
      assertFalse(aql.RELATIONAL_GREATER('abcd', 'abda'));
      assertFalse(aql.RELATIONAL_GREATER('abcd', 'abdd'));
      assertFalse(aql.RELATIONAL_GREATER('abcd', 'abcde'));
      assertFalse(aql.RELATIONAL_GREATER('0abcd', 'abcde'));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ ]));
      assertFalse(aql.RELATIONAL_GREATER([ 0 ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER([ 1 ], [ 1 ]));
      assertFalse(aql.RELATIONAL_GREATER([ true ], [ true ]));
      assertFalse(aql.RELATIONAL_GREATER([ false ], [ false ]));
      assertFalse(aql.RELATIONAL_GREATER([ [ 0, 1, 2 ] ], [ [ 0, 1, 2 ] ]));
      assertFalse(aql.RELATIONAL_GREATER([ [ 1, [ "true", 0, -99 , false ] ], 4 ], [ [ 1, [ "true", 0, -99, false ] ], 4 ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER([ 0 ], [ 1 ]));
      assertFalse(aql.RELATIONAL_GREATER([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertFalse(aql.RELATIONAL_GREATER([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertFalse(aql.RELATIONAL_GREATER([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertFalse(aql.RELATIONAL_GREATER([ 0, 1, 4 ], [ 1 ]));
      assertFalse(aql.RELATIONAL_GREATER([ 15, 99 ], [ 110 ]));
      assertFalse(aql.RELATIONAL_GREATER([ 15, 99 ], [ 15, 100 ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ undefined ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ null ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ false ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ true ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ -1 ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ '' ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ '0' ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ 'abcd' ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ [ ] ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ [ null ] ]));
      assertFalse(aql.RELATIONAL_GREATER([ ], [ { } ]));
      assertFalse(aql.RELATIONAL_GREATER([ null ], [ false ]));
      assertFalse(aql.RELATIONAL_GREATER([ null ], [ true ]));
      assertFalse(aql.RELATIONAL_GREATER([ null ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER([ null ], [ [ ] ]));
      assertFalse(aql.RELATIONAL_GREATER([ undefined ], [ ]));
      assertFalse(aql.RELATIONAL_GREATER([ null ], [ ]));
      assertFalse(aql.RELATIONAL_GREATER([ false ], [ true ]));
      assertFalse(aql.RELATIONAL_GREATER([ false ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER([ false ], [ -1 ]));
      assertFalse(aql.RELATIONAL_GREATER([ false ], [ '' ]));
      assertFalse(aql.RELATIONAL_GREATER([ false ], [ '0' ]));
      assertFalse(aql.RELATIONAL_GREATER([ false ], [ 'abcd' ]));
      assertFalse(aql.RELATIONAL_GREATER([ false ], [ [ ] ]));
      assertFalse(aql.RELATIONAL_GREATER([ false ], [ [ false ] ]));
      assertFalse(aql.RELATIONAL_GREATER([ true ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATER([ true ], [ -1 ]));
      assertFalse(aql.RELATIONAL_GREATER([ true ], [ '' ]));
      assertFalse(aql.RELATIONAL_GREATER([ true ], [ '0' ]));
      assertFalse(aql.RELATIONAL_GREATER([ true ], [ 'abcd' ]));
      assertFalse(aql.RELATIONAL_GREATER([ true ], [ [ ] ]));
      assertFalse(aql.RELATIONAL_GREATER([ true ], [ [ false ] ]));
      assertFalse(aql.RELATIONAL_GREATER([ false, false ], [ true ]));
      assertFalse(aql.RELATIONAL_GREATER([ false, false ], [ false, true ]));
      assertFalse(aql.RELATIONAL_GREATER([ false, false ], [ false, 0 ]));
      assertFalse(aql.RELATIONAL_GREATER([ null, null ], [ null, false ]));
      assertFalse(aql.RELATIONAL_GREATER({ 'a' : 1, 'b' : 2, 'c': null }, { 'b' : 2, 'a' : 1 }));
      assertFalse(aql.RELATIONAL_GREATER({ 'a' : 1, 'b' : 2, 'c' : null }, { 'a' : 1, 'b' : 2 }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_LESSEQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalLessEqualTrue: function() {
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, undefined));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, null));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, undefined));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, true));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, false));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, 0.0));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, 1.0));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, -1.0));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, ''));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, '0'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, '1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, [ ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, [ 0, 1 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, [ 1, 2 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, { 'a' : 0 }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, { 'a' : 1 }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, { '0' : false }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(NaN, false));
      assertTrue(aql.RELATIONAL_LESSEQUAL(NaN, true));
      assertTrue(aql.RELATIONAL_LESSEQUAL(NaN, ''));
      assertTrue(aql.RELATIONAL_LESSEQUAL(NaN, 0));
      assertTrue(aql.RELATIONAL_LESSEQUAL(NaN, null));
      assertTrue(aql.RELATIONAL_LESSEQUAL(NaN, undefined));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, NaN));
      assertTrue(aql.RELATIONAL_LESSEQUAL(undefined, NaN));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, false));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, true));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, 0));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, 1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, -1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, ''));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, ' '));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, '1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, '0'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, 'abcd'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, 'null'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, [ ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, [ true ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, [ false ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, [ null ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, { 'a' : null }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(null, null));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, true));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, 0));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, 1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, -1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, ''));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, ' '));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, '1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, '0'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, 'abcd'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, 'true'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, [ ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, [ true ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, [ false ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, [ null ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, { 'a' : null }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(false, false));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, 0));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, 1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, -1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, ''));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, ' '));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, '1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, '0'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, 'abcd'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, 'true'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, [ ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, [ true ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, [ false ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, [ null ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, { 'a' : null }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(true, true));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, 1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, 2));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, 100));
      assertTrue(aql.RELATIONAL_LESSEQUAL(20, 100));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-100, 1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-100, -10));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-11, -10));
      assertTrue(aql.RELATIONAL_LESSEQUAL(999, 1000));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-1, 1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-1, 0));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1.0, 1.01));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1.111, 1.2));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-1.111, -1.110));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-1.111, -1.1109));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, 0));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, 1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(2, 2));
      assertTrue(aql.RELATIONAL_LESSEQUAL(20, 20));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, 100));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-100, -100));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-11, -11));
      assertTrue(aql.RELATIONAL_LESSEQUAL(999, 999));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-1, -1));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1.0, 1.0));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1.111, 1.111));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1.2, 1.2));
      assertTrue(aql.RELATIONAL_LESSEQUAL(-1.111, -1.111));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, ''));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, ' '));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, '0'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, '1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, '-1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, 'true'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, 'false'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, 'null'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, ''));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, ' '));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, '0'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, '1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, '-1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, 'true'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, 'false'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1, 'null'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, '-1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, '-100'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, '-1.1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, '-0.0'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1000, '-1'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1000, '10'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(1000, '10000'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, [ ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(0, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(10, [ ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, [ ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, [ 0, 1 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, [ 99 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, [ 100 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, [ 101 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, { 'a' : 0 }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, { 'a' : 1 }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, { 'a' : 99 }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, { 'a' : 100 }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, { 'a' : 101 }));
      assertTrue(aql.RELATIONAL_LESSEQUAL(100, { 'a' : 1000 }));
      assertTrue(aql.RELATIONAL_LESSEQUAL('', ' '));
      assertTrue(aql.RELATIONAL_LESSEQUAL('0', 'a'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('a', 'a '));
      assertTrue(aql.RELATIONAL_LESSEQUAL('a', 'b'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('A', 'a'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('AB', 'Ab'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('abcd', 'bbcd'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('abcd', 'abda'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('abcd', 'abdd'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('abcd', 'abcde'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('0abcd', 'abcde'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(' abcd', 'abcd'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('', ''));
      assertTrue(aql.RELATIONAL_LESSEQUAL('0', '0'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('a', 'a'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('A', 'A'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('AB', 'AB'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('Ab', 'Ab'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('abcd', 'abcd'));
      assertTrue(aql.RELATIONAL_LESSEQUAL('0abcd', '0abcd'));
      assertTrue(aql.RELATIONAL_LESSEQUAL(' ', ' '));
      assertTrue(aql.RELATIONAL_LESSEQUAL('  ', '  '));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 0 ], [ 1 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 0, 1, 4 ], [ 1 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 15, 99 ], [ 110 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 15, 99 ], [ 15, 100 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ undefined ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ null ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ false ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ true ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ -1 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ '' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ '0' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ 'abcd' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ [ ] ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ [ null ] ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ { } ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], [ ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 0 ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 2 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 15, 99 ], [ 15, 99 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ null ], [ null ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ [ [ null, 1, 9 ], [ 12, "true", false ] ] , 0 ], [ [ [ null, 1, 9 ], [ 12, "true", false ] ] ,0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], [ false ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false, true ], [ false, true ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ null ], [ false ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ null ], [ true ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ null ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ null ], [ [ ] ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], [ true ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], [ -1 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], [ '' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], [ '0' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], [ 'abcd' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], [ [ ] ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], [ [ false ] ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ true ], [ 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ true ], [ -1 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ true ], [ '' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ true ], [ '0' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ true ], [ 'abcd' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ true ], [ [ ] ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ true ], [ [ false ] ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false, false ], [ true ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false, false ], [ false, true ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false, false ], [ false, 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ null, null ], [ null, false ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], { 'a' : true }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], { 'a' : null }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ ], { 'a' : false }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ '' ], { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 0 ], { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ null ], { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ false ], { 'a' : false }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ true ], { 'a' : false }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 'abcd' ], { 'a' : false }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5 ], { 'a' : false }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6 ], { 'a' : 2, 'b' : 2 }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, 7 ], { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, false ], [ 5, 6, true ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, 0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, 999 ], [ 5, 6, '' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, 'a' ], [ 5, 6, 'b' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, 'A' ], [ 5, 6, 'a' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, '' ], [ 5, 6, 'a' ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, 9, 9 ], [ 5, 6, [ ] ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, [ ] ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, { } ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ 5, 6, 9, 9 ], [ 5, 6, { } ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL({ }, { }));
      assertTrue(aql.RELATIONAL_LESSEQUAL({ 'A' : true }, { 'A' : true }));
      assertTrue(aql.RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : false }, { 'a' : true, 'b' : false }));
      assertTrue(aql.RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : false }, { 'b' : false, 'a' : true }));
      assertTrue(aql.RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : { 'c' : 1, 'f' : 2 }, 'x' : 9 }, { 'x' : 9, 'b' : { 'f' : 2, 'c' : 1 }, 'a' : true }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_LESSEQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalLessEqualFalse: function() {
      assertFalse(aql.RELATIONAL_LESSEQUAL(true, undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL(false, undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL(0.0, undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL(1.0, undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL(-1.0, undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL('', undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL('0', undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL('1', undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ ], undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0, 1 ], undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 1, 2 ], undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ }, undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : 0 }, undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : 1 }, undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ '0' : false }, undefined));
      assertFalse(aql.RELATIONAL_LESSEQUAL(false, NaN));
      assertFalse(aql.RELATIONAL_LESSEQUAL(true, NaN));
      assertFalse(aql.RELATIONAL_LESSEQUAL('', NaN));
      assertFalse(aql.RELATIONAL_LESSEQUAL(0, NaN));
      assertFalse(aql.RELATIONAL_LESSEQUAL(false, null));
      assertFalse(aql.RELATIONAL_LESSEQUAL(true, null));
      assertFalse(aql.RELATIONAL_LESSEQUAL(0, null));
      assertFalse(aql.RELATIONAL_LESSEQUAL(1, null));
      assertFalse(aql.RELATIONAL_LESSEQUAL(-1, null));
      assertFalse(aql.RELATIONAL_LESSEQUAL('', null));
      assertFalse(aql.RELATIONAL_LESSEQUAL(' ', null));
      assertFalse(aql.RELATIONAL_LESSEQUAL('1', null));
      assertFalse(aql.RELATIONAL_LESSEQUAL('0', null));
      assertFalse(aql.RELATIONAL_LESSEQUAL('abcd', null));
      assertFalse(aql.RELATIONAL_LESSEQUAL('null', null));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ ], null));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ true ], null));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ false ], null));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ null ], null));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], null));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ }, null));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : null }, null));
      assertFalse(aql.RELATIONAL_LESSEQUAL(true, false));
      assertFalse(aql.RELATIONAL_LESSEQUAL(0, false));
      assertFalse(aql.RELATIONAL_LESSEQUAL(1, false));
      assertFalse(aql.RELATIONAL_LESSEQUAL(-1, false));
      assertFalse(aql.RELATIONAL_LESSEQUAL('', false));
      assertFalse(aql.RELATIONAL_LESSEQUAL(' ', false));
      assertFalse(aql.RELATIONAL_LESSEQUAL('1', false));
      assertFalse(aql.RELATIONAL_LESSEQUAL('0', false));
      assertFalse(aql.RELATIONAL_LESSEQUAL('abcd', false));
      assertFalse(aql.RELATIONAL_LESSEQUAL('true', false));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ ], false));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ true ], false));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ false ], false));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ null ], false));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], false));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ }, false));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : null }, false));
      assertFalse(aql.RELATIONAL_LESSEQUAL(0, true));
      assertFalse(aql.RELATIONAL_LESSEQUAL(1, true));
      assertFalse(aql.RELATIONAL_LESSEQUAL(-1, true));
      assertFalse(aql.RELATIONAL_LESSEQUAL('', true));
      assertFalse(aql.RELATIONAL_LESSEQUAL(' ', true));
      assertFalse(aql.RELATIONAL_LESSEQUAL('1', true));
      assertFalse(aql.RELATIONAL_LESSEQUAL('0', true));
      assertFalse(aql.RELATIONAL_LESSEQUAL('abcd', true));
      assertFalse(aql.RELATIONAL_LESSEQUAL('true', true));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ ], true));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ true ], true));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ false ], true));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ null ], true));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], true));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ }, true));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : null }, true));
      assertFalse(aql.RELATIONAL_LESSEQUAL(1, 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL(2, 1));
      assertFalse(aql.RELATIONAL_LESSEQUAL(100, 1));
      assertFalse(aql.RELATIONAL_LESSEQUAL(100, 20));
      assertFalse(aql.RELATIONAL_LESSEQUAL(1, -100));
      assertFalse(aql.RELATIONAL_LESSEQUAL(-10, -100));
      assertFalse(aql.RELATIONAL_LESSEQUAL(-10, -11));
      assertFalse(aql.RELATIONAL_LESSEQUAL(1000, 999));
      assertFalse(aql.RELATIONAL_LESSEQUAL(1, -1));
      assertFalse(aql.RELATIONAL_LESSEQUAL(0, -1));
      assertFalse(aql.RELATIONAL_LESSEQUAL(1.01, 1.0));
      assertFalse(aql.RELATIONAL_LESSEQUAL(1.2, 1.111));
      assertFalse(aql.RELATIONAL_LESSEQUAL(-1.110, -1.111));
      assertFalse(aql.RELATIONAL_LESSEQUAL(-1.1109, -1.111));
      assertFalse(aql.RELATIONAL_LESSEQUAL('', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL(' ', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('0', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('1', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('-1', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('true', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('false', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('null', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('', 1));
      assertFalse(aql.RELATIONAL_LESSEQUAL(' ', 1));
      assertFalse(aql.RELATIONAL_LESSEQUAL('0', 1));
      assertFalse(aql.RELATIONAL_LESSEQUAL('1', 1));
      assertFalse(aql.RELATIONAL_LESSEQUAL('-1', 1));
      assertFalse(aql.RELATIONAL_LESSEQUAL('true', 1));
      assertFalse(aql.RELATIONAL_LESSEQUAL('false', 1));
      assertFalse(aql.RELATIONAL_LESSEQUAL('null', 1));
      assertFalse(aql.RELATIONAL_LESSEQUAL('-1', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('-100', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('-1.1', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('-0.0', 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL('-1', 1000));
      assertFalse(aql.RELATIONAL_LESSEQUAL('10', 1000));
      assertFalse(aql.RELATIONAL_LESSEQUAL('10000', 1000));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ ], 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], 0));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ ], 10));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ ], 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0, 1 ], 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 99 ], 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 100 ], 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 101 ], 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ }, 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : 0 }, 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : 1 }, 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : 99 }, 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : 100 }, 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : 101 }, 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : 1000 }, 100));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : false }, 'zz'));
      assertFalse(aql.RELATIONAL_LESSEQUAL({ 'a' : 'a' }, 'zz'));
      assertFalse(aql.RELATIONAL_LESSEQUAL(' ', ''));
      assertFalse(aql.RELATIONAL_LESSEQUAL('a', '0'));
      assertFalse(aql.RELATIONAL_LESSEQUAL('a ', 'a'));
      assertFalse(aql.RELATIONAL_LESSEQUAL('b', 'a'));
      assertFalse(aql.RELATIONAL_LESSEQUAL('a', 'A'));
      assertFalse(aql.RELATIONAL_LESSEQUAL('Ab', 'AB'));
      assertFalse(aql.RELATIONAL_LESSEQUAL('bbcd', 'abcd'));
      assertFalse(aql.RELATIONAL_LESSEQUAL('abda', 'abcd'));
      assertFalse(aql.RELATIONAL_LESSEQUAL('abdd', 'abcd'));
      assertFalse(aql.RELATIONAL_LESSEQUAL('abcde', 'abcd'));
      assertFalse(aql.RELATIONAL_LESSEQUAL('abcde', '0abcde'));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 1 ], [ 0 ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 1 ], [ 0, 1, 4 ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 110 ], [ 15, 99 ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 15, 100 ], [ 15, 99 ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ false ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ true ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ -1 ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ '' ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ '0' ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 'abcd' ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ [ ] ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ [ null ] ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ { } ], [ ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ false ], [ null ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ true ], [ null ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], [ null ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ [ ] ], [ null ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ true ], [ false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], [ false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ -1 ], [ false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ '' ], [ false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ '0' ], [ false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 'abcd' ], [ false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ [ ] ], [ false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ [ false ] ], [ false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 0 ], [ true ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ -1 ], [ true ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ '' ], [ true ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ '0' ], [ true ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ 'abcd' ], [ true ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ [ ] ], [ true ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ [ false] ], [ true ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ true ], [ false, false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ false, true ], [ false, false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ false, 0 ], [ false, false ]));
      assertFalse(aql.RELATIONAL_LESSEQUAL([ null, false ], [ null, null ]));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_GREATEREQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterEqualTrue: function() {
      assertTrue(aql.RELATIONAL_GREATEREQUAL(undefined, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(undefined, null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(null, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(NaN, null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(NaN, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(null, NaN));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(undefined, NaN));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(true, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(false, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(0.0, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1.0, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-1.0, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('', undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('0', undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('1', undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(false, NaN));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(true, NaN));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('', NaN));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(0, NaN));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ ], undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0, 1 ], undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 1, 2 ], undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 0 }, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 1 }, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ '0' : false }, undefined));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(false, null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(true, null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(0, null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1, null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-1, null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('', null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(' ', null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('1', null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('0', null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('abcd', null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('null', null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ ], null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ true ], null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ false ], null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ null ], null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : null }, null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(true, false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(0, false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1, false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-1, false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('', false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(' ', false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('1', false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('0', false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('abcd', false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('true', false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ ], false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ true ], false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ false ], false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ null ], false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : null }, false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(0, true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1, true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-1, true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('', true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(' ', true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('1', true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('0', true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('abcd', true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('true', true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ ], true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ true ], true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ false ], true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ null ], true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : null }, true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1, 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(2, 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(100, 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(100, 20));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1, -100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-10, -100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-10, -11));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1000, 999));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1, -1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(0, -1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1.01, 1.0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1.2, 1.111));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-1.110, -1.111));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-1.1109, -1.111));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(' ', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('0', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('1', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('-1', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('true', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('false', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('null', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('', 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(' ', 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('0', 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('1', 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('-1', 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('true', 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('false', 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('null', 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('-1', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('-100', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('-1.1', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('-0.0', 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('-1', 1000));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('10', 1000));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('10000', 1000));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ ], 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ ], 10));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ ], 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0, 1 ], 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 99 ], 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 100 ], 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 101 ], 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 0 }, 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 1 }, 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 99 }, 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 100 }, 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 101 }, 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 1000 }, 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : false }, 'zz'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 'a' }, 'zz'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(' ', ''));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('a', '0'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('a ', 'a'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('b', 'a'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('a', 'A'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('Ab', 'AB'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('bbcd', 'abcd'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('abda', 'abcd'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('abdd', 'abcd'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('abcde', 'abcd'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('abcde', '0abcde'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 1 ], [ 0 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 1 ], [ 0, 1, 4 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 110 ], [ 15, 99 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 15, 100 ], [ 15, 99 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ undefined ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ null ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ false ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ true ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ -1 ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ '' ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ '0' ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 'abcd' ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ [ ] ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ [ null ] ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ { } ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ false ], [ null ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ true ], [ null ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], [ null ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ [ ] ], [ null ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ true ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ -1 ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ '' ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ '0' ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 'abcd' ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ [ ] ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ [ false ] ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ -1 ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ '' ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ '0' ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 'abcd' ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ [ ] ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ [ false] ], [ true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ true ], [ false, false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ false, true ], [ false, false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ false, 0 ], [ false, false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ null, false ], [ null, null ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(null, null));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(false, false));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(true, true));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(0, 0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1, 1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(2, 2));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(20, 20));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(100, 100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-100, -100));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-11, -11));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(999, 999));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-1, -1));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1.0, 1.0));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1.111, 1.111));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(1.2, 1.2));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(-1.111, -1.111));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('', ''));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('0', '0'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('a', 'a'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('A', 'A'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('AB', 'AB'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('Ab', 'Ab'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('abcd', 'abcd'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('0abcd', '0abcd'));
      assertTrue(aql.RELATIONAL_GREATEREQUAL(' ', ' '));
      assertTrue(aql.RELATIONAL_GREATEREQUAL('  ', '  '));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0 ], [ 0 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 2 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 15, 99 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ null ], [ null ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ [ [ null, 1, 9 ], [ 12, "true", false ] ] , 0 ], [ [ [ null, 1, 9 ], [ 12, "true", false ] ] ,0 ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ undefined ], [ ]));
      assertTrue(aql.RELATIONAL_LESSEQUAL([ null ], [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ ], [ undefined ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ ], [ null ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ false ], [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ false, true ], [ false, true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : true }, [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : null }, [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : false }, [ ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, [ '' ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, [ 0 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, [ null ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : false }, [ false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : false }, [ true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : false }, [ 'abcd' ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : false }, [ 5 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 2, 'b' : 2 }, [ 5, 6 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, [ 5, 6, 7 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 5, 6, true ], [ 5, 6, false ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 5, 6, 0 ], [ 5, 6, true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 5, 6, '' ], [ 5, 6, 999 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 5, 6, 'b' ], [ 5, 6, 'a' ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 5, 6, 'a' ], [ 5, 6, 'A' ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 5, 6, 'a' ], [ 5, 6, '' ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 5, 6, [ ] ], [ 5, 6, 9, 9 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 5, 6, [ ] ], [ 5, 6, true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 5, 6, { } ], [ 5, 6, true ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL([ 5, 6, { } ], [ 5, 6, 9, 9 ]));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 0 }, { }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 2 }, { 'a' : 1 }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'A' : 2 }, { 'a' : 1 }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'A' : 1 }, { 'a' : 2 }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 1, 'b' : 2, 'c' : null }, { 'a' : 1, 'b' : 2 }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 1 }, { 'b' : 1 }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : 1, 'b' : 2, 'c': null }, { 'b' : 2, 'a' : 1 }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : false }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : true }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ ], 'b' : true }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : [ 10, 1 ] }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ }, { }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'A' : true }, { 'A' : true }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : false }, { 'a' : true, 'b' : false }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : false }, { 'b' : false, 'a' : true }));
      assertTrue(aql.RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : { 'c' : 1, 'f' : 2 }, 'x' : 9 }, { 'x' : 9, 'b' : { 'f' : 2, 'c' : 1 }, 'a' : true }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_GREATEREQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterEqualFalse: function() {
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, true));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, false));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, 0.0));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, 1.0));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, -1.0));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, ''));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, '0'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, '1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, [ ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, [ 0, 1 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, [ 1, 2 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, { }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, { 'a' : 0 }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, { 'a' : 1 }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(undefined, { '0' : false }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(NaN, false));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(NaN, true));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(NaN, ''));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(NaN, 0));

      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, false));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, true));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, 0));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, 1));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, -1));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, ''));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, ' '));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, '1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, '0'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, 'abcd'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, 'null'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, [ ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, [ true ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, [ false ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, [ null ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, { }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(null, { 'a' : null }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, true));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, 0));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, 1));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, -1));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, ''));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, ' '));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, '1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, '0'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, 'abcd'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, 'true'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, [ ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, [ true ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, [ false ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, [ null ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, { }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(false, { 'a' : null }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, 0));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, 1));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, -1));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, ''));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, ' '));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, '1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, '0'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, 'abcd'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, 'true'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, [ ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, [ true ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, [ false ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, [ null ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, { }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(true, { 'a' : null }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, 1));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1, 2));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1, 100));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(20, 100));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(-100, 1));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(-100, -10));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(-11, -10));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(999, 1000));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(-1, 1));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(-1, 0));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1.0, 1.01));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1.111, 1.2));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(-1.111, -1.110));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(-1.111, -1.1109));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, ''));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, ' '));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, '0'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, '1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, '-1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, 'true'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, 'false'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, 'null'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1, ''));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1, ' '));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1, '0'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1, '1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1, '-1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1, 'true'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1, 'false'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1, 'null'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, '-1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, '-100'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, '-1.1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, '-0.0'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1000, '-1'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1000, '10'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(1000, '10000'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, [ ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(0, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(10, [ ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, [ ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, [ 0, 1 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, [ 99 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, [ 100 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, [ 101 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, { }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, { 'a' : 0 }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, { 'a' : 1 }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, { 'a' : 99 }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, { 'a' : 100 }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, { 'a' : 101 }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(100, { 'a' : 1000 }));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('', ' '));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('0', 'a'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('a', 'a '));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('a', 'b'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('A', 'a'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('AB', 'Ab'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('abcd', 'bbcd'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('abcd', 'abda'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('abcd', 'abdd'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('abcd', 'abcde'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL('0abcd', 'abcde'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL(' abcd', 'abcd'));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ 0 ], [ 1 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ 0, 1, 4 ], [ 1 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 110 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 15, 100 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ false ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ true ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ -1 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ '' ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ '0' ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ 'abcd' ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ [ ] ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ [ null ] ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ ], [ { } ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ null ], [ false ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ null ], [ true ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ null ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ null ], [ [ ] ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false ], [ true ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false ], [ -1 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false ], [ '' ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false ], [ '0' ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false ], [ 'abcd' ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false ], [ [ ] ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false ], [ [ false ] ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ true ], [ 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ true ], [ -1 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ true ], [ '' ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ true ], [ '0' ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ true ], [ 'abcd' ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ true ], [ [ ] ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ true ], [ [ false ] ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false, false ], [ true ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false, false ], [ false, true ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ false, false ], [ false, 0 ]));
      assertFalse(aql.RELATIONAL_GREATEREQUAL([ null, null ], [ null, false ]));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_IN function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalInUndefined: function() {
      assertFalse(aql.RELATIONAL_IN(undefined, undefined));
      assertFalse(aql.RELATIONAL_IN(undefined, null));
      assertFalse(aql.RELATIONAL_IN(undefined, true));
      assertFalse(aql.RELATIONAL_IN(undefined, false));
      assertFalse(aql.RELATIONAL_IN(undefined, 0.0));
      assertFalse(aql.RELATIONAL_IN(undefined, 1.0));
      assertFalse(aql.RELATIONAL_IN(undefined, -1.0));
      assertFalse(aql.RELATIONAL_IN(undefined, ''));
      assertFalse(aql.RELATIONAL_IN(undefined, '0'));
      assertFalse(aql.RELATIONAL_IN(undefined, '1'));
      assertFalse(aql.RELATIONAL_IN(undefined, { }));
      assertFalse(aql.RELATIONAL_IN(undefined, { 'a' : 0 }));
      assertFalse(aql.RELATIONAL_IN(undefined, { 'a' : 1 }));
      assertFalse(aql.RELATIONAL_IN(undefined, { '0' : false }));
      assertFalse(aql.RELATIONAL_IN(null, undefined));
      assertFalse(aql.RELATIONAL_IN(true, undefined));
      assertFalse(aql.RELATIONAL_IN(false, undefined));
      assertFalse(aql.RELATIONAL_IN(0.0, undefined));
      assertFalse(aql.RELATIONAL_IN(1.0, undefined));
      assertFalse(aql.RELATIONAL_IN(-1.0, undefined));
      assertFalse(aql.RELATIONAL_IN('', undefined));
      assertFalse(aql.RELATIONAL_IN('0', undefined));
      assertFalse(aql.RELATIONAL_IN('1', undefined));
      assertFalse(aql.RELATIONAL_IN([ ], undefined));
      assertFalse(aql.RELATIONAL_IN([ 0 ], undefined));
      assertFalse(aql.RELATIONAL_IN([ 0, 1 ], undefined));
      assertFalse(aql.RELATIONAL_IN([ 1, 2 ], undefined));
      assertFalse(aql.RELATIONAL_IN({ }, undefined));
      assertFalse(aql.RELATIONAL_IN({ 'a' : 0 }, undefined));
      assertFalse(aql.RELATIONAL_IN({ 'a' : 1 }, undefined));
      assertFalse(aql.RELATIONAL_IN({ '0' : false }, undefined));
      assertFalse(aql.RELATIONAL_IN(NaN, false));
      assertFalse(aql.RELATIONAL_IN(NaN, true));
      assertFalse(aql.RELATIONAL_IN(NaN, ''));
      assertFalse(aql.RELATIONAL_IN(NaN, 0));
      assertFalse(aql.RELATIONAL_IN(NaN, null));
      assertFalse(aql.RELATIONAL_IN(NaN, undefined));
      assertFalse(aql.RELATIONAL_IN(false, NaN));
      assertFalse(aql.RELATIONAL_IN(true, NaN));
      assertFalse(aql.RELATIONAL_IN('', NaN));
      assertFalse(aql.RELATIONAL_IN(0, NaN));
      assertFalse(aql.RELATIONAL_IN(null, NaN));
      assertFalse(aql.RELATIONAL_IN(undefined, NaN));
      
      assertFalse(aql.RELATIONAL_IN(null, null));
      assertFalse(aql.RELATIONAL_IN(null, false));
      assertFalse(aql.RELATIONAL_IN(null, true));
      assertFalse(aql.RELATIONAL_IN(null, 0));
      assertFalse(aql.RELATIONAL_IN(null, 1));
      assertFalse(aql.RELATIONAL_IN(null, ''));
      assertFalse(aql.RELATIONAL_IN(null, '1'));
      assertFalse(aql.RELATIONAL_IN(null, 'a'));
      assertFalse(aql.RELATIONAL_IN(null, { }));
      assertFalse(aql.RELATIONAL_IN(null, { 'A' : true }));
      assertFalse(aql.RELATIONAL_IN(false, null));
      assertFalse(aql.RELATIONAL_IN(false, false));
      assertFalse(aql.RELATIONAL_IN(false, true));
      assertFalse(aql.RELATIONAL_IN(false, 0));
      assertFalse(aql.RELATIONAL_IN(false, 1));
      assertFalse(aql.RELATIONAL_IN(false, ''));
      assertFalse(aql.RELATIONAL_IN(false, '1'));
      assertFalse(aql.RELATIONAL_IN(false, 'a'));
      assertFalse(aql.RELATIONAL_IN(false, { }));
      assertFalse(aql.RELATIONAL_IN(false, { 'A' : true }));
      assertFalse(aql.RELATIONAL_IN(true, null));
      assertFalse(aql.RELATIONAL_IN(true, false));
      assertFalse(aql.RELATIONAL_IN(true, true));
      assertFalse(aql.RELATIONAL_IN(true, 0));
      assertFalse(aql.RELATIONAL_IN(true, 1));
      assertFalse(aql.RELATIONAL_IN(true, ''));
      assertFalse(aql.RELATIONAL_IN(true, '1'));
      assertFalse(aql.RELATIONAL_IN(true, 'a'));
      assertFalse(aql.RELATIONAL_IN(true, { }));
      assertFalse(aql.RELATIONAL_IN(true, { 'A' : true }));
      assertFalse(aql.RELATIONAL_IN(0, null));
      assertFalse(aql.RELATIONAL_IN(0, false));
      assertFalse(aql.RELATIONAL_IN(0, true));
      assertFalse(aql.RELATIONAL_IN(0, 0));
      assertFalse(aql.RELATIONAL_IN(0, 1));
      assertFalse(aql.RELATIONAL_IN(0, ''));
      assertFalse(aql.RELATIONAL_IN(0, '1'));
      assertFalse(aql.RELATIONAL_IN(0, 'a'));
      assertFalse(aql.RELATIONAL_IN(0, { }));
      assertFalse(aql.RELATIONAL_IN(0, { 'A' : true }));
      assertFalse(aql.RELATIONAL_IN(1, null));
      assertFalse(aql.RELATIONAL_IN(1, false));
      assertFalse(aql.RELATIONAL_IN(1, true));
      assertFalse(aql.RELATIONAL_IN(1, 0));
      assertFalse(aql.RELATIONAL_IN(1, 1));
      assertFalse(aql.RELATIONAL_IN(1, ''));
      assertFalse(aql.RELATIONAL_IN(1, '1'));
      assertFalse(aql.RELATIONAL_IN(1, 'a'));
      assertFalse(aql.RELATIONAL_IN(1, { }));
      assertFalse(aql.RELATIONAL_IN(1, { 'A' : true }));
      assertFalse(aql.RELATIONAL_IN('', null));
      assertFalse(aql.RELATIONAL_IN('', false));
      assertFalse(aql.RELATIONAL_IN('', true));
      assertFalse(aql.RELATIONAL_IN('', 0));
      assertFalse(aql.RELATIONAL_IN('', 1));
      assertFalse(aql.RELATIONAL_IN('', ''));
      assertFalse(aql.RELATIONAL_IN('', '1'));
      assertFalse(aql.RELATIONAL_IN('', 'a'));
      assertFalse(aql.RELATIONAL_IN('', { }));
      assertFalse(aql.RELATIONAL_IN('', { 'A' : true }));
      assertFalse(aql.RELATIONAL_IN('a', null));
      assertFalse(aql.RELATIONAL_IN('a', false));
      assertFalse(aql.RELATIONAL_IN('a', true));
      assertFalse(aql.RELATIONAL_IN('a', 0));
      assertFalse(aql.RELATIONAL_IN('a', 1));
      assertFalse(aql.RELATIONAL_IN('a', ''));
      assertFalse(aql.RELATIONAL_IN('a', '1'));
      assertFalse(aql.RELATIONAL_IN('a', 'a'));
      assertFalse(aql.RELATIONAL_IN('a', { }));
      assertFalse(aql.RELATIONAL_IN('a', { 'A' : true }));
      assertFalse(aql.RELATIONAL_IN([ ], null));
      assertFalse(aql.RELATIONAL_IN([ ], false));
      assertFalse(aql.RELATIONAL_IN([ ], true));
      assertFalse(aql.RELATIONAL_IN([ ], 0));
      assertFalse(aql.RELATIONAL_IN([ ], 1));
      assertFalse(aql.RELATIONAL_IN([ ], ''));
      assertFalse(aql.RELATIONAL_IN([ ], '1'));
      assertFalse(aql.RELATIONAL_IN([ ], 'a'));
      assertFalse(aql.RELATIONAL_IN([ ], { }));
      assertFalse(aql.RELATIONAL_IN([ ], { 'A' : true }));
      assertFalse(aql.RELATIONAL_IN([ 0 ], null));
      assertFalse(aql.RELATIONAL_IN([ 0 ], false));
      assertFalse(aql.RELATIONAL_IN([ 0 ], true));
      assertFalse(aql.RELATIONAL_IN([ 0 ], 0));
      assertFalse(aql.RELATIONAL_IN([ 0 ], 1));
      assertFalse(aql.RELATIONAL_IN([ 0 ], ''));
      assertFalse(aql.RELATIONAL_IN([ 0 ], '1'));
      assertFalse(aql.RELATIONAL_IN([ 0 ], 'a'));
      assertFalse(aql.RELATIONAL_IN([ 0 ], { }));
      assertFalse(aql.RELATIONAL_IN([ 0 ], { 'A' : true }));
      assertFalse(aql.RELATIONAL_IN([ 1 ], null));
      assertFalse(aql.RELATIONAL_IN([ 1 ], false));
      assertFalse(aql.RELATIONAL_IN([ 1 ], true));
      assertFalse(aql.RELATIONAL_IN([ 1 ], 0));
      assertFalse(aql.RELATIONAL_IN([ 1 ], 1));
      assertFalse(aql.RELATIONAL_IN([ 1 ], ''));
      assertFalse(aql.RELATIONAL_IN([ 1 ], '1'));
      assertFalse(aql.RELATIONAL_IN([ 1 ], 'a'));
      assertFalse(aql.RELATIONAL_IN([ 1 ], { }));
      assertFalse(aql.RELATIONAL_IN([ 1 ], { 'A' : true }));
      assertFalse(aql.RELATIONAL_IN({ }, null));
      assertFalse(aql.RELATIONAL_IN({ }, false));
      assertFalse(aql.RELATIONAL_IN({ }, true));
      assertFalse(aql.RELATIONAL_IN({ }, 0));
      assertFalse(aql.RELATIONAL_IN({ }, 1));
      assertFalse(aql.RELATIONAL_IN({ }, ''));
      assertFalse(aql.RELATIONAL_IN({ }, '1'));
      assertFalse(aql.RELATIONAL_IN({ }, 'a'));
      assertFalse(aql.RELATIONAL_IN({ }, { }));
      assertFalse(aql.RELATIONAL_IN({ }, { 'A' : true }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_IN function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalInTrue: function() {
      assertTrue(aql.RELATIONAL_IN(null, [ null ]));
      assertTrue(aql.RELATIONAL_IN(null, [ null, false ]));
      assertTrue(aql.RELATIONAL_IN(null, [ false, null ]));
      assertTrue(aql.RELATIONAL_IN(false, [ false ]));
      assertTrue(aql.RELATIONAL_IN(false, [ true, false ]));
      assertTrue(aql.RELATIONAL_IN(false, [ 0, false ]));
      assertTrue(aql.RELATIONAL_IN(true, [ true ]));
      assertTrue(aql.RELATIONAL_IN(true, [ false, true ]));
      assertTrue(aql.RELATIONAL_IN(true, [ 0, false, true ]));
      assertTrue(aql.RELATIONAL_IN(0, [ 0 ]));
      assertTrue(aql.RELATIONAL_IN(1, [ 1 ]));
      assertTrue(aql.RELATIONAL_IN(0, [ 3, 2, 1, 0 ]));
      assertTrue(aql.RELATIONAL_IN(1, [ 3, 2, 1, 0 ]));
      assertTrue(aql.RELATIONAL_IN(-35.5, [ 3, 2, 1, -35.5, 0 ]));
      assertTrue(aql.RELATIONAL_IN(1.23e32, [ 3, 2, 1, 1.23e32, 35.5, 0 ]));
      assertTrue(aql.RELATIONAL_IN('', [ '' ]));
      assertTrue(aql.RELATIONAL_IN('', [ ' ', '' ]));
      assertTrue(aql.RELATIONAL_IN('', [ 'a', 'b', 'c', '' ]));
      assertTrue(aql.RELATIONAL_IN('A', [ 'c', 'b', 'a', 'A' ]));
      assertTrue(aql.RELATIONAL_IN(' ', [ ' ' ]));
      assertTrue(aql.RELATIONAL_IN(' a', [ ' a' ]));
      assertTrue(aql.RELATIONAL_IN(' a ', [ ' a ' ]));
      assertTrue(aql.RELATIONAL_IN([ ], [ [ ] ]));
      assertTrue(aql.RELATIONAL_IN([ ], [ 1, null, 2, 3, [ ], 5 ]));
      assertTrue(aql.RELATIONAL_IN([ null ], [ [ null ] ]));
      assertTrue(aql.RELATIONAL_IN([ null ], [ null, [ null ], true ]));
      assertTrue(aql.RELATIONAL_IN([ null, false ], [ [ null, false ] ]));
      assertTrue(aql.RELATIONAL_IN([ 'a', 'A', false ], [ 'a', true, [ 'a', 'A', false ] ]));
      assertTrue(aql.RELATIONAL_IN({ }, [ { } ]));
      assertTrue(aql.RELATIONAL_IN({ }, [ 'a', null, false, 0, { } ]));
      assertTrue(aql.RELATIONAL_IN({ 'a' : true }, [ 'a', null, false, 0, { 'a' : true } ]));
      assertTrue(aql.RELATIONAL_IN({ 'a' : true, 'A': false }, [ 'a', null, false, 0, { 'A' : false, 'a' : true } ]));
      assertTrue(aql.RELATIONAL_IN({ 'a' : { 'b' : null, 'c': 1 } }, [ { 'a' : { 'c' : 1, 'b' : null } } ]));
      assertTrue(aql.RELATIONAL_IN({ 'a' : { 'b' : null, 'c': 1 } }, [ 'a', 'b', { 'a' : { 'c' : 1, 'b' : null } } ]));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_IN function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalInFalse: function() {
      assertFalse(aql.RELATIONAL_IN(undefined, [ ]));
      assertFalse(aql.RELATIONAL_IN(undefined, [ 0 ]));
      assertFalse(aql.RELATIONAL_IN(undefined, [ 0, 1 ]));
      assertFalse(aql.RELATIONAL_IN(undefined, [ 1, 2 ]));
      assertFalse(aql.RELATIONAL_IN(null, [ ]));
      assertFalse(aql.RELATIONAL_IN(false, [ ]));
      assertFalse(aql.RELATIONAL_IN(true, [ ]));
      assertFalse(aql.RELATIONAL_IN(0, [ ]));
      assertFalse(aql.RELATIONAL_IN(1, [ ]));
      assertFalse(aql.RELATIONAL_IN('', [ ]));
      assertFalse(aql.RELATIONAL_IN('0', [ ]));
      assertFalse(aql.RELATIONAL_IN('1', [ ]));
      assertFalse(aql.RELATIONAL_IN([ ], [ ]));
      assertFalse(aql.RELATIONAL_IN([ 0 ], [ ]));
      assertFalse(aql.RELATIONAL_IN({ }, [ ]));
      assertFalse(aql.RELATIONAL_IN({ 'a' : true }, [ ]));
      assertFalse(aql.RELATIONAL_IN(null, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, true, false, { }, [ ] ]));
      assertFalse(aql.RELATIONAL_IN(true, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, false, { }, [ ] ]));
      assertFalse(aql.RELATIONAL_IN(false, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, { }, [ ] ]));
      assertFalse(aql.RELATIONAL_IN(0, [ '0', '1', '', 'null', 'true', 'false', -1, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(aql.RELATIONAL_IN(1, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(aql.RELATIONAL_IN(-1, [ '0', '1', '', 'null', 'true', 'false', 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(aql.RELATIONAL_IN('', [ '0', '1', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(aql.RELATIONAL_IN('0', [ '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(aql.RELATIONAL_IN('1', [ '0', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(aql.RELATIONAL_IN([ ], [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { } ]));
      assertFalse(aql.RELATIONAL_IN({ }, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, [ ] ]));
      assertFalse(aql.RELATIONAL_IN(null, [ [ null ]]));
      assertFalse(aql.RELATIONAL_IN(false, [ [ false ]]));
      assertFalse(aql.RELATIONAL_IN(true, [ [ true ]]));
      assertFalse(aql.RELATIONAL_IN(0, [ [ 0 ]]));
      assertFalse(aql.RELATIONAL_IN(1, [ [ 1 ]]));
      assertFalse(aql.RELATIONAL_IN('', [ [ '' ]]));
      assertFalse(aql.RELATIONAL_IN('1', [ [ '1' ]]));
      assertFalse(aql.RELATIONAL_IN('', [ [ '' ]]));
      assertFalse(aql.RELATIONAL_IN(' ', [ [ ' ' ]]));
      assertFalse(aql.RELATIONAL_IN('a', [ 'A' ]));
      assertFalse(aql.RELATIONAL_IN('a', [ 'b', 'c', 'd' ]));
      assertFalse(aql.RELATIONAL_IN('', [ ' ', '  ' ]));
      assertFalse(aql.RELATIONAL_IN(' ', [ '', '  ' ]));
      assertFalse(aql.RELATIONAL_IN([ ], [ 0 ]));
      assertFalse(aql.RELATIONAL_IN([ ], [ 1, 2 ]));
      assertFalse(aql.RELATIONAL_IN([ 0 ], [ 1, 2 ]));
      assertFalse(aql.RELATIONAL_IN([ 1 ], [ 1, 2 ]));
      assertFalse(aql.RELATIONAL_IN([ 2 ], [ 1, 2 ]));
      assertFalse(aql.RELATIONAL_IN([ 1, 2 ], [ 1, 2 ]));
      assertFalse(aql.RELATIONAL_IN([ 1, 2 ], [ [ 1 ], [ 2 ] ]));
      assertFalse(aql.RELATIONAL_IN([ 1, 2 ], [ [ 2, 1 ] ]));
      assertFalse(aql.RELATIONAL_IN([ 1, 2 ], [ [ 1, 2, 3 ] ]));
      assertFalse(aql.RELATIONAL_IN([ 1, 2, 3 ], [ [ 1, 2, 4 ] ]));
      assertFalse(aql.RELATIONAL_IN([ 1, 2, 3 ], [ [ 0, 1, 2, 3 ] ]));
      assertFalse(aql.RELATIONAL_IN({ 'a' : true }, [ { 'a' : true, 'b' : false } ]));
      assertFalse(aql.RELATIONAL_IN({ 'a' : true }, [ { 'a' : false } ]));
      assertFalse(aql.RELATIONAL_IN({ 'a' : true }, [ { 'b' : true } ]));
      assertFalse(aql.RELATIONAL_IN({ 'a' : true }, [ [ { 'a' : true } ] ]));
      assertFalse(aql.RELATIONAL_IN({ 'a' : true }, [ 1, 2, { 'a' : { 'a' : true } } ]));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.UNARY_PLUS function
    ////////////////////////////////////////////////////////////////////////////////

    testUnaryPlusUndefined: function() {
      assertEqual(0, aql.UNARY_PLUS(undefined));
      assertEqual(0, aql.UNARY_PLUS(null));
      assertEqual(0, aql.UNARY_PLUS(NaN));
      assertEqual(0, aql.UNARY_PLUS(false));
      assertEqual(1, aql.UNARY_PLUS(true));
      assertEqual(0, aql.UNARY_PLUS(' '));
      assertEqual(0, aql.UNARY_PLUS('abc'));
      assertEqual(0, aql.UNARY_PLUS('1abc'));
      assertEqual(0, aql.UNARY_PLUS(''));
      assertEqual(-1, aql.UNARY_PLUS('-1'));
      assertEqual(0, aql.UNARY_PLUS('0'));
      assertEqual(1, aql.UNARY_PLUS('1'));
      assertEqual(1.5, aql.UNARY_PLUS('1.5'));
      assertEqual(0, aql.UNARY_PLUS([ ]));
      assertEqual(0, aql.UNARY_PLUS([ 0 ]));
      assertEqual(1, aql.UNARY_PLUS([ 1 ]));
      assertEqual(17, aql.UNARY_PLUS([ 17 ]));
      assertEqual(0, aql.UNARY_PLUS({ 'a' : 1 }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.UNARY_PLUS function
    ////////////////////////////////////////////////////////////////////////////////

    testUnaryPlusValue: function() {
      assertEqual(0, aql.UNARY_PLUS(0));
      assertEqual(1, aql.UNARY_PLUS(1));
      assertEqual(-1, aql.UNARY_PLUS(-1));
      assertEqual(0.0001, aql.UNARY_PLUS(0.0001));
      assertEqual(-0.0001, aql.UNARY_PLUS(-0.0001));
      assertEqual(100, aql.UNARY_PLUS(100));
      assertEqual(1054.342, aql.UNARY_PLUS(1054.342));
      assertEqual(-1054.342, aql.UNARY_PLUS(-1054.342));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.UNARY_MINUS function
    ////////////////////////////////////////////////////////////////////////////////

    testUnaryMinusUndefined: function() {
      assertEqual(0, aql.UNARY_MINUS(undefined));
      assertEqual(0, aql.UNARY_MINUS(null));
      assertEqual(0, aql.UNARY_MINUS(false));
      assertEqual(-1, aql.UNARY_MINUS(true));
      assertEqual(0, aql.UNARY_MINUS(''));
      assertEqual(0, aql.UNARY_MINUS(' '));
      assertEqual(0, aql.UNARY_MINUS('abc'));
      assertEqual(0, aql.UNARY_MINUS('1abc'));
      assertEqual(1, aql.UNARY_MINUS('-1'));
      assertEqual(0, aql.UNARY_MINUS('0'));
      assertEqual(-1, aql.UNARY_MINUS('1'));
      assertEqual(-1.5, aql.UNARY_MINUS('1.5'));
      assertEqual(1.5, aql.UNARY_MINUS('-1.5'));
      assertEqual(0, aql.UNARY_MINUS([ ]));
      assertEqual(0, aql.UNARY_MINUS([ 0 ]));
      assertEqual(-1, aql.UNARY_MINUS([ 1 ]));
      assertEqual(23, aql.UNARY_MINUS([ -23 ]));
      assertEqual(0, aql.UNARY_MINUS([ 1, 2 ]));
      assertEqual(0, aql.UNARY_MINUS({ 'a' : 1 }));
      assertEqual(0, aql.UNARY_MINUS(NaN));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.UNARY_MINUS function
    ////////////////////////////////////////////////////////////////////////////////

    testUnaryMinusValue: function() {
      assertEqual(0, aql.UNARY_MINUS(0));
      assertEqual(1, aql.UNARY_MINUS(-1));
      assertEqual(-1, aql.UNARY_MINUS(1));
      assertEqual(0.0001, aql.UNARY_MINUS(-0.0001));
      assertEqual(-0.0001, aql.UNARY_MINUS(0.0001));
      assertEqual(100, aql.UNARY_MINUS(-100));
      assertEqual(-100, aql.UNARY_MINUS(100));
      assertEqual(1054.342, aql.UNARY_MINUS(-1054.342));
      assertEqual(-1054.342, aql.UNARY_MINUS(1054.342));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.ARITHMETIC_PLUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticPlusUndefined: function() {
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, null));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, false));
      assertEqual(1, aql.ARITHMETIC_PLUS(undefined, true));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, 0));
      assertEqual(1, aql.ARITHMETIC_PLUS(undefined, 1));
      assertEqual(2, aql.ARITHMETIC_PLUS(undefined, 2));
      assertEqual(-1, aql.ARITHMETIC_PLUS(undefined, -1));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, ''));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, '0'));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, ' '));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, 'a'));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, [ ]));
      assertEqual(1, aql.ARITHMETIC_PLUS(undefined, [ 1 ]));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, { }));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, { 'a' : 0 }));
      assertEqual(0, aql.ARITHMETIC_PLUS(undefined, NaN));
      assertEqual(0, aql.ARITHMETIC_PLUS(null, undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS(false, undefined));
      assertEqual(1, aql.ARITHMETIC_PLUS(true, undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS(0, undefined));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, undefined));
      assertEqual(2, aql.ARITHMETIC_PLUS(2, undefined));
      assertEqual(-1, aql.ARITHMETIC_PLUS(-1, undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS('', undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS('0', undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS(' ', undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS('a', undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS([ ], undefined));
      assertEqual(1, aql.ARITHMETIC_PLUS([ 1 ], undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS({ }, undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS({ 'a' : 0 }, undefined));
      assertEqual(0, aql.ARITHMETIC_PLUS(NaN, undefined));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, NaN));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, null));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, false));
      assertEqual(2, aql.ARITHMETIC_PLUS(1, true));
      assertEqual('1', aql.ARITHMETIC_PLUS(1, ''));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, ' '));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, '0'));
      assertEqual(2, aql.ARITHMETIC_PLUS(1, '1'));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, 'a'));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, [ ]));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, [ 0 ]));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, { }));
      assertEqual(1, aql.ARITHMETIC_PLUS(1, { 'a' : 0 }));
      assertEqual(1, aql.ARITHMETIC_PLUS(NaN, 1));
      assertEqual(1, aql.ARITHMETIC_PLUS(null, 1));
      assertEqual(1, aql.ARITHMETIC_PLUS(false, 1));
      assertEqual(2, aql.ARITHMETIC_PLUS(true, 1));
      assertEqual(1, aql.ARITHMETIC_PLUS('', 1));
      assertEqual(1, aql.ARITHMETIC_PLUS(' ', 1));
      assertEqual(1, aql.ARITHMETIC_PLUS('0', 1));
      assertEqual(2, aql.ARITHMETIC_PLUS('1', 1));
      assertEqual(1, aql.ARITHMETIC_PLUS('a', 1));
      assertEqual(1, aql.ARITHMETIC_PLUS([ ], 1));
      assertEqual(1, aql.ARITHMETIC_PLUS([ 0 ], 1));
      assertEqual(4, aql.ARITHMETIC_PLUS([ 3 ], 1));
      assertEqual(1, aql.ARITHMETIC_PLUS({ }, 1));
      assertEqual(1, aql.ARITHMETIC_PLUS({ 'a' : 0 }, 1));
      assertEqual(0, aql.ARITHMETIC_PLUS('0', '0'));
      assertEqual(8, aql.ARITHMETIC_PLUS('4', '4'));
      assertEqual(0, aql.ARITHMETIC_PLUS('4', '-4'));
      assertEqual(0, aql.ARITHMETIC_PLUS(1.3e308 * 10, 1.3e308 * 10));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.ARITHMETIC_PLUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticPlusValue: function() {
      assertEqual(0, aql.ARITHMETIC_PLUS(0, 0));
      assertEqual(0, aql.ARITHMETIC_PLUS(1, -1));
      assertEqual(0, aql.ARITHMETIC_PLUS(-1, 1));
      assertEqual(0, aql.ARITHMETIC_PLUS(-100, 100));
      assertEqual(0, aql.ARITHMETIC_PLUS(100, -100));
      assertEqual(0, aql.ARITHMETIC_PLUS(0.11, -0.11));
      assertEqual(10, aql.ARITHMETIC_PLUS(5, 5));
      assertEqual(10, aql.ARITHMETIC_PLUS(4, 6));
      assertEqual(10, aql.ARITHMETIC_PLUS(1, 9));
      assertEqual(10, aql.ARITHMETIC_PLUS(0.1, 9.9));
      assertEqual(9.8, aql.ARITHMETIC_PLUS(-0.1, 9.9));
      assertEqual(-34.2, aql.ARITHMETIC_PLUS(-17.1, -17.1));
      assertEqual(-2, aql.ARITHMETIC_PLUS(-1, -1)); 
      assertEqual(2.6e307, aql.ARITHMETIC_PLUS(1.3e307, 1.3e307));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.ARITHMETIC_MINUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticMinusUndefined: function() {
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, null));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, false));
      assertEqual(-1, aql.ARITHMETIC_MINUS(undefined, true));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, 0));
      assertEqual(-1, aql.ARITHMETIC_MINUS(undefined, 1));
      assertEqual(-2, aql.ARITHMETIC_MINUS(undefined, 2));
      assertEqual(1, aql.ARITHMETIC_MINUS(undefined, -1));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, ''));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, '0'));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, ' '));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, 'a'));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, [ ]));
      assertEqual(-1, aql.ARITHMETIC_MINUS(undefined, [ 1 ]));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, { }));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, { 'a' : 0 }));
      assertEqual(0, aql.ARITHMETIC_MINUS(undefined, NaN));
      assertEqual(0, aql.ARITHMETIC_MINUS(null, undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS(false, undefined));
      assertEqual(1, aql.ARITHMETIC_MINUS(true, undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS(0, undefined));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, undefined));
      assertEqual(2, aql.ARITHMETIC_MINUS(2, undefined));
      assertEqual(-1, aql.ARITHMETIC_MINUS(-1, undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS('', undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS('0', undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS(' ', undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS('a', undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS([ ], undefined));
      assertEqual(1, aql.ARITHMETIC_MINUS([ 1 ], undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS({ }, undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS({ 'a' : 0 }, undefined));
      assertEqual(0, aql.ARITHMETIC_MINUS(NaN, undefined));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, NaN));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, null));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, false));
      assertEqual(0, aql.ARITHMETIC_MINUS(1, true));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, ''));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, ' '));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, '0'));
      assertEqual(0, aql.ARITHMETIC_MINUS(1, '1'));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, 'a'));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, [ ]));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, [ 0 ]));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, { }));
      assertEqual(1, aql.ARITHMETIC_MINUS(1, { 'a' : 0 }));
      assertEqual(-1, aql.ARITHMETIC_MINUS(NaN, 1));
      assertEqual(-1, aql.ARITHMETIC_MINUS(null, 1));
      assertEqual(-1, aql.ARITHMETIC_MINUS(false, 1));
      assertEqual(0, aql.ARITHMETIC_MINUS(true, 1));
      assertEqual(-1, aql.ARITHMETIC_MINUS('', 1));
      assertEqual(-1, aql.ARITHMETIC_MINUS(' ', 1));
      assertEqual(-1, aql.ARITHMETIC_MINUS('0', 1));
      assertEqual(0, aql.ARITHMETIC_MINUS('1', 1));
      assertEqual(-1, aql.ARITHMETIC_MINUS('a', 1));
      assertEqual(-1, aql.ARITHMETIC_MINUS([ ], 1));
      assertEqual(-1, aql.ARITHMETIC_MINUS([ 0 ], 1));
      assertEqual(-1, aql.ARITHMETIC_MINUS({ }, 1));
      assertEqual(-1, aql.ARITHMETIC_MINUS({ 'a' : 0 }, 1));
      assertEqual(0, aql.ARITHMETIC_MINUS('0', '0'));
      assertEqual(0, aql.ARITHMETIC_MINUS(-1.3e308 * 10, 1.3e308 * 10));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.ARITHMETIC_MINUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticMinusValue: function() {
      assertEqual(0, aql.ARITHMETIC_MINUS(0, 0));
      assertEqual(-1, aql.ARITHMETIC_MINUS(0, 1));
      assertEqual(0, aql.ARITHMETIC_MINUS(-1, -1)); 
      assertEqual(0, aql.ARITHMETIC_MINUS(1, 1));
      assertEqual(2, aql.ARITHMETIC_MINUS(1, -1));
      assertEqual(-2, aql.ARITHMETIC_MINUS(-1, 1));
      assertEqual(-200, aql.ARITHMETIC_MINUS(-100, 100));
      assertEqual(200, aql.ARITHMETIC_MINUS(100, -100));
      assertEqual(0.22, aql.ARITHMETIC_MINUS(0.11, -0.11));
      assertEqual(0, aql.ARITHMETIC_MINUS(5, 5));
      assertEqual(-2, aql.ARITHMETIC_MINUS(4, 6));
      assertEqual(-8, aql.ARITHMETIC_MINUS(1, 9));
      assertEqual(-9.8, aql.ARITHMETIC_MINUS(0.1, 9.9));
      assertEqual(-10, aql.ARITHMETIC_MINUS(-0.1, 9.9));
      assertEqual(0, aql.ARITHMETIC_MINUS(-17.1, -17.1));
      assertEqual(0, aql.ARITHMETIC_MINUS(1.3e307, 1.3e307));
      assertEqual(2.6e307, aql.ARITHMETIC_MINUS(1.3e307, -1.3e307));
      assertEqual(-2.6e307, aql.ARITHMETIC_MINUS(-1.3e307, 1.3e307));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.ARITHMETIC_TIMES function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticTimesUndefined: function() {
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, null));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, false));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, true));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, 0));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, 1));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, 2));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, -1));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, ''));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, '0'));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, ' '));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, 'a'));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, [ ]));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, [ 1 ]));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, { }));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, { 'a' : 0 }));
      assertEqual(0, aql.ARITHMETIC_TIMES(undefined, NaN));
      assertEqual(0, aql.ARITHMETIC_TIMES(null, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES(false, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES(true, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES(0, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES(2, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES(-1, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES('', undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES('0', undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES(' ', undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES('a', undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES([ ], undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES([ 1 ], undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES({ }, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES({ 'a' : 0 }, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES(NaN, undefined));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, NaN));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, null));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, false));
      assertEqual(1, aql.ARITHMETIC_TIMES(1, true));
      assertEqual(2, aql.ARITHMETIC_TIMES(2, true));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, ''));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, ' '));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, '0'));
      assertEqual(1, aql.ARITHMETIC_TIMES(1, '1'));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, 'a'));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, [ ]));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, [ 0 ]));
      assertEqual(2, aql.ARITHMETIC_TIMES(1, [ 2 ]));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, { }));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, { 'a' : 0 }));
      assertEqual(0, aql.ARITHMETIC_TIMES(NaN, 1));
      assertEqual(0, aql.ARITHMETIC_TIMES(null, 1));
      assertEqual(0, aql.ARITHMETIC_TIMES(false, 1));
      assertEqual(1, aql.ARITHMETIC_TIMES(true, 1));
      assertEqual(2, aql.ARITHMETIC_TIMES(true, 2));
      assertEqual(0, aql.ARITHMETIC_TIMES('', 1));
      assertEqual(0, aql.ARITHMETIC_TIMES(' ', 1));
      assertEqual(0, aql.ARITHMETIC_TIMES('0', 1));
      assertEqual(1, aql.ARITHMETIC_TIMES('1', 1));
      assertEqual(0, aql.ARITHMETIC_TIMES('a', 1));
      assertEqual(0, aql.ARITHMETIC_TIMES([ ], 1));
      assertEqual(0, aql.ARITHMETIC_TIMES([ 0 ], 1));
      assertEqual(0, aql.ARITHMETIC_TIMES({ }, 1));
      assertEqual(0, aql.ARITHMETIC_TIMES({ 'a' : 0 }, 1));
      assertEqual(0, aql.ARITHMETIC_TIMES(1.3e190, 1.3e190));
      assertEqual(0, aql.ARITHMETIC_TIMES(1.3e307, 1.3e307));
      assertEqual(0, aql.ARITHMETIC_TIMES(1.3e308 * 10, 1.3e308 * 10));
      assertEqual(0, aql.ARITHMETIC_TIMES('0', '0'));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.ARITHMETIC_TIMES function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticTimesValue: function() {
      assertEqual(0, aql.ARITHMETIC_TIMES(0, 0));
      assertEqual(0, aql.ARITHMETIC_TIMES(1, 0));
      assertEqual(0, aql.ARITHMETIC_TIMES(0, 1));
      assertEqual(1, aql.ARITHMETIC_TIMES(1, 1));
      assertEqual(2, aql.ARITHMETIC_TIMES(2, 1));
      assertEqual(2, aql.ARITHMETIC_TIMES(1, 2));
      assertEqual(4, aql.ARITHMETIC_TIMES(2, 2));
      assertEqual(100, aql.ARITHMETIC_TIMES(10, 10));
      assertEqual(1000, aql.ARITHMETIC_TIMES(10, 100));
      assertEqual(1.44, aql.ARITHMETIC_TIMES(1.2, 1.2));
      assertEqual(-1.44, aql.ARITHMETIC_TIMES(1.2, -1.2));
      assertEqual(1.44, aql.ARITHMETIC_TIMES(-1.2, -1.2));
      assertEqual(2, aql.ARITHMETIC_TIMES(0.25, 8));
      assertEqual(2, aql.ARITHMETIC_TIMES(8, 0.25));
      assertEqual(2.25, aql.ARITHMETIC_TIMES(9, 0.25));
      assertEqual(-2.25, aql.ARITHMETIC_TIMES(9, -0.25));
      assertEqual(-2.25, aql.ARITHMETIC_TIMES(-9, 0.25));
      assertEqual(2.25, aql.ARITHMETIC_TIMES(-9, -0.25));
      assertEqual(4, aql.ARITHMETIC_TIMES(-2, -2));
      assertEqual(-4, aql.ARITHMETIC_TIMES(-2, 2));
      assertEqual(-4, aql.ARITHMETIC_TIMES(2, -2));
      assertEqual(1000000, aql.ARITHMETIC_TIMES(1000, 1000));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.ARITHMETIC_DIVIDE function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticDivideUndefined: function() {
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, null));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, false));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(undefined, true));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, 0));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(undefined, 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(undefined, 2));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(undefined, -1));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, ''));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, '0'));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, ' '));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, 'a'));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, [ ]));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(undefined, [ 1 ]));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, { }));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, { 'a' : 0 }));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(undefined, NaN));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(null, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(false, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(true, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(0, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(2, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(-1, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE('', undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE('0', undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(' ', undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE('a', undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE([ ], undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE([ 1 ], undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE({ }, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE({ 'a' : 0 }, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(NaN, undefined));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, NaN));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, null));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, false));
      assertEqual(1, aql.ARITHMETIC_DIVIDE(1, true));
      assertEqual(2, aql.ARITHMETIC_DIVIDE(2, true));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, ''));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, ' '));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, '0'));
      assertEqual(1, aql.ARITHMETIC_DIVIDE(1, '1'));
      assertEqual(2, aql.ARITHMETIC_DIVIDE(2, '1'));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, 'a'));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, [ ]));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, [ 0 ]));
      assertEqual(0.5, aql.ARITHMETIC_DIVIDE(1, [ 2 ]));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, { }));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, { 'a' : 0 }));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(NaN, 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(null, 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(false, 1));
      assertEqual(1, aql.ARITHMETIC_DIVIDE(true, 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE('', 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(' ', 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE('0', 1));
      assertEqual(1, aql.ARITHMETIC_DIVIDE('1', 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE('a', 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE([ ], 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE([ 0 ], 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE({ }, 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE({ 'a' : 0 }, 1));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(1, 0));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(100, 0));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(-1, 0));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(-100, 0));
      assertEqual(null, aql.ARITHMETIC_DIVIDE(0, 0));
      assertEqual(null, aql.ARITHMETIC_DIVIDE('0', '0'));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.ARITHMETIC_DIVIDE function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticDivideValue: function() {
      assertEqual(0, aql.ARITHMETIC_DIVIDE(0, 1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(0, 2));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(0, 10));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(0, -1));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(0, -2));
      assertEqual(0, aql.ARITHMETIC_DIVIDE(0, -10));
      assertEqual(1, aql.ARITHMETIC_DIVIDE(1, 1));
      assertEqual(2, aql.ARITHMETIC_DIVIDE(2, 1));
      assertEqual(-1, aql.ARITHMETIC_DIVIDE(-1, 1));
      assertEqual(100, aql.ARITHMETIC_DIVIDE(100, 1));
      assertEqual(-100, aql.ARITHMETIC_DIVIDE(-100, 1));
      assertEqual(-1, aql.ARITHMETIC_DIVIDE(1, -1));
      assertEqual(-2, aql.ARITHMETIC_DIVIDE(2, -1));
      assertEqual(1, aql.ARITHMETIC_DIVIDE(-1, -1));
      assertEqual(-100, aql.ARITHMETIC_DIVIDE(100, -1));
      assertEqual(100, aql.ARITHMETIC_DIVIDE(-100, -1));
      assertEqual(0.5, aql.ARITHMETIC_DIVIDE(1, 2));
      assertEqual(1, aql.ARITHMETIC_DIVIDE(2, 2));
      assertEqual(2, aql.ARITHMETIC_DIVIDE(4, 2));
      assertEqual(1, aql.ARITHMETIC_DIVIDE(4, 4));
      assertEqual(5, aql.ARITHMETIC_DIVIDE(10, 2));
      assertEqual(2, aql.ARITHMETIC_DIVIDE(10, 5));
      assertEqual(2.5, aql.ARITHMETIC_DIVIDE(10, 4));
      assertEqual(1, aql.ARITHMETIC_DIVIDE(1.2, 1.2));
      assertEqual(-1, aql.ARITHMETIC_DIVIDE(1.2, -1.2));
      assertEqual(1, aql.ARITHMETIC_DIVIDE(-1.2, -1.2));
      assertEqual(2, aql.ARITHMETIC_DIVIDE(1, 0.5));
      assertEqual(4, aql.ARITHMETIC_DIVIDE(1, 0.25));
      assertEqual(16, aql.ARITHMETIC_DIVIDE(4, 0.25));
      assertEqual(-16, aql.ARITHMETIC_DIVIDE(4, -0.25));
      assertEqual(-16, aql.ARITHMETIC_DIVIDE(-4, 0.25));
      assertEqual(100, aql.ARITHMETIC_DIVIDE(25, 0.25));
      assertEqual(-100, aql.ARITHMETIC_DIVIDE(25, -0.25));
      assertEqual(-100, aql.ARITHMETIC_DIVIDE(-25, 0.25));
      assertEqual(1, aql.ARITHMETIC_DIVIDE(0.22, 0.22));
      assertEqual(0.5, aql.ARITHMETIC_DIVIDE(0.22, 0.44));
      assertEqual(2, aql.ARITHMETIC_DIVIDE(0.22, 0.11));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.ARITHMETIC_MODULUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticModulusUndefined: function() {
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, null));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, false));
      assertEqual(0, aql.ARITHMETIC_MODULUS(undefined, true));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, 0));
      assertEqual(0, aql.ARITHMETIC_MODULUS(undefined, 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS(undefined, 2));
      assertEqual(0, aql.ARITHMETIC_MODULUS(undefined, -1));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, ''));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, '0'));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, ' '));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, 'a'));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, [ ]));
      assertEqual(0, aql.ARITHMETIC_MODULUS(undefined, [ 1 ]));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, { }));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, { 'a' : 0 }));
      assertEqual(null, aql.ARITHMETIC_MODULUS(undefined, NaN));
      assertEqual(null, aql.ARITHMETIC_MODULUS(null, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS(false, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS(true, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS(0, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS(2, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS(-1, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS('', undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS('0', undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS(' ', undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS('a', undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS([ ], undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS([ 1 ], undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS({ }, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS({ 'a' : 0 }, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS(NaN, undefined));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, NaN));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, null));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, false));
      assertEqual(0, aql.ARITHMETIC_MODULUS(1, true));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, ''));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, ' '));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, '0'));
      assertEqual(0, aql.ARITHMETIC_MODULUS(1, '1'));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, 'a'));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, [ ]));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, [ 0 ]));
      assertEqual(0, aql.ARITHMETIC_MODULUS(1, [ 1 ]));
      assertEqual(1, aql.ARITHMETIC_MODULUS(4, [ 3 ]));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, { }));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, { 'a' : 0 }));
      assertEqual(0, aql.ARITHMETIC_MODULUS(NaN, 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS(null, 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS(false, 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS(true, 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS('', 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS(' ', 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS('0', 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS('1', 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS('a', 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS([ ], 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS([ 0 ], 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS({ }, 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS({ 'a' : 0 }, 1));
      assertEqual(null, aql.ARITHMETIC_MODULUS(1, 0));
      assertEqual(null, aql.ARITHMETIC_MODULUS(100, 0));
      assertEqual(null, aql.ARITHMETIC_MODULUS(-1, 0));
      assertEqual(null, aql.ARITHMETIC_MODULUS(-100, 0));
      assertEqual(null, aql.ARITHMETIC_MODULUS(0, 0));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.ARITHMETIC_MODULUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticModulusValue: function() {
      assertEqual(0, aql.ARITHMETIC_MODULUS(0, 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS(1, 1));
      assertEqual(1, aql.ARITHMETIC_MODULUS(1, 2));
      assertEqual(1, aql.ARITHMETIC_MODULUS(1, 3));
      assertEqual(1, aql.ARITHMETIC_MODULUS(1, 4));
      assertEqual(0, aql.ARITHMETIC_MODULUS(2, 2));
      assertEqual(2, aql.ARITHMETIC_MODULUS(2, 3));
      assertEqual(0, aql.ARITHMETIC_MODULUS(3, 3));
      assertEqual(0, aql.ARITHMETIC_MODULUS(10, 1));
      assertEqual(0, aql.ARITHMETIC_MODULUS(10, 2));
      assertEqual(1, aql.ARITHMETIC_MODULUS(10, 3));
      assertEqual(2, aql.ARITHMETIC_MODULUS(10, 4));
      assertEqual(0, aql.ARITHMETIC_MODULUS(10, 5));
      assertEqual(4, aql.ARITHMETIC_MODULUS(10, 6));
      assertEqual(3, aql.ARITHMETIC_MODULUS(10, 7));
      assertEqual(2, aql.ARITHMETIC_MODULUS(10, 8));
      assertEqual(1, aql.ARITHMETIC_MODULUS(10, 9));
      assertEqual(0, aql.ARITHMETIC_MODULUS(10, 10));
      assertEqual(10, aql.ARITHMETIC_MODULUS(10, 11));
      assertEqual(1, aql.ARITHMETIC_MODULUS(4, 3));
      assertEqual(2, aql.ARITHMETIC_MODULUS(5, 3));
      assertEqual(0, aql.ARITHMETIC_MODULUS(6, 3));
      assertEqual(1, aql.ARITHMETIC_MODULUS(7, 3));
      assertEqual(2, aql.ARITHMETIC_MODULUS(8, 3));
      assertEqual(0, aql.ARITHMETIC_MODULUS(9, 3));
      assertEqual(1, aql.ARITHMETIC_MODULUS(10, 3));
      assertEqual(0, aql.ARITHMETIC_MODULUS(10, -1));
      assertEqual(0, aql.ARITHMETIC_MODULUS(10, -2));
      assertEqual(1, aql.ARITHMETIC_MODULUS(10, -3));
      assertEqual(2, aql.ARITHMETIC_MODULUS(10, -4));
      assertEqual(0, aql.ARITHMETIC_MODULUS(10, -5));
      assertEqual(4, aql.ARITHMETIC_MODULUS(10, -6));
      assertEqual(3, aql.ARITHMETIC_MODULUS(10, -7));
      assertEqual(2, aql.ARITHMETIC_MODULUS(10, -8));
      assertEqual(1, aql.ARITHMETIC_MODULUS(10, -9));
      assertEqual(0, aql.ARITHMETIC_MODULUS(10, -10));
      assertEqual(10, aql.ARITHMETIC_MODULUS(10, -11));
      assertEqual(-1, aql.ARITHMETIC_MODULUS(-1, 3));
      assertEqual(-2, aql.ARITHMETIC_MODULUS(-2, 3));
      assertEqual(0, aql.ARITHMETIC_MODULUS(-3, 3));
      assertEqual(-1, aql.ARITHMETIC_MODULUS(-4, 3));
      assertEqual(-2, aql.ARITHMETIC_MODULUS(-5, 3));
      assertEqual(0, aql.ARITHMETIC_MODULUS(-6, 3));
      assertEqual(-1, aql.ARITHMETIC_MODULUS(-7, 3));
      assertEqual(-2, aql.ARITHMETIC_MODULUS(-8, 3));
      assertEqual(0, aql.ARITHMETIC_MODULUS(-9, 3));
      assertEqual(-1, aql.ARITHMETIC_MODULUS(-10, 3));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.CONCAT function
    ////////////////////////////////////////////////////////////////////////////////

    testStringConcatUndefined: function() {
      assertEqual("false", aql.AQL_CONCAT(undefined, false));
      assertEqual("true", aql.AQL_CONCAT(undefined, true));
      assertEqual("0", aql.AQL_CONCAT(undefined, 0));
      assertEqual("1", aql.AQL_CONCAT(undefined, 1));
      assertEqual("2", aql.AQL_CONCAT(undefined, 2));
      assertEqual("-1", aql.AQL_CONCAT(undefined, -1));
      assertEqual("[]", aql.AQL_CONCAT(undefined, [ ]));
      assertEqual("[1]", aql.AQL_CONCAT(undefined, [ 1 ]));
      assertEqual("[1,2]", aql.AQL_CONCAT(undefined, [ 1, 2 ]));
      assertEqual("{}", aql.AQL_CONCAT(undefined, { }));
      assertEqual("{\"a\":0}", aql.AQL_CONCAT(undefined, { 'a' : 0 }));
      assertEqual("false", aql.AQL_CONCAT(false, undefined));
      assertEqual("true", aql.AQL_CONCAT(true, undefined));
      assertEqual("0", aql.AQL_CONCAT(0, undefined));
      assertEqual("1", aql.AQL_CONCAT(1, undefined));
      assertEqual("2", aql.AQL_CONCAT(2, undefined));
      assertEqual("-1", aql.AQL_CONCAT(-1, undefined));
      assertEqual("[]", aql.AQL_CONCAT([ ], undefined));
      assertEqual("[1]", aql.AQL_CONCAT([ 1 ], undefined));
      assertEqual("[1,2]", aql.AQL_CONCAT([ 1, 2 ], undefined));
      assertEqual("{}", aql.AQL_CONCAT({ }, undefined));
      assertEqual("{\"a\":0}", aql.AQL_CONCAT({ 'a' : 0 }, undefined));
      assertEqual("1", aql.AQL_CONCAT(1, NaN));
      assertEqual("1", aql.AQL_CONCAT(1, null));
      assertEqual("1false", aql.AQL_CONCAT(1, false));
      assertEqual("1true", aql.AQL_CONCAT(1, true));
      assertEqual("1", aql.AQL_CONCAT(1, ''));
      assertEqual("1 ", aql.AQL_CONCAT(1, ' '));
      assertEqual("10", aql.AQL_CONCAT(1, '0'));
      assertEqual("11", aql.AQL_CONCAT(1, '1'));
      assertEqual("1a", aql.AQL_CONCAT(1, 'a'));
      assertEqual("1[]", aql.AQL_CONCAT(1, [ ]));
      assertEqual("1[0]", aql.AQL_CONCAT(1, [ 0 ]));
      assertEqual("1{}", aql.AQL_CONCAT(1, { }));
      assertEqual("1{\"a\":0}", aql.AQL_CONCAT(1, { 'a' : 0 }));
      assertEqual("1", aql.AQL_CONCAT(NaN, 1));
      assertEqual("1", aql.AQL_CONCAT(null, 1));
      assertEqual("false1", aql.AQL_CONCAT(false, 1));
      assertEqual("true1", aql.AQL_CONCAT(true, 1));
      assertEqual("1", aql.AQL_CONCAT('', 1));
      assertEqual(" 1", aql.AQL_CONCAT(' ', 1));
      assertEqual("01", aql.AQL_CONCAT('0', 1));
      assertEqual("11", aql.AQL_CONCAT('1', 1));
      assertEqual("a1", aql.AQL_CONCAT('a', 1));
      assertEqual("[]1", aql.AQL_CONCAT([ ], 1));
      assertEqual("[0]1", aql.AQL_CONCAT([ 0 ], 1));
      assertEqual("{}1", aql.AQL_CONCAT({ }, 1));
      assertEqual("{\"a\":0}1", aql.AQL_CONCAT({ 'a' : 0 }, 1));
      assertEqual("10", aql.AQL_CONCAT(1, 0));
      assertEqual("1000", aql.AQL_CONCAT(100, 0));
      assertEqual("-10", aql.AQL_CONCAT(-1, 0));
      assertEqual("-1000", aql.AQL_CONCAT(-100, 0));
      assertEqual("00", aql.AQL_CONCAT(0, 0));
      assertEqual("false", aql.AQL_CONCAT('', false));
      assertEqual("true", aql.AQL_CONCAT('', true));
      assertEqual("[]", aql.AQL_CONCAT('', [ ]));
      assertEqual("{}", aql.AQL_CONCAT('', { }));
      assertEqual("afalse", aql.AQL_CONCAT('a', false));
      assertEqual("atrue", aql.AQL_CONCAT('a', true));
      assertEqual("a[]", aql.AQL_CONCAT('a', [ ]));
      assertEqual("a{}", aql.AQL_CONCAT('a', { }));
      assertEqual("a{\"foo\":\"bar\"}", aql.AQL_CONCAT('a', { foo: "bar" }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.CONCAT function
    ////////////////////////////////////////////////////////////////////////////////

    testStringConcatValue: function() {
      assertEqual('', aql.AQL_CONCAT());
      assertEqual('', aql.AQL_CONCAT(''));
      assertEqual('a', aql.AQL_CONCAT('a'));
      assertEqual('a', aql.AQL_CONCAT('a', null));
      assertEqual('', aql.AQL_CONCAT('', null));
      assertEqual('', aql.AQL_CONCAT(undefined, ''));
      assertEqual('0', aql.AQL_CONCAT(undefined, '0'));
      assertEqual(' ', aql.AQL_CONCAT(undefined, ' '));
      assertEqual('a', aql.AQL_CONCAT(undefined, 'a'));
      assertEqual('', aql.AQL_CONCAT('', undefined));
      assertEqual('0', aql.AQL_CONCAT('0', undefined));
      assertEqual(' ', aql.AQL_CONCAT(' ', undefined));
      assertEqual('a', aql.AQL_CONCAT('a', undefined));
      assertEqual('', aql.AQL_CONCAT(undefined, NaN));
      assertEqual('', aql.AQL_CONCAT(null, undefined));
      assertEqual('', aql.AQL_CONCAT(NaN, undefined));

      assertEqual('', aql.AQL_CONCAT('', ''));
      assertEqual(' ', aql.AQL_CONCAT(' ', ''));
      assertEqual(' ', aql.AQL_CONCAT('', ' '));
      assertEqual('  ', aql.AQL_CONCAT(' ', ' '));
      assertEqual(' a a', aql.AQL_CONCAT(' a', ' a'));
      assertEqual(' a a ', aql.AQL_CONCAT(' a', ' a '));
      assertEqual('a', aql.AQL_CONCAT('a', ''));
      assertEqual('a', aql.AQL_CONCAT('', 'a'));
      assertEqual('a ', aql.AQL_CONCAT('', 'a '));
      assertEqual('a ', aql.AQL_CONCAT('', 'a '));
      assertEqual('a ', aql.AQL_CONCAT('a ', ''));
      assertEqual('ab', aql.AQL_CONCAT('a', 'b'));
      assertEqual('ba', aql.AQL_CONCAT('b', 'a'));
      assertEqual('AA', aql.AQL_CONCAT('A', 'A'));
      assertEqual('AaA', aql.AQL_CONCAT('A', 'aA'));
      assertEqual('AaA', aql.AQL_CONCAT('Aa', 'A'));
      assertEqual('0.00', aql.AQL_CONCAT('0.00', ''));
      assertEqual('abc', aql.AQL_CONCAT('a', aql.AQL_CONCAT('b', 'c')));
      assertEqual('', aql.AQL_CONCAT('', aql.AQL_CONCAT('', '')));
      assertEqual('fux', aql.AQL_CONCAT('f', 'u', 'x'));
      assertEqual('fux', aql.AQL_CONCAT('f', 'u', null, 'x'));
      assertEqual('fux', aql.AQL_CONCAT(null, 'f', null, 'u', null, 'x', null));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.TERNARY_OPERATOR function
    ////////////////////////////////////////////////////////////////////////////////

    testTernaryOperator: function() {
      assertEqual(2, aql.TERNARY_OPERATOR(true, function () { return 2; }, function () { return -1; }));
      assertEqual(-1, aql.TERNARY_OPERATOR(false, function () { return 1; }, function () { return -1; }));
      assertEqual(2, aql.TERNARY_OPERATOR(0, function () { return 1; }, function () { return 2; }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ranges
    ////////////////////////////////////////////////////////////////////////////////

    testRanges: function() {
      assertEqual([ [ 1, 2, 3 ] ], AQL_EXECUTE("RETURN 1..3").json);
      assertEqual([ [ 0, 1, 2, 3 ] ], AQL_EXECUTE("RETURN null..3").json);
      assertEqual([ [ 1, 2, 3, 4, 5, 6, 7 ] ], AQL_EXECUTE("RETURN 1..3 + 4").json);
      assertEqual([ [ -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 ] ], AQL_EXECUTE("RETURN -1 - 3..3 + 2").json);
      assertEqual([ [ 0, 1, 2, 3 ] ], AQL_EXECUTE("RETURN 1..2..3").json);
      assertEqual([ [ 1, 2, 3, 4 ] ], AQL_EXECUTE("RETURN 1..1..4").json);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlOperatorsTestSuite);

return jsunity.done();

