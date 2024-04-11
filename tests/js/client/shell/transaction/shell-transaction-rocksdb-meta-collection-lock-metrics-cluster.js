"use strict";
/* global fail, assertEqual, assertTrue, assertFalse, arango */
// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

const _ = require('lodash');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let {
  getCompleteMetricsValues,
  waitForShardsInSync
} = require('@arangodb/test-helper');


const database = "MyTestDatabaseRocksDBMetaCollectionLock";
const collection = "MyTestCollection";

function TransactionRocksDBMetaCollectionLockMetricsSuite() {

  const getAllMetrics = function () {
    return getCompleteMetricsValues([
          "arangodb_vocbase_meta_collection_lock_pending_exclusive",
          "arangodb_vocbase_meta_collection_lock_pending_shared",
          "arangodb_vocbase_meta_collection_lock_locked_exclusive",
          "arangodb_vocbase_meta_collection_lock_locked_shared"],
        "dbservers"
    );
  };

  const assertMetrics = function (expected) {
    const actual = getAllMetrics();
    assertEqual(expected.arangodb_vocbase_meta_collection_lock_pending_exclusive, actual[0]);
    assertEqual(expected.arangodb_vocbase_meta_collection_lock_pending_shared, actual[1]);
    assertEqual(expected.arangodb_vocbase_meta_collection_lock_locked_exclusive, actual[2]);
    assertEqual(expected.arangodb_vocbase_meta_collection_lock_locked_shared, actual[3]);
  };

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
      db._create(collection, {numberOfShards: 1, replicationFactor: 1});
    },

    tearDownAll: function () {
      db._useDatabase('_system');
      db._dropDatabase(database);
    },

    testSharedLock: function () {
      {
        assertMetrics({
          arangodb_vocbase_meta_collection_lock_pending_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_pending_shared: 0,
          arangodb_vocbase_meta_collection_lock_locked_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_locked_shared: 0,
        });
      }

      const trx = db._createTransaction({collections: {write: [collection]}});
      trx.collection(collection).insert({});

      {
        assertMetrics({
          arangodb_vocbase_meta_collection_lock_pending_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_pending_shared: 0,
          arangodb_vocbase_meta_collection_lock_locked_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_locked_shared: 1,
        });
      }

      trx.abort();

      {
        assertMetrics({
          arangodb_vocbase_meta_collection_lock_pending_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_pending_shared: 0,
          arangodb_vocbase_meta_collection_lock_locked_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_locked_shared: 0,
        });
      }
    },

    testExclusiveLock: function () {
      {
        assertMetrics({
          arangodb_vocbase_meta_collection_lock_pending_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_pending_shared: 0,
          arangodb_vocbase_meta_collection_lock_locked_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_locked_shared: 0,
        });
      }

      const trx = db._createTransaction({collections: {exclusive: [collection]}});
      trx.collection(collection).insert({});

      {
        assertMetrics({
          arangodb_vocbase_meta_collection_lock_pending_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_pending_shared: 0,
          arangodb_vocbase_meta_collection_lock_locked_exclusive: 1,
          arangodb_vocbase_meta_collection_lock_locked_shared: 0,
        });
      }

      trx.abort();

      {
        assertMetrics({
          arangodb_vocbase_meta_collection_lock_pending_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_pending_shared: 0,
          arangodb_vocbase_meta_collection_lock_locked_exclusive: 0,
          arangodb_vocbase_meta_collection_lock_locked_shared: 0,
        });
      }
    },

  };
};

jsunity.run(TransactionRocksDBMetaCollectionLockMetricsSuite);
return jsunity.done();
