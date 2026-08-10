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

// Authorization questions asked by a collection of /_admin/* server endpoints
// that do not have their own dedicated test file.
//
// Observation-based counterpart of tests/api/apitests/server.mjs.
//
// Handlers: RestAdminDeploymentHandler, RestAdminExecuteHandler,
// RestLicenseHandler, RestMetricsHandler, RestOptionsHandler,
// RestOptionsDescriptionHandler, RestPublicOptionsHandler,
// RestAdminRoutingHandler, RestAdminServerHandler.
//
// Every request first asks `UseApiVersion version=0` and then
// `UseDatabase name=_system level=read`. Beyond that:
//   - Hardened actions (license, metrics) ask nothing without --server.harden.
//   - isSuperuser / isSuperuserOrDisabled checks do not call can().
//   - The real extra questions come from RestAdminServerHandler:
//     PUT mode -> AdminMaintenance, api-calls -> AdminApiCalls,
//     aql-queries -> AdminAqlQueries.

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

function serverApiAuthzSuite () {

  return {
    tearDown: function () {
      disableObserve();
    },

    // GET /_admin/deployment/id - RestAdminDeploymentHandler, no check (AUTHEN)
    testDeploymentId: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/deployment/id`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // POST /_admin/execute - RestAdminExecuteHandler, no check (AUTHEN)
    // AUDIT: route only registered when V8 is compiled in AND
    // --javascript.allow-admin-execute=true (non-default); otherwise 404. When
    // registered it executes the posted JS ("1" here - a harmless no-op).
    testExecute: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/execute`, "1");
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/license - canUseHardenedAction(AdminLicense)
    // hardened action -> no question without --server.harden
    testGetLicense: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/license`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // PUT /_admin/license - canUseHardenedAction(AdminLicense)
    // hardened action -> no question without --server.harden
    testSetLicense: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/license`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/metrics - canUseHardenedAction(AdminMonitoring)
    // hardened action -> no question without --server.harden
    testMetrics: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/metrics`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/options - RestOptionsBaseHandler::checkAuthentication().
    // Default --server.options-api=jwt: isSuperuserOrDisabled() gate rejects a
    // basic-auth root before the AdminOptions check is reached.
    // AUDIT: in "admin" policy it asks canUseAdminAction(AdminOptions); the
    // base-only result assumes the default "jwt" policy.
    testGetOptions: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/options`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/options-description - same checkAuthentication() guard.
    // AUDIT: see testGetOptions.
    testGetOptionsDescription: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/options-description`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/options-public - RestPublicOptionsHandler, no check (AUTHEN)
    testGetOptionsPublic: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/options-public`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // POST /_admin/routing/reload - RestAdminRoutingHandler, no check (AUTHEN)
    testRoutingReload: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/routing/reload`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/server/api-calls - RestAdminServerHandler::handleApiCalls()
    // asks canUseAdminAction(AdminApiCalls) (line 331).
    // AUDIT: assumes the default admin recording mode (API enabled,
    // onlySuperUser=false); in jwt mode it uses isSuperuser (no question).
    testApiCalls: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/server/api-calls`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        "AdminApiCalls"
      ], endObserve());
    },

    // GET /_admin/server/aql-queries - RestAdminServerHandler::handleAqlQueries()
    // asks canUseAdminAction(AdminAqlQueries) (line 385).
    // AUDIT: assumes default admin recording mode (see testApiCalls).
    testAqlQueries: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/server/aql-queries`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        "AdminAqlQueries"
      ], endObserve());
    },

    // GET /_admin/server/availability - OPEN endpoint: the handler forces
    // superuser and returns before the base implementation runs, so not even
    // an authenticated request asks anything.
    testAvailability: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/server/availability`);
      // this endpoint bypasses RestHandler::checkUserCanAccess()
      assertPermissions([
        "UseApiVersion version=0",
      ], endObserve());
    },

    // GET /_admin/server/databaseDefaults - no check (AUTHEN)
    testDatabaseDefaults: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/server/databaseDefaults`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/server/id - no check (AUTHEN; 500 on single server)
    testServerId: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/server/id`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/server/mode - no check (AUTHEN)
    testGetMode: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/server/mode`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // PUT /_admin/server/mode - RestAdminServerHandler::handleMode() asks
    // canUseAdminAction(AdminMaintenance) (line 201). Body {mode:"default"} is
    // a no-op when the server is already in default mode.
    testSetMode: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/server/mode`, { mode: "default" });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        "AdminMaintenance"
      ], endObserve());
    },

    // GET /_admin/server/role - no check (AUTHEN)
    testServerRole: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/server/role`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/server/tls - no check (AUTHEN)
    testGetTls: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/server/tls`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // POST /_admin/server/tls - isSuperuserOrDisabled() gate (no question)
    testReloadTls: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/server/tls`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/server/jwt - no check (AUTHEN)
    // AUDIT: Enterprise Edition only; Community Edition returns 404.
    testGetJwt: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/server/jwt`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // POST /_admin/server/jwt - isSuperuserOrDisabled() gate (no question)
    // AUDIT: Enterprise Edition only; Community Edition returns 404.
    testReloadJwt: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/server/jwt`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/server/encryption - no check (AUTHEN)
    // AUDIT: Enterprise Edition only; Community Edition returns 404.
    testGetEncryption: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/server/encryption`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // POST /_admin/server/encryption - isSuperuserOrDisabled() gate (no question)
    // AUDIT: Enterprise Edition only; Community Edition returns 404.
    testRotateEncryption: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/server/encryption`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },
  };
}

jsunity.run(serverApiAuthzSuite);
return jsunity.done();
