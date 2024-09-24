/* global arango */
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
/// @author Alexandru Petenchea
/// @author Copyright 2024, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {assertEqual, assertIdentical, fail} = jsunity.jsUnity.assertions;
const {db, errors} = require('@arangodb');
const console = require('console');
const request = require("@arangodb/request");
const rh = require('@arangodb/testutils/restart-helper');
const {getCtrlDBServers} = require('@arangodb/test-helper');
const {sleep, time} = require('internal');

const disableMaintenanceMode = function () {
  const response = arango.PUT('/_admin/cluster/maintenance', '"off"');
  assertIdentical(false, response.error, JSON.stringify(response));
  assertIdentical(200, response.code, JSON.stringify(response));
  if (response.hasOwnProperty('warning')) {
    console.warn(response.warning);
  }
};
const enableMaintenanceMode = function () {
  const response = arango.PUT('/_admin/cluster/maintenance', '"on"');
  assertIdentical(false, response.error, JSON.stringify(response));
  assertIdentical(200, response.code, JSON.stringify(response));
  if (response.hasOwnProperty('warning')) {
    console.warn(response.warning);
  }
};

function testSuite() {
  const databaseNameR1 = 'replication1_restart_test_db';
  const databaseNameR2 = 'replication2_restart_test_db';
  let dbServer = null;

  let waitForAlive = function (timeout, baseurl, data) {
    let res;
    let all = Object.assign(data || {}, { method: "get", timeout: 1, url: baseurl + "/_api/version" });
    const end = time() + timeout;
    while (time() < end) {
      res = request(all);
      if (res.status === 200 || res.status === 401 || res.status === 403) {
        break;
      }
      console.warn(`waiting for server response from url ${baseurl} - got ${JSON.stringify(res)}`);
      sleep(0.5);
    }
    return res;
  };

  return {
    setUp: function () {
      dbServer = getCtrlDBServers()[0];
    },

    tearDown: function () {
      disableMaintenanceMode();
      db._useDatabase('_system');
      rh.restartAllServers();
      if (db._databases().indexOf(databaseNameR1) !== -1) {
        db._dropDatabase(databaseNameR1);
      }
      if (db._databases().indexOf(databaseNameR2) !== -1) {
        db._dropDatabase(databaseNameR2);
      }
    },

    testRestartReplicationAndCreate: function () {
      if (db._properties().replicationVersion === "2") {
        // This test is not applicable if there are already existing replication2 databases.
        return;
      }
      db._createDatabase(databaseNameR1, {'replicationVersion': '1'});
      let conn = arango.getConnectionHandle();
      enableMaintenanceMode();
      dbServer.exitStatus = null;
      dbServer.shutdownArangod(false);
      dbServer.waitForInstanceShutdown(30);
      dbServer.pid = null;
      dbServer.exitStatus = null;

      // Restart should be possible if there are only replication1 databases
      dbServer.restartOneInstance({
        "server.authentication": "false",
        "rocksdb.sync-interval": 0
      });
      let aliveStatus = waitForAlive(30, dbServer.url, {});
      assertEqual(200, aliveStatus.status, JSON.stringify(aliveStatus));
      arango.connectHandle(conn);
      // We should not be able to create a replication2 database if the syncer thread is off
      try {
        db._createDatabase(databaseNameR2, {'replicationVersion': '2'});
        fail("Should not be able to create a database with replication version 2");
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE.code, err.errorNum);
      }

      // Turning the syncer thread on again should allow us to create a replication2 database
      dbServer.exitStatus = null;
      dbServer.shutdownArangod(false);
      dbServer.waitForInstanceShutdown(30);
      dbServer.pid = null;
      dbServer.exitStatus = null;
      dbServer.restartOneInstance({
        "server.authentication": "false",
        "rocksdb.sync-interval": 100
      });
      aliveStatus = waitForAlive(30, dbServer.url, {});
      assertEqual(200, aliveStatus.status, JSON.stringify(aliveStatus));
      arango.connectHandle(conn);

      disableMaintenanceMode();

      const end = time() + 30;
      do {
        try {
          db._createDatabase(databaseNameR2, {'replicationVersion': '2'});
          break;
        } catch (err) {
          if (err.errorNum !== errors.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code) {
            throw err;
          }
          sleep(0.5);
        }
      }
      while (time() < end);
    },

    testRestartDisabled: function () {
      let conn = arango.getConnectionHandle();
      db._createDatabase(databaseNameR2, {'replicationVersion': '2'});

      // It should not be possible to start a cluster with the syncer thread off, if there are replication2 databases
      enableMaintenanceMode();
      dbServer.exitStatus = null;
      dbServer.shutdownArangod(false);
      dbServer.waitForInstanceShutdown(30);
      dbServer.pid = null;
      dbServer.exitStatus = null;

      try {
        dbServer.options.enableAliveMonitor = false;
        dbServer.restartOneInstance({
          "server.authentication": "false",
          "rocksdb.sync-interval": 0
        });
        fail("Should not be able to restart using a database with replication version 2");
      } catch (err) {
        // expected
      } finally {
        dbServer.exitStatus = null;
        dbServer.pid = null;
        dbServer.options.enableAliveMonitor = true;
        dbServer.restartOneInstance({
          "server.authentication": "false",
          "rocksdb.sync-interval": 100
        });

        // Things should be back to normal now
        let aliveStatus = waitForAlive(30, dbServer.url, {});
        assertEqual(200, aliveStatus.status, JSON.stringify(aliveStatus));
      }

      arango.connectHandle(conn);
      disableMaintenanceMode();
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();
