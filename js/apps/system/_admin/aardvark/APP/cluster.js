'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2013 triAGENS GmbH, Cologne, Germany
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const cluster = require('@arangodb/cluster');
const createRouter = require('@arangodb/foxx/router');

const router = createRouter();
module.exports = router;


router.use((req, res, next) => {
  if (internal.authenticationEnabled()) {
    if (!req.arangoUser) {
      res.throw('unauthorized');
    }
  }
  next();
});

router.get('/amICoordinator', function(req, res) {
  res.json(cluster.isCoordinator());
})
.summary('Ask the server whether it is a coordinator')
.description('This will return true if and only if the server is a coordinator.');

if (cluster.isCluster()) {
  router.get('/DBServers', function(req, res) {
    const list = global.ArangoClusterInfo.getDBServers();
    res.json(list.map(n => { 
      var r = { "id": n.serverId, "name": n.serverName, "role": "primary" };
      r.status = "ok";
      const endpoint = global.ArangoClusterInfo.getServerEndpoint(r.id);
      const proto = endpoint.substr(0, 6);
      if (proto === "tcp://") {
        r.protocol = "http";
        r.address = endpoint.substr(6);
      } else if (proto === "ssl://") {
        r.protocol = "https";
        r.address = endpoint.substr(6);
      } else {
        r.endpoint = endpoint;
      }
      return r;
    }));
  })
  .summary('Get all DBServers')
    .description('Get a list of all running and expected DBServers within the cluster');

  router.get('/Coordinators', function(req, res) {
    const list = global.ArangoClusterInfo.getCoordinators();
    res.json(list.map(n => { 
      var r = { "id": n, "role": "coordinator" };
      r.status = "ok";
      const endpoint = global.ArangoClusterInfo.getServerEndpoint(n);
      const proto = endpoint.substr(0, 6);
      if (proto === "tcp://") {
        r.protocol = "http";
        r.address = endpoint.substr(6);
      } else if (proto === "ssl://") {
        r.protocol = "https";
        r.address = endpoint.substr(6);
      } else {
        r.endpoint = endpoint;
      }
      return r;
    }));
  });
}
