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

// Authorization questions asked by the /_api/simple endpoint family.
//
// Observation-based counterpart of tests/api/apitests/simple.mjs.
//
// Handlers: arangod/RestHandler/RestSimpleQueryHandler.cpp (all, all-keys,
// by-example) and arangod/RestHandler/RestSimpleHandler.cpp (lookup-by-keys,
// remove-by-keys). Both derive from RestCursorHandler: they translate the
// simple query into an AQL query and run it through registerQueryOrCursor().
// The collection access check happens in two layers: the collection is loaded
// via Database::loadCollection() (vocbase.cpp:387), which always asks
// `UseCollection ... level=read`, and a write query additionally registers the
// collection in the transaction as write ->
// TransactionState::checkCollectionPermission asks `UseCollection ...
// level=writedata`. So a READ query (all/all-keys/by-example/lookup-by-keys)
// asks only read, while remove-by-keys (a REMOVE) asks read + writedata. Every
// request additionally asks `UseDatabase name=d level=read` first.

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

function simpleApiAuthzSuite () {
  const c = DOC_COLLECTION;

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
    },

    // PUT /_api/simple/all - AQL "FOR doc IN c ... RETURN doc" -> read trx
    testAll: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/simple/all`, { collection: c, limit: 1 });
      assertPermissions([
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // PUT /_api/simple/all-keys - AQL "FOR doc IN c RETURN doc._key" -> read trx
    testAllKeys: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/simple/all-keys`,
                     { collection: c, limit: 1 });
      assertPermissions([
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // PUT /_api/simple/by-example - AQL "FOR doc IN c FILTER ... RETURN doc"
    // -> read trx
    testByExample: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/simple/by-example`,
                     { collection: c, example: { Hallo: 1 } });
      assertPermissions([
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // PUT /_api/simple/lookup-by-keys - AQL
    // "FOR doc IN c FILTER doc._key IN @keys RETURN doc" -> read trx
    testLookupByKeys: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/simple/lookup-by-keys`,
                     { collection: c, keys: ['nonexistent-key-apitester-99999'] });
      assertPermissions([
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // PUT /_api/simple/remove-by-keys - AQL
    // "FOR key IN @keys REMOVE key IN c": c is loaded (read, loadCollection) and
    // registered for write (writedata, checkCollectionPermission).
    testRemoveByKeys: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/simple/remove-by-keys`,
                     { collection: c, keys: ['nonexistent-key-apitester-99999'] });
      assertPermissions([
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

jsunity.run(simpleApiAuthzSuite);
return jsunity.done();
