/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertEqual, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for soft shutdown of a coordinator
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
/// @author Max Neunhoeffer
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
//const _ = require('lodash');
const pu = require('@arangodb/testutils/process-utils');
const request = require("@arangodb/request");
const internal = require("internal");
const time = internal.time;
const wait = internal.wait;
const statusExternal = internal.statusExternal;

function testSuite() {
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

  let checkAvailability = function (server) {
    require("console").warn("checking (un)availability of " + server + "...");
    let res = request({ method: "get", url: server.url + "/_api/version", timeout: 3 });
    require("console").warn("Got response:", res);
    return res.status;
  };

  return {
    testNormalSoftShutdownWithoutTraffic : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];
      let instanceInfo = global.instanceInfo;

      let newInstanceInfo = {
        arangods: [ coordinator ],
        endpoint: instanceInfo.endpoint,
      };

      // Now use soft shutdown API to shut coordinator down:
      arango.DELETE("/_admin/shutdown?soft=true");
      let startTime = time();
      while (true) {
        if (time() > startTime + 30) {
          assertTrue(false, "Coordinator did not shutdown quickly enough!");
          return;
        }
        let status = checkAvailability(coordinator);
        if (status === 500) {
          break;
        }
        wait(0.5);
      }
      wait(10);
      coordinator.exitStatus = statusExternal(coordinator.pid, false);
      delete coordinator.pid;
      require("console").warn("Coordinator:", JSON.stringify(coordinator));
      require("console").warn("instanceInfo:", JSON.stringify(instanceInfo));
      let options = global.testOptions;
      pu.reStartInstance(options, instanceInfo, {});
        
      waitForAlive(30, coordinator.url, {});
    },
    
  };
}
jsunity.run(testSuite);
return jsunity.done();
