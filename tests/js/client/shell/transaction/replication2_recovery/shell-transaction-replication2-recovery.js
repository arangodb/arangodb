/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global print */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Alexandru Petenchea
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const { fail,
  assertTrue,
  assertEqual,
} = jsunity.jsUnity.assertions;

const arangodb = require('@arangodb');
const _ = require('lodash');
const db = arangodb.db;
const helper = require('@arangodb/test-helper');
const internal = require('internal');
const replicatedStateHelper = require('@arangodb/testutils/replicated-state-helper');
const replicatedLogsHelper = require('@arangodb/testutils/replicated-logs-helper');
const replicatedLogsPredicates = require('@arangodb/testutils/replicated-logs-predicates');
const replicatedStatePredicates = require('@arangodb/testutils/replicated-state-predicates');
const replicatedLogsHttpHelper = require('@arangodb/testutils/replicated-logs-http-helper');
const request = require('@arangodb/request');

/**
 * TODO this function is here temporarily and is will be removed once we have a better solution.
 * Its purpose is to synchronize the participants of replicated logs with the participants of their respective shards.
 * This is needed because we're using the list of participants from two places.
 */
const syncShardsWithLogs = function(dbn) {
  const coordinator = replicatedLogsHelper.coordinators[0];
  let logs = replicatedLogsHttpHelper.listLogs(coordinator, dbn).result;
  let collections = replicatedLogsHelper.readAgencyValueAt(`Plan/Collections/${dbn}`);
  for (const [colId, colInfo] of Object.entries(collections)) {
    for (const shardId of Object.keys(colInfo.shards)) {
      const logId = shardId.slice(1);
      if (logId in logs) {
        helper.agency.set(`Plan/Collections/${dbn}/${colId}/shards/${shardId}`, logs[logId]);
      }
    }
  }

  const waitForCurrent  = replicatedLogsHelper.readAgencyValueAt("Current/Version");
  helper.agency.increaseVersion(`Plan/Version`);

  replicatedLogsHelper.waitFor(() => {
    const currentVersion  = replicatedLogsHelper.readAgencyValueAt("Current/Version");
    if (currentVersion > waitForCurrent) {
      return true;
    }
    return Error(`Current/Version expected to be greater than ${waitForCurrent}, but got ${currentVersion}`);
  }, 30, (e) => {
    // We ignore this and continue. Most probably current was increased before we could observe it.
    print(e.message);
  });
};

/**
 * In this test suite we check if the DocumentState can survive modifications to the cluster participants
 * during transactions. We check for failover and moveshard.
 */
