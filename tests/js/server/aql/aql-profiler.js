/*jshint globalstrict:true, strict:true, esnext: true */
/*global AQL_EXPLAIN, assertTrue */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////


// contains common code for aql-profiler* tests
const profHelper = require("@arangodb/aql-profiler-test-helper");

const db = require('@arangodb').db;
const jsunity = require("jsunity");
const assert = jsunity.jsUnity.assertions;
const _ = require('lodash');


////////////////////////////////////////////////////////////////////////////////
/// @file test suite for AQL tracing/profiling
/// Tests primarily that every Aql block returns the expected number of rows
/// during queries, and that getSome() is called the expected number of times
/// (mainly that it is not called too often).
/// Some tests are located in aql-profiler-noncluster.js and
/// aql-profiler-noncluster-nightly.js, namely for the following blocks:
/// - EnumerateCollectionBlock
/// - IndexBlock
/// - TraversalBlock
////////////////////////////////////////////////////////////////////////////////

// TODO Test skipSome() as well.

// TODO EnumerateCollectionBlock *and* IndexBlock are suboptimal because both
// abort after iterating over the collection once and return the items fetched
// so far instead of filling up the result. (see aql-profiler-noncluster*.js)

// TODO IndexBlock is still suboptimal, as it can return HASMORE when there are no
// items left. (low impact) (see aql-profiler-noncluster*.js)


