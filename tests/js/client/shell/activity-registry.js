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
// / @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
// /
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const { assertTrue } = jsunity.jsUnity.assertions;

const internal = require('internal');
const db = require('internal').db;
const activitiesModule = require('@arangodb/activity-registry');
const IM = global.instanceManager;


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

  async function checkDumpFetchIsAnActivity(dumpId, server) {
    if (internal.isCluster()) {
      internal.arango.POST_RAW(`/_api/dump/next/${dumpId}?dbserver=${server}&batchId=0`, {});
    } else {
      internal.arango.POST_RAW(`/_api/dump/next/${dumpId}?batchId=0`, {});
    }
    const activities = activitiesModule.get_snapshot();
    assertTrue(activities.length > 2);
    assertTrue(activities.filter(activityRestHandlerFilter).length > 0);
    assertTrue(activities.filter(dumpContextFilter).length > 0);
    assertTrue(activities.filter(dumpContextFetchFilter).length > 0);
  }

  return {
    testRegistryIncludesAtLeastRestRequestActivity: function () {
      const activities = activitiesModule.get_snapshot(); // is one REST request
      assertTrue(activities.length > 0);
      assertTrue(activities.filter(activityRestHandlerFilter).length > 0);
    },

    testDumpContextIsAnActivity: function () {
      const c = "my_collection";
      db._create(c);
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
      let activities = activitiesModule.get_snapshot_from_server(server);
      assertTrue(activities.length > 1);
      assertTrue(activities.filter(activityRestHandlerFilter).length > 0);
      assertTrue(activities.filter(dumpContextFilter).length > 0);

      // activity: fetch dump context
      IM.debugSetFailAt("RestDumpHandler::fetch-delay");
      checkDumpFetchIsAnActivity(dumpId, server);
      IM.debugRemoveFailAt("RestDumpHandler::fetch-delay");

      // cleanup
      if (internal.isCluster()) {
        internal.arango.DELETE_RAW(`_api/dump/${dumpId}?dbserver=${server}`);
      } else {
        internal.arango.DELETE_RAW(`_api/dump/${dumpId}`);
      }
      db._drop(c);
    }
  };
}

jsunity.run(activityRegistrySuite);
return jsunity.done();
