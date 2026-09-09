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

// Authorization questions asked by the /_api/aql-builtin endpoint.
//
// Observation-based counterpart of tests/api/apitests/aql.mjs.
//
// Handler: arangod/RestHandler/RestAqlFunctionsHandler.cpp
//
// RestAqlFunctionsHandler just serialises the list of built-in AQL functions
// and returns it; it performs no in-handler authorization check. The only
// questions are the universal base checks: `UseApiVersion version=0` and
// `UseDatabase name=<db> level=read` for the database in the request path.
// The endpoint has no /_db/ prefix in the apitest and runs in the connected
// database context; here we address it explicitly in _system.

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
  tearDownApiTestData
} = require('@arangodb/testutils/apitest-fixtures');

function aqlApiAuthzSuite () {

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
    },

    // GET /_api/aql-builtin - no in-handler permission check, only the base
    // UseDatabase(read) question for _system
    testListBuiltinFunctions: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/aql-builtin`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },
  };
}

jsunity.run(aqlApiAuthzSuite);
return jsunity.done();
