/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for index usage
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
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

// returns some values of array around index (from index-before to index+after)
// as a comma separated string. Appends/prepends "..." when not printing the
// whole array.
function arrayStringExcerpt(array, index, before, after) {
  const lastIdx = array.length - 1;
  const from = Math.max(index-before, 0);
  const to = Math.min(index+after, lastIdx);
  const slice = array.slice(from, to).join(', ');
  const prefix = from > 0
    ? '..., '
    : '';
  const suffix = to < lastIdx
    ? ', ...'
    : '';
  return prefix + slice + suffix;
}

function sortTestSuite () {
  var c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 8 });

      let docs = [];
      for (var i = 0; i < 11111; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }
      c.insert(docs);
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test without index
////////////////////////////////////////////////////////////////////////////////

    testSortNoIndex: function() {
      const result =
          AQL_EXECUTE(`FOR doc IN ${c.name()} SORT doc.value RETURN doc.value`)
              .json;
      assertEqual(11111, result.length);

      let last = -1;
      for (let i = 0; i < result.length; ++i) {
        let slice = arrayStringExcerpt(result, i, 2, 1);
        assertTrue(result[i] > last, `failure at index ${i}: ${slice}`);
        last = result[i];
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with index
////////////////////////////////////////////////////////////////////////////////

    testSortSkiplist: function() {
      c.ensureIndex({type: "skiplist", fields: ["value"]});
      const result =
          AQL_EXECUTE(`FOR doc IN ${c.name()} SORT doc.value RETURN doc.value`)
              .json;
      assertEqual(11111, result.length);

      let last = -1;
      for (let i = 0; i < result.length; ++i) {
        let slice = arrayStringExcerpt(result, i, 2, 1);
        assertTrue(result[i] > last, `failure at index ${i}: ${slice}`);
        last = result[i];
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test SORT in a spliced subquery. Regression test for
/// https://github.com/arangodb/arangodb/issues/12693,
/// respectively https://github.com/arangodb/arangodb/pull/12752.
/// SORT only sorted part of its input when the input rows crossed a block
/// boundary in the subquery. E.g. an AqlItemBlock containing
/// 1) 500 data rows
/// 2) a shadow row
/// 3) 499 data rows
/// , where the next block usually should contain one data row, followed by the
/// second shadow row.
/// Now in this case SORT would sort the 499 rows, and ending the subquery
/// iteration, thus accidentally dropping the last data row of the second
/// iteration.
////////////////////////////////////////////////////////////////////////////////
    testSortInSubqueryIssue12693: function () {
      const query = `
        FOR iter IN 1..2
          LET sq = (
            FOR i IN 1..500
            SORT i
            RETURN i
          )
          RETURN COUNT(sq)
      `;
      const result = AQL_EXECUTE(query);
      assertEqual([
          'SingletonNode',
          'CalculationNode',
          'CalculationNode',
          'CalculationNode',
          'EnumerateListNode',
          'SubqueryStartNode',
          'EnumerateListNode',
          'SortNode',
          'SubqueryEndNode',
          'CalculationNode',
          'ReturnNode',
        ],
        AQL_EXPLAIN(query).plan.nodes.map(node => node.type)
      );
      assertEqual([500, 500], result.json);
    },

  };
}

jsunity.run(sortTestSuite);

return jsunity.done();

