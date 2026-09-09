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

// Authorization questions asked by the /_admin/log endpoint family.
//
// Observation-based counterpart of tests/api/apitests/log.mjs.
//
// Handler: arangod/RestHandler/RestAdminLogHandler.cpp
//
// Every request first asks `UseApiVersion version=1` and then
// `UseDatabase name=_system level=read`. RestAdminLogHandler::verifyPermitted()
// then, in the default configuration (--log.api-enabled=true,
// --log.api-jwt-policy=true i.e. admin mode), asks:
//   GET requests  -> canUseAdminAction(AdminReadLogs)
//   non-GET       -> canUseAdminAction(AdminSetLogLevel)
// (see RestAdminLogHandler.cpp:70-90). The isAPIEnabled()/onlySuperUser()
// gates do not call can(), so they add no observed question.

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

function logApiAuthzSuite () {

  return {
    tearDown: function () {
      disableObserve();
    },

    // GET /_admin/log - buffered log messages, legacy format (AdminReadLogs)
    testGetLog: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/log`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminReadLogs"
      ], endObserve());
    },

    // GET /_admin/log/entries - new object-per-entry format (AdminReadLogs)
    testGetLogEntries: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/log/entries`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminReadLogs"
      ], endObserve());
    },

    // GET /_admin/log/level - current per-topic log levels (AdminReadLogs)
    testGetLogLevel: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/log/level`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminReadLogs"
      ], endObserve());
    },

    // GET /_admin/log/structured - structured-logging parameters (AdminReadLogs)
    testGetLogStructured: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/log/structured`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminReadLogs"
      ], endObserve());
    },

    // PUT /_admin/log/level - no-op empty body (AdminSetLogLevel)
    testSetLogLevel: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/log/level`, {});
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminSetLogLevel"
      ], endObserve());
    },

    // PUT /_admin/log/structured - no-op body (AdminSetLogLevel)
    testSetLogStructured: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/log/structured`, {});
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminSetLogLevel"
      ], endObserve());
    },

    // DELETE /_admin/log - clear the in-memory log buffer (AdminSetLogLevel)
    testClearLog: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_admin/log`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminSetLogLevel"
      ], endObserve());
    },

    // DELETE /_admin/log/entries - alias for clearing the buffer (AdminSetLogLevel)
    testClearLogEntries: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_admin/log/entries`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminSetLogLevel"
      ], endObserve());
    },

    // DELETE /_admin/log/level - reset per-topic levels to defaults (AdminSetLogLevel)
    testResetLogLevel: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_admin/log/level`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=_system level=read",
        "AdminSetLogLevel"
      ], endObserve());
    },
  };
}

jsunity.run(logApiAuthzSuite);
return jsunity.done();
