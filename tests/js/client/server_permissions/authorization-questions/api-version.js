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

// The api-version question itself, which has no endpoint of its own: every
// request of an authenticated identity passes through
// RestHandler::checkApiVersionAccess(), which asks
// ExecContext::canUseApiVersion() for the version the request addresses. That
// is why `UseApiVersion version=<n>` heads nearly every observation in this
// directory - including the default version, which is not exempt anywhere
// except in RestVersionHandler (see version.js).
//
// /_api/engine serves as the sample endpoint here: RestEngineHandler asks only
// canUseHardenedAction(MonitoringInternal), which is silent without
// --server.harden, so nothing but the two base questions is observed.
//
// API version 1 is not in ApiVersion.h's supportedApiVersions yet, so it is
// normally rejected before any handler runs. The
// 'ApiVersion::treatVersion1AsSupported' failure point makes the version
// checks treat it as supported; it must be armed before the server registers
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

function apiVersionAuthzSuite () {
  return {
    tearDown: function () {
      // in case a test failed in the middle of an observation
      disableObserve();
    },

    // the default API version is asked about like any other
    testDefaultApiVersion: function () {
      beginObserve();
      arango.GET_RAW(`/_api/engine`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // a versioned request asks about the version it addresses
    testNonDefaultApiVersion: function () {
      beginObserve();
      arango.GET_RAW(`/_arango/v1/_api/engine`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },
  };
}

jsunity.run(apiVersionAuthzSuite);
return jsunity.done();
