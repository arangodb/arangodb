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
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");
const request = require('@arangodb/request');
const {waitFor} = require("@arangodb/testutils/replicated-logs-helper");

const database = "replication2_replicated_state_http_api_db";

const {setLeader, unsetLeader} = lh;
const dropState = (database, logId) => {
  const url = lh.getServerUrl(_.sample(lh.coordinators));
  const res = request.delete(`${url}/_db/${database}/_api/log/${logId}`);
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

    testReplaceParticipantReplaceFollower: function () {
      const {
        logId,
        servers: participants,
        followers
      } = lh.createReplicatedLogWithState(database, targetConfig, stateType);
      const nonParticipants = _.without(lh.dbservers, ...participants);
      const oldParticipant = _.sample(followers);
      const newParticipant = _.sample(nonParticipants);
      const newParticipants = _.union(_.without(participants, oldParticipant), [newParticipant]).sort();

      const result = lh.replaceParticipant(database, logId, oldParticipant, newParticipant);
      assertEqual({}, result);
      {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      }

      waitFor(() => {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.plan.participantsConfig.participants).sort());
      });
      // Current won't be cleaned up yet.
      // waitFor(() => {
      //   const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
      //   return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
      // });

      const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
      assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      assertEqual(newParticipants, Object.keys(stateAgencyContent.plan.participantsConfig.participants).sort());
      // Current won't be cleaned up yet.
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
    },

    testReplaceParticipantReplaceLeader: function () {
      const {
        logId,
        servers: participants,
        leader
      } = lh.createReplicatedLogWithState(database, targetConfig, stateType);
      const nonParticipants = _.without(lh.dbservers, ...participants);
      const oldParticipant = leader;
      const newParticipant = _.sample(nonParticipants);
      const newParticipants = _.union(_.without(participants, oldParticipant), [newParticipant]).sort();

      const result = lh.replaceParticipant(database, logId, oldParticipant, newParticipant);
      assertEqual({}, result);
      {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      }

      waitFor(() => {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.plan.participantsConfig.participants).sort());
      });
      // Current won't be cleaned up yet.
      // waitFor(() => {
      //   const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
      //   return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
      // });

      const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
      assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      assertEqual(newParticipants, Object.keys(stateAgencyContent.plan.participantsConfig.participants).sort());
      // Current won't be cleaned up yet.
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
    },

    testReplaceParticipantReplaceFixedLeader: function () {
      const {
        logId,
        servers: participants,
        leader
      } = lh.createReplicatedLogWithState(database, targetConfig, stateType);

      {
        // Explicitly set the leader, just use the existing one
        const result = setLeader(database, logId, leader);
        assertEqual({}, result);
        // Wait for it to trickle down the the log target
        lh.waitFor(lpreds.replicatedLogLeaderTargetIs(database, logId, leader));
        // The plan shouldn't have changed
        assertTrue(lpreds.replicatedLogLeaderPlanIs(database, logId, leader));
      }

      const nonParticipants = _.without(lh.dbservers, ...participants);
      const oldLeader = leader;
      const newLeader = _.sample(nonParticipants);
      const newParticipants = _.union(_.without(participants, oldLeader), [newLeader]).sort();

      const result = lh.replaceParticipant(database, logId, oldLeader, newLeader);
      assertEqual({}, result);
      {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      }

      waitFor(lpreds.replicatedLogLeaderTargetIs(database, logId, newLeader));
      waitFor(() => {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.plan.participantsConfig.participants).sort());
      });
      waitFor(lpreds.replicatedLogLeaderTargetIs(database, logId, newLeader));
      waitFor(lpreds.replicatedLogLeaderPlanIs(database, logId, newLeader));
      // Current won't be cleaned up yet.
      // waitFor(() => {
      //   const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
      //   return lh.sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
      // });

      const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
      assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      assertEqual(newParticipants, Object.keys(stateAgencyContent.plan.participantsConfig.participants).sort());
      // Current won't be cleaned up yet.
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
    },

    testReplaceParticipantReplaceNonParticipant: function () {
      const {
        logId,
        servers: participants,
      } = lh.createReplicatedLogWithState(database, targetConfig, stateType);
      const nonParticipants = _.without(lh.dbservers, ...participants);
      const [oldParticipant, newParticipant] = _.sampleSize(nonParticipants, 2);

      try {
        const result = lh.replaceParticipant(database, logId, oldParticipant, newParticipant);
        // noinspection ExceptionCaughtLocallyJS
        throw new Error(`replaceParticipant unexpectedly succeeded with ${JSON.stringify(result)}`);
      } catch (e) {
        assertEqual(412, e.code);
      }

      {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        assertEqual(participants.sort(), Object.keys(stateAgencyContent.target.participants).sort());
      }
    },

    testReplaceParticipantReplaceWithExistingParticipant: function () {
      const {
        logId,
        servers: participants,
      } = lh.createReplicatedLogWithState(database, targetConfig, stateType);
      const [oldParticipant, newParticipant] = _.sampleSize(participants, 2);

      try {
        const result = lh.replaceParticipant(database, logId, oldParticipant, newParticipant);
        // noinspection ExceptionCaughtLocallyJS
        throw new Error(`replaceParticipant unexpectedly succeeded with ${JSON.stringify(result)}`);
      } catch (e) {
        assertEqual(412, e.code);
      }

      {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        assertEqual(participants.sort(), Object.keys(stateAgencyContent.target.participants).sort());
      }
    },

    testSetLeader: function () {
      const {
        logId,
        followers,
      } = lh.createReplicatedLogWithState(database, targetConfig, stateType);
      const newLeader = _.sample(followers);

      const result = setLeader(database, logId, newLeader);
      assertEqual({}, result);
      {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        assertEqual(newLeader, stateAgencyContent.target.leader);
      }
      lh.waitFor(lpreds.replicatedLogLeaderTargetIs(database, logId, newLeader));
      lh.waitFor(lpreds.replicatedLogLeaderPlanIs(database, logId, newLeader));
    },

    testUnsetLeader: function () {
      const {
        logId,
        followers,
      } = lh.createReplicatedLogWithState(database, targetConfig, stateType);
      { // set the leader first
        const newLeader = _.sample(followers);

        const result = setLeader(database, logId, newLeader);
        assertEqual({}, result);
        {
          const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
          assertEqual(newLeader, stateAgencyContent.target.leader);
        }
        lh.waitFor(lpreds.replicatedLogLeaderTargetIs(database, logId, newLeader));
        lh.waitFor(lpreds.replicatedLogLeaderPlanIs(database, logId, newLeader));
      }

      // unset the leader
      const result = unsetLeader(database, logId);
      assertEqual({}, result);
      {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        assertUndefined(stateAgencyContent.target.leader);
      }
      lh.waitFor(() => {
        const currentLeader = lh.getReplicatedLogLeaderTarget(database, logId);
        if (currentLeader === undefined) {
          return true;
        } else {
          return new Error(`Expected log leader to be unset, but is still ${currentLeader}`);
        }
      });
    },

    testSetLeaderWithNonParticipant: function () {
      const {
        logId,
        servers: participants,
      } = lh.createReplicatedLogWithState(database, targetConfig, stateType);
      const nonParticipants = _.without(lh.dbservers, ...participants);
      const newLeader = _.sample(nonParticipants);

      // try to set a leader that's not a participant
      try {
        const result = setLeader(database, logId, newLeader);
        throw new Error(`setLeader unexpectedly succeeded with ${JSON.stringify(result)}`);
      } catch (e) {
        assertEqual(412, e.code);
      }
      {
        const stateAgencyContent = lh.readReplicatedLogAgency(database, logId);
        assertUndefined(stateAgencyContent.target.leader);
      }
    },

    testDropReplicatedState: function () {
      const {logId} = lh.createReplicatedLogWithState(database, targetConfig, stateType);

      const res = dropState(database, logId);
      assertEqual(200, res.code);

      lh.waitFor(lpreds.replicatedLogIsGone(database, logId));
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
