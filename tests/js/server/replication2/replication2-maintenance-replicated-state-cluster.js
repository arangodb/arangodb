/*jshint strict: true */
/*global assertTrue */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const db = arangodb.db;
const _ = require('lodash');
const LH = require("@arangodb/testutils/replicated-logs-helper");
const SH = require("@arangodb/testutils/replicated-state-helper");
const spreds = require("@arangodb/testutils/replicated-state-predicates");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");
const {dbservers} = require("@arangodb/testutils/replicated-logs-helper");
const request = require('@arangodb/request');

const database = "Replicated_StateMaintenanceTestDB";

const {setUpAll, tearDownAll, setUp, tearDown} = LH.testHelperFunctions(database);

const createReplicatedState = function (database, logId, servers, leader, stateType) {
  SH.updateReplicatedStatePlan(database, logId, function () {
    let log = {
      id: logId,
      currentTerm: {
        term: 1,
        leader: {
          serverId: leader,
          rebootId: LH.getServerRebootId(leader),
        }
      },
      participantsConfig: {
        generation: 1,
        config: {
          effectiveWriteConcern: 2,
          waitForSync: false,
        },
        participants: {},
      }
    };
    let state = {
      id: logId,
      generation: 1,
      participants: {},
      properties: {
        implementation: {
          type: stateType,
        }
      }
    };
    for (const server of servers) {
      state.participants[server] = {
        generation: 1,
      };
      log.participantsConfig.participants[server] = {};
    }
    return {state, log};
  });
};

const replicatedStateSuite = function (stateType) {

  return {
    setUpAll, tearDownAll, setUp, tearDown,

    ["testCreateReplicatedState_" + stateType]: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader, stateType);
      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));
    },

    ["testCheckTimestampSnapshotStatus_" + stateType]: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader, stateType);
      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));
      {
        const {current} = SH.readReplicatedStateAgency(database, logId);
        for (const p of servers) {
          assertTrue(current.participants[p].snapshot.timestamp !== undefined);
        }
      }
    },

    ["testReplicatedStateUpdateParticipantGeneration_" + stateType]: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader, stateType);

      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));


      const followers = _.difference(servers, [leader]);
      const follower = _.sample(followers);
      SH.updateReplicatedStatePlan(database, logId, function (state, log) {
        state.participants[follower].generation += 1;
        return {state, log};
      });

      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));
    },

    ["testReplicatedStateChangeLeader_" + stateType]: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader, stateType);

      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));

      SH.updateReplicatedStatePlan(database, logId, function (state, log) {
        log.currentTerm.term += 1;
        return {state, log};
      });

      LH.waitFor(lpreds.replicatedLogIsReady(database, logId, 2, servers, leader));
      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));
    },

    ["testReplicatedStateIncreaseSnapshotGen_" + stateType]: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      const follower = servers[1];
      createReplicatedState(database, logId, servers, leader, stateType);

      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));

      SH.updateReplicatedStatePlan(database, logId, function (state, log) {
        state.generation += 1;
        state.participants[follower].generation += 1;
        return {state, log};
      });

      LH.waitFor(lpreds.replicatedLogIsReady(database, logId, 1, servers, leader));
      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));
    },

    ["testReplicatedStateDropFollower_" + stateType]: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader, stateType);

      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));

      const followers = _.difference(servers, [leader]);
      const oldFollower = _.sample(followers);
      const newFollower = _.sample(_.difference(dbservers, servers));
      SH.updateReplicatedStatePlan(database, logId, function (state, log) {
        log.participantsConfig.participants[newFollower] = {allowedAsLeader: false, allowedInQuorum: false};
        delete log.participantsConfig.participants[oldFollower];
        log.participantsConfig.generation += 1;
        const nextGeneration = state.generation += 1;
        state.participants[newFollower] = {generation: nextGeneration};
        delete state.participants[oldFollower];
        return {state, log};
      });

      const newParticipants = [newFollower, ..._.difference(servers, [oldFollower])];
      LH.waitFor(lpreds.replicatedLogIsReady(database, logId, 1, newParticipants, leader));
      LH.waitFor(spreds.replicatedStateIsReady(database, logId, newParticipants));
    },
  };
};

const replicatedStateDropSuite = function (stateType) {

  const getLocalStatus = function (serverId, database, logId) {
    let url = LH.getServerUrl(serverId);
    const res = request.get(`${url}/_db/${database}/_api/replicated-state/${logId}/local-status`);
    return res.json;
  };

  return {
    setUpAll, tearDownAll, setUp, tearDown,

    ["testDropLogOnly_" + stateType]: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader, stateType);
      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));

      LH.waitFor(function () {
        for (const server of servers) {
          const response = getLocalStatus(server, database, logId);
          if (response.code !== 200) {
            return Error(`Server ${server} returned code ${response.code}, expected ${200}`);
          }
        }
        return true;
      });

      LH.replicatedLogDeletePlan(database, logId);
      SH.replicatedStateDeletePlan(database, logId);

      LH.waitFor(function () {
        for (const server of servers) {
          const response = getLocalStatus(server, database, logId);
          if (response.code !== 404) {
            return Error(`Server ${server} returned code ${response.code}, expected ${404}`);
          }
          if (response.errorNum !== 1203) {
            return Error(`Server ${server} returned errorNum ${response.errorNum}, expected ${1203}`);
          }
        }
        return true;
      });
    },
  };
};

const suiteWithState = function (suite, stateType) {
  return function () {
    return suite(stateType);
  };
};

for (const type of ["black-hole", "prototype"]) {
  jsunity.run(suiteWithState(replicatedStateSuite, type));
  jsunity.run(suiteWithState(replicatedStateDropSuite, type));
}
return jsunity.done();
