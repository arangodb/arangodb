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

  const assertMetrics = function(coordinators, expectedCollections, expectedShards) {
    coordinators.forEach((coordinator) => {
      let numCollections = getMetric(coordinator.endpoint, "arangodb_metadata_number_of_collections");
      let numShards = getMetric(coordinator.endpoint, "arangodb_metadata_number_of_shards");

      for (let i = 0; i < 100; ++i) {
        internal.sleep(0.1);

        numCollections = getMetric(coordinator.endpoint, "arangodb_metadata_number_of_collections");
        if (expectedCollections !== null && numCollections !== expectedCollections) {
          continue;
        }

        numShards = getMetric(coordinator.endpoint, "arangodb_metadata_number_of_shards");
        if (expectedShards !== null && numShards !== expectedShards) {
          continue;
        }

        break; // all matched or not checking exact values
      }

      if (expectedCollections !== null) {
        assertEqual(numCollections, expectedCollections,
                    `Number of collections found: ${numCollections}, expected: ${expectedCollections}`);
      } else {
        assertTrue(typeof numCollections === 'number' && numCollections >= 0,
                   `Number of collections metric missing or invalid: ${numCollections}`);
      }

      if (expectedShards !== null) {
        assertEqual(numShards, expectedShards,
                    `Number of shards found: ${numShards}, expected: ${expectedShards}`);
      } else {
        assertTrue(typeof numShards === 'number' && numShards >= 0,
                   `Number of shards metric missing or invalid: ${numShards}`);
      }
    });
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

    testMetricsSimple: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);
      
      // Get base values from _system database
      db._useDatabase("_system");
      const coordinator = coordinators[0];
      const baseCollections = getMetric(coordinator.endpoint, "arangodb_metadata_number_of_collections");
      const baseShards = getMetric(coordinator.endpoint, "arangodb_metadata_number_of_shards");
      
      // Assert that we reach those base metrics
      assertMetrics(coordinators, baseCollections, baseShards);
    },

    testCoordinatorNewMetricsExistAndMatchShards: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);
      coordinators.forEach((coordinator) => {
        let total = getMetric(coordinator.endpoint, "arangodb_metadata_total_number_of_shards");
        let planShards = getMetric(coordinator.endpoint, "arangodb_metadata_number_of_shards");
        for (let i = 0; i < 3 && total !== planShards; ++i) {
          internal.sleep(0.1);
          total = getMetric(coordinator.endpoint, "arangodb_metadata_total_number_of_shards");
          planShards = getMetric(coordinator.endpoint, "arangodb_metadata_number_of_shards");
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

      print(`expectedLeader: ${expectedLeader}, expectedTotal: ${expectedTotal}, expectedFollowers: ${expectedFollowers}`);
      eventuallyAssertMetric(
        coordinators, "arangodb_metadata_number_of_shards",
        (value) => value === expectedLeader,
        `expected: ${expectedLeader}`
      );
      eventuallyAssertMetric(
        coordinators, "arangodb_metadata_total_number_of_shards",
        (value) => value === expectedTotal, 
        `expected: ${expectedTotal}`
      );
      eventuallyAssertMetric(
        coordinators, "arangodb_metadata_number_follower_shards",
        (value) => value === expectedFollowers,
        `expected: ${expectedFollowers}`
      );
    },

    testCoordinatorMetricsNotReplicatedCount: function() {
      const coordinators = getCoordinators();
      assertTrue(coordinators.length > 0);
      const coordinator = coordinators[0];

      const baseNotRep = getMetric(coordinator.endpoint, "arangodb_metadata_number_not_replicated_shards");

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, { numberOfShards: 4, replicationFactor: 1 });

      const expectedNotRep = baseNotRep + 4;

      let notrep = 0;
      for (let i = 0; i < 8; ++i) {
        internal.sleep(0.1);
        notrep = getMetric(coordinator.endpoint, "arangodb_metadata_number_not_replicated_shards");
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
      const baseFollowers = getMetric(coordinator.endpoint, "arangodb_metadata_number_follower_shards");

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
        followers = getMetric(coordinator.endpoint, "arangodb_metadata_number_follower_shards");
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

      const baseOutOfSync = getMetric(coordinator.endpoint, "arangodb_metadata_shard_followers_out_of_sync_number");

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, { numberOfShards: 2, replicationFactor: 3 });

      // Wait for collection to be created and metrics to stabilize
      let outOfSync = 0;
      for (let i = 0; i < 8; ++i) {
        internal.sleep(0.1);
        outOfSync = getMetric(coordinator.endpoint, "arangodb_metadata_shard_followers_out_of_sync_number");
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