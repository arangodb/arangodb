/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertEqual, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
const _ = require('lodash');
const pu = require('@arangodb/process-utils');
const crypto = require('@arangodb/crypto');
const request = require("@arangodb/request");
const suspendExternal = require("internal").suspendExternal;
const continueExternal = require("internal").continueExternal;
const time = require("internal").time;

function testSuite() {
  const jwtSecret = 'haxxmann';

  let getServers = function (role) {
    return global.instanceInfo.arangods.filter((instance) => instance.role === role);
  };

  let waitForAlive = function (timeout, baseurl, data) {
    let tries = 0, res;
    let all = Object.assign(data || {}, { method: "get", timeout: 1, url: baseurl + "/_api/version" }); 
    const end = time() + timeout;
    while (time() < end) {
      res = request(all);
      if (res.status === 200 || res.status === 401 || res.status === 403) {
        break;
      }
      console.warn("waiting for server response from url " + baseurl);
      require('internal').sleep(0.5);
    }
    return res.status;
  };

  let checkAvailability = function (servers, expectedCode) {
    require("console").warn("checking (un)availability of " + servers.map((s) => s.url).join(", "));
    servers.forEach(function(server) {
      let res = request({ method: "get", url: server.url + "/_api/version", timeout: 3 });
      assertEqual(expectedCode, res.status);
    });
  };

  let suspend = function (servers) {
    require("console").warn("suspending servers with pid " + servers.map((s) => s.pid).join(", "));
    servers.forEach(function(server) {
      assertTrue(suspendExternal(server.pid));
      server.suspended = true;
    });
    require("console").warn("successfully suspended servers with pid " + servers.map((s) => s.pid).join(", "));
  };
  
  let resume = function (servers) {
    require("console").warn("resuming servers with pid " + servers.map((s) => s.pid).join(", "));
    servers.forEach(function(server) {
      assertTrue(continueExternal(server.pid));
      delete server.suspended;
    });
    require("console").warn("successfully resumed servers with pid " + servers.map((s) => s.pid).join(", "));
  };

  return {
    testRestartCoordinatorNormal : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];
      let instanceInfo = global.instanceInfo;

      let newInstanceInfo = {
        arangods: [ coordinator ],
        endpoint: instanceInfo.endpoint,
      };

      let options = global.testOptions;
      let shutdownStatus = pu.shutdownInstance(newInstanceInfo, options, false); 
      coordinator.pid = null;
      assertTrue(shutdownStatus);

      let extraOptions = {
        "database.old-system-collections": "true",
        "server.jwt-secret": jwtSecret
      };
      pu.reStartInstance(options, instanceInfo, extraOptions);
      
      waitForAlive(30, coordinator.url, {});
    },
    
    testRestartCoordinatorNormalNoOldSys : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];
      let instanceInfo = global.instanceInfo;

      let newInstanceInfo = {
        arangods: [ coordinator ],
        endpoint: instanceInfo.endpoint,
      };

      let options = global.testOptions;
      let shutdownStatus = pu.shutdownInstance(newInstanceInfo, options, false); 
      coordinator.pid = null;
      assertTrue(shutdownStatus);

      let extraOptions = {
        "database.old-system-collections": "false",
        "server.jwt-secret": jwtSecret
      };
      pu.reStartInstance(options, instanceInfo, extraOptions);
        
      waitForAlive(30, coordinator.url, {});
    },
    
    testRestartCoordinatorNoDBServersNoAuthentication : function() {
      let dbServers = getServers('dbserver');
      // assume all db servers are reachable
      checkAvailability(dbServers, 200);

      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      let instanceInfo = global.instanceInfo;
      let newInstanceInfo = {
        arangods: [ coordinator ],
        endpoint: instanceInfo.endpoint,
      };

      let options = global.testOptions;
      let shutdownStatus = pu.shutdownInstance(newInstanceInfo, options, false); 
      coordinator.pid = null;
      assertTrue(shutdownStatus);

      // make db servers unavailable
      suspend(dbServers);
   
      try {
        // assume all db servers are unreachable
        checkAvailability(dbServers, 500);
        let extraOptions = {
          "database.old-system-collections": "true",
          "server.authentication": "false",
        };

        coordinator.pid = null;
        // we need this so that testing.js will not loop trying to contact the coordinator
        coordinator.suspended = true; 
        pu.reStartInstance(options, instanceInfo, extraOptions);
       
        // we expect this to run into a timeout
        let aliveStatus = waitForAlive(20, coordinator.url, {});
        assertEqual(200, aliveStatus);
      } finally {
        // make db servers available again
        coordinator.suspended = false; 
        resume(dbServers);
        
        // check that the coordinator is usable again
        let aliveStatus = waitForAlive(20, coordinator.url, {});
        assertEqual(200, aliveStatus);
      }
    },
    
    testRestartCoordinatorNoDBServersNoAuthenticationNoOldSys : function() {
      let dbServers = getServers('dbserver');
      // assume all db servers are reachable
      checkAvailability(dbServers, 200);

      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      let instanceInfo = global.instanceInfo;
      let newInstanceInfo = {
        arangods: [ coordinator ],
        endpoint: instanceInfo.endpoint,
      };

      let options = global.testOptions;
      let shutdownStatus = pu.shutdownInstance(newInstanceInfo, options, false); 
      coordinator.pid = null;
      assertTrue(shutdownStatus);

      // make db servers unavailable
      suspend(dbServers);
   
      try {
        // assume all db servers are unreachable
        checkAvailability(dbServers, 500);
        let extraOptions = {
          "database.old-system-collections": "false",
          "server.authentication": "false",
        };

        coordinator.pid = null;
        coordinator.suspended = true; 
        pu.reStartInstance(options, instanceInfo, extraOptions);
       
        // we expect the coordinator to start eventually
        let aliveStatus = waitForAlive(20, coordinator.url, {});
        assertEqual(200, aliveStatus);
      } finally {
        // make db servers available again
        coordinator.suspended = false; 
        resume(dbServers);
      }
    },
    
    testRestartCoordinatorNoDBServersAuthenticationWrongUser : function() {
      let dbServers = getServers('dbserver');
      // assume all db servers are reachable
      checkAvailability(dbServers, 200);

      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      let instanceInfo = global.instanceInfo;
      let newInstanceInfo = {
        arangods: [ coordinator ],
        endpoint: instanceInfo.endpoint,
      };

      let options = global.testOptions;
      let shutdownStatus = pu.shutdownInstance(newInstanceInfo, options, false); 
      coordinator.pid = null;
      assertTrue(shutdownStatus);

      // make db servers unavailable
      suspend(dbServers);
   
      try {
        // assume all db servers are unreachable
        checkAvailability(dbServers, 500);
        let extraOptions = {
          "database.old-system-collections": "false",
          "server.jwt-secret": jwtSecret,
          "server.authentication": "true",
        };

        coordinator.pid = null;
        coordinator.suspended = true; 
        pu.reStartInstance(options, instanceInfo, extraOptions);
        
        let jwt = crypto.jwtEncode(jwtSecret, {
          "preferred_username": "",
          "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
        }, 'HS256');
    
        let aliveStatus = waitForAlive(30, coordinator.url, { auth: { bearer: jwt } });
        assertEqual(401, aliveStatus);
      } finally {
        // make db servers available again
        coordinator.suspended = false; 
        resume(dbServers);
      }
    },
    
    testRestartCoordinatorNoDBServersAuthenticationRootUser : function() {
      let dbServers = getServers('dbserver');
      // assume all db servers are reachable
      checkAvailability(dbServers, 200);

      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      let instanceInfo = global.instanceInfo;
      let newInstanceInfo = {
        arangods: [ coordinator ],
        endpoint: instanceInfo.endpoint,
      };

      let options = global.testOptions;
      let shutdownStatus = pu.shutdownInstance(newInstanceInfo, options, false); 
      coordinator.pid = null;
      assertTrue(shutdownStatus);

      // make db servers unavailable
      suspend(dbServers);
   
      try {
        // assume all db servers are unreachable
        checkAvailability(dbServers, 500);
        let extraOptions = {
          "database.old-system-collections": "false",
          "server.jwt-secret": jwtSecret,
          "server.authentication": "true",
        };

        coordinator.pid = null;
        coordinator.suspended = true; 
        pu.reStartInstance(options, instanceInfo, extraOptions);
        
        let jwt = crypto.jwtEncode(jwtSecret, {
          "preferred_username": "root",
          "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
        }, 'HS256');
    
        let aliveStatus = waitForAlive(30, coordinator.url, { auth: { bearer: jwt } });
        // note: this should actually work, but currently doesn't TODO
        assertTrue([500, 503].indexOf(aliveStatus) !== -1, aliveStatus);
      } finally {
        // make db servers available again
        coordinator.suspended = false; 
        resume(dbServers);
      }
    },
  };
}
jsunity.run(testSuite);
return jsunity.done();
