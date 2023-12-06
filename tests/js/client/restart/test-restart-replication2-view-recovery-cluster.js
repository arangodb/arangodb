/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertIdentical */

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
const {getCtrlDBServers} = require('@arangodb/test-helper');


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

  const dbn = 'UnitTestsDatabase';
  const viewName = 'UnitTestView';
  const {
    setUpAll,
    tearDownAll,
    setUpAnd,
    tearDownAnd,
  } = lh.testHelperFunctions(dbn, {replicationVersion: '2'});

  const compactLogs = () => {
    for (const l of logs) {
      l.compact();
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
        const {logs} = dh.getCollectionShardsAndLogs(db, collection);
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
      collection = setupCollection();
      shards = collection.shards();
      shardsToLogs = lh.getShardsToLogsMapping(dbn, collection._id);
      logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));

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
      require("console").error("Unsetting the leader");
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
      require("console").error("Unsetting the leader");
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
      insertDummyUpdates(documentCount / 10, 10);
      compactLogs();
      require("console").error("Unsetting the leader");
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view if the log is contains entries
    */
    [`testRecoverReplacesInLog_${namePostfix}`]() {
      compactLogs();
      insertDummyUpdates(documentCount / 10, 10);
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view on leader change
    */
    [`testRecoverReplacesInLogSwitchLeader_${namePostfix}`]() {
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
      insertDummyUpdates(documentCount / 10, 10);
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
      insertDummyUpdates(documentCount / 10, 10);
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
    return db._create("UnitTestCollection", {replicationFactor: 3});
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

jsunity.run(recoveryViewOnCollection);

return jsunity.done();