function transactionReplication2Recovery() {
  'use strict';
  const dbn = 'UnitTestsTransactionDatabase';
  const cn = 'UnitTestsTransaction';
  const coordinator = replicatedLogsHelper.coordinators[0];
  let c = null;
  let shards = null;
  let shardId = null;
  let logId = null;

  const { setUpAll, tearDownAll, stopServerWait, continueServerWait, setUpAnd, tearDownAnd } =
    replicatedLogsHelper.testHelperFunctions(dbn, { replicationVersion: "2" });

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      c = db._create(cn, { "numberOfShards": 1, "writeConcern": 2, "replicationFactor": 3 });
      shards = c.shards();
      shardId = shards[0];
      logId = shardId.slice(1);
    }),
    tearDown: tearDownAnd(() => {
      if (c !== null) {
        c.drop();
      }
      c = null;
    }),

    testFailoverDuringTransaction: function () {
      // Start a transaction.
      let trx = db._createTransaction({
        collections: { write: c.name() }
      });
      let tc = trx.collection(c.name());
      tc.insert({ _key: 'test1', value: 1 });
      tc.insert({ _key: 'test2', value: 2 });

      // Stop the leader. This triggers a failover.
      const logs = replicatedLogsHttpHelper.listLogs(coordinator, dbn).result;
      const participants = logs[logId];
      assertTrue(participants !== undefined);
      const leader = participants[0];
      const followers = participants.slice(1);
      let term = replicatedLogsHelper.readReplicatedLogAgency(dbn, logId).plan.currentTerm.term;
      let newTerm = term + 2;

      let servers = Object.assign({}, ...participants.map(
        (serverId) => ({ [serverId]: replicatedLogsHelper.getServerUrl(serverId) })));

      // We unset the leader here so that once the old leader node is resumed we
      // do not move leadership back to that node.
      replicatedLogsHelper.unsetLeader(dbn, logId);

      // Expect none of the keys to be found on any server.
      for (const [key, endpoint] of Object.entries(servers)) {
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, "test1", false));
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, "test2", false));
      }

      stopServerWait(leader);
      replicatedLogsHelper.waitFor(replicatedLogsPredicates.replicatedLogLeaderEstablished(
        dbn, logId, newTerm, followers));

      // Check if the universal abort command appears in the log during the current term.
      let logContents = replicatedLogsHelper.dumpShardLog(shardId);
      let abortAllEntryFound = _.some(logContents, entry => {
        if (entry.logTerm !== newTerm || entry.payload === undefined) {
          return false;
        }
        return entry.payload[1].operation === "AbortAllOngoingTrx";
      });
      assertTrue(abortAllEntryFound);

      // Expect further transaction operations to fail.
      try {
        tc.insert({ _key: 'test3', value: 3 });
        fail('Insert was expected to fail due to transaction abort.');
      } catch (ex) {
        assertEqual(internal.errors.ERROR_TRANSACTION_NOT_FOUND.code, ex.errorNum);
      }

      syncShardsWithLogs(dbn);

      servers = Object.assign({}, ...followers.map(
        (serverId) => ({ [serverId]: replicatedLogsHelper.getServerUrl(serverId) })));

      // Expect none of the keys to be found on the remaining servers.
      for (const [key, endpoint] of Object.entries(servers)) {
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, "test1", false));
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, "test2", false));
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, "test3", false));
      }

      // Try a new transaction, this time expecting it to work.
      try {
        trx = db._createTransaction({
          collections: { write: c.name() }
        });
        tc = trx.collection(c.name());
        tc.insert({ _key: "foo" });
        trx.commit();
      } catch (err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      }

      servers = Object.assign({}, ...followers.map(
        (serverId) => ({ [serverId]: replicatedLogsHelper.getServerUrl(serverId) })));
      for (const [key, endpoint] of Object.entries(servers)) {
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, "foo", true));
      }

      // Resume the dead server. Expect to read "foo" from it.
      continueServerWait(leader);
      syncShardsWithLogs(dbn);
      replicatedLogsHelper.waitFor(
        replicatedStatePredicates.localKeyStatus(
          replicatedLogsHelper.getServerUrl(leader), dbn, shardId, "foo", true), 30);

      // Try another transaction. This time expect it to work on all servers.
      try {
        trx = db._createTransaction({
          collections: { write: c.name() }
        });
        tc = trx.collection(c.name());
        tc.insert({ _key: "bar" });
        trx.commit();
      } catch (err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      }

      // Expect "bar" to be found on all servers.
      servers = Object.assign({}, ...participants.map(
        (serverId) => ({ [serverId]: replicatedLogsHelper.getServerUrl(serverId) })));
      for (const [_, endpoint] of Object.entries(servers)) {
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, "bar", true));
      }

      // After the leader rejoined, expect none of the aborted transactions to have affected any servers.
      for (const [key, endpoint] of Object.entries(servers)) {
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, "test1", false));
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, "test2", false));
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, "test3", false));
      }
    },

    /**
     * This test is disabled because currently we can't properly stop the maintenance from deleting the shard
     * on the newly added follower. When the core is constructed, the follower tries to create the new shard locally,
     * but because the server is not listed as a participant for that shard in Plan/Collections, the maintenance figures
     * the shard shouldn't exist locally.
     */
    DISABLED_testFollowerReplaceDuringTransaction: function () {
      // Prepare the grounds for replacing a follower.
      const logs = replicatedLogsHttpHelper.listLogs(coordinator, dbn).result;
      const participants = logs[logId];
      assertTrue(participants !== undefined);
      const followers = participants.slice(1);
      const oldParticipant = _.sample(followers);
      const nonParticipants = _.without(replicatedLogsHelper.dbservers, ...participants);
      const newParticipant = _.sample(nonParticipants);
      const newParticipants = _.union(_.without(participants, oldParticipant), [newParticipant]).sort();

      // Start a transaction.
      let trx = db._createTransaction({
        collections: {write: c.name()}
      });
      let tc = trx.collection(c.name());
      tc.insert({_key: 'test1', value: 1});
      tc.insert({_key: 'test2', value: 2});

      // Replace the follower.
      const result = replicatedStateHelper.replaceParticipant(dbn, logId, oldParticipant, newParticipant);
      assertEqual({}, result);
      replicatedLogsHelper.waitFor(() => {
        const stateAgencyContent = replicatedStateHelper.readReplicatedStateAgency(dbn, logId);
        return replicatedLogsHelper.sortedArrayEqualOrError(
          newParticipants, Object.keys(stateAgencyContent.plan.participants).sort());
      });

      // TODO check that we cannot find test1 and test2 on any servers.
      syncShardsWithLogs(dbn);

      // Continue the transaction and expect it to succeed.
      try {
        tc.insert({_key: "test3", value: 3});
      } catch (ex) {
        fail("Transaction failed with: " + JSON.stringify(ex));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      // Expect to find the values on current participants, but not on the old participant.
      let servers = Object.assign({}, ...newParticipants.map(
        (serverId) => ({[serverId]: replicatedLogsHelper.getServerUrl(serverId)})));
      const oldEndpoint = replicatedLogsHelper.getServerUrl(oldParticipant);
      for (let cnt = 1; cnt < 4; ++cnt) {
        for (const [_, endpoint] of Object.entries(servers)) {
          replicatedLogsHelper.waitFor(
            replicatedStatePredicates.localKeyStatus(endpoint, dbn, shardId, `test${cnt}`, true, cnt));
        }
        replicatedLogsHelper.waitFor(
          replicatedStatePredicates.localKeyStatus(oldEndpoint, dbn, shardId, `test${cnt}`, false));
      }
    },
  };
}

let suites = [
  transactionReplication2Recovery,
];

for (const suite of suites) {
  jsunity.run(suite);
}

return jsunity.done();
