/*jshint globalstrict:true, strict:true, esnext: true */

"use strict";

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Tobias Gödderz
// //////////////////////////////////////////////////////////////////////////////

// contains common code for aql-profiler* tests
const profHelper = require("@arangodb/testutils/aql-profiler-test-helper");

const _ = require('lodash');
const {db, aql} = require('@arangodb');
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

  const cn = 'AqlProfilerClusterTestCol';

  // helper functions
  const optimalNonEmptyBatches = rows => Math.ceil(rows / defaultBatchSize);
  const optimalBatches = rows => Math.max(1, optimalNonEmptyBatches(rows));

  const totalItems = (rowsPerShard) =>
    _.sum(_.values(rowsPerShard));
  const dbServerBatches = (rowsPerClient) => {
    return _.sum(
      _.values(rowsPerClient)
        .map(optimalBatches)
    );
  };
  const dbServerBatch = (rows) => {
    return optimalBatches(rows);
  };
  const dbServerOptimalBatches = (rowsPerClient) =>
    _.sum(
      _.values(rowsPerClient)
        .map(optimalBatches)
    );
  const groupedBatches = (rowsPerClient) => {
    const callInfo = {calls: 0, overhead: 0};

    for (const [shard, rows] of Object.entries(rowsPerClient)) {
      const testHere = rows + callInfo.overhead;
      callInfo.calls += dbServerBatch(testHere);
      callInfo.overhead = testHere % defaultBatchSize;
    }
    return callInfo.calls;
  };

  const createCollection = (numberOfShards) => {
    let col = db._create(cn, {numberOfShards});
    const maxRowsPerShard = _.max(defaultTestRowCounts);
    const shards = col.shards();
    assert.assertEqual(numberOfShards, shards.length);

    // Create an array of size numberOfShards, each entry an empty array.
    let exampleDocumentsByShard = _.fromPairs(shards.map(id => [id, []]));

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

    return [col, exampleDocumentsByShard];
  };

  return {
    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test RemoteBlock and UnsortingGatherBlock
////////////////////////////////////////////////////////////////////////////////
    
    testRemoteAndUnsortingGatherBlock : function () {
      // TODO: add tests with different shard numbers
      let shards = [5];
      shards.forEach((numberOfShards) => {
        const query = `FOR doc IN ${cn} RETURN doc`;
      
        const genNodeList = (rowsPerShard, rowsPerServer, rowCount) => { 
          let enumCalls = rowCount.reduce((value, c) => {
            return value + Math.max(1, Math.ceil(c / defaultBatchSize));
          }, 0);

          // the following heuristics are rather fragile and should be reworked.
          // unfortunately the actual number of calls depends partly on randomness,
          // so we specify a lower and an upper bound for the number of calls made.
          let remoteCalls = _.max(rowCount.map((rc) => 1 + Math.max(1, Math.ceil(rc / defaultBatchSize)))) * numberOfShards;
          let gatherCalls = numberOfShards + _.sum(rowCount.map((rc) => Math.floor(rc/ defaultBatchSize)));

          return [
            { type : SingletonBlock, calls : numberOfShards, items : numberOfShards, filtered: 0 },
            { type : EnumerateCollectionBlock, calls : enumCalls, items : totalItems(rowsPerShard), filtered: 0 },
            { type : RemoteBlock, calls : [remoteCalls / 2, remoteCalls * 2], items : totalItems(rowsPerShard), filtered: 0 },
            { type : UnsortingGatherBlock, calls : [gatherCalls / 2 , gatherCalls * 2], items : totalItems(rowsPerShard), filtered: 0 },
            { type : ReturnBlock, calls : [gatherCalls / 2, gatherCalls * 2], items : totalItems(rowsPerShard), filtered: 0 }
          ]; 
        };
        const options = {optimizer: { rules: ["-parallelize-gather"] } };
        let [col, exampleDocumentsByShard] = createCollection(numberOfShards);
        profHelper.runClusterChecks({col, exampleDocumentsByShard, query, genNodeList, options});
        col.drop();
      });
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
