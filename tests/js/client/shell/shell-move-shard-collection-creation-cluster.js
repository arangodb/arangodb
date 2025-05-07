/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
const request = require("@arangodb/request");
let db = arangodb.db;

const {
  getDBServers
} = require('@arangodb/test-helper');

function moveShardCreateCollectionSuite() {
  'use strict';
  const cn = 'UnitTestsMoveShardCreateCollection1';
  const cn2 = 'UnitTestsMoveShardCreateCollection2';

  return {
    setUp: function () {
      db._create(cn, {replicationFactor:2});
    },

    tearDown: function () {
      try {
        db._drop(cn2);
      } catch (err) {
      }
      try {
        db._drop(cn);
      } catch (err) {
      }
    },
    
    testMoveShardAndCreateCollectionDistributedShardsLike: function() {
      // Start a transaction and insert a document
      const trx = db._createTransaction({collections:{write:[cn]}});
      const trxCol = trx.collection(cn);
      trxCol.insert({_key: "test1", value: 1});
      // Intentionally do not commit yet, we want this to be going on!
      
      // Get the shard information
      const shardInfo = arango.GET("/_admin/cluster/shardDistribution");
      const shardId = Object.keys(shardInfo.results[cn].Plan)[0];
      const fromServer = shardInfo.results[cn].Plan[shardId].leader;
      const toServer = shardInfo.results[cn].Plan[shardId].followers[0];
      
      // Schedule MoveShard operation
      const moveShardResult = arango.POST("/_admin/cluster/moveShard", {
        database: "_system",
        collection: cn,
        shard: shardId,
        fromServer: fromServer,
        toServer: toServer
      });
      const moveShardId = moveShardResult.id;

      // Let the job begin: Check if MoveShard operation is pending:
      let moveShardPending = false;
      let maxWait = 30; // 30 seconds timeout
      while (maxWait > 0) {
        const jobStatus = arango.GET(`/_admin/cluster/queryAgencyJob?id=${moveShardId}`);
        if (jobStatus.status === "Pending") {
          moveShardPending = true;
          break;
        }
        internal.wait(1);
        maxWait--;
      }
      assertTrue(moveShardPending, "MoveShard operation should become pending");

      // Try to create second collection in background
      const createJob = arango.POST_RAW("/_api/collection", {
        name: cn2,
        replicationFactor: 2,
        distributeShardsLike: cn
      }, {"x-arango-async": "store"});
      // This will now be blocked (unless we have a regression), so wait
      // for a few seconds and confirm it is still ongoing:
      internal.wait(3);
      const jobId = createJob.headers["x-arango-async-id"];
      const jobStatus = arango.GET(`/_api/job/${jobId}`);
      assertEqual(jobStatus.code, 204, "Collection creation should be blocked");

      // Furthermore, confirm that the MoveShard operation is still ongoing:
      const moveShardStatus = arango.GET(`/_admin/cluster/queryAgencyJob?id=${moveShardId}`);
      assertEqual(moveShardStatus.status, "Pending");

      // Commit transaction
      trx.commit();
      
      // Check if create operation completed
      let createJobCompleted = false;
      maxWait = 30; // 30 seconds timeout
      while (maxWait > 0) {
        const jobStatus = arango.GET(`/_api/job/${jobId}`);
        if (jobStatus.code !== 204) {
          assertEqual(jobStatus.code, 200, "Create collection operation should have been successful");
          createJobCompleted = true;
          break;
        }
        internal.wait(1);
        maxWait--;
      }
      assertTrue(createJobCompleted, "Create collection operation should complete after transaction commit");

      // Wait for collection to be visible:
      maxWait = 30; // 30 seconds timeout
      while (maxWait > 0) {
        try {
          const col = db._collection(cn2);
          if (col !== null) {
            break;
          }
        } catch (err) {
          // Collection not ready yet
        }
        internal.wait(1);
        maxWait--;
      }
      assertTrue(db._collection(cn2) !== null, "Collection should be visible after transaction commit");
      
      // Check if MoveShard operation finished
      let moveShardCompleted = false;
      maxWait = 30; // 30 seconds timeout
      while (maxWait > 0) {
        const jobStatus = arango.GET(`/_admin/cluster/queryAgencyJob?id=${moveShardId}`);
        if (jobStatus.status === "Finished") {
          moveShardCompleted = true;
          break;
        }
        internal.wait(1);
        maxWait--;
      }
      assertTrue(moveShardCompleted, "MoveShard operation should complete after transaction commit");
    },
    
    testMoveShardWithDelaysAndCreateCollection: function() {
      // Get the shard information
      const shardInfo = arango.GET("/_admin/cluster/shardDistribution");
      const shardId = Object.keys(shardInfo.results[cn].Plan)[0];
      const fromServer = shardInfo.results[cn].Plan[shardId].leader;
      const toServer = shardInfo.results[cn].Plan[shardId].followers[0];
      
      // Set failure points on the follower
      const dbservers = getDBServers();
      let endpoints = {};
      for (let dbs of dbservers) {
        endpoints[dbs.shortName] = dbs.url;
        endpoints[dbs.id] = dbs.url;
      }
      try {
        let result = request({ method: "PUT", url: endpoints[toServer] + "/_admin/debug/failat/DelayCreateShard15", body: {} });
        assertEqual(200, result.status);
        result = request({ method: "PUT", url: endpoints[toServer] + "/_admin/debug/failat/DelayTakeoverShardLeadership15", body: {} });
        assertEqual(200, result.status);
        
        // Schedule MoveShard operation
        const moveShardResult = arango.POST("/_admin/cluster/moveShard", {
          database: "_system",
          collection: cn,
          shard: shardId,
          fromServer: fromServer,
          toServer: toServer
        });
        const moveShardId = moveShardResult.id;

        // Let the job begin:
        internal.wait(1);
        
        // Check if MoveShard operation is pending:
        let moveShardPending = false;
        let maxWait = 30; // 30 seconds timeout
        while (maxWait > 0) {
          const jobStatus = arango.GET(`/_admin/cluster/queryAgencyJob?id=${moveShardId}`);
          if (jobStatus.status === "Pending") {
            moveShardPending = true;
            break;
          }
          internal.wait(1);
          maxWait--;
        }
        assertTrue(moveShardPending, "MoveShard operation should become pending");
        // Try to create second collection in background
        const createJob = arango.POST_RAW("/_api/collection", {
          name: cn2,
          replicationFactor: 2,
          distributeShardsLike: cn
        }, {"x-arango-async": "store"});
        
        // This will be blocked by the delays, wait for a few seconds to confirm
        internal.wait(3);
        const jobId = createJob.headers["x-arango-async-id"];
        const jobStatus = arango.GET(`/_api/job/${jobId}`);
        assertEqual(jobStatus.code, 204, "Collection creation should be blocked");
        // Furthermore, confirm that the MoveShard operation is still ongoing:
        const moveShardStatus = arango.GET(`/_admin/cluster/queryAgencyJob?id=${moveShardId}`);
        assertEqual(moveShardStatus.status, "Pending");

        // Stop the delays:
        result = request({ method: "PUT", url: endpoints[toServer] + "/_admin/debug/failat/DontDelayCreateShard15", body: {} });
        assertEqual(200, result.status);
        result = request({ method: "PUT", url: endpoints[toServer] + "/_admin/debug/failat/DontDelayTakeoverShardLeadership15", body: {} });
        assertEqual(200, result.status);

        // Check if create operation completed
        let createJobCompleted = false;
        maxWait = 30; // 30 seconds timeout
        while (maxWait > 0) {
          const jobStatus = arango.GET(`/_api/job/${jobId}`);
          if (jobStatus.code !== 204) {
            assertEqual(jobStatus.code, 200, "Create collection operation should have been successful");
            createJobCompleted = true;
            break;
          }
          internal.wait(1);
          maxWait--;
        }
        assertTrue(createJobCompleted, "Create collection operation should complete after delays expire");

        // Wait for collection to be visible:
        maxWait = 30; // 30 seconds timeout
        while (maxWait > 0) {
          try {
            const col = db._collection(cn2);
            if (col !== null) {
              break;
            }
          } catch (err) {
            // Collection not ready yet
          }
          internal.wait(1);
          maxWait--;
        }
        assertTrue(db._collection(cn2) !== null, "Collection should be visible after delays expire");
        
        // Check if MoveShard operation finished successfully
        let moveShardCompleted = false;
        maxWait = 30; // 30 seconds timeout
        while (maxWait > 0) {
          const jobStatus = arango.GET(`/_admin/cluster/queryAgencyJob?id=${moveShardId}`);
          if (jobStatus.status === "Finished") {
            moveShardCompleted = true;
            break;
          }
          internal.wait(1);
          maxWait--;
        }
        assertTrue(moveShardCompleted, "MoveShard operation should complete after delays expire");
      } finally {
        let result = request({ method: "DELETE", url: endpoints[toServer] + "/_admin/debug/failat"});
        assertEqual(200, result.status);
      }
    },
    
  };
}

jsunity.run(moveShardCreateCollectionSuite);
return jsunity.done();
