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
const activitiesModule = require('@arangodb/activity-registry');
const IM = global.instanceManager;

const c = "my_collection";

function activityRegistrySuite() {
  function activityRestHandlerFilter() {
    return (a) => {
      const name = a.name.toLowerCase();
      return name.includes("activity") && name.includes("registry") && name.includes("rest") && a.state === "Active";
    };
  }
  function dumpContextFilter() {
    return (a) => {
      const name = a.name.toLowerCase();
      return name.includes("dump") && name.includes("context") && a.state === "Active";
    };
  }
  function dumpContextFetchFilter() {
    return (a) => {
      const name = a.name.toLowerCase();
      return name.includes("dump") && name.includes("context") && name.includes("fetch") && a.state === "Active";
    };
  }
  function assertArrayLengthLargerThan(array, length) {
    assertTrue(array.length > length, `Failed: ${array.length} > ${length}, ${JSON.stringify(array)}`);
  }

  function fetchDumpAsynchronously(dumpId, server) {
    let res;
    if (internal.isCluster()) {
      res = internal.arango.POST_RAW(`/_api/dump/next/${dumpId}?dbserver=${server}&batchId=0`, {}, {"x-arango-async": "store"});
    } else {
      res = internal.arango.POST_RAW(`/_api/dump/next/${dumpId}?batchId=0`, {}, {"x-arango-async": "store"});
    }
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
      const activities = activitiesModule.get_snapshot(); // is one REST request
      assertArrayLengthLargerThan(activities, 0);
      assertArrayLengthLargerThan(activities.filter(activityRestHandlerFilter), 0);
    },

    testDumpContextIsAnActivity: function () {
      let createDump;
      let server;
      if (internal.isCluster()) {
        const shards = db[c].shards(true);
        const shard = Object.keys(shards)[0];
        server = shards[shard][0];
        createDump = internal.arango.POST_RAW(`/_api/dump/start?dbserver=${server}`, { shards: [shard], ttl: 1000.0 });
      } else {
        createDump = internal.arango.POST_RAW(`/_api/dump/start`, { shards: [c], ttl: 1000.0 });
      }

      // activity: dump context
      const dumpId = createDump.headers["x-arango-dump-id"];
      const activities = activitiesModule.get_snapshot(server);
      assertArrayLengthLargerThan(activities, 1);
      assertArrayLengthLargerThan(activities.filter(activityRestHandlerFilter), 0);
      assertArrayLengthLargerThan(activities.filter(dumpContextFilter), 0);

      // activity: fetch dump context
      try {
        IM.debugSetFailAt("RestDumpHandler::fetch-delay");
        
        const cursorId = fetchDumpAsynchronously(dumpId, server); // Rest call sleeps
        const activities = activitiesModule.get_snapshot(server);
        internal.arango.DELETE_RAW(`/_api/job/${cursorId}`);

        assertArrayLengthLargerThan(activities, 3); // includes sleeping dump-fetch Rest call
        assertArrayLengthLargerThan(activities.filter(activityRestHandlerFilter), 2); // same here
        assertArrayLengthLargerThan(activities.filter(dumpContextFilter), 0);
        assertArrayLengthLargerThan(activities.filter(dumpContextFetchFilter), 0);

      } finally {
        IM.debugClearFailAt();
      }

      // cleanup
      if (internal.isCluster()) {
        internal.arango.DELETE_RAW(`_api/dump/${dumpId}?dbserver=${server}`);
      } else {
        internal.arango.DELETE_RAW(`_api/dump/${dumpId}`);
      }
    }
  };
}

jsunity.run(activityRegistrySuite);
return jsunity.done();
