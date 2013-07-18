////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, range optimisations
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
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlRangesTestSuite () {

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
/// @brief test merging of IN
////////////////////////////////////////////////////////////////////////////////

    testInListAnd1 : function () {
      var expected = [ 1, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i IN [ 1, 3, 5 ] && i IN [ 2, 3, 4, 1 ] RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merging of IN
////////////////////////////////////////////////////////////////////////////////

    testInListAnd2 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i IN [ 1, 3, 5 ] && i IN [ 2, 4, 6, 8 ] RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merging of IN
////////////////////////////////////////////////////////////////////////////////

    testInListOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i IN [ 1, 3, 5 ] || i IN [ 2, 3, 4, 1 ] RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merging of IN
////////////////////////////////////////////////////////////////////////////////

    testInListOr2 : function () {
      var expected = [ 1, 2, 6, 9 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i IN [ 1, 2 ] || i IN [ 9, 6 ] RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqAnd1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqAnd5 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqAnd6 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqAnd8 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqAnd9 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqOr1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqOr2 : function () {
      var expected = [ 1, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqOr3 : function () {
      var expected = [ 1, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqOr4 : function () {
      var expected = [ 1, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqOr5 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqOr6 : function () {
      var expected = [ 4, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqOr7 : function () {
      var expected = [ 1, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqOr8 : function () {
      var expected = [ 4, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, ==
////////////////////////////////////////////////////////////////////////////////

    testEqEqOr9 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeAnd2 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeAnd3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeAnd4 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeAnd6 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeAnd7 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeAnd8 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeOr2 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeOr4 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeOr7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeOr8 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, !=
////////////////////////////////////////////////////////////////////////////////

    testEqNeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtAnd4 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtAnd6 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtAnd7 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtAnd8 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtOr2 : function () {
      var expected = [ 1, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtOr3 : function () {
      var expected = [ 1, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtOr4 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtOr5 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtOr6 : function () {
      var expected = [ 4, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtOr7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtOr8 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >
////////////////////////////////////////////////////////////////////////////////

    testEqGtOr9 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeAnd1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeAnd4 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeAnd5 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeAnd6 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeAnd7 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeAnd8 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeAnd9 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeOr2 : function () {
      var expected = [ 1, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeOr3 : function () {
      var expected = [ 1, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeOr5 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeOr6 : function () {
      var expected = [ 4, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeOr8 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, >=
////////////////////////////////////////////////////////////////////////////////

    testEqGeOr9 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtAnd2 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtAnd3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtAnd6 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtAnd8 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtOr1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtOr2 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtOr4 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtOr5 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtOr7 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtOr8 : function () {
      var expected = [ 1, 2, 3, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <
////////////////////////////////////////////////////////////////////////////////

    testEqLtOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeAnd1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeAnd2 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeAnd3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeAnd5 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeAnd6 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeAnd8 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeAnd9 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeOr1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeOr2 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 1 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeOr4 : function () {
      var expected = [ 1, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeOr5 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 4 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeOr7 : function () {
      var expected = [ 1, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of ==, <=
////////////////////////////////////////////////////////////////////////////////

    testEqLeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i == 8 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqAnd2 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqAnd3 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqAnd4 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqAnd6 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqAnd7 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqAnd8 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqOr2 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqOr3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqOr4 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqOr6 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, ==
////////////////////////////////////////////////////////////////////////////////

    testNeEqOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeAnd1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeAnd2 : function () {
      var expected = [ 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeAnd3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeAnd4 : function () {
      var expected = [ 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeAnd5 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeAnd6 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeAnd7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeAnd8 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeAnd9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeOr1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeOr5 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, !=
////////////////////////////////////////////////////////////////////////////////

    testNeNeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtAnd1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtAnd2 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtAnd3 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtAnd4 : function () {
      var expected = [ 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtAnd5 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtAnd6 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtAnd7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtAnd8 : function () {
      var expected = [ 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtAnd9 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtOr1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtOr2 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtOr3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtOr5 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtOr6 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >
////////////////////////////////////////////////////////////////////////////////

    testNeGtOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeAnd1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeAnd2 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeAnd3 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeAnd4 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeAnd5 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeAnd6 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeAnd7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeAnd8 : function () {
      var expected = [ 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeAnd9 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeOr2 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeOr3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeOr6 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, >=
////////////////////////////////////////////////////////////////////////////////

    testNeGeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtAnd2 : function () {
      var expected = [ 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtAnd3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtAnd5 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtAnd6 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtAnd8 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtAnd9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtOr1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtOr4 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtOr5 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <
////////////////////////////////////////////////////////////////////////////////

    testNeLtOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeAnd2 : function () {
      var expected = [ 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeAnd3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeAnd4 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeAnd5 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeAnd6 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeAnd7 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeAnd8 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeAnd9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 1 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeOr4 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 4 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of !=, <=
////////////////////////////////////////////////////////////////////////////////

    testNeLeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i != 8 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqAnd2 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqAnd3 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqAnd6 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqAnd8 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqOr2 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqOr3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqOr4 : function () {
      var expected = [ 1, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqOr5 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqOr6 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqOr7 : function () {
      var expected = [ 1, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqOr8 : function () {
      var expected = [ 4, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, ==
////////////////////////////////////////////////////////////////////////////////

    testGtEqOr9 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeAnd1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeAnd2 : function () {
      var expected = [ 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeAnd3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeAnd4 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeAnd5 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeAnd6 : function () {
      var expected = [ 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeAnd7 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeAnd8 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeAnd9 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeOr1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeOr4 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeOr5 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeOr7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeOr8 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, !=
////////////////////////////////////////////////////////////////////////////////

    testGtNeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtAnd1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtAnd2 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtAnd3 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtAnd4 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtAnd5 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtAnd6 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtAnd7 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtAnd8 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtAnd9 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtOr1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtOr2 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtOr3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtOr4 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtOr5 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtOr6 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtOr7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtOr8 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >
////////////////////////////////////////////////////////////////////////////////

    testGtGtOr9 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeAnd1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeAnd2 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeAnd3 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeAnd4 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeAnd5 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeAnd6 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeAnd7 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeAnd8 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeAnd9 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeOr2 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeOr3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeOr5 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeOr6 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeOr8 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, >=
////////////////////////////////////////////////////////////////////////////////

    testGtGeOr9 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtAnd2 : function () {
      var expected = [ 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtAnd3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtAnd6 : function () {
      var expected = [ 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtAnd8 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtOr1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtOr4 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtOr5 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtOr7 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtOr8 : function () {
      var expected = [ 1, 2, 3, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <
////////////////////////////////////////////////////////////////////////////////

    testGtLtOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeAnd2 : function () {
      var expected = [ 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeAnd3 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeAnd6 : function () {
      var expected = [ 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeAnd8 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeOr4 : function () {
      var expected = [ 1, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 4 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeOr7 : function () {
      var expected = [ 1, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >, <=
////////////////////////////////////////////////////////////////////////////////

    testGtLeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 8 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqAnd1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqAnd2 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqAnd3 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqAnd5 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqAnd6 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqAnd8 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqAnd9 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqOr4 : function () {
      var expected = [ 1, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqOr5 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqOr6 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqOr7 : function () {
      var expected = [ 1, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqOr8 : function () {
      var expected = [ 4, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, ==
////////////////////////////////////////////////////////////////////////////////

    testGeEqOr9 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeAnd1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeAnd2 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeAnd3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeAnd4 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeAnd5 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeAnd6 : function () {
      var expected = [ 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeAnd7 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeAnd8 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeAnd9 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeOr4 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeOr7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeOr8 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, !=
////////////////////////////////////////////////////////////////////////////////

    testGeNeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtAnd1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtAnd2 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtAnd3 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtAnd4 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtAnd5 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtAnd6 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtAnd7 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtAnd8 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtAnd9 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtOr4 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtOr5 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtOr6 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtOr7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtOr8 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >
////////////////////////////////////////////////////////////////////////////////

    testGeGtOr9 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeAnd1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeAnd2 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeAnd3 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeAnd4 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeAnd5 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeAnd6 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeAnd7 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeAnd8 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeAnd9 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeOr5 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeOr6 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeOr8 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, >=
////////////////////////////////////////////////////////////////////////////////

    testGeGeOr9 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtAnd2 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtAnd3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtAnd6 : function () {
      var expected = [ 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtAnd8 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtOr4 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtOr7 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtOr8 : function () {
      var expected = [ 1, 2, 3, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <
////////////////////////////////////////////////////////////////////////////////

    testGeLtOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeAnd1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeAnd2 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeAnd3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeAnd5 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeAnd6 : function () {
      var expected = [ 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeAnd8 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeAnd9 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeOr2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 1 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeOr4 : function () {
      var expected = [ 1, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 4 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeOr7 : function () {
      var expected = [ 1, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of >=, <=
////////////////////////////////////////////////////////////////////////////////

    testGeLeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i >= 8 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqAnd4 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqAnd6 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqAnd7 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqAnd8 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqOr1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqOr2 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqOr3 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqOr4 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqOr5 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqOr6 : function () {
      var expected = [ 1, 2, 3, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, ==
////////////////////////////////////////////////////////////////////////////////

    testLtEqOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeAnd4 : function () {
      var expected = [ 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeAnd5 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeAnd6 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeAnd7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeAnd8 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeAnd9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeOr1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeOr2 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeOr5 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, !=
////////////////////////////////////////////////////////////////////////////////

    testLtNeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtAnd4 : function () {
      var expected = [ 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtAnd6 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtAnd7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtAnd8 : function () {
      var expected = [ 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtOr1 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtOr2 : function () {
      var expected = [ 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtOr3 : function () {
      var expected = [ 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtOr5 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtOr6 : function () {
      var expected = [ 1, 2, 3, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >
////////////////////////////////////////////////////////////////////////////////

    testLtGtOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeAnd4 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeAnd6 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeAnd7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeAnd8 : function () {
      var expected = [ 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeOr2 : function () {
      var expected = [ 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeOr3 : function () {
      var expected = [ 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeOr6 : function () {
      var expected = [ 1, 2, 3, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, >=
////////////////////////////////////////////////////////////////////////////////

    testLtGeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtAnd5 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtAnd6 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtAnd8 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtAnd9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtOr1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtOr2 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtOr4 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtOr5 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <
////////////////////////////////////////////////////////////////////////////////

    testLtLtOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeAnd4 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeAnd5 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeAnd6 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeAnd7 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeAnd8 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeAnd9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeOr1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeOr2 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 1 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeOr4 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeOr5 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 4 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <, <=
////////////////////////////////////////////////////////////////////////////////

    testLtLeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i < 8 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqAnd1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqAnd4 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqAnd5 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqAnd6 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqAnd7 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqAnd8 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqAnd9 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqOr1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqOr2 : function () {
      var expected = [ 1, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqOr3 : function () {
      var expected = [ 1, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqOr4 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqOr5 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqOr6 : function () {
      var expected = [ 1, 2, 3, 4, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i == 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i == 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, ==
////////////////////////////////////////////////////////////////////////////////

    testLeEqOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i == 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeAnd2 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeAnd3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeAnd4 : function () {
      var expected = [ 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeAnd5 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeAnd6 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeAnd7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeAnd8 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeAnd9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeOr2 : function () {
      var expected = [ 1, 2, 3, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i != 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i != 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, !=
////////////////////////////////////////////////////////////////////////////////

    testLeNeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i != 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtAnd4 : function () {
      var expected = [ 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtAnd5 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtAnd6 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtAnd7 : function () {
      var expected = [ 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtAnd8 : function () {
      var expected = [ 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtAnd9 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtOr2 : function () {
      var expected = [ 1, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtOr3 : function () {
      var expected = [ 1, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtOr6 : function () {
      var expected = [ 1, 2, 3, 4, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i > 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i > 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >
////////////////////////////////////////////////////////////////////////////////

    testLeGtOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i > 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeAnd1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeAnd2 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeAnd3 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeAnd4 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeAnd5 : function () {
      var expected = [ 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeAnd6 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeAnd7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeAnd8 : function () {
      var expected = [ 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeAnd9 : function () {
      var expected = [ 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeOr1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeOr2 : function () {
      var expected = [ 1, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeOr3 : function () {
      var expected = [ 1, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeOr4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeOr5 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i >= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i >= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, >=
////////////////////////////////////////////////////////////////////////////////

    testLeGeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i >= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtAnd1 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtAnd2 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtAnd3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtAnd4 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtAnd5 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtAnd6 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtAnd7 : function () {
      var expected = [  ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtAnd8 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtAnd9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtOr1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtOr2 : function () {
      var expected = [ 1, 2, 3 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtOr4 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtOr5 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i < 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i < 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <
////////////////////////////////////////////////////////////////////////////////

    testLeLtOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i < 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeAnd1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeAnd2 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeAnd3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeAnd4 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeAnd5 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeAnd6 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeAnd7 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeAnd8 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test && merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeAnd9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 && i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeOr1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeOr2 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeOr3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 1 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeOr4 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeOr5 : function () {
      var expected = [ 1, 2, 3, 4 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeOr6 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 4 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeOr7 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i <= 1 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeOr8 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i <= 4 RETURN i");
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test || merging of <=, <=
////////////////////////////////////////////////////////////////////////////////

    testLeLeOr9 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];

      var actual = getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i <= 8 || i <= 8 RETURN i");
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlRangesTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
