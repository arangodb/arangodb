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

// Authorization questions asked by the /_api/cursor endpoint family.
//
// Observation-based counterpart of tests/api/apitests/cursor.mjs.
//
// Handler: arangod/RestHandler/RestCursorHandler.cpp
//
// Creating a cursor (POST /_api/cursor) runs the AQL query. For the query used
// here (FOR d IN c RETURN d) collection c is accessed read-only, so the
// transaction layer (StorageEngine/TransactionState.cpp checkCollectionPermission)
// asks `UseCollection ... level=read`. Advancing (POST/PUT /_api/cursor/<id>,
// POST /_api/cursor/<id>/<batch-id>) and discarding (DELETE /_api/cursor/<id>)
// an already-materialised cursor only touch the cursor repository, so they ask
// no collection question beyond the base `UseDatabase name=d level=read`.
// The pre-existing cursors are created as root BEFORE beginObserve(), so their
// creation questions are not observed.

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

function cursorApiAuthzSuite () {
  const c = DOC_COLLECTION;

  // create a cursor with batchSize 10 (100 docs -> hasMore) as root, before
  // observation, and return its response body
  function createCursor () {
    const res = arango.POST_RAW(`/_db/${DB}/_api/cursor`,
                                { query: `FOR d IN ${c} RETURN d`, batchSize: 10 });
    return res.parsedBody;
  }
  function dropCursor (id) {
    if (id !== undefined && id !== null) {
      arango.DELETE_RAW(`/_db/${DB}/_api/cursor/${id}`);
    }
  }

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
    },

    // POST /_api/cursor - single batch; query executes -> read trx on c
    testCreateCursorSingleBatch: function () {
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/cursor`, { query: `FOR d IN ${c} RETURN d` });
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // POST /_api/cursor - batchSize 10; query executes -> read trx on c, then a
    // cursor is left open (discarded afterwards)
    testCreateCursorBatchSize: function () {
      beginObserve();
      const res = arango.POST_RAW(`/_db/${DB}/_api/cursor`,
                                  { query: `FOR d IN ${c} RETURN d`, batchSize: 10 });
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
      dropCursor(res.parsedBody.id);
    },

    // POST /_api/cursor/<id> - advance an existing cursor; reads from the
    // materialised result, no collection question
    // AUDIT: assumes advancing a non-streaming cursor re-checks no collection
    // permission (result already materialised at creation time).
    testAdvanceCursorPost: function () {
      const cur = createCursor();
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/cursor/${cur.id}`, {});
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=d level=read"
      ], endObserve());
      dropCursor(cur.id);
    },

    // PUT /_api/cursor/<id> - advance an existing cursor
    // AUDIT: see testAdvanceCursorPost.
    testAdvanceCursorPut: function () {
      const cur = createCursor();
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/cursor/${cur.id}`, {});
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=d level=read"
      ], endObserve());
      dropCursor(cur.id);
    },

    // POST /_api/cursor/<id>/<batch-id> - advance by explicit batch id
    // AUDIT: see testAdvanceCursorPost.
    testAdvanceCursorByBatchId: function () {
      const cur = createCursor();
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/cursor/${cur.id}/${cur.nextBatchId}`, {});
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=d level=read"
      ], endObserve());
      dropCursor(cur.id);
    },

    // DELETE /_api/cursor/<id> - discard a cursor; only touches the repository
    testDeleteCursor: function () {
      const cur = createCursor();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/cursor/${cur.id}`);
      assertPermissions([
        "UseApiVersion version=1",
        "UseDatabase name=d level=read"
      ], endObserve());
    },
  };
}

jsunity.run(cursorApiAuthzSuite);
return jsunity.done();
