/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertNull, assertTrue, assertUndefined, fail */

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

let jsunity = require("jsunity");
let arangodb = require("@arangodb");
let internal = require("internal");
let db = arangodb.db;

const {
  waitForShardsInSync, getMetric, getUrlById, getAllMetric
} = require('@arangodb/test-helper');

const database = "WriteConcernReadOnlyMetricDatabase";

function WriteConcernReadOnlyMetricSuite() {

  return {

    setUp: function () {
      db._createDatabase(database);
      db._useDatabase(database);
    },

    tearDown: function () {
      global.instanceManager.debugClearFailAt();
      db._useDatabase("_system");
      try {
        db._dropDatabase(database);
      } catch (e) {
      }
    },

    testCheckMetric: function () {

      const c = db._create("c", {numberOfShards: 1, replicationFactor: 2, writeConcern: 2});
      const [shard, [leader, follower]] = Object.entries(c.shards(true))[0];

      // this should work
      c.insert({});
      waitForShardsInSync(c.name());

      // query metric, it should be zero
      let metricValue = getMetric(getUrlById(leader), "arangodb_vocbase_shards_read_only_by_write_concern");
      assertEqual(metricValue, 0);

      // suspend the follower
      global.instanceManager.debugSetFailAt("LogicalCollection::insert", '', follower);
      global.instanceManager.debugSetFailAt("SynchronizeShard::disable", '', follower);

      // trigger a follower drop
      c.insert({});

      metricValue = getMetric(getUrlById(leader), "arangodb_vocbase_shards_read_only_by_write_concern");
      assertEqual(metricValue, 1); // one shard does not have enough in sync follower

      global.instanceManager.debugClearFailAt();
      waitForShardsInSync(c.name());

      metricValue = getMetric(getUrlById(leader), "arangodb_vocbase_shards_read_only_by_write_concern");
      assertEqual(metricValue, 0);

      // now drop the database. we expect the metric to be gone
      db._useDatabase("_system");
      db._dropDatabase(database);

      let k = 0;
      for (; k < 100; k++) {
        const metrics = getAllMetric(getUrlById(leader), '');
        if (metrics.indexOf(database) === -1) {
          break;
        }
        internal.sleep(0.5);
      }
      assertNotEqual(k, 100);
    },
  };
}

jsunity.run(WriteConcernReadOnlyMetricSuite);

return jsunity.done();
