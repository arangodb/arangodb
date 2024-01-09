/*jshint strict: true */
/*global assertTrue, assertEqual, print*/
"use strict";
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const lh = require("@arangodb/testutils/replicated-logs-helper");
const ch = require("@arangodb/testutils/collection-groups-helper");

const database = 'CollectionGroupsSupervisionTest';

const {setUpAll, tearDownAll, setUp, tearDown} = lh.testHelperFunctions(database, {replicationVersion: "2"});

const collectionGroupsSupervisionSuite = function () {
  return {
    setUpAll, tearDownAll, setUp, tearDown,

    testCreateCollectionGroup: function () {
      ch.createCollectionGroupTarget(database, {
        replicationFactor: 3,
        numberOfShards: 3,
      });
    },

    testAddCollection: function () {
      const {gid} = ch.createCollectionGroupTarget(database, {
        replicationFactor: 3,
        numberOfShards: 3,
      });

      const {cid} = ch.addCollectionToGroup(database, gid);
      lh.waitFor(ch.collectionGroupIsReady(database, gid));

      {
        const {plan} = ch.readCollectionGroup(database, gid);
        // check that collection was added to plan
        assertTrue(plan.collections[cid] !== undefined);
      }
      {
        const {plan} = ch.readCollection(database, cid);
        assertTrue(plan !== undefined);
        assertEqual(plan.groupId, gid);
      }
    },

    testRemoveCollection: function () {
      const {gid, cid} = ch.createCollectionGroupTarget(database, {
        replicationFactor: 3,
        numberOfShards: 3,
        numberOfCollections: 2,
      });

      ch.dropCollectionFromGroup(database, gid, cid);
      lh.waitFor(ch.collectionGroupIsReady(database, gid));

      {
        const {plan} = ch.readCollectionGroup(database, gid);
        // check that collection was added to plan
        assertTrue(plan.collections[cid] === undefined);
      }
      {
        const {plan} = ch.readCollection(database, cid);
        assertTrue(plan === undefined);
      }
    },

    testIncreaseReplicationFactor: function () {
      const {gid} = ch.createCollectionGroupTarget(database, {
        replicationFactor: 3,
        numberOfShards: 3,
        numberOfCollections: 2,
      });

      ch.modifyCollectionGroupTarget(database, gid, function (target) {
        target.attributes.mutable.replicationFactor = 4;
        target.version = 2;
      });

      lh.waitFor(ch.collectionGroupIsReady(database, gid));
      {
        const {servers} = ch.getCollectionGroupServers(database, gid);
        for (const shard of servers) {
          assertEqual(shard.length, 4);
        }
      }
    },

    testDecreaseReplicationFactor: function () {
      const {gid, leaders} = ch.createCollectionGroupTarget(database, {
        replicationFactor: 4,
        numberOfShards: 3,
        numberOfCollections: 2,
      });

      ch.modifyCollectionGroupTarget(database, gid, function (target) {
        target.attributes.mutable.replicationFactor = 2;
        target.version = 2;
      });

      lh.waitFor(ch.collectionGroupIsReady(database, gid));
      {
        const {servers, leaders: newLeaders} = ch.getCollectionGroupServers(database, gid);
        for (const shard of servers) {
          assertEqual(shard.length, 2);
        }
        // leaders should be untouched
        assertEqual(leaders, newLeaders);
      }
    },

    testRemoveLastCollection: function () {
      const {gid, cid} = ch.createCollectionGroupTarget(database, {
        replicationFactor: 3,
        numberOfShards: 3,
        numberOfCollections: 1,
      });

      ch.dropCollectionFromGroup(database, gid, cid);
      lh.waitFor(function() {
        const {target, plan} = ch.readCollectionGroup(database, gid);
        if (target !== undefined) {
          return Error("target still present");
        }
      });

      const {target, plan, current} = ch.readCollectionGroup(database, gid);
      assertEqual(target, undefined);
      assertEqual(plan, undefined);
      assertEqual(current, undefined);
    },
  };
};

jsunity.run(collectionGroupsSupervisionSuite);
return jsunity.done();
