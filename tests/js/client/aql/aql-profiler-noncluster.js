 /*jshint globalstrict:true, strict:true, esnext: true */
/*global assertTrue */

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
const isEnterprise = require("internal").isEnterprise();
const _ = require('lodash');
const db = require('@arangodb').db;
const jsunity = require("jsunity");
const internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @file test suite for AQL tracing/profiling: noncluster tests
/// Contains tests for EnumerateCollectionBlock, IndexBlock and TraversalBlock.
/// The main test suite (including comments) is in aql-profiler.js.
////////////////////////////////////////////////////////////////////////////////

// import some names from profHelper directly into our namespace
const colName = profHelper.colName;
const edgeColName = profHelper.edgeColName;
const viewName = profHelper.viewName;
const defaultBatchSize = profHelper.defaultBatchSize;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
let IM = global.instanceManager;

const {
 AsyncNode, CalculationNode, CollectNode, DistributeNode, EnumerateCollectionNode,
 EnumerateListNode, EnumerateViewNode, FilterNode, GatherNode, IndexNode,
 InsertNode, LimitNode, MaterializeNode, MutexNode, NoResultsNode, RemoteNode, RemoveNode, ReplaceNode,
 ReturnNode, ScatterNode, ShortestPathNode, SingletonNode, SortNode,
 TraversalNode, UpdateNode, UpsertNode
} = profHelper;

const {
 AsyncBlock, CalculationBlock, CountCollectBlock, DistinctCollectBlock,
 EnumerateCollectionBlock, EnumerateListBlock, FilterBlock,
 HashedCollectBlock, IndexBlock, LimitBlock, MaterializeBlock, MutexBlock, NoResultsBlock, RemoteBlock,
 ReturnBlock, ShortestPathBlock, SingletonBlock, SortBlock,
 SortedCollectBlock, SortingGatherBlock, TraversalBlock,
 UnsortingGatherBlock, RemoveBlock, InsertBlock, UpdateBlock, ReplaceBlock,
 UpsertBlock, ScatterBlock, DistributeBlock, IResearchViewUnorderedBlock,
 IResearchViewBlock, IResearchViewOrderedBlock
} = profHelper;

