////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// Tests that /_api/replication/* forwarding via the "DBserver" parameter is
// restricted to the commands that support it. A request forwarded to a
// client-supplied DBserver has its authorization header removed on the
// coordinator, so forwarding is only accepted for the commands that arangodump
// relies on ("dump"/"batch") and that validate the caller's permissions on the
// coordinator. For every other command the "DBserver" parameter is rejected.

const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse, assertNotUndefined} = jsunity.jsUnity.assertions;
const arango = require("@arangodb").arango;
const db = require("internal").db;
const users = require("@arangodb/users");
let IM = global.instanceManager;

// Regression test suite for COR-739
function ReplicationForwardingSuite() {
  'use strict';

  // user with read-only access on the database and no rights on the collection
  const roUser = 'roUser@arango.ai';
  // user with full rights on the database/collection
  const rwUser = 'rwUser@arango.ai';

  const cn = 'UnitTestsCollection';
  const dbName = 'UnitTestsDatabase';

  // find the shard of `cn` and the DBServer hosting it
  const locateShard = function () {
    const shards = db[cn].shards(true);
    const shard = Object.keys(shards)[0];
    const server = shards[shard][0];
    return {shard, server};
  };

  // Helper used by the "other forwarded commands are forbidden" tests below:
  // connects as the read-only user, issues the request built by `makeRequest`
  // (which receives the "?DBserver=...&collection=..." query string) and
  // asserts that it is rejected with a 403.
  const checkForwardIsForbidden = function (makeRequest) {
    const {shard, server} = locateShard();
    const q = `?DBserver=${encodeURIComponent(server)}&collection=${encodeURIComponent(shard)}`;

    arango.reconnect(IM.endpoint, dbName, roUser, 'foobar');
    const result = makeRequest(q);

    assertTrue(result.error, JSON.stringify(result));
    assertEqual(403, result.code, JSON.stringify(result));
  };

  return {
    setUpAll: function () {
      try { users.remove(roUser); } catch (err) {}
      try { users.remove(rwUser); } catch (err) {}
      try { db._dropDatabase(dbName); } catch (err) {}

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      db._create(cn);
      db._useDatabase('_system');

      users.save(roUser, "foobar");
      users.save(rwUser, "foobar");
      users.grantDatabase(rwUser, dbName);
      users.grantCollection(rwUser, dbName, "*", 'rw');
      users.grantDatabase(roUser, dbName, 'ro');
      users.grantCollection(roUser, dbName, cn, 'none');
      users.reload();
    },

    tearDownAll: function () {
      try { users.remove(roUser); } catch (err) {}
      try { users.remove(rwUser); } catch (err) {}
      try { db._dropDatabase(dbName); } catch (err) {}
    },

    // remember the (root) connection before each test and restore it
    // afterwards, so every test starts connected as root in `dbName`
    setUp: function () {
      IM.rememberConnection();
      db._useDatabase(dbName);
    },

    tearDown: function () {
      IM.reconnectMe();
    },

    // restore-data does not support the DBserver forward, so a user without
    // write access on the collection cannot use it to write to the shard.
    testRestoreDataForwardIsForbidden: function () {
      const {shard, server} = locateShard();

      arango.reconnect(IM.endpoint, dbName, roUser, 'foobar');
      const url = `/_api/replication/restore-data?DBserver=${encodeURIComponent(server)}&collection=${encodeURIComponent(shard)}`;
      const result = arango.PUT(url, [{_key: "someKey", _rev: "12312312312"}]);

      assertTrue(result.error, JSON.stringify(result));
      assertEqual(403, result.code, JSON.stringify(result));

      // and nothing was written
      arango.reconnect(IM.endpoint, dbName, "root", "");
      assertEqual(0, db[cn].count());
    },

    // The remaining replication commands do not support the DBserver forward
    // either, so they must reject the DBserver parameter.
    testRestoreIndexesForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.PUT(`/_api/replication/restore-indexes${q}`, {}));
    },

    testRestoreViewForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.PUT(`/_api/replication/restore-view${q}`, {}));
    },

    testSyncForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.PUT(`/_api/replication/sync${q}`, {endpoint: "tcp://127.0.0.1:1", database: dbName}));
    },

    testAddFollowerForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.PUT(`/_api/replication/addFollower${q}`, {}));
    },

    testRemoveFollowerForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.PUT(`/_api/replication/removeFollower${q}`, {}));
    },

    testSetTheLeaderForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.PUT(`/_api/replication/set-the-leader${q}`, {}));
    },

    testLoggerStateForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.GET(`/_api/replication/logger-state${q}`));
    },

    testLoggerTickRangesForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.GET(`/_api/replication/logger-tick-ranges${q}`));
    },

    testLoggerFirstTickForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.GET(`/_api/replication/logger-first-tick${q}`));
    },

    testLoggerFollowForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.GET(`/_api/replication/logger-follow${q}`));
    },

    testInventoryForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.GET(`/_api/replication/inventory${q}`));
    },

    testKeysForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.POST(`/_api/replication/keys${q}`, {}));
    },

    testRevisionsTreeForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.GET(`/_api/replication/revisions/tree${q}`));
    },

    testRevisionsTreePendingForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.GET(`/_api/replication/revisions/treepending${q}`));
    },

    testRevisionsRangesForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.PUT(`/_api/replication/revisions/ranges${q}`, {}));
    },

    testRevisionsDocumentsForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.PUT(`/_api/replication/revisions/documents${q}`, {}));
    },

    testServerIdForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.GET(`/_api/replication/server-id${q}`));
    },

    testClusterInventoryForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.GET(`/_api/replication/clusterInventory${q}`));
    },

    testHoldReadLockCollectionForwardIsForbidden: function () {
      checkForwardIsForbidden((q) => arango.POST(`/_api/replication/holdReadLockCollection${q}`, {}));
    },

    // The following is explicitly not here for the following reason: For the `restore-collection`
    // case we already fail at the `testPermissions` stage, so we do not fail with FORBIDDEN,
    // but with another message. The check that we only forward a few select routes
    // is done later, so we cannot test this here:
    //testRestoreCollectionForwardIsForbidden: function () {
    //  checkForwardIsForbidden((q) => arango.PUT(`/_api/replication/restore-collection${q}`, {}));
    //},

    // batch supports the DBserver forward: an authorized user can create a
    // replication batch on the hosting DBServer via ?DBserver= (this is what
    // arangodump uses).
    testBatchForward: function () {
      const {server} = locateShard();

      arango.reconnect(IM.endpoint, dbName, rwUser, 'foobar');
      const server_q = encodeURIComponent(server);

      let result = arango.POST(`/_api/replication/batch?DBserver=${server_q}`, {});
      assertFalse(result.error, JSON.stringify(result));
      assertNotUndefined(result.id, JSON.stringify(result));

      // clean up the batch we just created on the DBServer
      const del = arango.DELETE(`/_api/replication/batch/${encodeURIComponent(result.id)}?DBserver=${server_q}`);
      assertFalse(del.error, JSON.stringify(del));
    },

    // dump supports the DBserver forward: an authorized user can dump a shard
    // from the hosting DBServer via ?DBserver= (this is what arangodump uses).
    testDumpForward: function () {
      const {shard, server} = locateShard();

      arango.reconnect(IM.endpoint, dbName, rwUser, 'foobar');
      const server_q = encodeURIComponent(server);

      // a dump needs a batch context on the same DBServer
      const batch = arango.POST(`/_api/replication/batch?DBserver=${server_q}`, {});
      assertFalse(batch.error, JSON.stringify(batch));
      assertNotUndefined(batch.id, JSON.stringify(batch));

      const url = `/_api/replication/dump?collection=${encodeURIComponent(shard)}` +
                  `&batchId=${encodeURIComponent(batch.id)}&DBserver=${server_q}`;
      const result = arango.GET_RAW(url);
      // the collection is empty, so a successful dump replies 204 No Content
      assertEqual(204, result.code, JSON.stringify(result));

      const del = arango.DELETE(`/_api/replication/batch/${encodeURIComponent(batch.id)}?DBserver=${server_q}`);
      assertFalse(del.error, JSON.stringify(del));
    },
  };
}


jsunity.run(ReplicationForwardingSuite);

return jsunity.done();
