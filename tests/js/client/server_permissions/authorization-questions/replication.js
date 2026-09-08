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
// Every request first asks `UseApiVersion version=0` and then
// `UseDatabase name=_system level=read` (the paths carry the /_db/_system/ prefix).
//
//   batch (POST/PUT/DELETE)  Neither RestReplicationHandler::executeAsync()
//                            nor RocksDBRestReplicationHandler::
//                            handleCommandBatch() performs an ExecContext::can()
//                            check - the batch lifecycle is intentionally
//                            unrestricted (see checkDBserverForwardingAllowed()). So
//                            only the base UseDatabase question is observed.
//   restore-collection (PUT) handleCommandRestoreCollection() asks
//                            canRestoreCollection(overwrite), where `overwrite`
//                            is the parsed "overwrite" URL parameter OR'ed with
//                            "the collection does not exist yet".
//   clusterInventory (GET)   handleCommandClusterInventory() asks, per collection,
//                            canUseAdminAction(AdminDump) (and only if that
//                            fails, canUseCollection(read)). As root this asks
//                            `AdminDump`. But the command is coordinator-only:
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
  clusterOnly,
  singleOnly
} = require('@arangodb/testutils/apitest-fixtures');
const { db } = require('@arangodb');

function replicationApiAuthzSuite () {

  const cn = 'UnitTestsReplicationAuthz';

  function restoreCollection (queryString = '') {
    return arango.PUT_RAW(
      `/_db/_system/_api/replication/restore-collection${queryString}`,
      { parameters: { name: cn, type: 2 }, indexes: [] });
  }

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
    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      disableObserve();
      db._drop(cn);
    },

    // POST /_api/replication/batch - batch lifecycle is unrestricted (no can())
    testCreateBatch: function () {
      beginObserve();
      const res = arango.POST_RAW(
        `/_db/_system/_api/replication/batch`, { ttl: 30 });
      assertPermissions([
        "UseApiVersion version=0",
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
        "UseApiVersion version=0",
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
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_api/replication/clusterInventory
    // AUDIT: cluster-only - on a single server execute() returns
    // CLUSTER_ONLY_ON_COORDINATOR before handleCommandClusterInventory(), so only
    // the base UseDatabase question fires. On a coordinator each collection asks
    // `AdminDump` (and, only if that fails, `UseCollection ... level=read`).
    testClusterInventory: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/replication/clusterInventory`);
      // a single server rejects the request before asking
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        ...clusterOnly([
          "AdminDump"
        ])
      ], endObserve());
    },

    // PUT /_api/replication/restore-collection?overwrite=<true>
    // "overwrite" is parsed as a boolean (StringUtils::boolean(), which is
    // case-insensitive and also accepts "yes"/"on"/"y"/"1").
    // Only a single server distinguishes the spellings here: a coordinator
    // asks with overwrite=true whatever the parameter says, because it cannot
    // see the existing collection.
    testRestoreCollectionOverwrite: function () {
      db._create(cn);
      for (const overwrite of ['true', 'yes', 'on', '1', 'TRUE']) {
        beginObserve();
        restoreCollection(`?overwrite=${overwrite}`);
        assertPermissions([
          "UseApiVersion version=0",
          "UseDatabase name=_system level=read",
          `RestoreCollection db=_system name=${cn} overwrite=true`,
          "IsReadOnly",
          "AdminRestore",
          `UseCollection db=_system name=${cn} level=read`,
          `DropCollection db=_system name=${cn}`,
          `CreateCollection db=_system name=${cn}`
        ], endObserve());
      }
    },

    // PUT /_api/replication/restore-collection into an existing collection
    // Without overwrite the restore conflicts with the existing collection, so
    // nothing is dropped or created.
    testRestoreCollectionWithoutOverwrite: function () {
      db._create(cn);
      beginObserve();
      restoreCollection();
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        // a coordinator cannot see the existing collection, see above
        ...singleOnly([`RestoreCollection db=_system name=${cn} overwrite=false`]),
        ...clusterOnly([`RestoreCollection db=_system name=${cn} overwrite=true`]),
        "IsReadOnly",
        "AdminRestore",
        `UseCollection db=_system name=${cn} level=read`
      ], endObserve());
    },

    // PUT /_api/replication/restore-collection for a collection that does not
    // exist yet - asked with overwrite=true although the parameter is unset,
    // because a new collection has to be created either way.
    testRestoreCollectionNew: function () {
      beginObserve();
      restoreCollection();
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        `RestoreCollection db=_system name=${cn} overwrite=true`,
        "IsReadOnly",
        "AdminRestore",
        `CreateCollection db=_system name=${cn}`
      ], endObserve());
    },
  };
}

jsunity.run(replicationApiAuthzSuite);
return jsunity.done();
