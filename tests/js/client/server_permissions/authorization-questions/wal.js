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

// Authorization questions asked by the /_admin/wal endpoint family.
//
// Observation-based counterpart of tests/api/apitests/wal.mjs.
//
// Handler: arangod/RocksDBEngine/RocksDBRestWalHandler.cpp (single-server/DBServer)
//          arangod/ClusterEngine/ClusterRestWalHandler.cpp   (coordinator)
//
// Every request first asks `UseDatabase name=_system level=read` in
// RestHandler::checkUserCanAccess() (the routes have no /_db/ prefix, so the
// database is the connected _system).
//
//   GET  /properties               no in-handler auth check (returns 501)
//   PUT  /properties               no in-handler auth check (returns 501)
//   GET  /transactions             no in-handler auth check (returns 501)
//   PUT  /flush                    no in-handler auth check
//   PUT  /wait_for_estimator_sync  production build: !isSuperuser() -> 403
//                                    (isSuperuser() is not an ExecContext::can()
//                                     question, so nothing extra is observed)
//                                  maintainer build: canUseAdminAction(AdminWalAccess)
//                                    -> `AdminWalAccess`
//
// So for properties/transactions/flush only the base UseDatabase question fires.

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

function walApiAuthzSuite () {
  const useSystem = `UseDatabase name=_system level=read`;

  return {
    tearDown: function () {
      disableObserve();
    },

    // GET /_admin/wal/properties - no in-handler auth check
    testGetProperties: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/wal/properties`);
      assertPermissions([useSystem], endObserve());
    },

    // PUT /_admin/wal/properties - no in-handler auth check
    testSetProperties: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/wal/properties`, {});
      assertPermissions([useSystem], endObserve());
    },

    // GET /_admin/wal/transactions - no in-handler auth check
    testGetTransactions: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/wal/transactions`);
      assertPermissions([useSystem], endObserve());
    },

    // PUT /_admin/wal/flush - no in-handler auth check
    testFlush: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/wal/flush`, {});
      assertPermissions([useSystem], endObserve());
    },

    // PUT /_admin/wal/wait_for_estimator_sync
    // AUDIT: build-dependent. In a production (non-maintainer) build the guard is
    // `!isSuperuser()` which is NOT an ExecContext::can() question, so only the
    // base `UseDatabase name=_system level=read` is observed (as asserted here).
    // In a maintainer build the guard is `canUseAdminAction(AdminWalAccess)`, which
    // additionally asks `AdminWalAccess`.
    testWaitForEstimatorSync: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_admin/wal/wait_for_estimator_sync`, {});
      assertPermissions([useSystem], endObserve());
    },
  };
}

jsunity.run(walApiAuthzSuite);
return jsunity.done();
