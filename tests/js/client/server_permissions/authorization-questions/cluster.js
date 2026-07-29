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

// Authorization questions asked by the /_api/cluster endpoint family.
//
// Observation-based counterpart of tests/api/apitests/cluster.mjs.
//
// Handler: arangod/Cluster/RestClusterHandler.cpp
//
// IMPORTANT / AUDIT: cluster-only. The /_api/cluster prefix is ONLY registered
// when cluster mode is enabled. On a single-server deployment the whole prefix is
// unregistered, so every request returns 404 (routing miss) and NO authorization
// question fires at all. The assertions below therefore document the questions the
// handler asks on a COORDINATOR - each test is marked accordingly and is expected
// to observe nothing on a single server.
//
// On a coordinator, every request except `endpoints` first asks
// `UseDatabase name=_system level=read` in RestHandler::checkUserCanAccess()
// (the routes have no /_db/ prefix, so the database is the connected _system).
// Per-route ExecContext::can() questions:
//
//   agency-cache (GET)          canUseAdminAction(AdminReadAgency)   -> `AdminReadAgency`
//   agency-dump (GET)           isCoordinator() first, then
//                               canUseAdminAction(AdminReadAgency)   -> `AdminReadAgency`
//   cluster-info (GET)          canUseAdminAction(AdminClusterInfo)  -> `AdminClusterInfo`
//   cluster-info/<sub>          canUseAdminAction(AdminClusterInfo)  -> `AdminClusterInfo`
//                               then (production build) isSuperuser() guard, which
//                               is NOT a can() question, so nothing extra observed
//   endpoints (GET)             RestClusterHandler::checkUserCanAccess() returns OK
//                               for any authenticated user WITHOUT asking - and the
//                               handler needs no admin RBAC check -> no question at all

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true'
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
  tearDownApiTestData
} = require('@arangodb/testutils/apitest-fixtures');

function clusterApiAuthzSuite () {
  const useSystem = `UseDatabase name=_system level=read`;
  const adminClusterInfo = `AdminClusterInfo`;
  const adminReadAgency = `AdminReadAgency`;
  const base = `/_db/_system/_api/cluster`;

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
    },

    // GET /_api/cluster/agency-cache - canUseAdminAction(AdminReadAgency)
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testAgencyCache: function () {
      beginObserve();
      arango.GET_RAW(`${base}/agency-cache`);
      assertPermissions([useSystem, adminReadAgency], endObserve());
    },

    // GET /_api/cluster/agency-dump - isCoordinator() then
    // canUseAdminAction(AdminReadAgency)
    // AUDIT: cluster-only - route not registered on single server (404). On a
    // non-coordinator the isCoordinator() check returns 501 before the auth check,
    // so only the base UseDatabase question fires there.
    testAgencyDump: function () {
      beginObserve();
      arango.GET_RAW(`${base}/agency-dump`);
      assertPermissions([useSystem, adminReadAgency], endObserve());
    },

    // GET /_api/cluster/cluster-info - canUseAdminAction(AdminClusterInfo)
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testClusterInfo: function () {
      beginObserve();
      arango.GET_RAW(`${base}/cluster-info`);
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // PUT /_api/cluster/cluster-info/flush - canUseAdminAction(AdminClusterInfo),
    // then (prod) isSuperuser() guard (no question)
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testClusterInfoFlush: function () {
      beginObserve();
      arango.PUT_RAW(`${base}/cluster-info/flush`, {});
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // GET /_api/cluster/cluster-info/get_collection_info/d/c
    // canUseAdminAction(AdminClusterInfo), then (prod) isSuperuser() guard.
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testGetCollectionInfo: function () {
      beginObserve();
      arango.GET_RAW(`${base}/cluster-info/get_collection_info/d/c`);
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // GET /_api/cluster/cluster-info/get_collection_info_current/d/c/s1
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testGetCollectionInfoCurrent: function () {
      beginObserve();
      arango.GET_RAW(`${base}/cluster-info/get_collection_info_current/d/c/s1`);
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // POST /_api/cluster/cluster-info/get_responsible_servers
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testGetResponsibleServers: function () {
      beginObserve();
      arango.POST_RAW(`${base}/cluster-info/get_responsible_servers`, []);
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // POST /_api/cluster/cluster-info/get_responsible_shard/d/c/true
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testGetResponsibleShard: function () {
      beginObserve();
      arango.POST_RAW(`${base}/cluster-info/get_responsible_shard/d/c/true`,
                      { _key: 'testkey' });
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // GET /_api/cluster/cluster-info/get_analyzers_revision/_system
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testGetAnalyzersRevision: function () {
      beginObserve();
      arango.GET_RAW(`${base}/cluster-info/get_analyzers_revision/_system`);
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // GET /_api/cluster/cluster-info/wait_for_plan_version/1
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testWaitForPlanVersion: function () {
      beginObserve();
      arango.GET_RAW(`${base}/cluster-info/wait_for_plan_version/1`);
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // GET /_api/cluster/cluster-info/get_max_number_of_shards
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testGetMaxNumberOfShards: function () {
      beginObserve();
      arango.GET_RAW(`${base}/cluster-info/get_max_number_of_shards`);
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // GET /_api/cluster/cluster-info/get_max_replication_factor
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testGetMaxReplicationFactor: function () {
      beginObserve();
      arango.GET_RAW(`${base}/cluster-info/get_max_replication_factor`);
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // GET /_api/cluster/cluster-info/get_min_replication_factor
    // AUDIT: cluster-only - route not registered on single server (404, no question).
    testGetMinReplicationFactor: function () {
      beginObserve();
      arango.GET_RAW(`${base}/cluster-info/get_min_replication_factor`);
      assertPermissions([useSystem, adminClusterInfo], endObserve());
    },

    // GET /_api/cluster/endpoints - allowed for any authenticated user; the
    // overridden checkUserCanAccess() returns OK without asking and the handler
    // needs no admin RBAC check, so NO authorization question is observed.
    // AUDIT: cluster-only - route not registered on single server (404).
    testEndpoints: function () {
      beginObserve();
      arango.GET_RAW(`${base}/endpoints`);
      assertPermissions([], endObserve());
    },
  };
}

jsunity.run(clusterApiAuthzSuite);
return jsunity.done();
