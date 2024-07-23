/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertIdentical, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
const {db, errors} = require('@arangodb');
const lh = require('@arangodb/testutils/replicated-logs-helper');
const rh = require('@arangodb/testutils/restart-helper');
const dh = require("@arangodb/testutils/document-state-helper");
const {getCtrlDBServers} = require('@arangodb/test-helper');
const isEnterprise = require("internal").isEnterprise();

const smartGraphs = isEnterprise ? require('@arangodb/smart-graph') : null;
const eeGraphs = isEnterprise ? require('@arangodb/enterprise-graph') : null;

const dbn = 'UnitTestsDatabase';

const waitForServersToBeInCurrent = () => {
  const dbServers = getCtrlDBServers();
  lh.waitFor(function () {
    for (const server of dbServers) {
      if (!lh.isDBServerInCurrent(server.id)) {
        return Error(`server ${server.id} not in current`);
      }
    }
  });
  // Sleep a bit, to let AgencyCache see the update
  require("internal").wait(1);
};

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
        let insertEntries = dh.getDocumentEntries(dh.mergeLogs(logs), "Insert");
        require("console").error(JSON.stringify(insertEntries));
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
    * Recover the view if the log is contains entries
    */
    [`testRecoverInsertsInLog_${namePostfix}`]() {
      insertDummyData(documentCount);
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view on leader change
    */
    [`testRecoverInsertsInLogSwitchLeader_${namePostfix}`]() {
      insertDummyData(documentCount);
      // Try to switch the leader
      for (const l of logs) {
        lh.unsetLeader(dbn, l.id());
      }
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view if the log is contains entries
    */
    [`testRecoverUpdatesInLog_${namePostfix}`]() {
      insertDummyUpdates(documentCount / 10, 10);
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view on leader change
    */
    [`testRecoverUpdatesInLogSwitchLeader_${namePostfix}`]() {
      insertDummyUpdates(documentCount / 10, 10);
      // Try to switch the leader
      for (const l of logs) {
        lh.unsetLeader(dbn, l.id());
      }
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view if the log is contains entries
    */
    [`testRecoverReplacesInLog_${namePostfix}`]() {
      insertDummyReplaces(documentCount / 10, 10);
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view on leader change
    */
    [`testRecoverReplacesInLogSwitchLeader_${namePostfix}`]() {
      insertDummyReplaces(documentCount / 10, 10);
      // Try to switch the leader
      for (const l of logs) {
        lh.unsetLeader(dbn, l.id());
      }
      lh.bumpTermOfLogsAndWaitForConfirmation(dbn, collection);
      assertViewInSync();
    },

    /*
    * Recover the view if the log is contains entries
    */
    [`testRecoverInsertsInLogReboot_${namePostfix}`]() {
      insertDummyData(documentCount);
      // Reboot
      restartServers();
      assertViewInSync();
    },

    /*
    * Recover the view on reboot
    */
    [`testRecoverUpdatesInLogReboot_${namePostfix}`]() {
      insertDummyUpdates(documentCount / 10, 10);
      // Reboot
      restartServers();
      assertViewInSync();
    },

    /*
    * Recover the view on reboot
    */
    [`testRecoverReplacesInLogReboot_${namePostfix}`]() {
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
    const collection = db._create("UnitTestCollection", {replicationFactor: 2, waitForSync: true});
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
    let created = false;
    for (let i = 0; i < 5; i++) {
      try {
        smartGraphs._create(sgName, [smartGraphs._relation(edgeName, vertexName, vertexName)], [], {replicationFactor: 2, numberOfShards: 2, isSmart: true, smartGraphAttribute: "sga",  waitForSync: true});
        // One success is enough
        created = true;
        break;
      } catch (e) {
        // There is a chance that the Local AgencyCache of the Coordinator still has a dead
        // server listed. This happens if it fetches AgencyStatus exactly at the point
        // where the servers are restarted, and did not yet update it since.
        // So we simply give it some time and then retry.
        if (e.errorNum === errors.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code) {
          waitForServersToBeInCurrent();
        } else {
          throw e;
        }
      }
    }
    assertTrue(created);

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
    let created = false;
    for (let i = 0; i < 5; i++) {
      try {
        eeGraphs._create(sgName, [eeGraphs._relation(edgeName, vertexName, vertexName)], [], {replicationFactor: 2, numberOfShards: 2, isSmart: true,  waitForSync: true});
        // One success is enough
        created = true;
        break;
      } catch (e) {
        // There is a chance that the Local AgencyCache of the Coordinator still has a dead
        // server listed. This happens if it fetches AgencyStatus exactly at the point
        // where the servers are restarted, and did not yet update it since.
        // So we simply give it some time and then retry.
        if (e.errorNum === errors.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code) {
          waitForServersToBeInCurrent();
        } else {
          throw e;
        }
      }
    }
    assertTrue(created);
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

// TODO: Enable this test by default.
// For now this triggers a flaky issue and we do
// not want to turn replication 1 tests red
if (db._properties().replicationVersion === "2") {
  jsunity.run(recoveryViewOnCollection);
  if (isEnterprise) {
    jsunity.run(recoveryViewOnSmartGraph);
    jsunity.run(recoveryViewOnEnterpriseGraph);
  }
}

return jsunity.done();