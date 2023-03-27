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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Alexandru Petenchea
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const { fail,
  assertTrue,
  assertFalse,
  assertEqual,
} = jsunity.jsUnity.assertions;

const arangodb = require('@arangodb');
const _ = require('lodash');
const db = arangodb.db;
const helper = require('@arangodb/test-helper');
const internal = require('internal');
const lh = require('@arangodb/testutils/replicated-logs-helper');
const dh = require('@arangodb/testutils/document-state-helper');
const isReplication2Enabled = internal.db._version(true).details['replication2-enabled'] === 'true';

/**
 * This test suite checks the correctness of replicated operations with respect to replicated log contents.
 * For the commit test, we additionally look at the follower contents.
 */
function transactionReplication2ReplicateOperationSuite() {
  'use strict';
  const dbn = 'UnitTestsTransactionDatabase';
  const cn = 'UnitTestsTransaction';
  const rc = lh.dbservers.length;
  var c = null;

  const {setUpAll, tearDownAll, setUpAnd, tearDownAnd} =
    lh.testHelperFunctions(dbn, {replicationVersion: "2"});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      c = db._create(cn, {"numberOfShards": 4, "writeConcern": rc, "replicationFactor": rc});
    }),
    tearDown: tearDownAnd(() => {
      if (c !== null) {
        c.drop();
      }
      c = null;
    }),

    testTransactionAbortLogEntry: function () {
      let trx = db._createTransaction({
        collections: {write: c.name()}
      });
      let tc = trx.collection(c.name());
      tc.insert({_key: 'test2', value: 2});
      trx.abort();

      let shards = c.shards();
      const shardsToLogs = lh.getShardsToLogsMapping(dbn, c._id);
      let logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));

      let allEntries = {};
      let abortCount = 0;
      for (const log of logs) {
        let entries = log.head(1000);
        allEntries[log.id()] = entries;
        for (const entry of entries) {
          if (dh.getOperationType(entry) === "Abort") {
            ++abortCount;
            break;
          }
        }
      }
      assertEqual(abortCount, 1, `Wrong count of Abort operations in the replicated logs!\n` +
        `Log entries: ${JSON.stringify(allEntries)}`);
    },

    testTransactionCommitLogEntry: function () {
      let obj = {
        collections: {
          write: [cn]
        }
      };

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc = trx.collection(cn);

        for (let cnt = 0; cnt < 100; ++cnt) {
          tc.save({_key: `foo${cnt}`});
        }
      } catch (err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          trx.commit();
        }
      }

      let shards = c.shards();
      let servers = Object.assign({}, ...lh.dbservers.map(
        (serverId) => ({[serverId]: lh.getServerUrl(serverId)})));
      const shardsToLogs = lh.getShardsToLogsMapping(dbn, c._id);
      let logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));

      for (let idx = 0; idx < logs.size; ++idx) {
        let log = logs[idx];
        let shardId = shards[idx];
        let entries = log.head(1000);
        let commitFound = false;
        let keysFound = [];

        // Gather all log entries and see if we have any inserts on this shard.
        for (const entry of entries) {
          let payload = dh.getOperationPayload(entry);
          if (payload !== null) {
            assertEqual(shardId, payload.shardId);
            let opType = dh.getOperationType(entry);
            if (opType === "Commit") {
              commitFound = true;
            } else if (opType === "Insert") {
              keysFound.push(payload.data[0]._key);
            }
          }
        }

        // Verify that there is a commit entry and no duplicates.
        if (keysFound.length > 0) {
          assertTrue(commitFound, `Could not find Commit operation in log ${log.id()}!\n` +
            `Log entries: ${JSON.stringify(entries)}`);
          const keysSet = new Set(keysFound);
          assertEqual(keysFound.length, keysSet.size, `Found duplicate keys in the replicated log${log.id()}!\n` +
            `Log entries: ${JSON.stringify(entries)}`);
        }

        // Keys should be available on the current shard only.
        let replication2Log = `Log entries: ${JSON.stringify(entries)}`;
        for (const shard in shards) {
          for (const key in keysFound) {
            let localValues = {};
            for (const [serverId, endpoint] of Object.entries(servers)) {
              lh.waitFor(
                dh.localKeyStatus(endpoint, dbn, shard, key, shard === shardId));
              localValues[serverId] = dh.getLocalValue(endpoint, dbn, shard, key);
            }
            for (const [serverId, res] of Object.entries(localValues)) {
              if (shard === shardId) {
                assertTrue(res.code === undefined,
                  `Error while reading key ${key} from ${serverId}/${dbn}/${shard}, got: ${JSON.stringify(res)}. ` +
                  `All responses: ${JSON.stringify(localValues)}` + `\n${replication2Log}`);
                assertEqual(res._key, key,
                  `Wrong key returned by ${serverId}/${dbn}/${shard}, got: ${JSON.stringify(res)}. ` +
                  `All responses: ${JSON.stringify(localValues)}` + `\n${replication2Log}`);
              } else {
                assertTrue(res.code === 404,
                  `Expected 404 while reading key ${key} from ${serverId}/${dbn}/${shard}, ` +
                  `but the response was ${JSON.stringify(res)}. All responses: ${JSON.stringify(localValues)}` +
                  `\n${replication2Log}`);
              }
            }
          }
        }
      }
    },

    testTransactionLeaderChangeBeforeCommit: function () {
      let obj = {
        collections: {
          write: [cn],
        },
      };

      const trx = db._createTransaction(obj);
      const tc = trx.collection(cn);

      tc.save({_key: 'foo'});
      tc.save({_key: 'bar'});

      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, c);

      let committed = false;
      try {
        trx.commit();
        committed = true;
      } catch (ex) {
        // The actual error code is a little bit strange, but happens due to
        // automatic retry of the commit request on the coordinator.
        assertEqual(internal.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code, ex.errorNum);
      }
      assertFalse(committed, "Transaction should not have been committed!");

      const shards = c.shards();
      const shardsToLogs = lh.getShardsToLogsMapping(dbn, c._id);
      let logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));

      const logsWithCommit = logs.filter(log => log.head(1000).some(entry => dh.getOperationType(entry) === 'Commit'));
      if (logsWithCommit.length > 0) {
        fail(`Found commit operation(s) in one or more log ${JSON.stringify(logsWithCommit[0].head(1000))}.`);
      }
    },
  };
}

