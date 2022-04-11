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
const db = arangodb.db;
const lh = require("@arangodb/testutils/replicated-logs-helper");
const sh = require("@arangodb/testutils/replicated-state-helper");
const spreds = require("@arangodb/testutils/replicated-state-predicates");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");

const database = "replication2_supervision_test_db";

const updateReplicatedStateTarget = function (dbname, stateId, callback) {
  let {target: targetState} = sh.readReplicatedStateAgency(database, stateId);

  const state = callback(targetState);

  global.ArangoAgency.set(`Target/ReplicatedStates/${database}/${stateId}`, state);
  global.ArangoAgency.increaseVersion(`Target/Version`);
};

const replicatedStateSuite = function () {
  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    replicationFactor: 3,
    waitForSync: false,
  };

  const {setUpAll, tearDownAll, stopServer, continueServer, resumeAll} = (function () {
    let previousDatabase, databaseExisted = true;
    let stoppedServers = {};
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
      stopServer: function (serverId) {
        if (stoppedServers[serverId] !== undefined) {
          throw Error(`{serverId} already stopped`);
        }
        lh.stopServer(serverId);
        stoppedServers[serverId] = true;
      },
      continueServer: function (serverId) {
        if (stoppedServers[serverId] === undefined) {
          throw Error(`{serverId} not stopped`);
        }
        lh.continueServer(serverId);
        delete stoppedServers[serverId];
      },
      resumeAll: function () {
        Object.keys(stoppedServers).forEach(function (key) {
          continueServer(key);
        });
      }
    };
  }());

  const createReplicatedStateWithServers = function (servers) {
    const stateId = lh.nextUniqueLogId();

    let participants = {};
    for (const server of servers) {
      participants[server] = {};
    }

    updateReplicatedStateTarget(database, stateId,
      function () {
        return {
          id: stateId,
          participants: participants,
          config: targetConfig,
          version: 1,
          properties: {
            implementation: {
              type: "black-hole"
            }
          }
        };
      });

    lh.waitFor(spreds.replicatedStateIsReady(database, stateId, servers));
    const leader = lh.getReplicatedLogLeaderPlan(database, stateId).leader;
    const followers = _.difference(servers, [leader]);
    return {stateId, servers, leader, followers};
  };
  const createReplicatedState = function () {
    const servers = _.sampleSize(lh.dbservers, targetConfig.replicationFactor);
    return createReplicatedStateWithServers(servers);
  };

  return {
    setUpAll, tearDownAll,
    setUp: lh.registerAgencyTestBegin,
    tearDown: lh.registerAgencyTestEnd,

    // Request creation of a replicated state by
    // writing a configuration to Target
    //
    // Await availability of the requested state
    testCreateReplicatedState: function () {
      const stateId = lh.nextUniqueLogId();

      const servers = _.sampleSize(lh.dbservers, 3);
      let participants = {};
      for (const server of servers) {
        participants[server] = {};
      }

      updateReplicatedStateTarget(database, stateId,
        function (target) {
          return {
            id: stateId,
            participants: participants,
            config: {
              waitForSync: true,
              writeConcern: 2,
              softWriteConcern: 3,
              replicationFactor: 3
            },
            properties: {
              implementation: {
                type: "black-hole"
              }
            }
          };
        });

      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, servers));
    },

    testAddParticipant: function () {
      const {stateId, servers} = createReplicatedState();

      const remainingServers = _.difference(lh.dbservers, servers);
      const newParticipant = _.sample(remainingServers);

      updateReplicatedStateTarget(database, stateId,
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

    testUpdateVersionTest: function () {
      const {stateId} = createReplicatedState();

      const version = 4;
      updateReplicatedStateTarget(database, stateId,
        function (target) {
          target.version = version;
          return target;
        });

      lh.waitFor(spreds.replicatedStateVersionConverged(database, stateId, version));

      const newVersion = 6;
      updateReplicatedStateTarget(database, stateId,
        function (target) {
          target.version = newVersion;
          return target;
        });

      lh.waitFor(spreds.replicatedStateVersionConverged(database, stateId, newVersion));
    },

    testSetLeader: function () {
      const {stateId, followers} = createReplicatedState();
      const newLeader = _.sample(followers);
      updateReplicatedStateTarget(database, stateId,
        (target) => {
          target.leader = newLeader;
          return target;
        });
      lh.waitFor(lpreds.replicatedLogLeaderTargetIs(database, stateId, newLeader));
      lh.waitFor(lpreds.replicatedLogLeaderPlanIs(database, stateId, newLeader));
    },

    testUnsetLeader: function () {
      const {stateId} = createReplicatedState();
      updateReplicatedStateTarget(database, stateId,
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

    testRemoveLeader: function () {
      const servers = _.sampleSize(lh.dbservers, 4);
      const {stateId, leader} = createReplicatedStateWithServers(servers);

      const serverToBeRemoved = leader;
      updateReplicatedStateTarget(database, stateId,
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

    testReplaceAllServers: function() {
      const {stateId, servers} = createReplicatedState();
      const otherServers = _.difference(lh.dbservers, servers);

      updateReplicatedStateTarget(database, stateId, function (target) {
        target.participants = Object.assign({}, ...otherServers.map((x) => ({[x]: {}})));
        target.version = 3;
        return target;
      });
      lh.waitFor(spreds.replicatedStateVersionConverged(database, stateId, 3));
      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, otherServers));
    }
  };
};

jsunity.run(replicatedStateSuite);
return jsunity.done();
