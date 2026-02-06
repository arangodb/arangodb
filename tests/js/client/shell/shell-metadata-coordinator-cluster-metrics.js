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
const { getMetric, getCoordinators, moveShard } = require("@arangodb/test-helper");

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

  // Helper function to eventually assert a single metric with a custom comparison function
  // compareFn takes the metric value and returns true if the assertion should pass
  const eventuallyAssertMetric = function(coordinators, metricName, compareFn, errorMessage) {
    let metricValue;
    const coordinator = coordinators[0];
    for (let i = 0; i < 200; i++) {
      internal.wait(0.1);
      metricValue = getMetric(coordinator.endpoint, metricName);
      if (compareFn(metricValue)) {
        break;
      }
    }
    // Final assertion with error message
    assertTrue(compareFn(metricValue), `${errorMessage}: got ${metricValue}`);
    return metricValue;
  };

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
  const assertAllShardMetrics = function(coordinators, expectedLeaderShards,
                                         expectedTotalShards, expectedFollowerShards,
                                         expectedNotReplicated, expectedOutOfSync,
                                         expectedFollowersOutOfSync) {
    eventuallyAssertMetric(
      coordinators, numberOfShardsMetric,
      (value) => value === expectedLeaderShards,
      `leader shards expected: ${expectedLeaderShards}`
    );
    eventuallyAssertMetric(
      coordinators, totalNumberOfShardsMetric,
      (value) => value === expectedTotalShards,
      `total shards expected: ${expectedTotalShards}`
    );
    eventuallyAssertMetric(
      coordinators, numberFollowerShardsMetric,
      (value) => value === expectedFollowerShards,
      `follower shards expected: ${expectedFollowerShards}`
    );
    eventuallyAssertMetric(
      coordinators, numberNotReplicatedShardsMetric,
      (value) => value === expectedNotReplicated,
      `not replicated shards expected: ${expectedNotReplicated}`
    );
    eventuallyAssertMetric(
      coordinators, numberOutOfSyncShardsMetric,
      (value) => value === expectedOutOfSync,
      `out of sync shards expected: ${expectedOutOfSync}`
    );
    eventuallyAssertMetric(
      coordinators, shardFollowersOutOfSyncMetric,
      (value) => value === expectedFollowersOutOfSync,
      `followers out of sync expected: ${expectedFollowersOutOfSync}`
    );
  };

  // Helper: compute not-replicated shard count (shards with replicationFactor == 1)
  const getNotReplicatedShardCount = function() {
    const currentDb = db._name();
    let count = 0;
    db._useDatabase("_system");
    db._databases().forEach((database) => {
      db._useDatabase(database);
      db._collections().forEach((col) => {
        const props = col.properties();
        if (props.replicationFactor === 1) {
          count += props.numberOfShards;
        }
      });
    });
    db._useDatabase(currentDb);
    return count;
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
      const expectedNotReplicated = getNotReplicatedShardCount();

      // In a healthy baseline state, no shards should be out of sync
      assertAllShardMetrics(coordinators, expectedLeaderShards, expectedTotalShards,
                            expectedFollowerShards, expectedNotReplicated, 0, 0);
    },

    testCoordinatorNewMetricsExistAndMatchShards: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);
      coordinators.forEach((coordinator) => {
        let total = getMetric(coordinator.endpoint, totalNumberOfShardsMetric);
        let planShards = getMetric(coordinator.endpoint, numberOfShardsMetric);
        for (let i = 0; i < 3 && total !== planShards; ++i) {
          internal.sleep(0.1);
          total = getMetric(coordinator.endpoint, totalNumberOfShardsMetric);
          planShards = getMetric(coordinator.endpoint, numberOfShardsMetric);
        }
        assertEqual(total, planShards,
                    `coordinator total shards (${total}) must match metadata shards (${planShards})`);
      });
    },

    testCoordinatorMetricsAfterCreatingReplicatedCollection: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, { numberOfShards: 5, replicationFactor: 3 });

      const expectedLeader = getLeaderShardCount();
      const expectedTotal = getTotalShardCount();
      const expectedFollowers = getFollowerShardCount();

      eventuallyAssertMetric(
        coordinators, numberOfShardsMetric,
        (value) => value === expectedLeader,
        `expected: ${expectedLeader}`
      );
      eventuallyAssertMetric(
        coordinators, totalNumberOfShardsMetric,
        (value) => value === expectedTotal, 
        `expected: ${expectedTotal}`
      );
      eventuallyAssertMetric(
        coordinators, numberFollowerShardsMetric,
        (value) => value === expectedFollowers,
        `expected: ${expectedFollowers}`
      );
    },

    testCoordinatorMetricsNotReplicatedCount: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);
      const coordinator = coordinators[0];

      const baseNotRep = getMetric(coordinator.endpoint, numberNotReplicatedShardsMetric);

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, { numberOfShards: 4, replicationFactor: 1 });

      const expectedNotRep = baseNotRep + 4;

      let notrep = 0;
      for (let i = 0; i < 8; ++i) {
        internal.sleep(0.1);
        notrep = getMetric(coordinator.endpoint, numberNotReplicatedShardsMetric);
        if (notrep === expectedNotRep) break;
      }

      assertEqual(notrep, expectedNotRep, `not replicated shards: ${notrep}, expected ${expectedNotRep}`);

      // cleanup
      db._drop(testCollectionName);
      db._useDatabase("_system");
      db._dropDatabase(testDbName);
    },

    testCoordinatorFollowerCountStableOnMove: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);
      const coordinator = coordinators[0];

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      const col = db._create(testCollectionName, { numberOfShards: 3, replicationFactor: 3 });

      // baseline follower
      const baseFollowers = getMetric(coordinator.endpoint, numberFollowerShardsMetric);

      const shards = col.shards(true);
      const shardId = Object.keys(shards)[0];
      const fromServer = shards[shardId][0];
      const toServer = shards[shardId][1];
      assertNotEqual(fromServer, toServer);

      const moved = moveShard(testDbName, testCollectionName, shardId, fromServer, toServer, false);
      assertTrue(moved);

      let followers = 0;
      for (let i = 0; i < 12; ++i) {
        internal.sleep(0.1);
        followers = getMetric(coordinator.endpoint, numberFollowerShardsMetric);
        if (followers === baseFollowers) break;
      }

      assertEqual(followers, baseFollowers, `followers after move: ${followers}, expected ${baseFollowers}`);

      // cleanup
      db._drop(testCollectionName);
      db._useDatabase("_system");
      db._dropDatabase(testDbName);
    },

    testCoordinatorShardFollowersOutOfSyncNumber: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);
      const coordinator = coordinators[0];

      const baseOutOfSync = getMetric(coordinator.endpoint, shardFollowersOutOfSyncMetric);

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, { numberOfShards: 2, replicationFactor: 3 });

      // Wait for collection to be created and metrics to stabilize
      let outOfSync = 0;
      for (let i = 0; i < 8; ++i) {
        internal.sleep(0.1);
        outOfSync = getMetric(coordinator.endpoint, shardFollowersOutOfSyncMetric);
        // In a healthy cluster, followers should sync quickly, so out of sync should be 0 or low
        if (outOfSync >= 0) break;
      }

      // Verify metric exists and is non-negative
      assertTrue(typeof outOfSync === 'number' && outOfSync >= 0,
                 `out of sync followers metric missing or invalid: ${outOfSync}`);

      // cleanup
      db._drop(testCollectionName);
      db._useDatabase("_system");
      db._dropDatabase(testDbName);
    }
  };
}

jsunity.run(metadataCoordinatorMetricsSuite);
return jsunity.done();