/**
 * This test suite checks the correctness of replicated operations with respect to the follower contents.
 * All tests must use a single shard, so we don't have to guess where the keys are saved.
 * writeConcern must be equal to replicationFactor, so we replicate all documents on all followers.
 */
function transactionReplicationOnFollowersSuite(dbParams) {
  'use strict';
  const dbn = 'UnitTestsTransactionDatabase';
  const cn = 'UnitTestsTransaction';
  const rc = lh.dbservers.length;
  const isReplication2 = dbParams.replicationVersion === "2";
  let c = null;

  const {setUpAll, tearDownAll, setUpAnd, tearDownAnd} =
    lh.testHelperFunctions(dbn, {replicationVersion: isReplication2 ? "2" : "1"});

  return {
    setUpAll,
    tearDownAll,
    setUp: setUpAnd(() => {
      c = db._create(cn, {"numberOfShards": 1, "writeConcern": rc, "replicationFactor": rc});
    }),
    tearDown: tearDownAnd(() => {
      if (c !== null) {
        c.drop();
      }
      c = null;
    }),

    testFollowerAbort: function () {
      let trx = db._createTransaction({
        collections: {write: c.name()}
      });
      let tc = trx.collection(c.name());
      tc.insert({_key: "foo"});
      trx.abort();

      let shards = c.shards();
      let servers = Object.assign({}, ...lh.dbservers.map(
        (serverId) => ({[serverId]: lh.getServerUrl(serverId)})));
      let localValues = {};
      for (const [serverId, endpoint] of Object.entries(servers)) {
        lh.waitFor(
          dh.localKeyStatus(endpoint, dbn, shards[0], "foo", false));
        localValues[serverId] = dh.getLocalValue(endpoint, dbn, shards[0], "foo");
      }

      let replication2Log = '';
      if (isReplication2) {
        const shardsToLogs = lh.getShardsToLogsMapping(dbn, c._id);
        let log = db._replicatedLog(shardsToLogs[shards[0]]);
        let entries = log.head(1000);
        replication2Log = `Log entries: ${JSON.stringify(entries)}`;
      }
      for (const [serverId, res] of Object.entries(localValues)) {
        assertTrue(res.code === 404,
          `Expected 404 while reading key from ${serverId}/${dbn}/${shards[0]}, ` +
          `but the response was ${JSON.stringify(res)}. All responses: ${JSON.stringify(localValues)}` +
          `\n${replication2Log}`);
      }
    },

    testFollowerSaveAndCommit: function () {
      let obj = {
        collections: {
          write: [cn]
        }
      };

      let shards = c.shards();
      let servers = Object.assign({}, ...lh.dbservers.map(
        (serverId) => ({[serverId]: lh.getServerUrl(serverId)})));

      let trx;
      try {
        trx = db._createTransaction(obj);
        let tc = trx.collection(cn);
        tc.save({_key: "foo"});
      } catch (err) {
        fail("Transaction failed with: " + JSON.stringify(err));
      } finally {
        if (trx) {
          let localValues = {};
          for (const [serverId, endpoint] of Object.entries(servers)) {
            lh.waitFor(
              dh.localKeyStatus(endpoint, dbn, shards[0], "foo", false));
            localValues[serverId] = dh.getLocalValue(endpoint, dbn, shards[0], "foo");
          }

          // Make sure nothing is committed just yet
          let replication2Log = '';
          if (isReplication2) {
            const shardsToLogs = lh.getShardsToLogsMapping(dbn, c._id);
            const logId = shardsToLogs[shards[0]];
            let log = db._replicatedLog(logId);
            let entries = log.head(1000);
            replication2Log = `Log entries: ${JSON.stringify(entries)}`;
          }

          for (const [serverId, res] of Object.entries(localValues)) {
            assertTrue(res.code === 404,
              `Expected 404 while reading key from ${serverId}/${dbn}/${shards[0]}, ` +
              `but the response was ${JSON.stringify(res)}. All responses: ${JSON.stringify(localValues)}` +
              `\n${replication2Log}`);
          }

          trx.commit();
        }
      }

      let localValues = {};
      for (const [serverId, endpoint] of Object.entries(servers)) {
        lh.waitFor(
          dh.localKeyStatus(endpoint, dbn, shards[0], "foo", true));
        localValues[serverId] = dh.getLocalValue(endpoint, dbn, shards[0], "foo");
      }

      let replication2Log = '';
      if (isReplication2) {
        const shardsToLogs = lh.getShardsToLogsMapping(dbn, c._id);
        const logId = shardsToLogs[shards[0]];
        let log = db._replicatedLog(logId);
        let entries = log.head(1000);
        replication2Log = `Log entries: ${JSON.stringify(entries)}`;
      }

      for (const [serverId, res] of Object.entries(localValues)) {
        assertTrue(res.code === undefined,
          `Error while reading key from ${serverId}/${dbn}/${shards[0]}, got: ${JSON.stringify(res)}. ` +
          `All responses: ${JSON.stringify(localValues)}` + `\n${replication2Log}`);
        assertEqual(res._key, "foo",
          `Wrong key returned by ${serverId}/${dbn}/${shards[0]}, got: ${JSON.stringify(res)}. ` +
          `All responses: ${JSON.stringify(localValues)}` + `\n${replication2Log}`);
      }

      // All ids and revisions should be equal
      const revs = Object.values(localValues).map(value => value._rev);
      assertTrue(revs.every((val, i, arr) => val === arr[0]), `_rev mismatch ${JSON.stringify(localValues)}` +
        `\n${replication2Log}`);

      const ids = Object.values(localValues).map(value => value._id);
      assertTrue(ids.every((val, i, arr) => val === arr[0]), `_id mismatch ${JSON.stringify(localValues)}` +
        `\n${replication2Log}`);
    },
  };
}

function makeTestSuites(testSuite) {
  let suiteV1 = {};
  let suiteV2 = {};
  helper.deriveTestSuite(testSuite({}), suiteV1, "_V1");
  // For databases with replicationVersion 2 we internally use a ReplicatedRocksDBTransactionState
  // for the transaction. Since this class is currently not covered by unittests, these tests are
  // parameterized to cover both cases.
  helper.deriveTestSuite(testSuite({replicationVersion: "2"}), suiteV2, "_V2");
  return [suiteV1, suiteV2];
}

function transactionReplicationOnFollowersSuiteV1() {
  return makeTestSuites(transactionReplicationOnFollowersSuite)[0];
}

function transactionReplicationOnFollowersSuiteV2() {
  return makeTestSuites(transactionReplicationOnFollowersSuite)[1];
}

jsunity.run(transactionReplicationOnFollowersSuiteV1);

if (isReplication2Enabled) {
  let suites = [
    transactionReplication2ReplicateOperationSuite,
    transactionReplicationOnFollowersSuiteV2,
  ];

  for (const suite of suites) {
    jsunity.run(suite);
  }
}

return jsunity.done();
