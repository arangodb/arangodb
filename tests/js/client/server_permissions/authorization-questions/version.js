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

// Authorization questions asked by the version endpoints /_api/version and
// /_admin/version.
//
// Handler: arangod/RestHandler/RestVersionHandler.cpp
//
// Unlike every other handler, RestVersionHandler overrides
// checkApiVersionAccess() and lets the *default* API version pass without
// asking ExecContext::canUseApiVersion(). An identity that is not allowed to
// use that version can therefore still ask the server for its version. Only
// the database question of the base implementation remains, plus
// canUseHardenedAction(AdminMonitoringInternal) internally to decide whether
// the detailed version information is included - that one is silent without
// --server.harden.
//
// Any other API version is gated as everywhere else, on this route too. API
// version 1 is not in ApiVersion.h's supportedApiVersions yet, so it is
// normally rejected before any handler runs; the
// 'ApiVersion::treatVersion1AsSupported' failure point makes the version
// checks treat it as supported. It must be armed before the server registers
// its routes at startup, hence the startup option (see also
// failing-user-access-does-not-reveal-information.js).
if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.failure-point': 'ApiVersion::treatVersion1AsSupported',
    // the observer reads the log file right after the request, so the log must
    // not be written by the logging thread
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
  DB
} = require('@arangodb/testutils/apitest-fixtures');

function versionApiAuthzSuite () {
  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      // in case a test failed in the middle of an observation
      disableObserve();
    },

    // GET /_api/version - the api-version question is not asked at all, so
    // whichever versions the identity may use, it gets the version.
    testVersion: function () {
      beginObserve();
      const res = arango.GET_RAW(`/_api/version`);
      assertEqual(200, res.code);
      assertPermissions([
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // the database question follows the /_db/ prefix, the exemption does not
    // depend on it
    testVersionInOtherDatabase: function () {
      beginObserve();
      const res = arango.GET_RAW(`/_db/${DB}/_api/version`);
      assertEqual(200, res.code);
      assertPermissions([
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // GET /_arango/v1/_api/version - the exemption covers the default version
    // only, so a versioned request to the very same handler is gated.
    testVersionWithNonDefaultApiVersion: function () {
      beginObserve();
      const res = arango.GET_RAW(`/_arango/v1/_api/version`);
      assertEqual(200, res.code);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/version - the handler's second route, same exemption. It
    // always returns 200 and only uses
    // canUseHardenedAction(AdminMonitoringInternal) internally to decide
    // whether to include the version field - a hardened action, hence no
    // question without --server.harden.
    testAdminVersion: function () {
      beginObserve();
      const res = arango.GET_RAW(`/_db/_system/_admin/version`);
      assertEqual(200, res.code);
      assertPermissions([
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_arango/v1/_admin/version does not exist
    testAdminVersioV1DoesNotExist: function () {
      beginObserve();
      const res = arango.GET_RAW(`/_arango/v1/_admin/version`);
      assertEqual(404, res.code);
    },
  };
}

jsunity.run(versionApiAuthzSuite);
return jsunity.done();
