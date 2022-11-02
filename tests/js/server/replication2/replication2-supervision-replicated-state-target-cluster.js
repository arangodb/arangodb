/*jshint strict: true */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const _ = require('lodash');
const lh = require("@arangodb/testutils/replicated-logs-helper");
const sh = require("@arangodb/testutils/replicated-state-helper");
const spreds = require("@arangodb/testutils/replicated-state-predicates");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");

const database = "replication2_supervision_test_db";

const replicatedStateSuite = function (stateType) {
  const replicationFactor = 3;
  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    waitForSync: false,
  };

  const {setUpAll, tearDownAll, stopServer, continueServer, setUp, tearDown} = lh.testHelperFunctions(database);

  const createReplicatedStateWithServers = function (servers) {
    return sh.createReplicatedStateTargetWithServers(database, targetConfig, stateType, servers);
  };
  const createReplicatedState = function () {
    return sh.createReplicatedStateTarget(database, targetConfig, stateType);
  };

  return {
    setUpAll, tearDownAll, setUp, tearDown,

    // Request creation of a replicated state by
    // writing a configuration to Target
    //
    // Await availability of the requested state
    ["testCreateReplicatedState" + stateType]: function () {
      const stateId = lh.nextUniqueLogId();

      const servers = _.sampleSize(lh.dbservers, 3);
      let participants = {};
      for (const server of servers) {
        participants[server] = {};
      }

      sh.updateReplicatedStateTarget(database, stateId,
          function () {
            return {
              id: stateId,
              participants: participants,
              config: {
                waitForSync: true,
                writeConcern: 2,
                softWriteConcern: 3,
              },
              properties: {
                implementation: {
                  type: stateType
                }
              }
            };
          });

      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, servers));
    },

    ["testAddParticipant_" + stateType]: function () {
      const {stateId, servers, others} = createReplicatedState();
      const newParticipant = _.sample(others);

      sh.updateReplicatedStateTarget(database, stateId,
          function (target) {
            target.participants[newParticipant] = {};
            return target;
          });
      const newServers = [...servers, newParticipant];

      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, newServers));

      lh.waitFor(lpreds.replicatedLogParticipantsFlag(database, stateId, {
        [newParticipant]: {allowedInQuorum: true, allowedAsLeader: true, forced: false},
      }));
    },

    ["testUpdateVersionTest_" + stateType]: function () {
      const {stateId} = createReplicatedState();

      const version = 4;
      sh.updateReplicatedStateTarget(database, stateId,
          function (target) {
            target.version = version;
            return target;
          });

      lh.waitFor(spreds.replicatedStateVersionConverged(database, stateId, version));

      const newVersion = 6;
      sh.updateReplicatedStateTarget(database, stateId,
          function (target) {
            target.version = newVersion;
            return target;
          });

      lh.waitFor(spreds.replicatedStateVersionConverged(database, stateId, newVersion));
    },

    ["testSetLeader_" + stateType]: function () {
      const {stateId, followers} = createReplicatedState();
      const newLeader = _.sample(followers);
      sh.updateReplicatedStateTarget(database, stateId,
          (target) => {
            target.leader = newLeader;
            return target;
          });
      lh.waitFor(lpreds.replicatedLogLeaderTargetIs(database, stateId, newLeader));
      lh.waitFor(lpreds.replicatedLogLeaderPlanIs(database, stateId, newLeader));
    },

    ["testUnsetLeader_" + stateType]: function () {
      const {stateId} = createReplicatedState();
      sh.updateReplicatedStateTarget(database, stateId,
          (target) => {
            delete target.leader;
            return target;
          });
      lh.waitFor(() => {
        const currentLeader = lh.getReplicatedLogLeaderTarget(database, stateId);
        if (currentLeader === undefined) {
          return true;
        } else {
          return new Error(`Expected log leader to be unset, but is still ${currentLeader}`);
        }
      });
    },

    ["testRemoveLeader_" + stateType]: function () {
      const servers = _.sampleSize(lh.dbservers, 4);
      const {stateId, leader} = createReplicatedStateWithServers(servers);

      const serverToBeRemoved = leader;
      sh.updateReplicatedStateTarget(database, stateId,
          function (target) {
            delete target.participants[serverToBeRemoved];
            target.version = 2;
            return target;
          });
      lh.waitFor(spreds.replicatedStateVersionConverged(database, stateId, 2));
      const newServers = _.without(servers, serverToBeRemoved);
      lh.waitFor(lpreds.replicatedLogParticipantsFlag(database, stateId, {
        [serverToBeRemoved]: null,
      }));
      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, newServers));
    },

    ["testReplaceAllServers_" + stateType]: function () {
      const {stateId, others} = createReplicatedState();
      const otherServers = _.sampleSize(others, replicationFactor);

      sh.updateReplicatedStateTarget(database, stateId, function (target) {
        target.participants = Object.assign({}, ...otherServers.map((x) => ({[x]: {}})));
        target.version = 3;
        return target;
      });
      lh.waitFor(spreds.replicatedStateVersionConverged(database, stateId, 3));
      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, otherServers));
    },

    ["testReplaceWithBadParticipant_" + stateType]: function () {
      const {stateId, servers, others, followers} = createReplicatedState();
      const toBeReplaced = _.sample(followers);
      const newParticipant = _.sample(others);

      // first stop that new server
      stopServer(newParticipant);

      sh.updateReplicatedStateTarget(database, stateId,
          function (target) {
            delete target.participants[toBeReplaced];
            target.participants[newParticipant] = {};
            target.version = 4;
            return target;
          });
      const newServers = [..._.without(servers, toBeReplaced), newParticipant];

      // wait for the server to find its way into the current flags
      lh.waitFor(lpreds.replicatedLogParticipantsFlag(database, stateId, {
        [newParticipant]: {allowedAsLeader: false, allowedInQuorum: false, forced: false},
      }));

      // wait for the supervision to report that the snapshot is missing
      lh.waitFor(spreds.replicatedStateSupervisionStatus(database, stateId, [
        {participant: newParticipant, code: "ServerSnapshotMissing"},
        {participant: toBeReplaced, code: "InsufficientSnapshotCoverage"}
      ], true));

      continueServer(newParticipant);
      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, newServers));
      lh.waitFor(lpreds.replicatedLogParticipantsFlag(database, stateId, {
        [newParticipant]: {allowedInQuorum: true, allowedAsLeader: true, forced: false},
      }));
      lh.waitFor(spreds.replicatedStateVersionConverged(database, stateId, 4));
    },

    ["testDropFromTarget_" + stateType]: function () {
      const {stateId} = createReplicatedState();
      sh.replicatedStateDeleteTarget(database, stateId);

      lh.waitFor(lpreds.replicatedLogIsGone(database, stateId));
      lh.waitFor(spreds.replicatedStateIsGone(database, stateId));
    },

    ["testUpdateConfig_" + stateType]: function () {
      const {stateId} = createReplicatedState();
      const newConfig = {
        waitForSync: true,
        writeConcern: 3,
        softWriteConcern: 2,
      };
      sh.updateReplicatedStateTarget(database, stateId,
          (target) => {
            target.config = newConfig;
            target.version = 8;
            return target;
          });

      lh.waitFor(() => {
        const {target} = lh.readReplicatedLogAgency(database, stateId);
        if (!_.isEqual(target.config, newConfig)) {
          return Error(`Config not yet updated in target; found = ${JSON.stringify(target.config)}, expected = ${JSON.stringify(newConfig)}`);
        }
        return true;
      });

      lh.waitFor(spreds.replicatedStateVersionConverged(database, stateId, 8));
    },
  };
};

const suiteWithState = function (stateType) {
  return function () {
    return replicatedStateSuite(stateType);
  };
};

jsunity.run(suiteWithState("black-hole"));
jsunity.run(suiteWithState("prototype"));
return jsunity.done();
