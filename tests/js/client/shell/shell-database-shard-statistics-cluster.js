/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let arangodb = require('@arangodb');
let db = arangodb.db;
let { getServersByType } = require('@arangodb/test-helper');

function shardStatisticsDatabaseSuite() {
  'use strict';
  const cn = 'UnitTestsDatabase';

  return {

    setUp: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(cn);
      } catch (err) {}
      db._createDatabase(cn);
    },

    tearDown: function () {
      db._useDatabase("_system");
      db._dropDatabase(cn);
    },

    testShardStatistics: function () {
      db._useDatabase(cn);

      let res = arango.GET("/_api/database/shardStatistics");
      assertTrue(res.hasOwnProperty("result"), res);
      assertEqual(1, res.result.databases);

      let cols = res.result.collections;
      // the exact number of collections can vary with the number of system collections.
      // this is fragile, so not hard-coded here.
      assertTrue(cols > 0, res.result);
      assertEqual(cols * 2, res.result.shards, res.result);
      assertEqual(cols, res.result.leaders, res.result);
      assertEqual(cols, res.result.followers, res.result);
      // all collections but one use distributeShardsLike
      assertEqual(1, res.result.realLeaders, res.result);

      assertTrue(res.result.hasOwnProperty("servers"), res.result);
      // exact number of servers is unclear here
      assertTrue(res.result.servers > 0, res.result);
      
      let previousCols = cols;
      let previousShards = res.result.shards;
      let previousLeaders = res.result.leaders;
      let previousFollowers = res.result.followers;
      let previousRealLeaders = res.result.realLeaders;


      // create a few collections, w/o replicationFactor
      for (let i = 0; i < 5; ++i) {
        db._create("test" + i, { numberOfShards: 3, replicationFactor: 1 });
      }
      
      res = arango.GET("/_api/database/shardStatistics");
      assertEqual(1, res.result.databases);

      cols = res.result.collections;
      assertEqual(previousCols + 5, cols);
      assertEqual(previousShards + 5 * 3, res.result.shards, res.result);
      assertEqual(previousLeaders + 5 * 3, res.result.leaders, res.result);
      assertEqual(previousFollowers, res.result.followers, res.result);
      assertEqual(previousRealLeaders + 5 * 3, res.result.realLeaders, res.result);

      assertTrue(res.result.hasOwnProperty("servers"), res.result);
      // exact number of servers is unclear here
      assertTrue(res.result.servers > 0, res.result);
      
      previousCols = cols;
      previousShards = res.result.shards;
      previousLeaders = res.result.leaders;
      previousFollowers = res.result.followers;
      previousRealLeaders = res.result.realLeaders;
      

      // create a few collections, w/ replicationFactor 2
      for (let i = 0; i < 5; ++i) {
        db._create("testmann" + i, { numberOfShards: 3, replicationFactor: 2 });
      }
      
      res = arango.GET("/_api/database/shardStatistics");
      assertEqual(1, res.result.databases);

      cols = res.result.collections;
      assertEqual(previousCols + 5, cols);
      assertEqual(previousShards + 2 * 5 * 3, res.result.shards, res.result);
      assertEqual(previousLeaders + 5 * 3, res.result.leaders, res.result);
      assertEqual(previousFollowers + 5 * 3, res.result.followers, res.result);
      assertEqual(previousRealLeaders + 5 * 3, res.result.realLeaders, res.result);

      assertTrue(res.result.hasOwnProperty("servers"), res.result);
      // exact number of servers is unclear here
      assertTrue(res.result.servers > 0, res.result);
    },
    
    testShardStatisticsAggregate: function () {
      db._useDatabase(cn);

      // create a few collections, w/o replicationFactor
      for (let i = 0; i < 5; ++i) {
        db._create("test" + i, { numberOfShards: 3, replicationFactor: 2 });
      }
        
      let dbservers = getServersByType("dbserver");
      assertTrue(dbservers.length > 0);

      let partialValues = {
        databases: 0,
        collections: 0,
        shards: 0,
        leaders: 0,
        followers: 0,
        realLeaders: 0,
        servers: 0,
      };

      dbservers.forEach((server) => {
        let part = arango.GET("/_api/database/shardStatistics?DBserver=" + encodeURIComponent(server.id)).result;
        Object.keys(partialValues).forEach((k) => {
          partialValues[k] += part[k];
        });
      });
      
      let allValues = arango.GET("/_api/database/shardStatistics").result;
      assertEqual(allValues.shards, partialValues.shards);
      assertEqual(allValues.leaders, partialValues.leaders);
      assertEqual(allValues.followers, partialValues.followers);
      assertEqual(allValues.realLeaders, partialValues.realLeaders);
    },
  };
}

jsunity.run(shardStatisticsDatabaseSuite);
return jsunity.done();
