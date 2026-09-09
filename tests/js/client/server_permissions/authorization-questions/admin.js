/*jshint globalstrict:false, strict:false */
/* global getOptions, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

// Authorization questions asked by a collection of /_admin/* endpoints
// (auth reload, cluster, compact, crashes, database, shutdown).
//
// Observation-based counterpart of tests/api/apitests/admin.mjs.
//
// Handlers: RestAuthReloadHandler, RestAdminClusterHandler, RestCompactHandler,
// RestCrashHandler, RestAdminDatabaseHandler, RestShutdownHandler.
//
// Every request first asks `UseApiVersion version=1` and then
// `UseDatabase name=<db> level=read`. Beyond that, the cluster handler is the
// interesting case: several sub-handlers reject non-coordinator requests BEFORE
// they run the per-user auth check, so on a single server those endpoints emit
// ONLY the base question. Sub-handlers that run canUseAdminAction FIRST still
// emit their admin question even on a single server. isSuperuser checks never
// log a question. The mapping below was derived from RestAdminClusterHandler.cpp
// (check ordering: auth-first vs isCoordinator-first).

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false',
    // disable so it doesn't spoil the test output:
    'server.statistics': 'false'
  };
}

const jsunity = require('jsunity');
const {
  beginObserve,
  endObserve,
  disableObserve,
  assertPermissions
} = require('@arangodb/testutils/permissions-observer');
const {
  setUpApiTestData,
  tearDownApiTestData,
  DB,
  DOC_COLLECTION,
  singleOnly,
  clusterOnly
} = require('@arangodb/testutils/apitest-fixtures');

function adminApiAuthzSuite () {

  return {
    // the fixture creates database d + collection c, needed only so that the
    // /_db/d/... collectionShardDistribution request resolves its database
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
    },

    // POST /_admin/auth/reload - RestAuthReloadHandler asks
    // canUseAdminAction(AdminAuthReload) (execute:44).
    testAuthReload: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/auth/reload`, {});
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminAuthReload",
        "UseCollection db=_system name=_users level=read"
      ], endObserve());
    },

    // GET /_admin/cluster/collectionShardDistribution - handler rejects
    // non-coordinators FIRST (line 1456), before canUseAdminAction(AdminClusterInfo);
    // on a single server only the base question fires. Base db is `d`.
    testCollectionShardDistribution: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_admin/cluster/collectionShardDistribution?collection=${DOC_COLLECTION}`);
      // a single server rejects the request before asking
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=d level=read",
        ...clusterOnly([
          "AdminClusterInfo"
        ])
      ], endObserve());
    },

    // GET /_admin/cluster/health - no admin check (AUTHEN)
    testClusterHealth: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/health`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/cluster/maintenance - handleMaintenance asks
    // canUseAdminAction(AdminMaintenance) FIRST (line 1849), and works on a
    // single server (the agency rejection comes after the auth check).
    testGetClusterMaintenance: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/maintenance`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminMaintenance"
      ], endObserve());
    },

    // PUT /_admin/cluster/maintenance - same handleMaintenance auth (AdminMaintenance).
    // Body "off" is a harmless no-op. NB: body is a bare JSON string.
    testPutClusterMaintenance: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/cluster/maintenance`, "off");
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminMaintenance"
      ], endObserve());
    },

    // GET /_admin/cluster/nodeEngine - proxy handler rejects non-coordinators
    // FIRST (handleProxyGetRequest:1327); AUTHEN, so base question only.
    testNodeEngine: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/nodeEngine`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/cluster/nodeStatistics - non-coordinator rejected first (AUTHEN)
    testNodeStatistics: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/nodeStatistics`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/cluster/nodeVersion - non-coordinator rejected first (AUTHEN)
    testNodeVersion: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/nodeVersion`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/cluster/numberOfServers - handleNumberOfServers rejects
    // non-coordinators FIRST (line 2073); base question only.
    testGetNumberOfServers: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/numberOfServers`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // PUT /_admin/cluster/numberOfServers - non-coordinator rejected first;
    // the PUT branch would only use canUseHardenedAction(AdminMaintenance).
    // hardened action -> no question without --server.harden
    testPutNumberOfServers: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/cluster/numberOfServers`, {});
      // a single server rejects the request before asking
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        ...clusterOnly([
          "AdminMaintenance"
        ])
      ], endObserve());
    },

    // GET /_admin/cluster/rebalance - handleRebalance rejects non-coordinators
    // FIRST (line 2858), before canUseAdminAction(AdminRebalance); base only.
    testGetRebalance: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/rebalance`);
      // a single server rejects the request before asking
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        ...clusterOnly([
          "AdminRebalance"
        ])
      ], endObserve());
    },

    // PUT /_admin/cluster/rebalance - non-coordinator rejected first; base only.
    testPutRebalance: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/cluster/rebalance`, {});
      // a single server rejects the request before asking
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        ...clusterOnly([
          "AdminRebalance"
        ])
      ], endObserve());
    },

    // GET /_admin/cluster/shardDistribution - handleShardDistribution rejects
    // non-coordinators FIRST (line 1405); base only.
    testShardDistribution: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/shardDistribution`);
      // a single server rejects the request before asking
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        ...clusterOnly([
          "AdminClusterInfo"
        ])
      ], endObserve());
    },

    // GET /_admin/cluster/shardStatistics - handleShardStatistics asks
    // canUseAdminAction(AdminClusterInfo) FIRST (line 695), before the
    // coordinator check; the question fires even on a single server.
    testShardStatistics: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/shardStatistics`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminClusterInfo"
      ], endObserve());
    },

    // GET /_admin/cluster/statistics - proxy handler rejects non-coordinators
    // FIRST (AUTHEN); base only.
    testClusterStatistics: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/statistics`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // POST /_admin/cluster/cancelAgencyJob - handleCancelJob asks
    // canUseAdminAction(AdminMoveShards) FIRST (line 1069).
    testCancelAgencyJob: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/cluster/cancelAgencyJob`,
                      { id: "nonexistent-job-apitester" });
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminMoveShards"
      ], endObserve());
    },

    // POST /_admin/cluster/cleanOutServer - handleSingleServerJob asks
    // canUseAdminAction(AdminMoveShards) FIRST (line 1241).
    testCleanOutServer: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/cluster/cleanOutServer`, {});
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminMoveShards"
      ], endObserve());
    },

    // GET /_admin/cluster/maintenance/{serverId} - handleDBServerMaintenance
    // asks canUseAdminAction(AdminMaintenance) FIRST (line 1883).
    testGetDBServerMaintenance: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/maintenance/nonexistent`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminMaintenance"
      ], endObserve());
    },

    // PUT /_admin/cluster/maintenance/{serverId} - same handleDBServerMaintenance
    // auth (AdminMaintenance). Body {mode:"normal"} is harmless.
    testPutDBServerMaintenance: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/cluster/maintenance/nonexistent`,
                     { mode: "normal" });
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminMaintenance"
      ], endObserve());
    },

    // POST /_admin/cluster/moveShard - handleMoveShard rejects non-coordinators
    // FIRST (line 779), before the AdminMoveShards/UseCollection check; base only.
    testMoveShard: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/cluster/moveShard`,
                      { database: "_system", collection: "nonexistent_apitester",
                        shard: "s1", fromServer: "from", toServer: "to" });
      // a single server rejects the request before asking
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        ...clusterOnly([
          "AdminMoveShards"
        ])
      ], endObserve());
    },

    // GET /_admin/cluster/queryAgencyJob - handleQueryJobStatus asks
    // canUseAdminAction(AdminMoveShards) FIRST (line 989).
    testQueryAgencyJob: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/queryAgencyJob?id=nonexistent-job-apitester-99999`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminMoveShards"
      ], endObserve());
    },

    // POST /_admin/cluster/rebalanceShards - handleRebalanceShards rejects
    // non-coordinators FIRST (line 2527), before canUseAdminAction(AdminRebalance);
    // base only.
    testRebalanceShards: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/cluster/rebalanceShards`, {});
      // a single server rejects the request before asking
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        ...clusterOnly([
          "AdminRebalance"
        ])
      ], endObserve());
    },

    // POST /_admin/cluster/removeServer - handleRemoveServer asks
    // canUseAdminAction(AdminRemoveServer) FIRST (line 651).
    testRemoveServer: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/cluster/removeServer`,
                      { server: "PRMR-nonexistent-apitester" });
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminRemoveServer"
      ], endObserve());
    },

    // POST /_admin/cluster/resignLeadership - handleSingleServerJob asks
    // canUseAdminAction(AdminMoveShards) FIRST (line 1241).
    testResignLeadership: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/cluster/resignLeadership`, {});
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminMoveShards"
      ], endObserve());
    },

    // PUT /_admin/cluster/uniqId - handleUniqId rejects non-coordinators FIRST
    // (line 2104), before canUseAdminAction(AdminMaintenance); base only.
    testUniqId: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/cluster/uniqId?number=1`, {});
      // a single server rejects the request before asking
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        ...clusterOnly([
          "AdminMaintenance"
        ])
      ], endObserve());
    },

    // GET /_admin/cluster/vpackSortMigration/check - handleVPackSortMigration
    // gates on isSuperuserOrDisabled() (line 3036, no observed question); base only.
    testVPackSortMigration: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/cluster/vpackSortMigration/check`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // PUT /_admin/compact - RestCompactHandler gates on isSuperuserOrDisabled()
    // (execute:46, no observed question); base only.
    // AUDIT: triggers a full RocksDB compaction (safe/idempotent, small test DB).
    testCompact: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/compact`, {});
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/crashes - RestCrashHandler asks canUseAdminAction(AdminCrashHandler)
    // (executeAsync:42).
    testListCrashes: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/crashes`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminCrashHandler"
      ], endObserve());
    },

    // GET /_admin/crashes/{id} - same AdminCrashHandler check.
    testGetCrash: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/crashes/nonexistent`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminCrashHandler"
      ], endObserve());
    },

    // DELETE /_admin/crashes/{id} - same AdminCrashHandler check.
    testDeleteCrash: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_admin/crashes/nonexistent`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminCrashHandler"
      ], endObserve());
    },

    // GET /_admin/shutdown - RestShutdownHandler asks canUseAdminAction(AdminShutdown)
    // for BOTH verbs before branching (execute:59); the GET branch then returns
    // 405 on a single server, so firing this is safe.
    testGetShutdown: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/shutdown`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminShutdown"
      ], endObserve());
    },

    // DELETE /_admin/shutdown - RestShutdownHandler asks canUseAdminAction(AdminShutdown)
    // then actually shuts the server down.
    // AUDIT: fires a REAL shutdown when run as root (rw on _system passes the
    // AdminShutdown check) and would kill the test server. The request is
    // intentionally NOT fired here. Expected questions:
    //   ["UseDatabase name=_system level=read", "AdminShutdown"].
    // A human must decide how to guard this (e.g. observe as a user without
    // _system rw, expecting the auth check to reject before shutdown).
    testDoShutdown: function () {
      // intentionally disabled - see AUDIT above.
      // beginObserve();
      // arango.DELETE_RAW(`/_db/_system/_admin/shutdown`);
      // assertPermissions(["UseDatabase name=_system level=read",
      //                    "AdminShutdown"], endObserve());
    },
  };
}

jsunity.run(adminApiAuthzSuite);
return jsunity.done();
