/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertMatch, assertTrue, assertFalse, arango, db */

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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
let { getEndpointById,
      getEndpointsByType,
      getServersByType,
      reconnectRetry,
      getCoordinators,
      getDBServers
    } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

function adminClusterSuite() {
  'use strict';

  return {

    setUp: function () {
      IM.rememberConnection();
      IM.debugClearFailAt('', instanceRole.coordinator);
    },

    tearDown: function () {
      IM.debugClearFailAt('', instanceRole.coordinator);
    },

    testRemoveServerNonExisting: function () {
      let coords = getCoordinators();
      assertTrue(coords.length > 0);
      
      // this is assumed to be an invalid server id, so the operation must fail
      try {
        reconnectRetry(coords[0].endpoint, db._name(), "root", "");
        let res = arango.POST_RAW("/_admin/cluster/removeServer", new Buffer('"testmann123456"'), {
          "content-type": "application/json"
        });
        assertEqual(404, res.code);
      } finally {
        IM.reconnectMe();
      }
    },

    testRemoveNonFailedCoordinatorString: function () {
      let coords = getCoordinators();
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;
      try {
        // make removeServer fail quickly in case precondition is not met. if we don't set this, it will cycle for 60s
        IM.debugSetFailAt("removeServer::noRetry", instanceRole.coordinator, ep);
        reconnectRetry(ep, db._name(), "root", "");
        let res = arango.POST_RAW("/_admin/cluster/removeServer",
                                  new Buffer('"' + coordinatorId + '"'), {
                                    "content-type": "application/json"
                                  });

        // as the coordinator is still in use, we expect "precondition failed"
        assertEqual(412, res.code);
      } finally {
        IM.reconnectMe();
      }
    },
    
    testRemoveNonFailedCoordinatorObject: function () {
      let coords = getCoordinators();
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;
      try {
        // make removeServer fail quickly in case precondition is not met. if we don't set this, it will cycle for 60s
        IM.debugSetFailAt("removeServer::noRetry", instanceRole.coordinator, ep);
        reconnectRetry(ep, db._name(), "root", "");
        let res = arango.POST_RAW("/_admin/cluster/removeServer",
                                  new Buffer('"' + coordinatorId + '"'), {
                                    "content-type": "application/json"
                                  });

        // as the coordinator is still in use, we expect "precondition failed"
        assertEqual(412, res.code);
      } finally {
        IM.reconnectMe();
      }
    },
    
    testRemoveNonFailedDBServersString: function () {
      let coords = getCoordinators();
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;
      try {
        // make removeServer fail quickly in case precondition is not met. if we don't set this, it will cycle for 60s
        IM.debugSetFailAt("removeServer::noRetry", instanceRole.coordinator, ep);

        let dbservers = getDBServers();
        assertTrue(dbservers.length > 0);
        dbservers.forEach((dbs) => {
          reconnectRetry(ep, db._name(), "root", "");
          let res = arango.POST_RAW("/_admin/cluster/removeServer",
                                    new Buffer('"' + dbs.id + '"'), {
                                    "content-type": "application/json"
                                  });
          // as the coordinator is still in use, we expect "precondition failed"
          assertEqual(412, res.code);
        });
      } finally {
        IM.reconnectMe();
      }
    },
    
    testRemoveNonFailedDBServersObject: function () {
      let coords = getCoordinators();
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;
      try {
        // make removeServer fail quickly in case precondition is not met. if we don't set this, it will cycle for 60s
        IM.debugSetFailAt("removeServer::noRetry", instanceRole.coordinator, ep);

        let dbservers = getDBServers();
        assertTrue(dbservers.length > 0);
        dbservers.forEach((dbs) => {
          reconnectRetry(ep, "_system", "root", "");
          let res = arango.POST_RAW("/_admin/cluster/removeServer",
                                    new Buffer('"' + dbs.id + '"'), {
                                    "content-type": "application/json"
                                  });

          // as the coordinator is still in use, we expect "precondition failed"
          assertEqual(412, res.code);
        });
      } finally {
        IM.reconnectMe();
      }
    },
    
    testCleanoutServerStringNonExisting: function () {
      let coords = getCoordinators();
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;
      try {
        // this is assumed to be an invalid server id, so the operation must fail
        reconnectRetry(ep, "_system", "root", "");
        let res = arango.POST_RAW("/_admin/cluster/cleanOutServer",
                                  new Buffer('"testmann123456"'), {
                                    "content-type": "application/json"
                                  });
        // the server expects an object with a "server" attribute, but no string
        assertEqual(400, res.code);
      } finally {
        IM.reconnectMe();
      }
    },

    testCleanoutServerObjectNonExisting: function () {
      let coords = getCoordinators();
      assertTrue(coords.length > 0);
      
      let coordinatorId = coords[0].id;
      let ep = coords[0].endpoint;
      try {
        // this is assumed to be an invalid server id, so the operation must fail
        reconnectRetry(ep, "_system", "root", "");
        let res = arango.POST_RAW("/_admin/cluster/cleanOutServer", { server: "testmann123456" });
        assertEqual(202, res.code);
        assertTrue(res.parsedBody.hasOwnProperty("id"));
        assertEqual("string", typeof res.parsedBody.id);
        assertMatch(/^\d+/, res.parsedBody.id);
        let id = res.parsedBody.id;

        // Now wait until the job is finished and check that it has failed:
        let count = 30;
        while (--count >= 0) {
          require("internal").wait(0.5, false);
          res = arango.GET_RAW("/_admin/cluster/queryAgencyJob?id=" + id);
          if (res.code !== 200 || res.parsedBody.status === "ToDo") {
            continue;
          }
          assertTrue(res.parsedBody.hasOwnProperty("creator"));
          assertTrue(res.parsedBody.hasOwnProperty("jobId"));
          assertEqual(id, res.parsedBody.jobId);
          assertTrue(res.parsedBody.hasOwnProperty("server"));
          assertEqual("testmann123456", res.parsedBody.server);
          assertTrue(res.parsedBody.hasOwnProperty("timeCreated"));
          assertTrue(res.parsedBody.hasOwnProperty("timeFinished"));
          assertTrue(res.parsedBody.hasOwnProperty("error"));
          assertFalse(res.parsedBody.error);
          assertTrue(res.parsedBody.hasOwnProperty("status"));
          assertEqual("Failed", res.parsedBody.status);
          assertTrue(res.parsedBody.hasOwnProperty("type"));
          assertEqual("cleanOutServer", res.parsedBody.type);
          return;   // all good, and job is terminated
        }
        assertEqual(200, res);   // will fail if loop ends
      } finally {
        IM.reconnectMe();
      }
    }
  };
}

if (IM.debugCanUseFailAt()) {
  // only run when failure tests are available
  jsunity.run(adminClusterSuite);
}
return jsunity.done();
