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
const { getMetric, getEndpointsByType } = require("@arangodb/test-helper");

function metadataMetricsSuite() {
  'use strict';
  const testDbName = "testMetricsDb";
  const testCollectionName = "testMetricsCollection";
  const numberOfDatabasesMetric = "arangodb_metadata_number_of_databases";
  const numberOfCollectionsMetric = "arangodb_metadata_number_of_collections";
  const numberOfShardsMetric = "arangodb_metadata_number_of_shards";

  let assertMetrics = function(endpoints, expectedDatabases, expectedCollections, expectedShards) {
    endpoints.forEach((ep) => {
      const numDatabases = getMetric(ep, numberOfDatabasesMetric);
      assertTrue(numDatabases === expectedDatabases, 
                 `Number of databases found: ${numDatabases}, expected: ${expectedDatabases}`);

      const numCollections = getMetric(ep, numberOfCollectionsMetric);
      assertTrue(numCollections === expectedCollections, 
                 `Number of collections found: ${numCollections}, expected: ${expectedCollections}`);

      if (isCluster) {
        const numShards = getMetric(ep, numberOfShardsMetric);
        assertTrue(numShards === expectedShards, 
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
  };
}

jsunity.run(metadataMetricsSuite);
return jsunity.done();
