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

// Authorization questions asked by the /_admin monitoring and information
// endpoints.
//
// Observation-based counterpart of tests/api/apitests/monitoring.mjs.
//
// Handlers: RestAdminStatisticsHandler, RestAdminStatusHandler,
// RestSupervisionStateHandler, RestSupportInfoHandler, RestSystemReportHandler,
// RestTimeHandler, RestUsageMetricsHandler, RestVersionHandler.
//
// Every request first asks `UseDatabase name=_system level=read` in
// RestHandler::checkUserCanAccess(). Beyond that:
//   - Hardened actions (canUseHardenedAction) ask nothing without
//     --server.harden=true (our suites do not set it), so only the base
//     question remains.
//   - isSuperuser / isSuperuserOrDisabled checks do not call can(), so they
//     add no observed question.
//   - canUseAdminAction(AdminSupervisionState) is the only real extra question.

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false'
  };
}

const jsunity = require('jsunity');
const {
  beginObserve,
  endObserve,
  disableObserve,
  assertPermissions
} = require('@arangodb/testutils/permissions-observer');

function monitoringApiAuthzSuite () {
  const useSystem = 'UseDatabase name=_system level=read';

  return {
    tearDown: function () {
      disableObserve();
    },

    // GET /_admin/statistics - canUseHardenedAction(AdminMonitoring)
    // hardened action -> no question without --server.harden
    testStatistics: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/statistics`);
      assertPermissions([useSystem], endObserve());
    },

    // GET /_admin/statistics-description - canUseHardenedAction(AdminMonitoring)
    // hardened action -> no question without --server.harden
    testStatisticsDescription: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/statistics-description`);
      assertPermissions([useSystem], endObserve());
    },

    // GET /_admin/status - canUseHardenedAction(AdminMonitoring)
    // hardened action -> no question without --server.harden
    testStatus: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/status`);
      assertPermissions([useSystem], endObserve());
    },

    // GET /_admin/supervisionState - RestSupervisionStateHandler asks
    // canUseAdminAction(AdminSupervisionState) first (executeAsync:44), before
    // the single-server "only on coordinator" rejection.
    testSupervisionState: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/supervisionState`);
      assertPermissions([useSystem, `AdminSupervisionState`], endObserve());
    },

    // GET /_admin/support-info - default --server.support-info-api=jwt policy;
    // RestSupportInfoHandler asks isSuperuserOrDisabled() first (no observed
    // question) and rejects a basic-auth root before reaching the admin check.
    // AUDIT: in "admin" policy it would instead ask canUseAdminAction(AdminMonitoring);
    // the base-only result assumes the default "jwt" policy.
    testSupportInfo: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/support-info`);
      assertPermissions([useSystem, 'AdminMonitoring'], endObserve());
    },

    // GET /_admin/system-report - canUseHardenedAction(AdminMonitoringInternal)
    // hardened action -> no question without --server.harden
    // AUDIT: with harden=false this runs OS commands (date/dmesg/df/uptime/top)
    // as root and may take up to ~60s to respond.
    testSystemReport: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/system-report`);
      assertPermissions([useSystem], endObserve());
    },

    // GET /_admin/telemetrics - default --server.support-info-api=jwt policy;
    // isSuperuser gate (no observed question) rejects basic-auth root.
    // AUDIT: RestTelemetricsHandler source was not found in the current tree
    // (may have been removed/renamed on this branch); the endpoint may return
    // 404. In "admin" policy it would ask canUseAdminAction(AdminMonitoringInternal).
    testGetTelemetrics: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/telemetrics`);
      assertPermissions([useSystem], endObserve());
    },

    // DELETE /_admin/telemetrics - same auth guard as GET.
    // AUDIT: see testGetTelemetrics.
    testDeleteTelemetrics: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_admin/telemetrics`);
      assertPermissions([useSystem], endObserve());
    },

    // GET /_admin/time - RestTimeHandler has no permission check (AUTHEN)
    testTime: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/time`);
      assertPermissions([useSystem], endObserve());
    },

    // GET /_admin/usage-metrics - canUseHardenedAction(AdminMonitoringInternal)
    // hardened action -> no question without --server.harden
    testUsageMetrics: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/usage-metrics`);
      assertPermissions([useSystem], endObserve());
    },

    // GET /_admin/version - RestVersionHandler always returns 200 and only uses
    // canUseHardenedAction(AdminMonitoringInternal) internally to decide whether
    // to include the version field.
    // hardened action -> no question without --server.harden
    testVersion: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/version`);
      assertPermissions([useSystem], endObserve());
    },
  };
}

jsunity.run(monitoringApiAuthzSuite);
return jsunity.done();
