/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

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

var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlRefAccessAttributeTestSuite () {
  var collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function runQuery (query) {
    return getQueryResults("FOR i IN " + collection.name() + " FOR j IN " + collection.name() + " FILTER " + query + " SORT i.val RETURN i.val");
  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      internal.db._drop("UnitTestsAhuacatlRefAccess");
      collection = internal.db._create("UnitTestsAhuacatlRefAccess");

      let docs = [];
      for (var i = 1; i <= 10; ++i) {
        docs.push({ "val" : i });
      }
      collection.insert(docs);

      collection.ensureSkiplist("val");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.db._drop("UnitTestsAhuacatlRefAccess");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefEq : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      assertEqual(expected, runQuery("(i.val == j.val)"));
      assertEqual(expected, runQuery("(j.val == i.val)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefGt : function () {
      var expected = [ 2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10 ];

      assertEqual(expected, runQuery("(i.val > j.val)"));
      assertEqual(expected, runQuery("(j.val < i.val)")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefGe : function () {
      var expected = [ 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 ];

      assertEqual(expected, runQuery("(i.val >= j.val)"));
      assertEqual(expected, runQuery("(j.val <= i.val)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefLt : function () {
      var expected = [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8, 8, 9 ];

      assertEqual(expected, runQuery("(i.val < j.val)"));
      assertEqual(expected, runQuery("(j.val > i.val)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefLe : function () {
      var expected = [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 10 ];

      assertEqual(expected, runQuery("(i.val <= j.val)"));
      assertEqual(expected, runQuery("(j.val >= i.val)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];

      assertEqual(expected, runQuery("(i.val == j.val && i.val >= j.val)"));
      assertEqual(expected, runQuery("(j.val == i.val && j.val <= i.val)"));
      assertEqual(expected, runQuery("(i.val >= j.val && i.val == j.val)"));
      assertEqual(expected, runQuery("(j.val <= i.val && j.val == i.val)"));

      assertEqual(expected, runQuery("(i.val == j.val && i.val <= j.val)"));
      assertEqual(expected, runQuery("(j.val == i.val && j.val >= i.val)"));
      assertEqual(expected, runQuery("(i.val <= j.val && i.val == j.val)"));
      assertEqual(expected, runQuery("(j.val >= i.val && j.val == i.val)"));

      assertEqual(expected, runQuery("(i.val == j.val && i.val == j.val)"));
      assertEqual(expected, runQuery("(j.val == i.val && j.val == i.val)"));
      
      assertEqual(expected, runQuery("(i.val <= j.val && i.val >= j.val)"));
      assertEqual(expected, runQuery("(j.val >= i.val && j.val <= i.val)"));
      assertEqual(expected, runQuery("(i.val >= j.val && i.val <= j.val)"));
      assertEqual(expected, runQuery("(j.val <= i.val && j.val >= i.val)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd2 : function () {
      var expected = [ 2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10 ];

      assertEqual(expected, runQuery("(i.val > j.val && i.val >= j.val)"));
      assertEqual(expected, runQuery("(j.val < i.val && j.val <= i.val)"));
      assertEqual(expected, runQuery("(i.val >= j.val && i.val > j.val)"));
      assertEqual(expected, runQuery("(j.val <= i.val && j.val < i.val)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd3 : function () {
      var expected = [ 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 ];

      assertEqual(expected, runQuery("(i.val >= j.val && i.val >= j.val)"));
      assertEqual(expected, runQuery("(j.val <= i.val && j.val <= i.val)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd4 : function () {
      var expected = [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8, 8, 9 ];

      assertEqual(expected, runQuery("(i.val < j.val && i.val <= j.val)"));
      assertEqual(expected, runQuery("(j.val > i.val && j.val >= i.val)"));
      assertEqual(expected, runQuery("(i.val <= j.val && i.val < j.val)"));
      assertEqual(expected, runQuery("(j.val >= i.val && j.val > i.val)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd5 : function () {
      var expected = [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 10 ];

      assertEqual(expected, runQuery("(i.val <= j.val && i.val <= j.val)"));
      assertEqual(expected, runQuery("(j.val >= i.val && j.val >= i.val)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMergeAnd6 : function () {
      var expected = [ ];

      assertEqual(expected, runQuery("(i.val > j.val && i.val < j.val)"));
      assertEqual(expected, runQuery("(j.val < i.val && j.val > i.val)"));
      assertEqual(expected, runQuery("(i.val < j.val && i.val > j.val)"));
      assertEqual(expected, runQuery("(j.val > i.val && j.val < i.val)"));
      
      assertEqual(expected, runQuery("(i.val >= j.val && i.val < j.val)"));
      assertEqual(expected, runQuery("(j.val <= i.val && j.val > i.val)"));
      assertEqual(expected, runQuery("(i.val < j.val && i.val >= j.val)"));
      assertEqual(expected, runQuery("(j.val > i.val && j.val <= i.val)"));
      
      assertEqual(expected, runQuery("(i.val > j.val && i.val <= j.val)"));
      assertEqual(expected, runQuery("(j.val < i.val && j.val >= i.val)"));
      assertEqual(expected, runQuery("(i.val <= j.val && i.val > j.val)"));
      assertEqual(expected, runQuery("(j.val >= i.val && j.val < i.val)"));
    }

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlRefAccessAttributeTestSuite);

return jsunity.done();

