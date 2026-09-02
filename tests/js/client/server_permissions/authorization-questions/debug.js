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

// Authorization questions asked by the /_admin/debug endpoint family.
//
// Observation-based counterpart of tests/api/apitests/debug.mjs.
//
// Handler: arangod/RestHandler/RestDebugHandler.cpp
//
// RestDebugHandler contains NO ExecContext permission check at all (auth level
// is AUTHEN: any authenticated user), so the only observed questions are
// the base `UseApiVersion version=1` and `UseDatabase name=_system level=read`.
//
// AUDIT: RestDebugHandler is only compiled in when ARANGODB_ENABLE_FAILURE_TESTS
// is defined at build time. On a release build every /_admin/debug route is
// unregistered and returns 404; in that case the base UseDatabase question may
// or may not be asked depending on how the 404 is produced. These tests assume
// a build with failure tests enabled.
//
// NOTE: PUT /_admin/debug/crash is intentionally omitted - issuing it as root
// would terminate the arangod process. Its auth semantics are AUTHEN, identical
// to the routes below.

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

function debugApiAuthzSuite () {
  const failPoint = 'apitest-dummy';

  return {
    tearDown: function () {
      disableObserve();
      // remove any failure point a test may have left behind
      arango.DELETE_RAW(`/_db/_system/_admin/debug/failat/${failPoint}`);
    },

    // GET /_admin/debug/failat - failure-points availability (AUTHEN)
    testFailatAvailability: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/debug/failat`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_admin/debug/failat/all - list active failure points (AUTHEN)
    testFailatAll: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/debug/failat/all`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // PUT /_admin/debug/failat/{name} - activate a named failure point (AUTHEN)
    testActivateFailat: function () {
      // ensure no stale failure point from a previous interrupted run
      arango.DELETE_RAW(`/_db/_system/_admin/debug/failat/${failPoint}`);
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/debug/failat/${failPoint}`, {});
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
      arango.DELETE_RAW(`/_db/_system/_admin/debug/failat/${failPoint}`);
    },

    // DELETE /_admin/debug/failat/{name} - remove a single failure point (AUTHEN)
    testRemoveFailat: function () {
      // add the failure point first so the DELETE has something to remove
      arango.PUT_RAW(`/_db/_system/_admin/debug/failat/${failPoint}`, {});
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_admin/debug/failat/${failPoint}`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // DELETE /_admin/debug/failat - clear ALL failure points (AUTHEN)
    testClearFailat: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_admin/debug/failat`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // DELETE /_admin/debug/raceControl - reset the race controller (AUTHEN)
    testResetRaceControl: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_admin/debug/raceControl`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },
  };
}

jsunity.run(debugApiAuthzSuite);
return jsunity.done();
