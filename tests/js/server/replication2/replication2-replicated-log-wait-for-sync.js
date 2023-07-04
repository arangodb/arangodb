/*jshint strict: true */
/*global assertTrue, assertFalse, assertEqual, assertNotNull, print*/
'use strict';

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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const db = arangodb.db;
const helper = require('@arangodb/test-helper');
const lh = require("@arangodb/testutils/replicated-logs-helper");
const lp = require("@arangodb/testutils/replicated-logs-predicates");

const database = "replication2_wait_for_sync_test_db";

const replicatedLogSyncIndexSuiteReplication2 = function () {
  const numberOfServers = 3;

  const {setUpAll, tearDownAll, setUp, tearDownAnd} =
    lh.testHelperFunctions(database, {replicationVersion: "2"});

  const createReplicatedLog = function (database, config) {
    const res = lh.createReplicatedLog(database, config, numberOfServers);
    lh.waitFor(lp.replicatedLogIsReady(database, res.logId, res.term, res.servers, res.leader));
    return res;
  };

  const clearAllFailurePoints = () => {
    for (const server of lh.dbservers) {
      helper.debugClearFailAt(lh.getServerUrl(server));
    }
  };

  return {
    setUpAll,
    tearDownAll,
    setUp,
    tearDown: tearDownAnd(() => {
      clearAllFailurePoints();
    }),

    testSyncIndexWaitForSyncTrue: function () {
      const config = {
        writeConcern: numberOfServers,
        softWriteConcern: numberOfServers,
        waitForSync: true,
      };

      const {logId} = createReplicatedLog(database, config);
      let log = db._replicatedLog(logId)

      // Insert 100 entries
      for (let i = 0; i < 100; i++) {
        log.ping();
      }

      // For wait-for-sync true, we expect the syncIndex to be equal to the spearhead
      let status = log.status();
      for (const [k, v] of Object.entries(status.participants)) {
        assertEqual(v.response.local.syncIndex, v.response.local.spearhead.index,
          `syncIndex should be equal to spearhead for ${k}, status: ${JSON.stringify(status)}`);
        assertEqual(v.response.lowestIndexToKeep, 101,
          `lowestIndexToKeep should be 101 for ${k}, status: ${JSON.stringify(status)}`);
      }
    },

    testSyncIndexWaitForSyncFalse: function () {
      const config = {
        writeConcern: numberOfServers,
        softWriteConcern: numberOfServers,
        waitForSync: false,
      };

      const {logId, leader, followers} = createReplicatedLog(database, config);
      let log = db._replicatedLog(logId)

      for (let i = 0; i < 100; i++) {
        log.insert({foo: `foo${i}`});
      }

      // For wait-for-sync false, we expect the syncIndex to be lower or equal to the spearhead
      let status = log.status();
      for (const [k, v] of Object.entries(status.participants)) {
        assertTrue(v.response.local.syncIndex <= v.response.local.spearhead.index,
          `syncIndex should be less then or equal to spearhead for ${k}, status: ${JSON.stringify(status)}`);
      }

      // Wait for lowestIndexToKeep to be updated, an indication that data has been synced
      lh.waitFor(() => {
        log.ping();
        status = log.status();
        if (status.participants[leader].response.lowestIndexToKeep >= 101) {
          return true;
        }
        return Error(`lowestIndexToKeep should be >= 101 for ${leader}, status: ${JSON.stringify(status)}`);
      });

      const fixedLowestIndexToKeep = status.participants[leader].response.lowestIndexToKeep;
      const fixedFollowerSyncIndex = status.participants[followers[0]].response.local.syncIndex;

      // One of the followers no longer updates the syncIndex.
      // Therefore, the syncIndex and the lowestIndexToKeep should no longer be updated.
      helper.debugSetFailAt(lh.getServerUrl(followers[0]), "AsyncLogWriteBatcher::syncIndexDisabled");
      for (let i = 0; i < 100; i++) {
        log.insert({bar: `bar${i}`});
      }
      status = log.status();
      assertEqual(status.participants[followers[0]].response.local.syncIndex, fixedFollowerSyncIndex,
        `syncIndex should be ${fixedFollowerSyncIndex} for ${followers[0]}, status: ${JSON.stringify(status)}`);
      for (const [k, v] of Object.entries(status.participants)) {
        assertEqual(v.response.lowestIndexToKeep, fixedLowestIndexToKeep,
          `lowestIndexToKeep should be ${fixedLowestIndexToKeep} for ${k}, status: ${JSON.stringify(status)}`);
      }

      // Perform regular compaction.
      log.release(fixedLowestIndexToKeep + 100);
      log.compact();
      status = log.status();
      assertEqual(status.participants[leader].response.local.firstIndex, fixedLowestIndexToKeep,
        `firstIndex should be ${fixedLowestIndexToKeep} for ${leader}, status: ${JSON.stringify(status)}`);

      helper.debugClearFailAt(lh.getServerUrl(followers[0]));

      // Wait for lowestIndexToKeep to be updated, an indication that data has been synced
      lh.waitFor(() => {
        log.ping();
        status = log.status();
        if (status.participants[leader].response.lowestIndexToKeep > fixedLowestIndexToKeep) {
          return true;
        }
        return Error(`lowestIndexToKeep should be > ${fixedLowestIndexToKeep} for ${leader}, ` +
          `status: ${JSON.stringify(status)}`);
      });
    }
  };
};

jsunity.run(replicatedLogSyncIndexSuiteReplication2);

return jsunity.done();
