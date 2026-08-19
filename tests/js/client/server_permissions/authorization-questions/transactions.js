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
  DOC_COLLECTION
} = require('@arangodb/testutils/apitest-fixtures');

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

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
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
  };
}

jsunity.run(transactionApiAuthzSuite);
return jsunity.done();
