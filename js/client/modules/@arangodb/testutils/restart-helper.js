/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertEqual, arango */

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

const pu = require('@arangodb/testutils/process-utils');
const _ = require('lodash');

const {shutdownServers, restartServers, restartAllServers} = (function () {
  const getServersById = function (serverIds) {
    return global.instanceInfo.arangods.filter((instance) => _.includes(serverIds, instance.id));
  };

  const waitForAlive = function (timeout, baseurl, data) {
    const time = require("internal").time;
    const request = require("@arangodb/request");
    let res;
    let all = Object.assign(data || {}, {method: "get", timeout: 1, url: baseurl + "/_api/version"});
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

  const stoppedServers = {};

  const restartServers = function (serverIds) {
    const servers = getServersById(serverIds);
    pu.reStartInstance(global.testOptions, global.instanceInfo, {});
    for (const server of servers) {
      waitForAlive(30, server.url, {});
      delete stoppedServers[server.id];
    }
  };

  const shutdownServers = function (serverIds) {
    const servers = getServersById(serverIds);
    const newInstanceInfo = {
      arangods: servers,
      endpoint: global.instanceInfo.endpoint,
    };
    const shutdownStatus = pu.shutdownInstance(newInstanceInfo, global.testOptions, false);
    assertTrue(shutdownStatus);
    for (const server of servers) {
      server.pid = null;
      stoppedServers[server.id] = true;
    }
  };

  return {
    shutdownServers, restartServers,
    restartAllServers: function () {
      restartServers(Object.keys(stoppedServers));
    }
  };
})();

exports.shutdownServers = shutdownServers;
exports.restartServers = restartServers;
exports.restartAllServers = restartAllServers;
