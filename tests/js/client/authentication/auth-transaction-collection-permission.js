/*jshint globalstrict:false, strict:false */
/*global assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
// /
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;
const db = require("internal").db;
const users = require("@arangodb/users");
const ERRORS = require("internal").errors;
const IM = require("@arangodb/test-helper").getInstanceInfo();

// A document operation on the coordinator must perform a collection-level
// permission check even when it runs within an already existing transaction.
// The collection-level permission check normally happens when a collection is
// added to the transaction (on the coordinator, against the real user). When a
// single document operation runs inside an existing transaction and targets a
// collection that was not declared, that check used to be skipped entirely: on
// the coordinator the collection is not added at runtime, and on the DB server
// it is only added lazily in superuser context, where the check is a no-op.
// This suite pins down that such a read is denied for a user that lacks access.
function authTransactionCollectionPermissionSuite() {
  const dbName = "UnitTestsTrxPerm";
  const secret = "secretCollection";   // user has no access
  const allowed = "allowedCollection"; // user has read access
  const user = "trxPermUser";
  const pw = "pw";
  const key = "test1";

  return {

    setUpAll: function () {
      IM.rememberConnection();  // remember the root connection

      try {
        db._dropDatabase(dbName);
      } catch (err) { }
      try {
        users.remove(user);
      } catch (err) { }

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      db._create(secret);
      db._create(allowed);
      db._useDatabase("_system");

      users.save(user, pw);
      users.grantDatabase(user, dbName, "rw");
      // explicitly deny access to the secret collection, allow the other one
      users.grantCollection(user, dbName, allowed, "ro");
      users.grantCollection(user, dbName, secret, "none");
    },

    tearDownAll: function () {
      IM.reconnectMe();  // reconnect as root
      db._useDatabase("_system");
      try {
        db._dropDatabase(dbName);
      } catch (err) { }
      try {
        users.remove(user);
      } catch (err) { }
    },

    setUp: function () {
      IM.reconnectMe();  // reconnect as root
      db._useDatabase(dbName);
      db._collection(secret).truncate();
      db._collection(secret).insert({ _key: key, value: 42 });
      db._collection(allowed).truncate();
      db._collection(allowed).insert({ _key: key, value: 1 });
      db._useDatabase("_system");
    },

    tearDown: function () {
      IM.reconnectMe();  // reconnect as root
      db._useDatabase("_system");
    },

    // these tests intentionally use raw REST requests instead of the arangojs client
    // because the client uses explicit transaction collections which perform permission
    // checks at the time they are requested, which is not what we want to test here.

    testReadForbiddenCollectionInExistingTransaction: function () {
      arango.reconnect(arango.getEndpoint(), dbName, user, pw);

      let trx = db._createTransaction({ collections: {} });
      try {
        let ok = arango.GET_RAW(`/_api/document/${allowed}/${key}`,
          { "x-arango-trx-id": trx._id });
        assertEqual(200, ok.code);

        let res = arango.GET_RAW(`/_api/document/${secret}/${key}`,
          { "x-arango-trx-id": trx._id });
        assertEqual(403, res.code);
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, res.parsedBody.errorNum);
      } finally {
        trx.abort();
      }
    },

    testReadForbiddenCollectionStandalone: function () {
      arango.reconnect(arango.getEndpoint(), dbName, user, pw);

      let res = arango.GET_RAW(`/_api/document/${secret}/${key}`);
      assertEqual(403, res.code);
      assertEqual(ERRORS.ERROR_FORBIDDEN.code, res.parsedBody.errorNum);
    },

    testInsertForbiddenCollectionInExistingTransaction: function () {
      arango.reconnect(arango.getEndpoint(), dbName, user, pw);

      let trx = db._createTransaction({ collections: {} });
      try {
        let res = arango.POST_RAW(`/_api/document/${secret}`, { value: 1 },
          { "x-arango-trx-id": trx._id });
        assertEqual(403, res.code);
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, res.parsedBody.errorNum);
      } finally {
        trx.abort();
      }
    },

    testInsertForbiddenCollectionStandalone: function () {
      arango.reconnect(arango.getEndpoint(), dbName, user, pw);

      let res = arango.POST_RAW(`/_api/document/${secret}`, { value: 1 });
      assertEqual(403, res.code);
      assertEqual(ERRORS.ERROR_FORBIDDEN.code, res.parsedBody.errorNum);
    },

    testUpdateForbiddenCollectionInExistingTransaction: function () {
      arango.reconnect(arango.getEndpoint(), dbName, user, pw);

      let trx = db._createTransaction({ collections: {} });
      try {
        let res = arango.PATCH_RAW(`/_api/document/${secret}/${key}`,
          { value: 2 }, { "x-arango-trx-id": trx._id });
        assertEqual(403, res.code);
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, res.parsedBody.errorNum);
      } finally {
        trx.abort();
      }
    },

    testUpdateForbiddenCollectionStandalone: function () {
      arango.reconnect(arango.getEndpoint(), dbName, user, pw);

      let res = arango.PATCH_RAW(`/_api/document/${secret}/${key}`,
        { value: 2 });
      assertEqual(403, res.code);
      assertEqual(ERRORS.ERROR_FORBIDDEN.code, res.parsedBody.errorNum);
    },

    testRemoveForbiddenCollectionInExistingTransaction: function () {
      arango.reconnect(arango.getEndpoint(), dbName, user, pw);

      let trx = db._createTransaction({ collections: {} });
      try {
        let res = arango.DELETE_RAW(`/_api/document/${secret}/${key}`,
          {}, { "x-arango-trx-id": trx._id });
        assertEqual(403, res.code);
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, res.parsedBody.errorNum);
      } finally {
        trx.abort();
      }
    },

    testRemoveForbiddenCollectionStandalone: function () {
      arango.reconnect(arango.getEndpoint(), dbName, user, pw);

      let res = arango.DELETE_RAW(`/_api/document/${secret}/${key}`, {});
      assertEqual(403, res.code);
      assertEqual(ERRORS.ERROR_FORBIDDEN.code, res.parsedBody.errorNum);
    },

    testCountForbiddenCollectionInExistingTransaction: function () {
      arango.reconnect(arango.getEndpoint(), dbName, user, pw);

      let trx = db._createTransaction({ collections: {} });
      try {
        let ok = arango.GET_RAW(`/_api/collection/${allowed}/count`,
          { "x-arango-trx-id": trx._id });
        assertEqual(200, ok.code);

        let res = arango.GET_RAW(`/_api/collection/${secret}/count`,
          { "x-arango-trx-id": trx._id });
        assertEqual(403, res.code);
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, res.parsedBody.errorNum);
      } finally {
        trx.abort();
      }
    },

    testCountForbiddenCollectionStandalone: function () {
      arango.reconnect(arango.getEndpoint(), dbName, user, pw);

      let res = arango.GET_RAW(`/_api/collection/${secret}/count`);
      assertEqual(403, res.code);
      assertEqual(ERRORS.ERROR_FORBIDDEN.code, res.parsedBody.errorNum);
    },

  };
}

jsunity.run(authTransactionCollectionPermissionSuite);

return jsunity.done();
