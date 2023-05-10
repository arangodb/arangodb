////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
/// @author Tobias Gödderz
/// @author Copyright 2023, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {assertEqual, assertNotEqual, assertIdentical, assertTrue} = jsunity.jsUnity.assertions;
const {db, errors} = require('@arangodb');
const console = require('console');
const rh = require('@arangodb/testutils/restart-helper');
const lh = require("@arangodb/testutils/replicated-logs-helper");
const lp = require("@arangodb/testutils/replicated-logs-predicates");
const dh = require("@arangodb/testutils/document-state-helper");
const {getCtrlDBServers} = require('@arangodb/test-helper');
const {sleep} = require('internal');
const _ = require('lodash');

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

const compareAllDocuments = function (col, expectedKeys) {
  let actualKeys = null;
  for (let tries = 0; tries < 60 && actualKeys === null; ++tries) {
    try {
      actualKeys = col.toArray().map(doc => doc._key);
    } catch (err) {
      console.warn(err);
      if (err.errorNum === errors.ERROR_CLUSTER_CONNECTION_LOST.code) {
        // Sometimes Windows is just slower, give it a little time
        console.log('Will retry');
        sleep(1);
      } else {
        throw err;
      }
    }
  }
  assertNotEqual(actualKeys, null);
  assertEqual(expectedKeys, _.sortBy(actualKeys, _.toNumber));
};

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

function testSuite() {
  const databaseName = 'replication2_restart_test_db';
  const colName = 'replication2_restart_test_collection';

  return {

    setUp: function () {
      db._createDatabase(databaseName, {'replicationVersion': '2'});
      db._useDatabase(databaseName);
    },

    tearDown: function () {
      disableMaintenanceMode();
      db._useDatabase('_system');
      rh.restartAllServers();
      assertNotEqual(-1, db._databases().indexOf(databaseName));
      db._dropDatabase(databaseName);
    },

    testSimpleRestartAllDatabaseServers: function () {
      const col = db._createDocumentCollection(colName, {numberOfShards: 5, replicationFactor: 2});
      const expectedKeys = _.range(100).map(i => `${i}`);
      col.insert(expectedKeys.map(_key => ({_key})));

      const dbServers = getCtrlDBServers();

      enableMaintenanceMode();
      rh.shutdownServers(dbServers);

      restartAndWaitForRebootId(dbServers);
      disableMaintenanceMode();

      lh.waitFor(lp.allReplicatedStatesConverged(databaseName));

      compareAllDocuments(col, expectedKeys);
      // Insert another document to check if the collection is writable (it should)
      col.insert({_key: "another-document"});
    },

    testRestartAllReplicationFactorOne: function () {
      const col = db._createDocumentCollection(colName, {numberOfShards: 5, replicationFactor: 1});
      const expectedKeys = _.range(100).map(i => `${i}`);
      col.insert(expectedKeys.map(_key => ({_key})));

      const dbServers = getCtrlDBServers();

      enableMaintenanceMode();
      rh.shutdownServers(dbServers);

      restartAndWaitForRebootId(dbServers);
      disableMaintenanceMode();

      lh.waitFor(lp.allReplicatedStatesConverged(databaseName));

      compareAllDocuments(col, expectedKeys);
      // Insert another document to check if the collection is writable (it should)
      col.insert({_key: "another-document"});
    },

    testRestartLeader: function () {
      const col = db._createDocumentCollection(colName, {numberOfShards: 1, replicationFactor: 2});
      const expectedKeys = _.range(100).map(i => `${i}`);
      col.insert(expectedKeys.map(_key => ({_key})));

      const [shardId] = col.shards();
      const logId = lh.getShardsToLogsMapping(databaseName, col._id)[shardId];
      const logStatus = db._replicatedLog(logId).status();
      const leaderId = logStatus.leaderId;
      const dbServers = getCtrlDBServers();
      const leader = dbServers.find(server => server.id === leaderId);

      enableMaintenanceMode();
      rh.shutdownServers([leader]);

      restartAndWaitForRebootId([leader]);
      disableMaintenanceMode();

      lh.waitFor(lp.replicatedStateConverged(databaseName, logId));

      compareAllDocuments(col, expectedKeys);
      // Insert another document to check if the collection is writable (it should)
      col.insert({_key: "another-document"});
    },

    testRestartLeaderDistributeShardsLike: function () {
      const col = db._createDocumentCollection(colName, {numberOfShards: 1, replicationFactor: 2});
      const expectedKeys = _.range(100).map(i => `${i}`);
      col.insert(expectedKeys.map(_key => ({_key})));

      const [shardId] = col.shards();
      const logId = lh.getShardsToLogsMapping(databaseName, col._id)[shardId];
      const logStatus = db._replicatedLog(logId).status();
      const leaderId = logStatus.leaderId;
      const dbServers = getCtrlDBServers();
      const leader = dbServers.find(server => server.id === leaderId);

      // trigger a compaction to get rid of the create-collection entry
      db._replicatedLog(logId).compact();

      enableMaintenanceMode();
      rh.shutdownServers([leader]);

      restartAndWaitForRebootId([leader]);
      disableMaintenanceMode();

      lh.waitFor(lp.replicatedStateConverged(databaseName, logId));

      // query for shards on that server
      const assocShards = dh.getAssociatedShards(lh.getServerUrl(leaderId), databaseName, logId);
      assertEqual([shardId], assocShards);
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();
