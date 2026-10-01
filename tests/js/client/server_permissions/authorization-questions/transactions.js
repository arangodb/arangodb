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

// Authorization questions asked by the /_api/transaction endpoint family.
//
// Observation-based counterpart of tests/api/apitests/transactions.mjs.
//
// Handler: arangod/RestHandler/RestTransactionHandler.cpp
//
// - List (GET /_api/transaction) and get-state (GET /_api/transaction/{id})
//   only inspect the transaction manager, no collection question.
// - Begin (POST /_api/transaction/begin) creates a managed transaction; the
//   declared collections are added to the transaction and checked in the
//   transaction layer (StorageEngine/TransactionState.cpp checkCollectionPermission).
//   For read:['c'] this asks `UseCollection ... level=read`.
// - Commit (PUT), abort (DELETE /{id}) and abort-all-writes
//   (DELETE /_api/transaction/write) only act on an already-open transaction
//   via the manager, so they ask no collection question.
// - The JS transaction (POST /_api/transaction) runs its action inside a
//   transaction over the declared read collections.
// Every request additionally asks `UseApiVersion version=0` and
// `UseDatabase name=d level=read` first.
// Transactions used as preconditions are created as root BEFORE beginObserve().
//
// Other users' transactions: own transactions never ask more than the base
// checks. A transaction of another user asks `AdminMonitorTransactions` when
// it is listed or its state is requested, and `AdminKillTransactions` when it
// is aborted (also by DELETE /_api/transaction/write). Committing another
// user's transaction stays reserved to its owner and the superuser. The
// requests of the plain users go through the request module with basic auth,
// so the arangosh connection stays root for the observer.

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
  tearDownApiTestData,
  DB,
  DOC_COLLECTION
} = require('@arangodb/testutils/apitest-fixtures');
const { assertEqual, assertTrue, assertFalse } = jsunity.jsUnity.assertions;
const internal = require('internal');
const request = require('@arangodb/request');
const users = require('@arangodb/users');

const baseQuestions = [
  'UseApiVersion version=0',
  `UseDatabase name=${DB} level=read`
];
const monitorQuestions = baseQuestions.concat(['AdminMonitorTransactions']);
const killQuestions = baseQuestions.concat(['AdminKillTransactions']);

const password = 'testi';
const transactionApi = `/_db/${DB}/_api/transaction`;

// sends a request as `user` to the connected server without touching the
// arangosh connection, which has to stay root for the observer
function sendAs (user, method, path, body) {
  const url = arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  return request({
    method,
    url: url + path,
    body,
    json: true,
    headers: {
      authorization: `Basic ${internal.base64Encode(user + ':' + password)}`
    }
  });
}

