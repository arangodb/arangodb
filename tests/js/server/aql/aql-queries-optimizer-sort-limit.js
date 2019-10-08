/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXPLAIN, AQL_EXECUTE */

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

const jsunity = require("jsunity");
const internal = require("internal");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerLimitTestSuite () {
  var collection = null;
  var docCount = 1000;
  var cn = "UnitTestsAhuacatlOptimizerLimit";

  var getSorts = function (query, params) {
    return AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-indexes", "+use-index-for-sort", "+sort-limit" ] } }).plan.nodes.filter(node => node.type === "SortNode");
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, {numberOfShards: 9});

      let docs = [];
      for (var i = 0; i < docCount; ++i) {
        docs.push({ _key: "test" + i, value : i });
      }
      collection.insert(docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with simple data, too short
////////////////////////////////////////////////////////////////////////////////

    testLimitSimple : function () {
      var query = "FOR c IN [1,3,5,2,4] SORT c LIMIT 3 RETURN c";
      var actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual([1, 2, 3], actual);

      var sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 0);
      assertEqual(sorts[0].strategy, "standard");

      query = "FOR c IN [1,3,5,2,4] SORT c DESC LIMIT 3 RETURN c";
      actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual([5, 4, 3], actual);

      sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 0);
      assertEqual(sorts[0].strategy, "standard");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with simple data, filter, too short
////////////////////////////////////////////////////////////////////////////////

    testLimitSimpleFilter : function () {
      var query = "FOR c IN [1,3,5,2,4] SORT c FILTER c >= 3 LIMIT 2 RETURN c";
      var actual = getQueryResults(query);
      assertEqual(2, actual.length);
      assertEqual([3, 4], actual);

      var sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 0);
      assertEqual(sorts[0].strategy, "standard");

      query = "FOR c IN [1,3,5,2,4] SORT c DESC FILTER c >= 3 LIMIT 2 RETURN c";
      actual = getQueryResults(query);
      assertEqual(2, actual.length);
      assertEqual([5, 4], actual);

      sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 0);
      assertEqual(sorts[0].strategy, "standard");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check limit optimization with simple data
    ////////////////////////////////////////////////////////////////////////////////

    testLimitSimpleLong : function () {
      var query = "FOR c IN 1..1000 SORT c LIMIT 3 RETURN c";
      var actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual([1, 2, 3], actual);

      var sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 3);
      assertEqual(sorts[0].strategy, "constrained-heap");

      query = "FOR c IN 1..1000 SORT c LIMIT 100, 3 RETURN c";
      actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual([101, 102, 103], actual);

      sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 103);
      assertEqual(sorts[0].strategy, "constrained-heap");

      query = "FOR c IN 1..1000 SORT c DESC LIMIT 3 RETURN c";
      actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual([1000, 999, 998], actual);

      sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 3);
      assertEqual(sorts[0].strategy, "constrained-heap");
          
      query = "FOR c IN 1..1000 SORT c DESC LIMIT 100, 3 RETURN c";
      actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual([900, 899, 898], actual);

      sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 103);
      assertEqual(sorts[0].strategy, "constrained-heap");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check limit optimization with simple data, filter
    ////////////////////////////////////////////////////////////////////////////////

    testLimitSimpleFilterLong : function () {
      var query = "FOR c IN 1..1000 SORT c FILTER c > 3 LIMIT 3 RETURN c";
      var actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual([4, 5, 6], actual);

      var sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 0);
      assertEqual(sorts[0].strategy, "standard");

      query = "FOR c IN 1..1000 SORT c DESC FILTER c < 900 LIMIT 3 RETURN c";
      actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual([899, 898, 897], actual);

      sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 0);
      assertEqual(sorts[0].strategy, "standard");
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
        assertEqual(test.expectedLength, actual.length, `Test #${i}, query was: ${query}`);

        var sorts = getSorts(query);
        assertEqual(sorts.length, 1);
        assertEqual(sorts[0].limit, test.offset + test.limit);
        assertEqual(sorts[0].strategy, "constrained-heap");
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

      var sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 0);
      assertEqual(sorts[0].strategy, "standard");
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

      var sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 0);
      assertEqual(sorts[0].strategy, "standard");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort3 : function () {
      var query = "FOR c IN " + cn + " SORT c.value LIMIT 0, 10 FILTER c.value >= 20 && c.value < 30 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(0, actual.length);

      var sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 10);
      assertEqual(sorts[0].strategy, "constrained-heap");
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

      var sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 10);
      assertEqual(sorts[0].strategy, "constrained-heap");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort and fullCount
/// This is a regression test, the constrained heap did not play well with
/// fullCount when 3.5 was released.
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSortWithFullCount: function () {
      const query = `FOR c IN ${cn} SORT c.value LIMIT 20, 10 RETURN c`;

      const queryResult = AQL_EXECUTE(query, {}, {fullCount: true});

      const values = queryResult.json;
      const fullCount = queryResult.stats.fullCount;

      assertEqual(10, values.length);

      assertEqual(20, values[0].value);
      assertEqual(21, values[1].value);
      assertEqual(22, values[2].value);
      assertEqual(29, values[9].value);

      assertEqual(fullCount, 1000);

      const sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 30);
      assertEqual(sorts[0].strategy, "constrained-heap");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort and fullCount
/// Here, there are fewer rows to emit than the limit asks for.
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSortWithFullCountAndFewRows: function () {
      const query = `FOR c IN ${cn} FILTER c.value < 30 SORT c.value LIMIT 20, 700 RETURN c`;

      const queryResult = AQL_EXECUTE(query, {}, {fullCount: true});

      const values = queryResult.json;
      const fullCount = queryResult.stats.fullCount;

      assertEqual(10, values.length);

      assertEqual(20, values[0].value);
      assertEqual(21, values[1].value);
      assertEqual(22, values[2].value);
      assertEqual(29, values[9].value);

      assertEqual(fullCount, 30);

      const sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 720);
      assertEqual(sorts[0].strategy, 'constrained-heap');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort and fullCount
/// Here, all rows are skipped during the limit block's offset.
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSortWithFullCountAndAllRowsSkipped: function () {
      const query = `FOR c IN ${cn} FILTER c.value < 30 SORT c.value LIMIT 40, 700 RETURN c`;

      const queryResult = AQL_EXECUTE(query, {}, {fullCount: true});

      const values = queryResult.json;
      const fullCount = queryResult.stats.fullCount;

      assertEqual(0, values.length);

      assertEqual(fullCount, 30);

      const sorts = getSorts(query);
      assertEqual(sorts.length, 1);
      assertEqual(sorts[0].limit, 740);
      assertEqual(sorts[0].strategy, 'constrained-heap');
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief Test two consecutive sorts
/// This is a regression test. Only the latter block may be a constrained sort.
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionTwoSortBlocks : function () {
      const query = `FOR c IN ${cn} SORT c.value ASC LET b = c.value*2 SORT b DESC LIMIT 10 RETURN b`;
      var actual = getQueryResults(query);

      assertEqual(10, actual.length);

      assertEqual(1998, actual[0]);
      assertEqual(1996, actual[1]);
      assertEqual(1994, actual[2]);
      assertEqual(1980, actual[9]);

      var sorts = getSorts(query);
      assertEqual(sorts.length, 2);
      assertEqual(sorts[0].strategy, "standard");
      assertEqual(sorts[1].limit, 10);
      assertEqual(sorts[1].strategy, "constrained-heap");
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimizerLimitTestSuite);

return jsunity.done();
