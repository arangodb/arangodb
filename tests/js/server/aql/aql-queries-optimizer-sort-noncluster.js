/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, sort optimizations
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerSortTestSuite () {
  var collection = null;
  var cn = "UnitTestsAhuacatlOptimizerSort";
  
  var explain = function (query, params) {
    return helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-index-for-sort", "+use-indexes", "+remove-redundant-sorts" ] } })).map(function(node) { return node.type; });
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      for (var i = 0; i < 100; ++i) {
        collection.save({ "value" : i, "value2" : i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization without index
////////////////////////////////////////////////////////////////////////////////

    testNoIndex1 : function () {
      var query = "FOR c IN " + cn + " SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization without index
////////////////////////////////////////////////////////////////////////////////

    testNoIndex2 : function () {
      var query = "FOR c IN " + cn + " SORT c.value DESC RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(99, actual[0].value);
      assertEqual(98, actual[1].value);
      assertEqual(97, actual[2].value);
      assertEqual(96, actual[3].value);
      assertEqual(49, actual[50].value);
      assertEqual(1, actual[98].value);
      assertEqual(0, actual[99].value);
      
      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testSkiplist1 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testSkiplist2 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 15 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(85, actual.length);
      assertEqual(15, actual[0].value);
      assertEqual(16, actual[1].value);
      assertEqual(17, actual[2].value);
      assertEqual(18, actual[3].value);
      assertEqual(98, actual[83].value);
      assertEqual(99, actual[84].value);
     
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testSkiplist3 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 15 SORT c.value DESC RETURN c";

      var actual = getQueryResults(query);
      assertEqual(85, actual.length);
      assertEqual(99, actual[0].value);
      assertEqual(98, actual[1].value);
      assertEqual(97, actual[2].value);
      assertEqual(96, actual[3].value);
      assertEqual(16, actual[83].value);
      assertEqual(15, actual[84].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testMultipleSorts1 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testMultipleSorts2 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value SORT c.value DESC RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(99, actual[0].value);
      assertEqual(98, actual[1].value);
      assertEqual(97, actual[2].value);
      assertEqual(96, actual[3].value);
      assertEqual(49, actual[50].value);
      assertEqual(1, actual[98].value);
      assertEqual(0, actual[99].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testMultipleSorts3 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value DESC SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields1 : function () {
      collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
     
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query));
      
      query = "FOR c IN " + cn + " FILTER c.value == 0 && c.value2 >= 0 SORT c.value RETURN c";

      actual = getQueryResults(query);
      assertEqual(1, actual.length);
      assertEqual(0, actual[0].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields2 : function () {
      collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value, c.value2 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
     
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "ReturnNode" ], explain(query));
      
      query = "FOR c IN " + cn + " FILTER c.value == 0 && c.value2 <= 1 SORT c.value, c.value2 RETURN c";

      actual = getQueryResults(query);
      assertEqual(1, actual.length);
      assertEqual(0, actual[0].value);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields3 : function () {
      collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value2 >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields4 : function () {
      collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 && c.value2 >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
     
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields5 : function () {
      collection.ensureSkiplist("value2", "value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testNonFieldSort1 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value + 1 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testNonFieldSort2 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT 1 + c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testNonFieldSort3 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value * 2 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery sorting
////////////////////////////////////////////////////////////////////////////////

    testSortOnSubquery : function () {
      var query = "FOR x IN [1,2] LET values = (FOR i IN [x] FILTER i != 2 RETURN i) SORT values RETURN values";

      var actual = getQueryResults(query);
      assertEqual(2, actual.length);
      assertEqual([ ], actual[0]);
      assertEqual([1], actual[1]);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimizerSortTestSuite);

return jsunity.done();

