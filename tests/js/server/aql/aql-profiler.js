/*jshint globalstrict:true, strict:true, esnext: true */
/*global AQL_EXPLAIN */

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

// NOTE EnumerateCollectionBlock is suboptimal on mmfiles, is it returns HASMORE
// instead of DONE when asked for exactly all documents in the collection.
// (low impact) (see aql-profiler-noncluster*.js)

// TODO IndexBlock is still suboptimal, as it can return HASMORE when there are no
// items left. (low impact) (see aql-profiler-noncluster*.js)


function ahuacatlProfilerTestSuite () {


  // import some names from profHelper directly into our namespace:
  const defaultBatchSize = profHelper.defaultBatchSize;

  const { CalculationNode, CollectNode, DistributeNode, EnumerateCollectionNode,
    EnumerateListNode, EnumerateViewNode, FilterNode, GatherNode, IndexNode,
    InsertNode, LimitNode, NoResultsNode, RemoteNode, RemoveNode, ReplaceNode,
    ReturnNode, ScatterNode, ShortestPathNode, SingletonNode, SortNode,
    SubqueryNode, TraversalNode, UpdateNode, UpsertNode } = profHelper;

  const { CalculationBlock, CountCollectBlock, DistinctCollectBlock,
    EnumerateCollectionBlock, EnumerateListBlock, FilterBlock,
    HashedCollectBlock, IndexBlock, LimitBlock, NoResultsBlock, RemoteBlock,
    ReturnBlock, ShortestPathBlock, SingletonBlock, SortBlock,
    SortedCollectBlock, SortingGatherBlock, SubqueryBlock, TraversalBlock,
    UnsortingGatherBlock, RemoveBlock, InsertBlock, UpdateBlock, ReplaceBlock,
    UpsertBlock, ScatterBlock, DistributeBlock, IResearchViewUnorderedBlock,
    IResearchViewBlock, IResearchViewOrderedBlock } = profHelper;

  // See the limit tests (e.g. testLimitBlock3) for limit() and skip().
  const additionalLimitTestRowCounts = [
    // limit() = 1000 ± 1:
    1332, 1333, 1334,
    // skip() = 1000 ± 1:
    3999, 4000, 4003, 4004,
    // limit() = 2000 ± 1:
    2665, 2666, 2667,
    // skip() = 2000 ± 1:
    7999, 8000, 8003, 8004,
  ];

  {
    // These are copies from testLimitBlock3.
    const skip = rows => Math.floor(rows/4);
    const limit = rows => Math.ceil(3*rows/4);

    // This is more documentation than anything else:
    assert.assertEqual(999, limit(1332));
    assert.assertEqual(1000, limit(1333));
    assert.assertEqual(1001, limit(1334));
    assert.assertEqual(999, skip(3999));
    assert.assertEqual(1000, skip(4000));
    assert.assertEqual(1000, skip(4003));
    assert.assertEqual(1001, skip(4004));
    assert.assertEqual(1999, limit(2665));
    assert.assertEqual(2000, limit(2666));
    assert.assertEqual(2001, limit(2667));
    assert.assertEqual(1999, skip(7999));
    assert.assertEqual(2000, skip(8000));
    assert.assertEqual(2000, skip(8003));
    assert.assertEqual(2001, skip(8004));
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
/// @brief test {profile: 0}
////////////////////////////////////////////////////////////////////////////////

    testProfile0Fields : function () {
      const query = 'RETURN 1';
      const profileDefault = db._query(query, {}).getExtra();
      const profile0 = db._query(query, {}, {profile: 0}).getExtra();
      const profileFalse = db._query(query, {}, {profile: false}).getExtra();

      profHelper.assertIsLevel0Profile(profileDefault);
      profHelper.assertIsLevel0Profile(profile0);
      profHelper.assertIsLevel0Profile(profileFalse);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test {profile: 1}
////////////////////////////////////////////////////////////////////////////////

    testProfile1Fields : function () {
      const query = 'RETURN 1';
      const profile1 = db._query(query, {}, {profile: 1}).getExtra();
      const profileTrue = db._query(query, {}, {profile: true}).getExtra();

      profHelper.assertIsLevel1Profile(profile1);
      profHelper.assertIsLevel1Profile(profileTrue);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test {profile: 2}
////////////////////////////////////////////////////////////////////////////////

    testProfile2Fields : function () {
      const query = 'RETURN 1';
      const profile2 = db._query(query, {}, {profile: 2}).getExtra();

      profHelper.assertIsLevel2Profile(profile2);
      profHelper.assertStatsNodesMatchPlanNodes(profile2);
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
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
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
        {type: CalculationBlock, calls: 1, items: 1},
        {type: EnumerateListBlock, calls: batches, items: rows},
        {type: FilterBlock, calls: batches, items: rows},
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
      const query = 'FOR i IN 1..@rows LIMIT @skip, @limit RETURN i';
      const skip = rows => Math.floor(rows/4);
      const skipBatches = rows => Math.ceil(skip(rows) / defaultBatchSize);
      const limit = rows => Math.ceil(3*rows/4);
      const limitBatches = rows => Math.ceil(limit(rows) / defaultBatchSize);

      const genNodeList = (rows, batches) => [
        {type: SingletonBlock, calls: 1, items: 1},
        {type: CalculationBlock, calls: 1, items: 1},
        {type: EnumerateListBlock, calls: limitBatches(rows) + skipBatches(rows), items: limit(rows) + skip(rows)},
        {type: LimitBlock, calls: limitBatches(rows), items: limit(rows)},
        {type: ReturnBlock, calls: limitBatches(rows), items: limit(rows)},
      ];
      const bind = (rows) => ({
        rows,
        skip: skip(rows),
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

      // As the descendant blocks of NoResultsBlock don't get a single getSome
      // call, they don't show up in the statistics.

      const genNodeList = () => [
        {type: SingletonBlock, calls: 0, items: 0},
        {type: CalculationBlock, calls: 0, items: 0},
        {type: NoResultsBlock, calls: 1, items: 0},
        {type: EnumerateListBlock, calls: 1, items: 0},
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
// RemoteBlock
// *ReturnBlock
// ShortestPathBlock
// *SingletonBlock
// *SortBlock
// *SortedCollectBlock
// SortingGatherBlock
// SubqueryBlock
// *TraversalBlock
// UnsortingGatherBlock
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
