/*jshint globalstrict:true, strict:true, esnext: true */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
const {db, aql} = require('@arangodb');
const console = require('console');
const { getResponsibleServers } = global.ArangoClusterInfo;
const internal = require('internal');
const jsunity = require('jsunity');
const assert = jsunity.jsUnity.assertions;

////////////////////////////////////////////////////////////////////////////////
/// @file test suite for AQL tracing/profiling: cluster tests
/// Contains tests for RemoteBlock, SortingGatherBlock, UnsortingGatherBlock
/// The main test suite (including comments) is in aql-profiler.js.
////////////////////////////////////////////////////////////////////////////////

// TODO Add tests for ScatterBlock and DistributeBlock.

function ahuacatlProfilerTestSuite () {

  // import some names from profHelper directly into our namespace:
  const defaultBatchSize = profHelper.defaultBatchSize;
  const defaultTestRowCounts = profHelper.defaultTestRowCounts;
  const addIntervals = profHelper.addIntervals;

  const { CalculationNode, CollectNode, DistributeNode, EnumerateCollectionNode,
    EnumerateListNode, EnumerateViewNode, FilterNode, GatherNode, IndexNode,
    InsertNode, LimitNode, NoResultsNode, RemoteNode, RemoveNode, ReplaceNode,
    ReturnNode, ScatterNode, ShortestPathNode, SingletonNode, SortNode,
    TraversalNode, UpdateNode, UpsertNode } = profHelper;

  const { CalculationBlock, CountCollectBlock, DistinctCollectBlock,
    EnumerateCollectionBlock, EnumerateListBlock, FilterBlock,
    HashedCollectBlock, IndexBlock, LimitBlock, NoResultsBlock, RemoteBlock,
    ReturnBlock, ShortestPathBlock, SingletonBlock, SortBlock,
    SortedCollectBlock, SortingGatherBlock, TraversalBlock,
    UnsortingGatherBlock, RemoveBlock, InsertBlock, UpdateBlock, ReplaceBlock,
    UpsertBlock, ScatterBlock, DistributeBlock, IResearchViewUnorderedBlock,
    IResearchViewBlock, IResearchViewOrderedBlock } = profHelper;

  // TODO Test with different shard counts, if only to test both minelement and
  //  heap SortingGather.
  const numberOfShards = 5;
  const cn = 'AqlProfilerClusterTestCol';
  let col;

  // Example documents, grouped by the shard that would be responsible.
  let exampleDocumentsByShard;

  // helper functions
  const optimalNonEmptyBatches = rows => Math.ceil(rows / defaultBatchSize);
  const optimalBatches = rows => Math.max(1, optimalNonEmptyBatches(rows));
  const mmfilesBatches = (rows) => Math.max(1, optimalNonEmptyBatches(rows) + (rows % defaultBatchSize === 0 ? 1 : 0));

  const totalItems = (rowsPerShard) =>
    _.sum(_.values(rowsPerShard));
  const dbServerBatches = (rowsPerClient, fuzzy = false) => {
    return _.sum(
      _.values(rowsPerClient)
        .map(fuzzy ? mmfilesBatches : optimalBatches)
    );
  };
  const dbServerBatch = (rows, fuzzy = false) => {
    return (fuzzy ? mmfilesBatches : optimalBatches)(rows);
  };
  const dbServerOptimalBatches = (rowsPerClient) =>
    _.sum(
      _.values(rowsPerClient)
        .map(optimalBatches)
    );
  const groupedBatches = (rowsPerClient, fuzzy) => {
    const callInfo = {calls: 0, overhead: 0};

    for (const [shard, rows] of Object.entries(rowsPerClient)) {
      const testHere = rows + callInfo.overhead;
      callInfo.calls += dbServerBatch(testHere, fuzzy);
      callInfo.overhead = testHere % defaultBatchSize;
    }
    return callInfo.calls;
  };

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
/// @brief set up all
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      col = db._create(cn, {numberOfShards});
      const maxRowsPerShard = _.max(defaultTestRowCounts);
      const shards = col.shards();
      assert.assertEqual(numberOfShards, shards.length);

      // Create an array of size numberOfShards, each entry an empty array.
      exampleDocumentsByShard = _.fromPairs(shards.map(id => [id, []]));

      const allShardsFull = () =>
        _.values(exampleDocumentsByShard)
          .map(elt => elt.length)
          .every(n => n >= maxRowsPerShard);
      let i = 0;
      while (!allShardsFull()) {
        const doc = {_key: i.toString(), i};

        const shard = col.getResponsibleShard(doc);
        if (exampleDocumentsByShard[shard].length < maxRowsPerShard) {
          // shard needs more docs
          exampleDocumentsByShard[shard].push(doc);
        }

        ++i;
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down all
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test RemoteBlock and UnsortingGatherBlock
////////////////////////////////////////////////////////////////////////////////

    testRemoteAndUnsortingGatherBlock : function () {
      const query = `FOR doc IN ${cn} RETURN doc`;

      // Number of local getSome calls that do not return WAITING.
      // This is at least 1.
      // DONE can only be returned when the last shard is asked, so iff the last
      // asked shard is empty, there is one more call (the last call returns
      // DONE without any results).
      // As there is no guaranteed order in which the shards are processed, we
      // have to allow a range.
      const localCalls = (rowsPerShard) => {
        const batches = optimalNonEmptyBatches(_.sum(_.values(rowsPerShard)));
        return [
          Math.max(1, batches),
          Math.max(1, batches+1)
        ];
      };


      // If we figure out that we are done depends on randomness.
      // In some cases we get the full batch on last shard, in this case the DBServer knows it is done.
      // In other cases we get the full batch on an early shard, but 0 documents later, in this case the DBServer does
      // not know it is done in advance.
      const fuzzyDBServerBatches = rowsPerServer => [
        groupedBatches(rowsPerServer, false),
        groupedBatches(rowsPerServer, true)
      ];

      const coordinatorBatches = (rowsPerShard) => addIntervals(fuzzyDBServerBatches(rowsPerShard), localCalls(rowsPerShard));

      const genNodeList = (rowsPerShard, rowsPerServer) => [
        { type : SingletonBlock, calls : numberOfShards, items : numberOfShards },
        { type : EnumerateCollectionBlock, calls : groupedBatches(rowsPerShard), items : totalItems(rowsPerShard) },
        // Twice the number due to WAITING, fuzzy, because the Gather does not know
        { type : RemoteBlock, calls : fuzzyDBServerBatches(rowsPerServer).map(i => i * 2), items : totalItems(rowsPerShard) },
        // We get dbServerBatches(rowsPerShard) times WAITING, plus the non-waiting getSome calls.
        { type : UnsortingGatherBlock, calls : coordinatorBatches(rowsPerServer), items : totalItems(rowsPerShard) },
        { type : ReturnBlock, calls : coordinatorBatches(rowsPerServer), items : totalItems(rowsPerShard) }
      ];
      const options = {optimizer: { rules: ["-parallelize-gather"] } };
      profHelper.runClusterChecks({col, exampleDocumentsByShard, query, genNodeList, options});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test RemoteBlock and SortingGatherBlock
////////////////////////////////////////////////////////////////////////////////

/*
 * Note: disabled this test for now. Using parallel Gather this kind of get's out of hand.
 * The RemoteNode is asked rather often although it does not have data yet.
 * I checked that the upstream Blocks are not called too often.
    testRemoteAndSortingGatherBlock : function () {
      const query = `FOR doc IN ${cn} SORT doc.i RETURN doc`;
      // Number of local getSome calls that do not return WAITING.
      // This is at least 1.
      // If the DBServers could lie about HASMORE, this would increase the calls
      // here. However, due to the SortBlock, this cannot happen with this plan.
      const localCalls = (rowsPerShard) => {
        const batches = optimalBatches(_.sum(_.values(rowsPerShard)));
        return Math.max(1, batches);
      };

      const coordinatorBatches = (rowsPerShard) => addIntervals(dbServerOptimalBatches(rowsPerShard), localCalls(rowsPerShard));

      const genNodeList = (rowsPerShard, rowsPerServer) => [
        { type : SingletonBlock, calls : numberOfShards, items : numberOfShards },
        { type : EnumerateCollectionBlock, calls : dbServerBatches(rowsPerShard), items : totalItems(rowsPerShard) },
        { type : CalculationBlock, calls : dbServerBatches(rowsPerShard), items : totalItems(rowsPerShard) },
        { type : SortBlock, calls : dbServerOptimalBatches(rowsPerShard), items : totalItems(rowsPerShard) },
        // Twice the number due to WAITING, also we will call the upstream even if we do not have data yet (we do not know, if we have data we can continue.)
        { type : RemoteBlock, calls : [2 * dbServerOptimalBatches(rowsPerServer), 2 * coordinatorBatches(rowsPerServer)], items : totalItems(rowsPerShard) },
        // We get dbServerBatches(rowsPerShard) times WAITING, plus the non-waiting getSome calls.
        // In a very lucky case we may get away with 1 call less, that is if the DBServers are fist enough to deliver all data in
        // the same roundtrip interval
        { type : SortingGatherBlock, calls : [coordinatorBatches(rowsPerServer) - 1, coordinatorBatches(rowsPerServer)], items : totalItems(rowsPerShard) },
        { type : ReturnBlock, calls : [coordinatorBatches(rowsPerServer) - 1, coordinatorBatches(rowsPerServer)], items : totalItems(rowsPerShard) }
      ];
      profHelper.runClusterChecks({col, exampleDocumentsByShard, query, genNodeList});
    },
*/

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlProfilerTestSuite);

return jsunity.done();
