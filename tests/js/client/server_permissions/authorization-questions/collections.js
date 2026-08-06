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

// Authorization questions asked by the /_api/collection endpoint family.
//
// This is the jsunity, observation-based counterpart of
// tests/api/apitests/collections.mjs: it fires the same requests but, instead
// of tabulating status codes for a user matrix, it asserts which authorization
// questions RestCollectionHandler asks the ExecContext.
//
// Handler: arangod/RestHandler/RestCollectionHandler.cpp
//
// Every request first asks `UseApiVersion version=0` and then
// `UseDatabase name=d level=read` in
// RestHandler::checkUserCanAccess(). Read metadata endpoints then look the
// collection up via methods::Collections::lookup(), which asks
// `UseCollection ... level=read`. Write/metadata endpoints additionally ask a
// higher level (writedata/writemeta), and drop revokes permissions + cleans up
// graphs, hence the extra questions about _graphs/_users (cf.
// authorization-questions.js testDropCollection).
//
// Note: the `level=read` question on a written collection is asked whether the
// collection is reached via methods::Collections::lookup() or via a
// transaction (Database::loadCollection() during transaction begin), so a
// truncate shows both read and writedata.

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

function collectionApiAuthzSuite () {
  const c = DOC_COLLECTION;
  // a scratch collection for the destructive create/drop cells
  const tmp = 'c_apitest';

  function dropTmp () {
    arango.DELETE_RAW(`/_db/${DB}/_api/collection/${tmp}`);
  }
  function createTmp () {
    arango.POST_RAW(`/_db/${DB}/_api/collection`, { name: tmp });
  }

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      // make sure a test that threw in the middle of an observation does not
      // leave the log topic at TRACE
      disableObserve();
      dropTmp();
    },

    // GET /_api/collection - lists every collection the user canSee; the
    // handler asks canSeeCollection() for every (non-deleted) collection in
    // the database, including the system collections, so we enumerate them.
    testListCollections: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/collection`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "SeeCollection db=d name=c",
        "SeeCollection db=d name=e",
        "SeeCollection db=d name=_analyzers",
        "SeeCollection db=d name=_appbundles",
        "SeeCollection db=d name=_apps",
        "SeeCollection db=d name=_aqlfunctions",
        "SeeCollection db=d name=_frontend",
        "SeeCollection db=d name=_graphs",
        "SeeCollection db=d name=_jobs",
        "SeeCollection db=d name=_queues"
      ], endObserve());
    },

    // GET /_api/collection/c - lookup() -> UseCollection(Read)
    testGetCollection: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/collection/${c}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // GET /_api/collection/c/checksum - lookup() -> UseCollection(Read)
    testChecksum: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/collection/${c}/checksum`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // GET /_api/collection/c/count - lookup() -> UseCollection(Read)
    testCount: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/collection/${c}/count`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // GET /_api/collection/c/figures - lookup() -> UseCollection(Read)
    testFigures: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/collection/${c}/figures`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // GET /_api/collection/c/properties - lookup() -> UseCollection(Read)
    testGetProperties: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/collection/${c}/properties`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // GET /_api/collection/c/revision - lookup() -> UseCollection(Read)
    testRevision: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/collection/${c}/revision`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // GET /_api/collection/c/shards - lookup() -> UseCollection(Read)
    testShards: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/collection/${c}/shards`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // PUT /_api/collection/c/load - lookup() -> UseCollection(Read); no-op
    testLoad: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/collection/${c}/load`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // PUT /_api/collection/c/unload - lookup() -> UseCollection(Read); no-op
    testUnload: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/collection/${c}/unload`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // PUT /_api/collection/c/loadIndexesIntoMemory - lookup() ->
    // UseCollection(Read); warmup asks nothing extra
    testLoadIndexesIntoMemory: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/collection/${c}/loadIndexesIntoMemory`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // PUT /_api/collection/c/compact - lookup() -> UseCollection(Read) only;
    // the compaction itself runs without an ExecContext
    testCompact: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/collection/${c}/compact`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // PUT /_api/collection/c/properties - lookup() -> UseCollection(Read),
    // then updateProperties() -> UseCollection(WriteMeta), and the property
    // update runs in a transaction -> UseCollection(WriteData)
    testUpdateProperties: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/collection/${c}/properties`,
                     { waitForSync: false });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writemeta",
        ...singleOnly([
          "UseCollection db=d name=c level=writedata"
        ])
      ], endObserve());
    },

    // PUT /_api/collection/c/responsibleShard - lookup() -> UseCollection(Read)
    // (the cluster-only rejection happens after the lookup)
    testResponsibleShard: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/collection/${c}/responsibleShard`,
                     { _key: 'k1' });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // PUT /_api/collection/c/truncate - lookup() -> UseCollection(Read), then
    // the (exclusive) transaction -> UseCollection(WriteData)
    testTruncate: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/collection/${c}/truncate`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata"
      ], endObserve());
      // re-insert the 100 setup documents that truncate removed
      let docs = [];
      for (let i = 1; i <= 100; ++i) {
        docs.push({ _key: `k${i}`, value: i });
      }
      arango.POST_RAW(`/_db/${DB}/_api/document/${c}`, docs);
    },

    // PUT /_api/collection/tmp/rename - lookup() -> UseCollection(Read), then
    // rename() -> UseCollection(WriteMeta); renaming also fixes up the graph
    // definitions (_graphs) and looks the collection up under its new name
    testRename: function () {
      dropTmp();
      createTmp();
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/collection/${tmp}/rename`,
                     { name: `${tmp}_renamed` });
      // AUDIT: a coordinator only resolves the collection - neither the
      // writemeta question nor the graph cleanup is asked
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c_apitest level=read",
        ...singleOnly([
          // the read-only gate is only consulted where the writemeta
          // question is asked, i.e. not on a coordinator
          "IsReadOnly",
          "UseCollection db=d name=c_apitest level=writemeta",
          "UseCollection db=d name=_graphs level=read",
          "UseCollection db=d name=_graphs level=writedata",
          "UseCollection db=d name=c_apitest_renamed level=read"
        ])
      ], endObserve());
      arango.DELETE_RAW(`/_db/${DB}/_api/collection/${tmp}_renamed`);
    },

    // POST /_api/collection - canCreateCollection(); the new collection is
    // then granted to the creating user (_system's _users) and looked up
    testCreateCollection: function () {
      dropTmp();
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/collection`, { name: tmp });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "CreateCollection db=d name=c_apitest",
        "UseCollection db=d name=c_apitest level=read",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },

    // DELETE /_api/collection/tmp - canDropCollection(), lookup() ->
    // UseCollection(Read), plus the permission/graph cleanup reads on
    // _graphs and _users (cf. authorization-questions.js testDropCollection)
    testDropCollection: function () {
      dropTmp();
      createTmp();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/collection/${tmp}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "DropCollection db=d name=c_apitest",
        "UseCollection db=d name=c_apitest level=read",
        "UseCollection db=d name=_graphs level=read",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },
  };
}

jsunity.run(collectionApiAuthzSuite);
return jsunity.done();
