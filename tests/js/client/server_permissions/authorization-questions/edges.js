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

// Authorization questions asked by the /_api/edges endpoint family.
//
// Observation-based counterpart of tests/api/apitests/edges.mjs.
//
// Handler: arangod/RestHandler/RestEdgesHandler.cpp
//
// The edges handler does not check permissions itself: validateCollection()
// only resolves the collection name (no ExecContext question). It then runs an
// internal read-only AQL query (FOR e IN @@collection FILTER ... RETURN e) via
// StandaloneContext, so the collection access check happens in the transaction
// layer (StorageEngine/TransactionState.cpp checkCollectionPermission): a READ
// transaction asks `UseCollection ... level=read`. Every request additionally
// asks `UseApiVersion version=0` and `UseDatabase name=d level=read` first.

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
  EDGE_COLLECTION
} = require('@arangodb/testutils/apitest-fixtures');

function edgesApiAuthzSuite () {
  const e = EDGE_COLLECTION;

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
    },

    // GET /_api/edges/e?vertex=c/k1 - internal read AQL query on e -> read trx
    testReadEdgesGet: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/edges/${e}?vertex=c%2Fk1&direction=any`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=e level=read"
      ], endObserve());
    },

    // POST /_api/edges/e - internal read AQL query per vertex on e -> read trx
    // (all vertices share collection e, so the questions dedup to one read)
    testReadEdgesPost: function () {
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/edges/${e}`, ['c/k1', 'c/k2']);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=e level=read"
      ], endObserve());
    },
  };
}

jsunity.run(edgesApiAuthzSuite);
return jsunity.done();
