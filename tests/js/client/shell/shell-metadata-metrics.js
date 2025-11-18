/*jshint globalstrict:false, strict:false */
/*global arango, assertTrue, assertEqual, assertNotEqual, fail */

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
// / @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require('internal');
const isCluster = internal.isCluster();
const { getMetric, getEndpointsByType, moveShard } = require("@arangodb/test-helper");

function metadataMetricsSuite() {
  'use strict';
  const testDbName = "testMetricsDb";
  const testCollectionName = "testMetricsCollection";
  const numberOfDatabasesMetric = "arangodb_metadata_number_of_databases";
  const numberOfCollectionsMetric = "arangodb_metadata_number_of_collections";
  const numberOfShardsMetric = "arangodb_metadata_number_of_shards";

  let assertMetrics = function(endpoints, expectedDatabases, expectedCollections, expectedShards) {
    const retries = isCluster? 3 : 1;

    endpoints.forEach((ep) => {
        let numDatabases = getMetric(ep, numberOfDatabasesMetric);
        let numCollections = getMetric(ep, numberOfCollectionsMetric);
        let numShards = 0;
        if (isCluster) {
          numShards = getMetric(ep, numberOfShardsMetric);
        }
        for (let i = 0; i < retries; ++i) {
          if (isCluster) {
            internal.sleep(1);
          }

          numDatabases = getMetric(ep, numberOfDatabasesMetric);
          if (numDatabases !== expectedDatabases) {
            continue;
          }

          numCollections = getMetric(ep, numberOfCollectionsMetric);
          if (numCollections !== expectedCollections) {
            continue;
          }

          if (isCluster) {
            numShards = getMetric(ep, numberOfShardsMetric);
            if (numShards !== expectedShards) {
              continue;
            }
          }

          // All metrics match, break early
          break;
        }

        assertEqual(numDatabases, expectedDatabases, 
                   `Number of databases found: ${numDatabases}, expected: ${expectedDatabases}`);
        assertEqual(numCollections, expectedCollections, 
                   `Number of collections found: ${numCollections}, expected: ${expectedCollections}`);
        if (isCluster) {
          assertEqual(numShards, expectedShards, 
                   `Number of shards found: ${numShards}, expected: ${expectedShards}`);
        }
    });
  };

  return {
    tearDown: function() {
      try {
        db._useDatabase("_system");
        db._dropDatabase(testDbName);
      } catch (err) {
        // Ignore errors
      }
    },

    testMetricsSimple: function() {
      let endpoints;
      
      if (isCluster) {
        endpoints = getEndpointsByType('coordinator');
      } else {
        endpoints = getEndpointsByType('single');
      }
      assertTrue(endpoints.length > 0);

      assertMetrics(endpoints, 1, 12, 12);
    },

    testMetricsChangeWithCreatingAndDroppingDatabase: function() {
      let endpoints;
      if (isCluster) {
        endpoints = getEndpointsByType('coordinator');
      } else {
        endpoints = getEndpointsByType('single');
      }
      assertTrue(endpoints.length > 0);
      assertMetrics(endpoints, 1, 12, 12);

      db._createDatabase(testDbName);
      assertMetrics(endpoints, 2, 20, 20);

      db._useDatabase("_system");
      db._dropDatabase(testDbName);
      assertMetrics(endpoints, 1, 12, 12);
    },

    testMetricsChangeWithCreatingAndDroppingCollection: function() {
      let endpoints;
      if (isCluster) {
        endpoints = getEndpointsByType('coordinator');
      } else {
        endpoints = getEndpointsByType('single');
      }
      assertTrue(endpoints.length > 0);

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, {numberOfShards: 3});
      assertMetrics(endpoints, 2, 21, 23);

      db._drop(testCollectionName);
      assertMetrics(endpoints, 2, 20, 20);

      db._useDatabase("_system");
      db._dropDatabase(testDbName);
      assertMetrics(endpoints, 1, 12, 12);
    },

    testMetricsChangeWithCreatingAndDroppingCollectionWithReplication: function() {
      let endpoints;
      if (isCluster) {
        endpoints = getEndpointsByType('coordinator');
      } else {
        endpoints = getEndpointsByType('single');
      }
      assertTrue(endpoints.length > 0);

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, {numberOfShards: 5, replicationFactor: 3});
      assertMetrics(endpoints, 2, 21, 25);

      db._drop(testCollectionName);
      assertMetrics(endpoints, 2, 20, 20);

      db._useDatabase("_system");
      db._dropDatabase(testDbName);
      assertMetrics(endpoints, 1, 12, 12);
    },

    testMetricsWithSingleShardDatabase: function() {
      let endpoints;
      if (isCluster) {
        endpoints = getEndpointsByType('coordinator');
      } else {
        endpoints = getEndpointsByType('single');
      }
      assertTrue(endpoints.length > 0);

      db._createDatabase(testDbName, {sharding: "single"});
      db._useDatabase(testDbName);
      db._create(testCollectionName, {numberOfShards: 3});
      assertMetrics(endpoints, 2, 21, 21);

      db._drop(testCollectionName);
      assertMetrics(endpoints, 2, 20, 20);

      db._useDatabase("_system");
      db._dropDatabase(testDbName);
      assertMetrics(endpoints, 1, 12, 12);
    },

    testMetricsWithSatelliteCollection: function() {
      let endpoints;
      if (isCluster) {
        endpoints = getEndpointsByType('coordinator');
      } else {
        endpoints = getEndpointsByType('single');
      }
      assertTrue(endpoints.length > 0);

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, {replicationFactor: "satellite"});
      assertMetrics(endpoints, 2, 21, 21);
    },

    testMetricsSwitchLeaderFollower: function() {
      if (!isCluster) {
        // Shard movement only makes sense in cluster mode
        return;
      }

      const endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      const col = db._create(testCollectionName, {numberOfShards: 3});
      
      assertMetrics(endpoints, 2, 21, 23);

      // Get shard information - shards(true) returns server IDs
      const shards = col.shards(true);
      
      const shardId = Object.keys(shards)[0];
      // leader
      const fromServer = shards[shardId][0];
      // follower
      const toServer = shards[shardId][1];
      assertNotEqual(fromServer, toServer);

      // Move the shard (swap leader and follower) and wait for completion
      const moveResult = moveShard(testDbName, testCollectionName, shardId, 
                                   fromServer, toServer, false);
      assertTrue(moveResult);
      assertMetrics(endpoints, 2, 21, 23);

      // Cleanup
      db._drop(testCollectionName);
      assertMetrics(endpoints, 2, 20, 20);

      db._useDatabase("_system");
      db._dropDatabase(testDbName);
      assertMetrics(endpoints, 1, 12, 12);
    },

    testMetricsMoveToNewServer: function() {
      if (!isCluster) {
        // Shard movement only makes sense in cluster mode
        return;
      }

      const endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      const col = db._create(testCollectionName, {numberOfShards: 2});
      
      assertMetrics(endpoints, 2, 21, 22);

      const health = arango.GET("/_admin/cluster/health");
      const allDbServers = Object.keys(health.Health).filter(s => s.startsWith('PRMR-'));
      
      const shards = col.shards(true);
      
      const shardId = Object.keys(shards)[0];
      const currentServers = shards[shardId]; // Array of servers holding this shard
      const fromServer = currentServers[0]; // Leader
      
      // Find a DB server that doesn't currently hold this shard
      const toServer = allDbServers.find(server => !currentServers.includes(server));
      assertTrue(toServer);
      assertNotEqual(fromServer, toServer);

      // Move the shard to a new DB server and wait for completion
      const moveResult = moveShard(testDbName, testCollectionName, shardId, 
                                   fromServer, toServer, false);
      assertTrue(moveResult);
      assertMetrics(endpoints, 2, 21, 22);

      // Cleanup
      db._drop(testCollectionName);
      assertMetrics(endpoints, 2, 20, 20);

      db._useDatabase("_system");
      db._dropDatabase(testDbName);
      assertMetrics(endpoints, 1, 12, 12);
    },
  };
}

jsunity.run(metadataMetricsSuite);
return jsunity.done();
