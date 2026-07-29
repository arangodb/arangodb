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

// Authorization questions asked by the /_admin/actions endpoint.
//
// Observation-based counterpart of tests/api/apitests/actions.mjs.
//
// Handler: arangod/Cluster/MaintenanceRestHandler.cpp
//
// MaintenanceRestHandler performs NO ExecContext checks of its own; the
// endpoint is AUTHEN in the permission table. The only authorization question
// is therefore the base `UseDatabase name=_system level=read` asked by
// RestHandler::checkUserCanAccess() (the path carries no /_db prefix, so the
// connected database _system applies; we spell it out explicitly). This holds
// for all four methods, including the PUT/DELETE cases that end in a 400 - the
// base check is still asked before the handler rejects the request body.

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

function actionApiAuthzSuite () {
  const useSystem = `UseDatabase name=_system level=read`;

  return {
    tearDown: function () {
      disableObserve();
      // make sure maintenance is running again after a pause test
      arango.POST_RAW(`/_db/_system/_admin/actions`, { execute: 'proceed' });
    },

    // GET /_admin/actions - read-only registry dump
    testListActions: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/actions`);
      assertPermissions([useSystem], endObserve());
    },

    // POST /_admin/actions - proceed (idempotent resume)
    testProceed: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/actions`, { execute: 'proceed' });
      assertPermissions([useSystem], endObserve());
    },

    // POST /_admin/actions - pause for 1s (resumed in tearDown)
    testPause: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/actions`,
                      { execute: 'pause', duration: 1 });
      assertPermissions([useSystem], endObserve());
      arango.POST_RAW(`/_db/_system/_admin/actions`, { execute: 'proceed' });
    },

    // PUT /_admin/actions - empty body -> 400, but the base check is asked
    testPutEmptyBody: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/actions`, {});
      assertPermissions([useSystem], endObserve());
    },

    // DELETE /_admin/actions/999999 - non-existent action -> 400, base check
    // is still asked
    testDeleteNonExistent: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_admin/actions/999999`);
      assertPermissions([useSystem], endObserve());
    },
  };
}

jsunity.run(actionApiAuthzSuite);
return jsunity.done();
