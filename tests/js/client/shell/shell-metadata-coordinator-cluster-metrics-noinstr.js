/*jshint globalstrict:false, strict:false */
/*global arango, assertTrue, assertEqual, assertNotEqual, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2025 ArangoDB Hyderabad, India
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
// / @author Avanthi Dundigala
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require('internal');
const { getCoordinators, moveShard, getDBServers, eventuallyAssertMetric } = require("@arangodb/test-helper");

function metadataCoordinatorMetricsSuite() {
  'use strict';
  const testDbName = "testMetricsCoordinator";
  const testCollectionName = "testMetricsCollection";

  // Metric name constants
  const numberOfShardsMetric = "arangodb_metadata_number_of_shards";
  const totalNumberOfShardsMetric = "arangodb_metadata_total_number_of_shards";
  const numberFollowerShardsMetric = "arangodb_metadata_number_follower_shards";
  const numberOutOfSyncShardsMetric = "arangodb_metadata_number_out_of_sync_shards";
  const numberNotReplicatedShardsMetric = "arangodb_metadata_number_not_replicated_shards";
  const shardFollowersOutOfSyncMetric = "arangodb_metadata_shard_followers_out_of_sync_number";

  // Helper: compute total shard count across all databases (shards * replicationFactor)
  const getTotalShardCount = function() {
    const currentDb = db._name();
    let count = 0;
    db._useDatabase("_system");
    db._databases().forEach((database) => {
      db._useDatabase(database);
      db._collections().forEach((col) => {
        const props = col.properties();
        count += props.numberOfShards * props.replicationFactor;
      });
    });
    db._useDatabase(currentDb);
    return count;
  };

  // Helper: compute leader shard count across all databases (just numberOfShards)
  const getLeaderShardCount = function() {
    const currentDb = db._name();
    let count = 0;
    db._useDatabase("_system");
    db._databases().forEach((database) => {
      db._useDatabase(database);
      db._collections().forEach((col) => {
        count += col.properties().numberOfShards;
      });
    });
    db._useDatabase(currentDb);
    return count;
  };

  // Helper: compute follower shard count (total - leaders)
  const getFollowerShardCount = function() {
    return getTotalShardCount() - getLeaderShardCount();
  };

  // Helper: assert all shard metrics eventually match expected values
  const assertAllShardMetrics = function(endpoint, expectedLeaderShards,
                                         expectedTotalShards, expectedFollowerShards,
                                         expectedNotReplicated, expectedOutOfSync,
                                         expectedFollowersOutOfSync) {
    eventuallyAssertMetric(
      endpoint, numberOfShardsMetric,
      (value) => value === expectedLeaderShards,
      `leader shards expected: ${expectedLeaderShards}`
    );
    eventuallyAssertMetric(
      endpoint, totalNumberOfShardsMetric,
      (value) => value === expectedTotalShards,
      `total shards expected: ${expectedTotalShards}`
    );
    eventuallyAssertMetric(
      endpoint, numberFollowerShardsMetric,
      (value) => value === expectedFollowerShards,
      `follower shards expected: ${expectedFollowerShards}`
    );
    eventuallyAssertMetric(
      endpoint, numberNotReplicatedShardsMetric,
      (value) => value === expectedNotReplicated,
      `not replicated shards expected: ${expectedNotReplicated}`
    );
    eventuallyAssertMetric(
      endpoint, numberOutOfSyncShardsMetric,
      (value) => value === expectedOutOfSync,
      `out of sync shards expected: ${expectedOutOfSync}`
    );
    eventuallyAssertMetric(
      endpoint, shardFollowersOutOfSyncMetric,
      (value) => value === expectedFollowersOutOfSync,
      `followers out of sync expected: ${expectedFollowersOutOfSync}`
    );
  };

    // Helper function to generate documents that cover all shards
    const generateDocsForAllShards = function(collection, numberOfShards, docsPerShard) {
      let shardMap = {};
      let docsToInsert = [];
      const batchSize = 1000;
      let keyIndex = 0;

      while (Object.keys(shardMap).length < numberOfShards ||
             Object.values(shardMap).some(count => count < docsPerShard)) {

        const query = `
          FOR i IN @from..@to
            RETURN { key: CONCAT('key', i), shardId: SHARD_ID(@collection, { _key: CONCAT('test', i) }) }
        `;
        const results = db._query(query, {
          'from': keyIndex,
          'to': keyIndex + batchSize,
          'collection': collection.name()
        }).toArray();

        // Process the batch results
        for (const result of results) {
          const { key, shardId } = result;

          if (!shardMap[shardId]) {
            shardMap[shardId] = 0;
          }

          if (shardMap[shardId] < docsPerShard) {
            shardMap[shardId]++;
            docsToInsert.push({ _key: key });
          }

          // Early exit if we've satisfied all shards
          if (Object.keys(shardMap).length >= numberOfShards &&
              Object.values(shardMap).every(count => count >= docsPerShard)) {
            break;
          }
        }

        keyIndex += batchSize;
      }

      return docsToInsert;
    };


  return {
    tearDown: function() {
      try {
        db._useDatabase("_system");
        db._dropDatabase(testDbName);
      } catch (err) {
        // ignore
      }
    },

    testMetricsBaseValues: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);

      // Compute expected base values from collection definitions
      const expectedLeaderShards = getLeaderShardCount();
      const expectedTotalShards = getTotalShardCount();
      const expectedFollowerShards = getFollowerShardCount();

      // In a healthy baseline state, no shards should be out of sync
      assertAllShardMetrics(coordinators[0].endpoint, expectedLeaderShards, expectedTotalShards,
                            expectedFollowerShards, 0, 0, 0);
    },

    testShardOutOfSyncMetricChange: function () {
      const coordinators = getCoordinators();
      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, {
        numberOfShards: 2,
        replicationFactor: 3,
      });

      // Compute expected base values from collection definitions
      const expectedLeaderShards = getLeaderShardCount();
      const expectedTotalShards = getTotalShardCount();
      const expectedFollowerShards = getFollowerShardCount();

      assertAllShardMetrics(coordinators[0].endpoint, expectedLeaderShards, expectedTotalShards,
                            expectedFollowerShards, 0, 0, 0);

      const dbServers = getDBServers();
      const shards = db[testCollectionName].shards(true);
      const dbServerWithLeaderId = Object.values(shards).map(servers => servers[0]);
      const dbServerWithoutLeader = dbServers.find(server => !dbServerWithLeaderId.includes(server.id));
      dbServerWithoutLeader.suspend();

      // Ensure we insert documents on ALL shards
      const docsToInsert = generateDocsForAllShards(db[testCollectionName], 2, 50);
      db[testCollectionName].insert(docsToInsert);

      // Get metrics after we kill one db server with follower
      // const onlineServers = dbServers.filter(server => server.id !== dbServerWithoutLeader.id);
      // The server we crashed had two followers, so we have 2 out of sync shards
      // also other system shards might also be out of sync
      eventuallyAssertMetric(coordinators[0].endpoint, numberOutOfSyncShardsMetric,
        (value) => value >= 2,
        "Expected at least 2 shards out of sync");
      // One server is down, so we lose testCollShards shards (one replica per shard)
      // const onlineShardCount = totalShardCount - testCollShards;
      // getMetricsAndAssert(onlineServers, onlineShardCount, totalLeaderCount, null, 0);

      dbServerWithoutLeader.resume();

      // Eventually true
      assertAllShardMetrics(coordinators[0].endpoint, expectedLeaderShards, expectedTotalShards,
        expectedFollowerShards, 0, 0, 0);
    },

    testMetricsWhenFollowerIsMissing: function () {
      const coordinators = getCoordinators();

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, {
        numberOfShards: 1,
        replicationFactor: 2,
      });

      // Compute expected base values from collection definitions
      const expectedLeaderShards = getLeaderShardCount();
      const expectedTotalShards = getTotalShardCount();
      const expectedFollowerShards = getFollowerShardCount();

      assertAllShardMetrics(coordinators[0].endpoint, expectedLeaderShards, expectedTotalShards,
        expectedFollowerShards, 0, 0, 0);

        // Identify follower and other DB servers for our single shard
      const dbServers = getDBServers();
      const shards = db[testCollectionName].shards(true);
      const shardServers = Object.values(shards).flat();
      const dbServerFollower = dbServers.find(server => server.id === shardServers[1]);
      const dbServersOthers = dbServers.filter(server => !shardServers.includes(server.id));

      // Shutdown others first to prevent a FailedFollower job from replacing
      // the follower on a free server.
      dbServersOthers.forEach(server => server.suspend());
      // Then shutdown the follower.
      dbServerFollower.suspend();

      // Insert data to trigger replication
      db._query(`FOR i IN 0..10 INSERT {val: i} INTO ${testCollectionName}`);

      // Coordinator should see Plan != Current: the follower is missing.
      // With both non-leaders down, the shard has only the leader in Current,
      // so it should be out of sync, not replicated, and the follower out of sync.
      eventuallyAssertMetric(coordinators[0].endpoint, numberOutOfSyncShardsMetric,
        (value) => value >= 1,
        "Expected at least 1 shard out of sync");
      eventuallyAssertMetric(coordinators[0].endpoint, numberNotReplicatedShardsMetric,
        (value) => value >= 1,
        "Expected at least 1 shard not replicated");
      eventuallyAssertMetric(coordinators[0].endpoint, shardFollowersOutOfSyncMetric,
        (value) => value >= 1,
        "Expected at least 1 follower out of sync");

      // Resume all down servers
      dbServerFollower.resume();
      dbServersOthers.forEach(server => server.resume());

      // Eventually all metrics should return to healthy state
      assertAllShardMetrics(coordinators[0].endpoint, expectedLeaderShards, expectedTotalShards,
        expectedFollowerShards, 0, 0, 0);
    },

    testShardMetricsDuringMoveFollower: function () {
      const coordinators = getCoordinators();

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      let col = db._create(testCollectionName, {
        numberOfShards: 1,
        replicationFactor: 2,
      });
      // Data is necessary to trigger replication
      db._query(`FOR i IN 0..10 INSERT {} IN ${testCollectionName}`);

      // Calculate expected counts after setup
      const expectedTotalShards = getTotalShardCount();
      const expectedLeaderShards = getLeaderShardCount();
      const expectedFollowerShards = getFollowerShardCount();
      assertAllShardMetrics(coordinators[0].endpoint, expectedLeaderShards, expectedTotalShards,
                            expectedFollowerShards, 0, 0, 0);

      // Get shard information - shards(true) returns server IDs
      const shards = col.shards(true);
      const shardId = Object.keys(shards)[0];
      const leaderServerId = shards[shardId][0]; // leader server
      const fromServerId = shards[shardId][1]; // follower server

      const dbServers = getDBServers();
      const dbFreeDBServer = dbServers.filter(server => server.id !== leaderServerId && server.id !== fromServerId);
      const toServerId = dbFreeDBServer[0].id;
      assertEqual(dbFreeDBServer.length, 1);
      assertNotEqual(fromServerId, toServerId);

      // Move the shard (swap leader and follower) and wait for completion
      const moveResult = moveShard(testDbName, testCollectionName, shardId,
        fromServerId, toServerId, false);
      assertTrue(moveResult);

      assertAllShardMetrics(coordinators[0].endpoint, expectedLeaderShards, expectedTotalShards,
        expectedFollowerShards, 0, 0, 0);
    },

    testCoordinatorMetricsCreateAndDropDatabase: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);

      // Capture base values before creating the database
      const baseLeader = getLeaderShardCount();
      const baseTotal = getTotalShardCount();
      const baseFollowers = getFollowerShardCount();

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, { numberOfShards: 5, replicationFactor: 3 });

      const expectedLeader = getLeaderShardCount();
      const expectedTotal = getTotalShardCount();
      const expectedFollowers = getFollowerShardCount();

      assertAllShardMetrics(coordinators[0].endpoint, expectedLeader, expectedTotal,
                            expectedFollowers, 0, 0, 0);

      // Drop the database and assert metrics return to base values
      db._useDatabase("_system");
      db._dropDatabase(testDbName);

      assertAllShardMetrics(coordinators[0].endpoint, baseLeader, baseTotal,
                            baseFollowers, 0, 0, 0);
    },

    testCoordinatorMetricsAfterCreateAndDropCollection: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);

      // Capture base values before creating anything
      const baseLeader = getLeaderShardCount();
      const baseTotal = getTotalShardCount();
      const baseFollowers = getFollowerShardCount();

      db._create(testCollectionName, { numberOfShards: 5, replicationFactor: 3 });

      const expectedLeader = getLeaderShardCount();
      const expectedTotal = getTotalShardCount();
      const expectedFollowers = getFollowerShardCount();

      assertAllShardMetrics(coordinators[0].endpoint, expectedLeader, expectedTotal,
                            expectedFollowers, 0, 0, 0);

      // Drop just the collection and assert metrics return to base values
      db._drop(testCollectionName);

      assertAllShardMetrics(coordinators[0].endpoint, baseLeader, baseTotal,
                            baseFollowers, 0, 0, 0);
    },

  };
}

jsunity.run(metadataCoordinatorMetricsSuite);
return jsunity.done();
