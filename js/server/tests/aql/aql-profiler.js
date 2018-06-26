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

const _ = require('lodash');
const db = require('@arangodb').db;
const expect = require('chai').expect;
const helper = require('@arangodb/aql-helper');
const internal = require('internal');
const console = require('console');
const jsunity = require("jsunity");
const assert = jsunity.jsUnity.assertions;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for AQL tracing/profiling
////////////////////////////////////////////////////////////////////////////////

function ahuacatlProfilerTestSuite () {

  const colName = 'UnitTestProfilerCol';

  const batchSize = 1000;

  const testRowCounts = [1, 2, 10, 100, 999, 1000, 1001, 1500, 2000, 10500];

  const CalculationNode = 'CalculationNode';
  const CollectNode = 'CollectNode';
  const DistributeNode = 'DistributeNode';
  const EnumerateCollectionNode = 'EnumerateCollectionNode';
  const EnumerateListNode = 'EnumerateListNode';
  const EnumerateViewNode = 'EnumerateViewNode';
  const FilterNode = 'FilterNode';
  const GatherNode = 'GatherNode';
  const IndexNode = 'IndexNode';
  const InsertNode = 'InsertNode';
  const LimitNode = 'LimitNode';
  const NoResultsNode = 'NoResultsNode';
  const RemoteNode = 'RemoteNode';
  const RemoveNode = 'RemoveNode';
  const ReplaceNode = 'ReplaceNode';
  const ReturnNode = 'ReturnNode';
  const ScatterNode = 'ScatterNode';
  const ShortestPathNode = 'ShortestPathNode';
  const SingletonNode = 'SingletonNode';
  const SortNode = 'SortNode';
  const SubqueryNode = 'SubqueryNode';
  const TraversalNode = 'TraversalNode';
  const UpdateNode = 'UpdateNode';
  const UpsertNode = 'UpsertNode';

  const nodeTypesList = [
    CalculationNode, CollectNode, DistributeNode, EnumerateCollectionNode,
    EnumerateListNode, EnumerateViewNode, FilterNode, GatherNode, IndexNode,
    InsertNode, LimitNode, NoResultsNode, RemoteNode, RemoveNode, ReplaceNode,
    ReturnNode, ScatterNode, ShortestPathNode, SingletonNode, SortNode,
    SubqueryNode, TraversalNode, UpdateNode, UpsertNode
  ];

  const CalculationBlock = 'CalculationBlock';
  const CountCollectBlock = 'CountCollectBlock';
  const DistinctCollectBlock = 'DistinctCollectBlock';
  const EnumerateCollectionBlock = 'EnumerateCollectionBlock';
  const EnumerateListBlock = 'EnumerateListBlock';
  const FilterBlock = 'FilterBlock';
  const HashedCollectBlock = 'HashedCollectBlock';
  const IndexBlock = 'IndexBlock';
  const LimitBlock = 'LimitBlock';
  const NoResultsBlock = 'NoResultsBlock';
  const RemoteBlock = 'RemoteBlock';
  const ReturnBlock = 'ReturnBlock';
  const ShortestPathBlock = 'ShortestPathBlock';
  const SingletonBlock = 'SingletonBlock';
  const SortBlock = 'SortBlock';
  const SortedCollectBlock = 'SortedCollectBlock';
  const SortingGatherBlock = 'SortingGatherBlock';
  const SubqueryBlock = 'SubqueryBlock';
  const TraversalBlock = 'TraversalBlock';
  const UnsortingGatherBlock = 'UnsortingGatherBlock';
  const RemoveBlock = 'RemoveBlock';
  const InsertBlock = 'InsertBlock';
  const UpdateBlock = 'UpdateBlock';
  const ReplaceBlock = 'ReplaceBlock';
  const UpsertBlock = 'UpsertBlock';
  const ScatterBlock = 'ScatterBlock';
  const DistributeBlock = 'DistributeBlock';
  const IResearchViewUnorderedBlock = 'IResearchViewUnorderedBlock';
  const IResearchViewBlock = 'IResearchViewBlock';
  const IResearchViewOrderedBlock = 'IResearchViewOrderedBlock';

  const blockTypesList = [
    CalculationBlock, CountCollectBlock, DistinctCollectBlock,
    EnumerateCollectionBlock, EnumerateListBlock, FilterBlock,
    HashedCollectBlock, IndexBlock, LimitBlock, NoResultsBlock, RemoteBlock,
    ReturnBlock, ShortestPathBlock, SingletonBlock, SortBlock,
    SortedCollectBlock, SortingGatherBlock, SubqueryBlock, TraversalBlock,
    UnsortingGatherBlock, RemoveBlock, InsertBlock, UpdateBlock, ReplaceBlock,
    UpsertBlock, ScatterBlock, DistributeBlock, IResearchViewUnorderedBlock,
    IResearchViewBlock, IResearchViewOrderedBlock
  ];

  const getPlanNodeTypes = function (result) {
    return helper.getCompactPlan(result).map(function (node) {
      return node.type;
    });
  };

  const zipPlanNodesIntoStatsNodes = function (profile) {
    const planNodesById = profile.plan.nodes.reduce(
      (accum, node) => {
        accum[node.id] = node;
        return accum;
      },
      {}
    );

    return profile.stats.nodes.map(node => (
      { id: node.id, fromStats: node, fromPlan: planNodesById[node.id] }
    ));
  };

  const getStatsNodeTypes = function (profile) {
    return zipPlanNodesIntoStatsNodes(profile).map(
      node => node.fromPlan.type
    );
  };

  const getCompactStatsNodes = function (profile) {
    // TODO as long as we don't need the node(!) type anymore, we could map the
    // stats directly (without zipping the plan nodes)
    return zipPlanNodesIntoStatsNodes(profile).map(
      node => ({
        // type: node.fromPlan.type,
        type: node.fromStats.blockType,
        calls: node.fromStats.calls,
        items: node.fromStats.items,
      })
    );
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief assert structure of profile.stats
////////////////////////////////////////////////////////////////////////////////

  const assertIsProfileStatsObject = function (stats, {level}) {
    // internal argument check
    expect(level)
      .to.be.a('number')
      .and.to.be.oneOf([0, 1, 2]);

    expect(stats).to.be.an('object');

    let statsKeys = [
      'writesExecuted',
      'writesIgnored',
      'scannedFull',
      'scannedIndex',
      'filtered',
      'httpRequests',
      'executionTime',
    ];

    if (level === 2) {
      statsKeys.push('nodes');
    }

    expect(stats).to.have.all.keys(statsKeys);

    // check types
    expect(stats.writesExecuted).to.be.a('number');
    expect(stats.writesIgnored).to.be.a('number');
    expect(stats.scannedFull).to.be.a('number');
    expect(stats.scannedIndex).to.be.a('number');
    expect(stats.filtered).to.be.a('number');
    expect(stats.httpRequests).to.be.a('number');
    expect(stats.executionTime).to.be.a('number');
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief assert structure of profile.warnings
////////////////////////////////////////////////////////////////////////////////

  const assertIsProfileWarningsArray = function (warnings) {
    expect(warnings).to.be.an('array');

    // TODO check element type
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief assert structure of profile.profile
////////////////////////////////////////////////////////////////////////////////

  const assertIsProfileProfileObject = function (profile) {
    expect(profile).to.have.all.keys([
      'initializing',
      'parsing',
      'optimizing ast',
      'loading collections',
      'instantiating plan',
      'optimizing plan',
      'executing',
      'finalizing',
    ]);

    expect(profile.initializing).to.be.a('number');
    expect(profile.parsing).to.be.a('number');
    expect(profile['optimizing ast']).to.be.a('number');
    expect(profile['loading collections']).to.be.a('number');
    expect(profile['instantiating plan']).to.be.a('number');
    expect(profile['optimizing plan']).to.be.a('number');
    expect(profile.executing).to.be.a('number');
    expect(profile.finalizing).to.be.a('number');
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief assert structure of profile.plan
////////////////////////////////////////////////////////////////////////////////

  const assertIsProfilePlanObject = function (plan) {
    expect(plan).to.have.all.keys([
      'nodes',
      'rules',
      'collections',
      'variables',
      'estimatedCost',
      'estimatedNrItems',
      'initialize',
    ]);

    expect(plan.nodes).to.be.an('array');
    expect(plan.rules).to.be.an('array');
    expect(plan.collections).to.be.an('array');
    expect(plan.variables).to.be.an('array');
    expect(plan.estimatedCost).to.be.a('number');
    expect(plan.estimatedNrItems).to.be.a('number');
    expect(plan.initialize).to.be.a('boolean');

    for (let node of plan.nodes) {
      expect(node).to.include.all.keys([
        'type',
        'dependencies',
        'id',
        'estimatedCost',
        'estimatedNrItems',
      ]);

      expect(node.id).to.be.a('number');
      expect(node.estimatedCost).to.be.a('number');
      expect(node.estimatedNrItems).to.be.a('number');

      expect(node.type)
        .to.be.a('string')
        .and.to.be.oneOf(nodeTypesList);

      expect(node.dependencies)
        .to.be.an('array');
      // TODO add deep checks for plan.nodes[].dependencies

      // TODO add checks for the optional variables, maybe dependent on the type
      // e.g. for expression, inVariable, outVariable, canThrow, expressionType
      // or elements...
    }

    // TODO add deep checks for plan.rules
    // TODO add deep checks for plan.collections

    for (let variable of plan.variables) {
      expect(variable).to.have.all.keys([
        'id',
        'name',
      ]);

      expect(variable.id).to.be.a('number');
      expect(variable.name).to.be.a('string');
    }

  };

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that the passed variable looks like a level 0 profile
////////////////////////////////////////////////////////////////////////////////

  const assertIsLevel0Profile = function (profile) {
    expect(profile)
      .to.be.an('object')
      .that.has.all.keys([
        'stats',
        'warnings',
      ]);

    assertIsProfileStatsObject(profile.stats, {level: 0});
    assertIsProfileWarningsArray(profile.warnings);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that the passed variable looks like a level 1 profile
////////////////////////////////////////////////////////////////////////////////

  const assertIsLevel1Profile = function (profile) {
    expect(profile)
      .to.be.an('object')
      .that.has.all.keys([
        'stats',
        'warnings',
        'profile',
      ]);

    assertIsProfileStatsObject(profile.stats, {level: 1});
    assertIsProfileWarningsArray(profile.warnings);
    assertIsProfileProfileObject(profile.profile);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that the passed variable looks like a level 2 profile
////////////////////////////////////////////////////////////////////////////////

  const assertIsLevel2Profile = function (profile) {
    expect(profile)
      .to.be.an('object')
      .that.has.all.keys([
        'stats',
        'warnings',
        'profile',
        'plan',
      ]);

    assertIsProfileStatsObject(profile.stats, {level: 2});
    assertIsProfileWarningsArray(profile.warnings);
    assertIsProfileProfileObject(profile.profile);
    assertIsProfilePlanObject(profile.plan);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that the list of AQL nodes in the explain result matches the
/// list of AQL nodes of the profile.
////////////////////////////////////////////////////////////////////////////////

  const assertStatsNodesMatchPlanNodes = function (profile) {
    assert.assertEqual(
      profile.plan.nodes.map(node => node.id),
      profile.stats.nodes.map(node => node.id)
    );
  };

  // @brief Common checks for most blocks
  // @param query string - is assumed to have one bind parameter 'rows'
  // @param genNodeList function: (rows, batches) => [ { type, calls, items } ]
  //        must generate the list of expected nodes
  // @param prepare function: (rows) => {...}
  //        called before the query is executed
  // @param bind function: (rows) => ({rows})
  //        must return the bind parameters for the query
  // Example for genNodeList:
  // genNodeList(2500, 3) ===
  // [
  //   { type : SingletonNode, calls : 1, items : 1 },
  //   { type : EnumerateListNode, calls : 3, items : 2500 },
  //   { type : ReturnNode, calls : 3, items : 2500 }
  // ]
  const runDefaultChecks = function (
    query,
    genNodeList,
    prepare = () => {},
    bind = (rows) => ({rows})
  ) {
    for (const rows of testRowCounts) {
      prepare(rows);
      const profile = db._query(query, bind(rows), {profile: 2}).getExtra();

      assertIsLevel2Profile(profile);
      assertStatsNodesMatchPlanNodes(profile);

      const batches = Math.ceil(rows / batchSize);

      assert.assertEqual(
        genNodeList(rows, batches),
        getCompactStatsNodes(profile),
        {query,rows,batches}
      );
    }
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
      db._drop(colName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test {profile: 0}
////////////////////////////////////////////////////////////////////////////////

    testProfile0Fields : function () {
      const query = 'RETURN 1';
      const profileDefault = db._query(query, {}).getExtra();
      const profile0 = db._query(query, {}, {profile: 0}).getExtra();
      const profileFalse = db._query(query, {}, {profile: false}).getExtra();

      assertIsLevel0Profile(profileDefault);
      assertIsLevel0Profile(profile0);
      assertIsLevel0Profile(profileFalse);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test {profile: 1}
////////////////////////////////////////////////////////////////////////////////

    testProfile1Fields : function () {
      const query = 'RETURN 1';
      const profile1 = db._query(query, {}, {profile: 1}).getExtra();
      const profileTrue = db._query(query, {}, {profile: true}).getExtra();

      assertIsLevel1Profile(profile1);
      assertIsLevel1Profile(profileTrue);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test {profile: 2}
////////////////////////////////////////////////////////////////////////////////

    testProfile2Fields : function () {
      const query = 'RETURN 1';
      const profile2 = db._query(query, {}, {profile: 2}).getExtra();

      assertIsLevel2Profile(profile2);
      assertStatsNodesMatchPlanNodes(profile2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief minimal stats test
////////////////////////////////////////////////////////////////////////////////

    testStatsMinimal : function () {
      const query = 'RETURN 1';
      const profile = db._query(query, {}, {profile: 2}).getExtra();

      assertIsLevel2Profile(profile);
      assertStatsNodesMatchPlanNodes(profile);

      assert.assertEqual(
        [
          { type : SingletonBlock, calls : 1, items : 1 },
          { type : CalculationBlock, calls : 1, items : 1 },
          { type : ReturnBlock, calls : 1, items : 1 }
        ],
        getCompactStatsNodes(profile)
      );
    },

    // EnumerateListBlock
    testEnumerateListBlock : function () {
      const query = 'FOR i IN 1..@rows RETURN i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : ReturnBlock, calls : batches, items : rows }
      ];
      runDefaultChecks(query, genNodeList);
    },

    // CalculationBlock
    testCalculationBlock : function () {
      const query = 'FOR i IN 1..@rows RETURN i*i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : CalculationBlock, calls : batches, items : rows },
        { type : ReturnBlock, calls : batches, items : rows }
      ];
      runDefaultChecks(query, genNodeList);
    },

    // CountCollectBlock
    testCountCollectBlock : function () {
      const query = 'FOR i IN 1..@rows COLLECT WITH COUNT INTO c RETURN c';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : CountCollectBlock, calls : 1, items : 1 },
        { type : ReturnBlock, calls : 1, items : 1 }
      ];
      runDefaultChecks(query, genNodeList);
    },

    // DistinctCollectBlock
    testDistinctCollectBlock1 : function () {
      const query = 'FOR i IN 1..@rows RETURN DISTINCT i';
      const genNodeList = (rows, batches) => [
        { type : SingletonBlock, calls : 1, items : 1 },
        { type : CalculationBlock, calls : 1, items : 1 },
        { type : EnumerateListBlock, calls : batches, items : rows },
        { type : DistinctCollectBlock, calls : batches, items : rows },
        { type : ReturnBlock, calls : batches, items : rows }
      ];
      runDefaultChecks(query, genNodeList);
    },

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
      runDefaultChecks(query, genNodeList);
    },

    // EnumerateCollectionBlock
    testEnumerateCollectionBlock1 : function () {
      const col = db._create(colName);
      const prepare = (rows) => {
        col.truncate();
        col.insert(_.range(1, rows + 1).map((i) => ({value: i})));
      };
      const bind = () => ({'@col': colName});
      const query = `FOR d IN @@col RETURN d.value`;
      const genNodeList = (rows, batches) => [
        {type: SingletonBlock, calls: 1, items: 1},
        {type: EnumerateCollectionBlock, calls: batches, items: rows},
        {type: CalculationBlock, calls: batches, items: rows},
        {type: ReturnBlock, calls: batches, items: rows}
      ];
      runDefaultChecks(query, genNodeList, prepare, bind);
    }

// TODO Every block must be tested separately. Here follows the list of blocks
// (partly grouped due to the inheritance hierarchy). Intermediate blocks
// like ModificationBlock and BlockWithClients are never instantiated separately
// and therefore don't need to be tested on their own.

// *CalculationBlock
// *CountCollectBlock
// *DistinctCollectBlock
// EnumerateCollectionBlock
// *EnumerateListBlock
// FilterBlock
// HashedCollectBlock
// IndexBlock
// LimitBlock
// NoResultsBlock
// RemoteBlock
// ReturnBlock
// ShortestPathBlock
// SingletonBlock
// SortBlock
// SortedCollectBlock
// SortingGatherBlock
// SubqueryBlock
// TraversalBlock
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
