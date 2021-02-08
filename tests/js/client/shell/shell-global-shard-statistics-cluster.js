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

function shardStatisticsSuite() {
  'use strict';

  const cn = "UnitTestsDatabase";

  let fetchStats = function (append = '') {
    let old = db._name();
    db._useDatabase("_system");
    let result = arango.GET("/_admin/cluster/shardStatistics" + append).result;
    db._useDatabase(old);
    return result;
  };

  let cleanCollections = function () {
    let old = db._name();
    db._useDatabase(cn);
    for (let i = 0; i < 5; ++i) {
      db._drop(cn + i);
    }
    db._useDatabase(old);
  };

  return {
    setUpAll : function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(cn);
      } catch (err) {}
      db._createDatabase(cn);
    },
    
    tearDownAll : function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(cn);
      } catch (err) {}
    },
    
    testShardStatisticsNewDatabase: function () {
      let baseValues = fetchStats();

      try {
        db._useDatabase("_system");
        db._createDatabase(cn + "xxxx");

        let newValues = fetchStats();

        assertEqual(baseValues.databases + 1, newValues.databases);
        assertTrue(baseValues.collections < newValues.collections);
        assertTrue(baseValues.shards < newValues.shards);
        assertTrue(baseValues.leaders < newValues.leaders);
        assertTrue(baseValues.followers < newValues.followers);
        assertTrue(baseValues.realLeaders < newValues.realLeaders);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn + "xxxx");
      }
    },
    
    testShardStatisticsPerDatabase: function () {
      let baseValues = fetchStats();

      try {
        let expected = ["_system", cn];
        db._useDatabase("_system");
        for (let i = 0; i < 5; ++i) {
          db._createDatabase(cn + "test" + i);
          expected.push(cn + "test" + i);
        }

        let newValues = fetchStats();

        assertEqual(baseValues.databases + 5, newValues.databases);
        assertTrue(baseValues.collections < newValues.collections);
        assertTrue(baseValues.shards < newValues.shards);
        assertTrue(baseValues.leaders < newValues.leaders);
        assertTrue(baseValues.followers < newValues.followers);
        assertTrue(baseValues.realLeaders < newValues.realLeaders);

        newValues = fetchStats("?details=true");
        let keys = Object.keys(newValues);


        expected.forEach((dbName) => {
          assertTrue(newValues.hasOwnProperty(dbName));
          let stats = newValues[dbName];
          assertEqual(1, stats.databases);
          assertTrue(stats.collections > 0);
          assertTrue(stats.shards > 0);
          assertTrue(stats.leaders > 0);
          assertTrue(stats.realLeaders > 0);
          assertTrue(stats.servers > 0);
        });

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
          let part = fetchStats("?DBserver=" + encodeURIComponent(server.id));
          Object.keys(partialValues).forEach((k) => {
            partialValues[k] += part[k];
          });
        });
        
        let allValues = fetchStats();

        assertEqual(allValues.shards, partialValues.shards);
        assertEqual(allValues.leaders, partialValues.leaders);
        assertEqual(allValues.followers, partialValues.followers);
        assertEqual(allValues.realLeaders, partialValues.realLeaders);
      } finally {
        db._useDatabase("_system");
        for (let i = 0; i < 5; ++i) {
          try {
            db._dropDatabase(cn + "test" + i);
          } catch (err) {}
        }
      }
    },

    testShardStatisticsSingle: function () {
      let baseValues = fetchStats();

      try {
        // create some collections
        db._useDatabase(cn);
        for (let i = 0; i < 5; ++i) {
          db._create(cn + i, { numberOfShards: 1, replicationFactor: 1 });
        }

        let newValues = fetchStats();

        assertEqual(baseValues.databases, newValues.databases);
        assertEqual(baseValues.collections + 5, newValues.collections);
        assertEqual(baseValues.shards + 5, newValues.shards);
        assertEqual(baseValues.leaders + 5, newValues.leaders);
        assertEqual(baseValues.followers, newValues.followers);
        assertEqual(baseValues.realLeaders + 5, newValues.realLeaders);
      } finally {
        cleanCollections();
      }
    },
    
    testShardStatisticsSingleWithReplicationFactor: function () {
      let baseValues = fetchStats();

      try {
        // create some collections
        db._useDatabase(cn);
        for (let i = 0; i < 5; ++i) {
          db._create(cn + i, { numberOfShards: 1, replicationFactor: 2 });
        }

        let newValues = fetchStats();

        assertEqual(baseValues.databases, newValues.databases);
        assertEqual(baseValues.collections + 5, newValues.collections);
        assertEqual(baseValues.shards + 5 * 2, newValues.shards);
        assertEqual(baseValues.leaders + 5, newValues.leaders);
        assertEqual(baseValues.followers + 5, newValues.followers);
        assertEqual(baseValues.realLeaders + 5, newValues.realLeaders);
      } finally {
        cleanCollections();
      }
    },
    
    testShardStatisticsMulti: function () {
      let baseValues = fetchStats();

      try {
        // create some collections
        db._useDatabase(cn);
        for (let i = 0; i < 5; ++i) {
          db._create(cn + i, { numberOfShards: 4, replicationFactor: 1 });
        }

        let newValues = fetchStats();

        assertEqual(baseValues.databases, newValues.databases);
        assertEqual(baseValues.collections + 5, newValues.collections);
        assertEqual(baseValues.shards + 5 * 4, newValues.shards);
        assertEqual(baseValues.leaders + 5 * 4, newValues.leaders);
        assertEqual(baseValues.followers, newValues.followers);
        assertEqual(baseValues.realLeaders + 5 * 4, newValues.realLeaders);
      } finally {
        cleanCollections();
      }
    },
    
    testShardStatisticsMultiWithReplicationFactor: function () {
      let baseValues = fetchStats();

      try {
        // create some collections
        db._useDatabase(cn);
        for (let i = 0; i < 5; ++i) {
          db._create(cn + i, { numberOfShards: 4, replicationFactor: 2 });
        }

        let newValues = fetchStats();

        assertEqual(baseValues.databases, newValues.databases);
        assertEqual(baseValues.collections + 5, newValues.collections);
        assertEqual(baseValues.shards + 5 * 4 * 2, newValues.shards);
        assertEqual(baseValues.leaders + 5 * 4, newValues.leaders);
        assertEqual(baseValues.followers + 5 * 4, newValues.followers);
        assertEqual(baseValues.realLeaders + 5 * 4, newValues.realLeaders);
      } finally {
        cleanCollections();
      }
    },

  };
}

jsunity.run(shardStatisticsSuite);
return jsunity.done();
