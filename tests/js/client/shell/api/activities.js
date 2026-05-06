/* jshint esnext: true */

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

'use strict';

const jsunity = require('jsunity');
const { assertTrue } = jsunity.jsUnity.assertions;

const internal = require('internal');
const db = require('internal').db;
const activitiesModule = require('@arangodb/activities');
const dump = require('@arangodb/arango-dump');
const arangosh = require('@arangodb/arangosh');
const IM = global.instanceManager;

const c = "my_collection";

function activityRegistrySuite() {
  function activityRestHandlerFilter() {
    return (a) => {
      return a.type === "RestHandler" && a.data.handler === "ActivityRegistryRestHandler";
    };
  }
  function dumpRestHandlerFilter() {
    return (a) => {
      return a.type === "RestHandler" && a.data.handler === "RestDumpHandler";
    };
  }
  function dumpContextFilter() {
    return (a) => {
      return a.type === "RocksDBDump";
    };
  }
  function dumpContextFetchFilter() {
    return (a) => {
      return a.type === "RocksDBDumpNext";
    };
  }
  function assertArrayLengthLargerThan(array, length) {
    assertTrue(array.length > length, `Failed: ${array.length} > ${length}, ${JSON.stringify(array)}`);
  }

  function fetchDumpAsynchronously(dumpId, server) {
    const res = arangosh.checkRequestResult(dump.next(dumpId, 0, undefined, server, {"x-arango-async": "store"}));
    assertTrue(res.headers.hasOwnProperty("x-arango-async-id"));
    return res.headers["x-arango-async-id"];
  }

  return {
    setUpAll: function () {
      db._create(c);
    },

    tearDownAll: function () {
      db._drop(c);
    },

    testRegistryIncludesAtLeastRestRequestActivity: function () {
      const activities = activitiesModule.get_snapshot_bare(); // is one REST request
      assertArrayLengthLargerThan(activities, 0);
      assertArrayLengthLargerThan(activities.filter(activityRestHandlerFilter()), 0);
    },

    testDumpContextIsAnActivity: function () {
      let server;
      let shard;
      if (internal.isCluster()) {
        const shards = db[c].shards(true);
        shard = Object.keys(shards)[0];
        server = shards[shard][0];
      } else {
        shard = c;
      }
      const createDump = dump.start({ shards: [shard], ttl: 1000.0 }, server);

      // activity: dump context
      const dumpId = dump.get_batch_id_from_start_response(arangosh.checkRequestResult(createDump));
      const activities = activitiesModule.get_snapshot_bare(server);
      assertArrayLengthLargerThan(activities, 1);
      assertArrayLengthLargerThan(activities.filter(activityRestHandlerFilter()), 0);
      assertArrayLengthLargerThan(activities.filter(dumpContextFilter()), 0);

      // activity: fetch dump context
      try {
        IM.debugSetFailAt("RestDumpHandler::fetch-delay");
        
        const cursorId = fetchDumpAsynchronously(dumpId, server); // Rest call is kept busy with failure point

        // make sure that dump context fetch activity is created before activities are requested
        let maxWait = 5;
        let activities;
        while (maxWait > 0) {
          activities = activitiesModule.get_snapshot_bare(server);
          if (activities.filter(dumpContextFetchFilter()).length > 0) {
            break;
          }
          internal.wait(1);
          maxWait--;
        }
        const dumpContextFetchActivities = activities.filter(dumpContextFetchFilter());
        assertArrayLengthLargerThan(dumpContextFetchActivities, 0);
        assertArrayLengthLargerThan(activities, 3);
        assertArrayLengthLargerThan(activities.filter(activityRestHandlerFilter()), 0);
        assertArrayLengthLargerThan(activities.filter(dumpRestHandlerFilter()), 0);
        const dumpContextActivities = activities.filter(dumpContextFilter());
        assertArrayLengthLargerThan(dumpContextActivities, 0);

        // check that one of the dump context activities is the parent activity
        // of one of the dump fetch activities
        const dumpContextActivityIds = dumpContextActivities.map((a) => a.id);
        const dumpContextFetchActivityParents = dumpContextFetchActivities.map((a) => a.parent);
        assertArrayLengthLargerThan(dumpContextActivityIds
          .filter((id) => dumpContextFetchActivityParents.includes(id)), 0);

        // stop first dump-fetch Rest call with a second call
        const cursorId2 = fetchDumpAsynchronously(dumpId, server);
        internal.arango.DELETE_RAW(`/_api/job/${cursorId}`);
        internal.arango.DELETE_RAW(`/_api/job/${cursorId2}`);

      } finally {
        IM.debugClearFailAt();
      }

      // cleanup
      arangosh.checkRequestResult(dump.delete(dumpId, server));
    }
  };
}

jsunity.run(activityRegistrySuite);
return jsunity.done();
