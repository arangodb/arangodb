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
const { getMetric, getEndpointsByType, moveShard } = require("@arangodb/test-helper");

function metadataCoordinatorMetricsSuite() {
  'use strict';
  const testDbName = "testMetricsCoordinator";
  const testCollectionName = "testMetricsCollection";

  const assertMetrics = function(endpoints, expectedCollections, expectedShards) {
    const retries = 3;

    endpoints.forEach((ep) => {
      let numCollections = getMetric(ep, "arangodb_metadata_number_of_collections");
      let numShards = getMetric(ep, "arangodb_metadata_number_of_shards");

      for (let i = 0; i < retries; ++i) {
        internal.sleep(0.1);

        numCollections = getMetric(ep, "arangodb_metadata_number_of_collections");
        if (expectedCollections !== null && numCollections !== expectedCollections) {
          continue;
        }

        numShards = getMetric(ep, "arangodb_metadata_number_of_shards");
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
      const endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);
      // Do not assert exact numbers here; just ensure metrics exist and are
      // non-negative. Exact counts may vary across test environments.
      assertMetrics(endpoints, null, null);
    },

    testCoordinatorNewMetricsExistAndMatchShards: function() {
      const endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);
      endpoints.forEach((ep) => {
        let total = getMetric(ep, "arangodb_metadata_total_number_of_shards");
        let planShards = getMetric(ep, "arangodb_metadata_number_of_shards");
        for (let i = 0; i < 3 && total !== planShards; ++i) {
          internal.sleep(0.1);
          total = getMetric(ep, "arangodb_metadata_total_number_of_shards");
          planShards = getMetric(ep, "arangodb_metadata_number_of_shards");
        }
        assertEqual(total, planShards,
                    `coordinator total shards (${total}) must match metadata shards (${planShards})`);
      });
    },

    testCoordinatorMetricsAfterCreatingReplicatedCollection: function() {
      const endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);
      const ep = endpoints[0];

      const baseTotal = getMetric(ep, "arangodb_metadata_total_number_of_shards");
      const baseFollowers = getMetric(ep, "arangodb_metadata_number_follower_shards");

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, { numberOfShards: 5, replicationFactor: 3 });

      const expectedTotal = baseTotal + 5;
      const expectedFollowers = baseFollowers + 10; // 5 * (3-1)

      let total = 0, followers = 0;
      for (let i = 0; i < 8; ++i) {
        internal.sleep(0.1);
        total = getMetric(ep, "arangodb_metadata_total_number_of_shards");
        followers = getMetric(ep, "arangodb_metadata_number_follower_shards");
        if (total === expectedTotal && followers === expectedFollowers) break;
      }

      assertEqual(total, expectedTotal, `total shards after create: ${total}, expected ${expectedTotal}`);
      assertEqual(followers, expectedFollowers, `follower shards after create: ${followers}, expected ${expectedFollowers}`);

      // cleanup
      db._drop(testCollectionName);
      db._useDatabase("_system");
      db._dropDatabase(testDbName);
    },

    testCoordinatorMetricsNotReplicatedCount: function() {
      const endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);
      const ep = endpoints[0];

      const baseNotRep = getMetric(ep, "arangodb_metadata_number_not_replicated_shards");

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, { numberOfShards: 4, replicationFactor: 1 });

      const expectedNotRep = baseNotRep + 4;

      let notrep = 0;
      for (let i = 0; i < 8; ++i) {
        internal.sleep(0.1);
        notrep = getMetric(ep, "arangodb_metadata_number_not_replicated_shards");
        if (notrep === expectedNotRep) break;
      }

      assertEqual(notrep, expectedNotRep, `not replicated shards: ${notrep}, expected ${expectedNotRep}`);

      // cleanup
      db._drop(testCollectionName);
      db._useDatabase("_system");
      db._dropDatabase(testDbName);
    },

    testCoordinatorFollowerCountStableOnMove: function() {
      const endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);
      const ep = endpoints[0];

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      const col = db._create(testCollectionName, { numberOfShards: 3, replicationFactor: 3 });

      // baseline follower
      const baseFollowers = getMetric(ep, "arangodb_metadata_number_follower_shards");

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
        followers = getMetric(ep, "arangodb_metadata_number_follower_shards");
        if (followers === baseFollowers) break;
      }

      assertEqual(followers, baseFollowers, `followers after move: ${followers}, expected ${baseFollowers}`);

      // cleanup
      db._drop(testCollectionName);
      db._useDatabase("_system");
      db._dropDatabase(testDbName);
    },

    testCoordinatorShardsFollowerNumber: function() {
      const endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);
      const ep = endpoints[0];

      const baseFollowerNumber = getMetric(ep, "arangodb_metadata_shards_follower_number");
      const baseTotal = getMetric(ep, "arangodb_metadata_total_number_of_shards");
      const baseLeaders = getMetric(ep, "arangodb_metadata_number_of_shards");

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, { numberOfShards: 3, replicationFactor: 3 });

      const expectedTotal = baseTotal + 3;
      const expectedLeaders = baseLeaders + 3;
      const expectedFollowerNumber = baseFollowerNumber + 6; // 3 * (3-1)

      let followerNumber = 0, total = 0, leaders = 0;
      for (let i = 0; i < 8; ++i) {
        internal.sleep(0.1);
        followerNumber = getMetric(ep, "arangodb_metadata_shards_follower_number");
        total = getMetric(ep, "arangodb_metadata_total_number_of_shards");
        leaders = getMetric(ep, "arangodb_metadata_number_of_shards");
        if (followerNumber === expectedFollowerNumber && total === expectedTotal && leaders === expectedLeaders) break;
      }

      assertEqual(followerNumber, expectedFollowerNumber, 
                  `follower number after create: ${followerNumber}, expected ${expectedFollowerNumber}`);
      // Verify that follower number equals total - leaders
      assertEqual(followerNumber, total - leaders,
                  `follower number (${followerNumber}) should equal total (${total}) - leaders (${leaders})`);

      // cleanup
      db._drop(testCollectionName);
      db._useDatabase("_system");
      db._dropDatabase(testDbName);
    },

    testCoordinatorShardFollowersOutOfSyncNumber: function() {
      const endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);
      const ep = endpoints[0];

      const baseOutOfSync = getMetric(ep, "arangodb_metadata_shard_followers_out_of_sync_number");

      db._createDatabase(testDbName);
      db._useDatabase(testDbName);
      db._create(testCollectionName, { numberOfShards: 2, replicationFactor: 3 });

      // Wait for collection to be created and metrics to stabilize
      let outOfSync = 0;
      for (let i = 0; i < 8; ++i) {
        internal.sleep(0.1);
        outOfSync = getMetric(ep, "arangodb_metadata_shard_followers_out_of_sync_number");
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