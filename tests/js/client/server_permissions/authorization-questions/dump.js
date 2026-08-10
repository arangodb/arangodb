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

// Authorization questions asked by the /_api/dump endpoint family.
//
// Observation-based counterpart of tests/api/apitests/dump.mjs.
//
// Handler: arangod/RestHandler/RestDumpHandler.cpp
//
// Every request first asks `UseApiVersion version=0` and then
// `UseDatabase name=d level=read` (the paths carry the /_db/d/ prefix).
//
//   POST /_api/dump/start   validateRequest() iterates the requested shards and
//                           asks canDumpCollection(db, coll) for each ->
//                           `DumpCollection db=d name=c`. handleCommandDumpStart()
//                           then always calls canUseAdminAction(AdminDump) to
//                           decide whether to elevate to superuser on a single
//                           server -> `AdminDump`.
//   POST /_api/dump/next    handleCommandDumpNext() performs a SAME-USER check
//                           inside RocksDBDumpManager::find() (throws on mismatch);
//                           this is NOT an ExecContext::can() question, so only the
//                           base UseDatabase question is observed.
//   DELETE /_api/dump/{id}  handleCommandDumpFinished() -> RocksDBDumpManager::
//                           remove() SAME-USER check; again no can() question.
//
// AUDIT: these assertions reflect single-server behaviour (shard name == collection
// name "c", no ?dbserver= query string). On a cluster the dump API runs on a
// DBServer: `DumpCollection` then names the shard, and the single-server-only
// AdminDump elevation (handleCommandDumpStart: canUseAdminAction(AdminDump) &&
// isSingleServer()) still evaluates canUseAdminAction(AdminDump) so the question
// is asked regardless, but the coordinator proxies the request.

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
const {
  setUpApiTestData,
  tearDownApiTestData,
  DB,
  DOC_COLLECTION,
  singleOnly,
  clusterOnly
} = require('@arangodb/testutils/apitest-fixtures');

function dumpApiAuthzSuite () {
  const c = DOC_COLLECTION;

  // start a dump session as root (single-server: shard name == collection name)
  function startDump () {
    const res = arango.POST_RAW(`/_db/${DB}/_api/dump/start`, { shards: [c] });
    return res.headers ? res.headers['x-arango-dump-id'] : undefined;
  }
  function abortDump (id) {
    if (id) {
      arango.DELETE_RAW(`/_db/${DB}/_api/dump/${id}`);
    }
  }
  // create a replication batch as root (required for dump/next); return its id
  function createBatch () {
    const res = arango.POST_RAW(`/_db/${DB}/_api/replication/batch`, { ttl: 600 });
    return (res.parsedBody && res.parsedBody.id) ? res.parsedBody.id : undefined;
  }
  function deleteBatch (id) {
    if (id) {
      arango.DELETE_RAW(`/_db/${DB}/_api/replication/batch/${id}`);
    }
  }

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
    },

    // POST /_api/dump/start - canDumpCollection() + canUseAdminAction(AdminDump)
    testDumpStart: function () {
      beginObserve();
      const res = arango.POST_RAW(`/_db/${DB}/_api/dump/start`, { shards: [c] });
      // AUDIT: on a coordinator the question carries an empty collection
      // name, and AdminDump is not asked at all
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        ...singleOnly([
          "DumpCollection db=d name=c",
          "UseCollection db=d name=c level=read",
          "AdminDump"
        ]),
        ...clusterOnly([
          "DumpCollection db=d name="
        ])
      ], endObserve());
      // abort the dump the observed request started
      if (res.headers && res.headers['x-arango-dump-id']) {
        abortDump(res.headers['x-arango-dump-id']);
      }
    },

    // POST /_api/dump/next/{id} - SAME-USER check (no can() question)
    // AUDIT: the SAME-USER restriction is enforced inside RocksDBDumpManager::find()
    // and does not go through ExecContext::can(), so only the base UseDatabase
    // question is observed.
    testDumpNext: function () {
      const dumpId = startDump();
      const batchId = createBatch();
      beginObserve();
      arango.POST_RAW(
        `/_db/${DB}/_api/dump/next/${dumpId}?batchId=${batchId}`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
      abortDump(dumpId);
      deleteBatch(batchId);
    },

    // DELETE /_api/dump/{id} - SAME-USER check (no can() question)
    // AUDIT: the SAME-USER restriction is enforced inside RocksDBDumpManager::
    // remove() and does not go through ExecContext::can().
    testDumpDelete: function () {
      const dumpId = startDump();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/dump/${dumpId}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
      abortDump(dumpId);
    },
  };
}

jsunity.run(dumpApiAuthzSuite);
return jsunity.done();
