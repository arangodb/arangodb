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

// Authorization questions asked by the /_api/document endpoint family.
//
// Observation-based counterpart of tests/api/apitests/documents.mjs.
//
// Handler: arangod/RestHandler/RestDocumentHandler.cpp
//
// The document handler itself asks nothing directly; the collection questions
// come from two distinct places, so a write operation asks BOTH a read and a
// writedata question for the same collection:
//   - read:      Database::loadCollection() (vocbase.cpp:387) is called when the
//                collection is loaded into use at transaction begin
//                (useCollections() -> lockUsage() -> ensureCollection()); it
//                unconditionally asks `UseCollection ... level=read`.
//   - writedata: TransactionState::checkCollectionPermission() asks
//                `UseCollection ... level=writedata` when the collection is
//                registered in the transaction with write access.
// A pure read operation only loads the collection, so it asks read alone.
// Every request additionally asks `UseApiVersion version=0` and
// `UseDatabase name=d level=read` first.
//
// The writedata question is accompanied by the server-wide read-only gate, which
// shows up as the pseudo-question `IsReadOnly`; a pure read never triggers it.

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
  singleOnly
} = require('@arangodb/testutils/apitest-fixtures');

function documentApiAuthzSuite () {
  const c = DOC_COLLECTION;
  const key = 'testdoc';

  function makeDoc () {
    arango.DELETE_RAW(`/_db/${DB}/_api/document/${c}/${key}`);
    arango.POST_RAW(`/_db/${DB}/_api/document/${c}`, { _key: key, value: 1 });
  }
  function dropDoc () {
    arango.DELETE_RAW(`/_db/${DB}/_api/document/${c}/${key}`);
  }

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
      dropDoc();
    },

    // GET /_api/document/c/key - read transaction
    testReadDocument: function () {
      makeDoc();
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/document/${c}/${key}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // HEAD /_api/document/c/key - read transaction
    testHeadDocument: function () {
      makeDoc();
      beginObserve();
      arango.HEAD_RAW(`/_db/${DB}/_api/document/${c}/${key}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // POST /_api/document/c - write transaction (read + writedata)
    testInsertDocument: function () {
      dropDoc();
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/document/${c}`, { _key: key, value: 1 });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read"
        ])
      ], endObserve());
    },

    // PUT /_api/document/c/key - replace with key
    testReplaceDocumentWithKey: function () {
      makeDoc();
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/document/${c}/${key}`, { value: 2 });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read"
        ])
      ], endObserve());
    },

    // PUT /_api/document/c - replace without key (batch)
    testReplaceDocumentWithoutKey: function () {
      makeDoc();
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/document/${c}`, [{ _key: key, value: 2 }]);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read"
        ])
      ], endObserve());
    },

    // PATCH /_api/document/c/key - update with key
    testUpdateDocumentWithKey: function () {
      makeDoc();
      beginObserve();
      arango.PATCH_RAW(`/_db/${DB}/_api/document/${c}/${key}`, { value: 3 });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read"
        ])
      ], endObserve());
    },

    // PATCH /_api/document/c - update without key (batch)
    testUpdateDocumentWithoutKey: function () {
      makeDoc();
      beginObserve();
      arango.PATCH_RAW(`/_db/${DB}/_api/document/${c}`, [{ _key: key, value: 3 }]);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read"
        ])
      ], endObserve());
    },

    // DELETE /_api/document/c/key - delete with key
    testDeleteDocumentWithKey: function () {
      makeDoc();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/document/${c}/${key}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read"
        ])
      ], endObserve());
    },

    // DELETE /_api/document/c - delete without key (batch)
    testDeleteDocumentWithoutKey: function () {
      makeDoc();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/document/${c}`, [{ _key: key }]);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read"
        ])
      ], endObserve());
    },
  };
}

jsunity.run(documentApiAuthzSuite);
return jsunity.done();
