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
const {assertEqual, assertTrue, assertUndefined} = jsunity.jsUnity.assertions;
const arangodb = require("@arangodb");
const _ = require('lodash');
const db = arangodb.db;
const lh = require("@arangodb/testutils/replicated-logs-helper");
const sh = require("@arangodb/testutils/replicated-state-helper");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");
const spreds = require("@arangodb/testutils/replicated-state-predicates");
const request = require('@arangodb/request');
const {waitFor} = require("@arangodb/testutils/replicated-logs-helper");

const database = "replication2_replicated_state_http_api_db";

const replaceParticipant = (database, logId, oldParticipant, newParticipant) => {
  const url = lh.getServerUrl(_.sample(lh.coordinators));
  const res = request.post(
      `${url}/_db/${database}/_api/replicated-state/${logId}/participant/${oldParticipant}/replace-with/${newParticipant}`
  );
  lh.checkRequestResult(res);
  const {json: {result}} = res;
  return result;
};

const setLeader = (database, logId, newLeader) => {
  const url = lh.getServerUrl(_.sample(lh.coordinators));
  const res = request.post(`${url}/_db/${database}/_api/replicated-state/${logId}/leader/${newLeader}`);
  lh.checkRequestResult(res);
  const {json: {result}} = res;
  return result;
};
const unsetLeader = (database, logId) => {
  const url = lh.getServerUrl(_.sample(lh.coordinators));
  const res = request.delete(`${url}/_db/${database}/_api/replicated-state/${logId}/leader`);
  lh.checkRequestResult(res);
  const {json: {result}} = res;
  return result;
};
const dropState = (database, logId) => {
  const url = lh.getServerUrl(_.sample(lh.coordinators));
  const res = request.delete(`${url}/_db/${database}/_api/replicated-state/${logId}`);
  lh.checkRequestResult(res);
  return res.json;
};

