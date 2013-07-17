////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, ref access
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

function ahuacatlRefAccessVariableTestSuite () {

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function runQuery (query) {
    return getQueryResults("FOR i IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FOR j IN [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER " + query + " RETURN i");
  }

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
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefEq : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      assertEqual(expected, runQuery("(i == j)"));
      assertEqual(expected, runQuery("(j == i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefGt : function () {
      var expected = [ 2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10 ];

      assertEqual(expected, runQuery("(i > j)"));
      assertEqual(expected, runQuery("(j < i)")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefGe : function () {
      var expected = [ 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 ];

      assertEqual(expected, runQuery("(i >= j)"));
      assertEqual(expected, runQuery("(j <= i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefLt : function () {
      var expected = [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8, 8, 9 ];

      assertEqual(expected, runQuery("(i < j)"));
      assertEqual(expected, runQuery("(j > i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefLe : function () {
      var expected = [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 10 ];

      assertEqual(expected, runQuery("(i <= j)"));
      assertEqual(expected, runQuery("(j >= i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      assertEqual(expected, runQuery("(i == j && i >= j)"));
      assertEqual(expected, runQuery("(j == i && j <= i)"));
      assertEqual(expected, runQuery("(i >= j && i == j)"));
      assertEqual(expected, runQuery("(j <= i && j == i)"));

      assertEqual(expected, runQuery("(i == j && i <= j)"));
      assertEqual(expected, runQuery("(j == i && j >= i)"));
      assertEqual(expected, runQuery("(i <= j && i == j)"));
      assertEqual(expected, runQuery("(j >= i && j == i)"));

      assertEqual(expected, runQuery("(i == j && i == j)"));
      assertEqual(expected, runQuery("(j == i && j == i)"));
      
      assertEqual(expected, runQuery("(i <= j && i >= j)"));
      assertEqual(expected, runQuery("(j >= i && j <= i)"));
      assertEqual(expected, runQuery("(i >= j && i <= j)"));
      assertEqual(expected, runQuery("(j <= i && j >= i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd2 : function () {
      var expected = [ 2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10 ];

      assertEqual(expected, runQuery("(i > j && i >= j)"));
      assertEqual(expected, runQuery("(j < i && j <= i)"));
      assertEqual(expected, runQuery("(i >= j && i > j)"));
      assertEqual(expected, runQuery("(j <= i && j < i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd3 : function () {
      var expected = [ 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 ];

      assertEqual(expected, runQuery("(i >= j && i >= j)"));
      assertEqual(expected, runQuery("(j <= i && j <= i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd4 : function () {
      var expected = [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8, 8, 9 ];

      assertEqual(expected, runQuery("(i < j && i <= j)"));
      assertEqual(expected, runQuery("(j > i && j >= i)"));
      assertEqual(expected, runQuery("(i <= j && i < j)"));
      assertEqual(expected, runQuery("(j >= i && j > i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd5 : function () {
      var expected = [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 10 ];

      assertEqual(expected, runQuery("(i <= j && i <= j)"));
      assertEqual(expected, runQuery("(j >= i && j >= i)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd6 : function () {
      var expected = [ ];

      assertEqual(expected, runQuery("(i > j && i < j)"));
      assertEqual(expected, runQuery("(j < i && j > i)"));
      assertEqual(expected, runQuery("(i < j && i > j)"));
      assertEqual(expected, runQuery("(j > i && j < i)"));
      
      assertEqual(expected, runQuery("(i >= j && i < j)"));
      assertEqual(expected, runQuery("(j <= i && j > i)"));
      assertEqual(expected, runQuery("(i < j && i >= j)"));
      assertEqual(expected, runQuery("(j > i && j <= i)"));
      
      assertEqual(expected, runQuery("(i > j && i <= j)"));
      assertEqual(expected, runQuery("(j < i && j >= i)"));
      assertEqual(expected, runQuery("(i <= j && i > j)"));
      assertEqual(expected, runQuery("(j >= i && j < i)"));
    }

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlRefAccessVariableTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
