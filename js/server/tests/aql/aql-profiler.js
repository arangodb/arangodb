/*jshint globalstrict:true, strict:true, esnext: true */
/*global assertEqual */

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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlProfilerTestSuite () {

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
        .and.to.be.oneOf([
          "CalculationNode", "CollectNode", "DistributeNode",
          "EnumerateCollectionNode", "EnumerateListNode", "EnumerateViewNode",
          "FilterNode", "GatherNode", "IndexNode", "InsertNode", "LimitNode",
          "NoResultsNode", "RemoteNode", "RemoveNode", "ReplaceNode",
          "ReturnNode", "ScatterNode", "ShortestPathNode", "SingletonNode",
          "SortNode", "SubqueryNode", "TraversalNode", "UpdateNode",
          "UpsertNode",
        ]);

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
      const profileDefault = db._query('RETURN 1', {}).getExtra();
      const profile0 = db._query('RETURN 1', {}, {profile: 0}).getExtra();
      const profileFalse = db._query('RETURN 1', {}, {profile: false}).getExtra();

      assertIsLevel0Profile(profileDefault);
      assertIsLevel0Profile(profile0);
      assertIsLevel0Profile(profileFalse);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test {profile: 1}
////////////////////////////////////////////////////////////////////////////////

    testProfile1Fields : function () {
      const profile1 = db._query('RETURN 1', {}, {profile: 1}).getExtra();
      const profileTrue = db._query('RETURN 1', {}, {profile: true}).getExtra();

      assertIsLevel1Profile(profile1);
      assertIsLevel1Profile(profileTrue);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test {profile: 2}
////////////////////////////////////////////////////////////////////////////////

    testProfile2Fields : function () {
      const profile2 = db._query('RETURN 1', {}, {profile: 2}).getExtra();

      assertIsLevel2Profile(profile2);
    },


  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlProfilerTestSuite);

return jsunity.done();
