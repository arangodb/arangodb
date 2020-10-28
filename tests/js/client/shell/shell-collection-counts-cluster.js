/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertEqual, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for inventory
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const request = require("@arangodb/request");
const _ = require("lodash");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
  
const cn = "UnitTestsCollection";

let getDBServers = function() {
  const isDBServer = (d) => (_.toLower(d.role) === 'dbserver');
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

  return global.instanceInfo.arangods.filter(isDBServer)
                              .map((server) => { 
                                return { url: endpointToURL(server), id: server.id };
                              });
};

let clearFailurePoints = function () {
  getDBServers().forEach((server) => {
    // clear all failure points
    request({ method: "DELETE", url: server.url + "/_admin/debug/failat" });
  });
};

function BaseTestConfig () {
  'use strict';
  
  return {
    testFailureOnLeaderNoManagedTrx : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 }); 
      for (let i = 0; i < 200; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      assertEqual(200, c.count());
      assertEqual(200, c.toArray().length);

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

      // break leaseManagedTrx on the leader, so it will return a nullptr
      let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/leaseManagedTrxFail", body: {} });
      assertEqual(200, result.status);

      // add a follower. this will kick off the getting-in-sync protocol,
      // which will eventually call the holdReadLockCollection API, which then
      // will call leaseManagedTrx and get the nullptr back
      c.properties({ replicationFactor: 2 });

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (tries > 20) {
          if (servers.length === 2 && c.count() === 200) {
            break;
          }
        } else if (tries === 20) {
          // wait several seconds so we can be sure the
          clearFailurePoints();
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2, servers.length);
      assertEqual(200, c.count());
      assertEqual(200, c.toArray().length);

      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 2 * 200) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 200, total);
    },

    testWrongCountOnLeaderFullSync : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 }); 
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

      // set a failure point to get the counts wrong on the leader
      let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/RocksDBCommitCounts", body: {} });
      assertEqual(200, result.status);
      
      for (let i = 100; i < 200; ++i) {
        c.insert({ _key: "test" + i }); 
      }

      assertNotEqual(200, c.count());
      assertEqual(200, c.toArray().length);
      clearFailurePoints();

      c.properties({ replicationFactor: 2 });

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 2 && c.count() === 200) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2, servers.length);
      assertEqual(200, c.count());
      assertEqual(200, c.toArray().length);

      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 2 * 200) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 200, total);
    },
    
    testWrongCountOnLeaderFullSync2 : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 }); 

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

      // set a failure point to get the counts wrong on the leader
      let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/RocksDBCommitCounts", body: {} });
      assertEqual(200, result.status);
      
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      
      assertEqual(100, c.toArray().length);
      assertNotEqual(100, c.count());

      c.properties({ replicationFactor: 2 });

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 2 && c.count() === 100) {
          break;
        }
        require("internal").sleep(0.5);
      }
      
      assertEqual(2, servers.length);
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);
     
      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 2 * 100) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 100, total);
    },
    
    testRandomCountOnLeaderFullSync : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 }); 
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

      // set a failure point to get the counts wrong on the leader
      let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/RocksDBCommitCountsRandom", body: {} });
      assertEqual(200, result.status);
      
      for (let i = 100; i < 200; ++i) {
        c.insert({ _key: "test" + i }); 
      }

      assertNotEqual(200, c.count());
      assertEqual(200, c.toArray().length);
      clearFailurePoints();

      c.properties({ replicationFactor: 2 });

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 2 && c.count() === 200) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2, servers.length);
      assertEqual(200, c.count());
      assertEqual(200, c.toArray().length);

      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 2 * 200) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 200, total);
    },
    
    testWrongCountOnLeaderFullSyncLargeCollection : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 }); 
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value: i });
      }
      for (let i = 0; i < 10; ++i) {
        c.insert(docs);
      }
      assertEqual(50000, c.count());
      assertEqual(50000, c.toArray().length);

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

      // set a failure point to get the counts wrong on the leader
      let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/RocksDBCommitCounts", body: {} });
      assertEqual(200, result.status);
      
      for (let i = 0; i < 10; ++i) {
        c.insert(docs);
      }

      assertNotEqual(100000, c.count());
      assertEqual(100000, c.toArray().length);
      clearFailurePoints();

      c.properties({ replicationFactor: 2 });

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 2 && c.count() === 100000) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2, servers.length);
      assertEqual(100000, c.count());
      assertEqual(100000, c.toArray().length);

      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 2 * 100000) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 100000, total);
    },
    
    testWrongCountOnLeaderFullSyncMultipleFollowers : function () {
      let servers = getDBServers();
      if (servers.length <= 2) {
        // we need at least 3 DB servers
        return;
      }
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 }); 
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

      // set a failure point to get the counts wrong on the leader
      let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/RocksDBCommitCounts", body: {} });
      assertEqual(200, result.status);
      
      for (let i = 100; i < 200; ++i) {
        c.insert({ _key: "test" + i }); 
      }

      assertNotEqual(200, c.count());
      assertEqual(200, c.toArray().length);
      clearFailurePoints();

      c.properties({ replicationFactor: 3 });

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 3 && c.count() === 200) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(3, servers.length);
      assertEqual(200, c.count());
      assertEqual(200, c.toArray().length);

      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 3 * 200) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(3 * 200, total);
    },
    
    testWrongCountOnLeaderFullSync2MultipleFollowers : function () {
      let servers = getDBServers();
      if (servers.length <= 2) {
        // we need at least 3 DB servers
        return;
      }
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 }); 

      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

      // set a failure point to get the counts wrong on the leader
      let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/RocksDBCommitCounts", body: {} });
      assertEqual(200, result.status);
      
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      
      assertEqual(100, c.toArray().length);
      assertNotEqual(100, c.count());

      c.properties({ replicationFactor: 3 });

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 3 && c.count() === 100) {
          break;
        }
        require("internal").sleep(0.5);
      }
      
      assertEqual(3, servers.length);
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);
     
      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 3 * 100) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(3 * 100, total);
    },
    
    testWrongCountOnLeaderFullSyncLargeCollectionMultipleFollowers : function () {
      let servers = getDBServers();
      if (servers.length <= 2) {
        // we need at least 3 DB servers
        return;
      }
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 }); 
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value: i });
      }
      for (let i = 0; i < 10; ++i) {
        c.insert(docs);
      }
      assertEqual(50000, c.count());
      assertEqual(50000, c.toArray().length);

      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

      // set a failure point to get the counts wrong on the leader
      let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/RocksDBCommitCounts", body: {} });
      assertEqual(200, result.status);
      
      for (let i = 0; i < 10; ++i) {
        c.insert(docs);
      }

      assertNotEqual(100000, c.count());
      assertEqual(100000, c.toArray().length);
      clearFailurePoints();

      c.properties({ replicationFactor: 3 });

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 3 && c.count() === 100000) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(3, servers.length);
      assertEqual(100000, c.count());
      assertEqual(100000, c.toArray().length);

      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 3 * 100000) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(3 * 100000, total);
    },
    
    testWrongCountOnFollowerFullSync : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1, syncByRevision: false, usesRevisionsAsDocumentIds: false }); 
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      // set failure points to get the counts wrong on the followers
      getDBServers().filter((server) => server.id !== leader).forEach((server) => {
        let result = request({ method: "PUT", url: server.url + "/_admin/debug/failat/RocksDBCommitCounts", body: {} });
        assertEqual(200, result.status);
      });

      c.properties({ replicationFactor: 2 });

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 2) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2, servers.length);
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);
      
      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 2 * 100) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 100, total);
    },
    
    testRandomCountOnFollowerFullSync : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 }); 
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      // set failure points to get the counts wrong on the followers
      getDBServers().filter((server) => server.id !== leader).forEach((server) => {
        let result = request({ method: "PUT", url: server.url + "/_admin/debug/failat/RocksDBCommitCountsRandom", body: {} });
        assertEqual(200, result.status);
      });

      c.properties({ replicationFactor: 2 });

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 2) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2, servers.length);
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);
     
      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 2 * 100) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 100, total);
    },
    
    testWrongCountOnLeaderIncrementalSync : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 }); 

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      assertEqual(2, shardInfo[shard].length);
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      
      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 2) {
          break;
        }
        require("internal").sleep(0.5);
      }
      
      // set a failure point to get the counts wrong on the leader
      let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/RocksDBCommitCounts", body: {} });
      assertEqual(200, result.status);
      
      // set a failure point on the leader to drop the follower
      result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/replicateOperationsDropFollower", body: {} });
      assertEqual(200, result.status);
     
      c.insert({ _key: "test100" });

      assertEqual(101, c.toArray().length);
      
      clearFailurePoints();
        
      // wait until we have an in-sync follower again
      tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        // also wait for the replication to have repaired the count on the leader
        if (servers.length === 2 && c.count() === 101) {
          break;
        }
        require("internal").sleep(0.5);
      }
      
      assertEqual(2, servers.length);
      assertEqual(101, c.count());
      assertEqual(101, c.toArray().length);
      
      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 2 * 101) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 101, total);
    },
    
    testRandomCountOnLeaderIncrementalSync : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 }); 

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      assertEqual(2, shardInfo[shard].length);
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;
      
      // set a failure point to get the counts wrong on the leader
      let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/RocksDBCommitCountsRandom", body: {} });
      assertEqual(200, result.status);

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      
      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 2) {
          break;
        }
        require("internal").sleep(0.5);
      }
      
      // set a failure point on the leader to drop the follower
      result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/replicateOperationsDropFollower", body: {} });
      assertEqual(200, result.status);
     
      c.insert({ _key: "test100" });

      assertEqual(101, c.toArray().length);
      
      clearFailurePoints();
        
      // wait until we have an in-sync follower again
      tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        // also wait for the replication to have repaired the count on the leader
        if (servers.length === 2 && c.count() === 101) {
          break;
        }
        require("internal").sleep(0.5);
      }
      
      assertEqual(2, servers.length);
      assertEqual(101, c.count());
      assertEqual(101, c.toArray().length);
      
      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 2 * 101) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 101, total);
    },
    
    testWrongCountOnFollowerIncrementalSync : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 }); 

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      assertEqual(2, shardInfo[shard].length);
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;
      let follower = shardInfo[shard][1];
      let followerUrl = servers.filter((server) => server.id === follower)[0].url;
      
      // set a failure point to get the counts wrong on the follower
      let result = request({ method: "PUT", url: followerUrl + "/_admin/debug/failat/RocksDBCommitCounts", body: {} });
      assertEqual(200, result.status);

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      
      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (servers.length === 2) {
          break;
        }
        require("internal").sleep(0.5);
      }
      
      // set a failure point on the leader to drop the follower
      result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/replicateOperationsDropFollower", body: {} });
      assertEqual(200, result.status);
     
      c.insert({ _key: "test100" });

      assertEqual(101, c.toArray().length);
      assertEqual(101, c.count());
      
      clearFailurePoints();
        
      // wait until we have an in-sync follower again
      tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        // also wait for the replication to have repaired the count on the follower
        try {
          let result = request({ method: "GET", url: followerUrl + "/_api/collection/" + shard + "/count" });
          if (servers.length === 2 && result.json.count === 101) {
            break;
          }
        } catch (err) {}
        require("internal").sleep(0.5);
      }
      
      assertEqual(2, servers.length);
      assertEqual(101, c.count());
      assertEqual(101, c.toArray().length);
     
      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({ method: "GET", url: server.url + "/_api/collection/" + shard + "/count" });
          assertEqual(200, result.status);
          total += result.json.count;
        });
        if (total === 2 * 101) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 101, total);
    },

  };
}

function collectionCountsSuiteOldFormat () {
  'use strict';

  let suite = {
    setUp : function () {
      db._drop(cn);
      clearFailurePoints();
      getDBServers().forEach((server) => {
        // this disables usage of the new collection format
        let result = request({ method: "PUT", url: server.url + "/_admin/debug/failat/disableRevisionsAsDocumentIds", body: {} });
        assertEqual(200, result.status);
      });
    },

    tearDown : function () {
      clearFailurePoints();
      db._drop(cn);
    }
  };

  deriveTestSuite(BaseTestConfig(), suite, '_OldFormat');
  return suite;
}

function collectionCountsSuiteNewFormat () {
  'use strict';

  let suite = {
    setUp : function () {
      db._drop(cn);
      clearFailurePoints();
    },

    tearDown : function () {
      clearFailurePoints();
      db._drop(cn);
    }
  };

  deriveTestSuite(BaseTestConfig(), suite, '_NewFormat');
  return suite;
}

jsunity.run(collectionCountsSuiteOldFormat);
jsunity.run(collectionCountsSuiteNewFormat);
return jsunity.done();
