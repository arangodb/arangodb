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
/// @author Lars Maier
/// @author Tobias Gödderz
/// @author Copyright 2023, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {assertTrue, assertFalse} = jsunity.jsUnity.assertions;

const {
  shutdownServers,
  restartServers,
  restartAllServers,
} = (function () {
  const stoppedServers = {};

  const restartServers = function (servers) {
    for (const server of servers) {
      server.suspended = true;
      server.restartOneInstance({
        'server.authentication': 'false',
      });
      server.suspended = false;

      assertTrue(stoppedServers.hasOwnProperty(server.id));
      delete stoppedServers[server.id];
    }
  };

  const shutdownServers = function (servers) {
    for (const server of servers) {
      server.shutdownArangod(false);
    }
    for (const server of servers) {
      server.waitForInstanceShutdown(30);
      server.exitStatus = null;
      server.pid = null;
      server.suspended = true;

      assertFalse(stoppedServers.hasOwnProperty(server.id));
      stoppedServers[server.id] = server;
    }
  };

  const restartAllServers = function () {
    restartServers(Object.values(stoppedServers));
  };

  return {
    shutdownServers,
    restartServers,
    restartAllServers,
  };
})();

exports.shutdownServers = shutdownServers;
exports.restartServers = restartServers;
exports.restartAllServers = restartAllServers;
