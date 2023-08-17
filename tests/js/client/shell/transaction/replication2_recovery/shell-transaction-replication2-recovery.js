/* jshint globalstrict:false, strict:false, maxlen: 200 */

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
const internal = require('internal');
const lh = require('@arangodb/testutils/replicated-logs-helper');
const dh = require('@arangodb/testutils/document-state-helper');
const lpreds = require('@arangodb/testutils/replicated-logs-predicates');
const lhttp = require('@arangodb/testutils/replicated-logs-http-helper');
const request = require('@arangodb/request');

/**
 * In this test suite we check if the DocumentState can survive modifications to the cluster participants
 * during transactions. We check for failover and moveshard.
 */
function transactionReplication2Recovery() {
  'use strict';
  const dbn = 'UnitTestsTransactionDatabase';
  const cn = 'UnitTestsTransaction';
  const WC = 2;
  const coordinator = lh.coordinators[0];
  let c = null;
  let shards = null;
  let shardId = null;
  let logId = null;
  let logs = null;
  let shardsToLogs = null;

  const {
    setUpAll,
    tearDownAll,
    stopServerWait,
    stopServersWait,
    continueServerWait,
    continueServersWait,
    setUpAnd,
    tearDownAnd,
  } = lh.testHelperFunctions(dbn, {replicationVersion: '2'});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      c = db._create(cn, { "numberOfShards": 1, "writeConcern": WC, "replicationFactor": 3 });
      shards = c.shards();
      shardsToLogs = lh.getShardsToLogsMapping(dbn, c._id);
      logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));
      shardId = shards[0];
      logId = shardsToLogs[shardId];
    }),
    tearDown: tearDownAnd(() => {
      if (c !== null) {
        c.drop();
      }
      c = null;
    }),

    /*
     * Stop the leader in the middle of a transaction.
     */
    testFailoverDuringTransaction: function () {
      // Start a transaction.
      let trx = db._createTransaction({
        collections: { write: c.name() }
      });
      let tc = trx.collection(c.name());
      tc.insert({ _key: 'test1', value: 1 });
      tc.insert({ _key: 'test2', value: 2 });

      // Stop the leader. This triggers a failover.
      const logs = lhttp.listLogs(coordinator, dbn).result;
      const participants = logs[logId];
      assertTrue(participants !== undefined, `Could not find participants for log ${logId}`);
      const leader = participants[0];
      const followers = participants.slice(1);
      let term = lh.readReplicatedLogAgency(dbn, logId).plan.currentTerm.term;
      let newTerm = term + 2;

      let servers = Object.assign({}, ...participants.map(
        (serverId) => ({ [serverId]: lh.getServerUrl(serverId) })));

      // We unset the leader here so that once the old leader node is resumed we
      // do not move leadership back to that node.
      lh.unsetLeader(dbn, logId);

      // Expect none of the keys to be found on any server.
      for (const [key, endpoint] of Object.entries(servers)) {
        lh.waitFor(
          dh.localKeyStatus(endpoint, dbn, shardId, "test1", false));
        lh.waitFor(
          dh.localKeyStatus(endpoint, dbn, shardId, "test2", false));
      }

      // A new leader is elected.
      stopServerWait(leader);
      lh.waitFor(lpreds.replicatedLogLeaderEstablished(dbn, logId, newTerm, followers));

      // Check if the universal abort command appears in the log during the current term.
      let logContents = lh.dumpLogHead(logId);
      let abortAllEntryFound = _.some(logContents, entry => {
        return dh.getOperationType(entry) === "AbortAllOngoingTrx";
      });
      assertTrue(abortAllEntryFound, `AbortAllOngoingTrx not found in log ${logId}.` +
        ` Log contents: ${JSON.stringify(logContents)}`);

      // Expect further transaction operations to fail.
      try {
        tc.insert({ _key: 'test3', value: 3 });
        fail('Insert was expected to fail due to transaction abort.');
      } catch (ex) {
        logContents = lh.dumpLogHead(logId);
        assertEqual(internal.errors.ERROR_TRANSACTION_ABORTED.code, ex.errorNum,
          `Log ${logId} contents ${JSON.stringify(logContents)}.`);
      }

      servers = Object.assign({}, ...followers.map(
        (serverId) => ({ [serverId]: lh.getServerUrl(serverId) })));

      // Expect none of the keys to be found on the remaining servers.
      for (const [key, endpoint] of Object.entries(servers)) {
        lh.waitFor(dh.localKeyStatus(endpoint, dbn, shardId, "test1", false));
        lh.waitFor(dh.localKeyStatus(endpoint, dbn, shardId, "test2", false));
        lh.waitFor(dh.localKeyStatus(endpoint, dbn, shardId, "test3", false));
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
        logContents = lh.dumpLogHead(logId);
        fail(`Transaction failed with: ${JSON.stringify(err)}.` +
          ` Log ${logId} contents: ${JSON.stringify(logContents)}`);
      }

      servers = Object.assign({}, ...followers.map(
        (serverId) => ({ [serverId]: lh.getServerUrl(serverId) })));
      for (const [key, endpoint] of Object.entries(servers)) {
        lh.waitFor(dh.localKeyStatus(endpoint, dbn, shardId, "foo", true));
      }

      // Resume the dead server. Expect to read "foo" from it.
      continueServerWait(leader);
      lh.waitFor(dh.localKeyStatus(lh.getServerUrl(leader), dbn, shardId, "foo", true), 30);

      // Try another transaction. This time expect it to work on all servers.
      try {
        trx = db._createTransaction({
          collections: { write: c.name() }
        });
        tc = trx.collection(c.name());
        tc.insert({ _key: "bar" });
        trx.commit();
      } catch (err) {
        logContents = lh.dumpLogHead(logId);
        fail(`Transaction failed with: ${JSON.stringify(err)}.` +
          ` Log ${logId} contents: ${JSON.stringify(logContents)}`);
      }

      // Expect "bar" to be found on all servers.
      servers = Object.assign({}, ...participants.map(
        (serverId) => ({ [serverId]: lh.getServerUrl(serverId) })));
      for (const [_, endpoint] of Object.entries(servers)) {
        lh.waitFor(dh.localKeyStatus(endpoint, dbn, shardId, "bar", true));
      }

      // After the leader rejoined, expect none of the aborted transactions to have affected any servers.
      for (const [key, endpoint] of Object.entries(servers)) {
        lh.waitFor(dh.localKeyStatus(endpoint, dbn, shardId, "test1", false));
        lh.waitFor(dh.localKeyStatus(endpoint, dbn, shardId, "test2", false));
        lh.waitFor(dh.localKeyStatus(endpoint, dbn, shardId, "test3", false));
      }
    },

    /*
     * Replace a follower during a transaction.
     */
    testFollowerReplaceDuringTransaction: function () {
      // Prepare the grounds for replacing a follower.
      const logs = lhttp.listLogs(coordinator, dbn).result;
      const participants = logs[logId];
      assertTrue(participants !== undefined, `Could not find participants for log ${logId}`);
      const followers = participants.slice(1);
      const oldParticipant = _.sample(followers);
      const nonParticipants = _.without(lh.dbservers, ...participants);
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
      const result = lh.replaceParticipant(dbn, logId, oldParticipant, newParticipant);
      assertEqual({}, result);

      // Wait for replicated state to be available on the new follower.
      lh.waitFor(() => {
        const {current} = lh.readReplicatedLogAgency(dbn, logId);
        if (current.leader.hasOwnProperty("committedParticipantsConfig")) {
          return lh.sortedArrayEqualOrError(
            newParticipants,
            Object.keys(current.leader.committedParticipantsConfig.participants).sort());
        } else {
          return Error(`committedParticipantsConfig not found in ` +
            JSON.stringify(current.leader));
        }
      });

      // Expect to find no values on current participants.
      let servers = Object.assign({}, ...newParticipants.map(
        (serverId) => ({[serverId]: lh.getServerUrl(serverId)})));
      const oldEndpoint = lh.getServerUrl(oldParticipant);

      for (let cnt = 1; cnt <= 2; ++cnt) {
        for (const [_, endpoint] of Object.entries(servers)) {
          lh.waitFor(dh.localKeyStatus(endpoint, dbn, shardId, `test${cnt}`, false));
        }
        lh.waitFor(dh.localKeyStatus(oldEndpoint, dbn, shardId, `test${cnt}`, false));
      }

      // Continue the transaction and expect it to succeed.
      try {
        tc.insert({_key: "test3", value: 3});
      } catch (err) {
        const logContents = lh.dumpLogHead(logId);
        fail(`Transaction failed with: ${JSON.stringify(err)}.` +
          ` Log ${logId} contents: ${JSON.stringify(logContents)}`);
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      // Expect to find the values on current participants, but not on the old participant.
      for (let cnt = 1; cnt < 4; ++cnt) {
        for (const [_, endpoint] of Object.entries(servers)) {
          lh.waitFor(dh.localKeyStatus(endpoint, dbn, shardId, `test${cnt}`, true, cnt));
        }
        lh.waitFor(dh.localKeyStatus(oldEndpoint, dbn, shardId, `test${cnt}`, false));
      }
    },

    /*
     * We kill all followers, such that write concern cannot be reached anymore,
     * and assert that we behave as designed in this case - we can no longer insert/update/remove any documents.
     */
    testCannotReachWriteConcernDuringTransaction: function () {
      const logs = lhttp.listLogs(coordinator, dbn).result;
      const participants = logs[logId];
      assertTrue(participants !== undefined, `Could not find participants for log ${logId}`);
      const leader = participants[0];
      const followers = participants.slice(1);
      const allOtherServers = _.without(lh.dbservers, leader);

      // Start a transaction.
      let trx = db._createTransaction({
        collections: {write: c.name()}
      });

      // Stop all servers except for the leader.
      stopServersWait(allOtherServers);

      let tc = trx.collection(c.name());
      tc.insert({_key: 'test1', value: 1});

      // Commit transaction
      const coordinatorEndpoint = lh.getServerUrl(coordinator);
      const url = `/_db/${dbn}/_api/transaction/${trx._id}`;
      request.put({url: coordinatorEndpoint + url, timeout: 3});

      let leaderServer = lh.getServerUrl(leader);
      dh.localKeyStatus(leaderServer, dbn, shardId,
        "test1", false)();

      // Resume enough participants to reach write concern.
      continueServersWait(followers.slice(0, WC - 1));

      // Expect the transaction to be committed
      lh.waitFor(dh.localKeyStatus(leaderServer, dbn, shardId,
        "test1", true, 1));
      // TODO Uncomment this, after https://arangodb.atlassian.net/browse/CINFRA-668 is addressed.
      //      Currently, this can occasionally fail, if the replicated state is recreated (due to the change in
      //      RebootId) *after* the replicated log on the follower is completely up-to-date (including commit index),
      //      but *before* the latest entries have been applied to the replicated state.
      //      Because then, the leader has no reason to send new append entries requests, while the follower's log
      //      still has a freshly initialized commit index of 0.
      // for (let cnt = 0; cnt < WC - 1; ++cnt) {
      //   let server = replicatedLogsHelper.getServerUrl(followers[cnt]);
      //   replicatedLogsHelper.waitFor(replicatedStatePredicates.localKeyStatus(server, dbn, shardId,
      //     "test1", true, 1));
      // }
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