function ahuacatlProfilerTestSuite () {


  // import some names from profHelper directly into our namespace:
  const defaultBatchSize = profHelper.defaultBatchSize;
  const defaultTestRowCounts = profHelper.defaultTestRowCounts;

  const { CalculationNode, CollectNode, DistributeNode, EnumerateCollectionNode,
    EnumerateListNode, EnumerateViewNode, FilterNode, GatherNode, IndexNode,
    InsertNode, LimitNode, NoResultsNode, RemoteNode, RemoveNode, ReplaceNode,
    ReturnNode, ScatterNode, ShortestPathNode, SingletonNode, SortNode,
    SubqueryNode, TraversalNode, UpdateNode, UpsertNode } = profHelper;

  const { CalculationBlock, ConstrainedSortBlock, CountCollectBlock, DistinctCollectBlock,
    EnumerateCollectionBlock, EnumerateListBlock, FilterBlock,
    HashedCollectBlock, IndexBlock, LimitBlock, NoResultsBlock, RemoteBlock,
    ReturnBlock, ShortestPathBlock, SingletonBlock, SortBlock,
    SortedCollectBlock, SortingGatherBlock, SubqueryBlock, TraversalBlock,
    UnsortingGatherBlock, RemoveBlock, InsertBlock, UpdateBlock, ReplaceBlock,
    UpsertBlock, ScatterBlock, DistributeBlock, IResearchViewUnorderedBlock,
    IResearchViewBlock, IResearchViewOrderedBlock } = profHelper;

  // See the limit tests (e.g. testLimitBlock3) for limit() and offset().
  const additionalLimitTestRowCounts = [
    // limit() = 1000 ± 1:
    1332, 1333, 1334,
    // offset() = 1000 ± 1:
    3999, 4000, 4003, 4004,
    // limit() = 2000 ± 1:
    2665, 2666, 2667,
    // offset() = 2000 ± 1:
    7999, 8000, 8003, 8004,
  ];

  const offset = rows => Math.floor(rows/4);
  const limit = rows => Math.ceil(3*rows/4);
  const offsetBatches = rows => Math.ceil(offset(rows) / defaultBatchSize);
  const skipOffsetBatches = rows => Math.ceil(offset(rows) === 0 ? 0 : 1);
  const limitBatches = rows => Math.ceil(limit(rows) / defaultBatchSize);

  {
    // This is more documentation than anything else:
    assert.assertEqual(999, limit(1332));
    assert.assertEqual(1000, limit(1333));
    assert.assertEqual(1001, limit(1334));
    assert.assertEqual(999, offset(3999));
    assert.assertEqual(1000, offset(4000));
    assert.assertEqual(1000, offset(4003));
    assert.assertEqual(1001, offset(4004));
    assert.assertEqual(1999, limit(2665));
    assert.assertEqual(2000, limit(2666));
    assert.assertEqual(2001, limit(2667));
    assert.assertEqual(1999, offset(7999));
    assert.assertEqual(2000, offset(8000));
    assert.assertEqual(2000, offset(8003));
    assert.assertEqual(2001, offset(8004));
  }

  // This is the decision made by the sort-limit optimizer rule:
  const usesHeapSort = rows => {
    const n = rows;
    const m = limit(rows);
    return rows >= 100 && 0.25 * n * Math.log2(m) + m * Math.log2(m) < n * Math.log2(n);
  };
  // // Filter out row counts that would use the standard sort strategy
  // const sortLimitTestRowCounts = _.uniq(defaultTestRowCounts.concat(additionalLimitTestRowCounts).sort())
  //   .filter(usesHeapSort);
  const sortLimitTestRowCounts =
    // defaults, minus those < 100:
    [100, 999, 1000, 1001, 1500, 2000, 10500]
      .concat([
        // limit() - offset() = 1000 ± 1:
      1995, 1997, 1998, 2000, 1999, 2001,
        // limit() - offset() = 2000 ± 1:
      3995, 3997, 3998, 4000, 3999, 4001
    ]);
  const limitMinusSkip = rows => limit(rows) - offset(rows);
  const limitMinusSkipBatches = rows => Math.ceil(limitMinusSkip(rows) / defaultBatchSize);
  for (const rows of sortLimitTestRowCounts) {
    assert.assertTrue(usesHeapSort(rows),
      `Test row count would not trigger sort-limit rule: ${rows}`);
  }
  {
    // Documentation of the expected proportions. These are a little wonky due to the rounding,
    // but that's fine for the purpose.
    assert.assertEqual(999, limitMinusSkip(1995));
    assert.assertEqual(999, limitMinusSkip(1997));
    assert.assertEqual(1000, limitMinusSkip(1998));
    assert.assertEqual(1000, limitMinusSkip(2000));
    assert.assertEqual(1001, limitMinusSkip(1999));
    assert.assertEqual(1001, limitMinusSkip(2001));
    assert.assertEqual(1999, limitMinusSkip(3995));
    assert.assertEqual(1999, limitMinusSkip(3997));
    assert.assertEqual(2000, limitMinusSkip(3998));
    assert.assertEqual(2000, limitMinusSkip(4000));
    assert.assertEqual(2001, limitMinusSkip(3999));
    assert.assertEqual(2001, limitMinusSkip(4001));
  }


  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test {profile: 0}
////////////////////////////////////////////////////////////////////////////////

    testProfile0Fields : function () {
      const query = 'RETURN 1';
      const profileDefault = db._query(query, {}).getExtra();
      const profile0 = db._query(query, {}, {profile: 0}).getExtra();
      const profileFalse = db._query(query, {}, {profile: false}).getExtra();
      const profile0WithFullCount = db._query(query, {}, {profile: 0, fullCount: true}).getExtra();

      profHelper.assertIsLevel0Profile(profileDefault);
      profHelper.assertIsLevel0Profile(profile0);
      profHelper.assertIsLevel0Profile(profileFalse);
      profHelper.assertIsLevel0Profile(profile0WithFullCount, {fullCount: true});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test {profile: 1}
////////////////////////////////////////////////////////////////////////////////

    testProfile1Fields : function () {
      const query = 'RETURN 1';
      const profile1 = db._query(query, {}, {profile: 1}).getExtra();
      const profileTrue = db._query(query, {}, {profile: true}).getExtra();
      const profile1WithFullCount = db._query(query, {}, {profile: 1, fullCount: true}).getExtra();

      profHelper.assertIsLevel1Profile(profile1);
      profHelper.assertIsLevel1Profile(profileTrue);
      profHelper.assertIsLevel1Profile(profile1WithFullCount, {fullCount: true});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test {profile: 2}
////////////////////////////////////////////////////////////////////////////////

    testProfile2Fields : function () {
      const query = 'RETURN 1';
      const profile2 = db._query(query, {}, {profile: 2}).getExtra();
      const profile2WithFullCount = db._query(query, {}, {profile: 2, fullCount: true}).getExtra();

      profHelper.assertIsLevel2Profile(profile2);
      profHelper.assertStatsNodesMatchPlanNodes(profile2);
      profHelper.assertIsLevel2Profile(profile2WithFullCount, {fullCount: true});
      profHelper.assertStatsNodesMatchPlanNodes(profile2WithFullCount);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief minimal stats test
////////////////////////////////////////////////////////////////////////////////

    testStatsMinimal : function () {
      const query = 'RETURN 1';
      const profile = db._query(query, {}, {profile: 2}).getExtra();

      profHelper.assertIsLevel2Profile(profile);
      profHelper.assertStatsNodesMatchPlanNodes(profile);

      assert.assertEqual(
        [
          { type : SingletonBlock, calls : 1, items : 1 },
          { type : CalculationBlock, calls : 1, items : 1 },
          { type : ReturnBlock, calls : 1, items : 1 }
        ],
        profHelper.getCompactStatsNodes(profile)
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test EnumerateListBlock and ReturnBlock and SingletonBlock
////////////////////////////////////////////////////////////////////////////////

    testEnumerateListAndReturnAndSingletonBlock : function () {
      const query = 'FOR i IN 1..@rows RETURN i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : ReturnBlock, calls : batches, items : rows }
      ];
      profHelper.runDefaultChecks({query, genNodeList});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test CalculationBlock
////////////////////////////////////////////////////////////////////////////////

    testCalculationBlock : function () {
      const query = 'FOR i IN 1..@rows RETURN i*i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : CalculationBlock, calls : batches, items : rows },
        { type : ReturnBlock, calls : batches, items : rows }
      ];
      profHelper.runDefaultChecks({query, genNodeList});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test CountCollectBlock
////////////////////////////////////////////////////////////////////////////////

    testCountCollectBlock : function () {
      const query = 'FOR i IN 1..@rows COLLECT WITH COUNT INTO c RETURN c';
      const genNodeList = (rows) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : 1, items : rows },
        { type : CountCollectBlock, calls : 1, items : 1 },
        { type : ReturnBlock, calls : 1, items : 1 }
      ];
      profHelper.runDefaultChecks({query, genNodeList});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test DistinctCollectBlock
////////////////////////////////////////////////////////////////////////////////

    testDistinctCollectBlock1 : function () {
      const query = 'FOR i IN 1..@rows RETURN DISTINCT i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : DistinctCollectBlock, calls : batches, items : rows },
        { type : ReturnBlock, calls : batches, items : rows }
      ];
      profHelper.runDefaultChecks({query, genNodeList});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test DistinctCollectBlock
////////////////////////////////////////////////////////////////////////////////

    testDistinctCollectBlock2 : function () {
      const query = 'FOR i IN 1..@rows RETURN DISTINCT i%7';
      const genNodeList = (rows, batches) => {
        const resultRows = Math.min(rows, 7);
        const resultBatches = Math.ceil(resultRows / 1000);
        return [
          {type: SingletonBlock, calls: 1, items: 1},
          {type: CalculationBlock, calls: 1, items: 1},
          {type: EnumerateListBlock, calls: batches, items: rows},
          {type: CalculationBlock, calls: batches, items: rows},
          {type: DistinctCollectBlock, calls: resultBatches, items: resultRows},
          {type: ReturnBlock, calls: resultBatches, items: resultRows}
        ];
      };
      profHelper.runDefaultChecks({query, genNodeList});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test FilterBlock
    ////////////////////////////////////////////////////////////////////////////////

    testFilterBlock1: function() {
      const query = 'FOR i IN 1..@rows FILTER true RETURN i';
      const options = {
        optimizer: {
          rules: [
            "-remove-unnecessary-filters",
            "-remove-unnecessary-filters-2",
            "-move-filters-up",
            "-move-filters-up-2",
          ]
        }
      };
      const genNodeList = (rows, batches) => [
        {type: SingletonBlock, calls: 1, items: 1},
        {type: CalculationBlock, calls: 1, items: 1},
        {type: EnumerateListBlock, calls: batches, items: rows},
        {type: ReturnBlock, calls: batches, items: rows},
      ];
      profHelper.runDefaultChecks({query, genNodeList, options});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test FilterBlock
    ////////////////////////////////////////////////////////////////////////////////

    testFilterBlock2 : function () {
      const query = 'FOR i IN 1..@rows FILTER i % 13 != 0 RETURN i';
      const genNodeList = (rows, batches) => {
        // This is an array 1..rows where true means it passes the filter
        const list = Array.from(Array(rows)).map((_, index ) => {
          return ((index + 1) % 13 !== 0);
        });
        const rowsAfterFilter = rows - Math.floor(rows / 13);
        const batchesAfterFilter = Math.ceil(rowsAfterFilter / defaultBatchSize);

        return [
          { type : SingletonBlock, calls : 1, items : 1 },
          { type : CalculationBlock, calls : 1, items : 1 },
          { type : EnumerateListBlock, calls : batches, items : rows },
          { type : CalculationBlock, calls : batches, items : rows },
          { type : FilterBlock, calls : batchesAfterFilter, items : rowsAfterFilter },
          { type : ReturnBlock, calls : batchesAfterFilter, items : rowsAfterFilter },
        ];
      };
      profHelper.runDefaultChecks({query, genNodeList});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test FilterBlock
    ////////////////////////////////////////////////////////////////////////////////

    testFilterBlock3 : function () {
      const query = 'FOR i IN 1..@rows FILTER i % 13 == 0 RETURN i';
      const genNodeList = (rows, batches) => {
        // This is an array 1..rows where true means it passes the filter
        const list = Array.from(Array(rows)).map((_, index ) => {
          return ((index + 1) % 13 === 0);
        });
        const rowsAfterFilter = Math.floor(rows / 13);
        const batchesAfterFilter = Math.max(1, Math.ceil(rowsAfterFilter / defaultBatchSize));

        return [
          { type : SingletonBlock, calls : 1, items : 1 },
          { type : CalculationBlock, calls : 1, items : 1 },
          { type : EnumerateListBlock, calls : batches, items : rows },
          { type : CalculationBlock, calls : batches, items : rows },
          { type : FilterBlock, calls : batchesAfterFilter, items : rowsAfterFilter },
          { type : ReturnBlock, calls : batchesAfterFilter, items : rowsAfterFilter },
        ];
      };
      profHelper.runDefaultChecks({query, genNodeList});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test HashedCollectBlock
    ////////////////////////////////////////////////////////////////////////////////

    testHashedCollectBlock1 : function () {
      const query = 'FOR i IN 1..@rows COLLECT x = i RETURN x';
      const genNodeList = (rows, batches) => {

        return [
          { type: SingletonBlock, calls: 1, items: 1 },
          { type: CalculationBlock, calls: 1, items: 1 },
          { type: EnumerateListBlock, calls: batches, items: rows },
          { type: HashedCollectBlock, calls: batches, items: rows },
          { type: SortBlock, calls: batches, items: rows },
          { type: ReturnBlock, calls: batches, items: rows },
        ];
      };
      profHelper.runDefaultChecks({query, genNodeList});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test HashedCollectBlock
    ////////////////////////////////////////////////////////////////////////////////

    testHashedCollectBlock2 : function () {
      // x is [1,1,1,2,2,2,3,3,3,4,...
      const query = 'FOR i IN 1..@rows COLLECT x = FLOOR((i-1) / 3)+1 RETURN x';
      const genNodeList = (rows, batches) => {
        const rowsAfterCollect = Math.ceil(rows / 3);
        const batchesAfterCollect = Math.ceil(rowsAfterCollect / defaultBatchSize);

        return [
          { type: SingletonBlock, calls: 1, items: 1 },
          { type: CalculationBlock, calls: 1, items: 1 },
          { type: EnumerateListBlock, calls: batches, items: rows },
          { type: CalculationBlock, calls: batches, items: rows },
          { type: HashedCollectBlock, calls: batchesAfterCollect, items: rowsAfterCollect },
          { type: SortBlock, calls: batchesAfterCollect, items: rowsAfterCollect },
          { type: ReturnBlock, calls: batchesAfterCollect, items: rowsAfterCollect },
        ];
      };
      profHelper.runDefaultChecks({query, genNodeList});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test HashedCollectBlock
    ////////////////////////////////////////////////////////////////////////////////

    testHashedCollectBlock3 : function () {
      // example:
      // for @rows = 5,  x is [1,2,0,1,2]
      // for @rows = 12, x is [1,2,3,4,5,0,1,2,3,4,5,0]
      const query = 'FOR i IN 1..@rows COLLECT x = i % CEIL(@rows / 2) RETURN x';
      const genNodeList = (rows, batches) => {
        const rowsAfterCollect = Math.ceil(rows / 2);
        const batchesAfterCollect = Math.ceil(rowsAfterCollect / defaultBatchSize);

        return [
          { type: SingletonBlock, calls: 1, items: 1 },
          { type: CalculationBlock, calls: 1, items: 1 },
          { type: EnumerateListBlock, calls: batches, items: rows },
          { type: CalculationBlock, calls: batches, items: rows },
          { type: HashedCollectBlock, calls: batchesAfterCollect, items: rowsAfterCollect },
          { type: SortBlock, calls: batchesAfterCollect, items: rowsAfterCollect },
          { type: ReturnBlock, calls: batchesAfterCollect, items: rowsAfterCollect },
        ];
      };
      profHelper.runDefaultChecks({query, genNodeList});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test LimitBlock
    ////////////////////////////////////////////////////////////////////////////////

    testLimitBlock1: function() {
      const query = 'FOR i IN 1..@rows LIMIT 0, @rows RETURN i';
      const genNodeList = (rows, batches) => [
        {type: SingletonBlock, calls: 1, items: 1},
        {type: CalculationBlock, calls: 1, items: 1},
        {type: EnumerateListBlock, calls: batches, items: rows},
        {type: LimitBlock, calls: batches, items: rows},
        {type: ReturnBlock, calls: batches, items: rows},
      ];
      profHelper.runDefaultChecks({query, genNodeList});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test LimitBlock
    ////////////////////////////////////////////////////////////////////////////////

    testLimitBlock2: function() {
      const query = 'FOR i IN 1..@rows LIMIT @limit RETURN i';
      const limit = rows => Math.ceil(3*rows/4);
      const limitBatches = rows => Math.max(1, Math.ceil(limit(rows) / defaultBatchSize));

      const genNodeList = (rows) => [
        {type: SingletonBlock, calls: 1, items: 1},
        {type: CalculationBlock, calls: 1, items: 1},
        {type: EnumerateListBlock, calls: limitBatches(rows), items: limit(rows)},
        {type: LimitBlock, calls: limitBatches(rows), items: limit(rows)},
        {type: ReturnBlock, calls: limitBatches(rows), items: limit(rows)},
      ];
      const bind = (rows) => ({
        rows,
        limit: limit(rows)
      });
      const additionalTestRowCounts = additionalLimitTestRowCounts;
      profHelper.runDefaultChecks({query, genNodeList, bind, additionalTestRowCounts});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test LimitBlock
    ////////////////////////////////////////////////////////////////////////////////

    testLimitBlock3: function() {
      const query = 'FOR i IN 1..@rows LIMIT @offset, @limit RETURN i';

      const genNodeList = (rows) => [
        {type: SingletonBlock, calls: 1, items: 1},
        {type: CalculationBlock, calls: 1, items: 1},
        {type: EnumerateListBlock, calls: limitBatches(rows), items: limit(rows) + offset(rows)},
        {type: LimitBlock, calls: limitBatches(rows), items: limit(rows)},
        {type: ReturnBlock, calls: limitBatches(rows), items: limit(rows)},
      ];
      const bind = (rows) => ({
        rows,
        offset: offset(rows),
        limit: limit(rows),
      });
      const additionalTestRowCounts = additionalLimitTestRowCounts;
      profHelper.runDefaultChecks({query, genNodeList, bind, additionalTestRowCounts});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test NoResultsBlock
    ////////////////////////////////////////////////////////////////////////////////

    testNoResultsBlock1: function() {
      const query = 'FOR i IN 1..@rows FILTER 1 == 0 RETURN i';

      // Also if we have no results, we do send a drop-all to dependecies
      // potentielly we have modifiaction nodes that need to be executed.

      const genNodeList = () => [
        {type: SingletonBlock, calls: 1, items: 0},
        {type: CalculationBlock, calls: 1, items: 0},
        {type: EnumerateListBlock, calls: 1, items: 0},
        {type: NoResultsBlock, calls: 1, items: 0},
        {type: ReturnBlock, calls: 1, items: 0},
      ];

      // This is essentially runDefaultChecks, but we cannot use it because
      // of the missing blocks in the stats.

      for (const rows of profHelper.defaultTestRowCounts) {
        const profile = db._query(query, {rows},
          {profile: 2, defaultBatchSize}
        ).getExtra();

        profHelper.assertIsLevel2Profile(profile);
        // This can't work because of the missing blocks in the stats:
        // profHelper.assertStatsNodesMatchPlanNodes(profile);

        const batches = Math.ceil(rows / defaultBatchSize);

        const expected = genNodeList(rows, batches);
        // Like profHelper.getCompactStatsNodes(), but allows for missing stats
        // nodes.
        const actual = profHelper.zipPlanNodesIntoStatsNodes(profile).map(
          node => (
            node.fromStats ?
            {
            type: node.type,
            calls: node.fromStats.calls,
            items: node.fromStats.items,
          } : {})
        );

        profHelper.assertNodesItemsAndCalls(expected, actual,
          {query, rows, batches, expected, actual});
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test SortBlock
////////////////////////////////////////////////////////////////////////////////

    testSortBlock1 : function () {
      const query = 'FOR i IN 1..@rows SORT i DESC RETURN i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : SortBlock, calls : batches, items : rows },
        { type : ReturnBlock, calls : batches, items : rows }
      ];
      profHelper.runDefaultChecks({query, genNodeList});
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test SortBlock
////////////////////////////////////////////////////////////////////////////////

    testSortBlock2 : function () {
      const query = 'FOR i IN @rows..1 SORT i ASC RETURN i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : SortBlock, calls : batches, items : rows },
        { type : ReturnBlock, calls : batches, items : rows }
      ];
      profHelper.runDefaultChecks({query, genNodeList});
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test SortBlock
////////////////////////////////////////////////////////////////////////////////

    testSortBlock3 : function () {
      // effectively sort [ 0, 1, 2, ..., 0, 1, 2, ... ]
      const query = 'FOR i IN 1..@rows SORT i % @mod RETURN i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : CalculationBlock, calls : batches, items : rows },
        { type : SortBlock, calls : batches, items : rows },
        { type : ReturnBlock, calls : batches, items : rows }
      ];
      const bind = rows => ({rows, mod: Math.ceil(rows / 2)});
      profHelper.runDefaultChecks({query, genNodeList, bind});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test SortLimitBlock
////////////////////////////////////////////////////////////////////////////////

    testSortLimitBlock1 : function () {
      const query = 'FOR i IN 1..@rows SORT i DESC LIMIT @offset, @limit RETURN i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : ConstrainedSortBlock, calls : limitMinusSkipBatches(rows), items : limit(rows) },
        { type : LimitBlock, calls : limitMinusSkipBatches(rows), items : limitMinusSkip(rows) },
        { type : ReturnBlock, calls : limitMinusSkipBatches(rows), items : limitMinusSkip(rows) }
      ];
      const bind = rows => ({
        rows,
        // ~1/4 of rows:
        offset: offset(rows),
        // ~1/2 of rows:
        limit: limitMinusSkip(rows),
      });
      profHelper.runDefaultChecks({query, genNodeList, bind, testRowCounts: sortLimitTestRowCounts});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test SortLimitBlock
/// with fullCount
////////////////////////////////////////////////////////////////////////////////

    testSortLimitBlock2 : function () {
      const query = 'FOR i IN 1..@rows SORT i DESC LIMIT @offset, @limit RETURN i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : ConstrainedSortBlock, calls : limitMinusSkipBatches(rows), items : rows },
        { type : LimitBlock, calls : limitMinusSkipBatches(rows), items : limitMinusSkip(rows) },
        { type : ReturnBlock, calls : limitMinusSkipBatches(rows), items : limitMinusSkip(rows) }
      ];
      const bind = rows => ({
        rows,
        // ~1/4 of rows:
        offset: offset(rows),
        // ~1/2 of rows:
        limit: limitMinusSkip(rows),
      });
      profHelper.runDefaultChecks({query, genNodeList, bind, testRowCounts: sortLimitTestRowCounts, options: {fullCount: true}});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test SortedCollectBlock
    ////////////////////////////////////////////////////////////////////////////////

    testSortedCollectBlock1 : function () {
      const query = 'FOR i IN 1..@rows ' +
        'SORT i ' +
        'COLLECT x = i ' +
        'RETURN x';
      const genNodeList = (rows, batches) => {

        return [
          { type: SingletonBlock, calls: 1, items: 1 },
          { type: CalculationBlock, calls: 1, items: 1 },
          { type: EnumerateListBlock, calls: batches, items: rows },
          { type: SortBlock, calls: batches, items: rows },
          { type: SortedCollectBlock, calls: batches, items: rows },
          { type: ReturnBlock, calls: batches, items: rows },
        ];
      };
      profHelper.runDefaultChecks({query, genNodeList});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test SortedCollectBlock
    ////////////////////////////////////////////////////////////////////////////////

    testSortedCollectBlock2 : function () {
      // x is [1,1,1,2,2,2,3,3,3,4,...
      const query = 'FOR i IN 1..@rows ' +
        'COLLECT x = FLOOR((i-1) / 3) + 1 OPTIONS {method: "sorted"} ' +
        'RETURN x';
      const genNodeList = (rows, batches) => {
        const rowsAfterCollect = Math.ceil(rows / 3);
        const batchesAfterCollect = Math.ceil(rowsAfterCollect / defaultBatchSize);

        return [
          { type: SingletonBlock, calls: 1, items: 1 },
          { type: CalculationBlock, calls: 1, items: 1 },
          { type: EnumerateListBlock, calls: batches, items: rows },
          { type: CalculationBlock, calls: batches, items: rows },
          { type: SortBlock, calls: batches, items: rows },
          { type: SortedCollectBlock, calls: batchesAfterCollect, items: rowsAfterCollect },
          { type: ReturnBlock, calls: batchesAfterCollect, items: rowsAfterCollect },
        ];
      };
      profHelper.runDefaultChecks({query, genNodeList});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test SortedCollectBlock
    ////////////////////////////////////////////////////////////////////////////////

    testSortedCollectBlock3 : function () {
      // example:
      // for @rows = 5,  x is [1,2,0,1,2]
      // for @rows = 12, x is [1,2,3,4,5,0,1,2,3,4,5,0]
      const query = 'FOR i IN 1..@rows ' +
        'COLLECT x = i % CEIL(@rows / 2) OPTIONS {method: "sorted"} ' +
        'RETURN x';
      const genNodeList = (rows, batches) => {
        const rowsAfterCollect = Math.ceil(rows / 2);
        const batchesAfterCollect = Math.ceil(rowsAfterCollect / defaultBatchSize);

        return [
          { type: SingletonBlock, calls: 1, items: 1 },
          { type: CalculationBlock, calls: 1, items: 1 },
          { type: EnumerateListBlock, calls: batches, items: rows },
          { type: CalculationBlock, calls: batches, items: rows },
          { type: SortBlock, calls: batches, items: rows },
          { type: SortedCollectBlock, calls: batchesAfterCollect, items: rowsAfterCollect },
          { type: ReturnBlock, calls: batchesAfterCollect, items: rowsAfterCollect },
        ];
      };
      profHelper.runDefaultChecks({query, genNodeList});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test LimitBlock + CountCollectBlock
/// This is a regression test for ES-692.
/// Introduced and fixed in https://github.com/arangodb/arangodb/pull/12719.
////////////////////////////////////////////////////////////////////////////////

    testLimitBlockWithCountCollectBlock : function () {
      const query = `
        FOR i IN 1..@rows
          LIMIT @offset, @limit
          COLLECT WITH COUNT INTO c
          RETURN c
      `;
      const genNodeList = (rows) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : 1, items : limit(rows) },
        { type : LimitBlock, calls : 1, items : limitMinusSkip(rows) },
        { type : CountCollectBlock, calls : 1, items : 1 },
        { type : ReturnBlock, calls : 1, items : 1 }
      ];
      const bind = rows => ({
        rows,
        // ~1/4 of rows:
        offset: offset(rows),
        // ~1/2 of rows:
        limit: limitMinusSkip(rows),
      });
      profHelper.runDefaultChecks({query, bind, genNodeList});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test LimitBlock with fullCount + CountCollectBlock
/// This is a regression test for ES-692.
/// Introduced and fixed in https://github.com/arangodb/arangodb/pull/12719.
////////////////////////////////////////////////////////////////////////////////

    testLimitBlockWithCountCollectBlockAndFullCount : function () {
      const query = `
        FOR i IN 1..@rows
          LIMIT @offset, @limit
          COLLECT WITH COUNT INTO c
          RETURN c
      `;
      const genNodeList = (rows) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : 1, items : rows },
        { type : LimitBlock, calls : 1, items : limitMinusSkip(rows) },
        { type : CountCollectBlock, calls : 1, items : 1 },
        { type : ReturnBlock, calls : 1, items : 1 }
      ];
      const bind = rows => ({
        rows,
        // ~1/4 of rows:
        offset: offset(rows),
        // ~1/2 of rows:
        limit: limitMinusSkip(rows),
      });
      const genStats = rows => ({
        fullCount: rows
      });
      profHelper.runDefaultChecks({query, bind, genNodeList, genStats, options: {fullCount: true}});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test two adjacent LimitBlocks with fullCount.
/// NOTE That this is currently suboptimal! Currently, fullCount is passed
/// through the upper limit block, forcing upstream blocks to iterate everything.
////////////////////////////////////////////////////////////////////////////////

    testLimitBlockWithLimitAndFullCount : function () {
      const query = `
        FOR i IN 1..@rows
          LIMIT @upperOffset, @upperLimit
          LIMIT @lowerOffset, @lowerLimit
          RETURN i
      `;

      // The lower limit works with the rows passing through the upper limit,
      // so rows are divided roughly like this:
      // |                                rows                           |
      // | upperOffset |               upperLimit             |
      // |             | lowerOffset | lowerLimit |
      const upperOffset = rows => offset(rows);
      const upperLimit = rows => limitMinusSkip(rows);
      const lowerOffset = rows => offset(limitMinusSkip(rows));
      const lowerLimit = rows => limitMinusSkip(limitMinusSkip(rows));
      const batches = rows => Math.ceil(rows / defaultBatchSize);

      const genNodeList = (rows) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        // NOTE: `items: rows` should *really* be upperOffset(rows) + upperLimit(rows).
        //       That still needs to be implemented.
        { type : EnumerateListBlock, calls : batches(lowerLimit(rows)), items : rows },
        { type : LimitBlock, calls : batches(lowerLimit(rows)), items : upperLimit(rows) },
        { type : LimitBlock, calls : batches(lowerLimit(rows)), items : lowerLimit(rows) },
        { type : ReturnBlock, calls : batches(lowerLimit(rows)), items : lowerLimit(rows) }
      ];

      const bind = rows => ({
        rows,
        // ~1/4 of rows:
        upperOffset: upperOffset(rows),
        // ~1/2 of rows:
        upperLimit: upperLimit(rows),
        // ~1/8 of rows:
        lowerOffset: lowerOffset(rows),
        // ~1/4 of rows:
        lowerLimit: lowerLimit(rows),
      });
      const genStats = rows => ({
        fullCount: Math.min(rows, upperLimit(rows)),
      });
      profHelper.runDefaultChecks({query, bind, genNodeList, genStats, options: {fullCount: true}});
    },

// TODO Every block must be tested separately. Here follows the list of blocks
// (partly grouped due to the inheritance hierarchy). Intermediate blocks
// like ModificationBlock and BlockWithClients are never instantiated separately
// and therefore don't need to be tested on their own.
// TODO when this is done, write a list of tested blocks at the top of each
// test file.

// *CalculationBlock
// *CountCollectBlock
// *DistinctCollectBlock
// *EnumerateCollectionBlock
// *EnumerateListBlock
// *FilterBlock
// *HashedCollectBlock
// *IndexBlock
// *LimitBlock
// *NoResultsBlock
// *RemoteBlock
// *ReturnBlock
// ShortestPathBlock
// *SingletonBlock
// *SortBlock
// *SortedCollectBlock
// *SortingGatherBlock
// SubqueryBlock
// *TraversalBlock
// *UnsortingGatherBlock
//
// ModificationBlock
// -> RemoveBlock
// -> InsertBlock
// -> UpdateBlock
// -> ReplaceBlock
// -> UpsertBlock
//
// BlockWithClients
// -> ScatterBlock
// -> DistributeBlock
//
// IResearchViewBlockBase
// -> IResearchViewUnorderedBlock
//    -> IResearchViewBlock
// -> IResearchViewOrderedBlock

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlProfilerTestSuite);

return jsunity.done();
