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
    var cursor = AHUACATL_RUN(query, bindVars);
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
      skiplist = internal.db.UnitTestsAhuacatlSkiplist;

      if (skiplist.count() == 0) {
        for (var i = 1; i <= 5; ++i) {
          for (var j = 1; j <= 5; ++j) {
            skiplist.save({ "a" : i, "b": j });
          }
        }

        skiplist.ensureSkiplist("a", "b");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
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
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 5 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with greater than 
////////////////////////////////////////////////////////////////////////////////

    testGtSingle : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 4 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with greater equal 
////////////////////////////////////////////////////////////////////////////////

    testGeSingle : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a >= 5 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with less than 
////////////////////////////////////////////////////////////////////////////////

    testLtSingle : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a < 2 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with greater equal 
////////////////////////////////////////////////////////////////////////////////

    testGeSingle : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a <= 1 SORT v.b RETURN [ v.a, v.b ]");

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
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 1 && v.a < 2 RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle3 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a >= 1 && v.a < 2 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first skiplist index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle3 : function () {
      var expected = [ [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a > 1 && v.a <= 2 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test two skiplist index fields with equality 
////////////////////////////////////////////////////////////////////////////////

    testEqMultiAll : function () {
      for (var i = 1; i <= 5; ++i) {
        for (var j = 1; j <=5; ++j) {
          var expected = [ [ i, j ] ];
          var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == @a && v.b == @b RETURN [ v.a, v.b ]", { "a" : i, "b" : j });

          assertEqual(expected, actual);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test two skiplist index fields with different operators
////////////////////////////////////////////////////////////////////////////////

    testEqGt1 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 1 && v.b > 2 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test two skiplist index fields with different operators
////////////////////////////////////////////////////////////////////////////////

    testEqGt2 : function () {
      var expected = [ [ 4, 4 ], [ 4, 5 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 4 && v.b > 3 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test two skiplist index fields with different operators
////////////////////////////////////////////////////////////////////////////////

    testEqLt1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 1 && v.b < 4 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test two skiplist index fields with different operators
////////////////////////////////////////////////////////////////////////////////

    testEqLt2 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 5 && v.b < 5 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test two skiplist index fields with different operators
////////////////////////////////////////////////////////////////////////////////

    testEqLe1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ] ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 1 && v.b <= 3 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test two skiplist index fields with different operators
////////////////////////////////////////////////////////////////////////////////

    testEqLe2 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ]  ];
      var actual = getQueryResults("FOR v IN " + skiplist.name() + " FILTER v.a == 5 && v.b <= 4 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },
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
