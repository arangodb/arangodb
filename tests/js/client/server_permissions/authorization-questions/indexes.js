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

// Authorization questions asked by the /_api/index endpoint family.
//
// Observation-based counterpart of tests/api/apitests/indexes.mjs.
//
// Handler: arangod/RestHandler/RestIndexHandler.cpp (on a single server the
// collection() lookup itself asks no ExecContext question; the checks live in
// VocBase/Methods/Indexes.cpp).
//
// - GET /_api/index (list) uses a READ SingleCollectionTransaction ->
//   `UseCollection ... level=read`.
// - GET /_api/index/selectivity begins a READ transaction ->
//   `UseCollection ... level=read`.
// - POST /_api/index (create) calls ExecContext::canCreateIndex() ->
//   `UseCollection ... level=writemeta`.
// - POST /_api/index/sync-caches only calls engine.syncIndexCaches(), so it
//   asks no collection question (AUTHEN).
// - DELETE /_api/index (drop) calls canUseDatabase(Write) ->
//   `UseDatabase ... level=write` and canUseCollection(WriteMeta) ->
//   `UseCollection ... level=writemeta`, then begins an EXCLUSIVE transaction
//   whose permission check maps to `UseCollection ... level=writedata`.
// Every request additionally asks `UseDatabase name=d level=read` first.

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
const {
  setUpApiTestData,
  tearDownApiTestData,
  DB,
  DOC_COLLECTION
} = require('@arangodb/testutils/apitest-fixtures');

function indexApiAuthzSuite () {
  const useD = `UseDatabase name=${DB} level=read`;
  const useDWrite = `UseDatabase name=${DB} level=write`;
  const c = DOC_COLLECTION;
  const readC = `UseCollection db=${DB} name=${c} level=read`;
  const writeMetaC = `UseCollection db=${DB} name=${c} level=writemeta`;
  const writeDataC = `UseCollection db=${DB} name=${c} level=writedata`;

  // create a persistent index as root (before observation), return its handle
  function createIndex () {
    const res = arango.POST_RAW(`/_db/${DB}/_api/index?collection=${c}`,
                                { type: 'persistent', fields: ['value'] });
    return res.parsedBody.id;
  }
  function dropIndex (handle) {
    if (handle !== undefined && handle !== null) {
      arango.DELETE_RAW(`/_db/${DB}/_api/index/${handle}`);
    }
  }

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
    },

    // GET /_api/index?collection=c - list indexes; READ transaction
    testListIndexes: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/index?collection=${c}`);
      assertPermissions([useD, readC], endObserve());
    },

    // GET /_api/index/selectivity?collection=c - READ transaction
    testSelectivity: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/index/selectivity?collection=${c}`);
      assertPermissions([useD, readC], endObserve());
    },

    // POST /_api/index?collection=c - canCreateIndex() -> writemeta
    // AUDIT: index creation (methods::Indexes::ensureIndex -> createIndex) may
    // fill the index inside a transaction; if that transaction runs under the
    // caller's ExecContext it could add read/writedata questions as well.
    testCreateIndex: function () {
      beginObserve();
      const res = arango.POST_RAW(`/_db/${DB}/_api/index?collection=${c}`,
                                  { type: 'persistent', fields: ['value'] });
      assertPermissions([useD, writeMetaC], endObserve());
      if (res.parsedBody && res.parsedBody.id) {
        dropIndex(res.parsedBody.id);
      }
    },

    // POST /_api/index/sync-caches - engine.syncIndexCaches() only (AUTHEN)
    testSyncCaches: function () {
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/index/sync-caches`, {});
      assertPermissions([useD], endObserve());
    },

    // DELETE /_api/index/<handle> - drop() checks canUseDatabase(write) +
    // canUseCollection(writemeta); the EXCLUSIVE drop transaction
    // (Indexes::createTrxForDrop) both loads the collection -> read
    // (Database::loadCollection) and registers it for write -> writedata.
    testDropIndex: function () {
      const handle = createIndex();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/index/${handle}`);
      assertPermissions([useD, useDWrite, readC, writeMetaC, writeDataC],
                        endObserve());
    },
  };
}

jsunity.run(indexApiAuthzSuite);
return jsunity.done();
