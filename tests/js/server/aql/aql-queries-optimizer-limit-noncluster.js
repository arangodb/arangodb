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
/// @brief check limit optimization for non-collection access, limit > 0
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionNoRestriction : function () {
      var i;
      var list = [ ];
      for (i = 0; i < 100; ++i) {
        list.push(i);
      }

      var tests = [
        { offset: 0, limit: 500, expectedLength: 100 },
        { offset: 0, limit: 50, expectedLength: 50 },
        { offset: 0, limit: 5, expectedLength: 5 },
        { offset: 0, limit: 1, expectedLength: 1 },
        { offset: 1, limit: 50, expectedLength: 50 },
        { offset: 1, limit: 1, expectedLength: 1 },
        { offset: 10, limit: 50, expectedLength: 50 },
        { offset: 95, limit: 5, expectedLength: 5 },
        { offset: 95, limit: 50, expectedLength: 5 },
        { offset: 98, limit: 50, expectedLength: 2 },
        { offset: 98, limit: 2, expectedLength: 2 },
        { offset: 99, limit: 1, expectedLength: 1 },
        { offset: 99, limit: 2, expectedLength: 1 },
        { offset: 100, limit: 2, expectedLength: 0 }
      ];

      for (i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);

        assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for full collection access, limit > 0
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionNoRestriction : function () {
      var tests = [
        { offset: 0, limit: 500, expectedLength: 100 },
        { offset: 0, limit: 50, expectedLength: 50 },
        { offset: 0, limit: 5, expectedLength: 5 },
        { offset: 0, limit: 1, expectedLength: 1 },
        { offset: 1, limit: 50, expectedLength: 50 },
        { offset: 1, limit: 1, expectedLength: 1 },
        { offset: 10, limit: 50, expectedLength: 50 },
        { offset: 95, limit: 5, expectedLength: 5 },
        { offset: 95, limit: 50, expectedLength: 5 },
        { offset: 98, limit: 50, expectedLength: 2 },
        { offset: 98, limit: 2, expectedLength: 2 },
        { offset: 99, limit: 1, expectedLength: 1 },
        { offset: 99, limit: 2, expectedLength: 1 },
        { offset: 100, limit: 2, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);

        assertEqual([ "SingletonNode", "EnumerateCollectionNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for non collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionNoRestrictionEmpty : function () {
      var i;
      var list = [ ];
      for (i = 0; i < 100; ++i) {
        list.push(i);
      }

      var tests = [
        { offset: 0, limit: 0 },
        { offset: 1, limit: 0 },
        { offset: 10, limit: 0 },
        { offset: 95, limit: 0 },
        { offset: 98, limit: 0 },
        { offset: 99, limit: 0 },
        { offset: 100, limit: 0 }
      ];

      for (i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(0, actual.length);

        assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for full collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionNoRestrictionEmpty : function () {
      var tests = [
        { offset: 0, limit: 0 },
        { offset: 1, limit: 0 },
        { offset: 10, limit: 0 },
        { offset: 95, limit: 0 },
        { offset: 98, limit: 0 },
        { offset: 99, limit: 0 },
        { offset: 100, limit: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(0, actual.length);

        assertEqual([ "SingletonNode", "EnumerateCollectionNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for non-collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionDoubleLimitEmpty : function () {
      var list = [ ];
      for (var i = 0; i < 100; ++i) {
        list.push(i);
      }

      var query = "FOR c IN " + JSON.stringify(list) + " LIMIT 10 LIMIT 0 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(0, actual.length);

      assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "LimitNode", "LimitNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for full collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionDoubleLimitEmpty : function () {
      var query = "FOR c IN " + cn + " LIMIT 10 LIMIT 0 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(0, actual.length);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "LimitNode", "LimitNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with 2 limits
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionLimitLimit : function () {
      var i;
      var list = [ ];
      for (i = 0; i < 100; ++i) {
        list.push(i);
      }

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

      for (i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " LIMIT " + test.offset + ", " + test.limit + " LIMIT " + test.offset2 + ", " + test.limit2 + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);

        assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "LimitNode", "LimitNode", "ReturnNode" ], explain(query));
      }
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
/// @brief check limit optimization for non-collection access, limit > 0 and
/// filter conditions
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionFilter : function () {
      var i;
      var list = [ ];
      for (i = 0; i < 100; ++i) {
        list.push(i);
      }

      var tests = [
        { offset: 0, limit: 500, value: 0, expectedLength: 100 },
        { offset: 0, limit: 50, value: 0, expectedLength: 50 },
        { offset: 0, limit: 5, value: 0, expectedLength: 5 },
        { offset: 0, limit: 1, value: 0, expectedLength: 1 },
        { offset: 1, limit: 50, value: 0, expectedLength: 50 },
        { offset: 1, limit: 1, value: 0, expectedLength: 1 },
        { offset: 10, limit: 50, value: 0, expectedLength: 50 },
        { offset: 95, limit: 5, value: 0, expectedLength: 5 },
        { offset: 95, limit: 50, value: 0, expectedLength: 5 },
        { offset: 98, limit: 50, value: 0, expectedLength: 2 },
        { offset: 98, limit: 2, value: 0, expectedLength: 2 },
        { offset: 99, limit: 1, value: 0, expectedLength: 1 },
        { offset: 99, limit: 2, value: 0, expectedLength: 1 },
        { offset: 100, limit: 2, value: 0, expectedLength: 0 },
        { offset: 0, limit: 500, value: 10, expectedLength: 90 },
        { offset: 0, limit: 50, value: 10, expectedLength: 50 },
        { offset: 0, limit: 5, value: 10, expectedLength: 5 },
        { offset: 0, limit: 1, value: 10, expectedLength: 1 },
        { offset: 50, limit: 1, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 0, expectedLength: 1 },
        { offset: 89, limit: 1, value: 10, expectedLength: 1 },
        { offset: 89, limit: 2, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 10, expectedLength: 0 },
        { offset: 50, limit: 5, value: 40, expectedLength: 5 },
        { offset: 50, limit: 5, value: 50, expectedLength: 0 }
      ];

      for (i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " FILTER c >= " + test.value + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);
      
        assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for full collection access, limit > 0 and
/// filter conditions
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionFilter : function () {
      var tests = [
        { offset: 0, limit: 500, value: 0, expectedLength: 100 },
        { offset: 0, limit: 50, value: 0, expectedLength: 50 },
        { offset: 0, limit: 5, value: 0, expectedLength: 5 },
        { offset: 0, limit: 1, value: 0, expectedLength: 1 },
        { offset: 1, limit: 50, value: 0, expectedLength: 50 },
        { offset: 1, limit: 1, value: 0, expectedLength: 1 },
        { offset: 10, limit: 50, value: 0, expectedLength: 50 },
        { offset: 95, limit: 5, value: 0, expectedLength: 5 },
        { offset: 95, limit: 50, value: 0, expectedLength: 5 },
        { offset: 98, limit: 50, value: 0, expectedLength: 2 },
        { offset: 98, limit: 2, value: 0, expectedLength: 2 },
        { offset: 99, limit: 1, value: 0, expectedLength: 1 },
        { offset: 99, limit: 2, value: 0, expectedLength: 1 },
        { offset: 100, limit: 2, value: 0, expectedLength: 0 },
        { offset: 0, limit: 500, value: 10, expectedLength: 90 },
        { offset: 0, limit: 50, value: 10, expectedLength: 50 },
        { offset: 0, limit: 5, value: 10, expectedLength: 5 },
        { offset: 0, limit: 1, value: 10, expectedLength: 1 },
        { offset: 50, limit: 1, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 0, expectedLength: 1 },
        { offset: 89, limit: 1, value: 10, expectedLength: 1 },
        { offset: 89, limit: 2, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 10, expectedLength: 0 },
        { offset: 50, limit: 5, value: 40, expectedLength: 5 },
        { offset: 50, limit: 5, value: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " FILTER c.value >= " + test.value + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);
      
        assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "FilterNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for full collection access, limit > 0 and
/// filter conditions
////////////////////////////////////////////////////////////////////////////////

    testLimitFilterFilterCollectionFilter : function () {
      var tests = [
        { offset: 0, limit: 500, value: 0, expectedLength: 100 },
        { offset: 0, limit: 50, value: 0, expectedLength: 50 },
        { offset: 0, limit: 5, value: 0, expectedLength: 5 },
        { offset: 0, limit: 1, value: 0, expectedLength: 1 },
        { offset: 1, limit: 50, value: 0, expectedLength: 50 },
        { offset: 1, limit: 1, value: 0, expectedLength: 1 },
        { offset: 10, limit: 50, value: 0, expectedLength: 50 },
        { offset: 95, limit: 5, value: 0, expectedLength: 5 },
        { offset: 95, limit: 50, value: 0, expectedLength: 5 },
        { offset: 98, limit: 50, value: 0, expectedLength: 2 },
        { offset: 98, limit: 2, value: 0, expectedLength: 2 },
        { offset: 99, limit: 1, value: 0, expectedLength: 1 },
        { offset: 99, limit: 2, value: 0, expectedLength: 1 },
        { offset: 100, limit: 2, value: 0, expectedLength: 0 },
        { offset: 0, limit: 500, value: 10, expectedLength: 90 },
        { offset: 0, limit: 50, value: 10, expectedLength: 50 },
        { offset: 0, limit: 5, value: 10, expectedLength: 5 },
        { offset: 0, limit: 1, value: 10, expectedLength: 1 },
        { offset: 50, limit: 1, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 0, expectedLength: 1 },
        { offset: 89, limit: 1, value: 10, expectedLength: 1 },
        { offset: 89, limit: 2, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 10, expectedLength: 0 },
        { offset: 50, limit: 5, value: 40, expectedLength: 5 },
        { offset: 50, limit: 5, value: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " FILTER c.value >= " + test.value + " FILTER c.value <= 9999 LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);
      
        assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "FilterNode", "CalculationNode", "FilterNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionHashIndex1 : function () {
      collection.ensureHashIndex("value");

      var query = "FOR c IN " + cn + " FILTER c.value == 23 || c.value == 24 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(2, actual.length);
      assertEqual(23, actual[0].value);
      assertEqual(24, actual[1].value);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "LimitNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionHashIndex2 : function () {
      collection.ensureHashIndex("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "LimitNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionHashIndex3 : function () {
      collection.ensureHashIndex("value");

      var query = "FOR c IN " + cn + " SORT c.value DESC LIMIT 10, 10 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      for (var i = 0; i < 10; ++i) {
        assertEqual(docCount - 11 - i, actual[i].value);
      }

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "LimitNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFilterFilterCollectionHashIndex : function () {
      collection.ensureHashIndex("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 FILTER c.value <= 9999 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "FilterNode",
                    "LimitNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSkiplistIndex1 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value == 23 || c.value == 24 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(2, actual.length);
      assertEqual(23, actual[0].value);
      assertEqual(24, actual[1].value);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "LimitNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSkiplistIndex2 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "LimitNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSkiplist3 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " SORT c.value DESC LIMIT 10, 10 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      for (var i = 0; i < 10; ++i) {
        assertEqual(docCount - 11 - i, actual[i].value);
      }

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "LimitNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index large skip
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSkiplist4 : function () {
      collection.ensureSkiplist("value");

      for (var i = docCount; i < 1030; ++i) {
        internal.db[cn].save({value: i});
      }
      var query = "FOR c IN " + cn + " SORT c.value ASC LIMIT 1005, 10 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      for (var j = 0; j < 10; ++j) {
        assertEqual(1005 + j, actual[j].value);
      }

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "LimitNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFilterFilterSkiplistIndex : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 FILTER c.value <= 9999 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "FilterNode", "LimitNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
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

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested1 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] FOR i IN [ 5, 6, 7 ] LIMIT 2 RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 5, o: 1 }, { i: 6, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested2 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] FOR i IN [ 5, 6, 7 ] LIMIT 0, 1 RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 5, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested3 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] FOR i IN [ 5, 6, 7 ] SORT o, i LIMIT 1, 1 RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 6, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested4 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] LIMIT 1 FOR i IN [ 5, 6, 7 ] RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 5, o: 1 }, { i: 6, o: 1 }, { i: 7, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested5 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] LIMIT 1, 1 FOR i IN [ 5, 6, 7 ] RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 5, o: 2 }, { i: 6, o: 2 }, { i: 7, o: 2 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested6 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] LIMIT 1 FOR i IN [ 5, 6, 7 ] LIMIT 2 RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 5, o: 1 }, { i: 6, o: 1 } ], actual);
    },

    testLimitForIndexLookups: function () {
      var queries = [
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 0 RETURN doc2.value", []], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 1 RETURN doc2.value", [1]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 2 RETURN doc2.value", [1,2]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 3 RETURN doc2.value", [1,2,3]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 4 RETURN doc2.value", [1,2,3]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 1,0 RETURN doc2.value", []], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 1,1 RETURN doc2.value", [2]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 1,2 RETURN doc2.value", [2,3]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 1,3 RETURN doc2.value", [2,3]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 1,4 RETURN doc2.value", [2,3]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 2,0 RETURN doc2.value", []], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 2,1 RETURN doc2.value", [3]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 2,2 RETURN doc2.value", [3]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 2,3 RETURN doc2.value", [3]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 2,4 RETURN doc2.value", [3]], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 3,0 RETURN doc2.value", []], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 3,1 RETURN doc2.value", []], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 3,2 RETURN doc2.value", []], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 3,3 RETURN doc2.value", []], 
        ["FOR doc1 IN ['test1', 'test2', 'test3'] FOR doc2 IN " + collection.name() + " FILTER doc2._key == doc1 LIMIT 3,4 RETURN doc2.value", []] 
      ];

      queries.forEach(function(query) {
        var actual = getQueryResults(query[0]);
        assertEqual(query[1], actual, query);
      });
    },

    testLimitNestedLoops: function() {
      let expected = [[1, 1], [1, 2], [1, 3], [1, 4], [2, 1], [2, 2], [2, 3], [2, 4]];
      let query = "FOR i IN 1..2 FOR j IN 1..4 LIMIT @limit, 2 RETURN [i , j]";
      for (let i = 0; i <= 8; ++i) {
        let actual = getQueryResults(query, { limit: i });
        assertEqual(expected.slice(i, i + 2), actual, i);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimizerLimitTestSuite);

return jsunity.done();

