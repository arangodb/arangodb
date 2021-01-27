/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNull, assertTrue, assertFalse, fail */
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
var db = internal.db;
var errors = internal.errors;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;
var assertQueryWarningAndNull = helper.assertQueryWarningAndNull;

var sortObj = function (obj) {
  var result = { };
  Object.keys(obj).sort().forEach(function(k) {
    if (obj[k] !== null && typeof obj[k] === 'object' && ! Array.isArray(obj[k])) {
      result[k] = sortObj(obj[k]);
    }
    else {
      result[k] = obj[k]; 
    }
  });
  return result;
};

var assertEqualObj = function (expected, actual) {
  expected = sortObj(expected);
  actual   = sortObj(actual);

  assertEqual(expected, actual);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlFunctionsTestSuite () {
  var collectionName = "UnitTestsAqlFunctions";
  var collection;
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      db._drop(collectionName);
      collection = db._create(collectionName);
      
      // Insert 10 elements
      for (var i = 0; i < 10; ++i) {
        collection.save({});
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(collectionName);
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFail : function () {
      try {
        db._query("RETURN FAIL()");
        fail();
      }
      catch(err) {
        var expected = internal.errors.ERROR_QUERY_FAIL_CALLED.code;
        assertEqual(expected, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFailInt : function () {
      try {
        db._query("RETURN FAIL(1234)");
        fail();
      }
      catch(err) {
        var expected = internal.errors.ERROR_QUERY_FAIL_CALLED.code;
        assertEqual(expected, err.errorNum);
        assertTrue(err.errorMessage.search("1234") === -1);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFailObj : function () {
      try {
        db._query("RETURN FAIL({})");
        fail();
      }
      catch(err) {
        var expected = internal.errors.ERROR_QUERY_FAIL_CALLED.code;
        assertEqual(expected, err.errorNum);
        assertTrue(err.errorMessage.search("{}") === -1);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFailMessage : function () {
      try {
        db._query("RETURN FAIL('bla')");
        fail();
      }
      catch(err) {
        var expected = internal.errors.ERROR_QUERY_FAIL_CALLED.code;
        assertEqual(expected, err.errorNum);
        assertTrue(err.errorMessage.search("bla") !== -1);
      }
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirst1 : function () {
      var expected = [ { "the fox" : "jumped" } ];
      var actual = getQueryResults("RETURN FIRST([ { \"the fox\" : \"jumped\" }, \"over\", [ \"the dog\" ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirst2 : function () {
      var expected = [ "over" ];
      var actual = getQueryResults("RETURN FIRST([ \"over\", [ \"the dog\" ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirst3 : function () {
      var expected = [ [ "the dog" ] ];
      var actual = getQueryResults("RETURN FIRST([ [ \"the dog\" ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirst4 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN FIRST([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirstInvalid : function () {
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN FIRST(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN FIRST(true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN FIRST(4)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN FIRST(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN FIRST({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirstCxx1 : function () {
      var expected = [ { "the fox" : "jumped" } ];
      var actual = getQueryResults("RETURN NOOPT(FIRST([ { \"the fox\" : \"jumped\" }, \"over\", [ \"the dog\" ] ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirstCxx2 : function () {
      var expected = [ "over" ];
      var actual = getQueryResults("RETURN NOOPT(FIRST([ \"over\", [ \"the dog\" ] ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirstCxx3 : function () {
      var expected = [ [ "the dog" ] ];
      var actual = getQueryResults("RETURN NOOPT(FIRST([ [ \"the dog\" ] ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirstCxx4 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN NOOPT(FIRST([ ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test first function
////////////////////////////////////////////////////////////////////////////////

    testFirstCxxInvalid : function () {
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(FIRST(null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(FIRST(true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(FIRST(4))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(FIRST(\"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(FIRST({ }))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLast1 : function () {
      var expected = [ [ "the dog" ] ];
      var actual = getQueryResults("RETURN LAST([ { \"the fox\" : \"jumped\" }, \"over\", [ \"the dog\" ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLast2 : function () {
      var expected = [ "over" ];
      var actual = getQueryResults("RETURN LAST([ { \"the fox\" : \"jumped\" }, \"over\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLast3 : function () {
      var expected = [ { "the fox" : "jumped" } ];
      var actual = getQueryResults("RETURN LAST([ { \"the fox\" : \"jumped\" } ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLast4 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN LAST([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLastInvalid : function () {
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN LAST(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN LAST(true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN LAST(4)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN LAST(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN LAST({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLastCxx1 : function () {
      var expected = [ [ "the dog" ] ];
      var actual = getQueryResults("RETURN NOOPT(LAST([ { \"the fox\" : \"jumped\" }, \"over\", [ \"the dog\" ] ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLastCxx2 : function () {
      var expected = [ "over" ];
      var actual = getQueryResults("RETURN NOOPT(LAST([ { \"the fox\" : \"jumped\" }, \"over\" ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLastCxx3 : function () {
      var expected = [ { "the fox" : "jumped" } ];
      var actual = getQueryResults("RETURN NOOPT(LAST([ { \"the fox\" : \"jumped\" } ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLastCxx4 : function () {
      var expected = [ null ];
      var actual = getQueryResults("RETURN NOOPT(LAST([ ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test last function
////////////////////////////////////////////////////////////////////////////////

    testLastInvalidCxx : function () {
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(LAST(null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(LAST(true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(LAST(4))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(LAST(\"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(LAST({ }))"); 
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test position function
////////////////////////////////////////////////////////////////////////////////

    testPosition : function () {
      var list = [ "foo", "bar", null, "baz", true, 42, [ "BORK" ], { code: "foo", name: "test" }, false, 0, "" ];

      var data = [
        [ "foo", 0 ],
        [ "bar", 1 ],
        [ null, 2 ],
        [ "baz", 3 ],
        [ true, 4 ],
        [ 42, 5 ],
        [ [ "BORK" ], 6 ],
        [ { code: "foo", name: "test" }, 7 ],
        [ false, 8 ],
        [ 0, 9 ],
        [ "", 10 ]
      ];

      data.forEach(function (d) {
        var search = d[0];
        var expected = d[1];
        
        // find if element is contained in list (should be true)
        var actual = getQueryResults("RETURN POSITION(@list, @search, false)", { list: list, search: search });
        assertTrue(actual[0]);

        // find position of element in list
        actual = getQueryResults("RETURN POSITION(@list, @search, true)", { list: list, search: search });
        assertEqual(expected, actual[0]);
        
        // look up the element using the position
        actual = getQueryResults("RETURN NTH(@list, @position)", { list: list, position: actual[0] });
        assertEqual(search, actual[0]);

        // find if element is contained in list (should be true)
        actual = getQueryResults("RETURN NOOPT(POSITION(@list, @search, false))", { list: list, search: search });
        assertTrue(actual[0]);

        // find position of element in list
        actual = getQueryResults("RETURN NOOPT(POSITION(@list, @search, true))", { list: list, search: search });
        assertEqual(expected, actual[0]);
        
        // look up the element using the position
        actual = getQueryResults("RETURN NOOPT(NTH(@list, @position))", { list: list, position: actual[0] });
        assertEqual(search, actual[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test position function
////////////////////////////////////////////////////////////////////////////////

    testPositionNotThere : function () {
      var list = [ "foo", "bar", null, "baz", true, 42, [ "BORK" ], { code: "foo", name: "test" }, false, 0, "" ];

      var data = [ "foot", "barz", 43, 1, -42, { code: "foo", name: "test", bar: "baz" }, " ", " foo", "FOO", "bazt", 0.1, [ ], [ "bork" ] ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN POSITION(@list, @search, false)", { list: list, search: d });
        assertFalse(actual[0]);

        actual = getQueryResults("RETURN POSITION(@list, @search, true)", { list: list, search: d });
        assertEqual(-1, actual[0]);

        actual = getQueryResults("RETURN NOOPT(POSITION(@list, @search, false))", { list: list, search: d });
        assertFalse(actual[0]);

        actual = getQueryResults("RETURN NOOPT(POSITION(@list, @search, true))", { list: list, search: d });
        assertEqual(-1, actual[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test position function
////////////////////////////////////////////////////////////////////////////////

    testPositionInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN POSITION()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN POSITION([ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN POSITION(null, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN POSITION(true, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN POSITION(4, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN POSITION(\"yes\", 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN POSITION({ }, 'foo')"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(POSITION())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(POSITION([ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(POSITION(null, 'foo'))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(POSITION(true, 'foo'))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(POSITION(4, 'foo'))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(POSITION(\"yes\", 'foo'))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(POSITION({ }, 'foo'))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthEmpty : function () {
      var i;

      for (i = -3; i <= 3; ++i) {                         
        var actual = getQueryResults("RETURN NTH([ ], @pos)", { pos: i });
        assertNull(actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthNegative : function () {
      var i;

      for (i = -3; i <= 3; ++i) {                         
        var actual = getQueryResults("RETURN NTH([ 1, 2, 3, 4 ], @pos)", { pos: i });
        if (i < 0) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(i + 1, actual[0]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthBounds : function () {
      var i;

      for (i = 0; i <= 10; ++i) {                         
        var actual = getQueryResults("RETURN NTH([ 'a1', 'a2', 'a3', 'a4', 'a5' ], @pos)", { pos: i });
        if (i < 5) {
          assertEqual('a' + (i + 1), actual[0]);
        }
        else {
          assertNull(actual[0]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NTH()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NTH([ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NTH(null, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NTH(true, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NTH(4, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NTH(\"yes\", 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NTH({ }, 1)"); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], null)")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], false)")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], true)")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], '')")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], '1234')")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], [ ])")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], { \"foo\": true })")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ ], { })")); 
      
      assertEqual([ 1 ], getQueryResults("RETURN NTH([ 1, 2, 3 ], false)")); 
      assertEqual([ 2 ], getQueryResults("RETURN NTH([ 1, 2, 3 ], true)")); 
      assertEqual([ 3 ], getQueryResults("RETURN NTH([ 1, 2, 3 ], '2')")); 
      assertEqual([ null ], getQueryResults("RETURN NTH([ 1, 2, 3 ], '3')")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthEmptyCxx : function () {
      var i;

      for (i = -3; i <= 3; ++i) {                         
        var actual = getQueryResults("RETURN NOOPT(NTH([ ], @pos))", { pos: i });
        assertNull(actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthNegativeCxx : function () {
      var i;

      for (i = -3; i <= 3; ++i) {                         
        var actual = getQueryResults("RETURN NOOPT(NTH([ 1, 2, 3, 4 ], @pos))", { pos: i });
        if (i < 0) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(i + 1, actual[0]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthBoundsCxx : function () {
      var i;

      for (i = 0; i <= 10; ++i) {                         
        var actual = getQueryResults("RETURN NOOPT(NTH([ 'a1', 'a2', 'a3', 'a4', 'a5' ], @pos))", { pos: i });
        if (i < 5) {
          assertEqual('a' + (i + 1), actual[0]);
        }
        else {
          assertNull(actual[0]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nth function
////////////////////////////////////////////////////////////////////////////////

    testNthInvalidCxx : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(NTH())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(NTH([ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(NTH(null, 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(NTH(true, 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(NTH(4, 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(NTH(\"yes\", 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(NTH({ }, 1))"); 
      assertEqual([ null ], getQueryResults("RETURN NOOPT(NTH([ ], null))")); 
      assertEqual([ null ], getQueryResults("RETURN NOOPT(NTH([ ], false))")); 
      assertEqual([ null ], getQueryResults("RETURN NOOPT(NTH([ ], true))")); 
      assertEqual([ null ], getQueryResults("RETURN NOOPT(NTH([ ], ''))")); 
      assertEqual([ null ], getQueryResults("RETURN NOOPT(NTH([ ], '1234'))")); 
      assertEqual([ null ], getQueryResults("RETURN NOOPT(NTH([ ], [ ]))")); 
      assertEqual([ null ], getQueryResults("RETURN NOOPT(NTH([ ], { \"foo\": true }))")); 
      assertEqual([ null ], getQueryResults("RETURN NOOPT(NTH([ ], { }))")); 
      
      assertEqual([ 1 ], getQueryResults("RETURN NOOPT(NTH([ 1, 2, 3 ], false))")); 
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT(NTH([ 1, 2, 3 ], true))")); 
      assertEqual([ 3 ], getQueryResults("RETURN NOOPT(NTH([ 1, 2, 3 ], '2'))")); 
      assertEqual([ null ], getQueryResults("RETURN NOOPT(NTH([ 1, 2, 3 ], '3'))")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
////////////////////////////////////////////////////////////////////////////////

    testReverse1 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN REVERSE([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
////////////////////////////////////////////////////////////////////////////////

    testReverse2 : function () {
      var expected = [ [ "fox" ] ];
      var actual = getQueryResults("RETURN REVERSE([ \"fox\" ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
////////////////////////////////////////////////////////////////////////////////

    testReverse3 : function () {
      var expected = [ [ false, [ "fox", "jumped" ], { "quick" : "brown" }, "the" ] ];
      var actual = getQueryResults("RETURN REVERSE([ \"the\", { \"quick\" : \"brown\" }, [ \"fox\", \"jumped\" ], false ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
//////////////////////////////////////////////////////////////////////////////// 

    testReverse4 : function () {
      var expected = [ [ 1, 2, 3 ] ];
      var actual = getQueryResults("LET a = (FOR i IN [ 1, 2, 3 ] RETURN i) LET b = REVERSE(a) RETURN a");
      // make sure reverse does not modify the original value
      assertEqual(expected, actual);
      
      actual = getQueryResults("LET a = (FOR i IN [ 1, 2, 3] RETURN i) LET b = REVERSE(a) RETURN b");
      assertEqual([ expected[0].reverse() ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
//////////////////////////////////////////////////////////////////////////////// 

    testReverse5 : function () {
      let val = [
        ['eÜrCyeltÖM', 'MÖtleyCrÜe'],
        ['狗懶了過跳狸狐色棕的捷敏', '敏捷的棕色狐狸跳過了懶狗']
      ];
      for (let i = 0; i < 2; i++) {
        let v = val[i];
        assertEqual([v[0]], getQueryResults('RETURN REVERSE(@v)', {v: v[1]}));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse function
////////////////////////////////////////////////////////////////////////////////

    testReverseInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REVERSE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN REVERSE([ ], [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN REVERSE(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN REVERSE(true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN REVERSE(4)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN REVERSE({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUnique1 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN UNIQUE([ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUnique2 : function () {
      var expected = [ null, false, true, -1, 1, 2, 3, 4, 5, 6, 7, 8, 9, "FOX", "FoX", "Fox", "fox", [ 0 ], [ 1 ], { "the fox" : "jumped" } ]; 
      var actual = getQueryResults("FOR i IN UNIQUE([ 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, false, true, null, \"fox\", \"FOX\", \"Fox\", \"FoX\", [ 0 ], [ 1 ], { \"the fox\" : \"jumped\" } ]) SORT i RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUnique3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 7, 9, 42, -1, -33 ];
      var actual = getQueryResults("RETURN UNIQUE([ 1, -1, 1, 2, 3, -1, 2, 3, 4, 5, 1, 3, 9, 2, -1, 9, -33, 42, 7 ])");
      assertEqual(expected.sort(), actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUnique4 : function () {
      var expected = [ [1, 2, 3], [3, 2, 1], [2, 1, 3], [2, 3, 1], [1, 3, 2], [3, 1, 2] ];
      var actual = getQueryResults("RETURN UNIQUE([ [ 1, 2, 3 ], [ 3, 2, 1 ], [ 2, 1, 3 ], [ 2, 3, 1 ], [ 1, 2, 3 ], [ 1, 3, 2 ], [ 2, 3, 1 ], [ 3, 1, 2 ], [ 2 , 1, 3 ] ])");
      assertEqual(expected.sort(), actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUnique5 : function () {
      var expected = [ { "the fox" : "jumped" }, { "the fox" : "jumped over" }, { "over" : "the dog", "the fox" : "jumped" } ];

      var actual = getQueryResults("FOR i IN UNIQUE([ { \"the fox\" : \"jumped\" }, { \"the fox\" : \"jumped over\" }, { \"the fox\" : \"jumped\", \"over\" : \"the dog\" }, { \"over\" : \"the dog\", \"the fox\" : \"jumped\" } ]) SORT i RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique function
////////////////////////////////////////////////////////////////////////////////

    testUniqueInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNIQUE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNIQUE([ ], [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN UNIQUE(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN UNIQUE(true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN UNIQUE(4)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN UNIQUE(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN UNIQUE({ })"); 
    },

    testSorted : function () {
      var tests = [
        [ [ null, { b: 2 }, { a: 1 }, 1, "1", [], 2, "2", 3, "3", 0, false, -1, true, [2], [1] ], [ null, false, true, -1, 0, 1, 2, 3, "1", "2", "3", [], [1], [2], { b: 2 }, { a: 1 } ] ],
        [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ [ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1 ], [ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ [ 1, 10, 100, 1000, 99, 2, 7, 8, 13, 4, 242, 123 ], [ 1, 2, 4, 7, 8, 10, 13, 99, 100, 123, 242, 1000 ] ],
        [ [ 13, 1000, 99, 7, 8, 1, 10, 4, 242, 100, 123, 2 ], [ 1, 2, 4, 7, 8, 10, 13, 99, 100, 123, 242, 1000 ] ],
        [ [ "foo", "bar", "foo", "food", "baz", "bark", "foo", "sauron", "bar" ], [ "bar", "bar", "bark", "baz", "foo", "foo", "foo", "food", "sauron" ] ],
        [ [ 1, 2, 1, 2, 3, 4, -1, 2, -1, -2, 0 ], [ -2, -1, -1, 0, 1, 1, 2, 2, 2, 3, 4 ] ],
        [ [ true, true, false, null, false, true, null ], [ null, null, false, false, true, true, true ] ],
        [ [ -3.5, 12.4777, 12.477777, 12.436, 12.46777, -12.4777, 10000, 10000.1, 9999.9999, 9999.999 ], [ -12.4777, -3.5, 12.436, 12.46777, 12.4777, 12.477777, 9999.999, 9999.9999, 10000, 10000.1 ] ],
      ];

      tests.forEach(function(test) {
        var actual = getQueryResults("RETURN NOOPT(SORTED(" + JSON.stringify(test[0]) + "))");
        assertEqual([ test[1] ], actual);
        
        actual = getQueryResults("RETURN SORTED(" + JSON.stringify(test[0]) + ")");
        assertEqual([ test[1] ], actual);
      });
    },
    
    testSortedUnique : function () {
      var tests = [
        [ [ null, { b: 2 }, { a: 1 }, 1, "1", [], 2, "2", 3, "3", 0, false, -1, true, [2], [1] ], [ null, false, true, -1, 0, 1, 2, 3, "1", "2", "3", [], [1], [2], { b: 2 }, { a: 1 } ] ],
        [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 ], [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ [ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1 ], [ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ],
        [ [ 1, 10, 100, 1000, 99, 2, 7, 8, 13, 4, 242, 123 ], [ 1, 2, 4, 7, 8, 10, 13, 99, 100, 123, 242, 1000 ] ],
        [ [ 1, 4, 10, 100, 99, 1000, 99, 2, 1, 7, 8, 13, 4, 10, 2, 242, 123, 1000 ], [ 1, 2, 4, 7, 8, 10, 13, 99, 100, 123, 242, 1000 ] ],
        [ [ 13, 1000, 99, 7, 8, 1, 10, 4, 242, 100, 123, 2 ], [ 1, 2, 4, 7, 8, 10, 13, 99, 100, 123, 242, 1000 ] ],
        [ [ "foo", "bar", "foo", "food", "baz", "bark", "foo", "sauron", "bar" ], [ "bar", "bark", "baz", "foo", "food", "sauron" ] ],
        [ [ 1, 2, 1, 2, 3, 4, -1, 2, -1, -2, 0 ], [ -2, -1, 0, 1, 2, 3, 4 ] ],
        [ [ true, true, false, null, false, true, null ], [ null, false, true ] ],
        [ [ -3.5, 12.4777, 12.477777, 12.436, 12.46777, 1, 1.0, -12.4777, 10000, 10000.1, 9999.9999, 9999.999, 10000.0 ], [ -12.4777, -3.5, 1, 12.436, 12.46777, 12.4777, 12.477777, 9999.999, 9999.9999, 10000, 10000.1 ] ],
      ];

      tests.forEach(function(test) {
        var actual = getQueryResults("RETURN NOOPT(SORTED_UNIQUE(" + JSON.stringify(test[0]) + "))");
        assertEqual([ test[1] ], actual);
        
        actual = getQueryResults("RETURN SORTED_UNIQUE(" + JSON.stringify(test[0]) + ")");
        assertEqual([ test[1] ], actual);
      });
    },
    
    testCountUnique : function () {
      var tests = [
        [ [ null, { b: 2 }, { a: 1 }, 1, "1", [], 2, "2", 3, "3", 0, false, -1, true, [2], [1] ], 16 ],
        [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], 10 ],
        [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 ], 10 ],
        [ [ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1 ], 12 ],
        [ [ 1, 10, 100, 1000, 99, 2, 7, 8, 13, 4, 242, 123 ], 12 ],
        [ [ 1, 4, 10, 100, 99, 1000, 99, 2, 1, 7, 8, 13, 4, 10, 2, 242, 123, 1000 ], 12 ],
        [ [ 13, 1000, 99, 7, 8, 1, 10, 4, 242, 100, 123, 2 ], 12 ],
        [ [ "foo", "bar", "foo", "food", "baz", "bark", "foo", "sauron", "bar" ], 6 ],
        [ [ 1, 2, 1, 2, 3, 4, -1, 2, -1, -2, 0 ], 7 ],
        [ [ true, true, false, null, false, true, null ], 3 ],
        [ [ -3.5, 12.4777, 12.477777, 12.436, 12.46777, 1, 1.0, -12.4777, 10000, 10000.1, 9999.9999, 9999.999, 10000.0 ], 11 ]
      ];

      tests.forEach(function(test) {
        let actual = getQueryResults("RETURN COUNT_UNIQUE(" + JSON.stringify(test[0]) + ")");
        assertEqual([ test[1] ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test slice function
////////////////////////////////////////////////////////////////////////////////

    testSliceCxx : function () {
      var actual;
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ ], 0, 1))");
      assertEqual([ [ ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 0, 1))");
      assertEqual([ [ 1 ] ], actual);

      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 0, 2))");
      assertEqual([ [ 1, 2 ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 1, 2))");
      assertEqual([ [ 2, 3 ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 0))");
      assertEqual([ [ 1, 2, 3, 4, 5 ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 3))");
      assertEqual([ [ 4, 5 ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 0, -1))");
      assertEqual([ [ 1, 2, 3, 4 ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 0, -2))");
      assertEqual([ [ 1, 2, 3 ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 2, -1))");
      assertEqual([ [ 3, 4 ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 10))");
      assertEqual([ [ ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 1000))");
      assertEqual([ [ ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], -1000))");
      assertEqual([ [ 1, 2, 3, 4, 5 ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(SLICE([ 1, 2, 3, 4, 5 ], 1, -10))");
      assertEqual([ [ ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test slice function
////////////////////////////////////////////////////////////////////////////////

    testSliceInvalidCxx : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SLICE())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SLICE(true))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SLICE(1))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SLICE('foo'))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SLICE({ }))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SLICE([ ]))"); 
      assertEqual([ [ ] ], getQueryResults("RETURN NOOPT(SLICE([ ], { }))")); 
      assertEqual([ [ ] ], getQueryResults("RETURN NOOPT(SLICE([ ], true))")); 
      assertEqual([ [ ] ], getQueryResults("RETURN NOOPT(SLICE([ ], 'foo'))")); 
      assertEqual([ [ ] ], getQueryResults("RETURN NOOPT(SLICE([ ], [ ]))")); 
      assertEqual([ [ ] ], getQueryResults("RETURN NOOPT(SLICE([ ], { }))")); 
      assertEqual([ [ ] ], getQueryResults("RETURN NOOPT(SLICE([ ], 1, false))")); 
      assertEqual([ [ ] ], getQueryResults("RETURN NOOPT(SLICE([ ], 1, 'foo'))")); 
      assertEqual([ [ ] ], getQueryResults("RETURN NOOPT(SLICE([ ], 1, [ ]))")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace_nth function
////////////////////////////////////////////////////////////////////////////////

    testReplaceNthCxx : function () {

      var testArray = [
        null,
        true,
        1,
        3.5,
        [],
        [7, 6],
        { },
        { foo: 'bar'}
      ];

      for (let replaceIndex = 0;  replaceIndex <= testArray.length + 2; replaceIndex ++) {
        for (let replaceValue = 0;  replaceValue < testArray.length; replaceValue ++) {
          const msg = `Index: ${replaceIndex} ReplaceValue: ${replaceValue}`;
          let actual = getQueryResults("RETURN NOOPT(REPLACE_NTH(@testArray, @which, @replaceValue, @defaultValue))",
                                   {
                                     testArray: testArray,
                                     which: replaceIndex,
                                     replaceValue: testArray[replaceValue],
                                     defaultValue: testArray[replaceValue]
                                   }
                                  );
          if (replaceIndex === testArray.length + 2) {
            let expectValue = testArray[replaceValue];
            if (expectValue === undefined) {
              expectValue = null;
            }
            assertEqual(actual[0][testArray.length + 1], expectValue, msg);
          }
          assertEqual(actual[0][replaceIndex], testArray[replaceValue], msg);
        }
      }
      for (let replaceIndex = 0;  replaceIndex <= testArray.length; replaceIndex ++) {
        for (let replaceValue = 0;  replaceValue < testArray.length; replaceValue ++) {
          let actualReplaceIndex = testArray.length - replaceIndex -1 ;
          let replaceIndexValue = -(replaceIndex) - 1;
          if (actualReplaceIndex < 0) {
            actualReplaceIndex = 0;
          }
          const msg = `Index: ${replaceIndexValue} => ${actualReplaceIndex} ReplaceValue: ${replaceValue}`;
          let actual = getQueryResults("RETURN NOOPT(REPLACE_NTH(@testArray, @which, @replaceValue, @defaultValue))",
                                   {
                                     testArray: testArray,
                                     which: replaceIndexValue,
                                     replaceValue: testArray[replaceValue],
                                     defaultValue: testArray[replaceValue]
                                   }
                                      );
          assertEqual(actual[0][actualReplaceIndex], testArray[replaceValue], msg);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace_nth function
////////////////////////////////////////////////////////////////////////////////

    testReplaceNthInvalidCxx : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(REPLACE_NTH(null, null))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(REPLACE_NTH(['x'], null, { }))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(REPLACE_NTH([], null, { }))");
      assertEqual([ null ], getQueryResults("RETURN NOOPT(REPLACE_NTH(null, 1, 1))")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function
////////////////////////////////////////////////////////////////////////////////

    testLength1 : function () {
      var expected = [ 0, 0, 0 ];
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ ] RETURN q)) RETURN LENGTH(quarters)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function
////////////////////////////////////////////////////////////////////////////////

    testLength2 : function () {
      var expected = [ 4, 4, 4 ];
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] RETURN q)) RETURN LENGTH(quarters)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function
////////////////////////////////////////////////////////////////////////////////

    testLength3 : function () {
      var expected = [ 3, 2, 2, 1, 0 ];
      var actual = getQueryResults("FOR test IN [ { 'a' : 1, 'b' : 2, 'c' : null }, { 'baz' : [ 1, 2, 3, 4, 5 ], 'bar' : false }, { 'boom' : { 'bang' : false }, 'kawoom' : 0.0 }, { 'meow' : { } }, { } ] RETURN LENGTH(test)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function for strings
////////////////////////////////////////////////////////////////////////////////

    testLength4 : function () {
      var expected = [ 0, 1, 3, 3, 4, 5 ];
      var actual = getQueryResults("FOR test IN [ '', ' ', 'foo', 'bar', 'meow', 'mötör' ] RETURN LENGTH(test)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function for documents
////////////////////////////////////////////////////////////////////////////////

    testLength5 : function () {
      var expected = [ 0, 1, 3, 3, 4, 5 ];
      var actual = getQueryResults("FOR test IN [ { }, { foo: true }, { foo: 1, bar: 2, baz: 3 }, { one: { }, two: [ 1, 2, 3 ], three: { one: 1, two: 2 } }, { '1': 0, '2': 0, '3': 0, '4': 0 }, { abc: false, ABC: false, aBc: false, AbC: false, ABc: true } ] RETURN LENGTH(test)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test length function
////////////////////////////////////////////////////////////////////////////////

    testLengthInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LENGTH()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN LENGTH([ ], [ ])"); 
      assertEqual([ 0 ], getQueryResults("RETURN LENGTH(null)")); 
      assertEqual([ 1 ], getQueryResults("RETURN LENGTH(true)")); 
      assertEqual([ 0 ], getQueryResults("RETURN LENGTH(false)")); 
      assertEqual([ 1 ], getQueryResults("RETURN LENGTH(4)")); 
      assertEqual([ 2 ], getQueryResults("RETURN LENGTH(-4)")); 
      assertEqual([ 4 ], getQueryResults("RETURN LENGTH(-1.5)")); 
      assertEqual([ 1 ], getQueryResults("RETURN LENGTH('4')")); 
      assertEqual([ 0 ], getQueryResults("RETURN LENGTH('')")); 
      assertEqual([ 1 ], getQueryResults("RETURN LENGTH(' ')")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test keep function
////////////////////////////////////////////////////////////////////////////////
    
    testKeepInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN KEEP()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN KEEP({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP(null, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP(1, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP(false, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP(1, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP('bar', 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP('foo', 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN KEEP([ ], 'foo', { })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test keep function
////////////////////////////////////////////////////////////////////////////////
    
    testKeep : function () {
      var actual;

      actual = getQueryResults("FOR i IN [ { }, { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN KEEP(i, 'foo', 'bar', 'baz', [ 'meow' ], [ ])");
      assertEqual([ { }, { bar: 2, foo: 1, meow: 6 }, { foo: 0, meow: 2 }, { foo: null }, { foo: true }, { } ], actual);
      
      actual = getQueryResults("FOR i IN [ { }, { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN KEEP(i, [ 'foo', 'bar', 'baz', 'meow' ])");
      assertEqual([ { }, { bar: 2, foo: 1, meow: 6 }, { foo: 0, meow: 2 }, { foo: null }, { foo: true }, { } ], actual);
      
      actual = getQueryResults("FOR i IN [ { }, { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN KEEP(i, 'foo', 'bar', 'baz', 'meow')");
      assertEqual([ { }, { bar: 2, foo: 1, meow: 6 }, { foo: 0, meow: 2 }, { foo: null }, { foo: true }, { } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unset function
////////////////////////////////////////////////////////////////////////////////
    
    testUnsetInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSET()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSET({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET(null, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET(false, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET(1, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET(1, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET('bar', 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET('foo', 'bar')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET([ ], 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET('foo', 'foo')"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unset function
////////////////////////////////////////////////////////////////////////////////
    
    testUnset : function () {
      var expected = [ { bang: 5, goof: 4, moo: 3 }, { goof: 1 }, { }, { }, { goof: null } ];
      var actual;

      actual = getQueryResults("FOR i IN [ { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN UNSET(i, 'foo', 'bar', 'baz', [ 'meow' ], [ ])");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR i IN [ { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN UNSET(i, [ 'foo', 'bar', 'baz', 'meow' ])");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR i IN [ { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN UNSET(i, 'foo', 'bar', 'baz', 'meow')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test keep function
////////////////////////////////////////////////////////////////////////////////
    
    testKeepCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(KEEP())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(KEEP({ }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(KEEP(null, 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(KEEP(1, 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(KEEP(false, 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(KEEP(1, 'foo'))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(KEEP('bar', 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(KEEP('foo', 'foo'))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(KEEP([ ], 'foo', { }))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test keep function
////////////////////////////////////////////////////////////////////////////////
    
    testCxxKeep : function () {
      var actual;

      actual = getQueryResults("FOR i IN [ { }, { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN NOOPT(KEEP(i, 'foo', 'bar', 'baz', [ 'meow' ], [ ]))");
      assertEqual([ { }, { bar: 2, foo: 1, meow: 6 }, { foo: 0, meow: 2 }, { foo: null }, { foo: true }, { } ], actual);
      
      actual = getQueryResults("FOR i IN [ { }, { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN NOOPT(KEEP(i, [ 'foo', 'bar', 'baz', 'meow' ]))");
      assertEqual([ { }, { bar: 2, foo: 1, meow: 6 }, { foo: 0, meow: 2 }, { foo: null }, { foo: true }, { } ], actual);
      
      actual = getQueryResults("FOR i IN [ { }, { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN NOOPT(KEEP(i, 'foo', 'bar', 'baz', 'meow'))");
      assertEqual([ { }, { bar: 2, foo: 1, meow: 6 }, { foo: 0, meow: 2 }, { foo: null }, { foo: true }, { } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unset function
////////////////////////////////////////////////////////////////////////////////
    
    testUnsetCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(UNSET())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(UNSET({ }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNSET(null, 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNSET(false, 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNSET(1, 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNSET(1, 'foo'))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNSET('bar', 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNSET('foo', 'bar'))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNSET([ ], 1))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNSET('foo', 'foo'))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unset function
////////////////////////////////////////////////////////////////////////////////
    
    testUnsetCxx : function () {
      var expected = [ { bang: 5, goof: 4, moo: 3 }, { goof: 1 }, { }, { }, { goof: null } ];
      var actual;

      actual = getQueryResults("FOR i IN [ { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN NOOPT(UNSET(i, 'foo', 'bar', 'baz', [ 'meow' ], [ ]))");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR i IN [ { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN NOOPT(UNSET(i, [ 'foo', 'bar', 'baz', 'meow' ]))");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR i IN [ { foo: 1, bar: 2, moo: 3, goof: 4, bang: 5, meow: 6 }, { foo: 0, goof: 1, meow: 2 }, { foo: null }, { foo: true }, { goof: null } ] RETURN NOOPT(UNSET(i, 'foo', 'bar', 'baz', 'meow'))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unset_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testUnsetRecursiveInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSET_RECURSIVE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSET_RECURSIVE({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE(null, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE(false, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE(1, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE(1, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE('bar', 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE('foo', 'bar')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE([ ], 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE('foo', 'foo')"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unset_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testUnsetRecursive : function () {
      var actual, expected;

      expected = { foo: { bar: { baz: 1 }, baz: 2 }, baz: 3 };
      actual = getQueryResults("RETURN UNSET_RECURSIVE({ foo: { bar: { baz: 1 }, baz: 2 }, baz: 3 }, 'moo')");
      assertEqualObj(expected, actual[0]);
 
      expected = { baz: 3 };
      actual = getQueryResults("RETURN UNSET_RECURSIVE({ foo: { bar: { baz: 1 }, baz: 2 }, baz: 3 }, 'foo')");
      assertEqualObj(expected, actual[0]);

      expected = { foo: { baz: 2}, baz: 3 };
      actual = getQueryResults("RETURN UNSET_RECURSIVE({ foo: { bar: { baz: 1 }, baz: 2 }, baz: 3 }, 'bar')");
      assertEqualObj(expected, actual[0]);

      expected = { foo: { bar: { } } };
      actual = getQueryResults("RETURN UNSET_RECURSIVE({ foo: { bar: { baz: 1 }, baz: 2 }, baz: 3 }, 'baz')");
      assertEqualObj(expected, actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unset_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testUnsetRecursiveCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSET_RECURSIVE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNSET_RECURSIVE({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE(null, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE(false, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE(1, 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE(1, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE('bar', 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE('foo', 'bar')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE([ ], 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNSET_RECURSIVE('foo', 'foo')"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unset_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testUnsetRecursiveCxx : function () {
      var actual, expected;

      expected = { foo: { bar: { baz: 1 }, baz: 2 }, baz: 3 };
      actual = getQueryResults("RETURN NOOPT(UNSET_RECURSIVE({ foo: { bar: { baz: 1 }, baz: 2 }, baz: 3 }, 'moo'))");
      assertEqualObj(expected, actual[0]);
      
      expected = { baz: 3 };
      actual = getQueryResults("RETURN NOOPT(UNSET_RECURSIVE({ foo: { bar: { baz: 1 }, baz: 2 }, baz: 3 }, 'foo'))");
      assertEqualObj(expected, actual[0]);

      expected = { foo: { baz: 2 }, baz: 3 };
      actual = getQueryResults("RETURN NOOPT(UNSET_RECURSIVE({ foo: { bar: { baz: 1 }, baz: 2 }, baz: 3 }, 'bar'))");
      assertEqualObj(expected, actual[0]);

      expected = { foo: { bar: { } } };
      actual = getQueryResults("RETURN NOOPT(UNSET_RECURSIVE({ foo: { bar: { baz: 1 }, baz: 2 }, baz: 3 }, 'baz'))");
      assertEqualObj(expected, actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge1 : function () {
      var expected = [ { "quarter" : 1, "year" : 2010 }, { "quarter" : 2, "year" : 2010 }, { "quarter" : 3, "year" : 2010 }, { "quarter" : 4, "year" : 2010 }, { "quarter" : 1, "year" : 2011 }, { "quarter" : 2, "year" : 2011 }, { "quarter" : 3, "year" : 2011 }, { "quarter" : 4, "year" : 2011 }, { "quarter" : 1, "year" : 2012 }, { "quarter" : 2, "year" : 2012 }, { "quarter" : 3, "year" : 2012 }, { "quarter" : 4, "year" : 2012 } ]; 
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FOR quarter IN [ 1, 2, 3, 4 ] return MERGE({ \"year\" : year }, { \"quarter\" : quarter })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge2 : function () {
      var expected = [ { "age" : 15, "isAbove18" : false, "name" : "John" }, { "age" : 19, "isAbove18" : true, "name" : "Corey" } ];
      var actual = getQueryResults("FOR u IN [ { \"name\" : \"John\", \"age\" : 15 }, { \"name\" : \"Corey\", \"age\" : 19 } ] return MERGE(u, { \"isAbove18\" : u.age >= 18 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge3 : function () {
      var expected = [ { "age" : 15, "id" : 9, "name" : "John" }, { "age" : 19, "id" : 9, "name" : "Corey" } ];
      var actual = getQueryResults("FOR u IN [ { \"id\" : 100, \"name\" : \"John\", \"age\" : 15 }, { \"id\" : 101, \"name\" : \"Corey\", \"age\" : 19 } ] return MERGE(u, { \"id\" : 9 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge4 : function () {
      var expected = [ { "age" : 15, "id" : 33, "name" : "foo" }, { "age" : 19, "id" : 33, "name" : "foo" } ];
      var actual = getQueryResults("FOR u IN [ { \"id\" : 100, \"name\" : \"John\", \"age\" : 15 }, { \"id\" : 101, \"name\" : \"Corey\", \"age\" : 19 } ] return MERGE(u, { \"id\" : 9 }, { \"name\" : \"foo\", \"id\" : 33 })");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge5 : function () {
      var expected = [ { "age" : 15, "id" : 33, "sub": {"newer": "value"} } ];
      var actual = getQueryResults("RETURN MERGE({ \"id\" : 100, \"age\" : 15, \"sub\": {\"old\": \"value\" }}, { \"id\" : 9 }, { \"id\" : 33, \"sub\": {\"newer\": \"value\" }})");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeCxx1 : function () {
      var expected = [ { "quarter" : 1, "year" : 2010 }, { "quarter" : 2, "year" : 2010 }, { "quarter" : 3, "year" : 2010 }, { "quarter" : 4, "year" : 2010 }, { "quarter" : 1, "year" : 2011 }, { "quarter" : 2, "year" : 2011 }, { "quarter" : 3, "year" : 2011 }, { "quarter" : 4, "year" : 2011 }, { "quarter" : 1, "year" : 2012 }, { "quarter" : 2, "year" : 2012 }, { "quarter" : 3, "year" : 2012 }, { "quarter" : 4, "year" : 2012 } ]; 
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FOR quarter IN [ 1, 2, 3, 4 ] return NOOPT(MERGE({ \"year\" : year }, { \"quarter\" : quarter }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeCxx2 : function () {
      var expected = [ { "age" : 15, "isAbove18" : false, "name" : "John" }, { "age" : 19, "isAbove18" : true, "name" : "Corey" } ];
      var actual = getQueryResults("FOR u IN [ { \"name\" : \"John\", \"age\" : 15 }, { \"name\" : \"Corey\", \"age\" : 19 } ] return NOOPT(MERGE(u, { \"isAbove18\" : u.age >= 18 }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeCxx3 : function () {
      var expected = [ { "age" : 15, "id" : 9, "name" : "John" }, { "age" : 19, "id" : 9, "name" : "Corey" } ];
      var actual = getQueryResults("FOR u IN [ { \"id\" : 100, \"name\" : \"John\", \"age\" : 15 }, { \"id\" : 101, \"name\" : \"Corey\", \"age\" : 19 } ] return NOOPT(MERGE(u, { \"id\" : 9 }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeCxx4 : function () {
      var expected = [ { "age" : 15, "id" : 33, "name" : "foo" }, { "age" : 19, "id" : 33, "name" : "foo" } ];
      var actual = getQueryResults("FOR u IN [ { \"id\" : 100, \"name\" : \"John\", \"age\" : 15 }, { \"id\" : 101, \"name\" : \"Corey\", \"age\" : 19 } ] return NOOPT(MERGE(u, { \"id\" : 9 }, { \"name\" : \"foo\", \"id\" : 33 }))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeCxx5 : function () {
      var expected = [ { "age" : 15, "id" : 33, "sub": {"newer": "value"} } ];
      var actual = getQueryResults("RETURN NOOPT(MERGE({ \"id\" : 100, \"age\" : 15, \"sub\": {\"old\": \"value\" }}, { \"id\" : 9 }, { \"id\" : 33, \"sub\": {\"newer\": \"value\" }}))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeArrayCxx : function () {
      var expected = [ { "abc" : [ 1, 2, 3 ], "chicken" : "is a language", "empty" : false, "foo" : "bar", "foobar" : "baz", "quarter" : 1, "quux" : 123, "value" : true, "year" : 2010 } ];
      var actual = getQueryResults("RETURN NOOPT(MERGE([ { quarter: 1, year: 2010 }, { foo: 'bar' }, { chicken: 'is a language' }, { }, { foobar: 'baz' }, { quux: 123, value: true }, { empty: false }, { }, { abc: [1,2,3] } ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////

    testMergeInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MERGE()"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(null, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(true, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(3, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE(\"yes\", { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE({ }, { }, [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////

    testMergeCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MERGE())"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE({ }, null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE({ }, true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE({ }, 3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE({ }, \"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE({ }, [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE(null, { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE(true, { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE(3, { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE(\"yes\", { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE({ }, { }, null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE({ }, { }, true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE({ }, { }, 3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE({ }, { }, \"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE({ }, { }, [ ]))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive1 : function () {
      var doc1 = "{ \"black\" : { \"enabled\" : true, \"visible\": false }, \"white\" : { \"enabled\" : true}, \"list\" : [ 1, 2, 3, 4, 5 ] }";
      var doc2 = "{ \"black\" : { \"enabled\" : false }, \"list\": [ 6, 7, 8, 9, 0 ] }";

      var expected = [ { "black" : { "enabled" : false, "visible" : false }, "list" : [ 6, 7, 8, 9, 0 ], "white" : { "enabled" : true } } ];

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc1 + ", " + doc2 + "))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive2 : function () {
      var doc1 = "{ \"black\" : { \"enabled\" : true, \"visible\": false }, \"white\" : { \"enabled\" : true}, \"list\" : [ 1, 2, 3, 4, 5 ] }";
      var doc2 = "{ \"black\" : { \"enabled\" : false }, \"list\": [ 6, 7, 8, 9, 0 ] }";

      var expected = [ { "black" : { "enabled" : true, "visible" : false }, "list" : [ 1, 2, 3, 4, 5 ], "white" : { "enabled" : true } } ];

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc2 + ", " + doc1 + ")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc2 + ", " + doc1 + "))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive3 : function () {
      var doc1 = "{ \"a\" : 1, \"b\" : 2, \"c\" : 3 }";

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc1 + ")");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc1 + ", " + doc1 + ")");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc1 + ", " + doc1 + ", " + doc1 + ")");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);

      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc1 + ", " + doc1 + "))");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
      
      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc1 + ", " + doc1 + ", " + doc1 + "))");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
      
      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc1 + ", " + doc1 + ", " + doc1 + ", " + doc1 + "))");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive4 : function () {
      var doc1 = "{ \"a\" : 1, \"b\" : 2, \"c\" : 3 }";
      var doc2 = "{ \"a\" : 2, \"b\" : 3, \"c\" : 4 }";
      var doc3 = "{ \"a\" : 3, \"b\" : 4, \"c\" : 5 }";

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ", " + doc3 + ")");
      assertEqual([ { "a" : 3, "b" : 4, "c" : 5 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc3 + ", " + doc2 + ")");
      assertEqual([ { "a" : 2, "b" : 3, "c" : 4 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc2 + ", " + doc3 + ", " + doc1 + ")");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc3 + ", " + doc1 + ", " + doc2 + ")");
      assertEqual([ { "a" : 2, "b" : 3, "c" : 4 } ], actual);

      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ", " + doc3 + "))");
      assertEqual([ { "a" : 3, "b" : 4, "c" : 5 } ], actual);
      
      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc1 + ", " + doc3 + ", " + doc2 + "))");
      assertEqual([ { "a" : 2, "b" : 3, "c" : 4 } ], actual);
      
      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc2 + ", " + doc3 + ", " + doc1 + "))");
      assertEqual([ { "a" : 1, "b" : 2, "c" : 3 } ], actual);
      
      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc3 + ", " + doc1 + ", " + doc2 + "))");
      assertEqual([ { "a" : 2, "b" : 3, "c" : 4 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive5 : function () {
      var doc1 = "{ \"a\" : 1, \"b\" : 2, \"c\" : 3 }";
      var doc2 = "{ \"1\" : 7, \"b\" : 8, \"y\" : 9 }";
      var doc3 = "{ \"x\" : 4, \"y\" : 5, \"z\" : 6 }";

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ", " + doc3 + ")");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 8, "c" : 3, "x" : 4, "y": 5, "z" : 6 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc3 + ", " + doc2 + ")");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 8, "c" : 3, "x" : 4, "y": 9, "z" : 6 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc2 + ", " + doc3 + ", " + doc1 + ")");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 2, "c" : 3, "x" : 4, "y": 5, "z" : 6 } ], actual);
      
      actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc3 + ", " + doc1 + ", " + doc2 + ")");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 8, "c" : 3, "x" : 4, "y": 9, "z" : 6 } ], actual);

      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ", " + doc3 + "))");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 8, "c" : 3, "x" : 4, "y": 5, "z" : 6 } ], actual);
      
      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc1 + ", " + doc3 + ", " + doc2 + "))");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 8, "c" : 3, "x" : 4, "y": 9, "z" : 6 } ], actual);
      
      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc2 + ", " + doc3 + ", " + doc1 + "))");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 2, "c" : 3, "x" : 4, "y": 5, "z" : 6 } ], actual);
      
      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc3 + ", " + doc1 + ", " + doc2 + "))");
      assertEqual([ { "1" : 7, "a" : 1, "b" : 8, "c" : 3, "x" : 4, "y": 9, "z" : 6 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testMergeRecursive6 : function () {
      var doc1 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"DE\" : { \"city\" : \"Cologne\" } } } } }";
      var doc2 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"DE\" : { \"city\" : \"Frankfurt\" } } } } }";
      var doc3 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"DE\" : { \"city\" : \"Munich\" } } } } }";
      var doc4 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"UK\" : { \"city\" : \"Manchester\" } } } } }";
      var doc5 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"UK\" : { \"city\" : \"London\" } } } } }";
      var doc6 = "{ \"continent\" : { \"Europe\" : { \"country\" : { \"FR\" : { \"city\" : \"Paris\" } } } } }";
      var doc7 = "{ \"continent\" : { \"Asia\" : { \"country\" : { \"CN\" : { \"city\" : \"Beijing\" } } } } }";
      var doc8 = "{ \"continent\" : { \"Asia\" : { \"country\" : { \"CN\" : { \"city\" : \"Shanghai\" } } } } }";
      var doc9 = "{ \"continent\" : { \"Asia\" : { \"country\" : { \"JP\" : { \"city\" : \"Tokyo\" } } } } }";
      var doc10 = "{ \"continent\" : { \"Australia\" : { \"country\" : { \"AU\" : { \"city\" : \"Sydney\" } } } } }";
      var doc11 ="{ \"continent\" : { \"Australia\" : { \"country\" : { \"AU\" : { \"city\" : \"Melbourne\" } } } } }";
      var doc12 ="{ \"continent\" : { \"Africa\" : { \"country\" : { \"EG\" : { \"city\" : \"Cairo\" } } } } }";

      var actual = getQueryResults("RETURN MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ", " + doc3 + ", " + doc4 + ", " + doc5 + ", " + doc6 + ", " + doc7 + ", " + doc8 + ", " + doc9 + ", " + doc10 + ", " + doc11 + ", " + doc12 + ")");

      assertEqual([ { "continent" : { 
        "Europe" : { "country" : { "DE" : { "city" : "Munich" }, "UK" : { "city" : "London" }, "FR" : { "city" : "Paris" } } }, 
        "Asia" : { "country" : { "CN" : { "city" : "Shanghai" }, "JP" : { "city" : "Tokyo" } } }, 
        "Australia" : { "country" : { "AU" : { "city" : "Melbourne" } } }, 
        "Africa" : { "country" : { "EG" : { "city" : "Cairo" } } } 
      } } ], actual);

      actual = getQueryResults("RETURN NOOPT(MERGE_RECURSIVE(" + doc1 + ", " + doc2 + ", " + doc3 + ", " + doc4 + ", " + doc5 + ", " + doc6 + ", " + doc7 + ", " + doc8 + ", " + doc9 + ", " + doc10 + ", " + doc11 + ", " + doc12 + "))");

      assertEqual([ { "continent" : { 
        "Europe" : { "country" : { "DE" : { "city" : "Munich" }, "UK" : { "city" : "London" }, "FR" : { "city" : "Paris" } } }, 
        "Asia" : { "country" : { "CN" : { "city" : "Shanghai" }, "JP" : { "city" : "Tokyo" } } }, 
        "Australia" : { "country" : { "AU" : { "city" : "Melbourne" } } }, 
        "Africa" : { "country" : { "EG" : { "city" : "Cairo" } } } 
      } } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////

    testMergeRecursiveInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MERGE_RECURSIVE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MERGE_RECURSIVE({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(null, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(true, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(3, { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE(\"yes\", { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE([ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN MERGE_RECURSIVE({ }, { }, [ ])"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }, null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }, true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }, 3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }, \"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }, [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE(null, { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE(true, { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE(3, { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE(\"yes\", { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE([ ], { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }, { }, null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }, { }, true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }, { }, 3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }, { }, \"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(MERGE_RECURSIVE({ }, { }, [ ]))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test translate function
////////////////////////////////////////////////////////////////////////////////
    
    testTranslate1 : function () {
      var tests = [
        { value: "foo", lookup: { }, expected: "foo" },
        { value: "foo", lookup: { foo: "bar" }, expected: "bar" },
        { value: "foo", lookup: { bar: "foo" }, expected: "foo" },
        { value: 1, lookup: { 1: "one", 2: "two" }, expected: "one" },
        { value: 1, lookup: { "1": "one", "2": "two" }, expected: "one" },
        { value: "1", lookup: { 1: "one", 2: "two" }, expected: "one" },
        { value: "1", lookup: { "1": "one", "2": "two" }, expected: "one" },
        { value: null, lookup: { foo: "bar", bar: "foo" }, expected: null },
        { value: "foobar", lookup: { foo: "bar", bar: "foo" }, expected: "foobar" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: "replaced!" }, expected: "replaced!" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: null }, expected: null },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: [ 1, 2, 3 ] }, expected: [ 1, 2, 3 ] },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: { thefoxx: "is great" } }, expected: { thefoxx: "is great" } },
        { value: "one", lookup: { one: "two", two: "three", three: "four" }, expected: "two" },
        { value: null, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: null },
        { value: false, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: false },
        { value: 0, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: 0 },
        { value: "", lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: "" },
        { value: "CGN", lookup: { "DUS" : "Duessldorf", "FRA" : "Frankfurt", "CGN" : "Cologne" }, expected: "Cologne" }
      ];

      tests.forEach(function(t) {
        var actual = getQueryResults("RETURN TRANSLATE(@value, @lookup)", { value: t.value, lookup: t.lookup });
        assertEqual(t.expected, actual[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test translate function
////////////////////////////////////////////////////////////////////////////////
    
    testTranslate2 : function () {
      var i, lookup = { };
      for (i = 0; i < 100; ++i) {
        lookup["test" + i] = "the quick brown Foxx:" + i;
      }

      for (i = 0; i < 100; ++i) {
        var actual = getQueryResults("RETURN TRANSLATE(@value, @lookup)", { value: "test" + i, lookup: lookup });
        assertEqual(lookup["test" + i], actual[0]);
        assertEqual("the quick brown Foxx:" + i, actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test translate function
////////////////////////////////////////////////////////////////////////////////
    
    testTranslate3 : function () {
      var i, lookup = { };
      for (i = 0; i < 100; ++i) {
        lookup[i] = i * i;
      }

      for (i = 0; i < 100; ++i) {
        var actual = getQueryResults("RETURN TRANSLATE(@value, @lookup, @def)", { value: i, lookup: lookup, def: "fail!" });
        assertEqual(lookup[i], actual[0]);
        assertEqual(i * i, actual[0]);
        
        actual = getQueryResults("RETURN TRANSLATE(@value, @lookup, @def)", { value: "test" + i, lookup: lookup, def: "fail" + i });
        assertEqual("fail" + i, actual[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test translate function
////////////////////////////////////////////////////////////////////////////////
    
    testTranslateDefault : function () {
      var tests = [
        { value: "foo", lookup: { }, expected: "bar", def: "bar" },
        { value: "foo", lookup: { foo: "bar" }, expected: "bar", def: "lol" },
        { value: "foo", lookup: { bar: "foo" }, expected: "lol", def: "lol" },
        { value: 1, lookup: { 1: "one", 2: "two" }, expected: "one", def: "foobar" },
        { value: 1, lookup: { "1": "one", "2": "two" }, expected: "one", def: "test" },
        { value: 1, lookup: { "3": "one", "2": "two" }, expected: "test", def: "test" },
        { value: "1", lookup: { 1: "one", 2: "two" }, expected: "one", def: "test" },
        { value: "1", lookup: { 3: "one", 2: "two" }, expected: "test", def: "test" },
        { value: "1", lookup: { "1": "one", "2": "two" }, expected: "one", def: "test" },
        { value: "1", lookup: { "3": "one", "2": "two" }, expected: "test", def: "test" },
        { value: null, lookup: { foo: "bar", bar: "foo" }, expected: 1, def: 1 },
        { value: null, lookup: { foo: "bar", bar: "foo" }, expected: "foobar", def: "foobar" },
        { value: "foobar", lookup: { foo: "bar", bar: "foo" }, expected: "foobart", def: "foobart" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: "replaced!" }, expected: "replaced!", def: "foobart" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: null }, expected: null, def: "foobaz" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: [ 1, 2, 3 ] }, expected: [ 1, 2, 3 ], def: null },
        { value: "foobaz", lookup: { foobar: "bar" }, expected: null, def: null },
        { value: "foobaz", lookup: { }, expected: "Foxx", def: "Foxx" },
        { value: "foobaz", lookup: { foobar: "bar", foobaz: { thefoxx: "is great" } }, expected: { thefoxx: "is great" }, def: null },
        { value: "one", lookup: { one: "two", two: "three", three: "four" }, expected: "two", def: "foo" },
        { value: null, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: "bla", def: "bla" },
        { value: false, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: true, def: true },
        { value: 0, lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: 42, def: 42 },
        { value: "", lookup: { "foo": "one", " ": "bar", "empty": 3 }, expected: "three", def: "three" },
        { value: "CGN", lookup: { "DUS" : "Duessldorf", "FRA" : "Frankfurt", "MUC" : "Munich" }, expected: "Cologne", def: "Cologne" }
      ];

      tests.forEach(function(t) {
        var actual = getQueryResults("RETURN TRANSLATE(@value, @lookup, @def)", { value: t.value, lookup: t.lookup, def: t.def });
        assertEqual(t.expected, actual[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge_recursive function
////////////////////////////////////////////////////////////////////////////////

    testTranslateInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TRANSLATE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TRANSLATE('foo')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN TRANSLATE('foo', { }, '', 'baz')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE({ }, 'foo')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', 1)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', '')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN TRANSLATE('', [])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnion1 : function () {
      var expected = [ [ 1, 2, 3, 1, 2, 3 ] ];
      var actual = getQueryResults("RETURN UNION([ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnion2 : function () {
      var expected = [ [ 1, 2, 3, 3, 2, 1 ] ];
      var actual = getQueryResults("RETURN UNION([ 1, 2, 3 ], [ 3, 2, 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnion3 : function () {
      var expected = [ "Fred", "John", "John", "Amy" ];
      var actual = getQueryResults("FOR u IN UNION([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"]) RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////

    testUnionInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNION()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNION([ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION([ ], [ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(null, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(true, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(3, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION(\"yes\", [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION({ }, [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function indexed access
////////////////////////////////////////////////////////////////////////////////
    
    testUnionIndexedAccess1 : function () {
      var expected = [ "Fred" ];
      var actual = getQueryResults("RETURN UNION([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"])[0]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function indexed access
////////////////////////////////////////////////////////////////////////////////
    
    testUnionIndexedAccess2 : function () {
      var expected = [ "John" ];
      var actual = getQueryResults("RETURN UNION([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"])[1]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function indexed access
////////////////////////////////////////////////////////////////////////////////
    
    testUnionIndexedAccess3 : function () {
      var expected = [ "bar" ];
      var actual = getQueryResults("RETURN UNION([ { title : \"foo\" } ], [ { title : \"bar\" } ])[1].title");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct1 : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("RETURN UNION_DISTINCT([ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct2 : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("RETURN UNION_DISTINCT([ 1, 2, 3 ], [ 3, 2, 1 ])");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct3 : function () {
      var expected = [ "Fred", "John", "Amy" ];
      var actual = getQueryResults("FOR u IN UNION_DISTINCT([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"]) RETURN u");
      assertEqual(expected.sort(), actual.sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6 ];
      var actual = getQueryResults("RETURN UNION_DISTINCT([ 1, 2, 3 ], [ 3, 2, 1 ], [ 4 ], [ 5, 6, 1 ])");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct5 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN UNION_DISTINCT([ ], [ ], [ ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinct6 : function () {
      var expected = [ false, true ];
      var actual = getQueryResults("RETURN UNION_DISTINCT([ ], [ false ], [ ], [ true ])");
      assertEqual(expected.sort(), actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////

    testUnionDistinctInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNION_DISTINCT()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN UNION_DISTINCT([ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], \"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT([ ], [ ], { })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(null, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(true, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(3, [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT(\"yes\", [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN UNION_DISTINCT({ }, [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionCxx1 : function () {
      var expected = [ [ 1, 2, 3, 1, 2, 3 ] ];
      var actual = getQueryResults("RETURN NOOPT(UNION([ 1, 2, 3 ], [ 1, 2, 3 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionCxx2 : function () {
      var expected = [ [ 1, 2, 3, 3, 2, 1 ] ];
      var actual = getQueryResults("RETURN NOOPT(UNION([ 1, 2, 3 ], [ 3, 2, 1 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionCxx3 : function () {
      var expected = [ "Fred", "John", "John", "Amy" ];
      var actual = getQueryResults("FOR u IN NOOPT(UNION([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"])) RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function
////////////////////////////////////////////////////////////////////////////////

    testUnionCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(UNION())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(UNION([ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION([ ], null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION([ ], true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION([ ], 3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION([ ], \"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION([ ], { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION([ ], [ ], null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION([ ], [ ], true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION([ ], [ ], 3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION([ ], [ ], \"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION([ ], [ ], { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION(null, [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION(true, [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION(3, [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION(\"yes\", [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION({ }, [ ]))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function indexed access
////////////////////////////////////////////////////////////////////////////////
    
    testUnionCxxIndexedAccess1 : function () {
      var expected = [ "Fred" ];
      var actual = getQueryResults("RETURN NOOPT(UNION([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"]))[0]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function indexed access
////////////////////////////////////////////////////////////////////////////////
    
    testUnionCxxIndexedAccess2 : function () {
      var expected = [ "John" ];
      var actual = getQueryResults("RETURN NOOPT(UNION([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"]))[1]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union function indexed access
////////////////////////////////////////////////////////////////////////////////
    
    testUnionCxxIndexedAccess3 : function () {
      var expected = [ "bar" ];
      var actual = getQueryResults("RETURN NOOPT(UNION([ { title : \"foo\" } ], [ { title : \"bar\" } ]))[1].title");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinctCxx1 : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("RETURN NOOPT(UNION_DISTINCT([ 1, 2, 3 ], [ 1, 2, 3 ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinctCxx2 : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("RETURN NOOPT(UNION_DISTINCT([ 1, 2, 3 ], [ 3, 2, 1 ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinctCxx3 : function () {
      var expected = [ "Amy", "Fred", "John" ];
      var actual = getQueryResults("FOR u IN NOOPT(UNION_DISTINCT([ \"Fred\", \"John\" ], [ \"John\", \"Amy\"])) RETURN u");
      assertEqual(expected, actual.sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinctCxx4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6 ];
      var actual = getQueryResults("RETURN NOOPT(UNION_DISTINCT([ 1, 2, 3 ], [ 3, 2, 1 ], [ 4 ], [ 5, 6, 1 ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinctCxx5 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN NOOPT(UNION_DISTINCT([ ], [ ], [ ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////
    
    testUnionDistinctCxx6 : function () {
      var expected = [ false, true ];
      var actual = getQueryResults("RETURN NOOPT(UNION_DISTINCT([ ], [ false ], [ ], [ true ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test union_distinct function
////////////////////////////////////////////////////////////////////////////////

    testUnionDistinctCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ], null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ], true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ], 3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ], \"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ], { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ], [ ], null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ], [ ], true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ], [ ], 3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ], [ ], \"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT([ ], [ ], { }))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT(null, [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT(true, [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT(3, [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT(\"yes\", [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(UNION_DISTINCT({ }, [ ]))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange1 : function () {
      var expected = [ [ 1 ] ];
      var actual = getQueryResults("RETURN RANGE(1, 1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange2 : function () {
      var expected = [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ];
      var actual = getQueryResults("RETURN RANGE(1, 10)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange3 : function () {
      var expected = [ [ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ];
      var actual = getQueryResults("RETURN RANGE(-1, 10)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange4 : function () {
      var expected = [ [ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1 ] ];
      var actual = getQueryResults("RETURN RANGE(10, -1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange5 : function () {
      var expected = [ [ 0 ] ];
      var actual = getQueryResults("RETURN RANGE(0, 0)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange6 : function () {
      var expected = [ [ 10 ] ];
      var actual = getQueryResults("RETURN RANGE(10, 10, 5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange7 : function () {
      var expected = [ [ 10, 15, 20, 25, 30 ] ];
      var actual = getQueryResults("RETURN RANGE(10, 30, 5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange8 : function () {
      var expected = [ [ 30, 25, 20, 15, 10 ] ];
      var actual = getQueryResults("RETURN RANGE(30, 10, -5)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRange9 : function () {
      var expected = [ [ 3.4, 4.6, 5.8, 7.0, 8.2 ] ];
      var actual = getQueryResults("RETURN RANGE(3.4, 8.9, 1.2)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////

    testRangeInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RANGE()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RANGE(1)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN RANGE(1, 2, 3, 4)"); 
      assertEqual([ [ 0, 1, 2 ] ], getQueryResults("RETURN RANGE(null, 2)"));
      assertEqual([ [ 0 ] ], getQueryResults("RETURN RANGE(null, null)"));
      assertEqual([ [ 0, 1, 2 ] ], getQueryResults("RETURN RANGE(false, 2)"));
      assertEqual([ [ 1, 2 ] ], getQueryResults("RETURN RANGE(true, 2)"));
      assertEqual([ [ 1 ] ], getQueryResults("RETURN RANGE(1, true)"));
      assertEqual([ [ 1, 0 ] ], getQueryResults("RETURN RANGE(1, [ ])"));
      assertEqual([ [ 1, 0 ] ], getQueryResults("RETURN RANGE(1, { })"));
      
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(1, 1, 0)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(-1, -1, 0)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(1, -1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN RANGE(-1, 1, -1)");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRangeCxx1 : function () {
      var expected = [ [ 1 ] ];
      var actual = getQueryResults("RETURN NOOPT(RANGE(1, 1))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRangeCxx2 : function () {
      var expected = [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ];
      var actual = getQueryResults("RETURN NOOPT(RANGE(1, 10))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRangeCxx3 : function () {
      var expected = [ [ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] ];
      var actual = getQueryResults("RETURN NOOPT(RANGE(-1, 10))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRangeCxx4 : function () {
      var expected = [ [ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1 ] ];
      var actual = getQueryResults("RETURN NOOPT(RANGE(10, -1))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRangeCxx5 : function () {
      var expected = [ [ 0 ] ];
      var actual = getQueryResults("RETURN NOOPT(RANGE(0, 0))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRangeCxx6 : function () {
      var expected = [ [ 10 ] ];
      var actual = getQueryResults("RETURN NOOPT(RANGE(10, 10, 5))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRangeCxx7 : function () {
      var expected = [ [ 10, 15, 20, 25, 30 ] ];
      var actual = getQueryResults("RETURN NOOPT(RANGE(10, 30, 5))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRangeCxx8 : function () {
      var expected = [ [ 30, 25, 20, 15, 10 ] ];
      var actual = getQueryResults("RETURN NOOPT(RANGE(30, 10, -5))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////
    
    testRangeCxx9 : function () {
      var expected = [ [ 3.4, 4.6, 5.8, 7.0, 8.2 ] ];
      var actual = getQueryResults("RETURN NOOPT(RANGE(3.4, 8.9, 1.2))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range function
////////////////////////////////////////////////////////////////////////////////

    testRangeCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE.code, "RETURN NOOPT(RANGE(1, 100000000))");
      assertQueryError(errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE.code, "RETURN NOOPT(RANGE(100000000, 1, -1))");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(RANGE())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(RANGE(1))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(RANGE(1, 2, 3, 4))"); 
      assertEqual([ [ 0, 1, 2 ] ], getQueryResults("RETURN NOOPT(RANGE(null, 2))"));
      assertEqual([ [ 0 ] ], getQueryResults("RETURN NOOPT(RANGE(null, null))"));
      assertEqual([ [ 0, 1, 2 ] ], getQueryResults("RETURN NOOPT(RANGE(false, 2))"));
      assertEqual([ [ 1, 2 ] ], getQueryResults("RETURN NOOPT(RANGE(true, 2))"));
      assertEqual([ [ 1 ] ], getQueryResults("RETURN NOOPT(RANGE(1, true))"));
      assertEqual([ [ 1, 0 ] ], getQueryResults("RETURN NOOPT(RANGE(1, [ ]))"));
      assertEqual([ [ 1, 0 ] ], getQueryResults("RETURN NOOPT(RANGE(1, { }))"));
      
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(RANGE(1, 1, 0))");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(RANGE(-1, -1, 0))");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(RANGE(1, -1, 1))");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NOOPT(RANGE(-1, 1, -1))");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus1 : function () {
      var expected = [ 'b', 'd' ].sort();
      // Opt
      var actual = getQueryResults("RETURN MINUS([ 'a', 'b', 'c', 'd' ], [ 'c' ], [ 'a', 'c', 'e' ])");
      assertEqual(expected, actual[0].sort());
      // No opt / No v8
      actual = getQueryResults("RETURN NOOPT(MINUS([ 'a', 'b', 'c', 'd' ], [ 'c' ], [ 'a', 'c', 'e' ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus2 : function () {
      var expected = [ 'a', 'b' ].sort();
      // Opt
      var actual = getQueryResults("RETURN MINUS([ 'a', 'b', 'c' ], [ 'c', 'd', 'e' ])");
      assertEqual(expected, actual[0].sort());

      // No Opt / No v8
      actual = getQueryResults("RETURN NOOPT(MINUS([ 'a', 'b', 'c' ], [ 'c', 'd', 'e' ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus3 : function () {
      var expected = [ 'a', 'b', 'c' ].sort();

      // Opt
      var actual = getQueryResults("RETURN MINUS([ 'a', 'b', 'c' ], [ 1, 2, 3 ])");
      assertEqual(expected, actual[0].sort());

      // No Opt / No v8
      actual = getQueryResults("RETURN NOOPT(MINUS([ 'a', 'b', 'c' ], [ 1, 2, 3 ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus4 : function () {
      var expected = [ 'a', 'b', 'c' ].sort();

      // Opt
      var actual = getQueryResults("RETURN MINUS([ 'a', 'b', 'c' ], [ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual[0].sort());

      // No Opt / No v8
      actual = getQueryResults("RETURN NOOPT(MINUS([ 'a', 'b', 'c' ], [ 1, 2, 3 ], [ 1, 2, 3 ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus5 : function () {
      var expected = [ [ ] ];

      // Opt
      var actual = getQueryResults("RETURN MINUS([ 2 ], [ 'a', 'b', 'c' ], [ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual);

      // No Opt / No v8
      actual = getQueryResults("RETURN NOOPT(MINUS([ 2 ], [ 'a', 'b', 'c' ], [ 1, 2, 3 ], [ 1, 2, 3 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minus function
////////////////////////////////////////////////////////////////////////////////
    
    testMinus6 : function () {
      var expected = [ [ ] ];

      // Opt
      var actual = getQueryResults("RETURN MINUS([ ], [ 'a', 'b', 'c' ], [ 1, 2, 3 ], [ 1, 2, 3 ])");
      assertEqual(expected, actual);

      // No Opt / No v8
      actual = getQueryResults("RETURN NOOPT(MINUS([ ], [ 'a', 'b', 'c' ], [ 1, 2, 3 ], [ 1, 2, 3 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection1 : function () {
      var expected = [ -3, 1 ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1, -3 ], [ -3, 1 ])");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection2 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ ], [ 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection3 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1 ], [  ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection4 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1 ], [ 2, 3, 1 ], [ 4, 5, 6 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection5 : function () {
      var expected = [ 2, 4 ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1, 3, 2, 4 ], [ 2, 3, 1, 4 ], [ 4, 5, 6, 2 ])");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection6 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ [ 1, 2 ] ], [ 2, 1 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection7 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ [ 1, 2 ] ], [ 1, 2 ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection8 : function () {
      var expected = [ [ [ 1, 2 ] ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ [ 1, 2 ] ], [ [ 1, 2 ] ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection9 : function () {
      var expected = [ [ { foo: 'test' } ] ];
      var actual = getQueryResults("RETURN INTERSECTION([ { foo: 'bar' }, { foo: 'test' } ], [ { foo: 'test' } ])");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection10 : function () {
      var expected = [ 2, 4, 5 ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1, 2, 3, 3, 4, 4, 5, 1 ], [ 2, 4, 5 ])");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection11 : function () {
      var expected = [ 1, 3 ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1, 1, 2, 2, 3, 3, 3 ], [ 1, 1, 3 ])");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersection12 : function () {
      var expected = [ 3 ];
      var actual = getQueryResults("RETURN INTERSECTION([ 1, 1, 3 ], [ 2, 2, 3 ])");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersection function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx1 : function () {
      var expected = [ -3, 1 ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ 1, -3 ], [ -3, 1 ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx2 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ ], [ 1 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx3 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ 1 ], [  ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx4 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ 1 ], [ 2, 3, 1 ], [ 4, 5, 6 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx5 : function () {
      var expected = [ 2, 4 ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ 1, 3, 2, 4 ], [ 2, 3, 1, 4 ], [ 4, 5, 6, 2 ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx6 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ [ 1, 2 ] ], [ 2, 1 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx7 : function () {
      var expected = [ [ ] ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ [ 1, 2 ] ], [ 1, 2 ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx8 : function () {
      var expected = [ [ [ 1, 2 ] ] ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ [ 1, 2 ] ], [ [ 1, 2 ] ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx9 : function () {
      var expected = [ [ { foo: 'test' } ] ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ { foo: 'bar' }, { foo: 'test' } ], [ { foo: 'test' } ]))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx10 : function () {
      var expected = [ 2, 4, 5 ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ 1, 2, 3, 3, 4, 4, 5, 1 ], [ 2, 4, 5 ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx11 : function () {
      var expected = [ 1, 3 ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ 1, 1, 2, 2, 3, 3, 3 ], [ 1, 1, 3 ]))");
      assertEqual(expected, actual[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test intersect function
////////////////////////////////////////////////////////////////////////////////
    
    testIntersectionCxx12 : function () {
      var expected = [ 3 ];
      var actual = getQueryResults("RETURN NOOPT(INTERSECTION([ 1, 1, 3 ], [ 2, 2, 3 ]))");
      assertEqual(expected, actual[0].sort());
    },

    testIntersectionAllDocuments : function () {
      var bindVars = {"@collection": collectionName};
      var actual = getQueryResults("LET list = (FOR x IN @@collection RETURN x) RETURN INTERSECTION(list, list)", bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0].length, 10);
    },

    testIntersectionAllDocuments2 : function () {
      var bindVars = {"@collection": collectionName};
      var actual = getQueryResults("RETURN INTERSECTION((FOR x IN @@collection RETURN x), (FOR x IN @@collection RETURN x))", bindVars);
      assertEqual(actual.length, 1);
      assertEqual(actual[0].length, 10);
    },

    testIntersectionSingleDocuments : function () {
      var bindVars = {"@collection": collectionName};
      var actual = getQueryResults("FOR x IN @@collection RETURN INTERSECTION([x], [x])", bindVars);
      assertEqual(actual.length, 10);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test outersection function
////////////////////////////////////////////////////////////////////////////////
    
    testOutersection : function () {
      var queries = [
        [ [ [ ], [ ], [ ], [ ] ], [ ] ],
        [ [ [ 1 ], [ ] ], [ 1 ] ],
        [ [ [ ], [ 1 ] ], [ 1 ] ],
        [ [ [ 1 ], [ ], [ ], [ ] ], [ 1 ] ],
        [ [ [ 1 ], [ -1 ] ], [ -1, 1 ] ],
        [ [ [ 1, "a" ], [ 2, "b" ] ], [ 1, 2, "a", "b" ] ],
        [ [ [ 1, 2, 3 ], [ 3, 4, 5 ], [ 5, 6, 7 ] ], [ 1, 2, 4, 6, 7 ] ], 
        [ [ [ "a", "b", "c" ], [ "a", "b", "c" ], [ "a", "b", "c" ] ], [ ] ], 
        [ [ [ "a", "b", "c" ], [ "a", "b", "c", "d" ], [ "a", "b", "c", "d", "e" ] ], [ "e" ] ], 
        [ [ [ "a", "b", "c" ], [ "d", "e", "f" ], [ "g", "h", "i" ] ], [ "a", "b", "c", "d", "e", "f", "g", "h", "i" ] ], 
        [ [ [ "a", "x" ], [ "b", "z" ] ], [ "a", "b", "x", "z" ] ], 
        [ [ [ [ 1 ], 1, 2, [ 2 ] ], [ 3, [ 3 ] ] ], [ 1, 2, 3, [ 1 ], [ 2 ], [ 3 ] ] ],
        [ [ [ [ 1 ], [ 1 ] ], [ [ 2 ], [ 2 ] ] ], [ ] ],
        [ [ [ [ 1 ], [ 2 ] ], [ [ 3 ], [ 4 ] ] ], [ [ 1 ], [ 2 ], [ 3 ], [ 4 ] ] ],
        [ [ [ [ 1 ], [ 2 ] ], [ [ 2 ], [ 4 ] ] ], [ [ 1 ], [ 4 ] ] ]
      ];

      queries.forEach(function(query) {
        var actual = getQueryResults("RETURN SORTED(OUTERSECTION(" + JSON.stringify(query[0]).replace(/^\[(.*)\]$/, "$1") + "))");
        assertEqual(query[1], actual[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test flatten function
////////////////////////////////////////////////////////////////////////////////
    
    testFlattenInvalid : function () {
      var buildQuery = function (nr, input) {
        switch (nr) {
          case 0:
            // Optimized
            return `RETURN FLATTEN(${input})`;
          case 1:
            // No opt / No v8
            return `RETURN NOOPT(FLATTEN(${input}))`;
          default:
            assertTrue(false, "Unreachable state");
        }
      };
      for (var i = 0; i < 2; ++i) {
        var q = buildQuery(i, "");
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, q);
        q = buildQuery(i, "[], 1, 1");
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, q);
        q = buildQuery(i, "1");
        assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, q);
        q = buildQuery(i, "null");
        assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, q);
        q = buildQuery(i, "false");
        assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, q);
        q = buildQuery(i, "'foo'");
        assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, q);
        q = buildQuery(i, "[ ], 'foo'");
        assertEqual([ [ ] ], getQueryResults(q), q);
        q = buildQuery(i, "[ ], [ ]");
        assertEqual([ [ ] ], getQueryResults(q), q);
        q = buildQuery(i, "{ }");
        assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, q);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test flatten function
////////////////////////////////////////////////////////////////////////////////
    
    testFlatten1 : function () {
      var input = [ [ 2, 4, 5 ], [ 1, 2, 3 ], [ ], [ "foo", "bar" ] ];
      // Optimized
      var actual = getQueryResults("RETURN FLATTEN(@value)", { value: input });
      assertEqual([ [ 2, 4, 5, 1, 2, 3, "foo", "bar" ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 1)", { value: input });
      assertEqual([ [ 2, 4, 5, 1, 2, 3, "foo", "bar" ] ], actual);

      // No opt / No v8
      actual = getQueryResults("RETURN NOOPT(FLATTEN(@value))", { value: input });
      assertEqual([ [ 2, 4, 5, 1, 2, 3, "foo", "bar" ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(FLATTEN(@value, 1))", { value: input });
      assertEqual([ [ 2, 4, 5, 1, 2, 3, "foo", "bar" ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test flatten function
////////////////////////////////////////////////////////////////////////////////
    
    testFlatten2 : function () {
      var input = [ [ 1, 1, 2, 2 ], [ 1, 1, 2, 2 ], [ null, null ], [ [ 1, 2 ], [ 3, 4 ] ] ];
      // Optimized
      var actual = getQueryResults("RETURN FLATTEN(@value)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, null, null, [ 1, 2 ], [ 3, 4 ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 1)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, null, null, [ 1, 2 ], [ 3, 4 ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 2)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, null, null, 1, 2, 3, 4 ] ], actual);

      // No opt / No v8
      actual = getQueryResults("RETURN NOOPT(FLATTEN(@value))", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, null, null, [ 1, 2 ], [ 3, 4 ] ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(FLATTEN(@value, 1))", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, null, null, [ 1, 2 ], [ 3, 4 ] ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(FLATTEN(@value, 2))", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, null, null, 1, 2, 3, 4 ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test flatten_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testFlatten3 : function () {
      var input = [ [ 1, 1, 2, 2 ], [ 1, 1, 2, 2 ], [ [ 1, 2 ], [ 3, 4 ], [ [ 5, 6 ], [ 7, 8 ] ], [ 9, 10, [ 11, 12 ] ] ] ];

      // Optimized
      var actual = getQueryResults("RETURN FLATTEN(@value, 1)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, [ 1, 2 ], [ 3, 4 ], [ [ 5, 6 ], [ 7, 8 ] ], [ 9, 10, [ 11, 12 ] ] ] ], actual);

      actual = getQueryResults("RETURN FLATTEN(@value, 2)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, 1, 2, 3, 4, [ 5, 6 ], [ 7, 8 ], 9, 10, [ 11, 12 ] ] ], actual);
      
      actual = getQueryResults("RETURN FLATTEN(@value, 3)", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ] ], actual);

      // No opt / No v8
      actual = getQueryResults("RETURN NOOPT(FLATTEN(@value, 1))", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, [ 1, 2 ], [ 3, 4 ], [ [ 5, 6 ], [ 7, 8 ] ], [ 9, 10, [ 11, 12 ] ] ] ], actual);

      actual = getQueryResults("RETURN NOOPT(FLATTEN(@value, 2))", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, 1, 2, 3, 4, [ 5, 6 ], [ 7, 8 ], 9, 10, [ 11, 12 ] ] ], actual);
      
      actual = getQueryResults("RETURN NOOPT(FLATTEN(@value, 3))", { value: input });
      assertEqual([ [ 1, 1, 2, 2, 1, 1, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test flatten_recursive function
////////////////////////////////////////////////////////////////////////////////
    
    testFlatten4 : function () {
      var input = [ 1, [ 2, [ 3, [ 4, [ 5, [ 6, [ 7, [ 8, [ 9 ] ] ] ] ] ] ] ] ];
      var buildQuery = function (nr, input) {
        switch (nr) {
          case 0:
            return `RETURN FLATTEN(@value, ${input})`;
          case 1:
            return `RETURN NOOPT(FLATTEN(@value, ${input}))`;
          default:
          assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 2; ++i) {
        var q = buildQuery(i, 1);
        var actual = getQueryResults(q, { value: input });
        assertEqual([ [ 1, 2, [ 3, [ 4, [ 5, [ 6, [ 7, [ 8, [ 9 ] ] ] ] ] ] ] ] ], actual, q);

        q = buildQuery(i, 2);
        actual = getQueryResults(q, { value: input });
        assertEqual([ [ 1, 2, 3, [ 4, [ 5, [ 6, [ 7, [ 8, [ 9 ] ] ] ] ] ] ] ], actual, q);

        q = buildQuery(i, 3);
        actual = getQueryResults(q, { value: input });
        assertEqual([ [ 1, 2, 3, 4, [ 5, [ 6, [ 7, [ 8, [ 9 ] ] ] ] ] ] ], actual);

        q = buildQuery(i, 4);
        actual = getQueryResults(q, { value: input });
        assertEqual([ [ 1, 2, 3, 4, 5, [ 6, [ 7, [ 8, [ 9 ] ] ] ] ] ], actual);

        q = buildQuery(i, 5);
        actual = getQueryResults(q, { value: input });
        assertEqual([ [ 1, 2, 3, 4, 5, 6, [ 7, [ 8, [ 9 ] ] ] ] ], actual);

        q = buildQuery(i, 6);
        actual = getQueryResults(q, { value: input });
        assertEqual([ [ 1, 2, 3, 4, 5, 6, 7, [ 8, [ 9 ] ] ] ], actual);

        q = buildQuery(i, 7);
        actual = getQueryResults(q, { value: input });
        assertEqual([ [ 1, 2, 3, 4, 5, 6, 7, 8, [ 9 ] ] ], actual);

        q = buildQuery(i, 8);
        actual = getQueryResults(q, { value: input });
        assertEqual([ [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ], actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////
    
    testMin1 : function () {
      var expected = [ null, null ]; 
      var actual = getQueryResults("FOR u IN [ [ ], [ null, null ] ] return MIN(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////
    
    testMin2 : function () {
      var expected = [ 1, 1, 1, 1, 1, 1 ];
      var actual = getQueryResults("FOR u IN [ [ 1, 2, 3 ], [ 3, 2, 1 ], [ 1, 3, 2 ], [ 2, 3, 1 ], [ 2, 1, 3 ], [ 3, 1, 2 ] ] return MIN(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////
    
    testMin3 : function () {
      var expected = [ 2, false, false, false, false, true, -1, '', 1 ];
      var actual = getQueryResults("FOR u IN [ [ 3, 2, '1' ], [ [ ], null, true, false, 1, '0' ], [ '0', 1, false, true, null, [ ] ], [ false, true ], [ 0, false ], [ true, 0 ], [ '0', -1 ], [ '', '-1' ], [ [ ], 1 ] ] return MIN(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////

    testMinInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MIN()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MIN([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MIN(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MIN(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MIN(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MIN(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MIN({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////
    
    testMax1 : function () {
      var expected = [ null, null ]; 
      var actual = getQueryResults("FOR u IN [ [ ], [ null, null ] ] return MAX(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////
    
    testMax2 : function () {
      var expected = [ 3, 3, 3, 3, 3, 3 ];
      var actual = getQueryResults("FOR u IN [ [ 1, 2, 3 ], [ 3, 2, 1 ], [ 1, 3, 2 ], [ 2, 3, 1 ], [ 2, 1, 3 ], [ 3, 1, 2 ] ] return MAX(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////
    
    testMax3 : function () {
      var expected = [ '1', [ ], [ ], true, 0, 0, '0', '-1', [ ] ];
      var actual = getQueryResults("FOR u IN [ [ 3, 2, '1' ], [ [ ], null, true, false, 1, '0' ], [ '0', 1, false, true, null, [ ] ], [ false, true ], [ 0, false ], [ true, 0 ], [ '0', -1 ], [ '', '-1' ], [ [ ], 1 ] ] return MAX(u)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////

    testMaxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MAX()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MAX([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MAX(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MAX(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MAX(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MAX(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MAX({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sum function
////////////////////////////////////////////////////////////////////////////////
    
    testSum : function () {
      var data = [
        [ 0, [ ] ],
        [ 0, [ null ] ],
        [ 0, [ null, null ] ],
        [ 1, [ 1, null, null ] ],
        [ 2, [ 1, null, null, 1 ] ],
        [ 15, [ 1, 2, 3, 4, 5 ] ],
        [ 15, [ 5, 4, 3, 2, 1 ] ],
        [ 15, [ null, 5, 4, null, 3, 2, 1, null ] ],
        [ 0, [ -1, 1, -1, 1, -1, 1, 0 ] ],
        [ -4, [ -1, -1, -1, -1 ] ],
        [ 1.31, [ 0.1, 0.1, 0.01, 1.1 ] ],
        [ -1.31, [ -0.1, -0.1, -0.01, -1.1 ] ],
        [ 9040346.290954, [ 45.356, 256.23, -223.6767, -14512.63, 456.00222, -0.090566, 9054325.1 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN SUM(" + JSON.stringify(value[1]) + ")");
        assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sum function
////////////////////////////////////////////////////////////////////////////////

    testSumInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUM()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUM([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN SUM(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN SUM(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN SUM(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN SUM(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN SUM({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test average function
////////////////////////////////////////////////////////////////////////////////
    
    testAverage : function () {
      var data = [
        [ null, [ ] ],
        [ null, [ null ] ],
        [ null, [ null, null ] ],
        [ 1, [ 1, null, null ] ],
        [ 1, [ 1, null, null, 1 ] ],
        [ 2.5, [ 0, 1, 2, 3, 4, 5 ] ],
        [ 3, [ 1, 2, 3, 4, 5 ] ],
        [ 3, [ 5, 4, 3, 2, 1 ] ],
        [ 3, [ 5, 4, null, null, 3, 2, 1, null ] ],
        [ 0, [ -1, 1, -1, 1, -1, 1, 0 ] ],
        [ -1, [ -1, -1, -1, -1 ] ],
        [ 0.3275, [ 0.1, 0.1, 0.01, 1.1 ] ],
        [ -0.3275, [ -0.1, -0.1, -0.01, -1.1 ] ],
        [ 1291478.0415649, [ 45.356, 256.23, -223.6767, -14512.63, 456.00222, -0.090566, 9054325.1 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN AVERAGE(" + JSON.stringify(value[1]) + ")");
        if (actual[0] === null) {
          assertNull(value[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test average function
////////////////////////////////////////////////////////////////////////////////

    testAverageInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN AVERAGE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN AVERAGE([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN AVERAGE(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN AVERAGE(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN AVERAGE(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN AVERAGE(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN AVERAGE({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test product function
////////////////////////////////////////////////////////////////////////////////
    
    testProduct : function () {
      var data = [
        [ 1, [ ] ],
        [ 1, [ null ] ],
        [ 1, [ null, null ] ],
        [ 1, [ 1 ] ],
        [ 33, [ 33 ] ],
        [ -1, [ -1 ] ],
        [ 0.5, [ 0.5 ] ],
        [ 0.25, [ 0.5, 0.5 ] ],
        [ 1, [ 1, null, null ] ],
        [ 1, [ 1, null, null, 1 ] ],
        [ 0, [ 1, null, null, 0 ] ],
        [ 35, [ 1, null, null, 35 ] ],
        [ 120, [ 1, 2, 3, 4, 5 ] ],
        [ 120, [ 5, 4, 3, 2, 1 ] ],
        [ 120, [ null, 5, 4, null, 3, 2, 1, null ] ],
        [ -1, [ -1, 1, -1, 1, -1, 1 ] ],
        [ 0, [ -1, 1, -1, 1, -1, 0 ] ],
        [ 1, [ -1, -1, -1, -1 ] ],
        [ 0.0001, [ 0.1, 0.1, 0.01, 1.1 ] ],
        [ 0.001, [ 0.1, 0.1, 0.1 ] ],
        [ 0.01, [ 0.1, 0.1 ] ],
        [ 100, [ 0.1, 1000 ] ],
        [ 350, [ 0.1, 0.1, 35, 1000 ] ],
        [ 0.0001, [ -0.1, -0.1, -0.01, -1.1 ] ],
        [ -2599473.95222, [ 45.356, 256.23, -223.6767 ] ],
        [ -14106517788136454000, [ 45.356, 256.23, -223.6767, -14512.63, 456.00222, -0.090566, 9054325.1 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN PRODUCT(" + JSON.stringify(value[1]) + ")");
        assertEqual(value[0].toFixed(4), actual[0].toFixed(4), value);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test product function
////////////////////////////////////////////////////////////////////////////////

    testProductInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PRODUCT()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PRODUCT([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN PRODUCT(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN PRODUCT(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN PRODUCT(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN PRODUCT(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN PRODUCT({ })"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////
    
    testMinCxx1 : function () {
      var expected = [ null, null ]; 
      var actual = getQueryResults("FOR u IN [ [ ], [ null, null ] ] RETURN NOOPT(MIN(u))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////
    
    testMinCxx2 : function () {
      var expected = [ 1, 1, 1, 1, 1, 1 ];
      var actual = getQueryResults("FOR u IN [ [ 1, 2, 3 ], [ 3, 2, 1 ], [ 1, 3, 2 ], [ 2, 3, 1 ], [ 2, 1, 3 ], [ 3, 1, 2 ] ] RETURN NOOPT(MIN(u))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////
    
    testMinCxx3 : function () {
      var expected = [ 2, false, false, false, false, true, -1, '', 1 ];
      var actual = getQueryResults("FOR u IN [ [ 3, 2, '1' ], [ [ ], null, true, false, 1, '0' ], [ '0', 1, false, true, null, [ ] ], [ false, true ], [ 0, false ], [ true, 0 ], [ '0', -1 ], [ '', '-1' ], [ [ ], 1 ] ] RETURN NOOPT(MIN(u))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min function
////////////////////////////////////////////////////////////////////////////////

    testMinCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MIN())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MIN([ ], 2))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MIN(null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MIN(false))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MIN(3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MIN(\"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MIN({ }))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////
    
    testMaxCxx1 : function () {
      var expected = [ null, null ]; 
      var actual = getQueryResults("FOR u IN [ [ ], [ null, null ] ] RETURN NOOPT(MAX(u))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////
    
    testMaxCxx2 : function () {
      var expected = [ 3, 3, 3, 3, 3, 3 ];
      var actual = getQueryResults("FOR u IN [ [ 1, 2, 3 ], [ 3, 2, 1 ], [ 1, 3, 2 ], [ 2, 3, 1 ], [ 2, 1, 3 ], [ 3, 1, 2 ] ] RETURN NOOPT(MAX(u))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////
    
    testMaxCxx3 : function () {
      var expected = [ '1', [ ], [ ], true, 0, 0, '0', '-1', [ ] ];
      var actual = getQueryResults("FOR u IN [ [ 3, 2, '1' ], [ [ ], null, true, false, 1, '0' ], [ '0', 1, false, true, null, [ ] ], [ false, true ], [ 0, false ], [ true, 0 ], [ '0', -1 ], [ '', '-1' ], [ [ ], 1 ] ] RETURN NOOPT(MAX(u))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max function
////////////////////////////////////////////////////////////////////////////////

    testMaxCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MAX())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MAX([ ], 2))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MAX(null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MAX(false))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MAX(3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MAX(\"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MAX({ }))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sum function
////////////////////////////////////////////////////////////////////////////////
    
    testCxxSum : function () {
      var data = [
        [ 0, [ ] ],
        [ 0, [ null ] ],
        [ 0, [ null, null ] ],
        [ 1, [ 1, null, null ] ],
        [ 2, [ 1, null, null, 1 ] ],
        [ 15, [ 1, 2, 3, 4, 5 ] ],
        [ 15, [ 5, 4, 3, 2, 1 ] ],
        [ 15, [ null, 5, 4, null, 3, 2, 1, null ] ],
        [ 0, [ -1, 1, -1, 1, -1, 1, 0 ] ],
        [ -4, [ -1, -1, -1, -1 ] ],
        [ 1.31, [ 0.1, 0.1, 0.01, 1.1 ] ],
        [ -1.31, [ -0.1, -0.1, -0.01, -1.1 ] ],
        [ 9040346.290954, [ 45.356, 256.23, -223.6767, -14512.63, 456.00222, -0.090566, 9054325.1 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN SUM(NOOPT(" + JSON.stringify(value[1]) + "))");
        assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sum function
////////////////////////////////////////////////////////////////////////////////

    testSumCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SUM())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(SUM([ ], 2))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(SUM(null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(SUM(false))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(SUM(3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(SUM(\"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(SUM({ }))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test average function
////////////////////////////////////////////////////////////////////////////////
    
    testAverageCxx : function () {
      var data = [
        [ null, [ ] ],
        [ null, [ null ] ],
        [ null, [ null, null ] ],
        [ 1, [ 1, null, null ] ],
        [ 1, [ 1, null, null, 1 ] ],
        [ 2.5, [ 0, 1, 2, 3, 4, 5 ] ],
        [ 3, [ 1, 2, 3, 4, 5 ] ],
        [ 3, [ 5, 4, 3, 2, 1 ] ],
        [ 3, [ 5, 4, null, null, 3, 2, 1, null ] ],
        [ 0, [ -1, 1, -1, 1, -1, 1, 0 ] ],
        [ -1, [ -1, -1, -1, -1 ] ],
        [ 0.3275, [ 0.1, 0.1, 0.01, 1.1 ] ],
        [ -0.3275, [ -0.1, -0.1, -0.01, -1.1 ] ],
        [ 1291478.0415649, [ 45.356, 256.23, -223.6767, -14512.63, 456.00222, -0.090566, 9054325.1 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN NOOPT(AVERAGE(" + JSON.stringify(value[1]) + "))");
        if (actual[0] === null) {
          assertNull(value[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test average function
////////////////////////////////////////////////////////////////////////////////

    testAverageCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(AVERAGE())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(AVERAGE([ ], 2))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(AVERAGE(null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(AVERAGE(false))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(AVERAGE(3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(AVERAGE(\"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(AVERAGE({ }))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test median function
////////////////////////////////////////////////////////////////////////////////
    
    testMedian : function () {
      var data = [
        [ null, [ ] ],
        [ null, [ null ] ],
        [ null, [ null, null ] ],
        [ 1, [ 1, null, null ] ],
        [ 1, [ 1, null, null, 1 ] ],
        [ 1.5, [ 1, null, null, 2 ] ],
        [ 2, [ 1, null, null, 2, 3 ] ],
        [ 2.5, [ 1, null, null, 2, 3, 4 ] ],
        [ 2.5, [ 0, 1, 2, 3, 4, 5 ] ],
        [ 0, [ 0, 0, 0, 0, 0, 0, 1 ] ],
        [ 5, [ 1, 10, 2, 9, 3, 8, 5 ] ],
        [ 5.5, [ 1, 10, 2, 9, 3, 8, 6, 5 ] ],
        [ 3, [ 1, 2, 3, 4, 5 ] ],
        [ 3, [ 5, 4, 3, 2, 1 ] ],
        [ 3.5, [ 5, 4, 4, 3, 2, 1 ] ],
        [ 3, [ 5, 4, null, null, 3, 2, 1, null ] ],
        [ 0, [ -1, 1, -1, 1, -1, 1, 0 ] ],
        [ 0.5, [ -1, 1, -1, 1, -1, 1, 1, 0 ] ],
        [ -1, [ -1, -1, -1, -1 ] ],
        [ 0.1, [ 0.1, 0.1, 0.01, 1.1 ] ],
        [ -0.1, [ -0.1, -0.1, -0.01, -1.1 ] ],
        [ 45.356, [ 45.356, 256.23, -223.6767, -14512.63, 456.00222, -0.090566, 9054325.1 ] ],
        [ 2.5, [ 1, 2, 3, 100000000000 ] ],
        [ 3, [ 1, 2, 3, 4, 100000000000 ] ],
        [ 0.005, [ -100000, 0, 0.01, 0.2 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN MEDIAN(" + JSON.stringify(value[1]) + ")");
        if (actual[0] === null) {
          assertNull(value[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }

        actual = getQueryResults("RETURN NOOPT(MEDIAN(" + JSON.stringify(value[1]) + "))");
        if (actual[0] === null) {
          assertNull(value[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test median function
////////////////////////////////////////////////////////////////////////////////

    testMedianInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MEDIAN()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN MEDIAN([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MEDIAN(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MEDIAN(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MEDIAN(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MEDIAN(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN MEDIAN({ })"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MEDIAN())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(MEDIAN([ ], 2))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MEDIAN(null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MEDIAN(false))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MEDIAN(3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MEDIAN(\"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(MEDIAN({ }))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test percentile function
////////////////////////////////////////////////////////////////////////////////
 
    testPercentile : function () {
      var data = [
        [ null, [ 1, 2, 3, 4, 5 ], 0, "rank" ],
        [ null, [ 15, 20, 35, 40, 50 ], 0, "rank" ],
        [ 50, [ 50, 40, 35, 20, 15 ], 100, "rank" ],
        [ 50, [ 15, 20, 35, 40, 50 ], 100, "rank" ],
        [ 20, [ 15, 20, 35, 40, 50 ], 30, "rank" ],
        [ 20, [ 15, 20, 35, 40, 50 ], 40, "rank" ],
        [ 35, [ 15, 20, 35, 40, 50 ], 50, "rank" ],
        [ 50, [ 15, 20, 35, 40, 50 ], 100, "rank" ],
        [ 15, [ 3, 5, 7, 8, 9, 11, 13, 15 ], 100, "rank" ],
        [ 3, [ 1, 3, 3, 4, 5, 6, 6, 7, 8, 8 ], 25, "interpolation" ],
        [ 5.5, [ 1, 3, 3, 4, 5, 6, 6, 7, 8, 8 ], 50, "interpolation" ],
        [ 7.25, [ 1, 3, 3, 4, 5, 6, 6, 7, 8, 8 ], 75, "interpolation" ],
        [ 84, [ 50, 65, 70, 72, 72, 78, 80, 82, 84, 84, 85, 86, 88, 88, 90, 94, 96, 98, 98, 99 ], 45, "rank" ],
        [ 86, [ 50, 65, 70, 72, 72, 78, 80, 82, 84, 84, 85, 86, 88, 88, 90, 94, 96, 98, 98, 99 ], 58, "rank" ],
        [ null, [ 3, 5, 7, 8, 9, 11, 13, 15 ], 0, "interpolation" ],
        [ 5.5, [ 3, 5, 7, 8, 9, 11, 13, 15 ], 25, "interpolation" ],
        [ 5.5, [ null, 3, 5, null, 7, 8, 9, 11, 13, 15, null ], 25, "interpolation" ],
        [ 5, [ 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 9, 9, 9, 10, 10, 10 ], 25, "interpolation" ],
        [ 9.85, [ 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 9, 9, 9, 10, 10, 10 ], 85, "interpolation" ],
        [ 4, [ 2, 3, 5, 9 ], 50, "interpolation" ],
        [ 5, [ 2, 3, 5, 9, 11 ], 50, "interpolation" ],
        [ 11, [ 2, 3, 5, 9, 11 ], 100, "interpolation" ],
        [ 5, [ 2, 3, null, 5, 9, 11, null ], 50, "interpolation" ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN PERCENTILE(" + JSON.stringify(value[1]) + ", " + JSON.stringify(value[2]) + ", " + JSON.stringify(value[3]) + ")");
        if (actual[0] === null) {
          assertNull(value[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4), value);
        }

        actual = getQueryResults("RETURN NOOPT(PERCENTILE(" + JSON.stringify(value[1]) + ", " + JSON.stringify(value[2]) + ", " + JSON.stringify(value[3]) + "))");
        if (actual[0] === null) {
          assertNull(value[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4), value);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////
    
    testHas1 : function () {
      var expected = [ true, true, true, false, true ]; 
      var actual = getQueryResults("FOR u IN [ { \"the fox\" : true }, { \"the fox\" : false }, { \"the fox\" : null }, { \"the FOX\" : true }, { \"the FOX\" : true, \"the fox\" : false } ] return HAS(u, \"the fox\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////
    
    testHas2 : function () {
      var expected = [ false, false, false ];
      var actual = getQueryResults("FOR u IN [ { \"the foxx\" : { \"the fox\" : false } }, { \" the fox\" : false }, null ] return HAS(u, \"the fox\")");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////
    
    testHas3 : function () {
      var expected = [ [ "test2", [ "other" ] ] ];
      var actual = getQueryResults("LET doc = { \"_id\": \"test/76689250173\", \"_rev\": \"76689250173\", \"_key\": \"76689250173\", \"test1\": \"something\", \"test2\": { \"DATA\": [ \"other\" ] } } FOR attr IN ATTRIBUTES(doc) LET prop = doc[attr] FILTER HAS(prop, 'DATA') RETURN [ attr, prop.DATA ]"); 
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////

    testHasInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN HAS()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN HAS({ })"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN HAS({ }, \"fox\", true)"); 
      assertEqual([ false ], getQueryResults("RETURN HAS(false, \"fox\")")); 
      assertEqual([ false ], getQueryResults("RETURN HAS(3, \"fox\")")); 
      assertEqual([ false ], getQueryResults("RETURN HAS(\"yes\", \"fox\")")); 
      assertEqual([ false ], getQueryResults("RETURN HAS([ ], \"fox\")")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, null)")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, false)")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, true)")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, 1)")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, [ ])")); 
      assertEqual([ false ], getQueryResults("RETURN HAS({ }, { })")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////
    
    testHasCxx1 : function () {
      var expected = [ true, true, true, false, true ]; 
      var actual = getQueryResults("FOR u IN [ { \"the fox\" : true }, { \"the fox\" : false }, { \"the fox\" : null }, { \"the FOX\" : true }, { \"the FOX\" : true, \"the fox\" : false } ] return NOOPT(HAS(u, \"the fox\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////
    
    testHasCxx2 : function () {
      var expected = [ false, false, false ];
      var actual = getQueryResults("FOR u IN [ { \"the foxx\" : { \"the fox\" : false } }, { \" the fox\" : false }, null ] return NOOPT(HAS(u, \"the fox\"))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////
    
    testHasCxx3 : function () {
      var expected = [ [ "test2", [ "other" ] ] ];
      var actual = getQueryResults("LET doc = { \"_id\": \"test/76689250173\", \"_rev\": \"76689250173\", \"_key\": \"76689250173\", \"test1\": \"something\", \"test2\": { \"DATA\": [ \"other\" ] } } FOR attr IN ATTRIBUTES(doc) LET prop = doc[attr] FILTER NOOPT(HAS(prop, 'DATA')) RETURN [ attr, prop.DATA ]"); 
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test has function
////////////////////////////////////////////////////////////////////////////////

    testHasCxxInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(HAS())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(HAS({ }))"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(HAS({ }, \"fox\", true))"); 
      assertEqual([ false ], getQueryResults("RETURN NOOPT(HAS(false, \"fox\"))")); 
      assertEqual([ false ], getQueryResults("RETURN NOOPT(HAS(3, \"fox\"))")); 
      assertEqual([ false ], getQueryResults("RETURN NOOPT(HAS(\"yes\", \"fox\"))")); 
      assertEqual([ false ], getQueryResults("RETURN NOOPT(HAS([ ], \"fox\"))")); 
      assertEqual([ false ], getQueryResults("RETURN NOOPT(HAS({ }, null))")); 
      assertEqual([ false ], getQueryResults("RETURN NOOPT(HAS({ }, false))")); 
      assertEqual([ false ], getQueryResults("RETURN NOOPT(HAS({ }, true))")); 
      assertEqual([ false ], getQueryResults("RETURN NOOPT(HAS({ }, 1))")); 
      assertEqual([ false ], getQueryResults("RETURN NOOPT(HAS({ }, [ ]))")); 
      assertEqual([ false ], getQueryResults("RETURN NOOPT(HAS({ }, { }))")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributes1 : function () {
      var expected = [ [ "foo", "bar", "meow", "_id" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN ATTRIBUTES(u)");
      // Order is not guaranteed order here in-place
      assertEqual(actual.length, expected.length);
      assertEqual(actual[0].sort(), expected[0].sort());
      assertEqual(actual[1].sort(), expected[1].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributes2 : function () {
      var expected = [ [ "foo", "bar", "meow" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN ATTRIBUTES(u, true)");
      // Order is not guaranteed order here in-place
      assertEqual(actual.length, expected.length);
      assertEqual(actual[0].sort(), expected[0].sort());
      assertEqual(actual[1].sort(), expected[1].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributes3 : function () {
      var expected = [ [ "_id", "bar", "foo", "meow" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN ATTRIBUTES(u, false, true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributes4 : function () {
      var expected = [ [ "bar", "foo", "meow" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN ATTRIBUTES(u, true, true)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributesCxx1 : function () {
      var expected = [ [ "foo", "bar", "meow", "_id" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN NOOPT(ATTRIBUTES(u))");
      // Order is not guaranteed order here in-place
      assertEqual(actual.length, expected.length);
      assertEqual(actual[0].sort(), expected[0].sort());
      assertEqual(actual[1].sort(), expected[1].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributesCxx2 : function () {
      var expected = [ [ "foo", "bar", "meow" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN NOOPT(ATTRIBUTES(u, true))");
      // Order is not guaranteed order here in-place
      assertEqual(actual.length, expected.length);
      assertEqual(actual[0].sort(), expected[0].sort());
      assertEqual(actual[1].sort(), expected[1].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributesCxx3 : function () {
      var expected = [ [ "_id", "bar", "foo", "meow" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN NOOPT(ATTRIBUTES(u, false, true))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes function
////////////////////////////////////////////////////////////////////////////////
    
    testAttributesCxx4 : function () {
      var expected = [ [ "bar", "foo", "meow" ], [ "foo" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN NOOPT(ATTRIBUTES(u, true, true))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test values function
////////////////////////////////////////////////////////////////////////////////
    
    testValues1 : function () {
      var expected = [ [ "bar", "baz", true, "123/456" ], [ "bar" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN VALUES(u)");
      assertEqual(actual.length, expected.length);
      assertEqual(actual[0].sort(), expected[0].sort());
      assertEqual(actual[1].sort(), expected[1].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test values function
////////////////////////////////////////////////////////////////////////////////
    
    testValues2 : function () {
      var expected = [ [ "bar", "baz", true ], [ "bar" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN VALUES(u, true)");
      assertEqual(actual.length, expected.length);
      assertEqual(actual[0].sort(), expected[0].sort());
      assertEqual(actual[1].sort(), expected[1].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test values function
////////////////////////////////////////////////////////////////////////////////
    
    testValues3 : function () {
      var expected = [ [ "test/1123", "test/abc", "1234", "test", "", [ 1, 2, 3, 4, [ true ] ], null, { d: 42, e: null, f: [ "test!" ] } ] ];
      var actual = getQueryResults("RETURN VALUES({ _from: \"test/1123\", _to: \"test/abc\", _rev: \"1234\", _key: \"test\", void: \"\", a: [ 1, 2, 3, 4, [ true ] ], b : null, c: { d: 42, e: null, f: [ \"test!\" ] } })");
      assertEqual(actual.length, expected.length);
      assertEqual(actual[0].sort(), expected[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test values function
////////////////////////////////////////////////////////////////////////////////
    
    testValuesCxx1 : function () {
      var expected = [ [ "bar", "baz", true, "123/456" ], [ "bar" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN NOOPT(VALUES(u))");
      assertEqual(actual.length, expected.length);
      assertEqual(actual[0].sort(), expected[0].sort());
      assertEqual(actual[1].sort(), expected[1].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test values function
////////////////////////////////////////////////////////////////////////////////
    
    testValuesCxx2 : function () {
      var expected = [ [ "bar", "baz", true ], [ "bar" ] ];
      var actual = getQueryResults("FOR u IN [ { foo: \"bar\", bar: \"baz\", meow: true, _id: \"123/456\" }, { foo: \"bar\" } ] RETURN NOOPT(VALUES(u, true))");
      assertEqual(actual.length, expected.length);
      assertEqual(actual[0].sort(), expected[0].sort());
      assertEqual(actual[1].sort(), expected[1].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test values function
////////////////////////////////////////////////////////////////////////////////
    
    testValuesCxx3 : function () {
      var expected = [ [ "test/1123", "test/abc", "1234", "test", "", [ 1, 2, 3, 4, [ true ] ], null, { d: 42, e: null, f: [ "test!" ] } ] ];
      var actual = getQueryResults("RETURN NOOPT(VALUES({ _from: \"test/1123\", _to: \"test/abc\", _rev: \"1234\", _key: \"test\", void: \"\", a: [ 1, 2, 3, 4, [ true ] ], b : null, c: { d: 42, e: null, f: [ \"test!\" ] } }))");
      assertEqual(actual.length, expected.length);
      assertEqual(actual[0].sort(), expected[0].sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test zip function
////////////////////////////////////////////////////////////////////////////////
    
    testZip : function () {
      var values = [
        [ { }, [ ], [ ] ], 
        // duplicate keys
        [ { "" : 1, " " : 3 }, [ "", "", " ", " " ], [ 1, 2, 3, 4 ] ], 
        [ { "a" : 1 }, [ "a", "a", "a", "a" ], [ 1, 2, 3, 4 ] ], 
        [ { "" : true }, [ "" ], [ true ] ], 
        [ { " " : null }, [ " " ], [ null ] ], 
        [ { "MÖTÖR" : { } }, [ "MÖTÖR" ], [ { } ] ], 
        [ { "just-a-single-attribute!" : " with a senseless value " }, [ "just-a-single-attribute!" ], [ " with a senseless value " ] ], 
        [ { a: 1, b: 2, c: 3 }, [ "a", "b", "c" ], [ 1, 2, 3 ] ], 
        [ { foo: "baz", bar: "foo", baz: "bar" }, [ "foo", "bar", "baz" ], [ "baz", "foo", "bar" ] ],
        [ { a: null, b: false, c: true, d: 42, e: 2.5, f: "test", g: [], h: [ "test1", "test2" ], i : { something: "else", more: "less" } }, [ "a", "b", "c", "d", "e", "f", "g", "h", "i" ], [ null, false, true, 42, 2.5, "test", [ ], [ "test1", "test2" ], { something: "else", more: "less" } ] ],
        [ { a11111: "value", b222: "value", c: "value" }, [ "a11111", "b222", "c" ], [ "value", "value", "value" ] ]
      ];

      values.forEach(function (value) {
        // Opt
        var actual = getQueryResults("RETURN ZIP(" + JSON.stringify(value[1]) + ", " + JSON.stringify(value[2]) + ")");
        assertEqual(value[0], actual[0], value);
        // No opt / No v8
        actual = getQueryResults("RETURN NOOPT(ZIP(" + JSON.stringify(value[1]) + ", " + JSON.stringify(value[2]) + "))");
        assertEqual(value[0], actual[0], value);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test zip function
////////////////////////////////////////////////////////////////////////////////
    
    testZipSubquery : function () {
      // Opt
      var query = "LET value = (FOR i IN 1..3 RETURN 'value') RETURN ZIP([ 'a11111', 'b222', 'c' ], value)";
      var actual = getQueryResults(query);
      assertEqual({ a11111: "value", b222: "value", c: "value" }, actual[0]);
      // No opt / No v8
      query = "LET value = (FOR i IN 1..3 RETURN 'value') RETURN NOOPT(ZIP([ 'a11111', 'b222', 'c' ], value))";
      assertEqual({ a11111: "value", b222: "value", c: "value" }, actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test zip function
////////////////////////////////////////////////////////////////////////////////

    testZipInvalid : function () {
      var buildQuery = function (nr, input) {
        switch (nr) {
          case 0:
            return `RETURN ZIP(${input})`;
          case 1:
            return `RETURN NOOPT(ZIP(${input}))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };
      for (var i = 0; i < 2; ++i) {
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "")); 
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "[ ]"));
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "[ ], [ ], [ ]"));
        
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ ], null"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ ], false"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ ], true"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ ], 0"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ ], 1"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ ], \"\""));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ ], { }"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "null, [ ]"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "false, [ ]"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "true, [ ]"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "0, [ ]"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "1, [ ]"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "\"\", [ ]"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "{ }, [ ]"));

        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ 1 ], [ ]"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ 1 ], [ 1, 2 ]"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ ], [ 1, 2 ]"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test matches
////////////////////////////////////////////////////////////////////////////////
    
    testMatches : function () {
      var tests = [
        {
          doc: { },
          examples: [ { } ],
          flag: true,
          expected: [ 0 ]
        },
        {
          doc: { },
          examples: [ { } ],
          flag: false,
          expected: [ true ]
        },
        {
          doc: { },
          examples: [ { } ],
          flag: null,
          expected: [ true ]
        },
        {
          doc: {a:1,b:2},
          examples: {a:1},
          flag: true,
          expected: [ 0 ]
        },
        {
          doc: {a:1,b:2},
          examples: {b:2},
          flag: true,
          expected: [ 0 ]
        },
        {
          doc: {a:1,b:2 },
          examples: { },
          flag: true,
          expected: [ 0 ]
        },
        {
          doc: {a:1,b:2 },
          examples: {a:1,b:2,c:3},
          flag: true,
          expected: [ -1 ]
        },
        {
          doc: {a:1,b:2 },
          examples: {a:1,b:2,c:3},
          flag: null,
          expected: [ false ]
        },
        {
          doc: {a:1,b:2 },
          examples: {c:3},
          flag: null,
          expected: [ false ]
        },
        {
          doc: {a:1,b:2 },
          examples: {c:3},
          flag: true,
          expected: [ -1 ]
        },
        {
          doc: {a:1,b:2 },
          examples: {a:null,b:2 },
          flag: null,
          expected: [ false ]
        },
        {
          doc: {b:2 },
          examples: {a:null,b:2 },
          flag: null,
          expected: [ true ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 1 } ],
          flag: true,
          expected: [ -1 ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 1 } ],
          flag: false,
          expected: [ false ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 2 } ],
          flag: true,
          expected: [ 0 ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 2 } ],
          flag: false,
          expected: [ true ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 3 }, { test1: 1, test2: 2 } ],
          flag: true,
          expected: [ 1 ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 3 }, { test1: 1, test2: 2 } ],
          flag: false,
          expected: [ true ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { test1: 1, test2: 3 }, { test1: 1, test2: 2 } ],
          flag: null,
          expected: [ true ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { } ],
          flag: true,
          expected: [ 0 ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { } ],
          flag: false,
          expected: [ true ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { fox: true }, { fox: false}, { test1: "something" }, { test99: 1, test2: 2 }, { test1: "1", test2: "2" } ],
          flag: true,
          expected: [ -1 ]
        },
        {
          doc: { test1: 1, test2: 2 },
          examples: [ { fox: true }, { fox: false}, { test1: "something" }, { test99: 1, test2: 2 }, { test1: "1", test2: "2" }, { } ],
          flag: true,
          expected: [ 5 ]
        }
      ];

      tests.forEach(function (data) {
        let query = "RETURN NOOPT(MATCHES(" + JSON.stringify(data.doc) + ", " + JSON.stringify(data.examples);
        if (data.flag !== null) {
          query += ", " + JSON.stringify(data.flag) + "))";
        }
        else {
          query += "))";
        }
        let actual = getQueryResults(query);
        assertEqual(data.expected, actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance function
////////////////////////////////////////////////////////////////////////////////

    testVariancePopulation : function () {
      var data = [
        [ null, [ null ] ],
        [ null, [ null, null, null ] ],
        [ null, [ ] ],
        [ 0, [ 0 ] ],
        [ 0, [ null, 0 ] ],
        [ 0, [ 0.00001 ] ],
        [ 0, [ 1 ] ],
        [ 0, [ 100 ] ],
        [ 0, [ -1 ] ],
        [ 0, [ -10000000 ] ],
        [ 0, [ -100 ] ],
        [ 0, [ null, null, null, -100 ] ],
        [ 2, [ 1, 2, 3, 4, 5 ] ],
        [ 2, [ null, 1, null, 2, 3, null, null, 4, null, 5 ] ],
        [ 0.72727272727273, [ 1, 3, 1, 3, 2, 2, 2, 1, 3, 1, 3 ] ],
        [ 0.8, [ 1, 3, 1, 3, 2, 2, 1, 3, 1, 3 ] ],
        [ 0.88888888888889, [ 1, 3, 1, 3, 2, 1, 3, 1, 3 ] ],
        [ 0.66666666666667, [ 1, 1, 1, 2, 2, 2, 3, 3, 3] ],
        [ 141789.04, [ 12,96, 13, 143, 999 ] ],
        [ 141789.04, [ 12,96, 13, 143, null, 999 ] ],
        [ 491.64405555556, [ 18, -4, 6, 35.2, 63.66, 12.4 ] ],
        [ 1998, [ 1, 10, 100 ] ],
        [ 199800, [ 10, 100, 1000 ] ],
        [ 17538018.75, [ 10, 100, 1000, 10000 ] ],
        [ 15991127264736, [ 10, 100, 1000, 10000, 10000000 ] ],
        [ 17538018.75, [ -10, -100, -1000, -10000 ] ],
        [ 1998, [ -1, -10, -100 ] ],
        [ 6.6666666666667E-7, [ 0.001, 0.002, 0.003 ] ],
        [ 49.753697, [ -0.1, 2.4, -0.004, 12.054, 12.53, -7.35 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN VARIANCE_POPULATION(" + JSON.stringify(value[1]) + ")");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }

        actual = getQueryResults("RETURN NOOPT(VARIANCE_POPULATION(" + JSON.stringify(value[1]) + "))");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance function
////////////////////////////////////////////////////////////////////////////////

    testVariancePopulationInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN VARIANCE_POPULATION()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN VARIANCE_POPULATION([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN VARIANCE_POPULATION(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN VARIANCE_POPULATION(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN VARIANCE_POPULATION(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN VARIANCE_POPULATION(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN VARIANCE_POPULATION({ })"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(VARIANCE_POPULATION())"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(VARIANCE_POPULATION([ ], 2))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(VARIANCE_POPULATION(null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(VARIANCE_POPULATION(false))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(VARIANCE_POPULATION(3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(VARIANCE_POPULATION(\"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(VARIANCE_POPULATION({ }))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance function
////////////////////////////////////////////////////////////////////////////////

    testVarianceSample : function () {
      var data = [
        [ null, [ ] ],
        [ null, [ null ] ],
        [ null, [ null, null ] ],
        [ null, [ 1 ] ],
        [ null, [ 1, null ] ],
        [ 2.5, [ 1, 2, 3, 4, 5 ] ],
        [ 2.5, [ null, null, 1, 2, 3, 4, 5 ] ],
        [ 0.8, [ 1, 3, 1, 3, 2, 2, 2, 1, 3, 1, 3 ] ],   
        [ 0.88888888888889, [ 1, 3, 1, 3, 2, 2, 1, 3, 1, 3 ] ],
        [ 1, [ 1, 3, 1, 3, 2, 1, 3, 1, 3 ] ],
        [ 1, [ 1, 3, 1, 3, 2, 1, 3, 1, 3, null ] ],
        [ 0.75, [ 1, 1, 1, 2, 2, 2, 3, 3, 3 ] ],
        [ 0.75, [ null, 1, null, 1, null, 1, null, null, 2, null, 2, null, 2, null, 3, 3, 3 ] ],
        [ 177236.3, [ 12, 96, 13, 143, 999 ] ], 
        [ 589.97286666667, [ 18, -4, 6, 35.2, 63.66, 12.4 ] ],  
        [ 2997, [ 1, 10, 100 ] ],
        [ 2997, [ 1, 10, 100, null ] ],
        [ 299700, [ 10, 100, 1000 ] ],
        [ 23384025, [ 10, 100, 1000, 10000 ] ],
        [ 19988909080920, [ 10, 100, 1000, 10000, 10000000 ] ],
        [ 23384025, [ -10, -100, -1000, -10000 ] ],  
        [ 2997, [ -1, -10, -100 ] ], 
        [ 1.0E-6, [ 0.001, 0.002, 0.003 ] ],
        [ 59.7044364, [ -0.1, 2.4, -0.004, 12.054, 12.53, -7.35 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN VARIANCE_SAMPLE(" + JSON.stringify(value[1]) + ")");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }

        actual = getQueryResults("RETURN NOOPT(VARIANCE_SAMPLE(" + JSON.stringify(value[1]) + "))");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance function
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN VARIANCE_SAMPLE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN VARIANCE_SAMPLE([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN VARIANCE_SAMPLE(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN VARIANCE_SAMPLE(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN VARIANCE_SAMPLE(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN VARIANCE_SAMPLE(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN VARIANCE_SAMPLE({ })"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(VARIANCE_SAMPLE())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(VARIANCE_SAMPLE([ ], 2))");
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(VARIANCE_SAMPLE(null))");
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(VARIANCE_SAMPLE(false))");
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(VARIANCE_SAMPLE(3))");
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(VARIANCE_SAMPLE(\"yes\"))");
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(VARIANCE_SAMPLE({ }))");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev function
////////////////////////////////////////////////////////////////////////////////

    testStddevPopulation : function () {
      var data = [
        [ null, [ null ] ],
        [ null, [ null, null, null ] ],
        [ null, [ ] ],
        [ 0, [ 0 ] ],
        [ 0, [ null, 0 ] ],
        [ 0, [ 0.00001 ] ],
        [ 0, [ 1 ] ],
        [ 0, [ 100 ] ],
        [ 0, [ -1 ] ],
        [ 0, [ -10000000 ] ],
        [ 0, [ -100 ] ],
        [ 0, [ null, null, null, -100 ] ],
        [ 1.4142135623731, [ 1, 2, 3, 4, 5 ] ],
        [ 1.4142135623731, [ null, 1, null, 2, 3, null, null, 4, null, 5 ] ],
        [ 0.85280286542244, [ 1, 3, 1, 3, 2, 2, 2, 1, 3, 1, 3 ] ],
        [ 0.89442719099992, [ 1, 3, 1, 3, 2, 2, 1, 3, 1, 3 ] ],
        [ 0.94280904158206, [ 1, 3, 1, 3, 2, 1, 3, 1, 3 ] ],
        [ 0.81649658092773, [ 1, 1, 1, 2, 2, 2, 3, 3, 3] ],
        [ 376.54885473203, [ 12,96, 13, 143, 999 ] ],
        [ 376.54885473203, [ 12,96, 13, 143, null, 999 ] ],
        [ 22.173047953666, [ 18, -4, 6, 35.2, 63.66, 12.4 ] ],
        [ 44.698993277254, [ 1, 10, 100 ] ],
        [ 446.98993277254, [ 10, 100, 1000 ] ],
        [ 4187.8417770971, [ 10, 100, 1000, 10000 ] ],
        [ 3998890.7542887, [ 10, 100, 1000, 10000, 10000000 ] ],
        [ 4187.8417770971, [ -10, -100, -1000, -10000 ] ],
        [ 44.698993277254, [ -1, -10, -100 ] ],
        [ 0.00081649658092773, [ 0.001, 0.002, 0.003 ] ],
        [ 7.0536300583458, [ -0.1, 2.4, -0.004, 12.054, 12.53, -7.35 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN STDDEV_POPULATION(" + JSON.stringify(value[1]) + ")");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }

        actual = getQueryResults("RETURN NOOPT(STDDEV_POPULATION(" + JSON.stringify(value[1]) + "))");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev function
////////////////////////////////////////////////////////////////////////////////

    testStddevPopulationInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN STDDEV_POPULATION()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN STDDEV_POPULATION([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN STDDEV_POPULATION(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN STDDEV_POPULATION(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN STDDEV_POPULATION(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN STDDEV_POPULATION(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN STDDEV_POPULATION({ })"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(STDDEV_POPULATION())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(STDDEV_POPULATION([ ], 2))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(STDDEV_POPULATION(null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(STDDEV_POPULATION(false))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(STDDEV_POPULATION(3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(STDDEV_POPULATION(\"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(STDDEV_POPULATION({ }))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev function
////////////////////////////////////////////////////////////////////////////////

    testStddevSample : function () {
      var data = [
        [ null, [ null ] ],
        [ null, [ null, null, null ] ],
        [ null, [ ] ],
        [ null, [ 0 ] ],
        [ null, [ null, 0 ] ],
        [ null, [ 0.00001 ] ],
        [ null, [ 1 ] ],
        [ null, [ 100 ] ],
        [ null, [ -1 ] ],
        [ null, [ -10000000 ] ],
        [ null, [ -100 ] ],
        [ null, [ null, null, null, -100 ] ],
        [ 1.5811, [ 1, 2, 3, 4, 5 ] ],
        [ 1.5811, [ null, 1, null, 2, 3, null, null, 4, null, 5 ] ],
        [ 0.89443, [ 1, 3, 1, 3, 2, 2, 2, 1, 3, 1, 3 ] ],
        [ 0.94281, [ 1, 3, 1, 3, 2, 2, 1, 3, 1, 3 ] ],
        [ 1.0, [ 1, 3, 1, 3, 2, 1, 3, 1, 3 ] ],
        [ 0.86603, [ 1, 1, 1, 2, 2, 2, 3, 3, 3] ],
        [ 420.994418 , [ 12,96, 13, 143, 999 ] ],
        [ 420.994418, [ 12,96, 13, 143, null, 999 ] ],
        [ 24.2894, [ 18, -4, 6, 35.2, 63.66, 12.4 ] ],
        [ 54.74486277, [ 1, 10, 100 ] ],
        [ 547.4486277, [ 10, 100, 1000 ] ],
        [ 4835.7031548, [ 10, 100, 1000, 10000 ] ],
        [ 4470895.7806, [ 10, 100, 1000, 10000, 10000000 ] ],
        [ 4835.7031548, [ -10, -100, -1000, -10000 ] ],
        [ 54.74486277, [ -1, -10, -100 ] ],
        [ 0.001, [ 0.001, 0.002, 0.003 ] ],
        [ 7.72686, [ -0.1, 2.4, -0.004, 12.054, 12.53, -7.35 ] ]
      ];

      data.forEach(function (value) {
        var actual = getQueryResults("RETURN STDDEV_SAMPLE(" + JSON.stringify(value[1]) + ")");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }

        actual = getQueryResults("RETURN NOOPT(STDDEV_SAMPLE(" + JSON.stringify(value[1]) + "))");
        if (value[0] === null) {
          assertNull(actual[0]);
        }
        else {
          assertEqual(value[0].toFixed(4), actual[0].toFixed(4));
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev function
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN STDDEV_SAMPLE()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN STDDEV_SAMPLE([ ], 2)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN STDDEV_SAMPLE(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN STDDEV_SAMPLE(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN STDDEV_SAMPLE(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN STDDEV_SAMPLE(\"yes\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN STDDEV_SAMPLE({ })"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(STDDEV_SAMPLE())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN NOOPT(STDDEV_SAMPLE([ ], 2))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(STDDEV_SAMPLE(null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(STDDEV_SAMPLE(false))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(STDDEV_SAMPLE(3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(STDDEV_SAMPLE(\"yes\"))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_ARRAY_EXPECTED.code, "RETURN NOOPT(STDDEV_SAMPLE({ }))"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test IN_RANGE function
////////////////////////////////////////////////////////////////////////////////
    testInRange: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IN_RANGE()");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN IN_RANGE(123, 0, 500, null, true)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN IN_RANGE(123, 0, 500, false, null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN IN_RANGE(123, 0, 500, 1, true)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN IN_RANGE(123, 0, 500, false, 0)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN IN_RANGE(123, 0, 500, [1, 2, 3], true)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN IN_RANGE(123, 0, 500, false, [1,2,3])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN IN_RANGE(123, 0, 500, 'true', true)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN IN_RANGE(123, 0, 500, false, 'false')");
      {
        let res = getQueryResults("RETURN IN_RANGE(5, 1, 10, false, false)");
        assertEqual(1, res.length);  
        assertTrue(res[0]);
      }     
      {
        let res = getQueryResults("RETURN IN_RANGE(5+1, 1+1, 10-1, false, false)");
        assertEqual(1, res.length);  
        assertTrue(res[0]);
      }
      {
        let res = getQueryResults("RETURN IN_RANGE(MAX([5,6]), MIN([1,0]), MAX([0,6]), false, true)");
        assertEqual(1, res.length);  
        assertTrue(res[0]);
      }
      {
        let res = getQueryResults("RETURN IN_RANGE(MAX([5,6]), MIN([1,0]), MAX([0,6]), false, false)");
        assertEqual(1, res.length);  
        assertFalse(res[0]);
      }
      {
        let res = getQueryResults("RETURN IN_RANGE(NOOPT(MAX([5,6])), MIN([1,0]), MAX([0,6]), false, false)");
        assertEqual(1, res.length);  
        assertFalse(res[0]);
      }
      {
        let res = getQueryResults("RETURN IN_RANGE('foo', MIN([1,0]), 'poo', false, false)");
        assertEqual(1, res.length);  
        assertTrue(res[0]);
      }
      {
        let res = getQueryResults("RETURN IN_RANGE('foo', null, 'poo', false, false)");
        assertEqual(1, res.length);  
        assertTrue(res[0]);
      }
      {
        let res = getQueryResults("RETURN IN_RANGE(123, null, 'poo', false, false)");
        assertEqual(1, res.length);  
        assertTrue(res[0]);
      }
      {
        let res = getQueryResults("RETURN IN_RANGE({a:1}, null, 'poo', false, false)");
        assertEqual(1, res.length);  
        assertFalse(res[0]);
      }
      {
        let res = getQueryResults("RETURN IN_RANGE('foo', 'boo', 'poo', false, false)");
        assertEqual(1, res.length);  
        assertTrue(res[0]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-existing functions
////////////////////////////////////////////////////////////////////////////////

    testNonExisting : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN FOO()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN BAR()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN PENG(true)"); 
    }
  
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlFunctionsTestSuite);

return jsunity.done();
