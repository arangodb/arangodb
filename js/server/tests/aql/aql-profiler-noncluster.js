/*jshint globalstrict:true, strict:true, esnext: true */

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

const _ = require('lodash');
const console = require('console');
const db = require('@arangodb').db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for AQL tracing/profiling: noncluster tests
////////////////////////////////////////////////////////////////////////////////

function ahuacatlProfilerTestSuite () {

  // import some names from profHelper directly into our namespace:
  const colName = profHelper.colName;
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

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test EnumerateCollectionBlock
////////////////////////////////////////////////////////////////////////////////

    testEnumerateCollectionBlock1: function () {
      const col = db._create(colName);
      const prepare = (rows) => {
        col.truncate();
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = () => ({'@col': colName});
      const query = `FOR d IN @@col RETURN d.value`;

      const genNodeList = (rows, batches) => {
        if (db._engine().name === 'mmfiles') {
          // mmfiles lies about hasMore when asked for exactly the number of
          // arguments left in the collection, so we have 1 more call when
          // defaultBatchSize divides the actual number of rows.
          // rocksdb on the other hand is exact.
          batches = Math.floor(rows / defaultBatchSize) + 1;
        }
        return [
          {type: SingletonBlock, calls: 1, items: 1},
          {type: EnumerateCollectionBlock, calls: batches, items: rows},
          {type: CalculationBlock, calls: batches, items: rows},
          {type: ReturnBlock, calls: batches, items: rows}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test EnumerateCollectionBlock with multiple input rows. slow.
////////////////////////////////////////////////////////////////////////////////

    testEnumerateCollectionBlock2: function () {
      const col = db._create(colName);
      const query = `FOR i IN 1..@listRows FOR d IN @@col RETURN d.value`;
      const listRowCounts = [1, 2, 3, 100, 999, 1000, 1001];
      const collectionRowCounts = [1, 2, 499, 500, 501, 999, 1000, 1001];

      for (const collectionRows of collectionRowCounts) {
        col.truncate();
        col.insert(_.range(1, collectionRows + 1).map((i) => ({value: i})));
        for (const listRows of listRowCounts) {
          // forbid reordering of the enumeration nodes
          const profile = db._query(query, {listRows, '@col': colName},
            {
              profile: 2,
              optimizer: {rules: ["-interchange-adjacent-enumerations"]}
            }
          ).getExtra();

          profHelper.assertIsLevel2Profile(profile);
          profHelper.assertStatsNodesMatchPlanNodes(profile);

          const listBatches = Math.ceil(listRows / defaultBatchSize);
          const totalRows = listRows * collectionRows;

          const optimalBatches = Math.ceil(totalRows / defaultBatchSize);

          // This is more complex due to two reasons:
          // - mmfiles lies about hasMore and may result in +1
          // - the current EnumerateCollectionBlock::getSome implementation
          //   stops after iterating over the whole collection.
          // The second point in turn means that
          // a) there are at least as many batches as input rows times
          //    the number of batches in the collection, i.e. ceil(#col/#batch).
          // b) when, e.g., the collection contains 999, the parent block might
          //    ask for 1 after receiving 999, which can up to double the count
          //    described under a).
          // So endBatches is a sharp lower bound for every implementation,
          // while the upper bound may be reduced if the implementation changes.

          // Number of batches at the enumerate collection node
          const enumerateCollectionBatches = [
            optimalBatches,
            listRows * Math.ceil(collectionRows / defaultBatchSize) * 2 + 1
          ];

          // Number of batches at the return node
          let endBatches = optimalBatches;
          if (db._engine().name === 'mmfiles') {
            endBatches = [optimalBatches, optimalBatches + 1];
          }


          const expected = [
            {type: SingletonBlock, items: 1, calls: 1},
            {type: CalculationBlock, items: 1, calls: 1},
            {type: EnumerateListBlock, items: listRows, calls: listBatches},
            {
              type: EnumerateCollectionBlock,
              items: totalRows,
              calls: enumerateCollectionBatches
            },
            {type: CalculationBlock, items: totalRows, calls: endBatches},
            {type: ReturnBlock, items: totalRows, calls: endBatches}
          ];
          const actual = profHelper.getCompactStatsNodes(profile);

          profHelper.assertNodesItemsAndCalls(expected, actual,
            {query, listRows, collectionRows});
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test IndexBlock
////////////////////////////////////////////////////////////////////////////////

    testIndexBlock1 : function () {
      const col = db._create(colName);
      col.ensureIndex({ type: "hash", fields: [ "value" ] });
      const prepare = (rows) => {
        col.truncate();
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = (rows) => ({'@col': colName, rows});
      const query = `FOR i IN 1..@rows FOR d IN @@col FILTER i == d.value RETURN d.value`;

      const genNodeList = (rows, batches) => {
        // IndexBlock returns HASMORE when asked for the exact number of items
        // it has left. This could be improved.
        const indexBatches = Math.floor(rows / defaultBatchSize) + 1;
        return [
          {type: SingletonBlock, calls: 1, items: 1},
          {type: CalculationBlock, calls: 1, items: 1},
          {type: EnumerateListBlock, calls: batches, items: rows},
          {type: IndexBlock, calls: indexBatches, items: rows},
          {type: CalculationBlock, calls: indexBatches, items: rows},
          {type: ReturnBlock, calls: indexBatches, items: rows}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test IndexBlock, asking for every third document
////////////////////////////////////////////////////////////////////////////////

    testIndexBlock2 : function () {
      const col = db._create(colName);
      col.ensureIndex({ type: "hash", fields: [ "value" ] });
      const prepare = (rows) => {
        col.truncate();
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = (rows) => ({'@col': colName, rows});
      const query = `FOR i IN 0..FLOOR(@rows / 3)
        FOR d IN @@col
        FILTER i*3+1 == d.value
        RETURN d.value`;

      const genNodeList = (rows) => {
        const enumRows = Math.floor(rows / 3) + 1;
        const enumBatches = Math.ceil(enumRows / defaultBatchSize);
        const indexRows = Math.ceil(rows / 3);

        const optimalBatches = Math.ceil(indexRows / defaultBatchSize);
        // IndexBlock returns HASMORE when asked for the exact number of items
        // it has left. This could be improved.
        const maxIndexBatches = Math.max(1, Math.floor(indexRows / defaultBatchSize) + 1);
        // Number of calls made to the index block. See comment for
        // maxIndexBatches. As of now, maxIndexBatches is exact, but we don't
        // want to fail when this improves.
        const indexBatches = [
          optimalBatches,
          maxIndexBatches
        ];

        return [
          {type: SingletonBlock, calls: 1, items: 1},
          {type: CalculationBlock, calls: 1, items: 1},
          {type: EnumerateListBlock, calls: enumBatches, items: enumRows},
          {type: IndexBlock, calls: indexBatches, items: indexRows},
          {type: CalculationBlock, calls: indexBatches, items: indexRows},
          {type: ReturnBlock, calls: indexBatches, items: indexRows}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test IndexBlock with multiple input rows. slow.
////////////////////////////////////////////////////////////////////////////////

    testIndexBlock3 : function () {
      const col = db._create(colName);
      col.ensureIndex({ type: "hash", fields: [ "value" ] });
      const query = `FOR i IN 1..@listRows FOR k IN 1..@collectionRows FOR d IN @@col FILTER d.value == k RETURN d.value`;
      const listRowCounts = [1, 2, 3, 100, 999, 1000, 1001];
      const collectionRowCounts = [1, 2, 499, 500, 501, 999, 1000, 1001];

      for (const collectionRows of collectionRowCounts) {
        col.truncate();
        col.insert(_.range(1, collectionRows + 1).map((i) => ({value: i})));
        for (const listRows of listRowCounts) {
          // forbid reordering of the enumeration nodes as well as removal
          // of one CalculationNode in case listRows == collectionRows.
          const profile = db._query(query,
            {listRows, collectionRows, '@col': colName},
            {
              profile: 2,
              optimizer: {rules: [
                  "-interchange-adjacent-enumerations",
                  "-remove-redundant-calculations"
                ]}
            }
          ).getExtra();

          profHelper.assertIsLevel2Profile(profile);
          profHelper.assertStatsNodesMatchPlanNodes(profile);

          const listBatches = Math.ceil(listRows / defaultBatchSize);
          const totalRows = listRows * collectionRows;

          const optimalBatches = Math.ceil(totalRows / defaultBatchSize);


          // The current IndexBlock::getSome implementation, like the
          // EnumerateCollectionBlock::getSome implementation, stops after
          // iterating over the index. Thus
          // a) there are at least as many batches as input rows times
          //    the number of batches in the collection, i.e. ceil(#col/#batch).
          // b) when, e.g., the collection contains 999, the parent block might
          //    ask for 1 after receiving 999, which can up to double the count
          //    described under a).
          // So endBatches is a sharp lower bound for every implementation,
          // while the upper bound may be reduced if the implementation changes.

          // Number of batches the index block fetches from its child
          const indexCallsBatches = [
            optimalBatches,
            listRows * Math.ceil(collectionRows / defaultBatchSize) * 2 + 1
          ];

          // IndexBlock returns HASMORE when asked for the exact number of items
          // it has left. This could be improved.
          const maxIndexBatches = Math.floor(totalRows / defaultBatchSize) + 1;
          // Number of calls made to the index block. See comment for
          // maxIndexBatches. As of now, maxIndexBatches is exact, but we don't
          // want to fail when this improves.
          const indexBatches = [
            optimalBatches,
            maxIndexBatches
          ];

          const expected = [
            {type: SingletonBlock, calls: 1, items: 1},
            {type: CalculationBlock, calls: 1, items: 1},
            {type: CalculationBlock, calls: 1, items: 1},
            {type: EnumerateListBlock, calls: listBatches, items: listRows},
            {type: EnumerateListBlock, calls: indexCallsBatches, items: totalRows},
            {type: IndexBlock, calls: indexBatches, items: totalRows},
            {type: CalculationBlock, calls: indexBatches, items: totalRows},
            {type: ReturnBlock, calls: indexBatches, items: totalRows}
          ];
          const actual = profHelper.getCompactStatsNodes(profile);

          profHelper.assertNodesItemsAndCalls(expected, actual,
            {query, listRows, collectionRows, expected, actual});
        }
      }
    },

  };
}
