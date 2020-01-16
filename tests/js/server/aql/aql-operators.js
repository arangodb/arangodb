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
      
var LOGICAL_AND = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " && " + JSON.stringify(b)).toArray()[0];
};
var LOGICAL_OR = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " || " + JSON.stringify(b)).toArray()[0];
};
var LOGICAL_NOT = function (a) {
  return db._query("RETURN !" + JSON.stringify(a)).toArray()[0];
};
var UNARY_PLUS = function (a) {
  return db._query("RETURN +" + JSON.stringify(a)).toArray()[0];
};
var UNARY_MINUS = function (a) {
  return db._query("RETURN -" + JSON.stringify(a)).toArray()[0];
};
var ARITHMETIC_PLUS = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " + " + JSON.stringify(b)).toArray()[0];
};
var ARITHMETIC_MINUS = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " - " + JSON.stringify(b)).toArray()[0];
};
var ARITHMETIC_TIMES = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " * " + JSON.stringify(b)).toArray()[0];
};
var ARITHMETIC_DIVIDE = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " / " + JSON.stringify(b)).toArray()[0];
};
var ARITHMETIC_MODULUS = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " % " + JSON.stringify(b)).toArray()[0];
};
var RELATIONAL_EQUAL = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " == " + JSON.stringify(b)).toArray()[0];
};
var RELATIONAL_UNEQUAL = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " != " + JSON.stringify(b)).toArray()[0];
};
var RELATIONAL_LESS = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " < " + JSON.stringify(b)).toArray()[0];
};
var RELATIONAL_LESSEQUAL = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " <= " + JSON.stringify(b)).toArray()[0];
};
var RELATIONAL_GREATER = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " > " + JSON.stringify(b)).toArray()[0];
};
var RELATIONAL_GREATEREQUAL = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " >= " + JSON.stringify(b)).toArray()[0];
};
var RELATIONAL_IN = function (a, b) {
  return db._query("RETURN " + JSON.stringify(a) + " IN " + JSON.stringify(b)).toArray()[0];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlOperatorsTestSuite () {
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.IS_NULL function
    ////////////////////////////////////////////////////////////////////////////////

    testIsNullTrue: function() {
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
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_AND function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalAndUndefined: function() {
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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_AND function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalAndBool: function() {
      assertTrue(LOGICAL_AND(true, true));
      assertFalse(LOGICAL_AND(true, false));
      assertFalse(LOGICAL_AND(false, true));
      assertFalse(LOGICAL_AND(false, false));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_OR function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalOrUndefined: function() {
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
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_OR function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalOrBool: function() {
      assertTrue(LOGICAL_OR(true, true));
      assertTrue(LOGICAL_OR(true, false));
      assertTrue(LOGICAL_OR(false, true));
      assertFalse(LOGICAL_OR(false, false));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_NOT function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalNotUndefined: function() {
      assertEqual(true, LOGICAL_NOT(null));
      assertEqual(true, LOGICAL_NOT(false));
      assertEqual(false, LOGICAL_NOT(true));
      assertEqual(true, LOGICAL_NOT(0.0));
      assertEqual(false, LOGICAL_NOT(1.0));
      assertEqual(false, LOGICAL_NOT(-1.0));
      assertEqual(true, LOGICAL_NOT(''));
      assertEqual(false, LOGICAL_NOT('0'));
      assertEqual(false, LOGICAL_NOT('1'));
      assertEqual(false, LOGICAL_NOT([ ]));
      assertEqual(false, LOGICAL_NOT([ 0 ]));
      assertEqual(false, LOGICAL_NOT([ 0, 1 ]));
      assertEqual(false, LOGICAL_NOT([ 1, 2 ]));
      assertEqual(false, LOGICAL_NOT({ }));
      assertEqual(false, LOGICAL_NOT({ 'a' : 0 }));
      assertEqual(false, LOGICAL_NOT({ 'a' : 1 }));
      assertEqual(false, LOGICAL_NOT({ '0' : false}));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.LOGICAL_NOT function
    ////////////////////////////////////////////////////////////////////////////////

    testLogicalNotBool: function() {
      assertTrue(LOGICAL_NOT(false));
      assertFalse(LOGICAL_NOT(true));

      assertTrue(LOGICAL_NOT(LOGICAL_NOT(true)));
      assertFalse(LOGICAL_NOT(LOGICAL_NOT(false)));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.RELATIONAL_EQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalEqualTrue: function() {
      assertTrue(RELATIONAL_EQUAL(1, 1));
      assertTrue(RELATIONAL_EQUAL(0, 0));
      assertTrue(RELATIONAL_EQUAL(-1, -1));
      assertTrue(RELATIONAL_EQUAL(1.345, 1.345));
      assertTrue(RELATIONAL_EQUAL(1.0, 1.00));
      assertTrue(RELATIONAL_EQUAL(1.0, 1.000));
      assertTrue(RELATIONAL_EQUAL(1.1, 1.1));
      assertTrue(RELATIONAL_EQUAL(1.01, 1.01));
      assertTrue(RELATIONAL_EQUAL(1.001, 1.001));
      assertTrue(RELATIONAL_EQUAL(1.0001, 1.0001));
      assertTrue(RELATIONAL_EQUAL(1.00001, 1.00001));
      assertTrue(RELATIONAL_EQUAL(1.000001, 1.000001));
      assertTrue(RELATIONAL_EQUAL(1.245e307, 1.245e307));
      assertTrue(RELATIONAL_EQUAL(-99.43423, -99.43423));
      assertTrue(RELATIONAL_EQUAL(true, true));
      assertTrue(RELATIONAL_EQUAL(false, false));
      assertTrue(RELATIONAL_EQUAL('', ''));
      assertTrue(RELATIONAL_EQUAL(' ', ' '));
      assertTrue(RELATIONAL_EQUAL(' 1', ' 1'));
      assertTrue(RELATIONAL_EQUAL('0', '0'));
      assertTrue(RELATIONAL_EQUAL('abc', 'abc'));
      assertTrue(RELATIONAL_EQUAL('-1', '-1'));
      assertTrue(RELATIONAL_EQUAL('true', 'true'));
      assertTrue(RELATIONAL_EQUAL('false', 'false'));
      assertTrue(RELATIONAL_EQUAL('undefined', 'undefined'));
      assertTrue(RELATIONAL_EQUAL('null', 'null'));
      assertTrue(RELATIONAL_EQUAL([ ], [ ]));
      assertTrue(RELATIONAL_EQUAL([ null ], [ ]));
      assertTrue(RELATIONAL_EQUAL([ ], [ null ]));
      assertTrue(RELATIONAL_EQUAL([ 0 ], [ 0 ]));
      assertTrue(RELATIONAL_EQUAL([ 0, 1 ], [ 0, 1 ]));
      assertTrue(RELATIONAL_EQUAL([ 0, 1, 4 ], [ 0, 1, 4 ]));
      assertTrue(RELATIONAL_EQUAL([ 3, 4, -99 ], [ 3, 4, -99 ]));
      assertTrue(RELATIONAL_EQUAL([ 'a', 4, [ 1, 'a' ], false ], [ 'a', 4, [ 1, 'a' ], false ]));
      assertTrue(RELATIONAL_EQUAL({ }, { }));
      assertTrue(RELATIONAL_EQUAL({ 'a' : true }, { 'a' : true }));
      assertTrue(RELATIONAL_EQUAL({ 'a' : true, 'b': true }, { 'a' : true, 'b': true }));
      assertTrue(RELATIONAL_EQUAL({ 'a' : true, 'b': true }, { 'b' : true, 'a': true }));
      assertTrue(RELATIONAL_EQUAL({ 'b' : true, 'a': true }, { 'b' : true, 'a': true }));
      assertTrue(RELATIONAL_EQUAL({ 'b' : true, 'a': true }, { 'a' : true, 'b': true }));
      assertTrue(RELATIONAL_EQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }));
      assertTrue(RELATIONAL_EQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'b' : [ 1, 9 ], 'a' : [ 0, 1 ] }));
      assertTrue(RELATIONAL_EQUAL({ 'f' : { 'c' : { 'd' : [ 0, 1 ], 'a' : [ 1, 9 ] }, 'a' : false }, 'a' : true }, { 'a' : true, 'f' : { 'a' : false, 'c' : { 'a' : [ 1, 9 ], 'd' : [ 0, 1 ] } } }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_EQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalEqualFalse: function() {
      assertFalse(RELATIONAL_EQUAL(1, 0));
      assertFalse(RELATIONAL_EQUAL(0, 1));
      assertFalse(RELATIONAL_EQUAL(0, false));
      assertFalse(RELATIONAL_EQUAL(false, 0));
      assertFalse(RELATIONAL_EQUAL(false, 0));
      assertFalse(RELATIONAL_EQUAL(-1, 1));
      assertFalse(RELATIONAL_EQUAL(1, -1));
      assertFalse(RELATIONAL_EQUAL(-1, 0));
      assertFalse(RELATIONAL_EQUAL(0, -1));
      assertFalse(RELATIONAL_EQUAL(true, false));
      assertFalse(RELATIONAL_EQUAL(false, true));
      assertFalse(RELATIONAL_EQUAL(true, 1));
      assertFalse(RELATIONAL_EQUAL(1, true));
      assertFalse(RELATIONAL_EQUAL(0, true));
      assertFalse(RELATIONAL_EQUAL(true, 0));
      assertFalse(RELATIONAL_EQUAL(true, 'true'));
      assertFalse(RELATIONAL_EQUAL(false, 'false'));
      assertFalse(RELATIONAL_EQUAL('true', true));
      assertFalse(RELATIONAL_EQUAL('false', false));
      assertFalse(RELATIONAL_EQUAL(-1.345, 1.345));
      assertFalse(RELATIONAL_EQUAL(1.345, -1.345));
      assertFalse(RELATIONAL_EQUAL(1.345, 1.346));
      assertFalse(RELATIONAL_EQUAL(1.346, 1.345));
      assertFalse(RELATIONAL_EQUAL(1.344, 1.345));
      assertFalse(RELATIONAL_EQUAL(1.345, 1.344));
      assertFalse(RELATIONAL_EQUAL(1, 2));
      assertFalse(RELATIONAL_EQUAL(2, 1));
      assertFalse(RELATIONAL_EQUAL(1.246e307, 1.245e307));
      assertFalse(RELATIONAL_EQUAL(1.246e307, 1.247e307));
      assertFalse(RELATIONAL_EQUAL(1.246e307, 1.2467e307));
      assertFalse(RELATIONAL_EQUAL(-99.43423, -99.434233));
      assertFalse(RELATIONAL_EQUAL(1.00001, 1.000001));
      assertFalse(RELATIONAL_EQUAL(1.00001, 1.0001));
      assertFalse(RELATIONAL_EQUAL(null, 1));
      assertFalse(RELATIONAL_EQUAL(1, null));
      assertFalse(RELATIONAL_EQUAL(null, 0));
      assertFalse(RELATIONAL_EQUAL(0, null));
      assertFalse(RELATIONAL_EQUAL(null, ''));
      assertFalse(RELATIONAL_EQUAL('', null));
      assertFalse(RELATIONAL_EQUAL(null, '0'));
      assertFalse(RELATIONAL_EQUAL('0', null));
      assertFalse(RELATIONAL_EQUAL(null, false));
      assertFalse(RELATIONAL_EQUAL(false, null));
      assertFalse(RELATIONAL_EQUAL(null, true));
      assertFalse(RELATIONAL_EQUAL(true, null));
      assertFalse(RELATIONAL_EQUAL(null, 'null'));
      assertFalse(RELATIONAL_EQUAL('null', null));
      assertFalse(RELATIONAL_EQUAL(0, ''));
      assertFalse(RELATIONAL_EQUAL('', 0));
      assertFalse(RELATIONAL_EQUAL(1, ''));
      assertFalse(RELATIONAL_EQUAL('', 1));
      assertFalse(RELATIONAL_EQUAL(' ', ''));
      assertFalse(RELATIONAL_EQUAL('', ' '));
      assertFalse(RELATIONAL_EQUAL(' 1', '1'));
      assertFalse(RELATIONAL_EQUAL('1', ' 1'));
      assertFalse(RELATIONAL_EQUAL('1 ', '1'));
      assertFalse(RELATIONAL_EQUAL('1', '1 '));
      assertFalse(RELATIONAL_EQUAL('1 ', ' 1'));
      assertFalse(RELATIONAL_EQUAL(' 1', '1 '));
      assertFalse(RELATIONAL_EQUAL(' 1 ', '1'));
      assertFalse(RELATIONAL_EQUAL('0', ''));
      assertFalse(RELATIONAL_EQUAL('', ' '));
      assertFalse(RELATIONAL_EQUAL('abc', 'abcd'));
      assertFalse(RELATIONAL_EQUAL('abcd', 'abc'));
      assertFalse(RELATIONAL_EQUAL('dabc', 'abcd'));
      assertFalse(RELATIONAL_EQUAL('1', 1));
      assertFalse(RELATIONAL_EQUAL(1, '1'));
      assertFalse(RELATIONAL_EQUAL('0', 0));
      assertFalse(RELATIONAL_EQUAL('1.0', 1.0));
      assertFalse(RELATIONAL_EQUAL('1.0', 1));
      assertFalse(RELATIONAL_EQUAL('-1', -1));
      assertFalse(RELATIONAL_EQUAL('1.234', 1.234));
      assertFalse(RELATIONAL_EQUAL([ 0 ], [ ]));
      assertFalse(RELATIONAL_EQUAL([ ], [ 0 ]));
      assertFalse(RELATIONAL_EQUAL([ ], [ 0, 1 ]));
      assertFalse(RELATIONAL_EQUAL([ 0 ], [ 0, 1 ]));
      assertFalse(RELATIONAL_EQUAL([ 1, 1, 0 ], [ 1, 0, 1 ]));
      assertFalse(RELATIONAL_EQUAL([ 1, 0, 0 ], [ 1, 0, 1 ]));
      assertFalse(RELATIONAL_EQUAL([ 1, 0, 0 ], [ 1, 0 ]));
      assertFalse(RELATIONAL_EQUAL([ 1, 0 ], [ 1, 0, 0 ]));
      assertFalse(RELATIONAL_EQUAL([ 1, 0 ], [ 0, 1 ]));
      assertFalse(RELATIONAL_EQUAL([ 1, 0 ], [ 0 ]));
      assertFalse(RELATIONAL_EQUAL([ 1, 0 ], [ 1 ]));
      assertFalse(RELATIONAL_EQUAL([ 1, 2, 3 ], [ 3, 2, 1 ]));
      assertFalse(RELATIONAL_EQUAL([ [ 1 ] ], [ [ 0 ] ]));
      assertFalse(RELATIONAL_EQUAL([ 1, [ 1 , 0 ] ], [ 1, [ 0, 1 ] ]));
      assertFalse(RELATIONAL_EQUAL([ 1, [ 1 , 0, [ ] ] ], [ 1, [ [ ], 1, 0 ] ]));
      assertFalse(RELATIONAL_EQUAL([ '' ], false));
      assertFalse(RELATIONAL_EQUAL([ '' ], ''));
      assertFalse(RELATIONAL_EQUAL([ '' ], [ ]));
      assertFalse(RELATIONAL_EQUAL([ true ], [ ]));
      assertFalse(RELATIONAL_EQUAL([ true ], [ false ]));
      assertFalse(RELATIONAL_EQUAL([ false ], [ ]));
      assertFalse(RELATIONAL_EQUAL([ null ], [ false ]));
      assertFalse(RELATIONAL_EQUAL([ ], ''));
      assertFalse(RELATIONAL_EQUAL({ }, { 'a' : false }));
      assertFalse(RELATIONAL_EQUAL({ 'a' : false }, { }));
      assertFalse(RELATIONAL_EQUAL({ 'a' : true }, { 'a' : false }));
      assertFalse(RELATIONAL_EQUAL({ 'a' : true }, { 'b' : true }));
      assertFalse(RELATIONAL_EQUAL({ 'b' : true }, { 'a' : true }));
      assertFalse(RELATIONAL_EQUAL({ 'a' : true, 'b' : [ 0, 1 ] }, { 'a' : true, 'b' : [ 1, 0 ] }));
      assertFalse(RELATIONAL_EQUAL({ 'a' : true, 'b' : { 'a' : false, 'b' : true } }, { 'a' : true, 'b' : { 'a' : true, 'b': true } }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_UNEQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalUnequalTrue: function() {
      assertTrue(RELATIONAL_UNEQUAL(1, 0));
      assertTrue(RELATIONAL_UNEQUAL(0, 1));
      assertTrue(RELATIONAL_UNEQUAL(0, false));
      assertTrue(RELATIONAL_UNEQUAL(false, 0));
      assertTrue(RELATIONAL_UNEQUAL(false, 0));
      assertTrue(RELATIONAL_UNEQUAL(-1, 1));
      assertTrue(RELATIONAL_UNEQUAL(1, -1));
      assertTrue(RELATIONAL_UNEQUAL(-1, 0));
      assertTrue(RELATIONAL_UNEQUAL(0, -1));
      assertTrue(RELATIONAL_UNEQUAL(true, false));
      assertTrue(RELATIONAL_UNEQUAL(false, true));
      assertTrue(RELATIONAL_UNEQUAL(true, 1));
      assertTrue(RELATIONAL_UNEQUAL(1, true));
      assertTrue(RELATIONAL_UNEQUAL(0, true));
      assertTrue(RELATIONAL_UNEQUAL(true, 0));
      assertTrue(RELATIONAL_UNEQUAL(true, 'true'));
      assertTrue(RELATIONAL_UNEQUAL(false, 'false'));
      assertTrue(RELATIONAL_UNEQUAL('true', true));
      assertTrue(RELATIONAL_UNEQUAL('false', false));
      assertTrue(RELATIONAL_UNEQUAL(-1.345, 1.345));
      assertTrue(RELATIONAL_UNEQUAL(1.345, -1.345));
      assertTrue(RELATIONAL_UNEQUAL(1.345, 1.346));
      assertTrue(RELATIONAL_UNEQUAL(1.346, 1.345));
      assertTrue(RELATIONAL_UNEQUAL(1.344, 1.345));
      assertTrue(RELATIONAL_UNEQUAL(1.345, 1.344));
      assertTrue(RELATIONAL_UNEQUAL(1, 2));
      assertTrue(RELATIONAL_UNEQUAL(2, 1));
      assertTrue(RELATIONAL_UNEQUAL(1.246e307, 1.245e307));
      assertTrue(RELATIONAL_UNEQUAL(1.246e307, 1.247e307));
      assertTrue(RELATIONAL_UNEQUAL(1.246e307, 1.2467e307));
      assertTrue(RELATIONAL_UNEQUAL(-99.43423, -99.434233));
      assertTrue(RELATIONAL_UNEQUAL(1.00001, 1.000001));
      assertTrue(RELATIONAL_UNEQUAL(1.00001, 1.0001));
      assertTrue(RELATIONAL_UNEQUAL(null, 1));
      assertTrue(RELATIONAL_UNEQUAL(1, null));
      assertTrue(RELATIONAL_UNEQUAL(null, 0));
      assertTrue(RELATIONAL_UNEQUAL(0, null));
      assertTrue(RELATIONAL_UNEQUAL(null, ''));
      assertTrue(RELATIONAL_UNEQUAL('', null));
      assertTrue(RELATIONAL_UNEQUAL(null, '0'));
      assertTrue(RELATIONAL_UNEQUAL('0', null));
      assertTrue(RELATIONAL_UNEQUAL(null, false));
      assertTrue(RELATIONAL_UNEQUAL(false, null));
      assertTrue(RELATIONAL_UNEQUAL(null, true));
      assertTrue(RELATIONAL_UNEQUAL(true, null));
      assertTrue(RELATIONAL_UNEQUAL(null, 'null'));
      assertTrue(RELATIONAL_UNEQUAL('null', null));
      assertTrue(RELATIONAL_UNEQUAL(0, ''));
      assertTrue(RELATIONAL_UNEQUAL('', 0));
      assertTrue(RELATIONAL_UNEQUAL(1, ''));
      assertTrue(RELATIONAL_UNEQUAL('', 1));
      assertTrue(RELATIONAL_UNEQUAL(' ', ''));
      assertTrue(RELATIONAL_UNEQUAL('', ' '));
      assertTrue(RELATIONAL_UNEQUAL(' 1', '1'));
      assertTrue(RELATIONAL_UNEQUAL('1', ' 1'));
      assertTrue(RELATIONAL_UNEQUAL('1 ', '1'));
      assertTrue(RELATIONAL_UNEQUAL('1', '1 '));
      assertTrue(RELATIONAL_UNEQUAL('1 ', ' 1'));
      assertTrue(RELATIONAL_UNEQUAL(' 1', '1 '));
      assertTrue(RELATIONAL_UNEQUAL(' 1 ', '1'));
      assertTrue(RELATIONAL_UNEQUAL('0', ''));
      assertTrue(RELATIONAL_UNEQUAL('', ' '));
      assertTrue(RELATIONAL_UNEQUAL('abc', 'abcd'));
      assertTrue(RELATIONAL_UNEQUAL('abcd', 'abc'));
      assertTrue(RELATIONAL_UNEQUAL('dabc', 'abcd'));
      assertTrue(RELATIONAL_UNEQUAL('1', 1));
      assertTrue(RELATIONAL_UNEQUAL(1, '1'));
      assertTrue(RELATIONAL_UNEQUAL('0', 0));
      assertTrue(RELATIONAL_UNEQUAL('1.0', 1.0));
      assertTrue(RELATIONAL_UNEQUAL('1.0', 1));
      assertTrue(RELATIONAL_UNEQUAL('-1', -1));
      assertTrue(RELATIONAL_UNEQUAL('1.234', 1.234));
      assertTrue(RELATIONAL_UNEQUAL([ 0 ], [ ]));
      assertTrue(RELATIONAL_UNEQUAL([ ], [ 0 ]));
      assertTrue(RELATIONAL_UNEQUAL([ ], [ 0, 1 ]));
      assertTrue(RELATIONAL_UNEQUAL([ 0 ], [ 0, 1 ]));
      assertTrue(RELATIONAL_UNEQUAL([ 1, 1, 0 ], [ 1, 0, 1 ]));
      assertTrue(RELATIONAL_UNEQUAL([ 1, 0, 0 ], [ 1, 0, 1 ]));
      assertTrue(RELATIONAL_UNEQUAL([ 1, 0, 0 ], [ 1, 0 ]));
      assertTrue(RELATIONAL_UNEQUAL([ 1, 0 ], [ 1, 0, 0 ]));
      assertTrue(RELATIONAL_UNEQUAL([ 1, 0 ], [ 0, 1 ]));
      assertTrue(RELATIONAL_UNEQUAL([ 1, 0 ], [ 0 ]));
      assertTrue(RELATIONAL_UNEQUAL([ 1, 0 ], [ 1 ]));
      assertTrue(RELATIONAL_UNEQUAL([ 1, 2, 3 ], [ 3, 2, 1 ]));
      assertTrue(RELATIONAL_UNEQUAL([ [ 1 ] ], [ [ 0 ] ]));
      assertTrue(RELATIONAL_UNEQUAL([ 1, [ 1 , 0 ] ], [ 1, [ 0, 1 ] ]));
      assertTrue(RELATIONAL_UNEQUAL([ 1, [ 1 , 0, [ ] ] ], [ 1, [ [ ], 1, 0 ] ]));
      assertTrue(RELATIONAL_UNEQUAL([ '' ], false));
      assertTrue(RELATIONAL_UNEQUAL([ '' ], ''));
      assertTrue(RELATIONAL_UNEQUAL([ '' ], [ ]));
      assertTrue(RELATIONAL_UNEQUAL([ true ], [ ]));
      assertTrue(RELATIONAL_UNEQUAL([ true ], [ false ]));
      assertTrue(RELATIONAL_UNEQUAL([ false ], [ ]));
      assertTrue(RELATIONAL_UNEQUAL([ null ], [ false ]));
      assertTrue(RELATIONAL_UNEQUAL([ ], null));
      assertTrue(RELATIONAL_UNEQUAL([ ], ''));
      assertTrue(RELATIONAL_UNEQUAL({ }, { 'a' : false }));
      assertTrue(RELATIONAL_UNEQUAL({ 'a' : false }, { }));
      assertTrue(RELATIONAL_UNEQUAL({ 'a' : true }, { 'a' : false }));
      assertTrue(RELATIONAL_UNEQUAL({ 'a' : true }, { 'b' : true }));
      assertTrue(RELATIONAL_UNEQUAL({ 'b' : true }, { 'a' : true }));
      assertTrue(RELATIONAL_UNEQUAL({ 'a' : true, 'b' : [ 0, 1 ] }, { 'a' : true, 'b' : [ 1, 0 ] }));
      assertTrue(RELATIONAL_UNEQUAL({ 'a' : true, 'b' : { 'a' : false, 'b' : true } }, { 'a' : true, 'b' : { 'a' : true, 'b': true } }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_UNEQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalUnequalFalse: function() {
      assertFalse(RELATIONAL_UNEQUAL(1, 1));
      assertFalse(RELATIONAL_UNEQUAL(0, 0));
      assertFalse(RELATIONAL_UNEQUAL(-1, -1));
      assertFalse(RELATIONAL_UNEQUAL(1.345, 1.345));
      assertFalse(RELATIONAL_UNEQUAL(1.0, 1.00));
      assertFalse(RELATIONAL_UNEQUAL(1.0, 1.000));
      assertFalse(RELATIONAL_UNEQUAL(1.1, 1.1));
      assertFalse(RELATIONAL_UNEQUAL(1.01, 1.01));
      assertFalse(RELATIONAL_UNEQUAL(1.001, 1.001));
      assertFalse(RELATIONAL_UNEQUAL(1.0001, 1.0001));
      assertFalse(RELATIONAL_UNEQUAL(1.00001, 1.00001));
      assertFalse(RELATIONAL_UNEQUAL(1.000001, 1.000001));
      assertFalse(RELATIONAL_UNEQUAL(1.245e307, 1.245e307));
      assertFalse(RELATIONAL_UNEQUAL(-99.43423, -99.43423));
      assertFalse(RELATIONAL_UNEQUAL(true, true));
      assertFalse(RELATIONAL_UNEQUAL(false, false));
      assertFalse(RELATIONAL_UNEQUAL('', ''));
      assertFalse(RELATIONAL_UNEQUAL(' ', ' '));
      assertFalse(RELATIONAL_UNEQUAL(' 1', ' 1'));
      assertFalse(RELATIONAL_UNEQUAL('0', '0'));
      assertFalse(RELATIONAL_UNEQUAL('abc', 'abc'));
      assertFalse(RELATIONAL_UNEQUAL('-1', '-1'));
      assertFalse(RELATIONAL_UNEQUAL(null, null));
      assertFalse(RELATIONAL_UNEQUAL('true', 'true'));
      assertFalse(RELATIONAL_UNEQUAL('false', 'false'));
      assertFalse(RELATIONAL_UNEQUAL('undefined', 'undefined'));
      assertFalse(RELATIONAL_UNEQUAL('null', 'null'));
      assertFalse(RELATIONAL_UNEQUAL([ ], [ ]));
      assertFalse(RELATIONAL_UNEQUAL([ null ], [ ]));
      assertFalse(RELATIONAL_UNEQUAL([ ], [ null ]));
      assertFalse(RELATIONAL_UNEQUAL([ 0 ], [ 0 ]));
      assertFalse(RELATIONAL_UNEQUAL([ 0, 1 ], [ 0, 1 ]));
      assertFalse(RELATIONAL_UNEQUAL([ 0, 1, 4 ], [ 0, 1, 4 ]));
      assertFalse(RELATIONAL_UNEQUAL([ 3, 4, -99 ], [ 3, 4, -99 ]));
      assertFalse(RELATIONAL_UNEQUAL([ 'a', 4, [ 1, 'a' ], false ], [ 'a', 4, [ 1, 'a' ], false ]));
      assertFalse(RELATIONAL_UNEQUAL({ }, { }));
      assertFalse(RELATIONAL_UNEQUAL({ 'a' : true }, { 'a' : true }));
      assertFalse(RELATIONAL_UNEQUAL({ 'a' : true, 'b': true }, { 'a' : true, 'b': true }));
      assertFalse(RELATIONAL_UNEQUAL({ 'a' : true, 'b': true }, { 'b' : true, 'a': true }));
      assertFalse(RELATIONAL_UNEQUAL({ 'b' : true, 'a': true }, { 'b' : true, 'a': true }));
      assertFalse(RELATIONAL_UNEQUAL({ 'b' : true, 'a': true }, { 'a' : true, 'b': true }));
      assertFalse(RELATIONAL_UNEQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }));
      assertFalse(RELATIONAL_UNEQUAL({ 'a' : [ 0, 1 ], 'b' : [ 1, 9 ] }, { 'b' : [ 1, 9 ], 'a' : [ 0, 1 ] }));
      assertFalse(RELATIONAL_UNEQUAL({ 'f' : { 'c' : { 'd' : [ 0, 1 ], 'a' : [ 1, 9 ] }, 'a' : false }, 'a' : true }, { 'a' : true, 'f' : { 'a' : false, 'c' : { 'a' : [ 1, 9 ], 'd' : [ 0, 1 ] } } }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_LESS function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalLessTrue: function() {
      assertTrue(RELATIONAL_LESS(null, false));
      assertTrue(RELATIONAL_LESS(null, true));
      assertTrue(RELATIONAL_LESS(null, 0));
      assertTrue(RELATIONAL_LESS(null, 1));
      assertTrue(RELATIONAL_LESS(null, -1));
      assertTrue(RELATIONAL_LESS(null, ''));
      assertTrue(RELATIONAL_LESS(null, ' '));
      assertTrue(RELATIONAL_LESS(null, '1'));
      assertTrue(RELATIONAL_LESS(null, '0'));
      assertTrue(RELATIONAL_LESS(null, 'abcd'));
      assertTrue(RELATIONAL_LESS(null, 'null'));
      assertTrue(RELATIONAL_LESS(null, [ ]));
      assertTrue(RELATIONAL_LESS(null, [ true ]));
      assertTrue(RELATIONAL_LESS(null, [ false ]));
      assertTrue(RELATIONAL_LESS(null, [ null ]));
      assertTrue(RELATIONAL_LESS(null, [ 0 ]));
      assertTrue(RELATIONAL_LESS(null, { }));
      assertTrue(RELATIONAL_LESS(null, { 'a' : null }));
      assertTrue(RELATIONAL_LESS(false, true));
      assertTrue(RELATIONAL_LESS(false, 0));
      assertTrue(RELATIONAL_LESS(false, 1));
      assertTrue(RELATIONAL_LESS(false, -1));
      assertTrue(RELATIONAL_LESS(false, ''));
      assertTrue(RELATIONAL_LESS(false, ' '));
      assertTrue(RELATIONAL_LESS(false, '1'));
      assertTrue(RELATIONAL_LESS(false, '0'));
      assertTrue(RELATIONAL_LESS(false, 'abcd'));
      assertTrue(RELATIONAL_LESS(false, 'true'));
      assertTrue(RELATIONAL_LESS(false, [ ]));
      assertTrue(RELATIONAL_LESS(false, [ true ]));
      assertTrue(RELATIONAL_LESS(false, [ false ]));
      assertTrue(RELATIONAL_LESS(false, [ null ]));
      assertTrue(RELATIONAL_LESS(false, [ 0 ]));
      assertTrue(RELATIONAL_LESS(false, { }));
      assertTrue(RELATIONAL_LESS(false, { 'a' : null }));
      assertTrue(RELATIONAL_LESS(true, 0));
      assertTrue(RELATIONAL_LESS(true, 1));
      assertTrue(RELATIONAL_LESS(true, -1));
      assertTrue(RELATIONAL_LESS(true, ''));
      assertTrue(RELATIONAL_LESS(true, ' '));
      assertTrue(RELATIONAL_LESS(true, '1'));
      assertTrue(RELATIONAL_LESS(true, '0'));
      assertTrue(RELATIONAL_LESS(true, 'abcd'));
      assertTrue(RELATIONAL_LESS(true, 'true'));
      assertTrue(RELATIONAL_LESS(true, [ ]));
      assertTrue(RELATIONAL_LESS(true, [ true ]));
      assertTrue(RELATIONAL_LESS(true, [ false ]));
      assertTrue(RELATIONAL_LESS(true, [ null ]));
      assertTrue(RELATIONAL_LESS(true, [ 0 ]));
      assertTrue(RELATIONAL_LESS(true, { }));
      assertTrue(RELATIONAL_LESS(true, { 'a' : null }));
      assertTrue(RELATIONAL_LESS(0, 1));
      assertTrue(RELATIONAL_LESS(1, 2));
      assertTrue(RELATIONAL_LESS(1, 100));
      assertTrue(RELATIONAL_LESS(20, 100));
      assertTrue(RELATIONAL_LESS(-100, 1));
      assertTrue(RELATIONAL_LESS(-100, -10));
      assertTrue(RELATIONAL_LESS(-11, -10));
      assertTrue(RELATIONAL_LESS(999, 1000));
      assertTrue(RELATIONAL_LESS(-1, 1));
      assertTrue(RELATIONAL_LESS(-1, 0));
      assertTrue(RELATIONAL_LESS(1.0, 1.01));
      assertTrue(RELATIONAL_LESS(1.111, 1.2));
      assertTrue(RELATIONAL_LESS(-1.111, -1.110));
      assertTrue(RELATIONAL_LESS(-1.111, -1.1109));
      assertTrue(RELATIONAL_LESS(0, ''));
      assertTrue(RELATIONAL_LESS(0, ' '));
      assertTrue(RELATIONAL_LESS(0, '0'));
      assertTrue(RELATIONAL_LESS(0, '1'));
      assertTrue(RELATIONAL_LESS(0, '-1'));
      assertTrue(RELATIONAL_LESS(0, 'true'));
      assertTrue(RELATIONAL_LESS(0, 'false'));
      assertTrue(RELATIONAL_LESS(0, 'null'));
      assertTrue(RELATIONAL_LESS(1, ''));
      assertTrue(RELATIONAL_LESS(1, ' '));
      assertTrue(RELATIONAL_LESS(1, '0'));
      assertTrue(RELATIONAL_LESS(1, '1'));
      assertTrue(RELATIONAL_LESS(1, '-1'));
      assertTrue(RELATIONAL_LESS(1, 'true'));
      assertTrue(RELATIONAL_LESS(1, 'false'));
      assertTrue(RELATIONAL_LESS(1, 'null'));
      assertTrue(RELATIONAL_LESS(0, '-1'));
      assertTrue(RELATIONAL_LESS(0, '-100'));
      assertTrue(RELATIONAL_LESS(0, '-1.1'));
      assertTrue(RELATIONAL_LESS(0, '-0.0'));
      assertTrue(RELATIONAL_LESS(1000, '-1'));
      assertTrue(RELATIONAL_LESS(1000, '10'));
      assertTrue(RELATIONAL_LESS(1000, '10000'));
      assertTrue(RELATIONAL_LESS(0, [ ]));
      assertTrue(RELATIONAL_LESS(0, [ 0 ]));
      assertTrue(RELATIONAL_LESS(10, [ ]));
      assertTrue(RELATIONAL_LESS(100, [ ]));
      assertTrue(RELATIONAL_LESS(100, [ 0 ]));
      assertTrue(RELATIONAL_LESS(100, [ 0, 1 ]));
      assertTrue(RELATIONAL_LESS(100, [ 99 ]));
      assertTrue(RELATIONAL_LESS(100, [ 100 ]));
      assertTrue(RELATIONAL_LESS(100, [ 101 ]));
      assertTrue(RELATIONAL_LESS(100, { }));
      assertTrue(RELATIONAL_LESS(100, { 'a' : 0 }));
      assertTrue(RELATIONAL_LESS(100, { 'a' : 1 }));
      assertTrue(RELATIONAL_LESS(100, { 'a' : 99 }));
      assertTrue(RELATIONAL_LESS(100, { 'a' : 100 }));
      assertTrue(RELATIONAL_LESS(100, { 'a' : 101 }));
      assertTrue(RELATIONAL_LESS(100, { 'a' : 1000 }));
      assertTrue(RELATIONAL_LESS('', ' '));
      assertTrue(RELATIONAL_LESS('0', 'a'));
      assertTrue(RELATIONAL_LESS('a', 'a '));
      assertTrue(RELATIONAL_LESS('a', 'b'));
      assertTrue(RELATIONAL_LESS('A', 'a'));
      assertTrue(RELATIONAL_LESS('AB', 'Ab'));
      assertTrue(RELATIONAL_LESS('abcd', 'bbcd'));
      assertTrue(RELATIONAL_LESS('abcd', 'abda'));
      assertTrue(RELATIONAL_LESS('abcd', 'abdd'));
      assertTrue(RELATIONAL_LESS('abcd', 'abcde'));
      assertTrue(RELATIONAL_LESS('0abcd', 'abcde'));
      assertTrue(RELATIONAL_LESS('abcd', [ ]));
      assertTrue(RELATIONAL_LESS('abcd', [ 0 ]));
      assertTrue(RELATIONAL_LESS('abcd', [ -1 ]));
      assertTrue(RELATIONAL_LESS('abcd', [ " " ]));
      assertTrue(RELATIONAL_LESS('abcd', [ "" ]));
      assertTrue(RELATIONAL_LESS('abcd', [ "abc" ]));
      assertTrue(RELATIONAL_LESS('abcd', [ "abcd" ]));
      assertTrue(RELATIONAL_LESS('abcd', { } ));
      assertTrue(RELATIONAL_LESS('abcd', { 'a' : true } ));
      assertTrue(RELATIONAL_LESS('abcd', { 'abc' : true } ));
      assertTrue(RELATIONAL_LESS('ABCD', { 'a' : true } ));
      assertTrue(RELATIONAL_LESS([ ], [ 0 ]));
      assertTrue(RELATIONAL_LESS([ 0 ], [ 1 ]));
      assertTrue(RELATIONAL_LESS([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertTrue(RELATIONAL_LESS([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertTrue(RELATIONAL_LESS([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertTrue(RELATIONAL_LESS([ 0, 1, 4 ], [ 1 ]));
      assertTrue(RELATIONAL_LESS([ 15, 99 ], [ 110 ]));
      assertTrue(RELATIONAL_LESS([ 15, 99 ], [ 15, 100 ]));
      assertTrue(RELATIONAL_LESS([ ], [ false ]));
      assertTrue(RELATIONAL_LESS([ ], [ true ]));
      assertTrue(RELATIONAL_LESS([ ], [ 0 ]));
      assertTrue(RELATIONAL_LESS([ ], [ -1 ]));
      assertTrue(RELATIONAL_LESS([ ], [ '' ]));
      assertTrue(RELATIONAL_LESS([ ], [ '0' ]));
      assertTrue(RELATIONAL_LESS([ ], [ 'abcd' ]));
      assertTrue(RELATIONAL_LESS([ ], [ [ ] ]));
      assertTrue(RELATIONAL_LESS([ ], [ [ null ] ]));
      assertTrue(RELATIONAL_LESS([ ], [ { } ]));
      assertTrue(RELATIONAL_LESS([ null ], [ false ]));
      assertTrue(RELATIONAL_LESS([ null ], [ true ]));
      assertTrue(RELATIONAL_LESS([ null ], [ 0 ]));
      assertTrue(RELATIONAL_LESS([ null ], [ [ ] ]));
      assertTrue(RELATIONAL_LESS([ false ], [ true ]));
      assertTrue(RELATIONAL_LESS([ false ], [ 0 ]));
      assertTrue(RELATIONAL_LESS([ false ], [ -1 ]));
      assertTrue(RELATIONAL_LESS([ false ], [ '' ]));
      assertTrue(RELATIONAL_LESS([ false ], [ '0' ]));
      assertTrue(RELATIONAL_LESS([ false ], [ 'abcd' ]));
      assertTrue(RELATIONAL_LESS([ false ], [ [ ] ]));
      assertTrue(RELATIONAL_LESS([ false ], [ [ false ] ]));
      assertTrue(RELATIONAL_LESS([ true ], [ 0 ]));
      assertTrue(RELATIONAL_LESS([ true ], [ -1 ]));
      assertTrue(RELATIONAL_LESS([ true ], [ '' ]));
      assertTrue(RELATIONAL_LESS([ true ], [ '0' ]));
      assertTrue(RELATIONAL_LESS([ true ], [ 'abcd' ]));
      assertTrue(RELATIONAL_LESS([ true ], [ [ ] ]));
      assertTrue(RELATIONAL_LESS([ true ], [ [ false ] ]));
      assertTrue(RELATIONAL_LESS([ false, false ], [ true ]));
      assertTrue(RELATIONAL_LESS([ false, false ], [ false, true ]));
      assertTrue(RELATIONAL_LESS([ false, false ], [ false, 0 ]));
      assertTrue(RELATIONAL_LESS([ null, null ], [ null, false ]));
      assertTrue(RELATIONAL_LESS([ ], { }));
      assertTrue(RELATIONAL_LESS([ ], { 'a' : true }));
      assertTrue(RELATIONAL_LESS([ ], { 'a' : null }));
      assertTrue(RELATIONAL_LESS([ ], { 'a' : false }));
      assertTrue(RELATIONAL_LESS([ '' ], { }));
      assertTrue(RELATIONAL_LESS([ 0 ], { }));
      assertTrue(RELATIONAL_LESS([ null ], { }));
      assertTrue(RELATIONAL_LESS([ false ], { }));
      assertTrue(RELATIONAL_LESS([ false ], { 'a' : false }));
      assertTrue(RELATIONAL_LESS([ true ], { 'a' : false }));
      assertTrue(RELATIONAL_LESS([ 'abcd' ], { 'a' : false }));
      assertTrue(RELATIONAL_LESS([ 5 ], { 'a' : false }));
      assertTrue(RELATIONAL_LESS([ 5, 6 ], { 'a' : 2, 'b' : 2 }));
      assertTrue(RELATIONAL_LESS([ 5, 6, 7 ], { }));
      assertTrue(RELATIONAL_LESS([ 5, 6, false ], [ 5, 6, true ]));
      assertTrue(RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, 0 ]));
      assertTrue(RELATIONAL_LESS([ 5, 6, 999 ], [ 5, 6, '' ]));
      assertTrue(RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, 'b' ]));
      assertTrue(RELATIONAL_LESS([ 5, 6, 'A' ], [ 5, 6, 'a' ]));
      assertTrue(RELATIONAL_LESS([ 5, 6, '' ], [ 5, 6, 'a' ]));
      assertTrue(RELATIONAL_LESS([ 5, 6, 9, 9 ], [ 5, 6, [ ] ]));
      assertTrue(RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, [ ] ]));
      assertTrue(RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, { } ]));
      assertTrue(RELATIONAL_LESS([ 5, 6, 9, 9 ], [ 5, 6, { } ]));
      assertTrue(RELATIONAL_LESS({ }, { 'a' : 0 }));
      assertTrue(RELATIONAL_LESS({ 'a' : 1 }, { 'a' : 2 }));
      assertTrue(RELATIONAL_LESS({ 'b' : 2 }, { 'a' : 1 }));
      assertTrue(RELATIONAL_LESS({ 'z' : 1 }, { 'c' : 1 }));
      assertTrue(RELATIONAL_LESS({ 'a' : [ 9 ], 'b' : false }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(RELATIONAL_LESS({ 'a' : [ 9 ], 'b' : true }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(RELATIONAL_LESS({ 'a' : [ ], 'b' : true }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(RELATIONAL_LESS({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 10, 1 ] }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_LESS function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalLessFalse: function() {
      assertFalse(RELATIONAL_LESS(null, null));
      assertFalse(RELATIONAL_LESS(false, null));
      assertFalse(RELATIONAL_LESS(true, null));
      assertFalse(RELATIONAL_LESS(0, null));
      assertFalse(RELATIONAL_LESS(1, null));
      assertFalse(RELATIONAL_LESS(-1, null));
      assertFalse(RELATIONAL_LESS('', null));
      assertFalse(RELATIONAL_LESS(' ', null));
      assertFalse(RELATIONAL_LESS('1', null));
      assertFalse(RELATIONAL_LESS('0', null));
      assertFalse(RELATIONAL_LESS('abcd', null));
      assertFalse(RELATIONAL_LESS('null', null));
      assertFalse(RELATIONAL_LESS([ ], null));
      assertFalse(RELATIONAL_LESS([ true ], null));
      assertFalse(RELATIONAL_LESS([ false ], null));
      assertFalse(RELATIONAL_LESS([ null ], null));
      assertFalse(RELATIONAL_LESS([ 0 ], null));
      assertFalse(RELATIONAL_LESS({ }, null));
      assertFalse(RELATIONAL_LESS({ 'a' : null }, null));
      assertFalse(RELATIONAL_LESS(false, false));
      assertFalse(RELATIONAL_LESS(true, true));
      assertFalse(RELATIONAL_LESS(true, false));
      assertFalse(RELATIONAL_LESS(0, false));
      assertFalse(RELATIONAL_LESS(1, false));
      assertFalse(RELATIONAL_LESS(-1, false));
      assertFalse(RELATIONAL_LESS('', false));
      assertFalse(RELATIONAL_LESS(' ', false));
      assertFalse(RELATIONAL_LESS('1', false));
      assertFalse(RELATIONAL_LESS('0', false));
      assertFalse(RELATIONAL_LESS('abcd', false));
      assertFalse(RELATIONAL_LESS('true', false));
      assertFalse(RELATIONAL_LESS([ ], false));
      assertFalse(RELATIONAL_LESS([ true ], false));
      assertFalse(RELATIONAL_LESS([ false ], false));
      assertFalse(RELATIONAL_LESS([ null ], false));
      assertFalse(RELATIONAL_LESS([ 0 ], false));
      assertFalse(RELATIONAL_LESS({ }, false));
      assertFalse(RELATIONAL_LESS({ 'a' : null }, false));
      assertFalse(RELATIONAL_LESS(0, true));
      assertFalse(RELATIONAL_LESS(1, true));
      assertFalse(RELATIONAL_LESS(-1, true));
      assertFalse(RELATIONAL_LESS('', true));
      assertFalse(RELATIONAL_LESS(' ', true));
      assertFalse(RELATIONAL_LESS('1', true));
      assertFalse(RELATIONAL_LESS('0', true));
      assertFalse(RELATIONAL_LESS('abcd', true));
      assertFalse(RELATIONAL_LESS('true', true));
      assertFalse(RELATIONAL_LESS([ ], true));
      assertFalse(RELATIONAL_LESS([ true ], true));
      assertFalse(RELATIONAL_LESS([ false ], true));
      assertFalse(RELATIONAL_LESS([ null ], true));
      assertFalse(RELATIONAL_LESS([ 0 ], true));
      assertFalse(RELATIONAL_LESS({ }, true));
      assertFalse(RELATIONAL_LESS({ 'a' : null }, true));
      assertFalse(RELATIONAL_LESS(0, 0));
      assertFalse(RELATIONAL_LESS(1, 1));
      assertFalse(RELATIONAL_LESS(-10, -10));
      assertFalse(RELATIONAL_LESS(-100, -100));
      assertFalse(RELATIONAL_LESS(-334.5, -334.5));
      assertFalse(RELATIONAL_LESS(1, 0));
      assertFalse(RELATIONAL_LESS(2, 1));
      assertFalse(RELATIONAL_LESS(100, 1));
      assertFalse(RELATIONAL_LESS(100, 20));
      assertFalse(RELATIONAL_LESS(1, -100));
      assertFalse(RELATIONAL_LESS(-10, -100));
      assertFalse(RELATIONAL_LESS(-10, -11));
      assertFalse(RELATIONAL_LESS(1000, 999));
      assertFalse(RELATIONAL_LESS(1, -1));
      assertFalse(RELATIONAL_LESS(0, -1));
      assertFalse(RELATIONAL_LESS(1.01, 1.0));
      assertFalse(RELATIONAL_LESS(1.2, 1.111));
      assertFalse(RELATIONAL_LESS(-1.110, -1.111));
      assertFalse(RELATIONAL_LESS(-1.1109, -1.111));
      assertFalse(RELATIONAL_LESS('', 0));
      assertFalse(RELATIONAL_LESS(' ', 0));
      assertFalse(RELATIONAL_LESS('0', 0));
      assertFalse(RELATIONAL_LESS('1', 0));
      assertFalse(RELATIONAL_LESS('-1', 0));
      assertFalse(RELATIONAL_LESS('true', 0));
      assertFalse(RELATIONAL_LESS('false', 0));
      assertFalse(RELATIONAL_LESS('null', 0));
      assertFalse(RELATIONAL_LESS('', 1));
      assertFalse(RELATIONAL_LESS(' ', 1));
      assertFalse(RELATIONAL_LESS('0', 1));
      assertFalse(RELATIONAL_LESS('1', 1));
      assertFalse(RELATIONAL_LESS('-1', 1));
      assertFalse(RELATIONAL_LESS('true', 1));
      assertFalse(RELATIONAL_LESS('false', 1));
      assertFalse(RELATIONAL_LESS('null', 1));
      assertFalse(RELATIONAL_LESS('-1', 0));
      assertFalse(RELATIONAL_LESS('-100', 0));
      assertFalse(RELATIONAL_LESS('-1.1', 0));
      assertFalse(RELATIONAL_LESS('-0.0', 0));
      assertFalse(RELATIONAL_LESS('-1', 1000));
      assertFalse(RELATIONAL_LESS('10', 1000));
      assertFalse(RELATIONAL_LESS('10000', 1000));
      assertFalse(RELATIONAL_LESS([ ], 0));
      assertFalse(RELATIONAL_LESS([ 0 ], 0));
      assertFalse(RELATIONAL_LESS([ ], 10));
      assertFalse(RELATIONAL_LESS([ ], 100));
      assertFalse(RELATIONAL_LESS([ 0 ], 100));
      assertFalse(RELATIONAL_LESS([ 0, 1 ], 100));
      assertFalse(RELATIONAL_LESS([ 99 ], 100));
      assertFalse(RELATIONAL_LESS([ 100 ], 100));
      assertFalse(RELATIONAL_LESS([ 101 ], 100));
      assertFalse(RELATIONAL_LESS({ }, 100));
      assertFalse(RELATIONAL_LESS({ 'a' : 0 }, 100));
      assertFalse(RELATIONAL_LESS({ 'a' : 1 }, 100));
      assertFalse(RELATIONAL_LESS({ 'a' : 99 }, 100));
      assertFalse(RELATIONAL_LESS({ 'a' : 100 }, 100));
      assertFalse(RELATIONAL_LESS({ 'a' : 101 }, 100));
      assertFalse(RELATIONAL_LESS({ 'a' : 1000 }, 100));
      assertFalse(RELATIONAL_LESS({ 'a' : false }, 'zz'));
      assertFalse(RELATIONAL_LESS({ 'a' : 'a' }, 'zz'));
      assertFalse(RELATIONAL_LESS('', ''));
      assertFalse(RELATIONAL_LESS(' ', ' '));
      assertFalse(RELATIONAL_LESS('a', 'a'));
      assertFalse(RELATIONAL_LESS(' a', ' a'));
      assertFalse(RELATIONAL_LESS('abcd', 'abcd'));
      assertFalse(RELATIONAL_LESS(' ', ''));
      assertFalse(RELATIONAL_LESS('a', '0'));
      assertFalse(RELATIONAL_LESS('a ', 'a'));
      assertFalse(RELATIONAL_LESS('b', 'a'));
      assertFalse(RELATIONAL_LESS('a', 'A'));
      assertFalse(RELATIONAL_LESS('Ab', 'AB'));
      assertFalse(RELATIONAL_LESS('bbcd', 'abcd'));
      assertFalse(RELATIONAL_LESS('abda', 'abcd'));
      assertFalse(RELATIONAL_LESS('abdd', 'abcd'));
      assertFalse(RELATIONAL_LESS('abcde', 'abcd'));
      assertFalse(RELATIONAL_LESS('abcde', '0abcde'));
      assertFalse(RELATIONAL_LESS([ ], [ ]));
      assertFalse(RELATIONAL_LESS([ 0 ], [ 0 ]));
      assertFalse(RELATIONAL_LESS([ 1 ], [ 1 ]));
      assertFalse(RELATIONAL_LESS([ true ], [ true ]));
      assertFalse(RELATIONAL_LESS([ false ], [ false ]));
      assertFalse(RELATIONAL_LESS([ [ 0, 1, 2 ] ], [ [ 0, 1, 2 ] ]));
      assertFalse(RELATIONAL_LESS([ [ 1, [ "true", 0, -99 , false ] ], 4 ], [ [ 1, [ "true", 0, -99, false ] ], 4 ]));
      assertFalse(RELATIONAL_LESS([ 0 ], [ ]));
      assertFalse(RELATIONAL_LESS([ 1 ], [ 0 ]));
      assertFalse(RELATIONAL_LESS([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertFalse(RELATIONAL_LESS([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertFalse(RELATIONAL_LESS([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertFalse(RELATIONAL_LESS([ 1 ], [ 0, 1, 4 ]));
      assertFalse(RELATIONAL_LESS([ 110 ], [ 15, 99 ]));
      assertFalse(RELATIONAL_LESS([ 15, 100 ], [ 15, 99 ]));
      assertFalse(RELATIONAL_LESS([ null ], [ ]));
      assertFalse(RELATIONAL_LESS([ false ], [ ]));
      assertFalse(RELATIONAL_LESS([ true ], [ ]));
      assertFalse(RELATIONAL_LESS([ 0 ], [ ]));
      assertFalse(RELATIONAL_LESS([ -1 ], [ ]));
      assertFalse(RELATIONAL_LESS([ '' ], [ ]));
      assertFalse(RELATIONAL_LESS([ '0' ], [ ]));
      assertFalse(RELATIONAL_LESS([ 'abcd' ], [ ]));
      assertFalse(RELATIONAL_LESS([ [ ] ], [ ]));
      assertFalse(RELATIONAL_LESS([ [ null ] ], [ ]));
      assertFalse(RELATIONAL_LESS([ { } ], [ ]));
      assertFalse(RELATIONAL_LESS([ false ], [ null ]));
      assertFalse(RELATIONAL_LESS([ true ], [ null ]));
      assertFalse(RELATIONAL_LESS([ 0 ], [ null ]));
      assertFalse(RELATIONAL_LESS([ [ ] ], [ null ]));
      assertFalse(RELATIONAL_LESS([ true ], [ false ]));
      assertFalse(RELATIONAL_LESS([ 0 ], [ false ]));
      assertFalse(RELATIONAL_LESS([ -1 ], [ false ]));
      assertFalse(RELATIONAL_LESS([ '' ], [ false ]));
      assertFalse(RELATIONAL_LESS([ '0' ], [ false ]));
      assertFalse(RELATIONAL_LESS([ 'abcd' ], [ false ]));
      assertFalse(RELATIONAL_LESS([ [ ] ], [ false ]));
      assertFalse(RELATIONAL_LESS([ [ false ] ], [ false ]));
      assertFalse(RELATIONAL_LESS([ 0 ], [ true ]));
      assertFalse(RELATIONAL_LESS([ -1 ], [ true ]));
      assertFalse(RELATIONAL_LESS([ '' ], [ true ]));
      assertFalse(RELATIONAL_LESS([ '0' ], [ true ]));
      assertFalse(RELATIONAL_LESS([ 'abcd' ], [ true ]));
      assertFalse(RELATIONAL_LESS([ [ ] ], [ true ]));
      assertFalse(RELATIONAL_LESS([ [ false] ], [ true ]));
      assertFalse(RELATIONAL_LESS([ true ], [ false, false ]));
      assertFalse(RELATIONAL_LESS([ false, true ], [ false, false ]));
      assertFalse(RELATIONAL_LESS([ false, 0 ], [ false, false ]));
      assertFalse(RELATIONAL_LESS([ null, false ], [ null, null ]));
      assertFalse(RELATIONAL_LESS({ }, [ ]));
      assertFalse(RELATIONAL_LESS({ 'a' : true }, [ ]));
      assertFalse(RELATIONAL_LESS({ 'a' : null }, [ ]));
      assertFalse(RELATIONAL_LESS({ 'a' : false }, [ ]));
      assertFalse(RELATIONAL_LESS({ }, [ '' ]));
      assertFalse(RELATIONAL_LESS({ }, [ 0 ]));
      assertFalse(RELATIONAL_LESS({ }, [ null ]));
      assertFalse(RELATIONAL_LESS({ }, [ false ]));
      assertFalse(RELATIONAL_LESS({ 'a' : false }, [ false ]));
      assertFalse(RELATIONAL_LESS({ 'a' : false }, [ true ]));
      assertFalse(RELATIONAL_LESS({ 'a' : false }, [ 'abcd' ]));
      assertFalse(RELATIONAL_LESS({ 'a' : false }, [ 5 ]));
      assertFalse(RELATIONAL_LESS({ 'a' : 2, 'b' : 2 }, [ 5, 6 ]));
      assertFalse(RELATIONAL_LESS({ 'a' : 1, 'b' : 2 }, { 'a' : 1, 'b' : 2, 'c' : null }));
      assertFalse(RELATIONAL_LESS({ 'b' : 2, 'a' : 1 }, { 'a' : 1, 'b' : 2, 'c' : null }));
      assertFalse(RELATIONAL_LESS({ }, [ 5, 6, 7 ]));
      assertFalse(RELATIONAL_LESS([ 5, 6, true ], [ 5, 6, false ]));
      assertFalse(RELATIONAL_LESS([ 5, 6, 0 ], [ 5, 6, true ]));
      assertFalse(RELATIONAL_LESS([ 5, 6, '' ], [ 5, 6, 999 ]));
      assertFalse(RELATIONAL_LESS([ 5, 6, 'b' ], [ 5, 6, 'a' ]));
      assertFalse(RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, 'A' ]));
      assertFalse(RELATIONAL_LESS([ 5, 6, 'a' ], [ 5, 6, '' ]));
      assertFalse(RELATIONAL_LESS([ 5, 6, [ ] ], [ 5, 6, 9 ,9 ]));
      assertFalse(RELATIONAL_LESS([ 5, 6, [ ] ], [ 5, 6, true ]));
      assertFalse(RELATIONAL_LESS([ 5, 6, { } ], [ 5, 6, true ]));
      assertFalse(RELATIONAL_LESS([ 5, 6, { } ], [ 5, 6, 9, 9 ]));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_GREATER function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterTrue: function() {
      assertTrue(RELATIONAL_GREATER(false, null));
      assertTrue(RELATIONAL_GREATER(true, null));
      assertTrue(RELATIONAL_GREATER(0, null));
      assertTrue(RELATIONAL_GREATER(1, null));
      assertTrue(RELATIONAL_GREATER(-1, null));
      assertTrue(RELATIONAL_GREATER('', null));
      assertTrue(RELATIONAL_GREATER(' ', null));
      assertTrue(RELATIONAL_GREATER('1', null));
      assertTrue(RELATIONAL_GREATER('0', null));
      assertTrue(RELATIONAL_GREATER('abcd', null));
      assertTrue(RELATIONAL_GREATER('null', null));
      assertTrue(RELATIONAL_GREATER([ ], null));
      assertTrue(RELATIONAL_GREATER([ true ], null));
      assertTrue(RELATIONAL_GREATER([ false ], null));
      assertTrue(RELATIONAL_GREATER([ null ], null));
      assertTrue(RELATIONAL_GREATER([ 0 ], null));
      assertTrue(RELATIONAL_GREATER({ }, null));
      assertTrue(RELATIONAL_GREATER({ 'a' : null }, null));
      assertTrue(RELATIONAL_GREATER(true, false));
      assertTrue(RELATIONAL_GREATER(0, false));
      assertTrue(RELATIONAL_GREATER(1, false));
      assertTrue(RELATIONAL_GREATER(-1, false));
      assertTrue(RELATIONAL_GREATER('', false));
      assertTrue(RELATIONAL_GREATER(' ', false));
      assertTrue(RELATIONAL_GREATER('1', false));
      assertTrue(RELATIONAL_GREATER('0', false));
      assertTrue(RELATIONAL_GREATER('abcd', false));
      assertTrue(RELATIONAL_GREATER('true', false));
      assertTrue(RELATIONAL_GREATER([ ], false));
      assertTrue(RELATIONAL_GREATER([ true ], false));
      assertTrue(RELATIONAL_GREATER([ false ], false));
      assertTrue(RELATIONAL_GREATER([ null ], false));
      assertTrue(RELATIONAL_GREATER([ 0 ], false));
      assertTrue(RELATIONAL_GREATER({ }, false));
      assertTrue(RELATIONAL_GREATER({ 'a' : null }, false));
      assertTrue(RELATIONAL_GREATER(0, true));
      assertTrue(RELATIONAL_GREATER(1, true));
      assertTrue(RELATIONAL_GREATER(-1, true));
      assertTrue(RELATIONAL_GREATER('', true));
      assertTrue(RELATIONAL_GREATER(' ', true));
      assertTrue(RELATIONAL_GREATER('1', true));
      assertTrue(RELATIONAL_GREATER('0', true));
      assertTrue(RELATIONAL_GREATER('abcd', true));
      assertTrue(RELATIONAL_GREATER('true', true));
      assertTrue(RELATIONAL_GREATER([ ], true));
      assertTrue(RELATIONAL_GREATER([ true ], true));
      assertTrue(RELATIONAL_GREATER([ false ], true));
      assertTrue(RELATIONAL_GREATER([ null ], true));
      assertTrue(RELATIONAL_GREATER([ 0 ], true));
      assertTrue(RELATIONAL_GREATER({ }, true));
      assertTrue(RELATIONAL_GREATER({ 'a' : null }, true));
      assertTrue(RELATIONAL_GREATER(1, 0));
      assertTrue(RELATIONAL_GREATER(2, 1));
      assertTrue(RELATIONAL_GREATER(100, 1));
      assertTrue(RELATIONAL_GREATER(100, 20));
      assertTrue(RELATIONAL_GREATER(1, -100));
      assertTrue(RELATIONAL_GREATER(-10, -100));
      assertTrue(RELATIONAL_GREATER(-10, -11));
      assertTrue(RELATIONAL_GREATER(1000, 999));
      assertTrue(RELATIONAL_GREATER(1, -1));
      assertTrue(RELATIONAL_GREATER(0, -1));
      assertTrue(RELATIONAL_GREATER(1.01, 1.0));
      assertTrue(RELATIONAL_GREATER(1.2, 1.111));
      assertTrue(RELATIONAL_GREATER(-1.110, -1.111));
      assertTrue(RELATIONAL_GREATER(-1.1109, -1.111));
      assertTrue(RELATIONAL_GREATER('', 0));
      assertTrue(RELATIONAL_GREATER(' ', 0));
      assertTrue(RELATIONAL_GREATER('0', 0));
      assertTrue(RELATIONAL_GREATER('1', 0));
      assertTrue(RELATIONAL_GREATER('-1', 0));
      assertTrue(RELATIONAL_GREATER('true', 0));
      assertTrue(RELATIONAL_GREATER('false', 0));
      assertTrue(RELATIONAL_GREATER('null', 0));
      assertTrue(RELATIONAL_GREATER('', 1));
      assertTrue(RELATIONAL_GREATER(' ', 1));
      assertTrue(RELATIONAL_GREATER('0', 1));
      assertTrue(RELATIONAL_GREATER('1', 1));
      assertTrue(RELATIONAL_GREATER('-1', 1));
      assertTrue(RELATIONAL_GREATER('true', 1));
      assertTrue(RELATIONAL_GREATER('false', 1));
      assertTrue(RELATIONAL_GREATER('null', 1));
      assertTrue(RELATIONAL_GREATER('-1', 0));
      assertTrue(RELATIONAL_GREATER('-100', 0));
      assertTrue(RELATIONAL_GREATER('-1.1', 0));
      assertTrue(RELATIONAL_GREATER('-0.0', 0));
      assertTrue(RELATIONAL_GREATER('-1', 1000));
      assertTrue(RELATIONAL_GREATER('10', 1000));
      assertTrue(RELATIONAL_GREATER('10000', 1000));
      assertTrue(RELATIONAL_GREATER([ ], 0));
      assertTrue(RELATIONAL_GREATER([ 0 ], 0));
      assertTrue(RELATIONAL_GREATER([ ], 10));
      assertTrue(RELATIONAL_GREATER([ ], 100));
      assertTrue(RELATIONAL_GREATER([ 0 ], 100));
      assertTrue(RELATIONAL_GREATER([ 0, 1 ], 100));
      assertTrue(RELATIONAL_GREATER([ 99 ], 100));
      assertTrue(RELATIONAL_GREATER([ 100 ], 100));
      assertTrue(RELATIONAL_GREATER([ 101 ], 100));
      assertTrue(RELATIONAL_GREATER({ }, 100));
      assertTrue(RELATIONAL_GREATER({ 'a' : 0 }, 100));
      assertTrue(RELATIONAL_GREATER({ 'a' : 1 }, 100));
      assertTrue(RELATIONAL_GREATER({ 'a' : 99 }, 100));
      assertTrue(RELATIONAL_GREATER({ 'a' : 100 }, 100));
      assertTrue(RELATIONAL_GREATER({ 'a' : 101 }, 100));
      assertTrue(RELATIONAL_GREATER({ 'a' : 1000 }, 100));
      assertTrue(RELATIONAL_GREATER({ 'a' : false }, 'zz'));
      assertTrue(RELATIONAL_GREATER({ 'a' : 'a' }, 'zz'));
      assertTrue(RELATIONAL_GREATER(' ', ''));
      assertTrue(RELATIONAL_GREATER('a', '0'));
      assertTrue(RELATIONAL_GREATER('a ', 'a'));
      assertTrue(RELATIONAL_GREATER('b', 'a'));
      assertTrue(RELATIONAL_GREATER('a', 'A'));
      assertTrue(RELATIONAL_GREATER('Ab', 'AB'));
      assertTrue(RELATIONAL_GREATER('bbcd', 'abcd'));
      assertTrue(RELATIONAL_GREATER('abda', 'abcd'));
      assertTrue(RELATIONAL_GREATER('abdd', 'abcd'));
      assertTrue(RELATIONAL_GREATER('abcde', 'abcd'));
      assertTrue(RELATIONAL_GREATER('abcde', '0abcde'));
      assertTrue(RELATIONAL_GREATER([ 0 ], [ ]));
      assertTrue(RELATIONAL_GREATER([ 1 ], [ 0 ]));
      assertTrue(RELATIONAL_GREATER([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertTrue(RELATIONAL_GREATER([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertTrue(RELATIONAL_GREATER([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertTrue(RELATIONAL_GREATER([ 1 ], [ 0, 1, 4 ]));
      assertTrue(RELATIONAL_GREATER([ 110 ], [ 15, 99 ]));
      assertTrue(RELATIONAL_GREATER([ 15, 100 ], [ 15, 99 ]));
      assertTrue(RELATIONAL_GREATER([ false ], [ ]));
      assertTrue(RELATIONAL_GREATER([ true ], [ ]));
      assertTrue(RELATIONAL_GREATER([ 0 ], [ ]));
      assertTrue(RELATIONAL_GREATER([ -1 ], [ ]));
      assertTrue(RELATIONAL_GREATER([ '' ], [ ]));
      assertTrue(RELATIONAL_GREATER([ '0' ], [ ]));
      assertTrue(RELATIONAL_GREATER([ 'abcd' ], [ ]));
      assertTrue(RELATIONAL_GREATER([ [ ] ], [ ]));
      assertTrue(RELATIONAL_GREATER([ [ null ] ], [ ]));
      assertTrue(RELATIONAL_GREATER([ { } ], [ ]));
      assertTrue(RELATIONAL_GREATER([ false ], [ null ]));
      assertTrue(RELATIONAL_GREATER([ true ], [ null ]));
      assertTrue(RELATIONAL_GREATER([ 0 ], [ null ]));
      assertTrue(RELATIONAL_GREATER([ [ ] ], [ null ]));
      assertTrue(RELATIONAL_GREATER([ true ], [ false ]));
      assertTrue(RELATIONAL_GREATER([ 0 ], [ false ]));
      assertTrue(RELATIONAL_GREATER([ -1 ], [ false ]));
      assertTrue(RELATIONAL_GREATER([ '' ], [ false ]));
      assertTrue(RELATIONAL_GREATER([ '0' ], [ false ]));
      assertTrue(RELATIONAL_GREATER([ 'abcd' ], [ false ]));
      assertTrue(RELATIONAL_GREATER([ [ ] ], [ false ]));
      assertTrue(RELATIONAL_GREATER([ [ false ] ], [ false ]));
      assertTrue(RELATIONAL_GREATER([ 0 ], [ true ]));
      assertTrue(RELATIONAL_GREATER([ -1 ], [ true ]));
      assertTrue(RELATIONAL_GREATER([ '' ], [ true ]));
      assertTrue(RELATIONAL_GREATER([ '0' ], [ true ]));
      assertTrue(RELATIONAL_GREATER([ 'abcd' ], [ true ]));
      assertTrue(RELATIONAL_GREATER([ [ ] ], [ true ]));
      assertTrue(RELATIONAL_GREATER([ [ false] ], [ true ]));
      assertTrue(RELATIONAL_GREATER([ true ], [ false, false ]));
      assertTrue(RELATIONAL_GREATER([ false, true ], [ false, false ]));
      assertTrue(RELATIONAL_GREATER([ false, 0 ], [ false, false ]));
      assertTrue(RELATIONAL_GREATER([ null, false ], [ null, null ]));
      assertTrue(RELATIONAL_GREATER({ 'a' : 0 }, { }));
      assertTrue(RELATIONAL_GREATER({ 'a' : 2 }, { 'a' : 1 }));
      assertTrue(RELATIONAL_GREATER({ 'A' : 2 }, { 'a' : 1 }));
      assertTrue(RELATIONAL_GREATER({ 'A' : 1 }, { 'a' : 2 }));
      assertTrue(RELATIONAL_GREATER({ 'a' : 1 }, { 'b' : 1 }));
      assertTrue(RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : false }));
      assertTrue(RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : true }));
      assertTrue(RELATIONAL_GREATER({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ ], 'b' : true }));
      assertTrue(RELATIONAL_GREATER({ 'a' : [ 10, 1 ] }, { 'a' : [ 10 ], 'b' : true }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_GREATER function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterFalse: function() {
      assertFalse(RELATIONAL_GREATER(null, false));
      assertFalse(RELATIONAL_GREATER(null, true));
      assertFalse(RELATIONAL_GREATER(null, 0));
      assertFalse(RELATIONAL_GREATER(null, 1));
      assertFalse(RELATIONAL_GREATER(null, -1));
      assertFalse(RELATIONAL_GREATER(null, ''));
      assertFalse(RELATIONAL_GREATER(null, ' '));
      assertFalse(RELATIONAL_GREATER(null, '1'));
      assertFalse(RELATIONAL_GREATER(null, '0'));
      assertFalse(RELATIONAL_GREATER(null, 'abcd'));
      assertFalse(RELATIONAL_GREATER(null, 'null'));
      assertFalse(RELATIONAL_GREATER(null, [ ]));
      assertFalse(RELATIONAL_GREATER(null, [ true ]));
      assertFalse(RELATIONAL_GREATER(null, [ false ]));
      assertFalse(RELATIONAL_GREATER(null, [ null ]));
      assertFalse(RELATIONAL_GREATER(null, [ 0 ]));
      assertFalse(RELATIONAL_GREATER(null, { }));
      assertFalse(RELATIONAL_GREATER(null, { 'a' : null }));
      assertFalse(RELATIONAL_GREATER(false, false));
      assertFalse(RELATIONAL_GREATER(true, true));
      assertFalse(RELATIONAL_GREATER(false, true));
      assertFalse(RELATIONAL_GREATER(false, 0));
      assertFalse(RELATIONAL_GREATER(false, 1));
      assertFalse(RELATIONAL_GREATER(false, -1));
      assertFalse(RELATIONAL_GREATER(false, ''));
      assertFalse(RELATIONAL_GREATER(false, ' '));
      assertFalse(RELATIONAL_GREATER(false, '1'));
      assertFalse(RELATIONAL_GREATER(false, '0'));
      assertFalse(RELATIONAL_GREATER(false, 'abcd'));
      assertFalse(RELATIONAL_GREATER(false, 'true'));
      assertFalse(RELATIONAL_GREATER(false, [ ]));
      assertFalse(RELATIONAL_GREATER(false, [ true ]));
      assertFalse(RELATIONAL_GREATER(false, [ false ]));
      assertFalse(RELATIONAL_GREATER(false, [ null ]));
      assertFalse(RELATIONAL_GREATER(false, [ 0 ]));
      assertFalse(RELATIONAL_GREATER(false, { }));
      assertFalse(RELATIONAL_GREATER(false, { 'a' : null }));
      assertFalse(RELATIONAL_GREATER(true, 0));
      assertFalse(RELATIONAL_GREATER(true, 1));
      assertFalse(RELATIONAL_GREATER(true, -1));
      assertFalse(RELATIONAL_GREATER(true, ''));
      assertFalse(RELATIONAL_GREATER(true, ' '));
      assertFalse(RELATIONAL_GREATER(true, '1'));
      assertFalse(RELATIONAL_GREATER(true, '0'));
      assertFalse(RELATIONAL_GREATER(true, 'abcd'));
      assertFalse(RELATIONAL_GREATER(true, 'true'));
      assertFalse(RELATIONAL_GREATER(true, [ ]));
      assertFalse(RELATIONAL_GREATER(true, [ true ]));
      assertFalse(RELATIONAL_GREATER(true, [ false ]));
      assertFalse(RELATIONAL_GREATER(true, [ null ]));
      assertFalse(RELATIONAL_GREATER(true, [ 0 ]));
      assertFalse(RELATIONAL_GREATER(true, { }));
      assertFalse(RELATIONAL_GREATER(true, { 'a' : null }));
      assertFalse(RELATIONAL_GREATER(0, 0));
      assertFalse(RELATIONAL_GREATER(1, 1));
      assertFalse(RELATIONAL_GREATER(-10, -10));
      assertFalse(RELATIONAL_GREATER(-100, -100));
      assertFalse(RELATIONAL_GREATER(-334.5, -334.5));
      assertFalse(RELATIONAL_GREATER(0, 1));
      assertFalse(RELATIONAL_GREATER(1, 2));
      assertFalse(RELATIONAL_GREATER(1, 100));
      assertFalse(RELATIONAL_GREATER(20, 100));
      assertFalse(RELATIONAL_GREATER(-100, 1));
      assertFalse(RELATIONAL_GREATER(-100, -10));
      assertFalse(RELATIONAL_GREATER(-11, -10));
      assertFalse(RELATIONAL_GREATER(999, 1000));
      assertFalse(RELATIONAL_GREATER(-1, 1));
      assertFalse(RELATIONAL_GREATER(-1, 0));
      assertFalse(RELATIONAL_GREATER(1.0, 1.01));
      assertFalse(RELATIONAL_GREATER(1.111, 1.2));
      assertFalse(RELATIONAL_GREATER(-1.111, -1.110));
      assertFalse(RELATIONAL_GREATER(-1.111, -1.1109));
      assertFalse(RELATIONAL_GREATER(0, ''));
      assertFalse(RELATIONAL_GREATER(0, ' '));
      assertFalse(RELATIONAL_GREATER(0, '0'));
      assertFalse(RELATIONAL_GREATER(0, '1'));
      assertFalse(RELATIONAL_GREATER(0, '-1'));
      assertFalse(RELATIONAL_GREATER(0, 'true'));
      assertFalse(RELATIONAL_GREATER(0, 'false'));
      assertFalse(RELATIONAL_GREATER(0, 'null'));
      assertFalse(RELATIONAL_GREATER(1, ''));
      assertFalse(RELATIONAL_GREATER(1, ' '));
      assertFalse(RELATIONAL_GREATER(1, '0'));
      assertFalse(RELATIONAL_GREATER(1, '1'));
      assertFalse(RELATIONAL_GREATER(1, '-1'));
      assertFalse(RELATIONAL_GREATER(1, 'true'));
      assertFalse(RELATIONAL_GREATER(1, 'false'));
      assertFalse(RELATIONAL_GREATER(1, 'null'));
      assertFalse(RELATIONAL_GREATER(0, '-1'));
      assertFalse(RELATIONAL_GREATER(0, '-100'));
      assertFalse(RELATIONAL_GREATER(0, '-1.1'));
      assertFalse(RELATIONAL_GREATER(0, '-0.0'));
      assertFalse(RELATIONAL_GREATER(1000, '-1'));
      assertFalse(RELATIONAL_GREATER(1000, '10'));
      assertFalse(RELATIONAL_GREATER(1000, '10000'));
      assertFalse(RELATIONAL_GREATER(0, [ ]));
      assertFalse(RELATIONAL_GREATER(0, [ 0 ]));
      assertFalse(RELATIONAL_GREATER(10, [ ]));
      assertFalse(RELATIONAL_GREATER(100, [ ]));
      assertFalse(RELATIONAL_GREATER(100, [ 0 ]));
      assertFalse(RELATIONAL_GREATER(100, [ 0, 1 ]));
      assertFalse(RELATIONAL_GREATER(100, [ 99 ]));
      assertFalse(RELATIONAL_GREATER(100, [ 100 ]));
      assertFalse(RELATIONAL_GREATER(100, [ 101 ]));
      assertFalse(RELATIONAL_GREATER(100, { }));
      assertFalse(RELATIONAL_GREATER(100, { 'a' : 0 }));
      assertFalse(RELATIONAL_GREATER(100, { 'a' : 1 }));
      assertFalse(RELATIONAL_GREATER(100, { 'a' : 99 }));
      assertFalse(RELATIONAL_GREATER(100, { 'a' : 100 }));
      assertFalse(RELATIONAL_GREATER(100, { 'a' : 101 }));
      assertFalse(RELATIONAL_GREATER(100, { 'a' : 1000 }));
      assertFalse(RELATIONAL_GREATER('', ''));
      assertFalse(RELATIONAL_GREATER(' ', ' '));
      assertFalse(RELATIONAL_GREATER('a', 'a'));
      assertFalse(RELATIONAL_GREATER(' a', ' a'));
      assertFalse(RELATIONAL_GREATER('abcd', 'abcd'));
      assertFalse(RELATIONAL_GREATER('', ' '));
      assertFalse(RELATIONAL_GREATER('0', 'a'));
      assertFalse(RELATIONAL_GREATER('a', 'a '));
      assertFalse(RELATIONAL_GREATER('a', 'b'));
      assertFalse(RELATIONAL_GREATER('A', 'a'));
      assertFalse(RELATIONAL_GREATER('AB', 'Ab'));
      assertFalse(RELATIONAL_GREATER('abcd', 'bbcd'));
      assertFalse(RELATIONAL_GREATER('abcd', 'abda'));
      assertFalse(RELATIONAL_GREATER('abcd', 'abdd'));
      assertFalse(RELATIONAL_GREATER('abcd', 'abcde'));
      assertFalse(RELATIONAL_GREATER('0abcd', 'abcde'));
      assertFalse(RELATIONAL_GREATER([ ], [ ]));
      assertFalse(RELATIONAL_GREATER([ 0 ], [ 0 ]));
      assertFalse(RELATIONAL_GREATER([ 1 ], [ 1 ]));
      assertFalse(RELATIONAL_GREATER([ true ], [ true ]));
      assertFalse(RELATIONAL_GREATER([ false ], [ false ]));
      assertFalse(RELATIONAL_GREATER([ [ 0, 1, 2 ] ], [ [ 0, 1, 2 ] ]));
      assertFalse(RELATIONAL_GREATER([ [ 1, [ "true", 0, -99 , false ] ], 4 ], [ [ 1, [ "true", 0, -99, false ] ], 4 ]));
      assertFalse(RELATIONAL_GREATER([ ], [ 0 ]));
      assertFalse(RELATIONAL_GREATER([ 0 ], [ 1 ]));
      assertFalse(RELATIONAL_GREATER([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertFalse(RELATIONAL_GREATER([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertFalse(RELATIONAL_GREATER([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertFalse(RELATIONAL_GREATER([ 0, 1, 4 ], [ 1 ]));
      assertFalse(RELATIONAL_GREATER([ 15, 99 ], [ 110 ]));
      assertFalse(RELATIONAL_GREATER([ 15, 99 ], [ 15, 100 ]));
      assertFalse(RELATIONAL_GREATER([ ], [ null ]));
      assertFalse(RELATIONAL_GREATER([ ], [ false ]));
      assertFalse(RELATIONAL_GREATER([ ], [ true ]));
      assertFalse(RELATIONAL_GREATER([ ], [ 0 ]));
      assertFalse(RELATIONAL_GREATER([ ], [ -1 ]));
      assertFalse(RELATIONAL_GREATER([ ], [ '' ]));
      assertFalse(RELATIONAL_GREATER([ ], [ '0' ]));
      assertFalse(RELATIONAL_GREATER([ ], [ 'abcd' ]));
      assertFalse(RELATIONAL_GREATER([ ], [ [ ] ]));
      assertFalse(RELATIONAL_GREATER([ ], [ [ null ] ]));
      assertFalse(RELATIONAL_GREATER([ ], [ { } ]));
      assertFalse(RELATIONAL_GREATER([ null ], [ false ]));
      assertFalse(RELATIONAL_GREATER([ null ], [ true ]));
      assertFalse(RELATIONAL_GREATER([ null ], [ 0 ]));
      assertFalse(RELATIONAL_GREATER([ null ], [ [ ] ]));
      assertFalse(RELATIONAL_GREATER([ null ], [ ]));
      assertFalse(RELATIONAL_GREATER([ false ], [ true ]));
      assertFalse(RELATIONAL_GREATER([ false ], [ 0 ]));
      assertFalse(RELATIONAL_GREATER([ false ], [ -1 ]));
      assertFalse(RELATIONAL_GREATER([ false ], [ '' ]));
      assertFalse(RELATIONAL_GREATER([ false ], [ '0' ]));
      assertFalse(RELATIONAL_GREATER([ false ], [ 'abcd' ]));
      assertFalse(RELATIONAL_GREATER([ false ], [ [ ] ]));
      assertFalse(RELATIONAL_GREATER([ false ], [ [ false ] ]));
      assertFalse(RELATIONAL_GREATER([ true ], [ 0 ]));
      assertFalse(RELATIONAL_GREATER([ true ], [ -1 ]));
      assertFalse(RELATIONAL_GREATER([ true ], [ '' ]));
      assertFalse(RELATIONAL_GREATER([ true ], [ '0' ]));
      assertFalse(RELATIONAL_GREATER([ true ], [ 'abcd' ]));
      assertFalse(RELATIONAL_GREATER([ true ], [ [ ] ]));
      assertFalse(RELATIONAL_GREATER([ true ], [ [ false ] ]));
      assertFalse(RELATIONAL_GREATER([ false, false ], [ true ]));
      assertFalse(RELATIONAL_GREATER([ false, false ], [ false, true ]));
      assertFalse(RELATIONAL_GREATER([ false, false ], [ false, 0 ]));
      assertFalse(RELATIONAL_GREATER([ null, null ], [ null, false ]));
      assertFalse(RELATIONAL_GREATER({ 'a' : 1, 'b' : 2, 'c': null }, { 'b' : 2, 'a' : 1 }));
      assertFalse(RELATIONAL_GREATER({ 'a' : 1, 'b' : 2, 'c' : null }, { 'a' : 1, 'b' : 2 }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_LESSEQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalLessEqualTrue: function() {
      assertTrue(RELATIONAL_LESSEQUAL(null, false));
      assertTrue(RELATIONAL_LESSEQUAL(null, true));
      assertTrue(RELATIONAL_LESSEQUAL(null, 0));
      assertTrue(RELATIONAL_LESSEQUAL(null, 1));
      assertTrue(RELATIONAL_LESSEQUAL(null, -1));
      assertTrue(RELATIONAL_LESSEQUAL(null, ''));
      assertTrue(RELATIONAL_LESSEQUAL(null, ' '));
      assertTrue(RELATIONAL_LESSEQUAL(null, '1'));
      assertTrue(RELATIONAL_LESSEQUAL(null, '0'));
      assertTrue(RELATIONAL_LESSEQUAL(null, 'abcd'));
      assertTrue(RELATIONAL_LESSEQUAL(null, 'null'));
      assertTrue(RELATIONAL_LESSEQUAL(null, [ ]));
      assertTrue(RELATIONAL_LESSEQUAL(null, [ true ]));
      assertTrue(RELATIONAL_LESSEQUAL(null, [ false ]));
      assertTrue(RELATIONAL_LESSEQUAL(null, [ null ]));
      assertTrue(RELATIONAL_LESSEQUAL(null, [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL(null, { }));
      assertTrue(RELATIONAL_LESSEQUAL(null, { 'a' : null }));
      assertTrue(RELATIONAL_LESSEQUAL(null, null));
      assertTrue(RELATIONAL_LESSEQUAL(false, true));
      assertTrue(RELATIONAL_LESSEQUAL(false, 0));
      assertTrue(RELATIONAL_LESSEQUAL(false, 1));
      assertTrue(RELATIONAL_LESSEQUAL(false, -1));
      assertTrue(RELATIONAL_LESSEQUAL(false, ''));
      assertTrue(RELATIONAL_LESSEQUAL(false, ' '));
      assertTrue(RELATIONAL_LESSEQUAL(false, '1'));
      assertTrue(RELATIONAL_LESSEQUAL(false, '0'));
      assertTrue(RELATIONAL_LESSEQUAL(false, 'abcd'));
      assertTrue(RELATIONAL_LESSEQUAL(false, 'true'));
      assertTrue(RELATIONAL_LESSEQUAL(false, [ ]));
      assertTrue(RELATIONAL_LESSEQUAL(false, [ true ]));
      assertTrue(RELATIONAL_LESSEQUAL(false, [ false ]));
      assertTrue(RELATIONAL_LESSEQUAL(false, [ null ]));
      assertTrue(RELATIONAL_LESSEQUAL(false, [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL(false, { }));
      assertTrue(RELATIONAL_LESSEQUAL(false, { 'a' : null }));
      assertTrue(RELATIONAL_LESSEQUAL(false, false));
      assertTrue(RELATIONAL_LESSEQUAL(true, 0));
      assertTrue(RELATIONAL_LESSEQUAL(true, 1));
      assertTrue(RELATIONAL_LESSEQUAL(true, -1));
      assertTrue(RELATIONAL_LESSEQUAL(true, ''));
      assertTrue(RELATIONAL_LESSEQUAL(true, ' '));
      assertTrue(RELATIONAL_LESSEQUAL(true, '1'));
      assertTrue(RELATIONAL_LESSEQUAL(true, '0'));
      assertTrue(RELATIONAL_LESSEQUAL(true, 'abcd'));
      assertTrue(RELATIONAL_LESSEQUAL(true, 'true'));
      assertTrue(RELATIONAL_LESSEQUAL(true, [ ]));
      assertTrue(RELATIONAL_LESSEQUAL(true, [ true ]));
      assertTrue(RELATIONAL_LESSEQUAL(true, [ false ]));
      assertTrue(RELATIONAL_LESSEQUAL(true, [ null ]));
      assertTrue(RELATIONAL_LESSEQUAL(true, [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL(true, { }));
      assertTrue(RELATIONAL_LESSEQUAL(true, { 'a' : null }));
      assertTrue(RELATIONAL_LESSEQUAL(true, true));
      assertTrue(RELATIONAL_LESSEQUAL(0, 1));
      assertTrue(RELATIONAL_LESSEQUAL(1, 2));
      assertTrue(RELATIONAL_LESSEQUAL(1, 100));
      assertTrue(RELATIONAL_LESSEQUAL(20, 100));
      assertTrue(RELATIONAL_LESSEQUAL(-100, 1));
      assertTrue(RELATIONAL_LESSEQUAL(-100, -10));
      assertTrue(RELATIONAL_LESSEQUAL(-11, -10));
      assertTrue(RELATIONAL_LESSEQUAL(999, 1000));
      assertTrue(RELATIONAL_LESSEQUAL(-1, 1));
      assertTrue(RELATIONAL_LESSEQUAL(-1, 0));
      assertTrue(RELATIONAL_LESSEQUAL(1.0, 1.01));
      assertTrue(RELATIONAL_LESSEQUAL(1.111, 1.2));
      assertTrue(RELATIONAL_LESSEQUAL(-1.111, -1.110));
      assertTrue(RELATIONAL_LESSEQUAL(-1.111, -1.1109));
      assertTrue(RELATIONAL_LESSEQUAL(0, 0));
      assertTrue(RELATIONAL_LESSEQUAL(1, 1));
      assertTrue(RELATIONAL_LESSEQUAL(2, 2));
      assertTrue(RELATIONAL_LESSEQUAL(20, 20));
      assertTrue(RELATIONAL_LESSEQUAL(100, 100));
      assertTrue(RELATIONAL_LESSEQUAL(-100, -100));
      assertTrue(RELATIONAL_LESSEQUAL(-11, -11));
      assertTrue(RELATIONAL_LESSEQUAL(999, 999));
      assertTrue(RELATIONAL_LESSEQUAL(-1, -1));
      assertTrue(RELATIONAL_LESSEQUAL(1.0, 1.0));
      assertTrue(RELATIONAL_LESSEQUAL(1.111, 1.111));
      assertTrue(RELATIONAL_LESSEQUAL(1.2, 1.2));
      assertTrue(RELATIONAL_LESSEQUAL(-1.111, -1.111));
      assertTrue(RELATIONAL_LESSEQUAL(0, ''));
      assertTrue(RELATIONAL_LESSEQUAL(0, ' '));
      assertTrue(RELATIONAL_LESSEQUAL(0, '0'));
      assertTrue(RELATIONAL_LESSEQUAL(0, '1'));
      assertTrue(RELATIONAL_LESSEQUAL(0, '-1'));
      assertTrue(RELATIONAL_LESSEQUAL(0, 'true'));
      assertTrue(RELATIONAL_LESSEQUAL(0, 'false'));
      assertTrue(RELATIONAL_LESSEQUAL(0, 'null'));
      assertTrue(RELATIONAL_LESSEQUAL(1, ''));
      assertTrue(RELATIONAL_LESSEQUAL(1, ' '));
      assertTrue(RELATIONAL_LESSEQUAL(1, '0'));
      assertTrue(RELATIONAL_LESSEQUAL(1, '1'));
      assertTrue(RELATIONAL_LESSEQUAL(1, '-1'));
      assertTrue(RELATIONAL_LESSEQUAL(1, 'true'));
      assertTrue(RELATIONAL_LESSEQUAL(1, 'false'));
      assertTrue(RELATIONAL_LESSEQUAL(1, 'null'));
      assertTrue(RELATIONAL_LESSEQUAL(0, '-1'));
      assertTrue(RELATIONAL_LESSEQUAL(0, '-100'));
      assertTrue(RELATIONAL_LESSEQUAL(0, '-1.1'));
      assertTrue(RELATIONAL_LESSEQUAL(0, '-0.0'));
      assertTrue(RELATIONAL_LESSEQUAL(1000, '-1'));
      assertTrue(RELATIONAL_LESSEQUAL(1000, '10'));
      assertTrue(RELATIONAL_LESSEQUAL(1000, '10000'));
      assertTrue(RELATIONAL_LESSEQUAL(0, [ ]));
      assertTrue(RELATIONAL_LESSEQUAL(0, [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL(10, [ ]));
      assertTrue(RELATIONAL_LESSEQUAL(100, [ ]));
      assertTrue(RELATIONAL_LESSEQUAL(100, [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL(100, [ 0, 1 ]));
      assertTrue(RELATIONAL_LESSEQUAL(100, [ 99 ]));
      assertTrue(RELATIONAL_LESSEQUAL(100, [ 100 ]));
      assertTrue(RELATIONAL_LESSEQUAL(100, [ 101 ]));
      assertTrue(RELATIONAL_LESSEQUAL(100, { }));
      assertTrue(RELATIONAL_LESSEQUAL(100, { 'a' : 0 }));
      assertTrue(RELATIONAL_LESSEQUAL(100, { 'a' : 1 }));
      assertTrue(RELATIONAL_LESSEQUAL(100, { 'a' : 99 }));
      assertTrue(RELATIONAL_LESSEQUAL(100, { 'a' : 100 }));
      assertTrue(RELATIONAL_LESSEQUAL(100, { 'a' : 101 }));
      assertTrue(RELATIONAL_LESSEQUAL(100, { 'a' : 1000 }));
      assertTrue(RELATIONAL_LESSEQUAL('', ' '));
      assertTrue(RELATIONAL_LESSEQUAL('0', 'a'));
      assertTrue(RELATIONAL_LESSEQUAL('a', 'a '));
      assertTrue(RELATIONAL_LESSEQUAL('a', 'b'));
      assertTrue(RELATIONAL_LESSEQUAL('A', 'a'));
      assertTrue(RELATIONAL_LESSEQUAL('AB', 'Ab'));
      assertTrue(RELATIONAL_LESSEQUAL('abcd', 'bbcd'));
      assertTrue(RELATIONAL_LESSEQUAL('abcd', 'abda'));
      assertTrue(RELATIONAL_LESSEQUAL('abcd', 'abdd'));
      assertTrue(RELATIONAL_LESSEQUAL('abcd', 'abcde'));
      assertTrue(RELATIONAL_LESSEQUAL('0abcd', 'abcde'));
      assertTrue(RELATIONAL_LESSEQUAL(' abcd', 'abcd'));
      assertTrue(RELATIONAL_LESSEQUAL('', ''));
      assertTrue(RELATIONAL_LESSEQUAL('0', '0'));
      assertTrue(RELATIONAL_LESSEQUAL('a', 'a'));
      assertTrue(RELATIONAL_LESSEQUAL('A', 'A'));
      assertTrue(RELATIONAL_LESSEQUAL('AB', 'AB'));
      assertTrue(RELATIONAL_LESSEQUAL('Ab', 'Ab'));
      assertTrue(RELATIONAL_LESSEQUAL('abcd', 'abcd'));
      assertTrue(RELATIONAL_LESSEQUAL('0abcd', '0abcd'));
      assertTrue(RELATIONAL_LESSEQUAL(' ', ' '));
      assertTrue(RELATIONAL_LESSEQUAL('  ', '  '));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 0 ], [ 1 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 0, 1, 4 ], [ 1 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 15, 99 ], [ 110 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 15, 99 ], [ 15, 100 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ null ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ false ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ true ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ -1 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ '' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ '0' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ 'abcd' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ [ ] ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ [ null ] ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ { } ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], [ ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 0 ], [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 0, 1, 2 ], [ 0, 1, 2 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 15, 99 ], [ 15, 99 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ null ], [ null ]));
      assertTrue(RELATIONAL_LESSEQUAL([ [ [ null, 1, 9 ], [ 12, "true", false ] ] , 0 ], [ [ [ null, 1, 9 ], [ 12, "true", false ] ] ,0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], [ false ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false, true ], [ false, true ]));
      assertTrue(RELATIONAL_LESSEQUAL([ null ], [ false ]));
      assertTrue(RELATIONAL_LESSEQUAL([ null ], [ true ]));
      assertTrue(RELATIONAL_LESSEQUAL([ null ], [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ null ], [ [ ] ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], [ true ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], [ -1 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], [ '' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], [ '0' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], [ 'abcd' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], [ [ ] ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], [ [ false ] ]));
      assertTrue(RELATIONAL_LESSEQUAL([ true ], [ 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ true ], [ -1 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ true ], [ '' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ true ], [ '0' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ true ], [ 'abcd' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ true ], [ [ ] ]));
      assertTrue(RELATIONAL_LESSEQUAL([ true ], [ [ false ] ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false, false ], [ true ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false, false ], [ false, true ]));
      assertTrue(RELATIONAL_LESSEQUAL([ false, false ], [ false, 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ null, null ], [ null, false ]));
      assertTrue(RELATIONAL_LESSEQUAL([ ], { }));
      assertTrue(RELATIONAL_LESSEQUAL([ ], { 'a' : true }));
      assertTrue(RELATIONAL_LESSEQUAL([ ], { 'a' : null }));
      assertTrue(RELATIONAL_LESSEQUAL([ ], { 'a' : false }));
      assertTrue(RELATIONAL_LESSEQUAL([ '' ], { }));
      assertTrue(RELATIONAL_LESSEQUAL([ 0 ], { }));
      assertTrue(RELATIONAL_LESSEQUAL([ null ], { }));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], { }));
      assertTrue(RELATIONAL_LESSEQUAL([ false ], { 'a' : false }));
      assertTrue(RELATIONAL_LESSEQUAL([ true ], { 'a' : false }));
      assertTrue(RELATIONAL_LESSEQUAL([ 'abcd' ], { 'a' : false }));
      assertTrue(RELATIONAL_LESSEQUAL([ 5 ], { 'a' : false }));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6 ], { 'a' : 2, 'b' : 2 }));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, 7 ], { }));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, false ], [ 5, 6, true ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, 0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, 999 ], [ 5, 6, '' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, 'a' ], [ 5, 6, 'b' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, 'A' ], [ 5, 6, 'a' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, '' ], [ 5, 6, 'a' ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, 9, 9 ], [ 5, 6, [ ] ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, [ ] ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, true ], [ 5, 6, { } ]));
      assertTrue(RELATIONAL_LESSEQUAL([ 5, 6, 9, 9 ], [ 5, 6, { } ]));
      assertTrue(RELATIONAL_LESSEQUAL({ }, { }));
      assertTrue(RELATIONAL_LESSEQUAL({ 'A' : true }, { 'A' : true }));
      assertTrue(RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : false }, { 'a' : true, 'b' : false }));
      assertTrue(RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : false }, { 'b' : false, 'a' : true }));
      assertTrue(RELATIONAL_LESSEQUAL({ 'a' : true, 'b' : { 'c' : 1, 'f' : 2 }, 'x' : 9 }, { 'x' : 9, 'b' : { 'f' : 2, 'c' : 1 }, 'a' : true }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_LESSEQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalLessEqualFalse: function() {
      assertFalse(RELATIONAL_LESSEQUAL(false, null));
      assertFalse(RELATIONAL_LESSEQUAL(true, null));
      assertFalse(RELATIONAL_LESSEQUAL(0, null));
      assertFalse(RELATIONAL_LESSEQUAL(1, null));
      assertFalse(RELATIONAL_LESSEQUAL(-1, null));
      assertFalse(RELATIONAL_LESSEQUAL('', null));
      assertFalse(RELATIONAL_LESSEQUAL(' ', null));
      assertFalse(RELATIONAL_LESSEQUAL('1', null));
      assertFalse(RELATIONAL_LESSEQUAL('0', null));
      assertFalse(RELATIONAL_LESSEQUAL('abcd', null));
      assertFalse(RELATIONAL_LESSEQUAL('null', null));
      assertFalse(RELATIONAL_LESSEQUAL([ ], null));
      assertFalse(RELATIONAL_LESSEQUAL([ true ], null));
      assertFalse(RELATIONAL_LESSEQUAL([ false ], null));
      assertFalse(RELATIONAL_LESSEQUAL([ null ], null));
      assertFalse(RELATIONAL_LESSEQUAL([ 0 ], null));
      assertFalse(RELATIONAL_LESSEQUAL({ }, null));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : null }, null));
      assertFalse(RELATIONAL_LESSEQUAL(true, false));
      assertFalse(RELATIONAL_LESSEQUAL(0, false));
      assertFalse(RELATIONAL_LESSEQUAL(1, false));
      assertFalse(RELATIONAL_LESSEQUAL(-1, false));
      assertFalse(RELATIONAL_LESSEQUAL('', false));
      assertFalse(RELATIONAL_LESSEQUAL(' ', false));
      assertFalse(RELATIONAL_LESSEQUAL('1', false));
      assertFalse(RELATIONAL_LESSEQUAL('0', false));
      assertFalse(RELATIONAL_LESSEQUAL('abcd', false));
      assertFalse(RELATIONAL_LESSEQUAL('true', false));
      assertFalse(RELATIONAL_LESSEQUAL([ ], false));
      assertFalse(RELATIONAL_LESSEQUAL([ true ], false));
      assertFalse(RELATIONAL_LESSEQUAL([ false ], false));
      assertFalse(RELATIONAL_LESSEQUAL([ null ], false));
      assertFalse(RELATIONAL_LESSEQUAL([ 0 ], false));
      assertFalse(RELATIONAL_LESSEQUAL({ }, false));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : null }, false));
      assertFalse(RELATIONAL_LESSEQUAL(0, true));
      assertFalse(RELATIONAL_LESSEQUAL(1, true));
      assertFalse(RELATIONAL_LESSEQUAL(-1, true));
      assertFalse(RELATIONAL_LESSEQUAL('', true));
      assertFalse(RELATIONAL_LESSEQUAL(' ', true));
      assertFalse(RELATIONAL_LESSEQUAL('1', true));
      assertFalse(RELATIONAL_LESSEQUAL('0', true));
      assertFalse(RELATIONAL_LESSEQUAL('abcd', true));
      assertFalse(RELATIONAL_LESSEQUAL('true', true));
      assertFalse(RELATIONAL_LESSEQUAL([ ], true));
      assertFalse(RELATIONAL_LESSEQUAL([ true ], true));
      assertFalse(RELATIONAL_LESSEQUAL([ false ], true));
      assertFalse(RELATIONAL_LESSEQUAL([ null ], true));
      assertFalse(RELATIONAL_LESSEQUAL([ 0 ], true));
      assertFalse(RELATIONAL_LESSEQUAL({ }, true));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : null }, true));
      assertFalse(RELATIONAL_LESSEQUAL(1, 0));
      assertFalse(RELATIONAL_LESSEQUAL(2, 1));
      assertFalse(RELATIONAL_LESSEQUAL(100, 1));
      assertFalse(RELATIONAL_LESSEQUAL(100, 20));
      assertFalse(RELATIONAL_LESSEQUAL(1, -100));
      assertFalse(RELATIONAL_LESSEQUAL(-10, -100));
      assertFalse(RELATIONAL_LESSEQUAL(-10, -11));
      assertFalse(RELATIONAL_LESSEQUAL(1000, 999));
      assertFalse(RELATIONAL_LESSEQUAL(1, -1));
      assertFalse(RELATIONAL_LESSEQUAL(0, -1));
      assertFalse(RELATIONAL_LESSEQUAL(1.01, 1.0));
      assertFalse(RELATIONAL_LESSEQUAL(1.2, 1.111));
      assertFalse(RELATIONAL_LESSEQUAL(-1.110, -1.111));
      assertFalse(RELATIONAL_LESSEQUAL(-1.1109, -1.111));
      assertFalse(RELATIONAL_LESSEQUAL('', 0));
      assertFalse(RELATIONAL_LESSEQUAL(' ', 0));
      assertFalse(RELATIONAL_LESSEQUAL('0', 0));
      assertFalse(RELATIONAL_LESSEQUAL('1', 0));
      assertFalse(RELATIONAL_LESSEQUAL('-1', 0));
      assertFalse(RELATIONAL_LESSEQUAL('true', 0));
      assertFalse(RELATIONAL_LESSEQUAL('false', 0));
      assertFalse(RELATIONAL_LESSEQUAL('null', 0));
      assertFalse(RELATIONAL_LESSEQUAL('', 1));
      assertFalse(RELATIONAL_LESSEQUAL(' ', 1));
      assertFalse(RELATIONAL_LESSEQUAL('0', 1));
      assertFalse(RELATIONAL_LESSEQUAL('1', 1));
      assertFalse(RELATIONAL_LESSEQUAL('-1', 1));
      assertFalse(RELATIONAL_LESSEQUAL('true', 1));
      assertFalse(RELATIONAL_LESSEQUAL('false', 1));
      assertFalse(RELATIONAL_LESSEQUAL('null', 1));
      assertFalse(RELATIONAL_LESSEQUAL('-1', 0));
      assertFalse(RELATIONAL_LESSEQUAL('-100', 0));
      assertFalse(RELATIONAL_LESSEQUAL('-1.1', 0));
      assertFalse(RELATIONAL_LESSEQUAL('-0.0', 0));
      assertFalse(RELATIONAL_LESSEQUAL('-1', 1000));
      assertFalse(RELATIONAL_LESSEQUAL('10', 1000));
      assertFalse(RELATIONAL_LESSEQUAL('10000', 1000));
      assertFalse(RELATIONAL_LESSEQUAL([ ], 0));
      assertFalse(RELATIONAL_LESSEQUAL([ 0 ], 0));
      assertFalse(RELATIONAL_LESSEQUAL([ ], 10));
      assertFalse(RELATIONAL_LESSEQUAL([ ], 100));
      assertFalse(RELATIONAL_LESSEQUAL([ 0 ], 100));
      assertFalse(RELATIONAL_LESSEQUAL([ 0, 1 ], 100));
      assertFalse(RELATIONAL_LESSEQUAL([ 99 ], 100));
      assertFalse(RELATIONAL_LESSEQUAL([ 100 ], 100));
      assertFalse(RELATIONAL_LESSEQUAL([ 101 ], 100));
      assertFalse(RELATIONAL_LESSEQUAL({ }, 100));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : 0 }, 100));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : 1 }, 100));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : 99 }, 100));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : 100 }, 100));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : 101 }, 100));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : 1000 }, 100));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : false }, 'zz'));
      assertFalse(RELATIONAL_LESSEQUAL({ 'a' : 'a' }, 'zz'));
      assertFalse(RELATIONAL_LESSEQUAL(' ', ''));
      assertFalse(RELATIONAL_LESSEQUAL('a', '0'));
      assertFalse(RELATIONAL_LESSEQUAL('a ', 'a'));
      assertFalse(RELATIONAL_LESSEQUAL('b', 'a'));
      assertFalse(RELATIONAL_LESSEQUAL('a', 'A'));
      assertFalse(RELATIONAL_LESSEQUAL('Ab', 'AB'));
      assertFalse(RELATIONAL_LESSEQUAL('bbcd', 'abcd'));
      assertFalse(RELATIONAL_LESSEQUAL('abda', 'abcd'));
      assertFalse(RELATIONAL_LESSEQUAL('abdd', 'abcd'));
      assertFalse(RELATIONAL_LESSEQUAL('abcde', 'abcd'));
      assertFalse(RELATIONAL_LESSEQUAL('abcde', '0abcde'));
      assertFalse(RELATIONAL_LESSEQUAL([ 0 ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 1 ], [ 0 ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 1 ], [ 0, 1, 4 ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 110 ], [ 15, 99 ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 15, 100 ], [ 15, 99 ]));
      assertFalse(RELATIONAL_LESSEQUAL([ false ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ true ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 0 ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ -1 ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ '' ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ '0' ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 'abcd' ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ [ ] ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ [ null ] ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ { } ], [ ]));
      assertFalse(RELATIONAL_LESSEQUAL([ false ], [ null ]));
      assertFalse(RELATIONAL_LESSEQUAL([ true ], [ null ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 0 ], [ null ]));
      assertFalse(RELATIONAL_LESSEQUAL([ [ ] ], [ null ]));
      assertFalse(RELATIONAL_LESSEQUAL([ true ], [ false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 0 ], [ false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ -1 ], [ false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ '' ], [ false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ '0' ], [ false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 'abcd' ], [ false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ [ ] ], [ false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ [ false ] ], [ false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 0 ], [ true ]));
      assertFalse(RELATIONAL_LESSEQUAL([ -1 ], [ true ]));
      assertFalse(RELATIONAL_LESSEQUAL([ '' ], [ true ]));
      assertFalse(RELATIONAL_LESSEQUAL([ '0' ], [ true ]));
      assertFalse(RELATIONAL_LESSEQUAL([ 'abcd' ], [ true ]));
      assertFalse(RELATIONAL_LESSEQUAL([ [ ] ], [ true ]));
      assertFalse(RELATIONAL_LESSEQUAL([ [ false] ], [ true ]));
      assertFalse(RELATIONAL_LESSEQUAL([ true ], [ false, false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ false, true ], [ false, false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ false, 0 ], [ false, false ]));
      assertFalse(RELATIONAL_LESSEQUAL([ null, false ], [ null, null ]));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_GREATEREQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterEqualTrue: function() {
      assertTrue(RELATIONAL_GREATEREQUAL(false, null));
      assertTrue(RELATIONAL_GREATEREQUAL(true, null));
      assertTrue(RELATIONAL_GREATEREQUAL(0, null));
      assertTrue(RELATIONAL_GREATEREQUAL(1, null));
      assertTrue(RELATIONAL_GREATEREQUAL(-1, null));
      assertTrue(RELATIONAL_GREATEREQUAL('', null));
      assertTrue(RELATIONAL_GREATEREQUAL(' ', null));
      assertTrue(RELATIONAL_GREATEREQUAL('1', null));
      assertTrue(RELATIONAL_GREATEREQUAL('0', null));
      assertTrue(RELATIONAL_GREATEREQUAL('abcd', null));
      assertTrue(RELATIONAL_GREATEREQUAL('null', null));
      assertTrue(RELATIONAL_GREATEREQUAL([ ], null));
      assertTrue(RELATIONAL_GREATEREQUAL([ true ], null));
      assertTrue(RELATIONAL_GREATEREQUAL([ false ], null));
      assertTrue(RELATIONAL_GREATEREQUAL([ null ], null));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], null));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, null));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : null }, null));
      assertTrue(RELATIONAL_GREATEREQUAL(true, false));
      assertTrue(RELATIONAL_GREATEREQUAL(0, false));
      assertTrue(RELATIONAL_GREATEREQUAL(1, false));
      assertTrue(RELATIONAL_GREATEREQUAL(-1, false));
      assertTrue(RELATIONAL_GREATEREQUAL('', false));
      assertTrue(RELATIONAL_GREATEREQUAL(' ', false));
      assertTrue(RELATIONAL_GREATEREQUAL('1', false));
      assertTrue(RELATIONAL_GREATEREQUAL('0', false));
      assertTrue(RELATIONAL_GREATEREQUAL('abcd', false));
      assertTrue(RELATIONAL_GREATEREQUAL('true', false));
      assertTrue(RELATIONAL_GREATEREQUAL([ ], false));
      assertTrue(RELATIONAL_GREATEREQUAL([ true ], false));
      assertTrue(RELATIONAL_GREATEREQUAL([ false ], false));
      assertTrue(RELATIONAL_GREATEREQUAL([ null ], false));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], false));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, false));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : null }, false));
      assertTrue(RELATIONAL_GREATEREQUAL(0, true));
      assertTrue(RELATIONAL_GREATEREQUAL(1, true));
      assertTrue(RELATIONAL_GREATEREQUAL(-1, true));
      assertTrue(RELATIONAL_GREATEREQUAL('', true));
      assertTrue(RELATIONAL_GREATEREQUAL(' ', true));
      assertTrue(RELATIONAL_GREATEREQUAL('1', true));
      assertTrue(RELATIONAL_GREATEREQUAL('0', true));
      assertTrue(RELATIONAL_GREATEREQUAL('abcd', true));
      assertTrue(RELATIONAL_GREATEREQUAL('true', true));
      assertTrue(RELATIONAL_GREATEREQUAL([ ], true));
      assertTrue(RELATIONAL_GREATEREQUAL([ true ], true));
      assertTrue(RELATIONAL_GREATEREQUAL([ false ], true));
      assertTrue(RELATIONAL_GREATEREQUAL([ null ], true));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], true));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, true));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : null }, true));
      assertTrue(RELATIONAL_GREATEREQUAL(1, 0));
      assertTrue(RELATIONAL_GREATEREQUAL(2, 1));
      assertTrue(RELATIONAL_GREATEREQUAL(100, 1));
      assertTrue(RELATIONAL_GREATEREQUAL(100, 20));
      assertTrue(RELATIONAL_GREATEREQUAL(1, -100));
      assertTrue(RELATIONAL_GREATEREQUAL(-10, -100));
      assertTrue(RELATIONAL_GREATEREQUAL(-10, -11));
      assertTrue(RELATIONAL_GREATEREQUAL(1000, 999));
      assertTrue(RELATIONAL_GREATEREQUAL(1, -1));
      assertTrue(RELATIONAL_GREATEREQUAL(0, -1));
      assertTrue(RELATIONAL_GREATEREQUAL(1.01, 1.0));
      assertTrue(RELATIONAL_GREATEREQUAL(1.2, 1.111));
      assertTrue(RELATIONAL_GREATEREQUAL(-1.110, -1.111));
      assertTrue(RELATIONAL_GREATEREQUAL(-1.1109, -1.111));
      assertTrue(RELATIONAL_GREATEREQUAL('', 0));
      assertTrue(RELATIONAL_GREATEREQUAL(' ', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('0', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('1', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('-1', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('true', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('false', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('null', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('', 1));
      assertTrue(RELATIONAL_GREATEREQUAL(' ', 1));
      assertTrue(RELATIONAL_GREATEREQUAL('0', 1));
      assertTrue(RELATIONAL_GREATEREQUAL('1', 1));
      assertTrue(RELATIONAL_GREATEREQUAL('-1', 1));
      assertTrue(RELATIONAL_GREATEREQUAL('true', 1));
      assertTrue(RELATIONAL_GREATEREQUAL('false', 1));
      assertTrue(RELATIONAL_GREATEREQUAL('null', 1));
      assertTrue(RELATIONAL_GREATEREQUAL('-1', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('-100', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('-1.1', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('-0.0', 0));
      assertTrue(RELATIONAL_GREATEREQUAL('-1', 1000));
      assertTrue(RELATIONAL_GREATEREQUAL('10', 1000));
      assertTrue(RELATIONAL_GREATEREQUAL('10000', 1000));
      assertTrue(RELATIONAL_GREATEREQUAL([ ], 0));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], 0));
      assertTrue(RELATIONAL_GREATEREQUAL([ ], 10));
      assertTrue(RELATIONAL_GREATEREQUAL([ ], 100));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], 100));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0, 1 ], 100));
      assertTrue(RELATIONAL_GREATEREQUAL([ 99 ], 100));
      assertTrue(RELATIONAL_GREATEREQUAL([ 100 ], 100));
      assertTrue(RELATIONAL_GREATEREQUAL([ 101 ], 100));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, 100));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 0 }, 100));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 1 }, 100));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 99 }, 100));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 100 }, 100));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 101 }, 100));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 1000 }, 100));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : false }, 'zz'));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 'a' }, 'zz'));
      assertTrue(RELATIONAL_GREATEREQUAL(' ', ''));
      assertTrue(RELATIONAL_GREATEREQUAL('a', '0'));
      assertTrue(RELATIONAL_GREATEREQUAL('a ', 'a'));
      assertTrue(RELATIONAL_GREATEREQUAL('b', 'a'));
      assertTrue(RELATIONAL_GREATEREQUAL('a', 'A'));
      assertTrue(RELATIONAL_GREATEREQUAL('Ab', 'AB'));
      assertTrue(RELATIONAL_GREATEREQUAL('bbcd', 'abcd'));
      assertTrue(RELATIONAL_GREATEREQUAL('abda', 'abcd'));
      assertTrue(RELATIONAL_GREATEREQUAL('abdd', 'abcd'));
      assertTrue(RELATIONAL_GREATEREQUAL('abcde', 'abcd'));
      assertTrue(RELATIONAL_GREATEREQUAL('abcde', '0abcde'));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 1 ], [ 0 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0, 1, 2, 3 ], [ 0, 1, 2 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0, 1, 3 ], [ 0, 1, 2 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 1, 0, 0 ], [ 0, 1, 4 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 1 ], [ 0, 1, 4 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 110 ], [ 15, 99 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 15, 100 ], [ 15, 99 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ null ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ false ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ true ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ -1 ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ '' ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ '0' ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 'abcd' ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ [ ] ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ [ null ] ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ { } ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ false ], [ null ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ true ], [ null ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], [ null ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ [ ] ], [ null ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ true ], [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ -1 ], [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ '' ], [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ '0' ], [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 'abcd' ], [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ [ ] ], [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ [ false ] ], [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], [ true ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ -1 ], [ true ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ '' ], [ true ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ '0' ], [ true ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 'abcd' ], [ true ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ [ ] ], [ true ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ [ false] ], [ true ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ true ], [ false, false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ false, true ], [ false, false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ false, 0 ], [ false, false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ null, false ], [ null, null ]));
      assertTrue(RELATIONAL_GREATEREQUAL(null, null));
      assertTrue(RELATIONAL_GREATEREQUAL(false, false));
      assertTrue(RELATIONAL_GREATEREQUAL(true, true));
      assertTrue(RELATIONAL_GREATEREQUAL(0, 0));
      assertTrue(RELATIONAL_GREATEREQUAL(1, 1));
      assertTrue(RELATIONAL_GREATEREQUAL(2, 2));
      assertTrue(RELATIONAL_GREATEREQUAL(20, 20));
      assertTrue(RELATIONAL_GREATEREQUAL(100, 100));
      assertTrue(RELATIONAL_GREATEREQUAL(-100, -100));
      assertTrue(RELATIONAL_GREATEREQUAL(-11, -11));
      assertTrue(RELATIONAL_GREATEREQUAL(999, 999));
      assertTrue(RELATIONAL_GREATEREQUAL(-1, -1));
      assertTrue(RELATIONAL_GREATEREQUAL(1.0, 1.0));
      assertTrue(RELATIONAL_GREATEREQUAL(1.111, 1.111));
      assertTrue(RELATIONAL_GREATEREQUAL(1.2, 1.2));
      assertTrue(RELATIONAL_GREATEREQUAL(-1.111, -1.111));
      assertTrue(RELATIONAL_GREATEREQUAL('', ''));
      assertTrue(RELATIONAL_GREATEREQUAL('0', '0'));
      assertTrue(RELATIONAL_GREATEREQUAL('a', 'a'));
      assertTrue(RELATIONAL_GREATEREQUAL('A', 'A'));
      assertTrue(RELATIONAL_GREATEREQUAL('AB', 'AB'));
      assertTrue(RELATIONAL_GREATEREQUAL('Ab', 'Ab'));
      assertTrue(RELATIONAL_GREATEREQUAL('abcd', 'abcd'));
      assertTrue(RELATIONAL_GREATEREQUAL('0abcd', '0abcd'));
      assertTrue(RELATIONAL_GREATEREQUAL(' ', ' '));
      assertTrue(RELATIONAL_GREATEREQUAL('  ', '  '));
      assertTrue(RELATIONAL_GREATEREQUAL([ ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0 ], [ 0 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 2 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 15, 99 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ null ], [ null ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ [ [ null, 1, 9 ], [ 12, "true", false ] ] , 0 ], [ [ [ null, 1, 9 ], [ 12, "true", false ] ] ,0 ]));
      assertTrue(RELATIONAL_LESSEQUAL([ null ], [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ ], [ null ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ false ], [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ false, true ], [ false, true ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : true }, [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : null }, [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : false }, [ ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, [ '' ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, [ 0 ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, [ null ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : false }, [ false ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : false }, [ true ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : false }, [ 'abcd' ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : false }, [ 5 ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 2, 'b' : 2 }, [ 5, 6 ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, [ 5, 6, 7 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 5, 6, true ], [ 5, 6, false ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 5, 6, 0 ], [ 5, 6, true ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 5, 6, '' ], [ 5, 6, 999 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 5, 6, 'b' ], [ 5, 6, 'a' ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 5, 6, 'a' ], [ 5, 6, 'A' ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 5, 6, 'a' ], [ 5, 6, '' ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 5, 6, [ ] ], [ 5, 6, 9, 9 ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 5, 6, [ ] ], [ 5, 6, true ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 5, 6, { } ], [ 5, 6, true ]));
      assertTrue(RELATIONAL_GREATEREQUAL([ 5, 6, { } ], [ 5, 6, 9, 9 ]));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 0 }, { }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 2 }, { 'a' : 1 }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'A' : 2 }, { 'a' : 1 }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'A' : 1 }, { 'a' : 2 }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 1, 'b' : 2, 'c' : null }, { 'a' : 1, 'b' : 2 }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 1 }, { 'b' : 1 }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : 1, 'b' : 2, 'c': null }, { 'b' : 2, 'a' : 1 }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : false }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ 9 ], 'b' : true }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : [ 10 ], 'b' : true }, { 'a' : [ ], 'b' : true }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : [ 10, 1 ] }, { 'a' : [ 10 ], 'b' : true }));
      assertTrue(RELATIONAL_GREATEREQUAL({ }, { }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'A' : true }, { 'A' : true }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : false }, { 'a' : true, 'b' : false }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : false }, { 'b' : false, 'a' : true }));
      assertTrue(RELATIONAL_GREATEREQUAL({ 'a' : true, 'b' : { 'c' : 1, 'f' : 2 }, 'x' : 9 }, { 'x' : 9, 'b' : { 'f' : 2, 'c' : 1 }, 'a' : true }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_GREATEREQUAL function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalGreaterEqualFalse: function() {
      assertFalse(RELATIONAL_GREATEREQUAL(null, false));
      assertFalse(RELATIONAL_GREATEREQUAL(null, true));
      assertFalse(RELATIONAL_GREATEREQUAL(null, 0));
      assertFalse(RELATIONAL_GREATEREQUAL(null, 1));
      assertFalse(RELATIONAL_GREATEREQUAL(null, -1));
      assertFalse(RELATIONAL_GREATEREQUAL(null, ''));
      assertFalse(RELATIONAL_GREATEREQUAL(null, ' '));
      assertFalse(RELATIONAL_GREATEREQUAL(null, '1'));
      assertFalse(RELATIONAL_GREATEREQUAL(null, '0'));
      assertFalse(RELATIONAL_GREATEREQUAL(null, 'abcd'));
      assertFalse(RELATIONAL_GREATEREQUAL(null, 'null'));
      assertFalse(RELATIONAL_GREATEREQUAL(null, [ ]));
      assertFalse(RELATIONAL_GREATEREQUAL(null, [ true ]));
      assertFalse(RELATIONAL_GREATEREQUAL(null, [ false ]));
      assertFalse(RELATIONAL_GREATEREQUAL(null, [ null ]));
      assertFalse(RELATIONAL_GREATEREQUAL(null, [ 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL(null, { }));
      assertFalse(RELATIONAL_GREATEREQUAL(null, { 'a' : null }));
      assertFalse(RELATIONAL_GREATEREQUAL(false, true));
      assertFalse(RELATIONAL_GREATEREQUAL(false, 0));
      assertFalse(RELATIONAL_GREATEREQUAL(false, 1));
      assertFalse(RELATIONAL_GREATEREQUAL(false, -1));
      assertFalse(RELATIONAL_GREATEREQUAL(false, ''));
      assertFalse(RELATIONAL_GREATEREQUAL(false, ' '));
      assertFalse(RELATIONAL_GREATEREQUAL(false, '1'));
      assertFalse(RELATIONAL_GREATEREQUAL(false, '0'));
      assertFalse(RELATIONAL_GREATEREQUAL(false, 'abcd'));
      assertFalse(RELATIONAL_GREATEREQUAL(false, 'true'));
      assertFalse(RELATIONAL_GREATEREQUAL(false, [ ]));
      assertFalse(RELATIONAL_GREATEREQUAL(false, [ true ]));
      assertFalse(RELATIONAL_GREATEREQUAL(false, [ false ]));
      assertFalse(RELATIONAL_GREATEREQUAL(false, [ null ]));
      assertFalse(RELATIONAL_GREATEREQUAL(false, [ 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL(false, { }));
      assertFalse(RELATIONAL_GREATEREQUAL(false, { 'a' : null }));
      assertFalse(RELATIONAL_GREATEREQUAL(true, 0));
      assertFalse(RELATIONAL_GREATEREQUAL(true, 1));
      assertFalse(RELATIONAL_GREATEREQUAL(true, -1));
      assertFalse(RELATIONAL_GREATEREQUAL(true, ''));
      assertFalse(RELATIONAL_GREATEREQUAL(true, ' '));
      assertFalse(RELATIONAL_GREATEREQUAL(true, '1'));
      assertFalse(RELATIONAL_GREATEREQUAL(true, '0'));
      assertFalse(RELATIONAL_GREATEREQUAL(true, 'abcd'));
      assertFalse(RELATIONAL_GREATEREQUAL(true, 'true'));
      assertFalse(RELATIONAL_GREATEREQUAL(true, [ ]));
      assertFalse(RELATIONAL_GREATEREQUAL(true, [ true ]));
      assertFalse(RELATIONAL_GREATEREQUAL(true, [ false ]));
      assertFalse(RELATIONAL_GREATEREQUAL(true, [ null ]));
      assertFalse(RELATIONAL_GREATEREQUAL(true, [ 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL(true, { }));
      assertFalse(RELATIONAL_GREATEREQUAL(true, { 'a' : null }));
      assertFalse(RELATIONAL_GREATEREQUAL(0, 1));
      assertFalse(RELATIONAL_GREATEREQUAL(1, 2));
      assertFalse(RELATIONAL_GREATEREQUAL(1, 100));
      assertFalse(RELATIONAL_GREATEREQUAL(20, 100));
      assertFalse(RELATIONAL_GREATEREQUAL(-100, 1));
      assertFalse(RELATIONAL_GREATEREQUAL(-100, -10));
      assertFalse(RELATIONAL_GREATEREQUAL(-11, -10));
      assertFalse(RELATIONAL_GREATEREQUAL(999, 1000));
      assertFalse(RELATIONAL_GREATEREQUAL(-1, 1));
      assertFalse(RELATIONAL_GREATEREQUAL(-1, 0));
      assertFalse(RELATIONAL_GREATEREQUAL(1.0, 1.01));
      assertFalse(RELATIONAL_GREATEREQUAL(1.111, 1.2));
      assertFalse(RELATIONAL_GREATEREQUAL(-1.111, -1.110));
      assertFalse(RELATIONAL_GREATEREQUAL(-1.111, -1.1109));
      assertFalse(RELATIONAL_GREATEREQUAL(0, ''));
      assertFalse(RELATIONAL_GREATEREQUAL(0, ' '));
      assertFalse(RELATIONAL_GREATEREQUAL(0, '0'));
      assertFalse(RELATIONAL_GREATEREQUAL(0, '1'));
      assertFalse(RELATIONAL_GREATEREQUAL(0, '-1'));
      assertFalse(RELATIONAL_GREATEREQUAL(0, 'true'));
      assertFalse(RELATIONAL_GREATEREQUAL(0, 'false'));
      assertFalse(RELATIONAL_GREATEREQUAL(0, 'null'));
      assertFalse(RELATIONAL_GREATEREQUAL(1, ''));
      assertFalse(RELATIONAL_GREATEREQUAL(1, ' '));
      assertFalse(RELATIONAL_GREATEREQUAL(1, '0'));
      assertFalse(RELATIONAL_GREATEREQUAL(1, '1'));
      assertFalse(RELATIONAL_GREATEREQUAL(1, '-1'));
      assertFalse(RELATIONAL_GREATEREQUAL(1, 'true'));
      assertFalse(RELATIONAL_GREATEREQUAL(1, 'false'));
      assertFalse(RELATIONAL_GREATEREQUAL(1, 'null'));
      assertFalse(RELATIONAL_GREATEREQUAL(0, '-1'));
      assertFalse(RELATIONAL_GREATEREQUAL(0, '-100'));
      assertFalse(RELATIONAL_GREATEREQUAL(0, '-1.1'));
      assertFalse(RELATIONAL_GREATEREQUAL(0, '-0.0'));
      assertFalse(RELATIONAL_GREATEREQUAL(1000, '-1'));
      assertFalse(RELATIONAL_GREATEREQUAL(1000, '10'));
      assertFalse(RELATIONAL_GREATEREQUAL(1000, '10000'));
      assertFalse(RELATIONAL_GREATEREQUAL(0, [ ]));
      assertFalse(RELATIONAL_GREATEREQUAL(0, [ 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL(10, [ ]));
      assertFalse(RELATIONAL_GREATEREQUAL(100, [ ]));
      assertFalse(RELATIONAL_GREATEREQUAL(100, [ 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL(100, [ 0, 1 ]));
      assertFalse(RELATIONAL_GREATEREQUAL(100, [ 99 ]));
      assertFalse(RELATIONAL_GREATEREQUAL(100, [ 100 ]));
      assertFalse(RELATIONAL_GREATEREQUAL(100, [ 101 ]));
      assertFalse(RELATIONAL_GREATEREQUAL(100, { }));
      assertFalse(RELATIONAL_GREATEREQUAL(100, { 'a' : 0 }));
      assertFalse(RELATIONAL_GREATEREQUAL(100, { 'a' : 1 }));
      assertFalse(RELATIONAL_GREATEREQUAL(100, { 'a' : 99 }));
      assertFalse(RELATIONAL_GREATEREQUAL(100, { 'a' : 100 }));
      assertFalse(RELATIONAL_GREATEREQUAL(100, { 'a' : 101 }));
      assertFalse(RELATIONAL_GREATEREQUAL(100, { 'a' : 1000 }));
      assertFalse(RELATIONAL_GREATEREQUAL('', ' '));
      assertFalse(RELATIONAL_GREATEREQUAL('0', 'a'));
      assertFalse(RELATIONAL_GREATEREQUAL('a', 'a '));
      assertFalse(RELATIONAL_GREATEREQUAL('a', 'b'));
      assertFalse(RELATIONAL_GREATEREQUAL('A', 'a'));
      assertFalse(RELATIONAL_GREATEREQUAL('AB', 'Ab'));
      assertFalse(RELATIONAL_GREATEREQUAL('abcd', 'bbcd'));
      assertFalse(RELATIONAL_GREATEREQUAL('abcd', 'abda'));
      assertFalse(RELATIONAL_GREATEREQUAL('abcd', 'abdd'));
      assertFalse(RELATIONAL_GREATEREQUAL('abcd', 'abcde'));
      assertFalse(RELATIONAL_GREATEREQUAL('0abcd', 'abcde'));
      assertFalse(RELATIONAL_GREATEREQUAL(' abcd', 'abcd'));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ 0 ], [ 1 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 2, 3 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ 0, 1, 2 ], [ 0, 1, 3 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ 0, 1, 4 ], [ 1, 0, 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ 0, 1, 4 ], [ 1 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 110 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ 15, 99 ], [ 15, 100 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ false ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ true ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ -1 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ '' ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ '0' ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ 'abcd' ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ [ ] ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ [ null ] ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ ], [ { } ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ null ], [ false ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ null ], [ true ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ null ], [ 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ null ], [ [ ] ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false ], [ true ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false ], [ 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false ], [ -1 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false ], [ '' ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false ], [ '0' ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false ], [ 'abcd' ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false ], [ [ ] ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false ], [ [ false ] ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ true ], [ 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ true ], [ -1 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ true ], [ '' ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ true ], [ '0' ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ true ], [ 'abcd' ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ true ], [ [ ] ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ true ], [ [ false ] ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false, false ], [ true ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false, false ], [ false, true ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ false, false ], [ false, 0 ]));
      assertFalse(RELATIONAL_GREATEREQUAL([ null, null ], [ null, false ]));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_IN function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalInUndefined: function() {
      assertFalse(RELATIONAL_IN(null, null));
      assertFalse(RELATIONAL_IN(null, false));
      assertFalse(RELATIONAL_IN(null, true));
      assertFalse(RELATIONAL_IN(null, 0));
      assertFalse(RELATIONAL_IN(null, 1));
      assertFalse(RELATIONAL_IN(null, ''));
      assertFalse(RELATIONAL_IN(null, '1'));
      assertFalse(RELATIONAL_IN(null, 'a'));
      assertFalse(RELATIONAL_IN(null, { }));
      assertFalse(RELATIONAL_IN(null, { 'A' : true }));
      assertFalse(RELATIONAL_IN(false, null));
      assertFalse(RELATIONAL_IN(false, false));
      assertFalse(RELATIONAL_IN(false, true));
      assertFalse(RELATIONAL_IN(false, 0));
      assertFalse(RELATIONAL_IN(false, 1));
      assertFalse(RELATIONAL_IN(false, ''));
      assertFalse(RELATIONAL_IN(false, '1'));
      assertFalse(RELATIONAL_IN(false, 'a'));
      assertFalse(RELATIONAL_IN(false, { }));
      assertFalse(RELATIONAL_IN(false, { 'A' : true }));
      assertFalse(RELATIONAL_IN(true, null));
      assertFalse(RELATIONAL_IN(true, false));
      assertFalse(RELATIONAL_IN(true, true));
      assertFalse(RELATIONAL_IN(true, 0));
      assertFalse(RELATIONAL_IN(true, 1));
      assertFalse(RELATIONAL_IN(true, ''));
      assertFalse(RELATIONAL_IN(true, '1'));
      assertFalse(RELATIONAL_IN(true, 'a'));
      assertFalse(RELATIONAL_IN(true, { }));
      assertFalse(RELATIONAL_IN(true, { 'A' : true }));
      assertFalse(RELATIONAL_IN(0, null));
      assertFalse(RELATIONAL_IN(0, false));
      assertFalse(RELATIONAL_IN(0, true));
      assertFalse(RELATIONAL_IN(0, 0));
      assertFalse(RELATIONAL_IN(0, 1));
      assertFalse(RELATIONAL_IN(0, ''));
      assertFalse(RELATIONAL_IN(0, '1'));
      assertFalse(RELATIONAL_IN(0, 'a'));
      assertFalse(RELATIONAL_IN(0, { }));
      assertFalse(RELATIONAL_IN(0, { 'A' : true }));
      assertFalse(RELATIONAL_IN(1, null));
      assertFalse(RELATIONAL_IN(1, false));
      assertFalse(RELATIONAL_IN(1, true));
      assertFalse(RELATIONAL_IN(1, 0));
      assertFalse(RELATIONAL_IN(1, 1));
      assertFalse(RELATIONAL_IN(1, ''));
      assertFalse(RELATIONAL_IN(1, '1'));
      assertFalse(RELATIONAL_IN(1, 'a'));
      assertFalse(RELATIONAL_IN(1, { }));
      assertFalse(RELATIONAL_IN(1, { 'A' : true }));
      assertFalse(RELATIONAL_IN('', null));
      assertFalse(RELATIONAL_IN('', false));
      assertFalse(RELATIONAL_IN('', true));
      assertFalse(RELATIONAL_IN('', 0));
      assertFalse(RELATIONAL_IN('', 1));
      assertFalse(RELATIONAL_IN('', ''));
      assertFalse(RELATIONAL_IN('', '1'));
      assertFalse(RELATIONAL_IN('', 'a'));
      assertFalse(RELATIONAL_IN('', { }));
      assertFalse(RELATIONAL_IN('', { 'A' : true }));
      assertFalse(RELATIONAL_IN('a', null));
      assertFalse(RELATIONAL_IN('a', false));
      assertFalse(RELATIONAL_IN('a', true));
      assertFalse(RELATIONAL_IN('a', 0));
      assertFalse(RELATIONAL_IN('a', 1));
      assertFalse(RELATIONAL_IN('a', ''));
      assertFalse(RELATIONAL_IN('a', '1'));
      assertFalse(RELATIONAL_IN('a', 'a'));
      assertFalse(RELATIONAL_IN('a', { }));
      assertFalse(RELATIONAL_IN('a', { 'A' : true }));
      assertFalse(RELATIONAL_IN([ ], null));
      assertFalse(RELATIONAL_IN([ ], false));
      assertFalse(RELATIONAL_IN([ ], true));
      assertFalse(RELATIONAL_IN([ ], 0));
      assertFalse(RELATIONAL_IN([ ], 1));
      assertFalse(RELATIONAL_IN([ ], ''));
      assertFalse(RELATIONAL_IN([ ], '1'));
      assertFalse(RELATIONAL_IN([ ], 'a'));
      assertFalse(RELATIONAL_IN([ ], { }));
      assertFalse(RELATIONAL_IN([ ], { 'A' : true }));
      assertFalse(RELATIONAL_IN([ 0 ], null));
      assertFalse(RELATIONAL_IN([ 0 ], false));
      assertFalse(RELATIONAL_IN([ 0 ], true));
      assertFalse(RELATIONAL_IN([ 0 ], 0));
      assertFalse(RELATIONAL_IN([ 0 ], 1));
      assertFalse(RELATIONAL_IN([ 0 ], ''));
      assertFalse(RELATIONAL_IN([ 0 ], '1'));
      assertFalse(RELATIONAL_IN([ 0 ], 'a'));
      assertFalse(RELATIONAL_IN([ 0 ], { }));
      assertFalse(RELATIONAL_IN([ 0 ], { 'A' : true }));
      assertFalse(RELATIONAL_IN([ 1 ], null));
      assertFalse(RELATIONAL_IN([ 1 ], false));
      assertFalse(RELATIONAL_IN([ 1 ], true));
      assertFalse(RELATIONAL_IN([ 1 ], 0));
      assertFalse(RELATIONAL_IN([ 1 ], 1));
      assertFalse(RELATIONAL_IN([ 1 ], ''));
      assertFalse(RELATIONAL_IN([ 1 ], '1'));
      assertFalse(RELATIONAL_IN([ 1 ], 'a'));
      assertFalse(RELATIONAL_IN([ 1 ], { }));
      assertFalse(RELATIONAL_IN([ 1 ], { 'A' : true }));
      assertFalse(RELATIONAL_IN({ }, null));
      assertFalse(RELATIONAL_IN({ }, false));
      assertFalse(RELATIONAL_IN({ }, true));
      assertFalse(RELATIONAL_IN({ }, 0));
      assertFalse(RELATIONAL_IN({ }, 1));
      assertFalse(RELATIONAL_IN({ }, ''));
      assertFalse(RELATIONAL_IN({ }, '1'));
      assertFalse(RELATIONAL_IN({ }, 'a'));
      assertFalse(RELATIONAL_IN({ }, { }));
      assertFalse(RELATIONAL_IN({ }, { 'A' : true }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_IN function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalInTrue: function() {
      assertTrue(RELATIONAL_IN(null, [ null ]));
      assertTrue(RELATIONAL_IN(null, [ null, false ]));
      assertTrue(RELATIONAL_IN(null, [ false, null ]));
      assertTrue(RELATIONAL_IN(false, [ false ]));
      assertTrue(RELATIONAL_IN(false, [ true, false ]));
      assertTrue(RELATIONAL_IN(false, [ 0, false ]));
      assertTrue(RELATIONAL_IN(true, [ true ]));
      assertTrue(RELATIONAL_IN(true, [ false, true ]));
      assertTrue(RELATIONAL_IN(true, [ 0, false, true ]));
      assertTrue(RELATIONAL_IN(0, [ 0 ]));
      assertTrue(RELATIONAL_IN(1, [ 1 ]));
      assertTrue(RELATIONAL_IN(0, [ 3, 2, 1, 0 ]));
      assertTrue(RELATIONAL_IN(1, [ 3, 2, 1, 0 ]));
      assertTrue(RELATIONAL_IN(-35.5, [ 3, 2, 1, -35.5, 0 ]));
      assertTrue(RELATIONAL_IN(1.23e32, [ 3, 2, 1, 1.23e32, 35.5, 0 ]));
      assertTrue(RELATIONAL_IN('', [ '' ]));
      assertTrue(RELATIONAL_IN('', [ ' ', '' ]));
      assertTrue(RELATIONAL_IN('', [ 'a', 'b', 'c', '' ]));
      assertTrue(RELATIONAL_IN('A', [ 'c', 'b', 'a', 'A' ]));
      assertTrue(RELATIONAL_IN(' ', [ ' ' ]));
      assertTrue(RELATIONAL_IN(' a', [ ' a' ]));
      assertTrue(RELATIONAL_IN(' a ', [ ' a ' ]));
      assertTrue(RELATIONAL_IN([ ], [ [ ] ]));
      assertTrue(RELATIONAL_IN([ ], [ 1, null, 2, 3, [ ], 5 ]));
      assertTrue(RELATIONAL_IN([ null ], [ [ null ] ]));
      assertTrue(RELATIONAL_IN([ null ], [ null, [ null ], true ]));
      assertTrue(RELATIONAL_IN([ null, false ], [ [ null, false ] ]));
      assertTrue(RELATIONAL_IN([ 'a', 'A', false ], [ 'a', true, [ 'a', 'A', false ] ]));
      assertTrue(RELATIONAL_IN({ }, [ { } ]));
      assertTrue(RELATIONAL_IN({ }, [ 'a', null, false, 0, { } ]));
      assertTrue(RELATIONAL_IN({ 'a' : true }, [ 'a', null, false, 0, { 'a' : true } ]));
      assertTrue(RELATIONAL_IN({ 'a' : true, 'A': false }, [ 'a', null, false, 0, { 'A' : false, 'a' : true } ]));
      assertTrue(RELATIONAL_IN({ 'a' : { 'b' : null, 'c': 1 } }, [ { 'a' : { 'c' : 1, 'b' : null } } ]));
      assertTrue(RELATIONAL_IN({ 'a' : { 'b' : null, 'c': 1 } }, [ 'a', 'b', { 'a' : { 'c' : 1, 'b' : null } } ]));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test RELATIONAL_IN function
    ////////////////////////////////////////////////////////////////////////////////

    testRelationalInFalse: function() {
      assertFalse(RELATIONAL_IN(null, [ ]));
      assertFalse(RELATIONAL_IN(false, [ ]));
      assertFalse(RELATIONAL_IN(true, [ ]));
      assertFalse(RELATIONAL_IN(0, [ ]));
      assertFalse(RELATIONAL_IN(1, [ ]));
      assertFalse(RELATIONAL_IN('', [ ]));
      assertFalse(RELATIONAL_IN('0', [ ]));
      assertFalse(RELATIONAL_IN('1', [ ]));
      assertFalse(RELATIONAL_IN([ ], [ ]));
      assertFalse(RELATIONAL_IN([ 0 ], [ ]));
      assertFalse(RELATIONAL_IN({ }, [ ]));
      assertFalse(RELATIONAL_IN({ 'a' : true }, [ ]));
      assertFalse(RELATIONAL_IN(null, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, true, false, { }, [ ] ]));
      assertFalse(RELATIONAL_IN(true, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, false, { }, [ ] ]));
      assertFalse(RELATIONAL_IN(false, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, { }, [ ] ]));
      assertFalse(RELATIONAL_IN(0, [ '0', '1', '', 'null', 'true', 'false', -1, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(RELATIONAL_IN(1, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(RELATIONAL_IN(-1, [ '0', '1', '', 'null', 'true', 'false', 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(RELATIONAL_IN('', [ '0', '1', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(RELATIONAL_IN('0', [ '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(RELATIONAL_IN('1', [ '0', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { }, [ ] ]));
      assertFalse(RELATIONAL_IN([ ], [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, { } ]));
      assertFalse(RELATIONAL_IN({ }, [ '0', '1', '', 'null', 'true', 'false', -1, 0, 1, 2, 3, null, true, false, [ ] ]));
      assertFalse(RELATIONAL_IN(null, [ [ null ]]));
      assertFalse(RELATIONAL_IN(false, [ [ false ]]));
      assertFalse(RELATIONAL_IN(true, [ [ true ]]));
      assertFalse(RELATIONAL_IN(0, [ [ 0 ]]));
      assertFalse(RELATIONAL_IN(1, [ [ 1 ]]));
      assertFalse(RELATIONAL_IN('', [ [ '' ]]));
      assertFalse(RELATIONAL_IN('1', [ [ '1' ]]));
      assertFalse(RELATIONAL_IN('', [ [ '' ]]));
      assertFalse(RELATIONAL_IN(' ', [ [ ' ' ]]));
      assertFalse(RELATIONAL_IN('a', [ 'A' ]));
      assertFalse(RELATIONAL_IN('a', [ 'b', 'c', 'd' ]));
      assertFalse(RELATIONAL_IN('', [ ' ', '  ' ]));
      assertFalse(RELATIONAL_IN(' ', [ '', '  ' ]));
      assertFalse(RELATIONAL_IN([ ], [ 0 ]));
      assertFalse(RELATIONAL_IN([ ], [ 1, 2 ]));
      assertFalse(RELATIONAL_IN([ 0 ], [ 1, 2 ]));
      assertFalse(RELATIONAL_IN([ 1 ], [ 1, 2 ]));
      assertFalse(RELATIONAL_IN([ 2 ], [ 1, 2 ]));
      assertFalse(RELATIONAL_IN([ 1, 2 ], [ 1, 2 ]));
      assertFalse(RELATIONAL_IN([ 1, 2 ], [ [ 1 ], [ 2 ] ]));
      assertFalse(RELATIONAL_IN([ 1, 2 ], [ [ 2, 1 ] ]));
      assertFalse(RELATIONAL_IN([ 1, 2 ], [ [ 1, 2, 3 ] ]));
      assertFalse(RELATIONAL_IN([ 1, 2, 3 ], [ [ 1, 2, 4 ] ]));
      assertFalse(RELATIONAL_IN([ 1, 2, 3 ], [ [ 0, 1, 2, 3 ] ]));
      assertFalse(RELATIONAL_IN({ 'a' : true }, [ { 'a' : true, 'b' : false } ]));
      assertFalse(RELATIONAL_IN({ 'a' : true }, [ { 'a' : false } ]));
      assertFalse(RELATIONAL_IN({ 'a' : true }, [ { 'b' : true } ]));
      assertFalse(RELATIONAL_IN({ 'a' : true }, [ [ { 'a' : true } ] ]));
      assertFalse(RELATIONAL_IN({ 'a' : true }, [ 1, 2, { 'a' : { 'a' : true } } ]));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test UNARY_PLUS function
    ////////////////////////////////////////////////////////////////////////////////

    testUnaryPlusUndefined: function() {
      assertEqual(0, UNARY_PLUS(null));
      assertEqual(0, UNARY_PLUS(false));
      assertEqual(1, UNARY_PLUS(true));
      assertEqual(0, UNARY_PLUS(' '));
      assertEqual(0, UNARY_PLUS('abc'));
      assertEqual(0, UNARY_PLUS('1abc'));
      assertEqual(0, UNARY_PLUS('abc1'));
      assertEqual(1, UNARY_PLUS('1  '));
      assertEqual(1, UNARY_PLUS('  1  '));
      assertEqual(0, UNARY_PLUS(''));
      assertEqual(-1, UNARY_PLUS('-1'));
      assertEqual(0, UNARY_PLUS('0'));
      assertEqual(1, UNARY_PLUS('1'));
      assertEqual(1.5, UNARY_PLUS('1.5'));
      assertEqual(0, UNARY_PLUS([ ]));
      assertEqual(0, UNARY_PLUS([ 0 ]));
      assertEqual(1, UNARY_PLUS([ 1 ]));
      assertEqual(17, UNARY_PLUS([ 17 ]));
      assertEqual(0, UNARY_PLUS({ 'a' : 1 }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test UNARY_PLUS function
    ////////////////////////////////////////////////////////////////////////////////

    testUnaryPlusValue: function() {
      assertEqual(0, UNARY_PLUS(0));
      assertEqual(1, UNARY_PLUS(1));
      assertEqual(-1, UNARY_PLUS(-1));
      assertEqual(0.0001, UNARY_PLUS(0.0001));
      assertEqual(-0.0001, UNARY_PLUS(-0.0001));
      assertEqual(100, UNARY_PLUS(100));
      assertEqual(1054.342, UNARY_PLUS(1054.342));
      assertEqual(-1054.342, UNARY_PLUS(-1054.342));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test UNARY_MINUS function
    ////////////////////////////////////////////////////////////////////////////////

    testUnaryMinusUndefined: function() {
      assertEqual(0, UNARY_MINUS(null));
      assertEqual(0, UNARY_MINUS(false));
      assertEqual(-1, UNARY_MINUS(true));
      assertEqual(0, UNARY_MINUS(''));
      assertEqual(0, UNARY_MINUS(' '));
      assertEqual(0, UNARY_MINUS('abc'));
      assertEqual(0, UNARY_MINUS('1abc'));
      assertEqual(0, UNARY_MINUS('abc1'));
      assertEqual(-1, UNARY_MINUS(' 1'));
      assertEqual(-1, UNARY_MINUS('1 '));
      assertEqual(1, UNARY_MINUS('-1'));
      assertEqual(0, UNARY_MINUS('0'));
      assertEqual(-1, UNARY_MINUS('1'));
      assertEqual(-1.5, UNARY_MINUS('1.5'));
      assertEqual(1.5, UNARY_MINUS('-1.5'));
      assertEqual(0, UNARY_MINUS([ ]));
      assertEqual(0, UNARY_MINUS([ 0 ]));
      assertEqual(-1, UNARY_MINUS([ 1 ]));
      assertEqual(23, UNARY_MINUS([ -23 ]));
      assertEqual(0, UNARY_MINUS([ 1, 2 ]));
      assertEqual(0, UNARY_MINUS({ 'a' : 1 }));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test UNARY_MINUS function
    ////////////////////////////////////////////////////////////////////////////////

    testUnaryMinusValue: function() {
      assertEqual(0, UNARY_MINUS(0));
      assertEqual(1, UNARY_MINUS(-1));
      assertEqual(-1, UNARY_MINUS(1));
      assertEqual(0.0001, UNARY_MINUS(-0.0001));
      assertEqual(-0.0001, UNARY_MINUS(0.0001));
      assertEqual(100, UNARY_MINUS(-100));
      assertEqual(-100, UNARY_MINUS(100));
      assertEqual(1054.342, UNARY_MINUS(-1054.342));
      assertEqual(-1054.342, UNARY_MINUS(1054.342));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ARITHMETIC_PLUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticPlusUndefined: function() {
      assertEqual(1, ARITHMETIC_PLUS(1, null));
      assertEqual(1, ARITHMETIC_PLUS(1, false));
      assertEqual(2, ARITHMETIC_PLUS(1, true));
      assertEqual('1', ARITHMETIC_PLUS(1, ''));
      assertEqual(1, ARITHMETIC_PLUS(1, ' '));
      assertEqual(1, ARITHMETIC_PLUS(1, '0'));
      assertEqual(2, ARITHMETIC_PLUS(1, '1'));
      assertEqual(1, ARITHMETIC_PLUS(1, 'a'));
      assertEqual(1, ARITHMETIC_PLUS(1, [ ]));
      assertEqual(1, ARITHMETIC_PLUS(1, [ 0 ]));
      assertEqual(1, ARITHMETIC_PLUS(1, { }));
      assertEqual(1, ARITHMETIC_PLUS(1, { 'a' : 0 }));
      assertEqual(1, ARITHMETIC_PLUS(null, 1));
      assertEqual(1, ARITHMETIC_PLUS(false, 1));
      assertEqual(2, ARITHMETIC_PLUS(true, 1));
      assertEqual(1, ARITHMETIC_PLUS('', 1));
      assertEqual(1, ARITHMETIC_PLUS(' ', 1));
      assertEqual(1, ARITHMETIC_PLUS('0', 1));
      assertEqual(2, ARITHMETIC_PLUS('1', 1));
      assertEqual(1, ARITHMETIC_PLUS('a', 1));
      assertEqual(1, ARITHMETIC_PLUS([ ], 1));
      assertEqual(1, ARITHMETIC_PLUS([ 0 ], 1));
      assertEqual(4, ARITHMETIC_PLUS([ 3 ], 1));
      assertEqual(1, ARITHMETIC_PLUS({ }, 1));
      assertEqual(1, ARITHMETIC_PLUS({ 'a' : 0 }, 1));
      assertEqual(0, ARITHMETIC_PLUS('0', '0'));
      assertEqual(8, ARITHMETIC_PLUS('4', '4'));
      assertEqual(0, ARITHMETIC_PLUS('4', '-4'));
      assertEqual(0, ARITHMETIC_PLUS(1.3e308 * 10, 1.3e308 * 10));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ARITHMETIC_PLUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticPlusValue: function() {
      assertEqual(0, ARITHMETIC_PLUS(0, 0));
      assertEqual(0, ARITHMETIC_PLUS(1, -1));
      assertEqual(0, ARITHMETIC_PLUS(-1, 1));
      assertEqual(0, ARITHMETIC_PLUS(-100, 100));
      assertEqual(0, ARITHMETIC_PLUS(100, -100));
      assertEqual(0, ARITHMETIC_PLUS(0.11, -0.11));
      assertEqual(10, ARITHMETIC_PLUS(5, 5));
      assertEqual(10, ARITHMETIC_PLUS(4, 6));
      assertEqual(10, ARITHMETIC_PLUS(1, 9));
      assertEqual(10, ARITHMETIC_PLUS(0.1, 9.9));
      assertEqual(9.8, ARITHMETIC_PLUS(-0.1, 9.9));
      assertEqual(-34.2, ARITHMETIC_PLUS(-17.1, -17.1));
      assertEqual(-2, ARITHMETIC_PLUS(-1, -1)); 
      assertEqual(2.6e307, ARITHMETIC_PLUS(1.3e307, 1.3e307));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ARITHMETIC_MINUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticMinusUndefined: function() {
      assertEqual(1, ARITHMETIC_MINUS(1, null));
      assertEqual(1, ARITHMETIC_MINUS(1, false));
      assertEqual(0, ARITHMETIC_MINUS(1, true));
      assertEqual(1, ARITHMETIC_MINUS(1, ''));
      assertEqual(1, ARITHMETIC_MINUS(1, ' '));
      assertEqual(1, ARITHMETIC_MINUS(1, '0'));
      assertEqual(0, ARITHMETIC_MINUS(1, '1'));
      assertEqual(1, ARITHMETIC_MINUS(1, 'a'));
      assertEqual(1, ARITHMETIC_MINUS(1, [ ]));
      assertEqual(1, ARITHMETIC_MINUS(1, [ 0 ]));
      assertEqual(1, ARITHMETIC_MINUS(1, { }));
      assertEqual(1, ARITHMETIC_MINUS(1, { 'a' : 0 }));
      assertEqual(-1, ARITHMETIC_MINUS(null, 1));
      assertEqual(-1, ARITHMETIC_MINUS(false, 1));
      assertEqual(0, ARITHMETIC_MINUS(true, 1));
      assertEqual(-1, ARITHMETIC_MINUS('', 1));
      assertEqual(-1, ARITHMETIC_MINUS(' ', 1));
      assertEqual(-1, ARITHMETIC_MINUS('0', 1));
      assertEqual(0, ARITHMETIC_MINUS('1', 1));
      assertEqual(-1, ARITHMETIC_MINUS('a', 1));
      assertEqual(-1, ARITHMETIC_MINUS([ ], 1));
      assertEqual(-1, ARITHMETIC_MINUS([ 0 ], 1));
      assertEqual(-1, ARITHMETIC_MINUS({ }, 1));
      assertEqual(-1, ARITHMETIC_MINUS({ 'a' : 0 }, 1));
      assertEqual(0, ARITHMETIC_MINUS('0', '0'));
      assertEqual(0, ARITHMETIC_MINUS(-1.3e308 * 10, 1.3e308 * 10));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ARITHMETIC_MINUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticMinusValue: function() {
      assertEqual(0, ARITHMETIC_MINUS(0, 0));
      assertEqual(-1, ARITHMETIC_MINUS(0, 1));
      assertEqual(0, ARITHMETIC_MINUS(-1, -1)); 
      assertEqual(0, ARITHMETIC_MINUS(1, 1));
      assertEqual(2, ARITHMETIC_MINUS(1, -1));
      assertEqual(-2, ARITHMETIC_MINUS(-1, 1));
      assertEqual(-200, ARITHMETIC_MINUS(-100, 100));
      assertEqual(200, ARITHMETIC_MINUS(100, -100));
      assertEqual(0.22, ARITHMETIC_MINUS(0.11, -0.11));
      assertEqual(0, ARITHMETIC_MINUS(5, 5));
      assertEqual(-2, ARITHMETIC_MINUS(4, 6));
      assertEqual(-8, ARITHMETIC_MINUS(1, 9));
      assertEqual(-9.8, ARITHMETIC_MINUS(0.1, 9.9));
      assertEqual(-10, ARITHMETIC_MINUS(-0.1, 9.9));
      assertEqual(0, ARITHMETIC_MINUS(-17.1, -17.1));
      assertEqual(0, ARITHMETIC_MINUS(1.3e307, 1.3e307));
      assertEqual(2.6e307, ARITHMETIC_MINUS(1.3e307, -1.3e307));
      assertEqual(-2.6e307, ARITHMETIC_MINUS(-1.3e307, 1.3e307));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ARITHMETIC_TIMES function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticTimesUndefined: function() {
      assertEqual(0, ARITHMETIC_TIMES(1, null));
      assertEqual(0, ARITHMETIC_TIMES(1, false));
      assertEqual(1, ARITHMETIC_TIMES(1, true));
      assertEqual(2, ARITHMETIC_TIMES(2, true));
      assertEqual(0, ARITHMETIC_TIMES(1, ''));
      assertEqual(0, ARITHMETIC_TIMES(1, ' '));
      assertEqual(0, ARITHMETIC_TIMES(1, '0'));
      assertEqual(1, ARITHMETIC_TIMES(1, '1'));
      assertEqual(0, ARITHMETIC_TIMES(1, 'a'));
      assertEqual(0, ARITHMETIC_TIMES(1, [ ]));
      assertEqual(0, ARITHMETIC_TIMES(1, [ 0 ]));
      assertEqual(2, ARITHMETIC_TIMES(1, [ 2 ]));
      assertEqual(0, ARITHMETIC_TIMES(1, { }));
      assertEqual(0, ARITHMETIC_TIMES(1, { 'a' : 0 }));
      assertEqual(0, ARITHMETIC_TIMES(null, 1));
      assertEqual(0, ARITHMETIC_TIMES(false, 1));
      assertEqual(1, ARITHMETIC_TIMES(true, 1));
      assertEqual(2, ARITHMETIC_TIMES(true, 2));
      assertEqual(0, ARITHMETIC_TIMES('', 1));
      assertEqual(0, ARITHMETIC_TIMES(' ', 1));
      assertEqual(0, ARITHMETIC_TIMES('0', 1));
      assertEqual(1, ARITHMETIC_TIMES('1', 1));
      assertEqual(0, ARITHMETIC_TIMES('a', 1));
      assertEqual(0, ARITHMETIC_TIMES([ ], 1));
      assertEqual(0, ARITHMETIC_TIMES([ 0 ], 1));
      assertEqual(0, ARITHMETIC_TIMES({ }, 1));
      assertEqual(0, ARITHMETIC_TIMES({ 'a' : 0 }, 1));
      assertEqual(null, ARITHMETIC_TIMES(1.3e190, 1.3e190));
      assertEqual(null, ARITHMETIC_TIMES(1.3e307, 1.3e307));
      assertEqual(0, ARITHMETIC_TIMES(1.3e308 * 10, 1.3e308 * 10));
      assertEqual(0, ARITHMETIC_TIMES('0', '0'));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ARITHMETIC_TIMES function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticTimesValue: function() {
      assertEqual(0, ARITHMETIC_TIMES(0, 0));
      assertEqual(0, ARITHMETIC_TIMES(1, 0));
      assertEqual(0, ARITHMETIC_TIMES(0, 1));
      assertEqual(1, ARITHMETIC_TIMES(1, 1));
      assertEqual(2, ARITHMETIC_TIMES(2, 1));
      assertEqual(2, ARITHMETIC_TIMES(1, 2));
      assertEqual(4, ARITHMETIC_TIMES(2, 2));
      assertEqual(100, ARITHMETIC_TIMES(10, 10));
      assertEqual(1000, ARITHMETIC_TIMES(10, 100));
      assertEqual(1.44, ARITHMETIC_TIMES(1.2, 1.2));
      assertEqual(-1.44, ARITHMETIC_TIMES(1.2, -1.2));
      assertEqual(1.44, ARITHMETIC_TIMES(-1.2, -1.2));
      assertEqual(2, ARITHMETIC_TIMES(0.25, 8));
      assertEqual(2, ARITHMETIC_TIMES(8, 0.25));
      assertEqual(2.25, ARITHMETIC_TIMES(9, 0.25));
      assertEqual(-2.25, ARITHMETIC_TIMES(9, -0.25));
      assertEqual(-2.25, ARITHMETIC_TIMES(-9, 0.25));
      assertEqual(2.25, ARITHMETIC_TIMES(-9, -0.25));
      assertEqual(4, ARITHMETIC_TIMES(-2, -2));
      assertEqual(-4, ARITHMETIC_TIMES(-2, 2));
      assertEqual(-4, ARITHMETIC_TIMES(2, -2));
      assertEqual(1000000, ARITHMETIC_TIMES(1000, 1000));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ARITHMETIC_DIVIDE function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticDivideUndefined: function() {
      assertEqual(null, ARITHMETIC_DIVIDE(1, null));
      assertEqual(null, ARITHMETIC_DIVIDE(1, false));
      assertEqual(1, ARITHMETIC_DIVIDE(1, true));
      assertEqual(2, ARITHMETIC_DIVIDE(2, true));
      assertEqual(null, ARITHMETIC_DIVIDE(1, ''));
      assertEqual(null, ARITHMETIC_DIVIDE(1, ' '));
      assertEqual(null, ARITHMETIC_DIVIDE(1, '0'));
      assertEqual(1, ARITHMETIC_DIVIDE(1, '1'));
      assertEqual(2, ARITHMETIC_DIVIDE(2, '1'));
      assertEqual(null, ARITHMETIC_DIVIDE(1, 'a'));
      assertEqual(null, ARITHMETIC_DIVIDE(1, [ ]));
      assertEqual(null, ARITHMETIC_DIVIDE(1, [ 0 ]));
      assertEqual(0.5, ARITHMETIC_DIVIDE(1, [ 2 ]));
      assertEqual(null, ARITHMETIC_DIVIDE(1, { }));
      assertEqual(null, ARITHMETIC_DIVIDE(1, { 'a' : 0 }));
      assertEqual(0, ARITHMETIC_DIVIDE(null, 1));
      assertEqual(0, ARITHMETIC_DIVIDE(false, 1));
      assertEqual(1, ARITHMETIC_DIVIDE(true, 1));
      assertEqual(0, ARITHMETIC_DIVIDE('', 1));
      assertEqual(0, ARITHMETIC_DIVIDE(' ', 1));
      assertEqual(0, ARITHMETIC_DIVIDE('0', 1));
      assertEqual(1, ARITHMETIC_DIVIDE('1', 1));
      assertEqual(0, ARITHMETIC_DIVIDE('a', 1));
      assertEqual(0, ARITHMETIC_DIVIDE([ ], 1));
      assertEqual(0, ARITHMETIC_DIVIDE([ 0 ], 1));
      assertEqual(0, ARITHMETIC_DIVIDE({ }, 1));
      assertEqual(0, ARITHMETIC_DIVIDE({ 'a' : 0 }, 1));
      assertEqual(null, ARITHMETIC_DIVIDE(1, 0));
      assertEqual(null, ARITHMETIC_DIVIDE(100, 0));
      assertEqual(null, ARITHMETIC_DIVIDE(-1, 0));
      assertEqual(null, ARITHMETIC_DIVIDE(-100, 0));
      assertEqual(null, ARITHMETIC_DIVIDE(0, 0));
      assertEqual(null, ARITHMETIC_DIVIDE('0', '0'));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ARITHMETIC_DIVIDE function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticDivideValue: function() {
      assertEqual(0, ARITHMETIC_DIVIDE(0, 1));
      assertEqual(0, ARITHMETIC_DIVIDE(0, 2));
      assertEqual(0, ARITHMETIC_DIVIDE(0, 10));
      assertEqual(0, ARITHMETIC_DIVIDE(0, -1));
      assertEqual(0, ARITHMETIC_DIVIDE(0, -2));
      assertEqual(0, ARITHMETIC_DIVIDE(0, -10));
      assertEqual(1, ARITHMETIC_DIVIDE(1, 1));
      assertEqual(2, ARITHMETIC_DIVIDE(2, 1));
      assertEqual(-1, ARITHMETIC_DIVIDE(-1, 1));
      assertEqual(100, ARITHMETIC_DIVIDE(100, 1));
      assertEqual(-100, ARITHMETIC_DIVIDE(-100, 1));
      assertEqual(-1, ARITHMETIC_DIVIDE(1, -1));
      assertEqual(-2, ARITHMETIC_DIVIDE(2, -1));
      assertEqual(1, ARITHMETIC_DIVIDE(-1, -1));
      assertEqual(-100, ARITHMETIC_DIVIDE(100, -1));
      assertEqual(100, ARITHMETIC_DIVIDE(-100, -1));
      assertEqual(0.5, ARITHMETIC_DIVIDE(1, 2));
      assertEqual(1, ARITHMETIC_DIVIDE(2, 2));
      assertEqual(2, ARITHMETIC_DIVIDE(4, 2));
      assertEqual(1, ARITHMETIC_DIVIDE(4, 4));
      assertEqual(5, ARITHMETIC_DIVIDE(10, 2));
      assertEqual(2, ARITHMETIC_DIVIDE(10, 5));
      assertEqual(2.5, ARITHMETIC_DIVIDE(10, 4));
      assertEqual(1, ARITHMETIC_DIVIDE(1.2, 1.2));
      assertEqual(-1, ARITHMETIC_DIVIDE(1.2, -1.2));
      assertEqual(1, ARITHMETIC_DIVIDE(-1.2, -1.2));
      assertEqual(2, ARITHMETIC_DIVIDE(1, 0.5));
      assertEqual(4, ARITHMETIC_DIVIDE(1, 0.25));
      assertEqual(16, ARITHMETIC_DIVIDE(4, 0.25));
      assertEqual(-16, ARITHMETIC_DIVIDE(4, -0.25));
      assertEqual(-16, ARITHMETIC_DIVIDE(-4, 0.25));
      assertEqual(100, ARITHMETIC_DIVIDE(25, 0.25));
      assertEqual(-100, ARITHMETIC_DIVIDE(25, -0.25));
      assertEqual(-100, ARITHMETIC_DIVIDE(-25, 0.25));
      assertEqual(1, ARITHMETIC_DIVIDE(0.22, 0.22));
      assertEqual(0.5, ARITHMETIC_DIVIDE(0.22, 0.44));
      assertEqual(2, ARITHMETIC_DIVIDE(0.22, 0.11));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ARITHMETIC_MODULUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticModulusUndefined: function() {
      assertEqual(null, ARITHMETIC_MODULUS(1, null));
      assertEqual(null, ARITHMETIC_MODULUS(1, false));
      assertEqual(0, ARITHMETIC_MODULUS(1, true));
      assertEqual(null, ARITHMETIC_MODULUS(1, ''));
      assertEqual(null, ARITHMETIC_MODULUS(1, ' '));
      assertEqual(null, ARITHMETIC_MODULUS(1, '0'));
      assertEqual(0, ARITHMETIC_MODULUS(1, '1'));
      assertEqual(null, ARITHMETIC_MODULUS(1, 'a'));
      assertEqual(null, ARITHMETIC_MODULUS(1, [ ]));
      assertEqual(null, ARITHMETIC_MODULUS(1, [ 0 ]));
      assertEqual(0, ARITHMETIC_MODULUS(1, [ 1 ]));
      assertEqual(1, ARITHMETIC_MODULUS(4, [ 3 ]));
      assertEqual(null, ARITHMETIC_MODULUS(1, { }));
      assertEqual(null, ARITHMETIC_MODULUS(1, { 'a' : 0 }));
      assertEqual(0, ARITHMETIC_MODULUS(null, 1));
      assertEqual(0, ARITHMETIC_MODULUS(false, 1));
      assertEqual(0, ARITHMETIC_MODULUS(true, 1));
      assertEqual(0, ARITHMETIC_MODULUS('', 1));
      assertEqual(0, ARITHMETIC_MODULUS(' ', 1));
      assertEqual(0, ARITHMETIC_MODULUS('0', 1));
      assertEqual(0, ARITHMETIC_MODULUS('1', 1));
      assertEqual(0, ARITHMETIC_MODULUS('a', 1));
      assertEqual(0, ARITHMETIC_MODULUS([ ], 1));
      assertEqual(0, ARITHMETIC_MODULUS([ 0 ], 1));
      assertEqual(0, ARITHMETIC_MODULUS({ }, 1));
      assertEqual(0, ARITHMETIC_MODULUS({ 'a' : 0 }, 1));
      assertEqual(null, ARITHMETIC_MODULUS(1, 0));
      assertEqual(null, ARITHMETIC_MODULUS(100, 0));
      assertEqual(null, ARITHMETIC_MODULUS(-1, 0));
      assertEqual(null, ARITHMETIC_MODULUS(-100, 0));
      assertEqual(null, ARITHMETIC_MODULUS(0, 0));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test ARITHMETIC_MODULUS function
    ////////////////////////////////////////////////////////////////////////////////

    testArithmeticModulusValue: function() {
      assertEqual(0, ARITHMETIC_MODULUS(0, 1));
      assertEqual(0, ARITHMETIC_MODULUS(1, 1));
      assertEqual(1, ARITHMETIC_MODULUS(1, 2));
      assertEqual(1, ARITHMETIC_MODULUS(1, 3));
      assertEqual(1, ARITHMETIC_MODULUS(1, 4));
      assertEqual(0, ARITHMETIC_MODULUS(2, 2));
      assertEqual(2, ARITHMETIC_MODULUS(2, 3));
      assertEqual(0, ARITHMETIC_MODULUS(3, 3));
      assertEqual(0, ARITHMETIC_MODULUS(10, 1));
      assertEqual(0, ARITHMETIC_MODULUS(10, 2));
      assertEqual(1, ARITHMETIC_MODULUS(10, 3));
      assertEqual(2, ARITHMETIC_MODULUS(10, 4));
      assertEqual(0, ARITHMETIC_MODULUS(10, 5));
      assertEqual(4, ARITHMETIC_MODULUS(10, 6));
      assertEqual(3, ARITHMETIC_MODULUS(10, 7));
      assertEqual(2, ARITHMETIC_MODULUS(10, 8));
      assertEqual(1, ARITHMETIC_MODULUS(10, 9));
      assertEqual(0, ARITHMETIC_MODULUS(10, 10));
      assertEqual(10, ARITHMETIC_MODULUS(10, 11));
      assertEqual(1, ARITHMETIC_MODULUS(4, 3));
      assertEqual(2, ARITHMETIC_MODULUS(5, 3));
      assertEqual(0, ARITHMETIC_MODULUS(6, 3));
      assertEqual(1, ARITHMETIC_MODULUS(7, 3));
      assertEqual(2, ARITHMETIC_MODULUS(8, 3));
      assertEqual(0, ARITHMETIC_MODULUS(9, 3));
      assertEqual(1, ARITHMETIC_MODULUS(10, 3));
      assertEqual(0, ARITHMETIC_MODULUS(10, -1));
      assertEqual(0, ARITHMETIC_MODULUS(10, -2));
      assertEqual(1, ARITHMETIC_MODULUS(10, -3));
      assertEqual(2, ARITHMETIC_MODULUS(10, -4));
      assertEqual(0, ARITHMETIC_MODULUS(10, -5));
      assertEqual(4, ARITHMETIC_MODULUS(10, -6));
      assertEqual(3, ARITHMETIC_MODULUS(10, -7));
      assertEqual(2, ARITHMETIC_MODULUS(10, -8));
      assertEqual(1, ARITHMETIC_MODULUS(10, -9));
      assertEqual(0, ARITHMETIC_MODULUS(10, -10));
      assertEqual(10, ARITHMETIC_MODULUS(10, -11));
      assertEqual(-1, ARITHMETIC_MODULUS(-1, 3));
      assertEqual(-2, ARITHMETIC_MODULUS(-2, 3));
      assertEqual(0, ARITHMETIC_MODULUS(-3, 3));
      assertEqual(-1, ARITHMETIC_MODULUS(-4, 3));
      assertEqual(-2, ARITHMETIC_MODULUS(-5, 3));
      assertEqual(0, ARITHMETIC_MODULUS(-6, 3));
      assertEqual(-1, ARITHMETIC_MODULUS(-7, 3));
      assertEqual(-2, ARITHMETIC_MODULUS(-8, 3));
      assertEqual(0, ARITHMETIC_MODULUS(-9, 3));
      assertEqual(-1, ARITHMETIC_MODULUS(-10, 3));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test aql.CONCAT function
    ////////////////////////////////////////////////////////////////////////////////

    testStringConcatValue: function() {
      assertEqual('', db._query(`RETURN CONCAT('')`).toArray()[0]);
      assertEqual('a', db._query(`RETURN CONCAT('a')`).toArray()[0]);
      assertEqual('a', db._query(`RETURN CONCAT('a', null)`).toArray()[0]);
      assertEqual('', db._query(`RETURN CONCAT('', null)`).toArray()[0]);
      assertEqual('', db._query(`RETURN CONCAT('', '')`).toArray()[0]);
      assertEqual(' ', db._query(`RETURN CONCAT(' ', '')`).toArray()[0]);
      assertEqual(' ', db._query(`RETURN CONCAT('', ' ')`).toArray()[0]);
      assertEqual('  ', db._query(`RETURN CONCAT(' ', ' ')`).toArray()[0]);
      assertEqual(' a a', db._query(`RETURN CONCAT(' a', ' a')`).toArray()[0]);
      assertEqual(' a a ', db._query(`RETURN CONCAT(' a', ' a ')`).toArray()[0]);
      assertEqual('a', db._query(`RETURN CONCAT('a', '')`).toArray()[0]);
      assertEqual('a', db._query(`RETURN CONCAT('', 'a')`).toArray()[0]);
      assertEqual('a ', db._query(`RETURN CONCAT('', 'a ')`).toArray()[0]);
      assertEqual('a ', db._query(`RETURN CONCAT('', 'a ')`).toArray()[0]);
      assertEqual('a ', db._query(`RETURN CONCAT('a ', '')`).toArray()[0]);
      assertEqual('ab', db._query(`RETURN CONCAT('a', 'b')`).toArray()[0]);
      assertEqual('ba', db._query(`RETURN CONCAT('b', 'a')`).toArray()[0]);
      assertEqual('AA', db._query(`RETURN CONCAT('A', 'A')`).toArray()[0]);
      assertEqual('AaA', db._query(`RETURN CONCAT('A', 'aA')`).toArray()[0]);
      assertEqual('AaA', db._query(`RETURN CONCAT('Aa', 'A')`).toArray()[0]);
      assertEqual('0.00', db._query(`RETURN CONCAT('0.00', '')`).toArray()[0]);
      assertEqual('abc', db._query(`RETURN CONCAT('a', CONCAT('b', 'c'))`).toArray()[0]);
      assertEqual('', db._query(`RETURN CONCAT('', CONCAT('', ''))`).toArray()[0]);
      assertEqual('fux', db._query(`RETURN CONCAT('f', 'u', 'x')`).toArray()[0]);
      assertEqual('fux', db._query(`RETURN CONCAT('f', 'u', null, 'x')`).toArray()[0]);
      assertEqual('fux', db._query(`RETURN CONCAT(null, 'f', null, 'u', null, 'x', null)`).toArray()[0]);
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
    },
    
    testNumberConversion: function() {
      assertEqual([ [ true, 0, 0 ] ], AQL_EXECUTE('LET str = "3a" RETURN [ 0 + str == TO_NUMBER(str), 0 + str, TO_NUMBER(str) ]').json);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlOperatorsTestSuite);

return jsunity.done();