const replicatedStateSuite = function (stateType) {
  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    waitForSync: false,
  };

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

  return {
    setUpAll, tearDownAll,
    setUp: lh.registerAgencyTestBegin,
    tearDown: lh.registerAgencyTestEnd,

    testGetLocalStatus: function () {
      const {stateId, followers, leader} = sh.createReplicatedStateTarget(database, targetConfig, stateType);

      {
        const status = sh.getLocalStatus(leader, database, stateId);
        assertEqual(status.role, "leader");
        assertEqual(status.manager.managerState, "ServiceAvailable");

        assertEqual(status.snapshot.status, "Completed");
        assertTrue(status.snapshot.timestamp !== undefined);
        assertEqual(status.generation, 1);

      }

      for (const server of followers) {
        const status = sh.getLocalStatus(server, database, stateId);
        assertEqual(status.role, "follower");
        assertEqual(status.manager.managerState, "NothingToApply");
        assertEqual(status.snapshot.status, "Completed");
        assertTrue(status.snapshot.timestamp !== undefined);
        assertEqual(status.generation, 1);
      }
    },

    testReplaceParticipantReplaceFollower: function () {
      const {
        stateId,
        servers: participants,
        followers
      } = sh.createReplicatedStateTarget(database, targetConfig, stateType);
      const nonParticipants = _.without(lh.dbservers, ...participants);
      const oldParticipant = _.sample(followers);
      const newParticipant = _.sample(nonParticipants);
      const newParticipants = _.union(_.without(participants, oldParticipant), [newParticipant]).sort();

      const result = replaceParticipant(database, stateId, oldParticipant, newParticipant);
      assertEqual({}, result);
      {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      }

      waitFor(() => {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      });
      // Current won't be cleaned up yet.
      // waitFor(() => {
      //   const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      //   return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
      // });

      const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      assertEqual(newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      // Current won't be cleaned up yet.
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
    },

    testReplaceParticipantReplaceLeader: function () {
      const {
        stateId,
        servers: participants,
        leader
      } = sh.createReplicatedStateTarget(database, targetConfig, stateType);
      const nonParticipants = _.without(lh.dbservers, ...participants);
      const oldParticipant = leader;
      const newParticipant = _.sample(nonParticipants);
      const newParticipants = _.union(_.without(participants, oldParticipant), [newParticipant]).sort();

      const result = replaceParticipant(database, stateId, oldParticipant, newParticipant);
      assertEqual({}, result);
      {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      }

      waitFor(() => {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      });
      // Current won't be cleaned up yet.
      // waitFor(() => {
      //   const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      //   return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
      // });

      const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      assertEqual(newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      // Current won't be cleaned up yet.
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
    },

    testReplaceParticipantReplaceFixedLeader: function () {
      const {
        stateId,
        servers: participants,
        leader
      } = sh.createReplicatedStateTarget(database, targetConfig, stateType);

      {
        // Explicitly set the leader, just use the existing one
        const result = setLeader(database, stateId, leader);
        assertEqual({}, result);
        // Wait for it to trickle down the the log target
        lh.waitFor(lpreds.replicatedLogLeaderTargetIs(database, stateId, leader));
        // The plan shouldn't have changed
        assertTrue(lpreds.replicatedLogLeaderPlanIs(database, stateId, leader));
      }

      const nonParticipants = _.without(lh.dbservers, ...participants);
      const oldLeader = leader;
      const newLeader = _.sample(nonParticipants);
      const newParticipants = _.union(_.without(participants, oldLeader), [newLeader]).sort();

      const result = replaceParticipant(database, stateId, oldLeader, newLeader);
      assertEqual({}, result);
      {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      }

      waitFor(spreds.replicatedStateTargetLeaderIs(database, stateId, newLeader));
      waitFor(() => {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      });
      waitFor(lpreds.replicatedLogLeaderTargetIs(database, stateId, newLeader));
      waitFor(lpreds.replicatedLogLeaderPlanIs(database, stateId, newLeader));
      // Current won't be cleaned up yet.
      // waitFor(() => {
      //   const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      //   return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
      // });

      const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      assertEqual(newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      // Current won't be cleaned up yet.
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
    },

    testReplaceParticipantReplaceNonParticipant: function () {
      const {
        stateId,
        servers: participants,
      } = sh.createReplicatedStateTarget(database, targetConfig, stateType);
      const nonParticipants = _.without(lh.dbservers, ...participants);
      const [oldParticipant, newParticipant] = _.sampleSize(nonParticipants, 2);

      try {
        const result = replaceParticipant(database, stateId, oldParticipant, newParticipant);
        // noinspection ExceptionCaughtLocallyJS
        throw new Error(`replaceParticipant unexpectedly succeeded with ${JSON.stringify(result)}`);
      } catch (e) {
        assertEqual(412, e.code);
      }

      {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        assertEqual(participants.sort(), Object.keys(stateAgencyContent.target.participants).sort());
      }
    },

    testReplaceParticipantReplaceWithExistingParticipant: function () {
      const {
        stateId,
        servers: participants,
      } = sh.createReplicatedStateTarget(database, targetConfig, stateType);
      const [oldParticipant, newParticipant] = _.sampleSize(participants, 2);

      try {
        const result = replaceParticipant(database, stateId, oldParticipant, newParticipant);
        // noinspection ExceptionCaughtLocallyJS
        throw new Error(`replaceParticipant unexpectedly succeeded with ${JSON.stringify(result)}`);
      } catch (e) {
        assertEqual(412, e.code);
      }

      {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        assertEqual(participants.sort(), Object.keys(stateAgencyContent.target.participants).sort());
      }
    },

    testSetLeader: function () {
      const {
        stateId,
        followers,
      } = sh.createReplicatedStateTarget(database, targetConfig, stateType);
      const newLeader = _.sample(followers);

      const result = setLeader(database, stateId, newLeader);
      assertEqual({}, result);
      {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        assertEqual(newLeader, stateAgencyContent.target.leader);
      }
      lh.waitFor(lpreds.replicatedLogLeaderTargetIs(database, stateId, newLeader));
      lh.waitFor(lpreds.replicatedLogLeaderPlanIs(database, stateId, newLeader));
    },

    testUnsetLeader: function () {
      const {
        stateId,
        followers,
      } = sh.createReplicatedStateTarget(database, targetConfig, stateType);
      { // set the leader first
        const newLeader = _.sample(followers);

        const result = setLeader(database, stateId, newLeader);
        assertEqual({}, result);
        {
          const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
          assertEqual(newLeader, stateAgencyContent.target.leader);
        }
        lh.waitFor(lpreds.replicatedLogLeaderTargetIs(database, stateId, newLeader));
        lh.waitFor(lpreds.replicatedLogLeaderPlanIs(database, stateId, newLeader));
      }

      // unset the leader
      const result = unsetLeader(database, stateId);
      assertEqual({}, result);
      {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        assertUndefined(stateAgencyContent.target.leader);
      }
      lh.waitFor(() => {
        const currentLeader = lh.getReplicatedLogLeaderTarget(database, stateId);
        if (currentLeader === undefined) {
          return true;
        } else {
          return new Error(`Expected log leader to be unset, but is still ${currentLeader}`);
        }
      });
    },

    testSetLeaderWithNonParticipant: function () {
      const {
        stateId,
        servers: participants,
      } = sh.createReplicatedStateTarget(database, targetConfig, stateType);
      const nonParticipants = _.without(lh.dbservers, ...participants);
      const newLeader = _.sample(nonParticipants);

      // try to set a leader that's not a participant
      try {
        const result = setLeader(database, stateId, newLeader);
        throw new Error(`setLeader unexpectedly succeeded with ${JSON.stringify(result)}`);
      } catch (e) {
        assertEqual(412, e.code);
      }
      {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        assertUndefined(stateAgencyContent.target.leader);
      }
    },

    testDropReplicatedState: function () {
      const {stateId} = sh.createReplicatedStateTarget(database, targetConfig, stateType);

      const res = dropState(database, stateId);
      assertEqual(200, res.code);

      lh.waitFor(spreds.replicatedStateIsGone(database, stateId));
      lh.waitFor(lpreds.replicatedLogIsGone(database, stateId));
    },
  };
};

const suiteWithState = function (stateType) {
  return function () {
    return replicatedStateSuite(stateType);
  };
};

jsunity.run(suiteWithState("black-hole"));
return jsunity.done();