function transactionApiAuthzSuite () {
  const c = DOC_COLLECTION;

  // begin a stream transaction as root (before observation) and return its id
  function beginTrx (collections) {
    const res = arango.POST_RAW(`/_db/${DB}/_api/transaction/begin`,
                                { collections: collections });
    return res.parsedBody.result.id;
  }
  function abortTrx (id) {
    if (id !== undefined && id !== null) {
      arango.DELETE_RAW(`/_db/${DB}/_api/transaction/${id}`);
    }
  }

  // begins a stream transaction as `user` (before observation) and returns its id
  function beginTrxAs (user, collections) {
    const res = sendAs(user, 'POST', `${transactionApi}/begin`, { collections });
    assertEqual(201, res.status, JSON.stringify(res.json));
    return res.json.result.id;
  }
  // ids of the transactions root (an admin) can see
  function transactionIdsAsRoot () {
    return arango.GET(transactionApi).transactions.map((trx) => trx.id);
  }
  function abortAllAsRoot () {
    transactionIdsAsRoot().forEach(abortTrx);
  }

  return {
    setUpAll: function () {
      setUpApiTestData();
      // alice may write to the test database, bob may only read it; root
      // keeps read-write access to _system and is therefore an admin
      users.save('alice', password);
      users.grantDatabase('alice', DB, 'rw');
      users.save('bob', password);
      users.grantDatabase('bob', DB, 'ro');
    },

    tearDownAll: function () {
      abortAllAsRoot();
      ['alice', 'bob'].forEach((user) => {
        try {
          users.remove(user);
        } catch (err) {
        }
      });
      tearDownApiTestData();
    },

    tearDown: function () {
      disableObserve();
      abortAllAsRoot();
    },

    // GET /_api/transaction - list ongoing transactions; manager only
    testListTransactions: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/transaction`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // GET /_api/transaction/{id} - get state; manager only
    testGetTransactionState: function () {
      const id = beginTrx({ read: [c] });
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/transaction/${id}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
      abortTrx(id);
    },

    // POST /_api/transaction - JS transaction reading from c -> read trx on c
    // AUDIT: requires a V8 context; without V8 the server returns 503 before
    // the transaction runs and only the base UseDatabase question is asked.
    testRunJsTransaction: function () {
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/transaction`, {
        collections: { read: [c] },
        action: 'function () { return 1; }'
      });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // POST /_api/transaction/begin - begin read stream trx on c -> read check
    testBeginReadTransaction: function () {
      beginObserve();
      const res = arango.POST_RAW(`/_db/${DB}/_api/transaction/begin`,
                                  { collections: { read: [c] } });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
      if (res.parsedBody && res.parsedBody.result) {
        abortTrx(res.parsedBody.result.id);
      }
    },

    // PUT /_api/transaction/{id} - commit an open write trx; manager only
    testCommitTransaction: function () {
      const id = beginTrx({ write: [c] });
      arango.POST_RAW(`/_db/${DB}/_api/document/${c}`,
                      { _key: 'apitester-trx-doc', value: 999 },
                      { 'x-arango-trx-id': id });
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/transaction/${id}`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
      // committed -> the document now exists; remove it again
      arango.DELETE_RAW(`/_db/${DB}/_api/document/${c}/apitester-trx-doc`);
    },

    // DELETE /_api/transaction/{id} - abort an open write trx; manager only
    testAbortTransaction: function () {
      const id = beginTrx({ write: [c] });
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/transaction/${id}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // DELETE /_api/transaction/write - abort all write transactions; manager only
    testAbortAllWriteTransactions: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/transaction/write`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // ── transactions of other users ──────────────────────────────────────

    testListOwnTransactionAsUser: function () {
      const id = beginTrxAs('alice', { read: [c] });

      beginObserve();
      const res = sendAs('alice', 'GET', transactionApi);
      assertPermissions(baseQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
      assertTrue(res.json.transactions.some((trx) => trx.id === id));
    },

    testListForeignTransactionAsUser: function () {
      const id = beginTrxAs('alice', { read: [c] });

      beginObserve();
      const res = sendAs('bob', 'GET', transactionApi);
      assertPermissions(monitorQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
      assertFalse(res.json.transactions.some((trx) => trx.id === id));
    },

    testListForeignTransactionAsAdmin: function () {
      const id = beginTrxAs('alice', { read: [c] });

      beginObserve();
      const res = arango.GET_RAW(transactionApi);
      assertPermissions(monitorQuestions, endObserve());

      assertEqual(200, res.code, JSON.stringify(res.parsedBody));
      assertTrue(res.parsedBody.transactions.some((trx) => trx.id === id));
    },

    testGetForeignTransactionStateAsUser: function () {
      const id = beginTrxAs('alice', { read: [c] });

      beginObserve();
      const res = sendAs('bob', 'GET', `${transactionApi}/${id}`);
      assertPermissions(monitorQuestions, endObserve());

      assertEqual(404, res.status, JSON.stringify(res.json));
    },

    testGetForeignTransactionStateAsAdmin: function () {
      const id = beginTrxAs('alice', { read: [c] });

      beginObserve();
      const res = arango.GET_RAW(`${transactionApi}/${id}`);
      assertPermissions(monitorQuestions, endObserve());

      assertEqual(200, res.code, JSON.stringify(res.parsedBody));
      assertEqual('running', res.parsedBody.result.status);
    },

    testAbortOwnTransactionAsUser: function () {
      const id = beginTrxAs('alice', { write: [c] });

      beginObserve();
      const res = sendAs('alice', 'DELETE', `${transactionApi}/${id}`);
      assertPermissions(baseQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
    },

    testAbortForeignTransactionAsUser: function () {
      const id = beginTrxAs('alice', { write: [c] });

      beginObserve();
      const res = sendAs('bob', 'DELETE', `${transactionApi}/${id}`);
      assertPermissions(killQuestions, endObserve());

      assertEqual(404, res.status, JSON.stringify(res.json));
      // still running
      assertTrue(transactionIdsAsRoot().includes(id));
    },

    testAbortForeignTransactionAsAdmin: function () {
      const id = beginTrxAs('alice', { write: [c] });

      beginObserve();
      const res = arango.DELETE_RAW(`${transactionApi}/${id}`);
      assertPermissions(killQuestions, endObserve());

      assertEqual(200, res.code, JSON.stringify(res.parsedBody));
      assertFalse(transactionIdsAsRoot().includes(id));
    },

    // committing stays reserved to the owner (and the superuser): no admin
    // question is asked and the transaction is reported as not found
    testCommitForeignTransactionAsAdmin: function () {
      const id = beginTrxAs('alice', { write: [c] });

      beginObserve();
      const res = arango.PUT_RAW(`${transactionApi}/${id}`, {});
      assertPermissions(baseQuestions, endObserve());

      assertEqual(404, res.code, JSON.stringify(res.parsedBody));
      assertTrue(transactionIdsAsRoot().includes(id));
    },

    testAbortAllWriteTransactionsAsUserKeepsForeign: function () {
      const id = beginTrxAs('alice', { write: [c] });

      beginObserve();
      const res = sendAs('bob', 'DELETE', `${transactionApi}/write`);
      assertPermissions(killQuestions, endObserve());

      assertEqual(200, res.status, JSON.stringify(res.json));
      assertTrue(transactionIdsAsRoot().includes(id));
    },

    testAbortAllWriteTransactionsAsAdminAbortsForeign: function () {
      const id = beginTrxAs('alice', { write: [c] });

      beginObserve();
      const res = arango.DELETE_RAW(`${transactionApi}/write`);
      assertPermissions(killQuestions, endObserve());

      assertEqual(200, res.code, JSON.stringify(res.parsedBody));
      assertFalse(transactionIdsAsRoot().includes(id));
    },
  };
}

jsunity.run(transactionApiAuthzSuite);
return jsunity.done();
