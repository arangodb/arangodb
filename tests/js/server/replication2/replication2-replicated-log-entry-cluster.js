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
const {db} = arangodb;
const lh = require("@arangodb/testutils/replicated-logs-helper");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");
const helper = require("@arangodb/testutils/replicated-logs-helper");

const {waitForReplicatedLogAvailable} = helper;

const database = "replication2_entry_test_db";

const replicatedLogEntrySuite = function () {
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
      }
    };
  }());

  return {
    setUpAll, tearDownAll,
    setUp: lh.registerAgencyTestBegin,
    tearDown: lh.registerAgencyTestEnd,

    testCheckFirstIndexOfTermEntry: function () {
      const {logId, leader, servers} = lh.createReplicatedLog(database, targetConfig);
      waitForReplicatedLogAvailable(logId);
      const head = db._replicatedLog(logId).head();
      assertEqual(head.length, 1);
      const [firstEntry] = head;
      assertEqual(firstEntry.logIndex, 1);
      assertTrue([1,2].includes(firstEntry.logTerm));
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
      const {logId, servers, followers} = lh.createReplicatedLog(database, targetConfig);
      waitForReplicatedLogAvailable(logId);
      const follower = _.sample(followers);

      lh.replicatedLogUpdateTargetParticipants(database, logId, {
        [follower]: {forced: true},
      });
      lh.waitFor(lpreds.replicatedLogParticipantsFlag(database, logId, {
        [follower]: {
          allowedInQuorum: true,
          allowedAsLeader: true,
          forced: true
        }
      }));

      const head = db._replicatedLog(logId).head();
      assertEqual(head.length, 2);
      const entry = head[1];
      assertEqual(entry.logIndex, 2);
      assertTrue([1,2].includes(entry.logTerm));
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
