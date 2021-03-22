/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const db = arangodb.db;
const request = require("@arangodb/request");

let getServers = function(role) {
  const isRole = (d) => (d.role.toLowerCase() === role);
  const endpointToURL = (server) => {
    let endpoint = server.endpoint;
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    let pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  return global.instanceInfo.arangods.filter(isRole)
                              .map((server) => { 
                                return { url: endpointToURL(server), id: server.id };
                              });
};

function replicationClientsSuite() {
  'use strict';
  const cn = 'UnitTestsCollection';
      
  let getClients = (shardLeaderUrl, maxIterations) => {
    let clients;
    let iterations = 0;
    while (++iterations < maxIterations) {
      let result = request({ method: "GET", url: shardLeaderUrl + "/_api/replication/logger-state", body: {} });
      clients = result.json.clients;
      assertTrue(Array.isArray(clients));
      if (clients.length === 0) {
        break;
      }
      require("internal").sleep(0.5);
    }
    return clients;
  };

  return {
    testNoClientsThenIncreaseReplicationFactor: function () {
      const dbservers = getServers("dbserver");
      const findServer = (id) => dbservers.filter((s) => s.id === id)[0];
      
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }

      db._createDatabase(cn);
      db._useDatabase(cn);
      try {
        let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 });
        c.insert(docs);
        
        let shards = c.shards(true);
        let shardName = Object.keys(shards)[0];
        let shardServers = shards[shardName];
        let shardLeader = shardServers[0];

        // leader should have 0 clients registered
        assertEqual([], getClients(findServer(shardLeader).url + "/_db/" + cn, 300));

        // add a follower
        c.properties({ replicationFactor: 2 });
       
        // wait until follower is ready
        let iterations = 0;
        while (++iterations < 150) {
          shards = c.shards(true);
          if (shards[shardName].length === 2) {
            break;
          }
          require("internal").sleep(0.5);
        }
        assertEqual(2, shards[shardName].length);
        let shardFollower = shards[shardName][1];
        assertNotEqual(shardLeader, shardFollower);
        
        // leader should have 0 clients registered
        assertEqual([], getClients(findServer(shardLeader).url + "/_db/" + cn, 300));
        // follower should have 0 clients registered
        assertEqual([], getClients(findServer(shardFollower).url + "/_db/" + cn, 300));
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },
    
    testNoClientsWithFollowerFromTheStart: function () {
      const dbservers = getServers("dbserver");
      const findServer = (id) => dbservers.filter((s) => s.id === id)[0];
      
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }

      db._createDatabase(cn);
      db._useDatabase(cn);
      try {
        let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
        c.insert(docs);
        
        let shards = c.shards(true);
        let shardName = Object.keys(shards)[0];
        assertEqual(2, shards[shardName].length);
        let shardServers = shards[shardName];
        let shardLeader = shardServers[0];
        let shardFollower = shards[shardName][1];
        assertNotEqual(shardLeader, shardFollower);
        
        // leader should have 0 clients registered
        assertEqual([], getClients(findServer(shardLeader).url + "/_db/" + cn, 300));
        // follower should have 0 clients registered
        assertEqual([], getClients(findServer(shardFollower).url + "/_db/" + cn, 300));
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },
    
    testNoClientsMultipleDatabaseMultipleCollections: function () {
      const dbservers = getServers("dbserver");
      assertTrue(dbservers.length > 1);

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }
      try {
        // set up 5 databases with 5 collections each
        for (let i = 0; i < 5; ++i) {
          db._useDatabase("_system");
          db._createDatabase(cn + i);
          db._useDatabase(cn + i);
          for (let j = 0; j < 10; ++j) {
            let c = db._create(cn + j, { numberOfShards: i + 1, replicationFactor: 1 });
            c.insert(docs);
          }
        }

        // now add followers for all collections!
        for (let i = 0; i < 5; ++i) {
          db._useDatabase(cn + i);
          for (let j = 0; j < 5; ++j) {
            db._collection(cn + j).properties({ replicationFactor: 1 + (i % dbservers.length) });
          }
        }

        // wait until they all have all their replicas in place
        for (let i = 0; i < 5; ++i) {
          db._useDatabase(cn + i);
          for (let j = 0; j < 5; ++j) {
            let c = db._collection(cn + j);
            let shards = c.shards(true);
            assertEqual(i + 1, Object.keys(shards).length);

            let totalFollowerShards;
            let expectedFollowerShards;
            let iterations = 0;
            while (++iterations < 150) {
              totalFollowerShards = 0;
              expectedFollowerShards = 0;
              shards = c.shards(true);
              for (let k = 0; k < i + 1; ++k) {
                totalFollowerShards += Object.values(shards)[k].length - 1;
                expectedFollowerShards += (i % dbservers.length); 
              }
              if (expectedFollowerShards === totalFollowerShards) {
                break;
              }
              require("internal").sleep(0.5);
            }
            assertEqual(expectedFollowerShards, totalFollowerShards);
          }
        }

        // no server should have any clients registered, in none of the dbs
        dbservers.forEach((dbs) => {
          for (let i = 0; i < 5; ++i) {
            assertEqual([], getClients(dbs.url + "/_db/" + cn + i, 300));
          }
        });

      } finally {
        // clean it all up
        db._useDatabase("_system");
        for (let i = 0; i < 5; ++i) {
          try {
            db._dropDatabase(cn + i);
          } catch (err) {}
        }
      }
    },

  };
}

jsunity.run(replicationClientsSuite);
return jsunity.done();
