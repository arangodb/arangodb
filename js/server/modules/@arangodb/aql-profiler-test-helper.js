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

const _ = require('lodash');
const db = require('@arangodb').db;
const expect = require('chai').expect;
const jsunity = require("jsunity");
const assert = jsunity.jsUnity.assertions;



////////////////////////////////////////////////////////////////////////////////
/// @file common variables and functions for aql-profiler* tests
////////////////////////////////////////////////////////////////////////////////


const colName = 'UnitTestProfilerCol';
const edgeColName = 'UnitTestProfilerEdgeCol';
const viewName = 'UnitTestProfilerView';

const defaultBatchSize = 1000;

const defaultTestRowCounts = [1, 2, 10, 100, 999, 1000, 1001, 1500, 2000, 10500];

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

const CalculationBlock = 'CalculationNode';
const CountCollectBlock = 'CountCollectNode';
const DistinctCollectBlock = 'DistinctCollectNode';
const EnumerateCollectionBlock = 'EnumerateCollectionNode';
const EnumerateListBlock = 'EnumerateListNode';
const FilterBlock = 'FilterNode';
const HashedCollectBlock = 'HashedCollectNode';
const IndexBlock = 'IndexNode';
const LimitBlock = 'LimitNode';
const NoResultsBlock = 'NoResultsNode';
const RemoteBlock = 'RemoteNode';
const ReturnBlock = 'ReturnNode';
const ShortestPathBlock = 'ShortestPathNode';
const SingletonBlock = 'SingletonNode';
const SortBlock = 'SortNode';
const SortedCollectBlock = 'SortedCollectNode';
const SortingGatherBlock = 'SortingGatherNode';
const SubqueryBlock = 'SubqueryNode';
const TraversalBlock = 'TraversalNode';
const UnsortingGatherBlock = 'UnsortingGatherNode';
const RemoveBlock = 'RemoveNode';
const InsertBlock = 'InsertNode';
const UpdateBlock = 'UpdateNode';
const ReplaceBlock = 'ReplaceNode';
const UpsertBlock = 'UpsertNode';
const ScatterBlock = 'ScatterNode';
const DistributeBlock = 'DistributeNode';
const IResearchViewUnorderedBlock = 'IResearchUnorderedViewNode';
const IResearchViewBlock = 'IResearchViewNode';
const IResearchViewOrderedBlock = 'IResearchOrderedViewNode';

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

let translateType = function(nodes, node) {
  let types = {};
  nodes.forEach(function(node) { 
    let type = node.type;
    if (type === 'CollectNode') {
      if (node.collectOptions.method === 'sorted') {
        type = 'SortedCollectNode';
      } else if (node.collectOptions.method === 'hash') {
        type = 'HashedCollectNode';
      } else if (node.collectOptions.method === 'distinct') {
        type = 'DistinctCollectNode';
      } else if (node.collectOptions.method === 'count') {
        type = 'CountCollectNode';
      }
    } else if (node.type === 'GatherNode') {
      if (node.sortmode === 'minelement' || node.sortmode === 'heap') {
        type = 'SortingGatherNode';
      } else {
        type = 'UnsortingGatherNode';
      }
    }
    types[node.id] = type;
  });

  return types[node.id];
};

/// @brief check that numbers in actual are in the range specified by
/// expected. Each element in expected may either be
///  - a number, for an exact match;
///  - a range [min, max], to check if the actual element lies in this interval;
///  - null, so the actual element will be ignored
///  - undefined must be undefined
/// Also, the arrays must be of equal lengths.
function assertFuzzyNumArrayEquality(expected, actual, details) {
  assert.assertEqual(expected.length, actual.length, details);

  for (let i = 0; i < expected.length; i++) {
    const exp = expected[i];
    const act = actual[i];

    if (exp === null) {
      // do nothing
    } else if ('number' === typeof exp) {
      assert.assertEqual(exp, act, Object.assign({i}, details));
    } else if ('undefined' === typeof exp) {
      assert.assertEqual(exp, act, Object.assign({i}, details));
    } else if (Array.isArray(exp)) {
      assert.assertTrue(exp[0] <= act && act <= exp[1],
        Object.assign(
          {i, 'failed_test': `${exp[0]} <= ${act} <= ${exp[1]}`},
          details
        )
      );
    } else {
      assert.assertTrue(false, {msg: "Logical error in the test code", exp});
    }
  }
}


