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

// Authorization questions asked by the /_open/auth endpoint family.
//
// Observation-based counterpart of tests/api/apitests/open.mjs.
//
// Handler: arangod/RestHandler/RestAuthHandler.cpp
//
// TODO fix
// These are OPEN (public) endpoints. RestAuthHandler overrides
// checkUserCanAccess() to unconditionally forceSuperuser() and return OK, so
// the universal `UseDatabase name=... level=read` base check is NOT asked.
// execute() only inspects the request body / JWT and generates a token; it
// never consults the ExecContext. Hence these endpoints ask NOTHING at all -
// the observed set is empty for every request, regardless of the credentials
// supplied.

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

function openApiAuthzSuite () {
  return {
    tearDown: function () {
      disableObserve();
    },

    // POST /_open/auth - obtain a JWT. OPEN: checkUserCanAccess forces
    // superuser, execute() only reads the body -> no authorization question.
    // AUDIT: the observed set is empty whether or not the credentials are
    // valid; there is no "AR" user in this fixture, so the login itself fails,
    // but that does not change which questions are asked (none).
    testObtainToken: function () {
      beginObserve();
      arango.POST_RAW(`/_open/auth`, { username: 'AR', password: 'AR' },
                      { 'Authorization': 'Bearer not-a-real-token' });
      assertPermissions([
      ], endObserve());
    },

    // POST /_open/auth/renew with a bearer token - still OPEN, asks nothing.
    // AUDIT: we send a dummy bearer header; a valid user JWT would behave
    // identically as far as authorization questions go (none are asked).
    testRenewWithToken: function () {
      beginObserve();
      arango.POST_RAW(`/_open/auth/renew`, {},
                      { 'Authorization': 'Bearer not-a-real-token' });
      assertPermissions([], endObserve());
    },

    // POST /_open/auth/renew without a proper token - OPEN, asks nothing.
    testRenewWithoutToken: function () {
      beginObserve();
      arango.POST_RAW(`/_open/auth/renew`, {},
                      { 'Authorization': 'Bearer not-a-real-token' });
      assertPermissions([
      ], endObserve());
    },
  };
}

jsunity.run(openApiAuthzSuite);
return jsunity.done();
