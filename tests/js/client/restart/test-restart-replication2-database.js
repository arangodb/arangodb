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
const {assertEqual, assertNotEqual, assertIdentical} = jsunity.jsUnity.assertions;
const {db} = require('@arangodb');
const console = require('console');
const rh = require('@arangodb/testutils/restart-helper');
const lh = require("@arangodb/testutils/replicated-logs-helper");
const {getCtrlDBServers} = require('@arangodb/test-helper');
const {dbName} = require('@arangodb/testutils/user-helper');
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

function testSuite () {
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
      const col = db._createDocumentCollection(colName, {numberOfShards: 5, replicationFactor: 3});
      const expectedKeys = _.range(100).map(i => `${i}`);
      col.insert(expectedKeys.map(_key => ({_key})));

      const dbServers = getCtrlDBServers();

      enableMaintenanceMode();
      rh.shutdownServers(dbServers);

      rh.restartServers(dbServers);
      disableMaintenanceMode();

      const actualKeys = col.toArray().map(doc => doc._key);
      assertEqual(expectedKeys, _.sortBy(actualKeys, _.toNumber));
    },

    testRestartAllReplicationFactorOne: function () {
      const col = db._createDocumentCollection(colName, {numberOfShards: 5, replicationFactor: 1});
      const expectedKeys = _.range(100).map(i => `${i}`);
      col.insert(expectedKeys.map(_key => ({_key})));

      const dbServers = getCtrlDBServers();

      enableMaintenanceMode();
      rh.shutdownServers(dbServers);

      rh.restartServers(dbServers);
      disableMaintenanceMode();

      const actualKeys = col.toArray().map(doc => doc._key);
      assertEqual(expectedKeys, _.sortBy(actualKeys, _.toNumber));
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

      rh.restartServers([leader]);
      disableMaintenanceMode();

      const actualKeys = col.toArray().map(doc => doc._key);
      assertEqual(expectedKeys, _.sortBy(actualKeys, _.toNumber));
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
