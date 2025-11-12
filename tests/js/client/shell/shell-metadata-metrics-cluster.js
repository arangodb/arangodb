/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, assertFalse */

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

const jsunity = require('jsunity');
const db = require('@arangodb').db;
const { getMetric, getEndpointsByType } = require('@arangodb/test-helper');

function metadataMetricsSuite() {
  'use strict';

  const metrics = [
    'arangodb_metadata_number_of_databases',
    'arangodb_metadata_number_of_collections',
    'arangodb_metadata_number_of_shards',
  ];
  
  const testDbName = "testMetadataMetricsDB";
      
  return {

    setUp: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(testDbName);
      } catch (e) {
        // Ignore errors during cleanup
      }
    },
    
    tearDown: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(testDbName);
      } catch (e) {
        // Ignore errors during cleanup
      }
    },
    
    testMetadataMetrics: function () {
      let endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);

      endpoints.forEach((ep) => {
        let numDatabases = getMetric(ep, 'arangodb_metadata_number_of_databases');
        assertEqual(1, numDatabases, "Should have 1 database (_system)");
        
        let numCollections = getMetric(ep, 'arangodb_metadata_number_of_collections');
        assertEqual(12, numCollections, "Should have 12 collections");
        
        let numShards = getMetric(ep, 'arangodb_metadata_number_of_shards');
        assertEqual(12, numShards, "Should have 12 shards");
      });
    },

    testMetadataMetricsWithCollectionCreation: function () {
        let endpoints = getEndpointsByType('coordinator');
        assertTrue(endpoints.length > 0);
  
        endpoints.forEach((ep) => {
            let numDatabases = getMetric(ep, 'arangodb_metadata_number_of_databases');
            db._useDatabase("_system");
            db._databases();
            print(db._databases());
            assertEqual(1, numDatabases, "Should have 1 database (_system)");

            let numCollections = getMetric(ep, 'arangodb_metadata_number_of_collections');
            assertEqual(12, numCollections, "Should have 12 collections");

            let numShards = getMetric(ep, 'arangodb_metadata_number_of_shards');
            assertEqual(12, numShards, "Should have 12 shards");
        });

        db._useDatabase("_system");
        db._createDatabase(testDbName);
        db._useDatabase(testDbName);
        db._create("testMetadataMetricsCollection", { numberOfShards: 4 });
        require("internal").sleep(2);

        endpoints.forEach((ep) => {
            let numDatabases = getMetric(ep, 'arangodb_metadata_number_of_databases');
            assertEqual(2, numDatabases, "Should have 2 databases");

            let numCollections = getMetric(ep, 'arangodb_metadata_number_of_collections');
            assertEqual(21, numCollections, "Should have 12 collections");

            let numShards = getMetric(ep, 'arangodb_metadata_number_of_shards');
            assertEqual(24, numShards, "Should have 12 shards");
        });
    },
  };
}

jsunity.run(metadataMetricsSuite);
return jsunity.done();

