/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertEqual, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for Foxx app self-heal
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
const pu = require('@arangodb/testutils/process-utils');
const crypto = require('@arangodb/crypto');
const request = require("@arangodb/request");
const suspendExternal = require("internal").suspendExternal;
const continueExternal = require("internal").continueExternal;
const time = require("internal").time;
const path = require('path');
const FoxxManager = require('@arangodb/foxx/manager');
const basePath = path.resolve(require("internal").pathForTesting('common'), 'test-data', 'apps', 'perdb1');

const originalEndpoint = arango.getEndpoint();
const originalUser = arango.connectedUser();

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
      
  const mount = '/test';

  return {
    setUp : function() {
      // make sure self heal has run, otherwise we may not be able to install
      arango.POST(`/_admin/execute`, "require('@arangodb/foxx/manager').healAll(); return 1");
     
      let tries = 0;
      while (++tries < 10) {
        try {
          FoxxManager.install(basePath, mount);
          break;
        } catch (err) {
          // for some reason we get sporadic connection errors when trying to install Foxx apps.
          // this seems to be a timing issue
          require('internal').sleep(0.5);
        }
      }
    },

    tearDown : function() {
      arango.reconnect(originalEndpoint, "_system", originalUser, "");
      // make sure self heal has run, otherwise we may not be able to uninstall
      let res = arango.POST(`/_admin/execute`, "require('@arangodb/foxx/manager').healAll(); return 1");
      assertEqual("1", res);
        
      // uninstall Foxx app at the end
      FoxxManager.uninstall(mount, {force: true});
    },

    /*
      install A Foxx App.
      Suspend DBServers
      Request Foxx on this Coordinator => Service Unavailable.
      Unsuspend DBServers
      Retry accessing Foxx => Eventually the service will get available.
     */

    testRequestFoxxAppWithoutSelfHeal : function() {
      let dbServers = getServers('dbserver');
      // assume all db servers are reachable
      checkAvailability(dbServers, 200);
        
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      // make db servers unavailable
      suspend(dbServers);
      
      // restart coordinator
      try {
        let instanceInfo = global.instanceInfo;

        let newInstanceInfo = {
          arangods: [ coordinator ],
          endpoint: instanceInfo.endpoint,
        };

        let options = global.testOptions;
        // shut down and restart coordinator.
        // on this coordinator, the self-heal cannot run successfully because
        // the DB servers are down
        let shutdownStatus = pu.shutdownInstance(newInstanceInfo, options, false); 
        coordinator.pid = null;
        assertTrue(shutdownStatus);

        let extraOptions = {
          "foxx.force-update-on-startup": "false",
          "foxx.queues": "false",
          "server.jwt-secret": jwtSecret
        };
        pu.reStartInstance(options, instanceInfo, extraOptions);
          
        waitForAlive(30, coordinator.url, {});
 
        // try to request Foxx app. the app must be inaccessible, because self-heal
        // hasn't run yet
        let res = request({ method: "get", timeout: 3, url: coordinator.url + `/${mount}/echo` }); 
        assertEqual(503, res.status);

      } finally {
        resume(dbServers);
      }
      
      // make sure self heal has run, otherwise we may not be able to access the app
      arango.reconnect(originalEndpoint, "_system", originalUser, "");
      let res = arango.POST(`/_admin/execute`, "require('@arangodb/foxx/manager').healAll(); return 1");
      assertEqual("1", res);
        
      res = request({ method: "get", timeout: 3, url: coordinator.url + `/${mount}/echo` }); 
      assertEqual(200, res.status);
    },
    
    testRequestFoxxAppWithForcedSelfHeal : function() {
      let dbServers = getServers('dbserver');
      // assume all db servers are reachable
      checkAvailability(dbServers, 200);

      // restart coordinator
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];
      let instanceInfo = global.instanceInfo;

      let newInstanceInfo = {
        arangods: [ coordinator ],
        endpoint: instanceInfo.endpoint,
      };

      let options = global.testOptions;
      // shut down and restart coordinator.
      // on this coordinator, the self-heal cannot run successfully because
      // the DB servers are down
      let shutdownStatus = pu.shutdownInstance(newInstanceInfo, options, false); 
      coordinator.pid = null;
      assertTrue(shutdownStatus);

      let extraOptions = {
        "foxx.force-update-on-startup": "false",
        "foxx.queues": "false",
        "server.jwt-secret": jwtSecret
      };
      pu.reStartInstance(options, instanceInfo, extraOptions);
        
      waitForAlive(30, coordinator.url, {});
      
      // make sure self heal has run 
      arango.reconnect(originalEndpoint, "_system", originalUser, "");
      let res = arango.POST(`/_admin/execute`, "require('@arangodb/foxx/manager').healAll(); return 1");
      assertEqual("1", res);

      // try to request Foxx app. the app must be accessible now, because we ran self-heal
      // just now
      res = request({ method: "get", timeout: 3, url: coordinator.url + `/${mount}/echo` }); 
      assertEqual(200, res.status);
    },
    
    testRequestFoxxAppWithSelfHealAtStartup : function() {
      let dbServers = getServers('dbserver');
      // assume all db servers are reachable
      checkAvailability(dbServers, 200);

      // restart coordinator
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];
      let instanceInfo = global.instanceInfo;

      let newInstanceInfo = {
        arangods: [ coordinator ],
        endpoint: instanceInfo.endpoint,
      };

      let options = global.testOptions;
      // shut down and restart coordinator.
      // on this coordinator, the self-heal cannot run successfully because
      // the DB servers are down
      let shutdownStatus = pu.shutdownInstance(newInstanceInfo, options, false); 
      coordinator.pid = null;
      assertTrue(shutdownStatus);

      let extraOptions = {
        "foxx.force-update-on-startup": "true",
        "foxx.queues": "false",
        "server.jwt-secret": jwtSecret
      };
      pu.reStartInstance(options, instanceInfo, extraOptions);
        
      waitForAlive(30, coordinator.url, {});

      // try to request Foxx app. the app must be accessible, because of self-heal
      // at startup
      let tries = 0;
      let res;
      while (++tries < 30) {
        res = request({ method: "get", timeout: 3, url: coordinator.url + `/${mount}/echo` }); 
        if (res.status === 200) {
          break;
        }
        require('internal').sleep(0.5);
      }
      assertEqual(200, res.status);
    },
    
  };
}
jsunity.run(testSuite);
return jsunity.done();