function ahuacatlProfilerTestSuite() {
  return {
    tearDown: function () {
      db._drop(colName);
      db._drop(edgeColName);
    },

    testMaterializeBlock: function () {
      IM.debugSetFailAt('batch-materialize-no-estimation');
      const col = db._create(colName);
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      let indexMeta = {};
      if (isEnterprise) {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          "value"
        ]};
      } else {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested[*]"},
          "value"
        ]};
      }
      col.ensureIndex(indexMeta);
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      col.ensureIndex({ type: "persistent", fields: ["value", "other", "more"]});

      const bind = () => ({'@col': colName});
      const prepare = (rows) => {
        col.truncate({ compact: false });
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const query = `FOR d IN @@col FILTER d.value <= 10 SORT d.more LIMIT 3 RETURN d`;    
      // we intentionally disable the constrained heap sort in this test, so
      // we always have a regular sort block
      const options = {optimizer: {rules: ["-sort-limit"]}};

      const genNodeList = (rows, batches) => {
        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: IndexBlock, calls: 1, items: Math.min(rows, 10), filtered: 0},
          {type: SortBlock, calls: 1, items: Math.min(rows, 3), filtered: 0},
          {type: LimitBlock, calls: 1, items: Math.min(rows, 3), filtered: 0},
          {type: MaterializeBlock, calls: 1, items: Math.min(rows, 3), filtered: 0},
          {type: ReturnBlock, calls: 1, items: Math.min(rows, 3), filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind, options}
      );

      // collection was truncated. Insert them again
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      const iiQuery = `FOR d IN ${colName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} 
         FILTER d.value <= 10 SORT d.more LIMIT 3 RETURN d`;
      let iiQueryRes = db._query(iiQuery).toArray();
      assertTrue(iiQueryRes.length >= 2); // 2 docs with nested + more from 'prepare' function
      IM.debugClearFailAt();
    },

    testMaterializeBlockThatFilters: function () {
      if (!IM.debugCanUseFailAt()) {
        return;
      }

      try {
        IM.debugSetFailAt('batch-materialize-no-estimation');
        const col = db._create(colName);
        col.insert({name_1: "foo", "value_nested": [{"nested_1": [{"nested_2": "foo123"}]}]});
        let indexMeta = {};
        if (isEnterprise) {
          indexMeta = {
            type: 'inverted', name: 'inverted', fields: [
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value"
            ]
          };
        } else {
          indexMeta = {
            type: 'inverted', name: 'inverted', fields: [
              {"name": "value_nested[*]"},
              "value"
            ]
          };
        }
        col.ensureIndex(indexMeta);
        col.insert({name_1: "bar", "value_nested": [{"nested_1": [{"nested_2": "foo321"}]}]});
        col.ensureIndex({type: "persistent", fields: ["value", "other", "more"]});

        const bind = () => ({'@col': colName});
        const prepare = (rows) => {
          col.truncate({compact: false});
          col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
        };
        const query = `FOR d IN @@col FILTER d.value <= 10 SORT d.more LIMIT 3 RETURN d`;
        // we intentionally disable the constrained heap sort in this test, so
        // we always have a regular sort block
        const options = {optimizer: {rules: ["-sort-limit"]}};

        const genNodeList = (rows, batches) => {
          return [
            {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
            {type: IndexBlock, calls: 1, items: Math.min(rows, 10), filtered: 0},
            {type: SortBlock, calls: 1, items: Math.min(rows, 3), filtered: 0},
            {type: LimitBlock, calls: 1, items: Math.min(rows, 3), filtered: 0},
            {type: MaterializeBlock, calls: 1, items: Math.min(rows, 3), filtered: 0},
            {type: ReturnBlock, calls: 1, items: Math.min(rows, 3), filtered: 0}
          ];
        };

        profHelper.runDefaultChecks(
          {query, genNodeList, prepare, bind, options}
        );

        // collection was truncated. Insert them again
        col.insert({name_1: "foo", "value_nested": [{"nested_1": [{"nested_2": "foo123"}]}]});
        col.insert({name_1: "bar", "value_nested": [{"nested_1": [{"nested_2": "foo321"}]}]});
        const iiQuery = `FOR d IN ${colName} OPTIONS {indexHint: 'inverted', 
          forceIndexHint: true, waitForSync: true} FILTER d.value <= 10 SORT d.more LIMIT 3 RETURN d`;
        let iiQueryRes = db._query(iiQuery).toArray();
        assertTrue(iiQueryRes.length >= 2); // 2 docs with nested + more from 'prepare' function

      } finally {
        // clear failure points!
        IM.debugClearFailAt();
      }
    },

    testEnumerateCollectionBlock: function () {
      const col = db._create(colName);
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      let indexMeta = {};
      if (isEnterprise) {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          "value"
        ]};
      } else {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested[*]"},
          "value"
        ]};
      }
      col.ensureIndex(indexMeta);
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      const prepare = (rows) => {
        col.truncate({ compact: false });
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = () => ({'@col': colName});
      const query = `FOR d IN @@col RETURN d.value`;
      const options = {optimizer: {rules: ["-optimize-projections"]}};

      const genNodeList = (rows, batches) => {
        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: EnumerateCollectionBlock, calls: batches, items: rows, filtered: 0},
          {type: CalculationBlock, calls: batches, items: rows, filtered: 0},
          {type: ReturnBlock, calls: batches, items: rows, filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind, options}
      );
    },

    testEnumerateCollectionBlockWithFilter: function () {
      const col = db._create(colName);
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      let indexMeta = {};
      if (isEnterprise) {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          "value"
        ]};
      } else {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested[*]"},
          "value"
        ]};
      }
      col.ensureIndex(indexMeta);
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      const prepare = (rows) => {
        col.truncate({ compact: false });
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = () => ({'@col': colName});
      const query = `FOR d IN @@col FILTER d.value <= 10 RETURN d`;
      const options = {optimizer: {rules: ["-optimize-projections"]}};

      const genNodeList = (rows, batches) => {
        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: EnumerateCollectionBlock, calls: 1, items: Math.min(rows, 10), filtered: rows <= 10 ? 0 : rows - 10},
          {type: ReturnBlock, calls: 1, items: Math.min(rows, 10), filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind, options}
      );

      // collection was truncated. Insert them again
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      const iiQuery = `FOR d IN ${colName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER d.value <= 10 RETURN d`;
      let iiQueryRes = db._query(iiQuery).toArray();
      assertTrue(iiQueryRes.length >= 2); // 2 docs with nested + more from 'prepare' function 
    },

    testFilterBlock: function () {
      const col = db._create(colName);
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      let indexMeta = {};
      if (isEnterprise) {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          "value"
        ]};
      } else {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested[*]"},
          "value"
        ]};
      }
      col.ensureIndex(indexMeta);
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      const prepare = (rows) => {
        col.truncate({ compact: false });
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = () => ({'@col': colName});
      const query = `FOR d IN @@col FILTER d.value <= 10 RETURN d`;

      const genNodeList = (rows, batches) => {
        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: EnumerateCollectionBlock, calls: batches, items: rows, filtered: 0},
          {type: CalculationBlock, calls: batches, items: rows, filtered: 0},
          {type: FilterBlock, calls: 1, items: Math.min(rows, 10), filtered: rows <= 10 ? 0 : rows - 10},
          {type: ReturnBlock, calls: 1, items: Math.min(rows, 10), filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind, options: { optimizer: { rules: ["-move-filters-into-enumerate"] } }}
      );

      // collection was truncated. Insert them again
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      const iiQuery = `FOR d IN ${colName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER d.value <= 10 RETURN d`;
      let iiQueryRes = db._query(iiQuery).toArray();
      assertTrue(iiQueryRes.length >= 2); // 2 docs with nested + more from 'prepare' function 
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test IndexBlock
    ////////////////////////////////////////////////////////////////////////////////

    testIndexBlock1 : function () {
      const col = db._create(colName);
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      let indexMeta = {};
      if (isEnterprise) {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          "value"
        ]};
      } else {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested[*]"},
          "value"
        ]};
      }
      col.ensureIndex(indexMeta);
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      col.ensureIndex({ type: "hash", fields: [ "value" ] });
      const prepare = (rows) => {
        col.truncate({ compact: false });
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = (rows) => ({'@col': colName, rows});
      const query = `FOR i IN 1..@rows FOR d IN @@col FILTER i == d.value RETURN d.value`;

      const genNodeList = (rows, batches) => {
        // IndexBlock returns HASMORE when asked for the exact number of items
        // it has left. This could be improved.
        const optimalBatches = Math.ceil(rows / defaultBatchSize);
        const maxIndexBatches = Math.floor(rows / defaultBatchSize) + 1;
        const indexBatches = [optimalBatches, maxIndexBatches];

        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: CalculationBlock, calls: 1, items: 1, filtered: 0},
          {type: EnumerateListBlock, calls: batches, items: rows, filtered: 0},
          {type: IndexBlock, calls: indexBatches, items: rows, filtered: 0},
          {type: ReturnBlock, calls: indexBatches, items: rows, filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind, options: { optimizer: { rules: ["-interchange-adjacent-enumerations"] } }}
      );

      const iiQuery = `FOR i IN 1..1000 
        FOR d IN ${colName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER d.value == i RETURN d.value`;
      // collection was truncated. Insert them again
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      /*
        PLEASE UNCOMMENT LINES BELOW AFTER FIXING https://arangodb.atlassian.net/browse/SEARCH-449
        */
      // let iiQueryRes = db._query(iiQuery).toArray();
      // assertTrue(iiQueryRes.length >= 2); // 2 docs with nested + more from 'prepare' function 
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test IndexBlock, asking for every third document
    ////////////////////////////////////////////////////////////////////////////////

    testIndexBlock2 : function () {
      const col = db._create(colName);
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      let indexMeta = {};
      if (isEnterprise) {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          "value"
        ]};
      } else {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested[*]"},
          "value"
        ]};
      }
      col.ensureIndex(indexMeta);
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      col.ensureIndex({ type: "hash", fields: [ "value" ] });
      const prepare = (rows) => {
        col.truncate({ compact: false });
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = (rows) => ({'@col': colName, rows});
      const query = `FOR i IN 0..FLOOR(@rows / 3)
        FOR d IN @@col
        FILTER i * 3 + 1 == d.value
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
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: CalculationBlock, calls: 1, items: 1, filtered: 0},
          {type: EnumerateListBlock, calls: enumBatches, items: enumRows, filtered: 0},
          {type: IndexBlock, calls: indexBatches, items: indexRows, filtered: 0},
          {type: ReturnBlock, calls: indexBatches, items: indexRows, filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind, options: { optimizer: { rules: ["-interchange-adjacent-enumerations"] } }}
      );

      // collection was truncated. Insert them again
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      const iiQuery = `FOR i IN 0..FLOOR(1000 / 3)
        FOR d IN ${colName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true}
        FILTER i * 3 + 1 == d.value
        RETURN d.value`;  
      /*
        PLEASE UNCOMMENT LINES BELOW AFTER FIXING https://arangodb.atlassian.net/browse/SEARCH-449
        */  
      // let iiQueryRes = db._query(iiQuery).toArray();
      // assertTrue(iiQueryRes.length >= 2); // 2 docs with nested + more from 'prepare' function 
    },

    testIndexBlockWithProjection: function () {
      const col = db._create(colName);
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      let indexMeta = {};
      if (isEnterprise) {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          "value"
        ]};
      } else {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested[*]"},
          "value"
        ]};
      }
      col.ensureIndex(indexMeta);
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      col.ensureIndex({ type: "hash", fields: [ "value" ] });
      const prepare = (rows) => {
        col.truncate({ compact: false });
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = () => ({'@col': colName});
      const query = `FOR d IN @@col RETURN d.value`;

      const genNodeList = (rows, batches) => {
        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: IndexBlock, calls: batches, items: rows, filtered: 0},
          {type: ReturnBlock, calls: batches, items: rows, filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );
    },

    testIndexBlockWithFilter: function () {
      const col = db._create(colName);
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      let indexMeta = {};
      if (isEnterprise) {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          "value"
        ]};
      } else {
        indexMeta = {type: 'inverted', name: 'inverted', fields: [
          {"name": "value_nested[*]"},
          "value"
        ]};
      }
      col.ensureIndex(indexMeta);
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      col.ensureIndex({ type: "hash", fields: [ "value" ] });
      const prepare = (rows) => {
        col.truncate({ compact: false });
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = () => ({'@col': colName});
      const query = `FOR d IN @@col FILTER d.value <= 10 RETURN d`;

      const genNodeList = (rows, batches) => {
        if (rows >= 100) {
          return [
            {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
            {type: IndexBlock, calls: 1, items: Math.min(rows, 10), filtered: 0},
            {type: MaterializeBlock, calls: 1, items: Math.min(rows, 10), filtered: 0},
            {type: ReturnBlock, calls: 1, items: Math.min(rows, 10), filtered: 0}
          ];
        } else {
          return [
            {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
            {type: IndexBlock, calls: 1, items: Math.min(rows, 10), filtered: 0},
            {type: ReturnBlock, calls: 1, items: Math.min(rows, 10), filtered: 0}
          ];
        }
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );

      // collection was truncated. Insert them again
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      const iiQuery = `FOR d IN ${colName}
        OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER d.value <= 10 RETURN d`;
      let iiQueryRes = db._query(iiQuery).toArray();
      assertTrue(iiQueryRes.length >= 2); // 2 docs with nested + more from 'prepare' function 
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test TraversalBlock: traverse a tree
    ////////////////////////////////////////////////////////////////////////////////

    testTraversalBlock1: function () {
      const col = db._createDocumentCollection(colName);
      const edgeCol = db._createEdgeCollection(edgeColName);
      const prepare = (rows) => {
        profHelper.createBinaryTree(col, edgeCol, rows);
      };
      const query = `WITH @@vertexCol FOR v IN 0..@rows OUTBOUND @root @@edgeCol RETURN v`;
      const rootNodeId = `${colName}/1`;
      const bind = rows => ({
        rows: rows,
        root: rootNodeId,
        '@edgeCol': edgeColName,
        '@vertexCol': colName,
      });

      const genNodeList = (rows, batches) => {
        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: TraversalBlock, calls: batches, items: rows, filtered: 0},
          {type: ReturnBlock, calls: batches, items: rows, filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test TraversalBlock: traverse ~half a tree
    ////////////////////////////////////////////////////////////////////////////////

    testTraversalBlock2: function () {
      const col = db._createDocumentCollection(colName);
      const edgeCol = db._createEdgeCollection(edgeColName);
      const prepare = (rows) => {
        profHelper.createBinaryTree(col, edgeCol, rows);
      };
      const query = `WITH @@vertexCol FOR v IN 0..@depth OUTBOUND @root @@edgeCol RETURN v`;
      const rootNodeId = `${colName}/1`;
      // actual tree depth:
      // const treeDepth = rows => Math.ceil(Math.log2(rows));
      // tree is perfect up to this depth:
      const maxFullDepth = rows => Math.floor(Math.log2(rows));
      // substract one to get rid of ~half the nodes, but never go below 0
      const depth = rows => Math.max(0, maxFullDepth(rows) - 1);
      const bind = rows => ({
        depth: depth(rows),
        root: rootNodeId,
        '@edgeCol': edgeColName,
        '@vertexCol': colName,
      });
      const visitedNodes = rows => Math.pow(2, depth(rows)+1) - 1;

      const genNodeList = (rows, batches) => {
        rows = visitedNodes(rows);
        batches = Math.ceil(rows / defaultBatchSize);
        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: TraversalBlock, calls: batches, items: rows, filtered: 0},
          {type: ReturnBlock, calls: batches, items: rows, filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test TraversalBlock: skip ~half a tree
    ////////////////////////////////////////////////////////////////////////////////

    testTraversalBlock3: function () {
      const col = db._createDocumentCollection(colName);
      const edgeCol = db._createEdgeCollection(edgeColName);
      const prepare = (rows) => {
        profHelper.createBinaryTree(col, edgeCol, rows);
      };
      const query = `WITH @@vertexCol FOR v IN @depth..@rows OUTBOUND @root @@edgeCol RETURN v`;
      const rootNodeId = `${colName}/1`;
      // actual tree depth:
      // const treeDepth = rows => Math.ceil(Math.log2(rows));
      // tree is perfect up to this depth:
      const maxFullDepth = rows => Math.floor(Math.log2(rows));
      // substract one to leave ~half the nodes, but never go below 0
      const depth = rows => Math.max(0, maxFullDepth(rows) - 1);
      const bind = rows => ({
        rows: rows,
        depth: depth(rows),
        root: rootNodeId,
        '@edgeCol': edgeColName,
        '@vertexCol': colName,
      });
      const skippedNodes = rows => Math.pow(2, depth(rows)) - 1;
      const visitedNodes = rows => rows - skippedNodes(rows);

      const genNodeList = (rows, batches) => {
        rows = visitedNodes(rows);
        batches = Math.max(1, Math.ceil(rows / defaultBatchSize));
        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: TraversalBlock, calls: batches, items: rows, filtered: 0},
          {type: ReturnBlock, calls: batches, items: rows, filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test TraversalBlock: check filter result
    ////////////////////////////////////////////////////////////////////////////////

    testTraversalBlockWithFilter: function () {
      const col = db._createDocumentCollection(colName);
      const edgeCol = db._createEdgeCollection(edgeColName);
      const prepare = (rows) => {
        profHelper.createBinaryTree(col, edgeCol, rows);
      };
      const query = `WITH @@vertexCol FOR v, e, p IN @depth..@rows OUTBOUND @root @@edgeCol FILTER p.vertices[*].value ALL > 100000 RETURN v`;
      const rootNodeId = `${colName}/1`;
      // actual tree depth:
      // const treeDepth = rows => Math.ceil(Math.log2(rows));
      // tree is perfect up to this depth:
      const maxFullDepth = rows => Math.floor(Math.log2(rows));
      // substract one to leave ~half the nodes, but never go below 0
      const depth = rows => Math.max(0, maxFullDepth(rows) - 1);
      const bind = rows => ({
        rows: rows,
        depth: depth(rows),
        root: rootNodeId,
        '@edgeCol': edgeColName,
        '@vertexCol': colName,
      });
      const skippedNodes = rows => Math.pow(2, depth(rows)) - 1;
      const visitedNodes = rows => rows - skippedNodes(rows);

      const genNodeList = (rows, batches) => {
        // note: filter condition is never true
        rows = 0;
        batches = Math.max(1, Math.ceil(rows / defaultBatchSize));

        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: TraversalBlock, calls: batches, items: rows, filtered: 1},
          {type: ReturnBlock, calls: batches, items: rows, filtered: 0}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test TraversalBlock: traverse ~half a tree
    ////////////////////////////////////////////////////////////////////////////////

    /* TODO: enable this test once we have parallelism ready 
    testTraversalBlockParallel: function () {
      const col = db._createDocumentCollection(colName);
      const edgeCol = db._createEdgeCollection(edgeColName);
      const prepare = (rows) => {
        profHelper.createBinaryTree(col, edgeCol, rows);
      };
      const query = `FOR v IN 0..@depth OUTBOUND @root @@edgeCol OPTIONS {parallelism:2} RETURN v`;
      const rootNodeId = `${colName}/1`;
    // actual tree depth:
    // const treeDepth = rows => Math.ceil(Math.log2(rows));
    // tree is perfect up to this depth:
      const maxFullDepth = rows => Math.floor(Math.log2(rows));
    // substract one to get rid of ~half the nodes, but never go below 0
      const depth = rows => Math.max(0, maxFullDepth(rows) - 1);
      const bind = rows => ({
        depth: depth(rows),
        root: rootNodeId,
        '@edgeCol': edgeColName,
      });
      const visitedNodes = rows => Math.pow(2, depth(rows)+1)-1;

      const genNodeList = (rows, batches) => {
        rows = visitedNodes(rows);
        batches = Math.ceil(rows / defaultBatchSize);
        return [
          {type: SingletonBlock, calls: 1, items: 1},
          {type: MutexBlock, calls: batches, items: rows},
          {type: TraversalBlock, calls: batches, items: rows},
          {type: AsyncBlock, calls: batches, items: rows},
          {type: UnsortingGatherBlock, calls: batches, items: rows},
          {type: ReturnBlock, calls: batches, items: rows}
        ];
      };
      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );
    },
    */

    testLimitCollectCombination: function () {
      const query = `
            FOR x IN 1..@rows
              COLLECT AGGREGATE total = SUM(x)
              LIMIT 0, 1
              RETURN total
      `;
      const prepare = () => {};
      const bind = (rows) => ({rows});
      const genNodeList = (rows, batches) => {
        return [
          {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
          {type: CalculationBlock, calls: 1, items: 1, filtered: 0},
          {type: EnumerateListBlock, calls: batches, items: rows, filtered: 0},
          {type: SortedCollectBlock, calls: 1, items: 1, filtered: 0},
          {type: LimitBlock, calls: 1, items: 1, filtered: 0},
          {type: ReturnBlock, calls: 1, items: 1, filtered: 0}
        ];
      };

      profHelper.runDefaultChecks(
        {query, genNodeList, prepare, bind}
      );
    }

  };
}

 function ahuacatlProfilerViewTestSuite(isSearchAlias) {
   return {
     tearDown: function () {
       db._drop(colName);
       db._drop(edgeColName);
       db._dropView(viewName);
     },

////////////////////////////////////////////////////////////////////////////////
/// @brief test EnumerateViewBlock1
////////////////////////////////////////////////////////////////////////////////

     testEnumerateViewBlock1: function () {
       const col = db._create(colName);
       col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
       let indexMeta = {};
       if (isEnterprise) {
         indexMeta = {type: 'inverted', name: 'inverted', fields: [
             {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
             "value"
         ]};
       } else {
         indexMeta = {type: 'inverted', name: 'inverted', fields: [
             {"name": "value_nested[*]"},
             "value"
         ]};
       }
       col.ensureIndex(indexMeta);
       col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
       let view;
       if (isSearchAlias) {
         let i = col.ensureIndex({type: "inverted", includeAllFields: true});
         view = db._createView(viewName, "search-alias", {indexes: [{collection: colName, index: i.name}]});
       } else {
         let meta = {};
         if (isEnterprise) {
           meta = {links: {[colName]: {includeAllFields: true, "fields": { "nested_value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
         } else {
           meta = {links: {[colName]: {includeAllFields: true}}};
         }
         view = db._createView(viewName, "arangosearch", meta);
       }
       const prepare = (rows) => {
         col.truncate({compact: false});
         col.insert(_.range(1, rows + 1).map((i) => ({value: i, name_1: "name2", "nested_value": [{ "nested_1": [{ "nested_2": `foo${i}`}]}]})));
       };
       const bind = () => ({'@view': viewName});
       const query = `FOR d IN @@view SEARCH d.value != 0 OPTIONS { waitForSync: true } RETURN d.value`;

       const genNodeList = (rows, batches) => {
         // EnumerateViewBlock returns HASMORE when asked for the exact number
         // of items it has left. This could be improved.
         const optimalBatches = Math.ceil(rows / defaultBatchSize);
         const maxViewBatches = Math.floor(rows / defaultBatchSize) + 1;
         const viewBatches = [optimalBatches, maxViewBatches];

         return [
           {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
           {type: EnumerateViewNode, calls: viewBatches, items: rows, filtered: 0},
           {
             type: CalculationBlock,
             calls: rows % defaultBatchSize === 0 ? batches + 1 : batches,
             items: rows,
             filtered: 0
           },
           {type: ReturnBlock, calls: rows % defaultBatchSize === 0 ? batches + 1 : batches, items: rows, filtered: 0}
         ];
       };
       profHelper.runDefaultChecks(
         {query, genNodeList, prepare, bind}
       );

       // collection was truncated. Insert them again
       col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
       col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
       const iiQuery = `FOR d IN ${colName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} 
         FILTER d.value != 0 RETURN d.value`;
       let iiQueryRes = db._query(iiQuery).toArray();
       assertTrue(iiQueryRes.length >= 2); // 2 docs with nested + more from 'prepare' function 
     },

////////////////////////////////////////////////////////////////////////////////
/// @brief test EnumerateViewBlock2
////////////////////////////////////////////////////////////////////////////////

     testEnumerateViewBlock2: function () {
       const col = db._create(colName);
       col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
       let indexMeta = {};
       if (isEnterprise) {
         indexMeta = {type: 'inverted', name: 'inverted', fields: [
             {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
             "value"
         ]};
       } else {
         indexMeta = {type: 'inverted', name: 'inverted', fields: [
             {"name": "value_nested[*]"},
             "value"
         ]};
       }
       col.ensureIndex(indexMeta);
       col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
       let view;
       if (isSearchAlias) {
         let i = col.ensureIndex({type: "inverted", includeAllFields: true});
         view = db._createView(viewName, "search-alias", {indexes: [{collection: colName, index: i.name}]});
       } else {
         let meta = {};
         if (isEnterprise) {
           meta = {links: {[colName]: {includeAllFields: true, "fields": { "nested_value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
         } else {
           meta = {links: {[colName]: {includeAllFields: true}}};
         }
         view = db._createView(viewName, "arangosearch", meta);
       }
       const prepare = (rows) => {
         col.truncate({compact: false});
         col.insert(_.range(1, rows + 1).map((i) => ({value: i, name_1: "name2", "nested_value": [{ "nested_1": [{ "nested_2": `foo${i}`}]}]})));
       };
       const bind = () => ({'@view': viewName});
       const query = `FOR d IN @@view SEARCH d.value != 0 OPTIONS { waitForSync: true } SORT d.value DESC RETURN d.value`;

       const genNodeList = (rows, batches) => {
         // EnumerateViewBlock returns HASMORE when asked for the exact number
         // of items it has left. This could be improved.
         const optimalBatches = Math.ceil(rows / defaultBatchSize);
         const maxViewBatches = Math.floor(rows / defaultBatchSize) + 1;
         const viewBatches = [optimalBatches, maxViewBatches];

         return [
           {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
           {type: EnumerateViewNode, calls: viewBatches, items: rows, filtered: 0},
           {
             type: CalculationBlock,
             calls: rows % defaultBatchSize === 0 ? batches + 1 : batches,
             items: rows,
             filtered: 0
           },
           {type: SortBlock, calls: batches, items: rows, filtered: 0},
           {type: ReturnBlock, calls: batches, items: rows, filtered: 0}
         ];
       };
       profHelper.runDefaultChecks(
         {query, genNodeList, prepare, bind}
       );

      // collection was truncated. Insert them again
      col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
      const iiQuery = `FOR d IN ${colName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER
        d.value != 0 SORT d.value DESC RETURN d.value`;
      let iiQueryRes = db._query(iiQuery).toArray();
      assertTrue(iiQueryRes.length >= 2); // 2 docs with nested + more from 'prepare' function 
     },

////////////////////////////////////////////////////////////////////////////////
/// @brief test EnumerateViewBlock3
////////////////////////////////////////////////////////////////////////////////

     testEnumerateViewBlock3: function () {
       const col = db._create(colName);
       col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
       let indexMeta = {};
       if (isEnterprise) {
         indexMeta = {type: 'inverted', name: 'inverted', fields: [
             {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
             "value"
         ]};
       } else {
         indexMeta = {type: 'inverted', name: 'inverted', fields: [
             {"name": "value_nested[*]"},
             "value"
         ]};
       }
       col.ensureIndex(indexMeta);
       col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
       let view;
       if (isSearchAlias) {
         let i = col.ensureIndex({type: "inverted", includeAllFields: true});
         view = db._createView(viewName, "search-alias", {indexes: [{collection: colName, index: i.name}]});
       } else {
         let meta = {};
         if (isEnterprise) {
           meta = {links: {[colName]: {includeAllFields: true, "fields": { "nested_value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
         } else {
           meta = {links: {[colName]: {includeAllFields: true}}};
         }
         view = db._createView(viewName, "arangosearch", meta);
       }
       const prepare = (rows) => {
         col.truncate({compact: false});
         col.insert(_.range(1, rows + 1).map((i) => ({value: i, name_1: "name2", "nested_value": [{ "nested_1": [{ "nested_2": `foo${i}`}]}]})));
       };
       const bind = () => ({'@view': viewName});
       const query = `FOR d IN @@view SEARCH d.value != 0 OPTIONS { waitForSync: true } SORT TFIDF(d) ASC, BM25(d) RETURN d.value`;

       const genNodeList = (rows, batches) => {
         // EnumerateViewBlock returns HASMORE when asked for the exact number
         // of items it has left. This could be improved.
         const optimalBatches = Math.ceil(rows / defaultBatchSize);
         const maxViewBatches = Math.floor(rows / defaultBatchSize) + 1;
         const viewBatches = [optimalBatches, maxViewBatches];

         return [
           {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
           {type: EnumerateViewNode, calls: viewBatches, items: rows, filtered: 0},
           {type: SortBlock, calls: batches, items: rows, filtered: 0},
           {type: CalculationBlock, calls: batches, items: rows, filtered: 0},
           {type: ReturnBlock, calls: batches, items: rows, filtered: 0}
         ];
       };

       profHelper.runDefaultChecks(
         {query, genNodeList, prepare, bind}
       );

       // collection was truncated. Insert them again
       col.insert({ name_1: "foo", "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
       col.insert({ name_1: "bar", "value_nested": [{ "nested_1": [{ "nested_2": "foo321"}]}]});
       const iiQuery = `FOR d IN ${colName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true}  FILTER
         d.value != 0 SORT d.other RETURN d.value`;;
       let iiQueryRes = db._query(iiQuery).toArray();
       assertTrue(iiQueryRes.length >= 2); // 2 docs with nested + more from 'prepare' function 
     },

     testLimitCollectCombination: function () {
       const query = `
            FOR x IN 1..@rows
              COLLECT AGGREGATE total = SUM(x)
              LIMIT 0, 1
              RETURN total
      `;
       const prepare = () => {
       };
       const bind = (rows) => ({rows});
       const genNodeList = (rows, batches) => {
         return [
           {type: SingletonBlock, calls: 1, items: 1, filtered: 0},
           {type: CalculationBlock, calls: 1, items: 1, filtered: 0},
           {type: EnumerateListBlock, calls: batches, items: rows, filtered: 0},
           {type: SortedCollectBlock, calls: 1, items: 1, filtered: 0},
           {type: LimitBlock, calls: 1, items: 1, filtered: 0},
           {type: ReturnBlock, calls: 1, items: 1, filtered: 0}
         ];
       };

       profHelper.runDefaultChecks(
         {query, genNodeList, prepare, bind}
       );
     }

   };
 }

 ////////////////////////////////////////////////////////////////////////////////
 /// @brief executes the test suite
 ////////////////////////////////////////////////////////////////////////////////

 jsunity.run(ahuacatlProfilerTestSuite);

 function ahuacatlProfilerArangoSearchTestSuite() {
   let suite = {};
   deriveTestSuite(
     ahuacatlProfilerViewTestSuite(false),
     suite,
     "_arangosearch"
   );
   return suite;
 }

 function ahuacatlProfilerSearchAliasTestSuite() {
   let suite = {};
   deriveTestSuite(
     ahuacatlProfilerViewTestSuite(true),
     suite,
     "_search-alias"
   );
   return suite;
 }

 jsunity.run(ahuacatlProfilerArangoSearchTestSuite);
 jsunity.run(ahuacatlProfilerSearchAliasTestSuite);

 return jsunity.done();
