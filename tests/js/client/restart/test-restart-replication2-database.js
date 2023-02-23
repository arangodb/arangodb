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
const rh = require('@arangodb/testutils/restart-helper');
const {db} = require('@arangodb');
const {getCtrlDBServers} = require('@arangodb/test-helper');
const console = require('console');

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
      disableMaintenanceMode();
    },

    tearDown: function () {
      db._useDatabase('_system');
      rh.restartAllServers();
      assertNotEqual(-1, db._databases().indexOf(databaseName));
      db._dropDatabase(databaseName);
      enableMaintenanceMode();
    },

    testRestartDatabaseServers: function () {
      db._createDocumentCollection(colName);
      const expectedKeys = ['42'];
      db[colName].insert(expectedKeys.map(_key => ({_key})));

      const dbServers = getCtrlDBServers();

      rh.shutdownServers(dbServers);

      rh.restartServers(dbServers);

      const actualKeys = db[colName].toArray().map(doc => doc._key);
      assertEqual(expectedKeys, actualKeys);
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
