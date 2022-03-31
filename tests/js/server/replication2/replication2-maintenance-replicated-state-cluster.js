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
const {dbservers} = require("@arangodb/testutils/replicated-logs-helper");

const database = "Replicated_StateMaintenanceTestDB";

const {setUpAll, tearDownAll} = (function () {
  let previousDatabase, databaseExisted = true;
  return {
    setUpAll: function () {
      previousDatabase = db._name();
      if (!_.includes(db._databases(), database)) {
        db._createDatabase(database);
        databaseExisted = false;
      }
      db._useDatabase(database);
    },

    tearDownAll: function () {
      db._useDatabase(previousDatabase);
      if (!databaseExisted) {
        db._dropDatabase(database);
      }
    },
  };
}());

const replicatedStateSuite = function () {

  const createReplicatedState = function (database, logId, servers, leader) {
    SH.updateReplicatedStatePlan(database, logId, function () {
      let log = {
        id: logId,
        currentTerm: {
          term: 1,
          config: {
            replicationFactor: 3,
            writeConcern: 2,
            softWriteConcern: 2,
            waitForSync: false,
          },
          leader: {
            serverId: leader,
            rebootId: LH.getServerRebootId(leader),
          }
        },
        participantsConfig: {
          generation: 1,
          participants: {},
        }
      };
      let state = {
        id: logId,
        generation: 1,
        participants: {},
        properties: {
          implementation: {
            type: "black-hole",
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

  return {
    setUpAll, tearDownAll,
    setUp: LH.registerAgencyTestBegin,
    tearDown: LH.registerAgencyTestEnd,

    testCreateReplicatedState: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader);
      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));
    },

    testCheckTimestampSnapshotStatus: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader);
      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));
      {
        const {current} = SH.readReplicatedStateAgency(database, logId);
        for (const p of servers) {
          assertTrue(current.participants[p].snapshot.timestamp !== undefined);
        }
      }
    },

    testReplicatedStateUpdateParticipantGeneration: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader);

      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));


      const followers = _.difference(servers, [leader]);
      const follower = _.sample(followers);
      SH.updateReplicatedStatePlan(database, logId, function (state, log) {
        state.participants[follower].generation += 1;
        return {state, log};
      });

      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));
    },

    testReplicatedStateChangeLeader: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader);

      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));

      SH.updateReplicatedStatePlan(database, logId, function (state, log) {
        log.currentTerm.term += 1;
        return {state, log};
      });

      LH.waitFor(LH.replicatedLogIsReady(database, logId, 2, servers, leader));
      LH.waitFor(spreds.replicatedStateIsReady(database, logId, servers));
    },

    testReplicatedStateDropFollower: function () {
      const logId = LH.nextUniqueLogId();
      const servers = _.sampleSize(LH.dbservers, 3);
      const leader = servers[0];
      createReplicatedState(database, logId, servers, leader);

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
      LH.waitFor(LH.replicatedLogIsReady(database, logId, 1, newParticipants, leader));
      LH.waitFor(spreds.replicatedStateIsReady(database, logId, newParticipants));
    },
  };
};

jsunity.run(replicatedStateSuite);
return jsunity.done();
