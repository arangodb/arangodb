/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertMatch, assertTrue, assertFalse */

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

const jsunity = require('jsunity');
const { getEndpointById, getEndpointsByType, getServersByType } = require('@arangodb/test-helper');
const request = require('@arangodb/request');

function endpointToURL(endpoint) {
  if (endpoint.substr(0, 6) === 'ssl://') {
    return 'https://' + endpoint.substr(6);
  }
  let pos = endpoint.indexOf('://');
  if (pos === -1) {
    return 'http://' + endpoint;
  }
  return 'http' + endpoint.substr(pos);
}

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

function debugClearFailAt(endpoint) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat',
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure points";
  }
}

function adminClusterSuite() {
  'use strict';

  return {

    setUp: function () {
      getEndpointsByType("coordinator").forEach((ep) => debugClearFailAt(ep));
    },

    tearDown: function () {
      getEndpointsByType("coordinator").forEach((ep) => debugClearFailAt(ep));
    },
    
    testRemoveServerNonExisting: function () {
      let coords = getEndpointsByType('coordinator');
      assertTrue(coords.length > 0);
      
      // this is assumed to be an invalid server id, so the operation must fail
      let res = request.post({ url: coords[0] + "/_admin/cluster/removeServer", body: "testmann123456", json: true });
      assertEqual(404, res.status);
    },
    
    testRemoveNonFailedCoordinatorString: function () {
      let coords = getServersByType('coordinator');
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;
      // make removeServer fail quickly in case precondition is not met. if we don't set this, it will cycle for 60s
      debugSetFailAt(endpointToURL(ep), "removeServer::noRetry");
      let res = request.post({ url: endpointToURL(ep) + "/_admin/cluster/removeServer", body: coordinatorId, json: true });
      // as the coordinator is still in use, we expect "precondition failed"
      assertEqual(412, res.status);
    },
    
    testRemoveNonFailedCoordinatorObject: function () {
      let coords = getServersByType('coordinator');
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;
      // make removeServer fail quickly in case precondition is not met. if we don't set this, it will cycle for 60s
      debugSetFailAt(endpointToURL(ep), "removeServer::noRetry");
      let res = request.post({ url: endpointToURL(ep) + "/_admin/cluster/removeServer", body: { server: coordinatorId }, json: true });
      // as the coordinator is still in use, we expect "precondition failed"
      assertEqual(412, res.status);
    },
    
    testRemoveNonFailedDBServersString: function () {
      let coords = getServersByType('coordinator');
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;
      // make removeServer fail quickly in case precondition is not met. if we don't set this, it will cycle for 60s
      debugSetFailAt(endpointToURL(ep), "removeServer::noRetry");

      let dbservers = getServersByType('dbserver');
      assertTrue(dbservers.length > 0);
      dbservers.forEach((dbs) => {
        let res = request.post({ url: endpointToURL(ep) + "/_admin/cluster/removeServer", body: dbs.id, json: true });
        // as the coordinator is still in use, we expect "precondition failed"
        assertEqual(412, res.status);
      });
    },
    
    testRemoveNonFailedDBServersObject: function () {
      let coords = getServersByType('coordinator');
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;
      // make removeServer fail quickly in case precondition is not met. if we don't set this, it will cycle for 60s
      debugSetFailAt(endpointToURL(ep), "removeServer::noRetry");

      let dbservers = getServersByType('dbserver');
      assertTrue(dbservers.length > 0);
      dbservers.forEach((dbs) => {
        let res = request.post({ url: endpointToURL(ep) + "/_admin/cluster/removeServer", body: { server: dbs.id }, json: true });
        // as the coordinator is still in use, we expect "precondition failed"
        assertEqual(412, res.status);
      });
    },
    
    testCleanoutServerStringNonExisting: function () {
      let coords = getServersByType('coordinator');
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;

      // this is assumed to be an invalid server id, so the operation must fail
      let res = request.post({ url: endpointToURL(ep) + "/_admin/cluster/cleanOutServer", body: "testmann123456", json: true });
      // the server expects an object with a "server" attribute, but no string
      assertEqual(400, res.status);
    },
    
    testCleanoutServerObjectNonExisting: function () {
      let coords = getServersByType('coordinator');
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;

      // this is assumed to be an invalid server id, so the operation must fail
      let res = request.post({ url: endpointToURL(ep) + "/_admin/cluster/cleanOutServer", body: { server: "testmann123456" }, json: true });
      assertEqual(202, res.status);
      assertTrue(res.json.hasOwnProperty("id"));
      assertEqual("string", typeof res.json.id);
      assertMatch(/^\d+/, res.json.id);
      let id = res.json.id;

      // Now wait until the job is finished and check that it has failed:
      let count = 10;
      while (--count >= 0) {
        require("internal").wait(1.0, false);
        res = request.get({ url: endpointToURL(ep) + "/_admin/cluster/queryAgencyJob?id=" + id, json: true });
        if (res.status !== 200) {
          continue;
        }
        assertTrue(res.json.hasOwnProperty("creator"));
        assertTrue(res.json.hasOwnProperty("jobId"));
        assertEqual(id, res.json.jobId);
        assertTrue(res.json.hasOwnProperty("server"));
        assertEqual("testmann123456", res.json.server);
        assertTrue(res.json.hasOwnProperty("timeCreated"));
        assertTrue(res.json.hasOwnProperty("timeFinished"));
        assertTrue(res.json.hasOwnProperty("error"));
        assertFalse(res.json.error);
        assertTrue(res.json.hasOwnProperty("status"));
        assertEqual("Failed", res.json.status);
        assertTrue(res.json.hasOwnProperty("type"));
        assertEqual("cleanOutServer", res.json.type);
        return;   // all good, and job is terminated
      }
      assertEqual(200, res);   // will fail if loop ends
    },
   
  };
}

let ep = getEndpointsByType('coordinator');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only run when failure tests are available
  jsunity.run(adminClusterSuite);
}
return jsunity.done();
