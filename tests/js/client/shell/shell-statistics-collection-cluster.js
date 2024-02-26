/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, assertNull, assertNotNull, assertMatch */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const db = arangodb.db;
const request = require("@arangodb/request");
const { getDBServers } = require('@arangodb/test-helper');


function statisticsCollectionsSuite() {
  'use strict';
  const collections = ["_statistics", "_statistics15", "_statisticsRaw"];
  const cn = 'UnitTestsDatabase';

  return {

    testStatisticsCollectionsInSystemDatabaseOnCoordinator: function () {
      db._useDatabase("_system");
      collections.forEach((s) => {
        assertNotNull(db._collection(s));
      });
    },
    
    testStatisticsCollectionsInOtherDatabaseOnCoordinator: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase(cn);
      } catch (err) {}
      db._createDatabase(cn);
      try {
        db._useDatabase(cn);
        collections.forEach((s) => {
          assertNull(db._collection(s));
        });
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },

    testStatisticsCollectionsOnDBServer: function () {
      let dbservers = getDBServers();
      dbservers.forEach((server) => {
        let result = request({ method: "GET", url: server.url + "/_api/collection", body: {} });
        assertEqual(200, result.json.code);
        let shards = result.json.result;
        assertTrue(Array.isArray(shards));
        // verify that there are no shards named _statistics, _statistics15, _statisticsRaw
        collections.forEach((collectionName) => {
          let shardNames = shards.map((s) => s.name);
          shardNames.forEach((s) => {
            assertMatch(/^s[0-9]+$/, s);
          });
          assertEqual(-1, shardNames.indexOf(collectionName));
        });
      });
    },
  };
}

jsunity.run(statisticsCollectionsSuite);
return jsunity.done();