function zipPlanNodesIntoStatsNodes (profile) {
  const statsNodesById = profile.stats.nodes.reduce(
    (accum, node) => {
      accum[node.id] = node;
      return accum;
    },
    {}
  );
  
  // Note: We need to take the order plan.nodes here, not stats.nodes,
  // as stats.nodes is sorted by id.
  return profile.plan.nodes.map(node => (
    { 
      id: node.id, 
      type: translateType(profile.plan.nodes, node), 
      fromStats: statsNodesById[node.id], fromPlan: node 
    }
  ));
}

function getCompactStatsNodes (profile) {
  // While we don't use any .fromPlan info here, zip uses the (correct) order
  // of the plan, not from the stats (which is sorted by id).
  return zipPlanNodesIntoStatsNodes(profile).map(
    node => ({
      type: translateType(profile.plan.nodes, node),
      calls: node.fromStats.calls,
      items: node.fromStats.items,
    })
  );
}

function getPlanNodesWithId (profile) {
  return profile.plan.nodes.map(
    node => ({
      id: node.id,
      type: node.type,
    })
  );
}

function getStatsNodesWithId (profile) {
  return profile.stats.nodes.map(
    node => ({
      id: node.id,
      type: node.type,
    })
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert structure of profile.stats
////////////////////////////////////////////////////////////////////////////////

function assertIsProfileStatsObject (stats, {level}) {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert structure of profile.warnings
////////////////////////////////////////////////////////////////////////////////

function assertIsProfileWarningsArray (warnings) {
  expect(warnings).to.be.an('array');

  // TODO check element type
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert structure of profile.profile
////////////////////////////////////////////////////////////////////////////////

function assertIsProfileProfileObject (profile) {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert structure of profile.plan
////////////////////////////////////////////////////////////////////////////////

function assertIsProfilePlanObject (plan) {
  expect(plan).to.have.all.keys([
    'nodes',
    'rules',
    'collections',
    'variables',
    'estimatedCost',
    'estimatedNrItems',
    'initialize',
    'isModificationQuery',
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

}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that the passed variable looks like a level 0 profile
////////////////////////////////////////////////////////////////////////////////

function assertIsLevel0Profile (profile) {
  expect(profile)
    .to.be.an('object')
    .that.has.all.keys([
    'stats',
    'warnings',
  ]);

  assertIsProfileStatsObject(profile.stats, {level: 0});
  assertIsProfileWarningsArray(profile.warnings);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that the passed variable looks like a level 1 profile
////////////////////////////////////////////////////////////////////////////////

function assertIsLevel1Profile (profile) {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that the passed variable looks like a level 2 profile
////////////////////////////////////////////////////////////////////////////////

function assertIsLevel2Profile (profile) {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that the list of AQL nodes in the explain result matches the
/// list of AQL nodes of the profile.
////////////////////////////////////////////////////////////////////////////////

function assertStatsNodesMatchPlanNodes (profile) {
  // Note: reorderings in the plan would break this comparison, because the
  // stats are ordered by id. Thus we sort the nodes here.
  assert.assertEqual(
    profile.plan.nodes.map(node => node.id).sort(),
    profile.stats.nodes.map(node => node.id).sort(),
    {
      'profile.plan.nodes': getPlanNodesWithId(profile),
      'profile.stats.nodes': getStatsNodesWithId(profile),
    }
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Compares lists of nodes with items and calls, i.e., expected and
/// actual both have the structure [ { type, calls, items } ].
/// details may contain an object that will be output when the test fails,
/// maybe with additional fields.
/// .calls and .items may be either a number for an exact test, or a range
/// [min, max] which tests for min <= actualCalls <= max instead, or null to
/// ignore the value.
////////////////////////////////////////////////////////////////////////////////

function assertNodesItemsAndCalls (expected, actual, details = {}) {
  // assert node types first
  assert.assertEqual(
    expected.map(node => node.type),
    actual.map(node => node.type),
    details
  );

  // assert item count second
  assertFuzzyNumArrayEquality(
    expected.map(node => node.items),
    actual.map(node => node.items),
    details
  );

  // assert call count last.
  assertFuzzyNumArrayEquality(
    expected.map(node => node.calls),
    actual.map(node => node.calls),
    details
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Common checks for most blocks
/// @param query string - is assumed to have one bind parameter 'rows'
/// @param genNodeList function: (rows, batches) => [ { type, calls, items } ]
///        must generate the list of expected nodes
/// @param prepare function: (rows) => {...}
///        called before the query is executed
/// @param bind function: (rows) => ({rows})
///        must return the bind parameters for the query
/// Example for genNodeList:
/// genNodeList(2500, 3) ===
/// [
///   { type : SingletonNode, calls : 1, items : 1 },
///   { type : EnumerateListNode, calls : 3, items : 2500 },
///   { type : ReturnNode, calls : 3, items : 2500 }
/// ]
/// The number of calls may be a range [min, max], e.g.:
///   { type : EnumerateCollectionNode, calls : [3,5] , items : 2500 }
////////////////////////////////////////////////////////////////////////////////

function runDefaultChecks (
  {
    query,
    genNodeList,
    prepare = () => {},
    bind = rows => ({rows}),
    options = {},
  }
) {
  for (const rows of defaultTestRowCounts) {
    prepare(rows);
    const profile = db._query(query, bind(rows),
      _.merge(options, {profile: 2, defaultBatchSize})
    ).getExtra();

    assertIsLevel2Profile(profile);
    assertStatsNodesMatchPlanNodes(profile);

    const batches = Math.ceil(rows / defaultBatchSize);

    const expected = genNodeList(rows, batches);
    const actual = getCompactStatsNodes(profile);

    assertNodesItemsAndCalls(expected, actual,
     {query, rows, batches, expected, actual});
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Fill the passed collections with a balanced binary tree.
///        All existing documents will be deleted in both collections!
/// @param vertexCol A document ArangoCollection
/// @param edgeCol An edge ArangoCollection
/// @param numVertices Number of vertices the binary tree should contain
////////////////////////////////////////////////////////////////////////////////
function createBinaryTree (vertexCol, edgeCol, numVertices) {
  // clear collections
  vertexCol.truncate();
  edgeCol.truncate();

  // add vertices
  vertexCol.insert(_.range(1, numVertices + 1).map((i) => ({_key: ""+i})));

  // create a balanced binary tree on edgeCol
  edgeCol.insert(
    // for every v._key from col
    _.range(1, numVertices + 1)
    // calculate child indices of (potential) children
      .map(i => [{from: i, to: 2*i}, {from: i, to: 2*i+1}])
      // flatten
      .reduce((accum, cur) => accum.concat(cur), [])
      // omit edges to non-existent vertices
      .filter(e => e.to <= numVertices)
      // create edge documents
      .map(e => (
        {
          _from: `${colName}/${e.from}`,
          _to: `${colName}/${e.to}`,
        }
      ))
  );
}

exports.colName = colName;
exports.viewName = viewName;
exports.edgeColName = edgeColName;
exports.defaultBatchSize = defaultBatchSize;
exports.defaultTestRowCounts = defaultTestRowCounts;
exports.CalculationNode = CalculationNode;
exports.CollectNode = CollectNode;
exports.DistributeNode = DistributeNode;
exports.EnumerateCollectionNode = EnumerateCollectionNode;
exports.EnumerateListNode = EnumerateListNode;
exports.EnumerateViewNode = EnumerateViewNode;
exports.FilterNode = FilterNode;
exports.GatherNode = GatherNode;
exports.IndexNode = IndexNode;
exports.InsertNode = InsertNode;
exports.LimitNode = LimitNode;
exports.NoResultsNode = NoResultsNode;
exports.RemoteNode = RemoteNode;
exports.RemoveNode = RemoveNode;
exports.ReplaceNode = ReplaceNode;
exports.ReturnNode = ReturnNode;
exports.ScatterNode = ScatterNode;
exports.ShortestPathNode = ShortestPathNode;
exports.SingletonNode = SingletonNode;
exports.SortNode = SortNode;
exports.SubqueryNode = SubqueryNode;
exports.TraversalNode = TraversalNode;
exports.UpdateNode = UpdateNode;
exports.UpsertNode = UpsertNode;
exports.nodeTypesList = nodeTypesList;
exports.CalculationBlock = CalculationBlock;
exports.CountCollectBlock = CountCollectBlock;
exports.DistinctCollectBlock = DistinctCollectBlock;
exports.EnumerateCollectionBlock = EnumerateCollectionBlock;
exports.EnumerateListBlock = EnumerateListBlock;
exports.FilterBlock = FilterBlock;
exports.HashedCollectBlock = HashedCollectBlock;
exports.IndexBlock = IndexBlock;
exports.LimitBlock = LimitBlock;
exports.NoResultsBlock = NoResultsBlock;
exports.RemoteBlock = RemoteBlock;
exports.ReturnBlock = ReturnBlock;
exports.ShortestPathBlock = ShortestPathBlock;
exports.SingletonBlock = SingletonBlock;
exports.SortBlock = SortBlock;
exports.SortedCollectBlock = SortedCollectBlock;
exports.SortingGatherBlock = SortingGatherBlock;
exports.SubqueryBlock = SubqueryBlock;
exports.TraversalBlock = TraversalBlock;
exports.UnsortingGatherBlock = UnsortingGatherBlock;
exports.RemoveBlock = RemoveBlock;
exports.InsertBlock = InsertBlock;
exports.UpdateBlock = UpdateBlock;
exports.ReplaceBlock = ReplaceBlock;
exports.UpsertBlock = UpsertBlock;
exports.ScatterBlock = ScatterBlock;
exports.DistributeBlock = DistributeBlock;
exports.IResearchViewUnorderedBlock = IResearchViewUnorderedBlock;
exports.IResearchViewBlock = IResearchViewBlock;
exports.IResearchViewOrderedBlock = IResearchViewOrderedBlock;
exports.blockTypesList = blockTypesList;
exports.assertFuzzyNumArrayEquality = assertFuzzyNumArrayEquality;
exports.zipPlanNodesIntoStatsNodes = zipPlanNodesIntoStatsNodes;
exports.getCompactStatsNodes = getCompactStatsNodes;
exports.getPlanNodesWithId = getPlanNodesWithId;
exports.getStatsNodesWithId = getStatsNodesWithId;
exports.assertIsProfileStatsObject = assertIsProfileStatsObject;
exports.assertIsProfileWarningsArray = assertIsProfileWarningsArray;
exports.assertIsProfileProfileObject = assertIsProfileProfileObject;
exports.assertIsProfilePlanObject = assertIsProfilePlanObject;
exports.assertIsLevel0Profile = assertIsLevel0Profile;
exports.assertIsLevel1Profile = assertIsLevel1Profile;
exports.assertIsLevel2Profile = assertIsLevel2Profile;
exports.assertStatsNodesMatchPlanNodes = assertStatsNodesMatchPlanNodes;
exports.assertNodesItemsAndCalls = assertNodesItemsAndCalls;
exports.runDefaultChecks = runDefaultChecks;
exports.createBinaryTree = createBinaryTree;
