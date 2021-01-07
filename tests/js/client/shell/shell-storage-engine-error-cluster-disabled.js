/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let errors = arangodb.errors;
let { getServerById, getServersByType, getEndpointById, getEndpointsByType } = require('@arangodb/test-helper');
const request = require('@arangodb/request');

/// @brief set failure point
function debugCanUseFailAt(endpoint) {
  let res = request.get({
    url: endpoint + '/_admin/debug/failat',
  });
  return res.status === 200;
}

/// @brief set failure point
function debugSetFailAt(endpoint, failAt) {
  let res = request.put({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error setting failure point";
  }
}

/// @brief remove failure point
function debugRemoveFailAt(endpoint, failAt) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure point";
  }
}

function debugClearFailAt(endpoint) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat',
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure points";
  }
}

function waitUntil(coordinator, targetStatus, id, timeout) {
  let tries = 0;
  while (++tries < timeout * 2) {
    let res = request.get({
      url: coordinator + '/_admin/cluster/health'
    });
    let health = res.json.Health;
    let servers = Object.keys(health);
    let ourServer = health[id];
    if (ourServer.Status === targetStatus) {
      return ourServer;
    }
    internal.sleep(0.5);
  }
  return {};
}

function storageEngineErrorSuite() {
  'use strict';

  return {
    testStorageEngineError: function () {
      let coordinators = getEndpointsByType("coordinator");
      let dbservers = getServersByType("dbserver");
      assertTrue(dbservers.length > 0);

      let res = request.get({
        url: coordinators[0] + '/_admin/cluster/health'
      });
     
      let health = res.json.Health;
      let servers = Object.keys(health);
      dbservers.forEach((s) => {
        assertNotEqual(-1, servers.indexOf(s.id));
      });
      servers.forEach((s) => {
        assertEqual("GOOD", health[s].Status);
      });
    
      // set failure point that triggers a storage engine error
      debugSetFailAt(getEndpointById(dbservers[0].id), "RocksDBEngine::healthCheck");
      try {
        // wait until the DB server went into status BAD
        let result = waitUntil(coordinators[0], "BAD", dbservers[0].id, 20);
        assertEqual("BAD", result.Status);
      } finally {
        // remove failure point
        getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
        // wait until it is good again
        waitUntil(coordinators[0], "GOOD", dbservers[0].id, 30);
      }
    },
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(storageEngineErrorSuite);
}
return jsunity.done();
