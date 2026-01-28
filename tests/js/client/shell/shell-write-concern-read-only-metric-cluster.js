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
  waitForShardsInSync, getMetric, getUrlById, getAllMetric, eventuallyAssertEqual
} = require('@arangodb/test-helper');

const database = "WriteConcernReadOnlyMetricDatabase";
const metricName = "arangodb_vocbase_shards_read_only_by_write_concern";

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

    testLadidaMetric: function () {
      const c = db._create("c", {numberOfShards: 1, replicationFactor: 2, writeConcern: 2});
      const [shard, [leader, follower]] = Object.entries(c.shards(true))[0];

      // this should work
      c.insert({});
      waitForShardsInSync(c.name());

      // query metric, it should be zero
      eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 0);

      // suspend the follower
      global.instanceManager.debugSetFailAt("LogicalCollection::insert", '', follower);
      global.instanceManager.debugSetFailAt("SynchronizeShard::disable", '', follower);

      // trigger a follower drop
      c.insert({});

      // one shard does not have enough in sync follower
      eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 1);

      // Lets drop the collection and see if the metric changes
      db._drop(c.name());

      global.instanceManager.debugClearFailAt();

      eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 0);
    },

    testCheckMetric: function () {
      const c = db._create("c", {numberOfShards: 1, replicationFactor: 2, writeConcern: 2});
      const [shard, [leader, follower]] = Object.entries(c.shards(true))[0];

      // this should work
      c.insert({});
      waitForShardsInSync(c.name());

      // query metric, it should be zero
      eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 0);

      // suspend the follower
      global.instanceManager.debugSetFailAt("LogicalCollection::insert", '', follower);
      global.instanceManager.debugSetFailAt("SynchronizeShard::disable", '', follower);

      // trigger a follower drop
      c.insert({});

      // one shard does not have enough in sync follower
      eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 1);

      global.instanceManager.debugClearFailAt();
      waitForShardsInSync(c.name());

      eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 0);

      // now drop the database. we expect the metric to be gone
      db._useDatabase("_system");
      db._dropDatabase(database);

      eventuallyAssertEqual(() => getAllMetric(getUrlById(leader), '').indexOf(database), -1);
    },

    // This is evident in _system database, where the metric is not reset after collection drop.
    testMetricAfterCollectionDrop: function () {
      db._useDatabase("_system");
      const c = db._create("c", {numberOfShards: 1, replicationFactor: 2, writeConcern: 2});
      try {
        const [shard, [leader, follower]] = Object.entries(c.shards(true))[0];

        // this should work
        c.insert({});
        waitForShardsInSync(c.name());

        // query metric, it should be zero
        eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 0);

        // trigger a follower drop
        c.insert({});

        // one shard does not have enough in sync follower
        eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 1);

        // Lets drop the collection and see if the metric changes
        db._drop(c.name());

        eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 0);
      } finally {
        db._drop(c.name());
      }
    },

    testCheckMetricChangeWriteConcern: function () {

      const c = db._create("c", {numberOfShards: 1, replicationFactor: 2, writeConcern: 2});
      const [shard, [leader, follower]] = Object.entries(c.shards(true))[0];

      // this should work
      c.insert({});
      waitForShardsInSync(c.name());

      // query metric, it should be zero
      eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 0);

      // suspend the follower
      global.instanceManager.debugSetFailAt("LogicalCollection::insert", '', follower);
      global.instanceManager.debugSetFailAt("SynchronizeShard::disable", '', follower);

      // trigger a follower drop
      c.insert({});

      // one shard does not have enough in sync follower
      eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 1);

      // change write concern
      c.properties({writeConcern: 1});

      // This waits until the property is set in the agency, however,
      // it is possible that the leader needs some more time to receive
      // this information, therefore we do a relatively short retry loop
      // for the next operation:

      let tries = 10;
      while (true) {
        // should work fine:
        try {
          c.insert({});
          break;
        } catch (err) {
        }
        if (--tries === 0) {
          assertTrue(false, "insert() should have worked within 5 seconds");
        }
        internal.wait(0.5);
      }

      eventuallyAssertEqual(() => getMetric(getUrlById(leader), metricName), 0);

      global.instanceManager.debugClearFailAt();
      waitForShardsInSync(c.name());

      // now drop the database. we expect the metric to be gone
      db._useDatabase("_system");
      db._dropDatabase(database);

      eventuallyAssertEqual(() => getAllMetric(getUrlById(leader), '').indexOf(database), -1);
    },
  };
}

jsunity.run(WriteConcernReadOnlyMetricSuite);

return jsunity.done();
