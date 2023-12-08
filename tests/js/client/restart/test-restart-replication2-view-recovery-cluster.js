/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertIdentical, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Arango View Recovery Tests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
// / @author Michael Hackstein
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const db = require('@arangodb').db;
const lh = require('@arangodb/testutils/replicated-logs-helper');
const rh = require('@arangodb/testutils/restart-helper');
const dh = require("@arangodb/testutils/document-state-helper");
const lp = require("@arangodb/testutils/replicated-logs-predicates");
const {getCtrlDBServers} = require('@arangodb/test-helper');
const lhttp = require('@arangodb/testutils/replicated-logs-http-helper');
const isEnterprise = require("internal").isEnterprise();

const smartGraphs = isEnterprise ? require('@arangodb/smart-graph') : null;
const eeGraphs = isEnterprise ? require('@arangodb/enterprise-graph') : null;

const dbn = 'UnitTestsDatabase';

const testSuite = (config) => {
  const documentCount = 100;

  const restartAndWaitForRebootId = function (servers) {
    const rebootIds = servers.map(s => [s.id, lh.getServerRebootId(s.id)]);
    rh.restartServers(servers);
    lh.waitFor(function () {
      for (const [server, rid] of rebootIds) {
        const current = lh.getServerRebootId(server);
        if (rid > current) {
          return Error(`server ${server} reboot id is ${current}, expected > ${rid}`);
        }
      }
    });
  };

  const disableMaintenanceMode = function () {
    const response = db._connection.PUT('/_admin/cluster/maintenance', '"off"');
    assertIdentical(false, response.error);
    assertIdentical(200, response.code);
    if (response.hasOwnProperty('warning')) {
      console.warn(response.warning);
    }
  };

  const enableMaintenanceMode = function () {
    const response = db._connection.PUT('/_admin/cluster/maintenance', '"on"');
    assertIdentical(false, response.error);
    assertIdentical(200, response.code);
    if (response.hasOwnProperty('warning')) {
      console.warn(response.warning);
    }
  };

  const restartServers = () => {
    const dbServers = getCtrlDBServers();

    enableMaintenanceMode();
    rh.shutdownServers(dbServers);

    restartAndWaitForRebootId(dbServers);
    disableMaintenanceMode();
  };


  let collection;
  let shardsToLogs;
  let shards;
  let logs;

  const {setupCollection, dropCollection, genDoc, namePostfix} = config;

  const viewName = 'UnitTestView';
  const {
    setUpAll,
    tearDownAll,
    setUpAnd,
    tearDownAnd,
  } = lh.testHelperFunctions(dbn, {replicationVersion: '2'});

  const compactLogs = () => {

    const waitForLogToBeCompacted = (log) => {
      const coordinator = lh.coordinators[0];
      const logId = log.id();

      const participants = lhttp.listLogs(coordinator, dbn).result[logId];
      assertTrue(participants !== undefined, `No participants found for log ${logId}`);
      let leader = participants[0];
      let status = log.status();
      const commitIndexAfterCompaction = status.participants[leader].response.local.commitIndex + 1;


      let logContents = log.head(1000);

      // We need to make sure that the CreateShard entries are being compacted before doing recovery.
      // Otherwise, the recovery procedure will try to create the shards again. The point is to test how
      // the recovery procedure handles operations referring to non-existing shards.
      // By waiting for lowestIndexToKeep to be greater than the current commitIndex, and for the index to be released,
      // we make sure the CreateShard operation is going to be compacted.
      lh.waitFor(lp.lowestIndexToKeepReached(log, leader, commitIndexAfterCompaction));
      // Wait for all participants to have called release on the CreateShard entry.
      lh.waitFor(() => {
        log.ping();
        status = log.status();
        for (const [pid, value] of Object.entries(status.participants)) {
          if (value.response.local.releaseIndex <= commitIndexAfterCompaction) {
            return Error(`Participant ${pid} cannot compact enough, status: ${JSON.stringify(status)}, ` +
              `log contents: ${JSON.stringify(logContents)}`);
          }
        }
        return true;
      });

      // Trigger compaction intentionally. We aim to clear a prefix of the log here, removing the CreateShard entries.
      // This way, the recovery procedure will be forced to handle entries referring to an unknown shard.
      log.compact();
    };


    // This is not too impartant, we try to get into an
    // "empty" and "filled" log with this.
    // However the filled one is the problemeatic one.
    // So this was more to proof that empty log is not a problem
    for (const l of logs) {
      // waitForLogToBeCompacted(l);
    }
  };

  const insertDummyData = (number) => {
    // NOTE: This is on purpose single calls.
    // We need those to be different entries in the replicated log
    // Otherwise we would only have a single entry for the batch
    for (let i = 0; i < number; ++i) {
      collection.save(genDoc());
    }
  };

  const insertDummyUpdates = (number, updates) => {
    // NOTE: This is on purpose single calls.
    // We need those to be different entries in the replicated log
    // Otherwise we would only have a single entry for the batch
    const docs = [];
    for (let i = 0; i < number; ++i) {
      const doc = genDoc();
      const {_key} = collection.save(doc);
      docs.push({doc, _key});
    }
    for (let i = 0; i < updates; ++i) {
      for (const {_key, doc} of docs) {
        collection.update(_key, doc);
      }
    }
  };

  const insertDummyReplaces = (number, updates) => {
    // NOTE: This is on purpose single calls.
    // We need those to be different entries in the replicated log
    // Otherwise we would only have a single entry for the batch
    const docs = [];
    for (let i = 0; i < number; ++i) {
      const doc = genDoc();
      const {_key} = collection.save(doc);
      docs.push({doc, _key});
    }
    for (let i = 0; i < updates; ++i) {
      for (const {_key, doc} of docs) {
        collection.replace(_key, doc);
      }
    }
  };

  const assertViewInSync = () => {
    const result = db._query(`FOR doc IN ${viewName} SEARCH doc.c >= 0 OPTIONS {waitForSync: true} RETURN doc._key`).toArray();
    const expectedResult = db._query(`FOR doc IN ${collection.name()} FILTER doc.c >= 0 RETURN doc._key`).toArray();

    const uniqView = new Set(result);
    const uniqCol = new Set(expectedResult);
    if (result.length !== expectedResult.length) {
      {
        const dh = require("@arangodb/testutils/document-state-helper");
        let intermediateCommitEntries = dh.getDocumentEntries(dh.mergeLogs(logs), "Insert");
        require("console").error(JSON.stringify(intermediateCommitEntries));
      }
      require("console").error(`Counts missmatch expecting: ${expectedResult.length} got: ${result.length}`);
      require("console").error(`Unique Counts expecting: ${uniqCol.size} got: ${uniqView.size}`);
      if (uniqView.size < result.length) {
        const tmp = new Set();
        // Found duplicate keys
        for (const i of result) {
          if (tmp.has(i)) {
            require("console").error(`Duplicate Key in result: ${i}`);
          } else {
            tmp.add(i);
          }
        }
      }
      for (const i of result) {
        if (!uniqCol.has(i)) {
          require("console").error(`Key in result not in collection: ${i}`);
        }
      }
    }
    assertEqual(result.length, expectedResult.length, `View: ${result.length}, Col: ${expectedResult.length}`);
  };

  return {
    setUpAll,
    tearDownAll,

    setUp: setUpAnd(() => {
      const setupInfo = setupCollection();
      collection = setupInfo.collection;
      shards = setupInfo.shards;
      shardsToLogs = setupInfo.shardsToLogs;
      logs = setupInfo.logs;

      db._dropView(viewName);
      db._createView(viewName, 'arangosearch', {});

      var meta = { links: { [collection.name()]: { includeAllFields: true } } };
      db._view(viewName).properties(meta);
    }),

    tearDown: tearDownAnd(() => {
      dropCollection();
    }),

    /*
     * Recover the view if the log is empty
     */
    [`testRecoverEmptyLog_${namePostfix}`]() {
      insertDummyData(documentCount);
      compactLogs();
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view if the log is contains entries
    */
    [`testRecoverInsertsInLog_${namePostfix}`]() {
      compactLogs();
      insertDummyData(documentCount);
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view on leader change
    */
    [`testRecoverInsertsInLogSwitchLeader_${namePostfix}`]() {
      compactLogs();
      insertDummyData(documentCount);
      // Try to switch the leader
      for (const l of logs) {
        lh.unsetLeader(dbn, l.id());
      }
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view if the log is empty
    */
    [`testRecoverEmptyLogUpdates_${namePostfix}`]() {
      insertDummyUpdates(documentCount / 10, 10);
      compactLogs();
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view if the log is contains entries
    */
    [`testRecoverUpdatesInLog_${namePostfix}`]() {
      compactLogs();
      insertDummyUpdates(documentCount / 10, 10);
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view on leader change
    */
    [`testRecoverUpdatesInLogSwitchLeader_${namePostfix}`]() {
      compactLogs();
      insertDummyUpdates(documentCount / 10, 10);
      // Try to switch the leader
      for (const l of logs) {
        lh.unsetLeader(dbn, l.id());
      }
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view if the log is empty
    */
    [`testRecoverEmptyLogReplaces_${namePostfix}`]() {
      insertDummyReplaces(documentCount / 10, 10);
      compactLogs();
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view if the log is contains entries
    */
    [`testRecoverReplacesInLog_${namePostfix}`]() {
      compactLogs();
      insertDummyReplaces(documentCount / 10, 10);
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view on leader change
    */
    [`testRecoverReplacesInLogSwitchLeader_${namePostfix}`]() {
      compactLogs();
      insertDummyReplaces(documentCount / 10, 10);
      // Try to switch the leader
      for (const l of logs) {
        lh.unsetLeader(dbn, l.id());
      }
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
 * Recover the view if the log is empty
 */
    [`testRecoverEmptyLog_${namePostfix}`]() {
      insertDummyData(documentCount);
      compactLogs();
      // Reboot
      restartServers();
      assertViewInSync();
    },

    /*
    * Recover the view if the log is contains entries
    */
    [`testRecoverInsertsInLog_${namePostfix}`]() {
      compactLogs();
      insertDummyData(documentCount);
      // Reboot
      restartServers();
      assertViewInSync();
    },

    /*
    * Recover the view on leader change
    */
    [`testRecoverInsertsInLogSwitchLeader_${namePostfix}`]() {
      compactLogs();
      insertDummyData(documentCount);
      // Reboot
      restartServers();
      assertViewInSync();
    },

    /*
    * Recover the view if the log is empty on reboot
    */
    [`testRecoverEmptyLogUpdatesReboot_${namePostfix}`]() {
      insertDummyUpdates(documentCount / 10, 10);
      compactLogs();
      // Reboot
      restartServers();
      assertViewInSync();
    },

    /*
    * Recover the view on reboot
    */
    [`testRecoverUpdatesInLogReboot_${namePostfix}`]() {
      compactLogs();
      insertDummyUpdates(documentCount / 10, 10);
      // Reboot
      restartServers();
      assertViewInSync();
    },

    /*
    * Recover the view if the log is empty on reboot
    */
    [`testRecoverEmptyLogReplaces_${namePostfix}`]() {
      insertDummyReplaces(documentCount / 10, 10);
      compactLogs();
      // Reboot
      restartServers();
      assertViewInSync();
    },


    /*
    * Recover the view on reboot
    */
    [`testRecoverReplacesInLogReboot_${namePostfix}`]() {
      compactLogs();
      insertDummyReplaces(documentCount / 10, 10);
      // Reboot
      restartServers();
      assertViewInSync();
    },
  };
};

function recoveryViewOnCollection() {
  "use strict";
  const namePostfix = "collection";
  const setupCollection = () => {
    const collection = db._create("UnitTestCollection", {replicationFactor: 3, waitForSync: true});
    const shards = collection.shards();
    const shardsToLogs = lh.getShardsToLogsMapping(dbn, collection._id);
    const logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));
    return {collection, shards, shardsToLogs, logs};
  };
  const dropCollection = () => {
    db._drop("UnitTestCollection");
  };
  function* DocGenerator() {
    let i = 0;
    while (true) {
      ++i;
      yield {_key: `${i}`, c: 1, value: i};
    }
  }
  const generator = DocGenerator();
  const genDoc = () => {
    return generator.next().value;
  };
  return testSuite({namePostfix, setupCollection, dropCollection, genDoc});
}

function recoveryViewOnSmartGraph() {
  "use strict";
  const namePostfix = "SmartGraph";
  const sgName = "UnitTestSmartGraph";

  const edgeName = "UnitTestSmartEdge";
  const vertexName = "UnitTestSmartVertex";

  const setupCollection = () => {
    smartGraphs._create(sgName, [smartGraphs._relation(edgeName, vertexName, vertexName)], [], {replicationFactor: 2, numberOfShards: 2, isSmart: true, smartGraphAttribute: "sga",  waitForSync: true});
    const collection = db._collection(edgeName);
    const localCol = db._collection(`_local_${edgeName}`);
    const shards = localCol.shards();
    const shardsToLogs = lh.getShardsToLogsMapping(dbn, localCol._id);
    const logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));
    return {collection, shards, shardsToLogs, logs};
  };
  const dropCollection = () => {
    smartGraphs._drop(sgName);
  };
  function* DocGenerator() {
    let i = 0;
    while (true) {
      ++i;
      // Local
      yield {_from: `${vertexName}/1:${i}`,  _to: `${vertexName}/1:${i}`, c: 1, value: i};
      // Remote
      yield {_from: `${vertexName}/1:${i}`,  _to: `${vertexName}/2:${i}`, c: 1, value: i};
    }
  }
  const generator = DocGenerator();
  const genDoc = () => {
    return generator.next().value;
  };
  return testSuite({namePostfix, setupCollection, dropCollection, genDoc});
}

function recoveryViewOnEnterpriseGraph() {
  "use strict";
  const namePostfix = "EEGraph";
  const sgName = "UnitTestEEGraph";

  const edgeName = "UnitTestEEEdge";
  const vertexName = "UnitTestEEVertex";

  const setupCollection = () => {
    eeGraphs._create(sgName, [eeGraphs._relation(edgeName, vertexName, vertexName)], [], {replicationFactor: 2, numberOfShards: 2, isSmart: true,  waitForSync: true});
    const collection = db._collection(edgeName);
    const localCol = db._collection(`_local_${edgeName}`);
    const shards = localCol.shards();
    const shardsToLogs = lh.getShardsToLogsMapping(dbn, localCol._id);
    const logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));
    return {collection, shards, shardsToLogs, logs};
  };
  const dropCollection = () => {
    eeGraphs._drop(sgName);
  };
  function* DocGenerator() {
    let i = 0;
    while (true) {
      ++i;
      // Local
      yield {_from: `${vertexName}/${i}`,  _to: `${vertexName}/${i}`, c: 1, value: i};
      // Remote
      yield {_from: `${vertexName}/${i}`,  _to: `${vertexName}/${i + 1}`, c: 1, value: i};
    }
  }
  const generator = DocGenerator();
  const genDoc = () => {
    const doc= generator.next().value;
    return doc;
  };
  return testSuite({namePostfix, setupCollection, dropCollection, genDoc});
}

jsunity.run(recoveryViewOnCollection);
if (isEnterprise) {
  jsunity.run(recoveryViewOnSmartGraph);
  jsunity.run(recoveryViewOnEnterpriseGraph);
}

return jsunity.done();