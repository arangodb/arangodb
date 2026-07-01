/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertEqual, assertNotEqual */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const _ = require("lodash");
const { deriveTestSuite, getEndpointById, getMetric, waitForShardsInSync } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');

const cn = "UnitTestsCollection";
const IM = global.instanceManager;

function BaseTestConfig () {
  'use strict';
  
  return {
    testFailureOnLeaderNoManagedTrx : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 });
      let docs = [];
      for (let i = 0; i < 200; ++i) {
        docs.push({ _key: "test" + i }); 
      }
      c.insert(docs);
      assertEqual(200, c.count());
      assertEqual(200, c.toArray().length);

      let servers;
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);

      // break leaseManagedTrx on the leader, so it will return a nullptr
      leader.debugSetFailAt("leaseManagedTrxFail");

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
          // wait several seconds so we can be sure the failure point was triggered.
          IM.debugClearFailAt();
          waitForShardsInSync(cn, undefined, 1); 
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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
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
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i }); 
      }
      c.insert(docs);
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let servers = [];
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);

      // set a failure point to get the counts wrong on the leader
      leader.debugSetFailAt("RocksDBCommitCounts");
      
      for (let i = 100; i < 200; ++i) {
        c.insert({ _key: "test" + i }); 
      }

      assertNotEqual(200, c.count());
      assertEqual(200, c.toArray().length);
      IM.debugClearFailAt();

      c.properties({ replicationFactor: 2 });
      
      waitForShardsInSync(cn, undefined, 1); 

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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
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

      let servers = [];
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);

      // set a failure point to get the counts wrong on the leader
      leader.debugSetFailAt("RocksDBCommitCounts");
      
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      
      assertEqual(100, c.toArray().length);
      assertNotEqual(100, c.count());

      c.properties({ replicationFactor: 2 });
      
      waitForShardsInSync(cn, undefined, 1); 

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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
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
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i }); 
      }
      c.insert(docs);
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let servers = [];
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);

      // set a failure point to get the counts wrong on the leader
      leader.debugSetFailAt("RocksDBCommitCountsRandom");
      
      for (let i = 100; i < 200; ++i) {
        c.insert({ _key: "test" + i }); 
      }

      assertNotEqual(200, c.count());
      assertEqual(200, c.toArray().length);
      IM.debugClearFailAt();

      c.properties({ replicationFactor: 2 });
      
      waitForShardsInSync(cn, undefined, 1); 

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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
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

      let servers = [];
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);

      // set a failure point to get the counts wrong on the leader
      leader.debugSetFailAt("RocksDBCommitCounts");
      
      for (let i = 0; i < 10; ++i) {
        c.insert(docs);
      }

      assertNotEqual(100000, c.count());
      assertEqual(100000, c.toArray().length);
      IM.debugClearFailAt();

      c.properties({ replicationFactor: 2 });
      
      waitForShardsInSync(cn, undefined, 1); 

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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
        });
        if (total === 2 * 100000) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 100000, total);
    },
    
    testWrongCountOnLeaderFullSyncMultipleFollowers : function () {
      let servers = IM.getInstancesRole(instanceRole.dbserver);
      if (servers.length <= 2) {
        // we need at least 3 DB servers
        return;
      }
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 });
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i }); 
      }
      c.insert(docs);
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);
      
      // set a failure point to get the counts wrong on the leader
      leader.debugSetFailAt("RocksDBCommitCounts");
      
      for (let i = 100; i < 200; ++i) {
        c.insert({ _key: "test" + i }); 
      }

      assertNotEqual(200, c.count());
      assertEqual(200, c.toArray().length);
      IM.debugClearFailAt();
      
      c.properties({ replicationFactor: 3 });
      
      waitForShardsInSync(cn, undefined, 2); 

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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code, { shard, server, servers, result: result.parsedBody });
            total += result.parsedBody.count;
          });
        });
        if (total === 3 * 200) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(3 * 200, total);
    },
    
    testWrongCountOnLeaderFullSync2MultipleFollowers : function () {
      let servers = IM.getInstancesRole(instanceRole.dbserver);
      if (servers.length <= 2) {
        // we need at least 3 DB servers
        return;
      }
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 1 }); 

      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);

      // set a failure point to get the counts wrong on the leader
      leader.debugSetFailAt("RocksDBCommitCounts");
      
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      
      assertEqual(100, c.toArray().length);
      assertNotEqual(100, c.count());

      c.properties({ replicationFactor: 3 });

      waitForShardsInSync(cn, undefined, 2); 
      
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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code, result);
            total += result.parsedBody.count;
          });
        });
        if (total === 3 * 100) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(3 * 100, total);
    },
    
    testWrongCountOnLeaderFullSyncLargeCollectionMultipleFollowers : function () {
      let dbServers = IM.getInstancesRole(instanceRole.dbserver);
      if (dbServers.length <= 2) {
        // we need at least 3 DB servers
        return;
      }
      let servers = [];
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
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);

      // set a failure point to get the counts wrong on the leader
      leader.debugSetFailAt("RocksDBCommitCounts");
      
      for (let i = 0; i < 10; ++i) {
        c.insert(docs);
      }

      assertNotEqual(100000, c.count());
      assertEqual(100000, c.toArray().length);
      IM.debugClearFailAt();

      c.properties({ replicationFactor: 3 });
      
      waitForShardsInSync(cn, undefined, 2); 

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
        dbServers.forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
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
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i }); 
      }
      c.insert(docs);
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let servers = [];
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);
      // set failure points to get the counts wrong on the followers
      leader.debugSetFailAt("RocksDBCommitCounts");

      c.properties({ replicationFactor: 2 });
      
      waitForShardsInSync(cn, undefined, 1); 

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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
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
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i }); 
      }
      c.insert(docs);
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let servers = [];
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);
      // set failure points to get the counts wrong on the followers
      IM.getInstancesRole(instanceRole.dbserver).filter((server) => server.id !== leader).forEach((server) => {
        server.debugSetFailAt("RocksDBCommitCountsRandom");
      });

      c.properties({ replicationFactor: 2 });
      
      waitForShardsInSync(cn, undefined, 1); 

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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
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

      let servers = [];
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      assertEqual(2, shardInfo[shard].length);
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i }); 
      }
      c.insert(docs);
      
      waitForShardsInSync(cn, undefined, 1); 
      
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
      leader.debugSetFailAt("RocksDBCommitCounts");
      
      // set a failure point on the leader to drop the follower
      leader.debugSetFailAt("replicateOperationsDropFollower");
     
      c.insert({ _key: "test100" });

      assertEqual(101, c.toArray().length);
      
      IM.debugClearFailAt();
      
      waitForShardsInSync(cn, undefined, 1); 
        
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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
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

      let servers = [];
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      assertEqual(2, shardInfo[shard].length);
      let leaderID = shardInfo[shard][0];
      let leader = IM.getInstanceByID(leaderID);
      
      // set a failure point to get the counts wrong on the leader
      leader.debugSetFailAt("RocksDBCommitCountsRandom");

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      
      waitForShardsInSync(cn, undefined, 1); 
      
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
      leader.debugSetFailAt("replicateOperationsDropFollower");
     
      c.insert({ _key: "test100" });

      assertEqual(101, c.toArray().length);
      
      IM.debugClearFailAt();
      
      waitForShardsInSync(cn, undefined, 1); 
        
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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
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

      let servers = [];
      let shardInfo = c.shards(true);
      const [shard, [leaderID, followerID]] = Object.entries(shardInfo)[0];
      let leader = IM.getInstanceByID(leaderID);
      let follower = IM.getInstanceByID(followerID);
      
      // set a failure point to get the counts wrong on the follower
      follower.debugSetFailAt("RocksDBCommitCounts");

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      
      waitForShardsInSync(cn, undefined, 1); 
      
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
      leader.debugSetFailAt("replicateOperationsDropFollower");
     
      c.insert({ _key: "test100" });

      assertEqual(101, c.toArray().length);
      assertEqual(101, c.count());
      
      IM.debugClearFailAt();
      
      waitForShardsInSync(cn, undefined, 1); 
        
      // wait until we have an in-sync follower again
      tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        // also wait for the replication to have repaired the count on the follower
        try {
          let result;
          follower.toThisInstance(() => {
            result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
          });
          if (servers.length === 2 && result.parsedBody.count === 101) {
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
        IM.getInstancesRole(instanceRole.dbserver).forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          server.toThisInstance(() => {
            let result = arango.GET_RAW(`/_api/collection/${shard}/count`);
            assertEqual(200, result.code);
            total += result.parsedBody.count;
          });
        });
        if (total === 2 * 101) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2 * 101, total);
    },
   
    testWrongCountOnFollowerIncrementalSyncManyFailures : function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 }); 

      let servers = [];
      let shardInfo = c.shards(true);
      const [shard, [leaderID, followerID]] = Object.entries(shardInfo)[0];
      let leader = IM.getInstanceByID(leaderID);
      let follower = IM.getInstanceByID(followerID);
      
      // set a failure point to get the counts wrong on the follower
      follower.debugSetFailAt("RocksDBCommitCounts");

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i }); 
      }
      
      waitForShardsInSync(cn, undefined, 1); 
      
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
        
      // follower count must be broken
      let result = follower.toThisInstance(() => { return arango.GET_RAW(`/_api/collection/${shard}/count`);} );
      assertEqual(200, result.code);
      assertEqual(0, result.parsedBody.count);
      
      let checksumFailuresBefore = follower.getMetric("arangodb_sync_wrong_checksum_total");
      
      // set a failure point on the leader to drop the follower
      leader.debugSetFailAt("replicateOperationsDropFollower");
      
      // set failure points on the follower to always retry shard synchronization
      follower.debugSetFailAt("SynchronizeShard%3A%3AnoSleepOnSyncError");
      follower.debugSetFailAt("SynchronizeShard%3A%3AwrongChecksum");
      follower.debugSetFailAt("disableCountAdjustment");
      
      c.insert({ _key: "test100" });

      assertEqual(101, c.toArray().length);
      assertEqual(101, c.count());
   
      let cleanup = () => {
        follower.debugClearFailAt("SynchronizeShard%3A%3AwrongChecksum");
        follower.debugClearFailAt("disableCountAdjustment");
      };

      tries = 0;
      try {
        let checksumFailuresAfter;
        while (tries++ < 120) {
          checksumFailuresAfter = follower.getMetric("arangodb_sync_wrong_checksum_total");

          if (checksumFailuresAfter > checksumFailuresBefore) {
            break;
          }

          if (tries === 15) {
            cleanup();
          }

          require("internal").sleep(0.25);
        }
          
        assertTrue(checksumFailuresAfter > checksumFailuresBefore);
      } finally {
        cleanup();
      }

      // follower count must be ok now
      tries = 0;
      let count;
      while (tries++ < 120) {
        result = follower.toThisInstance(() => { return arango.GET_RAW(`/_api/collection/${shard}/count`); });
        assertEqual(200, result.code);
        count = result.parsedBody.count;
        
        if (count === 101) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(101, count);
    },

  };
}

function collectionCountsSuiteOldFormat () {
  'use strict';

  let suite = {
    setUp : function () {
      IM.debugClearFailAt();
      db._drop(cn);
    },

    tearDown : function () {
      IM.debugClearFailAt();
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
      IM.debugClearFailAt();
      db._drop(cn);
    },

    tearDown : function () {
      IM.debugClearFailAt();
      db._drop(cn);
    }
  };

  deriveTestSuite(BaseTestConfig(), suite, '_NewFormat');
  return suite;
}

// these tests only make sense with the old replication protocol
if (db._properties().replicationVersion !== "2") {
  jsunity.run(collectionCountsSuiteOldFormat);
  jsunity.run(collectionCountsSuiteNewFormat);
}
return jsunity.done();
