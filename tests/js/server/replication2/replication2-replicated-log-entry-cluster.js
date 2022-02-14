/*jshint strict: true */
/*global assertTrue, assertEqual*/
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
const {sleep} = require('internal');
const {db, errors: ERRORS} = arangodb;
const lh = require("@arangodb/testutils/replicated-logs-helper");

const database = "replication2_entry_test_db";

const waitForReplicatedLogAvailable = function (id) {
  while (true) {
    try {
      let status = db._replicatedLog(id).status();
      const leaderId = status.leaderId;
      if (leaderId !== undefined && status.participants !== undefined &&
          status.participants[leaderId].connection.errorCode === 0 && status.participants[leaderId].response.role === "leader") {
        break;
      }
      console.info("replicated log not yet available");
    } catch (err) {
      const errors = [
        ERRORS.ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED.code,
        ERRORS.ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND.code
      ];
      if (errors.indexOf(err.errorNum) === -1) {
        throw err;
      }
    }

    sleep(1);
  }
};

const replicatedLogEntrySuite = function () {
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
      }
    };
  }());

  return {
    setUpAll, tearDownAll,

    testCheckFirstIndexOfTermEntry: function () {
      const {logId, leader, servers} = lh.createReplicatedLog(database, targetConfig);
      waitForReplicatedLogAvailable(logId);
      const head = db._replicatedLog(logId).head();
      assertEqual(head.length, 1);
      const [firstEntry] = head;
      assertEqual(firstEntry.logIndex, 1);
      assertEqual(firstEntry.logTerm, 2);
      assertEqual(firstEntry.logPayload, undefined);
      assertTrue(firstEntry.meta !== undefined);
      const meta = firstEntry.meta;
      assertEqual(meta.type, "FirstIndexOfTerm");
      assertEqual(meta.leader, leader);
      assertEqual(meta.participants.generation, 1);
      assertEqual(Object.keys(meta.participants.participants).sort(), servers.sort());
      lh.replicatedLogDeleteTarget(database, logId);
    },

    testCheckUpdateParticipantsConfig: function () {
      const {logId, leader, servers, followers} = lh.createReplicatedLog(database, targetConfig);
      waitForReplicatedLogAvailable(logId);
      const follower = _.sample(followers);

      lh.replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {
        [follower]: {forced: true},
      });
      lh.waitFor(lh.replicatedLogParticipantsFlag(database, logId, {
        [follower]: {
          excluded: false,
          forced: true
        }
      }));

      const head = db._replicatedLog(logId).head();
      assertEqual(head.length, 2);
      const entry = head[1];
      assertEqual(entry.logIndex, 2);
      assertEqual(entry.logTerm, 2);
      assertEqual(entry.logPayload, undefined);
      assertTrue(entry.meta !== undefined);
      const meta = entry.meta;
      assertEqual(meta.type, "UpdateParticipantsConfig");
      assertEqual(meta.leader, undefined);
      assertEqual(meta.participants.generation, 2);
      assertEqual(Object.keys(meta.participants.participants).sort(), servers.sort());
      lh.replicatedLogDeleteTarget(database, logId);
    }
  };
};

jsunity.run(replicatedLogEntrySuite);
return jsunity.done();
