/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, limit optimizations
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
var internal = require("internal");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerLimitTestSuite () {
  var collection = null;
  var docCount = 100;
  var cn = "UnitTestsAhuacatlOptimizerLimit";

  var explain = function (query, params) {
    return helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-indexes", "+use-index-for-sort" ] } })).map(function(node) { return node.type; });
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      for (var i = 0; i < docCount; ++i) {
        collection.save({ _key: "test" + i, value : i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with simple data
////////////////////////////////////////////////////////////////////////////////

    testLimitSimple : function () {
      var query = "FOR c IN [1,3,5,2,4] SORT c LIMIT 3 RETURN c";
      var actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual([1, 2, 3], actual);

      query = "FOR c IN [1,3,5,2,4] SORT c DESC LIMIT 3 RETURN c";
      actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual([5, 4, 3], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with simple data, filter
////////////////////////////////////////////////////////////////////////////////

    testLimitSimpleFilter : function () {
      var query = "FOR c IN [1,3,5,2,4] SORT c FILTER c >= 3 LIMIT 2 RETURN c";
      var actual = getQueryResults(query);
      assertEqual(2, actual.length);
      assertEqual([3, 4], actual);

      query = "FOR c IN [1,3,5,2,4] SORT c DESC FILTER c >= 3 LIMIT 2 RETURN c";
      actual = getQueryResults(query);
      assertEqual(2, actual.length);
      assertEqual([5, 4], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with 2 limits
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionLimitLimit : function () {
      var tests = [
        { offset: 0, limit: 500, offset2: 0, limit2: 1, expectedLength: 1 },
        { offset: 10, limit: 5, offset2: 0, limit2: 1, expectedLength: 1 },
        { offset: 10, limit: 5, offset2: 0, limit2: 20, expectedLength: 5 },
        { offset: 10, limit: 50, offset2: 1, limit2: 20, expectedLength: 20 },
        { offset: 10, limit: 90, offset2: 10, limit2: 20, expectedLength: 20 },
        { offset: 90, limit: 10, offset2: 9, limit2: 20, expectedLength: 1 },
        { offset: 50, limit: 50, offset2: 0, limit2: 50, expectedLength: 50 },
        { offset: 50, limit: 50, offset2: 10, limit2: 50, expectedLength: 40 },
        { offset: 50, limit: 50, offset2: 50, limit2: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " SORT c.value LIMIT " + test.offset + ", " + test.limit + " LIMIT " + test.offset2 + ", " + test.limit2 + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort1 : function () {
      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "FilterNode", "LimitNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort2 : function () {
      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);

      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(22, actual[2].value);
      assertEqual(29, actual[9].value);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "FilterNode", "LimitNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort3 : function () {
      var query = "FOR c IN " + cn + " SORT c.value LIMIT 0, 10 FILTER c.value >= 20 && c.value < 30 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(0, actual.length);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "LimitNode", "CalculationNode", "FilterNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort4 : function () {
      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 SORT c.value LIMIT 0, 10 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);

      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(22, actual[2].value);
      assertEqual(29, actual[9].value);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "LimitNode", "ReturnNode" ], explain(query));
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimizerLimitTestSuite);

return jsunity.done();
