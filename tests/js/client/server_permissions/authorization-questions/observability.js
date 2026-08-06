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

// Authorization questions asked by the observability endpoints
// /_admin/activities and /_admin/async-registry.
//
// Observation-based counterpart of tests/api/apitests/observability.mjs.
//
// Handlers:
//   arangod/SystemMonitor/Activities/RestHandler.cpp
//   arangod/SystemMonitor/AsyncRegistry/RestHandler.cpp
//
// Every request first asks `UseApiVersion version=0` and then
// `UseDatabase name=_system level=read` in
// RestHandler::checkUserCanAccess() (connected database is _system). Both
// handlers then ask canUseAdminAction(AdminMonitoringInternal), so the
// observed question is AdminMonitoringInternal.

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

function observabilityApiAuthzSuite () {

  return {
    tearDown: function () {
      disableObserve();
    },

    // GET /_arango/experimental/_admin/activities - Activities/RestHandler asks
    // canUseAdminAction(AdminMonitoringInternal) (executeAsync:126). The auth
    // check runs before the api-version check, so the question fires even
    // though the experimental prefix is required for a 200.
    // AUDIT: assumes --activities.only-superuser=false (default); when enabled
    // the handler uses isSuperuserOrDisabled() instead (no observed question).
    // Connected database is _system (no /_db prefix combines with the
    // /_arango/experimental api-version prefix).
    testListActivities: function () {
      beginObserve();
      arango.GET_RAW(`/_arango/experimental/_admin/activities`);
      assertPermissions([
        // the experimental prefix addresses api version 2
        "UseApiVersion version=2",
        "UseDatabase name=_system level=read",
        "AdminMonitoringInternal"
      ], endObserve());
    },

    // GET /_admin/async-registry - AsyncRegistry/RestHandler asks
    // canUseAdminAction(AdminMonitoringInternal) (executeAsync:37).
    testListAsyncRegistry: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/async-registry`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        "AdminMonitoringInternal"
      ], endObserve());
    },
  };
}

jsunity.run(observabilityApiAuthzSuite);
return jsunity.done();
