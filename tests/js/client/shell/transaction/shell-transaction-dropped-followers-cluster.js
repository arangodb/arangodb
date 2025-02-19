/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual, assertTrue */

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
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const _ = require('lodash');
const db = arangodb.db;
const { getEndpointById, getEndpointsByType, getMetric } = require('@arangodb/test-helper');

function getDroppedFollowers(servers) {
  let droppedFollowers = {};
  servers.forEach((serverId) => {
    let endpoint = getEndpointById(serverId);
    droppedFollowers[serverId] = getMetric(endpoint, "arangodb_dropped_followers_total");
  });
  return droppedFollowers;
}

function getIntermediateCommits(servers) {
  let intermediateCommits = {};
  servers.forEach((serverId) => {
    let endpoint = getEndpointById(serverId);
    intermediateCommits[serverId] = getMetric(endpoint, "arangodb_intermediate_commits_total");
  });
  return intermediateCommits;
}

function transactionDroppedFollowersSuite() {
  'use strict';
  const cn = 'UnitTestsTransaction';

  let isReplication2 = false;

  return {

    setUp: function () {
      db._drop(cn);
      isReplication2 = db._properties().replicationVersion === "2";
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testInsertSameFollower: function () {
      let c = db._create(cn, { numberOfShards: 40, replicationFactor: 3 });
      let docs = [];
      for (let i = 0; i < 1000; ++i) { 
        docs.push({});
      }

      let shards = db._collection(cn).shards(true);
      let servers = shards[Object.keys(shards)[0]];

      let droppedFollowersBefore = getDroppedFollowers(servers);

      for (let i = 0; i < 50; ++i) {
        c.insert(docs);
      }
      
      assertEqual(1000 * 50, c.count());

      // follower must not have been dropped
      let droppedFollowersAfter = getDroppedFollowers(servers);
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
    },
    
    testInsertAQLSameFollower: function () {
      let c = db._create(cn, { numberOfShards: 40, replicationFactor: 3 });

      let shards = db._collection(cn).shards(true);
      let servers = shards[Object.keys(shards)[0]];

      let droppedFollowersBefore = getDroppedFollowers(servers);

      for (let i = 0; i < 50; ++i) {
        db._query("FOR i IN 1..1000 INSERT {} INTO " + cn);
      }

      assertEqual(1000 * 50, c.count());
      
      // follower must not have been dropped
      let droppedFollowersAfter = getDroppedFollowers(servers);
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
    },

    testTransactionWritesSameFollower: function () {
      let c = db._create(cn, { numberOfShards: 40, replicationFactor: 3 });
      let docs = [];
      for (let i = 0; i < 1000; ++i) { 
        docs.push({});
      }
      const opts = {
        collections: {
          write: [ cn  ]
        }
      };

      let shards = db._collection(cn).shards(true);
      let servers = shards[Object.keys(shards)[0]];

      let droppedFollowersBefore = getDroppedFollowers(servers);

      for (let i = 0; i < 50; ++i) { 
        const trx = db._createTransaction(opts);
        const tc = trx.collection(cn);
        tc.insert(docs);
        let result = trx.commit();
        assertEqual("committed", result.status);
      }

      assertEqual(1000 * 50, c.count());
      
      // follower must not have been dropped
      let droppedFollowersAfter = getDroppedFollowers(servers);
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
    },
    
    testTransactionExclusiveSameFollower: function () {
      let c = db._create(cn, { numberOfShards: 40, replicationFactor: 3 });
      let docs = [];
      for (let i = 0; i < 1000; ++i) { 
        docs.push({});
      }
      const opts = {
        collections: {
          exclusive: [ cn  ]
        }
      };

      let shards = db._collection(cn).shards(true);
      let servers = shards[Object.keys(shards)[0]];

      let droppedFollowersBefore = getDroppedFollowers(servers);

      for (let i = 0; i < 50; ++i) { 
        const trx = db._createTransaction(opts);
        const tc = trx.collection(cn);
        tc.insert(docs);
        let result = trx.commit();
        assertEqual("committed", result.status);
      }

      assertEqual(1000 * 50, c.count());
      
      // follower must not have been dropped
      let droppedFollowersAfter = getDroppedFollowers(servers);
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
    },
    
    testTransactionAbortsSameFollower: function () {
      let c = db._create(cn, { numberOfShards: 40, replicationFactor: 3 });
      let docs = [];
      for (let i = 0; i < 1000; ++i) { 
        docs.push({});
      }
      const opts = {
        collections: {
          write: [ cn  ]
        }
      };

      let shards = db._collection(cn).shards(true);
      let servers = shards[Object.keys(shards)[0]];

      let droppedFollowersBefore = getDroppedFollowers(servers);

      for (let i = 0; i < 50; ++i) { 
        const trx = db._createTransaction(opts);
        const tc = trx.collection(cn);
        tc.insert(docs);
        let result = trx.abort();
        assertEqual("aborted", result.status);
      }

      assertEqual(0, c.count());
      
      // follower must not have been dropped
      let droppedFollowersAfter = getDroppedFollowers(servers);
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
    },

    testTransactionWritesSameFollowerIntermediateCommit: function () {
      let c = db._create(cn, { numberOfShards: 40, replicationFactor: 3 });
      let docs = [];
      for (let i = 0; i < 2000; ++i) { 
        docs.push({});
      }
      const opts = {
        collections: {
          write: [ cn  ]
        },
        intermediateCommitCount: 50
      };

      let shards = db._collection(cn).shards(true);
      let servers = shards[Object.keys(shards)[0]];

      let droppedFollowersBefore = getDroppedFollowers(servers);
      let intermediateCommitsBefore = getIntermediateCommits(servers);

      for (let i = 0; i < 10; ++i) { 
        const trx = db._createTransaction(opts);
        const tc = trx.collection(cn);
        tc.insert(docs);
        let result = trx.commit();
        assertEqual("committed", result.status);
      }

      assertEqual(2000 * 10, c.count());
      
      // follower must not have been dropped
      let droppedFollowersAfter = getDroppedFollowers(servers);
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
      
      let intermediateCommitsAfter = getIntermediateCommits(servers);

      Object.keys(intermediateCommitsBefore).forEach((s) => {
        if (isReplication2) {
          // For the non-AQL document API, intermediate commits are not performed on the leader.
          // Hence, they are not performed on the followers either.
          assertTrue(intermediateCommitsBefore[s] === intermediateCommitsAfter[s]);
        } else {
          // For replication1, intermediate commits are performed on the followers only.
          assertTrue(intermediateCommitsBefore[s] + 5 < intermediateCommitsAfter[s]);
        }
      });
    },

    testFollowerPrematureTrxAbort: function () {
      db._create(cn, { numberOfShards: 20, replicationFactor: 3 });
      const servers = [...new Set(Object.values(db._collection(cn).shards(true)).flat())];
      const droppedFollowersBefore = getDroppedFollowers(servers);

      // We have a query that results in a failure after the first 9000
      // documents because of a bad key. This will cause the leaders to abort
      // the transaction on the followers. However, the followers have
      // transactions that are shared with leaders of other shards. If one of
      // those leaders has not yet seen the error, then it will happily
      // continue to replicate to that follower. But if the follower has
      // already aborted the transaction, then it will reject the replication
      // request. Previously this would cause the follower to be dropped, but
      // now this should be handled gracefully.
      for (let i = 0; i < 10; ++i) {
        try {
          db._query(`FOR i IN 1..10000 INSERT { _key: i >= 9000 ? '' : CONCAT('test', i), value: i } INTO ${cn}`);
          fail();
        } catch (err) {
          assertEqual(arangodb.ERROR_ARANGO_DOCUMENT_KEY_BAD, err.errorNum);
        }
        assertEqual(0, db[cn].count());
        const droppedFollowersAfter = getDroppedFollowers(servers);
        assertEqual(droppedFollowersBefore, droppedFollowersAfter);
      }
    },

    testFollowerPrematureTrxAbortStreamingTransaction: function () {
      db._create(cn, { numberOfShards: 20, replicationFactor: 3 });
      const servers = [...new Set(Object.values(db._collection(cn).shards(true)).flat())];
      const droppedFollowersBefore = getDroppedFollowers(servers);

      // Same as the test above, but using a streaming transaction.
      for (let i = 0; i < 10; ++i) {
        const trx = db._createTransaction({ collections: { write: [cn] } });
        const col = trx.collection(cn);
        assertEqual(0, col.count());
        col.insert({});
        try {
          trx.query(`FOR i IN 1..10000 INSERT { _key: i >= 9000 ? '' : CONCAT('test', i), value: i } INTO ${cn}`);
          fail();
        } catch (err) {
          assertEqual(arangodb.ERROR_ARANGO_DOCUMENT_KEY_BAD, err.errorNum);
        }
        assertTrue(col.count() > 0);
        trx.abort();
        assertEqual(0, db[cn].count());
        const droppedFollowersAfter = getDroppedFollowers(servers);
        assertEqual(droppedFollowersBefore, droppedFollowersAfter);
      }
    },
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length >= 3) {
  jsunity.run(transactionDroppedFollowersSuite);
}
return jsunity.done();
