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
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
let arangodb = require("@arangodb");
let internal = require("internal");
let db = arangodb.db;
const inst = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

const {
  waitForShardsInSync, eventuallyAssertMetric
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
      IM.debugClearFailAt();
      db._useDatabase("_system");
      try {
        db._dropDatabase(database);
      } catch (e) {
      }
    },

    testCheckMetric: function () {
      const c = db._create("c", {numberOfShards: 1, replicationFactor: 2, writeConcern: 2});
      const [shard, [leaderID, followerID]] = Object.entries(c.shards(true))[0];
      let leader = IM.getInstanceByID(leaderID);
      let follower = IM.getInstanceByID(followerID);

      // this should work
      c.insert({});
      waitForShardsInSync(c.name());

      // query metric, it should be zero
      eventuallyAssertMetric(leader, metricName, (v) => v === 0, `expected ${metricName} to be 0`);

      // suspend the follower
      follower.debugSetFailAt("LogicalCollection::insert");
      follower.debugSetFailAt("SynchronizeShard::disable");

      // trigger a follower drop
      c.insert({});

      // one shard does not have enough in sync follower
      eventuallyAssertMetric(leader, metricName, (v) => v === 1, `expected ${metricName} to be 1`);

      IM.debugClearFailAt();
      waitForShardsInSync(c.name());

      eventuallyAssertMetric(leader, metricName, (v) => v === 0, `expected ${metricName} to be 0 after sync`);

      // now drop the database. we expect the metric to be gone
      db._useDatabase("_system");
      db._dropDatabase(database);

      eventuallyAssertMetric(leader, metricName, (v) => v === 0, `expected ${metricName} to be 0 after drop`);
    },

    testMetricAfterCollectionDrop: function () {
      const c = db._create("c", {numberOfShards: 1, replicationFactor: 2, writeConcern: 2});
      const [shard, [leaderID, followerID]] = Object.entries(c.shards(true))[0];
      let leader = IM.getInstanceByID(leaderID);
      let follower = IM.getInstanceByID(followerID);

      // this should work
      c.insert({});
      waitForShardsInSync(c.name());

      // query metric, it should be zero
      eventuallyAssertMetric(leader, metricName, (v) => v === 0, `expected ${metricName} to be 0`);

      // suspend the follower
      follower.debugSetFailAt("LogicalCollection::insert");
      follower.debugSetFailAt("SynchronizeShard::disable");

      // trigger a follower drop
      c.insert({});

      // one shard does not have enough in sync follower
      eventuallyAssertMetric(leader, metricName, (v) => v === 1, `expected ${metricName} to be 1`);

      // Lets drop the collection and see if the metric changes
      db._drop(c.name());

      eventuallyAssertMetric(leader, metricName, (v) => v === 0, `expected ${metricName} to be 0 after drop`);
    },

    testCheckMetricChangeWriteConcern: function () {

      const c = db._create("c", {numberOfShards: 1, replicationFactor: 2, writeConcern: 2});
      const [shard, [leaderID, followerID]] = Object.entries(c.shards(true))[0];
      let leader = IM.getInstanceByID(leaderID);
      let follower = IM.getInstanceByID(followerID);

      // this should work
      c.insert({});
      waitForShardsInSync(c.name());

      // query metric, it should be zero
      eventuallyAssertMetric(leader, metricName, (v) => v === 0, `expected ${metricName} to be 0`);

      // suspend the follower
      follower.debugSetFailAt("LogicalCollection::insert");
      follower.debugSetFailAt("SynchronizeShard::disable");

      // trigger a follower drop
      c.insert({});

      // one shard does not have enough in sync follower
      eventuallyAssertMetric(leader, metricName, (v) => v === 1, `expected ${metricName} to be 1`);

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

      eventuallyAssertMetric(leader, metricName, (v) => v === 0, `expected ${metricName} to be 0 after write concern change`);

      IM.debugClearFailAt();
      waitForShardsInSync(c.name());

      // now drop the database. we expect the metric to be gone
      db._useDatabase("_system");
      db._dropDatabase(database);

      eventuallyAssertMetric(leader, metricName, (v) => v === 0, `expected ${metricName} to be 0 after database drop`);
    },

    testFollowerNotCountedAfterWriteConcernChange: function () {

      const c = db._create("c", {numberOfShards: 1, replicationFactor: 3, writeConcern: 2});
      const [shard, servers] = Object.entries(c.shards(true))[0];
      const instances = servers.map((id) => IM.getInstanceByID(id));

      c.insert({});
      waitForShardsInSync(c.name());

      const assertNoneCounted = function (context) {
        instances.forEach((server, i) => {
          eventuallyAssertMetric(server, metricName, (v) => v === 0,
            `expected ${metricName} to be 0 on ${i === 0 ? "the leader" : "follower " + i} ${context}`);
        });
      };

      assertNoneCounted("in a healthy state");

      // Raising writeConcern makes the maintenance push the new properties to
      // every replica. A follower does not maintain a follower list, so it must
      // not be counted, and nothing on a follower would ever recompute it.
      const setWriteConcern = function (value) {
        c.properties({writeConcern: value});
        let tries = 20;
        while (c.properties().writeConcern !== value) {
          if (--tries === 0) {
            assertTrue(false, `writeConcern change to ${value} was not applied within 10 seconds`);
          }
          internal.wait(0.5);
        }
      };

      setWriteConcern(3);
      assertNoneCounted("after raising writeConcern to 3");

      setWriteConcern(2);
      assertNoneCounted("after lowering writeConcern to 2");
    },
  };
}

jsunity.run(WriteConcernReadOnlyMetricSuite);

return jsunity.done();
