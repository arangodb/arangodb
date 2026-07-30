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

// Authorization questions asked by the /_api/replication endpoint family.
//
// Observation-based counterpart of tests/api/apitests/replication.mjs.
//
// Handler: arangod/RestHandler/RestReplicationHandler.cpp
//          arangod/RocksDBEngine/RocksDBRestReplicationHandler.cpp (batch)
//
// Every request first asks `UseDatabase name=_system level=read` in
// RestHandler::checkUserCanAccess() (the paths carry the /_db/_system/ prefix).
//
//   batch (POST/PUT/DELETE)  RestReplicationHandler::testPermissions() does not
//                            check the batch command (only Dump GET and
//                            RestoreCollection PUT), and RocksDBRestReplicationHandler::
//                            handleCommandBatch() performs no ExecContext::can()
//                            check - the batch lifecycle is intentionally
//                            unrestricted (see isDBserverForwardingAllowed()). So
//                            only the base UseDatabase question is observed.
//   clusterInventory (GET)   handleCommandClusterInventory() asks, per collection,
//                            canUseAdminAction(AdminClusterInfo) (and only if that
//                            fails, canUseCollection(read)). As root this asks
//                            `AdminClusterInfo`. But the command is coordinator-only:
//                            execute() rejects with CLUSTER_ONLY_ON_COORDINATOR on a
//                            single server BEFORE handleCommandClusterInventory runs.
//
// AUDIT: the apitest sends ?DBserver=DBServer0001 on the batch calls for the
// coordinator trampoline; it is omitted here so the single-server observation is
// not forwarded away. The base UseDatabase question is unaffected either way.

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
const { clusterOnly } = require('@arangodb/testutils/apitest-fixtures');

function replicationApiAuthzSuite () {

  function createBatch () {
    const res = arango.POST_RAW(
      `/_db/_system/_api/replication/batch`, { ttl: 60 });
    return (res.parsedBody && res.parsedBody.id) ? res.parsedBody.id : undefined;
  }
  function deleteBatch (id) {
    if (id) {
      arango.DELETE_RAW(`/_db/_system/_api/replication/batch/${id}`);
    }
  }

  return {
    tearDown: function () {
      disableObserve();
    },

    // POST /_api/replication/batch - batch lifecycle is unrestricted (no can())
    testCreateBatch: function () {
      beginObserve();
      const res = arango.POST_RAW(
        `/_db/_system/_api/replication/batch`, { ttl: 30 });
      assertPermissions([
        "UseDatabase name=_system level=read"
      ], endObserve());
      if (res.parsedBody && res.parsedBody.id) {
        deleteBatch(res.parsedBody.id);
      }
    },

    // PUT /_api/replication/batch/{id} - extend batch TTL (no can())
    testExtendBatch: function () {
      const id = createBatch();
      beginObserve();
      arango.PUT_RAW(
        `/_db/_system/_api/replication/batch/${id}`, { ttl: 30 });
      assertPermissions([
        "UseDatabase name=_system level=read"
      ], endObserve());
      deleteBatch(id);
    },

    // DELETE /_api/replication/batch/{id} - discard batch (no can())
    testDeleteBatch: function () {
      const id = createBatch();
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/replication/batch/${id}`);
      assertPermissions([
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_api/replication/clusterInventory
    // AUDIT: cluster-only - on a single server execute() returns
    // CLUSTER_ONLY_ON_COORDINATOR before handleCommandClusterInventory(), so only
    // the base UseDatabase question fires. On a coordinator each collection asks
    // `AdminClusterInfo` (and, only if that fails, `UseCollection ... level=read`).
    testClusterInventory: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/replication/clusterInventory`);
      // a single server rejects the request before asking
      assertPermissions([
        "UseDatabase name=_system level=read",
        ...clusterOnly([
          "AdminClusterInfo"
        ])
      ], endObserve());
    },
  };
}

jsunity.run(replicationApiAuthzSuite);
return jsunity.done();
