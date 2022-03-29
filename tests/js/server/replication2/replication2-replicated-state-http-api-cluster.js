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
const {assertEqual, assertTrue} = jsunity.jsUnity.assertions;
const arangodb = require("@arangodb");
const _ = require('lodash');
const db = arangodb.db;
const lh = require("@arangodb/testutils/replicated-logs-helper");
const sh = require("@arangodb/testutils/replicated-state-helper");
const request = require('@arangodb/request');
const {waitFor} = require("@arangodb/testutils/replicated-logs-helper");

const database = "replication2_replicated_state_http_api_db";

const sortedArrayEqualOrError = (left, right) => {
  if (_.eq(left, right)) {
    return true;
  } else {
    return Error(`Expected the following to be equal: ${JSON.stringify(left)} and ${JSON.stringify(right)}`);
  }
};

const replaceParticipant = (database, logId, oldParticipant, newParticipant) => {
  const url = lh.getServerUrl(_.sample(lh.coordinators));
  const res = request.post(
    `${url}/_db/${database}/_api/replicated-state/${logId}/participant/${oldParticipant}/replace-with/${newParticipant}`
  );
  lh.checkRequestResult(res);
  const {json: {result}} = res;
  return result;
};

const replicatedStateSuite = function () {
  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    replicationFactor: 3,
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
      const {stateId, followers, leader} = sh.createReplicatedStateTarget(database, targetConfig, "black-hole");

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
      } = sh.createReplicatedStateTarget(database, targetConfig, "black-hole");
      const nonParticipants = _.without(lh.dbservers, ...participants);
      const oldParticipant = _.sample(followers);
      const newParticipant = _.sample(nonParticipants);
      const newParticipants = _.union(_.without(participants, oldParticipant), [newParticipant]).sort();

      const url = lh.getServerUrl(_.sample(lh.coordinators));
      const res = request.post(`${url}/_db/${database}/_api/replicated-state/${stateId}/participant/${oldParticipant}/replace-with/${newParticipant}`);
      lh.checkRequestResult(res);
      const {json: {result}} = res;
      assertEqual({}, result);
      {
        const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
        assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      }
      // The following does not yet work.

      // waitFor(() => {
      //   const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      //   return sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      // });
      // waitFor(() => {
      //   const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      //   return sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
      // });
      //
      // const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
    },

    testReplaceParticipantReplaceLeader: function () {
      const {
        stateId,
        servers: participants,
        leader
      } = sh.createReplicatedStateTarget(database, targetConfig, "black-hole");
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
      // The following does not yet work.

      // waitFor(() => {
      //   const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      //   return sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      // });
      // waitFor(() => {
      //   const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      //   return sortedArrayEqualOrError(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
      // });
      //
      // const stateAgencyContent = sh.readReplicatedStateAgency(database, stateId);
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.target.participants).sort());
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      // assertEqual(newParticipants, Object.keys(stateAgencyContent.current.participants).sort());
    },

    testReplaceParticipantReplaceNonParticipant: function () {
      const {
        stateId,
        servers: participants,
      } = sh.createReplicatedStateTarget(database, targetConfig, "black-hole");
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
      } = sh.createReplicatedStateTarget(database, targetConfig, "black-hole");
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
  };
};

jsunity.run(replicatedStateSuite);
return jsunity.done();
