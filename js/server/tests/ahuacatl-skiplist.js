////////////////////////////////////////////////////////////////////////////////
/// @brief tests for Ahuacatl, skiplist index queries
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
var QUERY = internal.AQL_QUERY;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlSkiplistTestSuite () {
  var errors = internal.errors;
  var skiplist;

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query, bindVars) {
    var cursor = QUERY(query, bindVars);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, bindVars) {
    var result = executeQuery(query, bindVars).getRows();
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
      internal.db._drop("UnitTestsAhuacatlSkiplist");
      skiplist = internal.db._create("UnitTestsAhuacatlSkiplist");

      for (var i = 1; i <= 5; ++i) {
        for (var j = 1; j <= 5; ++j) {
          skiplist.save({ "a" : i, "b": j });
        }
      }

      skiplist.ensureSkiplist("a", "b");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop("UnitTestsAhuacatlSkiplist");
      skiplist = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field without results
////////////////////////////////////////////////////////////////////////////////

    testEqSingleVoid1 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 99 RETURN v");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field without results
////////////////////////////////////////////////////////////////////////////////

    testEqSingleVoid2 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 99 == v.a RETURN v");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 1 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 == v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle3 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 5 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle4 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 5 == v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with greater than 
////////////////////////////////////////////////////////////////////////////////

    testGtSingle1 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 4 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with greater than 
////////////////////////////////////////////////////////////////////////////////

    testGtSingle2 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 4 < v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with greater equal 
////////////////////////////////////////////////////////////////////////////////

    testGeSingle1 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a >= 5 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with greater equal 
////////////////////////////////////////////////////////////////////////////////

    testGeSingle2 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 5 <= v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with less than 
////////////////////////////////////////////////////////////////////////////////

    testLtSingle1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a < 2 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with less than 
////////////////////////////////////////////////////////////////////////////////

    testLtSingle2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 2 > v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with greater equal 
////////////////////////////////////////////////////////////////////////////////

    testGeSingle1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a <= 1 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with greater equal 
////////////////////////////////////////////////////////////////////////////////

    testGeSingle2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 >= v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a >= 1 && v.a <= 2 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 <= v.a && 2 >= v.a SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle3 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 1 && v.a < 2 RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle4 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 < v.a && 2 > v.a RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle5 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 <= v.a && 2 > v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle6 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 <= v.a && 2 > v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle7 : function () {
      var expected = [ [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 1 && v.a <= 2 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle8 : function () {
      var expected = [ [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 < v.a && 2 >= v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiVoid1 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 99 && v.b == 1 RETURN v");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiVoid2 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 99 == v.a && 1 == v.b RETURN v");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiVoid3 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 1 && v.b == 99 RETURN v");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiVoid4 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 == v.a && 99 == v.b RETURN v");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiAll1 : function () {
      for (var i = 1; i <= 5; ++i) {
        for (var j = 1; j <=5; ++j) {
          var expected = [ [ i, j ] ];
          var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == @a && v.b == @b RETURN [ v.a, v.b ]", { "a" : i, "b" : j });

          assertEqual(expected, actual);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiAll2 : function () {
      for (var i = 1; i <= 5; ++i) {
        for (var j = 1; j <=5; ++j) {
          var expected = [ [ i, j ] ];
          var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER @a == v.a && @b == v.b RETURN [ v.a, v.b ]", { "a" : i, "b" : j });

          assertEqual(expected, actual);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqGt1 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 1 && v.b > 2 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqGt2 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 == v.a && 2 < v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqGt3 : function () {
      var expected = [ [ 4, 4 ], [ 4, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 4 && v.b > 3 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqGt4 : function () {
      var expected = [ [ 4, 4 ], [ 4, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 4 == v.a && 3 < v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLt1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 1 && v.b < 4 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLt2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 == v.a && 4 > v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLt3 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 5 && v.b < 5 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLt4 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 5 == v.a && 5 > v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLe1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 1 && v.b <= 3 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLe2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 1 == v.a && 3 >= v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLe3 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ]  ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 5 && v.b <= 4 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLe4 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ]  ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 5 == v.a && 4 >= v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtlt1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 2, 1 ], [ 2, 2 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a < 3 && v.b < 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtlt2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 2, 1 ], [ 2, 2 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 > v.a && 3 > v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeLe1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 3, 1 ], [ 3, 2 ], [ 3, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a <= 3 && v.b <= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeLe2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 3, 1 ], [ 3, 2 ], [ 3, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 >= v.a && 3 >= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtLe1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a < 3 && v.b <= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtLe2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 > v.a && 3 >= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeLt1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 2, 1 ], [ 2, 2 ], [ 3, 1 ], [ 3, 2 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a <= 3 && v.b < 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeLt2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 2, 1 ], [ 2, 2 ], [ 3, 1 ], [ 3, 2 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 >= v.a && 3 > v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtEq1 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a < 3 && v.b == 4 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtEq2 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 > v.a && 4 == v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeEq1 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ], [ 3, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a <= 3 && v.b == 4 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeEq2 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ], [ 3, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 >= v.a && 4 == v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtGt1 : function () {
      var expected = [ [ 1, 4 ], [ 1, 5 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a < 3 && v.b > 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtGt2 : function () {
      var expected = [ [ 1, 4 ], [ 1, 5 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 > v.a && 3 < v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeGe1 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ], [ 3, 3 ], [ 3, 4 ], [ 3, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a <= 3 && v.b >= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeGe2 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ], [ 3, 3 ], [ 3, 4 ], [ 3, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 >= v.a && 3 <= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtGe1 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a < 3 && v.b >= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtGe2 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 > v.a && 3 <= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeGt1 : function () {
      var expected = [ [ 1, 4 ], [ 1, 5 ], [ 2, 4 ], [ 2, 5 ], [ 3, 4 ], [ 3, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a <= 3 && v.b > 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeGt2 : function () {
      var expected = [ [ 1, 4 ], [ 1, 5 ], [ 2, 4 ], [ 2, 5 ], [ 3, 4 ], [ 3, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 >= v.a && 3 < v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtlt1 : function () {
      var expected = [ [ 4, 1 ], [ 4, 2 ], [ 5, 1 ], [ 5, 2 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 3 && v.b < 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtlt2 : function () {
      var expected = [ [ 4, 1 ], [ 4, 2 ], [ 5, 1 ], [ 5, 2 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 < v.a && 3 > v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeLe1 : function () {
      var expected = [ [ 3, 1 ], [ 3, 2 ], [ 3, 3 ], [ 4, 1 ], [ 4, 2 ], [ 4, 3 ], [ 5, 1 ], [ 5, 2 ], [ 5, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a >= 3 && v.b <= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeLe2 : function () {
      var expected = [ [ 3, 1 ], [ 3, 2 ], [ 3, 3 ], [ 4, 1 ], [ 4, 2 ], [ 4, 3 ], [ 5, 1 ], [ 5, 2 ], [ 5, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 <= v.a && 3 >= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtLe1 : function () {
      var expected = [ [ 4, 1 ], [ 4, 2 ], [ 4, 3 ], [ 5, 1 ], [ 5, 2 ], [ 5, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 3 && v.b <= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtLe2 : function () {
      var expected = [ [ 4, 1 ], [ 4, 2 ], [ 4, 3 ], [ 5, 1 ], [ 5, 2 ], [ 5, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 < v.a && 3 >= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeLt1 : function () {
      var expected = [ [ 3, 1 ], [ 3, 2 ], [ 4, 1 ], [ 4, 2 ], [ 5, 1 ], [ 5, 2 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a >= 3 && v.b < 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeLt2 : function () {
      var expected = [ [ 3, 1 ], [ 3, 2 ], [ 4, 1 ], [ 4, 2 ], [ 5, 1 ], [ 5, 2 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 <= v.a && 3 > v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtEq1 : function () {
      var expected = [ [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 3 && v.b == 4 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtEq2 : function () {
      var expected = [ [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 < v.a && 4 == v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeEq1 : function () {
      var expected = [ [ 3, 4 ], [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a >= 3 && v.b == 4 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeEq2 : function () {
      var expected = [ [ 3, 4 ], [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 <= v.a && 4 == v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtGt1 : function () {
      var expected = [ [ 4, 4 ], [ 4, 5 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 3 && v.b > 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtGt2 : function () {
      var expected = [ [ 4, 4 ], [ 4, 5 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 < v.a && 3 < v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeGe1 : function () {
      var expected = [ [ 3, 3 ], [ 3, 4 ], [ 3, 5 ], [ 4, 3 ], [ 4, 4 ], [ 4, 5 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a >= 3 && v.b >= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeGe2 : function () {
      var expected = [ [ 3, 3 ], [ 3, 4 ], [ 3, 5 ], [ 4, 3 ], [ 4, 4 ], [ 4, 5 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 <= v.a && 3 <= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtGe1 : function () {
      var expected = [ [ 4, 3 ], [ 4, 4 ], [ 4, 5 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 3 && v.b >= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtGe2 : function () {
      var expected = [ [ 4, 3 ], [ 4, 4 ], [ 4, 5 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 < v.a && 3 <= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeGt1 : function () {
      var expected = [ [ 3, 4 ], [ 3, 5 ], [ 4, 4 ], [ 4, 5 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a >= 3 && v.b > 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple skiplist fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeGt2 : function () {
      var expected = [ [ 3, 4 ], [ 3, 5 ], [ 4, 4 ], [ 4, 5 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER 3 <= v.a && 3 < v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with a constant
////////////////////////////////////////////////////////////////////////////////

    testRefConst1 : function () {
      var expected = [ [ 3, 1 ], [ 3, 2 ], [ 3, 3 ], [ 3, 4 ], [ 3, 5 ] ];
      var actual = getQueryResults("LET x = 3 FOR v IN " + skiplist.name() + " FILTER v.a == x SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with a constant
////////////////////////////////////////////////////////////////////////////////

    testRefConst2 : function () {
      var expected = [ [ 3, 5 ] ];
      var actual = getQueryResults("LET x = 3 LET y = 5 FOR v IN " + skiplist.name() + " FILTER v.a == x && v.b == y RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefSingle1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v1 IN " + skiplist.name() + " FOR v2 IN " + skiplist.name() + " FILTER v1.a == 1 && v2.a == v1.a && v1.b == 1 SORT v1.a, v2.b RETURN [ v1.a, v2.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefSingle2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v1 IN " + skiplist.name() + " FOR v2 IN " + skiplist.name() + " FILTER 1 == v1.a && v1.a == v2.a && 1 == v1.b SORT v1.a, v2.b RETURN [ v1.a, v2.b ]");

      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSkiplistTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
