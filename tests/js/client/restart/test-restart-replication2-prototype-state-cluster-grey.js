/*jshint globalstrict:false, strict:false */
/* global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
/// @author Copyright 2022, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const _ = require('lodash');
const rh = require('@arangodb/testutils/restart-helper');
const {db, errors} = require('@arangodb');
const { getCtrlDBServers } = require('@arangodb/test-helper');

const retryWithExceptions = function (check) {
  let i = 0, lastError = null;
  while (true) {
    try {
      check();
      break;
    } catch (err) {
      lastError = err;
    }
    i += 1;
    if (i >= 100) {
      throw Error("repeated error conditions: " + lastError);
    }
    require('internal').sleep(1);
  }
};

const getSnapshotStatus = function (logId) {
  const response = db._connection.GET(`/_api/replicated-state/${logId}/snapshot-status`);
  if (response.code !== 200) {
    throw new Error("Snapshot status returned invalid response code: " + response.code);
  }
  return response.result;
};

const disableMaintenanceMode = function () {
  const response = db._connection.PUT("/_admin/cluster/maintenance", "\"off\"");
};

function testSuite() {


  return {
    tearDown: function () {
      rh.restartAllServers();
    },

    testRestartDatabaseServers: function () {
      disableMaintenanceMode();
      const dbServers = getCtrlDBServers();
      const servers = _.sampleSize(dbServers, 3);
      const state = db._createPrototypeState({servers});
      state.write({"foo": "bar"});

      const sstatus = getSnapshotStatus(state.id());

      rh.shutdownServers(dbServers);
      rh.restartAllServers();

      retryWithExceptions(function () {
        assertEqual(state.read("foo"), "bar");
      });

      // we expect the snapshost status to be unchanged
      assertEqual(sstatus, getSnapshotStatus(state.id()));
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
