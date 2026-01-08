/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, */

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
/// @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const { getMetric, getDBServers, moveShard } = require("@arangodb/test-helper");

function ClusterDBServerShardMetricsTestSuite() {
  'use strict';
 
  const dbName = "UnitTestShardMetricsDatabase";
  const collectionName = "UnitTestShardMetricsCollection";

  const shardsNumMetric = "arangodb_shards_number";
  const shardsLeaderNumMetric = "arangodb_shards_leader_number";
  const shardsFollowerNumMetric = "arangodb_shards_follower_number";
  const shardsOutOfSyncNumMetric = "arangodb_shards_out_of_sync";
  const followersOutOfSyncNumMetric = "arangodb_shard_followers_out_of_sync_number";
  const shardsNotReplicatedNumMetric = "arangodb_shards_not_replicated";

  // Helper function to get sum of a metric across all DB servers
  const getDBServerMetricSum = function(dbServers, metricName) {
    let sum = 0;
    for (let server of dbServers) {
      const value = getMetric(server.endpoint, metricName);
      sum += value;
    }
    return sum;
  };

  // Helper to calculate total shard count for a database (shards * replicationFactor)
  const getDbShardCount = function(database) {
    const oldDb = db._name();
    db._useDatabase(database);
    const count = db._collections().reduce((sum, col) => {
      const props = col.properties();
      return sum + props.numberOfShards * props.replicationFactor;
    }, 0);
    db._useDatabase(oldDb);
    return count;
  };

  // Helper to calculate total leader count for a database (just numberOfShards)
  const getDbLeaderCount = function(database) {
    const oldDb = db._name();
    db._useDatabase(database);
    const count = db._collections().reduce((sum, col) => {
      return sum + col.properties().numberOfShards;
    }, 0);
    db._useDatabase(oldDb);
    return count;
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

  const getMetricsAndAssert = function(servers, expectedShardsNum, expectedShardsLeaderNum = null, expectedShardsOutOfSync = null, expectedShardsNotReplicated = null, expectedFollowersOutOfSync = null) {
    const shardsNumMetricValue = getDBServerMetricSum(servers, shardsNumMetric);
    assertEqual(shardsNumMetricValue, expectedShardsNum, `shardsNumMetricValue: ${shardsNumMetricValue} !== expectedShardsNum: ${expectedShardsNum}`);

    if (expectedShardsLeaderNum !== null) {
      const shardsLeaderNumMetricValue = getDBServerMetricSum(servers, shardsLeaderNumMetric);
      assertEqual(shardsLeaderNumMetricValue, expectedShardsLeaderNum, `shardsLeaderNumMetricValue: ${shardsLeaderNumMetricValue} !== expectedShardsLeaderNum: ${expectedShardsLeaderNum}`);
    }

    if (expectedShardsLeaderNum !== null) {
      const shardsFollowerNumMetricValue = getDBServerMetricSum(servers, shardsFollowerNumMetric);
      assertEqual(shardsFollowerNumMetricValue, expectedShardsNum - expectedShardsLeaderNum, `shardsFollowerNumMetricValue: ${shardsFollowerNumMetricValue} !== expectedShardsNum - expectedShardsLeaderNum: ${expectedShardsNum - expectedShardsLeaderNum}`);
    }

    if (expectedShardsOutOfSync !== null) {
      const shardsOutOfSyncNumMetricValue = getDBServerMetricSum(servers, shardsOutOfSyncNumMetric);
      assertEqual(shardsOutOfSyncNumMetricValue, expectedShardsOutOfSync, `shardsOutOfSyncNumMetricValue: ${shardsOutOfSyncNumMetricValue} !== expectedShardsOutOfSync: ${expectedShardsOutOfSync}`);
    }

    if (expectedFollowersOutOfSync !== null) {
      const followersOutOfSyncNumMetricValue = getDBServerMetricSum(servers, followersOutOfSyncNumMetric);
      assertEqual(followersOutOfSyncNumMetricValue, expectedFollowersOutOfSync, `followersOutOfSyncNumMetricValue: ${followersOutOfSyncNumMetricValue} !== expectedFollowersOutOfSync: ${expectedFollowersOutOfSync}`);
    }

    if (expectedShardsNotReplicated !== null) {
      const shardsNotReplicatedNumMetricValue = getDBServerMetricSum(servers, shardsNotReplicatedNumMetric);
      assertEqual(shardsNotReplicatedNumMetricValue, expectedShardsNotReplicated, `shardsNotReplicatedNumMetricValue: ${shardsNotReplicatedNumMetricValue} !== expectedShardsNotReplicated: ${expectedShardsNotReplicated}`);
    }
  };

  const getMetricsAndEventuallyAssert = function(servers, expectedShardsNum, expectedShardsLeaderNum, expectedShardsOutOfSync, expectedShardsNotReplicated, expectedFollowersOutOfSync = 0) {
    for(let i = 0; i < 100; i++) {
      internal.wait(0.1);
      const shardsNumMetricValue = getDBServerMetricSum(servers, shardsNumMetric);
      if (shardsNumMetricValue !== expectedShardsNum && expectedShardsNum !== null) {
        continue;
      } 

      const shardsLeaderNumMetricValue = getDBServerMetricSum(servers, shardsLeaderNumMetric);
      if (shardsLeaderNumMetricValue !== expectedShardsLeaderNum && expectedShardsLeaderNum !== null) {
        continue;
      }

      const shardsOutOfSyncNumMetricValue = getDBServerMetricSum(servers, shardsOutOfSyncNumMetric);
      if (shardsOutOfSyncNumMetricValue !== expectedShardsOutOfSync) {
        continue;
      }

      const followersOutOfSyncNumMetricValue = getDBServerMetricSum(servers, followersOutOfSyncNumMetric);
      if (followersOutOfSyncNumMetricValue !== expectedFollowersOutOfSync) {
        continue;
      }

      const shardsNotReplicatedNumMetricValue = getDBServerMetricSum(servers, shardsNotReplicatedNumMetric);
      if (shardsNotReplicatedNumMetricValue !== expectedShardsNotReplicated) {
        continue;
      }

      break;
    }

    getMetricsAndAssert(servers, expectedShardsNum, expectedShardsLeaderNum, expectedShardsOutOfSync, expectedShardsNotReplicated, expectedFollowersOutOfSync);
  };

  // Helper function to eventually assert a single metric with a custom comparison function
  // compareFn takes the metric value and returns true if the assertion should pass
  const eventuallyAssertMetric = function(servers, metricName, compareFn, errorMessage) {
    let metricValue;
    for (let i = 0; i < 100; i++) {
      internal.wait(0.1);
      metricValue = getDBServerMetricSum(servers, metricName);
      if (compareFn(metricValue)) {
        return metricValue;
      }
    }
    // Final assertion with error message
    assertTrue(compareFn(metricValue), `${errorMessage}: got ${metricValue}`);
    return metricValue;
  };

  return {
    tearDown: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(dbName);
      } catch (e) {
        // Ignore errors
      }
    },

    testShardCountMetricStability: function () {
      const dbServers = getDBServers();
      const systemShardCount = getDbShardCount("_system");
      const systemLeaderCount = getDbLeaderCount("_system");
      getMetricsAndAssert(dbServers, systemShardCount, systemLeaderCount, 0, 0);

      db._createDatabase(dbName);
      const newDbShardCount = getDbShardCount(dbName);
      const newDbLeaderCount = getDbLeaderCount(dbName);
      const totalShardCount = systemShardCount + newDbShardCount;
      const totalLeaderCount = systemLeaderCount + newDbLeaderCount;
      getMetricsAndEventuallyAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);

      db._useDatabase(dbName);
      const testCollShards = 6;
      const testCollReplication = 2;
      db._create(collectionName, {
        numberOfShards: testCollShards,
        replicationFactor: testCollReplication
      });

      const expectedShardCount = totalShardCount + (testCollShards * testCollReplication);
      const expectedLeaderCount = totalLeaderCount + testCollShards;
      getMetricsAndEventuallyAssert(dbServers, expectedShardCount, expectedLeaderCount, 0, 0);

      // Check stability of metrics
      let metricsMap = {
        [shardsNumMetric]: [],
        [shardsLeaderNumMetric]: [],
        [shardsFollowerNumMetric]: [],
        [shardsOutOfSyncNumMetric]: [],
        [followersOutOfSyncNumMetric]: [],
        [shardsNotReplicatedNumMetric]: [],
      };
      for(let i = 0; i < 40; i++) {
        Object.entries(metricsMap).forEach(([key, value]) => {
          value.push(getDBServerMetricSum(dbServers, key));
        });
      }

      // Assert the value of the first entry
      assertEqual(metricsMap[shardsNumMetric][0], expectedShardCount);
      assertEqual(metricsMap[shardsLeaderNumMetric][0], expectedLeaderCount);
      assertEqual(metricsMap[shardsFollowerNumMetric][0], expectedShardCount - expectedLeaderCount);
      assertEqual(metricsMap[shardsOutOfSyncNumMetric][0], 0);
      assertEqual(metricsMap[followersOutOfSyncNumMetric][0], 0);
      assertEqual(metricsMap[shardsNotReplicatedNumMetric][0], 0);

      // Test stability of metrics by asserting that all inserted values
      // of a metric are same as the first entry
      Object.entries(metricsMap).forEach(([metricName, values]) => {
        assertEqual(values.length, 40);
        values.forEach(value => assertEqual(value, values[0], `Metric ${metricName} is not stable it has values: ${values}`));
      });
    },

    testShardOutOfSyncMetricChange: function () {
      const dbServers = getDBServers();

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      const testCollShards = 2;
      db._create(collectionName, {
        numberOfShards: testCollShards,
        replicationFactor: 3,
      });

      // Calculate expected counts after setup
      const totalShardCount = getDbShardCount("_system") + getDbShardCount(dbName);
      const totalLeaderCount = getDbLeaderCount("_system") + getDbLeaderCount(dbName);
      getMetricsAndAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);

      const shards = db[collectionName].shards(true);
      const dbServerWithLeaderId = Object.values(shards).map(servers => servers[0]);
      const dbServerWithoutLeader = dbServers.find(server => !dbServerWithLeaderId.includes(server.id));
      dbServerWithoutLeader.suspend();

      // Ensure we insert documents on ALL shards
      const docsToInsert = generateDocsForAllShards(db[collectionName], testCollShards, 50);
      db[collectionName].insert(docsToInsert);

      // Get metrics after we kill one db server with follower
      const onlineServers = dbServers.filter(server => server.id !== dbServerWithoutLeader.id);
      // The server we crashed had two followers, so we have 2 out of sync shards
      // also other system shards might also be out of sync
      eventuallyAssertMetric(onlineServers, shardsOutOfSyncNumMetric,
        (value) => value >= 2,
        "Expected at least 2 shards out of sync");
      // One server is down, so we lose testCollShards shards (one replica per shard)
      const onlineShardCount = totalShardCount - testCollShards;
      getMetricsAndAssert(onlineServers, onlineShardCount, totalLeaderCount, null, 0);

      dbServerWithoutLeader.resume();

      // Eventually true
      getMetricsAndEventuallyAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);
    },

    testShardNotReplicatedMetricChange: function () {
      const dbServers = getDBServers();

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      db._create(collectionName, {
        numberOfShards: 1,
        replicationFactor: 2,
      });
      // Data is necessary to trigger replication
      db._query(`FOR i IN 0..100 INSERT {value: i} IN ${collectionName}`);

      // Calculate expected counts after setup
      const totalShardCount = getDbShardCount("_system") + getDbShardCount(dbName);
      const totalLeaderCount = getDbLeaderCount("_system") + getDbLeaderCount(dbName);
      getMetricsAndAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);

      // Get db servers which have the two followers
      const shards = db[collectionName].shards(true);
      const dbServerFollowersId = Object.values(shards).flatMap(servers => servers.slice(1));
      const dbServerFollowers = dbServers.filter(server => dbServerFollowersId.includes(server.id));

      // Get a db server which has a single leader and its metrics
      const dbServerLeaderId = Object.values(shards).flatMap(servers => servers[0])[0];
      const dbServerLeader = dbServers.find(server => server.id === dbServerLeaderId);
      const onlineServers = [dbServerLeader];

      // Shutdown followers
      dbServerFollowers.forEach(server => {
        server.suspend();
      });

      // Everything is out of sync and not replicated
      // and we cannot assert precisely the values since we do not know the
      // shards distribution of internal collections from _system and $dbName databases, but we can assert that:
      // - eventually the number of shards would be equal to the total number of leaders
      // - eventually the number of shards leaders must be 1 or greater
      // - eventually the number of out of sync should be at least 1
      // - eventually the number of not replicated shards should be at least 1
      for(let i = 0; i < 100; i++) {
        internal.wait(0.1);
        const shardsNumMetricValue = getDBServerMetricSum(onlineServers, shardsNumMetric);
        if (shardsNumMetricValue !== totalLeaderCount) {
          continue;
        }
        const shardsLeaderNumMetricValue = getDBServerMetricSum(onlineServers, shardsLeaderNumMetric);
        if (shardsLeaderNumMetricValue < 1) {
          continue;
        }
        const shardsOutOfSyncNumMetricValue = getDBServerMetricSum(onlineServers, shardsOutOfSyncNumMetric);
        if (shardsOutOfSyncNumMetricValue < 1) {
          continue;
        }
        const shardsNotReplicatedNumMetricValue = getDBServerMetricSum(onlineServers, shardsNotReplicatedNumMetric);
        if (shardsNotReplicatedNumMetricValue < 1) {
          continue;
        }
        const followersOutOfSyncNumMetricValue = getDBServerMetricSum(onlineServers, followersOutOfSyncNumMetric);
        if (followersOutOfSyncNumMetricValue < 1) {
          continue;
        }

        break;
      }
      const shardsOutOfSyncNumMetricValue = getDBServerMetricSum(onlineServers, shardsOutOfSyncNumMetric);
      const shardsNotReplicatedNumMetricValue = getDBServerMetricSum(onlineServers, shardsNotReplicatedNumMetric);
      assertEqual(shardsOutOfSyncNumMetricValue, shardsNotReplicatedNumMetricValue, `shardsOutOfSyncNumMetricValue: ${shardsOutOfSyncNumMetricValue} !== shardsNotReplicatedNumMetricValue: ${shardsNotReplicatedNumMetricValue}`);

      // Bring back the followers
      dbServerFollowers.forEach(server => {
        server.resume();
      });

      // Eventually true
      getMetricsAndEventuallyAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);
    },

    testShardFollowerOutOfSync: function () {
      const dbServers = getDBServers();

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      db._create(collectionName, {
        numberOfShards: 1,
        replicationFactor: 2,
      });

      // Calculate expected counts after setup
      const totalShardCount = getDbShardCount("_system") + getDbShardCount(dbName);
      const totalLeaderCount = getDbLeaderCount("_system") + getDbLeaderCount(dbName);
      getMetricsAndAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);

      // Get the db servers which do not have the leader
      const shards = db[collectionName].shards(true);
      const dbServerLeaderId = Object.values(shards).flatMap(servers => servers[0])[0];
      const dbServersWithoutLeader = dbServers.filter(server => server.id !== dbServerLeaderId);
      assertEqual(dbServersWithoutLeader.length, 2);

      // Shutdown followers
      dbServersWithoutLeader.forEach(server => {
        server.suspend();
      });

      // Insert some data to trigger replication
      db._query(`FOR i IN 0..1000 INSERT {val: i} INTO ${collectionName}`);

      dbServersWithoutLeader[0].resume();
      // Only the second db server id down
      const onlineServers = dbServers.filter(server => server.id !== dbServersWithoutLeader[1].id);

      let followersOutOfSyncNumMetricValue;
      for(let i = 0; i < 100; i++) {
        internal.wait(0.1);
        followersOutOfSyncNumMetricValue = getDBServerMetricSum(onlineServers, followersOutOfSyncNumMetric);
        if (followersOutOfSyncNumMetricValue === 0) {
          continue;
        }

        break;
      }

      assertTrue(followersOutOfSyncNumMetricValue > 0);
      dbServersWithoutLeader[1].resume();

      getMetricsAndEventuallyAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);
    },

    testShardMetricsDuringMoveLeader: function () {
      const dbServers = getDBServers();

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      let col = db._create(collectionName, {
        numberOfShards: 2,
        replicationFactor: 3,
      });
      // Data is necessary to trigger replication
      db._query(`FOR i IN 0..100 INSERT {value: i} IN ${collectionName}`);

      // Calculate expected counts after setup
      const totalShardCount = getDbShardCount("_system") + getDbShardCount(dbName);
      const totalLeaderCount = getDbLeaderCount("_system") + getDbLeaderCount(dbName);
      getMetricsAndAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);

      // Get shard information - shards(true) returns server IDs
      const shards = col.shards(true);
      const shardId = Object.keys(shards)[0];
      const fromServer = shards[shardId][0]; // leader server
      const toServer = shards[shardId][1]; // follower server
      assertNotEqual(fromServer, toServer);

      // Move the shard (swap leader and follower) and wait for completion
      const moveResult = moveShard(dbName, collectionName, shardId, 
                                   fromServer, toServer, false);
      assertTrue(moveResult);

      // The metrics should remain the same
      getMetricsAndAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);
    },

    testShardMetricsDuringMoveFollower: function () {
      const dbServers = getDBServers();

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      let col = db._create(collectionName, {
        numberOfShards: 1,
        replicationFactor: 2,
      });
      // Data is necessary to trigger replication
      db._query(`FOR i IN 0..100 INSERT {value: i} IN ${collectionName}`);

      // Calculate expected counts after setup
      const totalShardCount = getDbShardCount("_system") + getDbShardCount(dbName);
      const totalLeaderCount = getDbLeaderCount("_system") + getDbLeaderCount(dbName);
      getMetricsAndAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);

      // Get shard information - shards(true) returns server IDs
      const shards = col.shards(true);
      const shardId = Object.keys(shards)[0];
      const leaderServer = shards[shardId][0]; // leader server
      const fromServer = shards[shardId][1]; // follower server

      const dbFreeDBServer = dbServers.filter(server => server.id !== leaderServer && server.id !== fromServer);
      const toServer = dbFreeDBServer[0].id;
      assertEqual(dbFreeDBServer.length, 1);
      assertNotEqual(fromServer, toServer);

      // Move the shard (swap leader and follower) and wait for completion
      const moveResult = moveShard(dbName, collectionName, shardId, 
                                   fromServer, toServer, false);
      assertTrue(moveResult);

      // The metrics should remain the same
      getMetricsAndAssert(dbServers, totalShardCount, totalLeaderCount, 0, 0);
    },

    testShardMetricsAfterCollectionDeletion: function () {
      const dbServers = getDBServers();

      // Get baseline metrics before creating anything
      const baselineShardCount = getDbShardCount("_system");
      const baselineLeaderCount = getDbLeaderCount("_system");
      getMetricsAndEventuallyAssert(dbServers, baselineShardCount, baselineLeaderCount, 0, 0);

      // Create database and collection
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      const newDatabaseShardsCount = getDbShardCount(dbName);
      const newDatabaseLeaderCount = getDbLeaderCount(dbName);

      const testCollShards = 3;
      const testCollReplication = 2;
      db._create(collectionName, {
        numberOfShards: testCollShards,
        replicationFactor: testCollReplication,
      });

      // Calculate expected counts after creating collection
      const expectedShardCount = baselineShardCount + (testCollShards * testCollReplication) + newDatabaseShardsCount;
      const expectedLeaderCount = baselineLeaderCount + testCollShards + newDatabaseLeaderCount;
      getMetricsAndEventuallyAssert(dbServers, expectedShardCount, expectedLeaderCount, 0, 0);

      // Drop the collection
      db._drop(collectionName);

      // Metrics should return to baseline (only _system shards remain, dbName has no collections)
      getMetricsAndEventuallyAssert(dbServers, baselineShardCount + newDatabaseShardsCount, baselineLeaderCount + newDatabaseLeaderCount, 0, 0);
    },

    testShardMetricsAfterDatabaseDeletion: function () {
      const dbServers = getDBServers();

      // Get baseline metrics before creating anything
      const baselineShardCount = getDbShardCount("_system");
      const baselineLeaderCount = getDbLeaderCount("_system");
      getMetricsAndEventuallyAssert(dbServers, baselineShardCount, baselineLeaderCount, 0, 0);

      // Create database and collection
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      const newDatabaseShardsCount = getDbShardCount(dbName);
      const newDatabaseLeaderCount = getDbLeaderCount(dbName);

      const testCollShards = 3;
      const testCollReplication = 2;
      db._create(collectionName, {
        numberOfShards: testCollShards,
        replicationFactor: testCollReplication,
      });

      // Calculate expected counts after creating collection
      const expectedShardCount = baselineShardCount + (testCollShards * testCollReplication) + newDatabaseShardsCount;
      const expectedLeaderCount = baselineLeaderCount + testCollShards + newDatabaseLeaderCount;
      getMetricsAndEventuallyAssert(dbServers, expectedShardCount, expectedLeaderCount, 0, 0);

      db._useDatabase("_system");
      db._dropDatabase(dbName);

      // Metrics should return to baseline
      getMetricsAndEventuallyAssert(dbServers, baselineShardCount, baselineLeaderCount, 0, 0);
    },

  };
}

jsunity.run(ClusterDBServerShardMetricsTestSuite);
return jsunity.done();

