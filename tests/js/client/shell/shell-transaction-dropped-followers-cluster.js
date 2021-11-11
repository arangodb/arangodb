/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let _ = require('lodash');
let db = arangodb.db;
let { getEndpointById, getEndpointsByType } = require('@arangodb/test-helper');
const request = require('@arangodb/request');

function getMetric(endpoint, name) {
  let res = request.get({
    url: endpoint + '/_admin/metrics/v2',
  });
  let re = new RegExp("^" + name + "\\{");
  let matches = res.body.split('\n').filter((line) => !line.match(/^#/)).filter((line) => line.match(re));
  if (!matches.length) {
    throw "Metric " + name + " not found";
  }
  return Number(matches[0].replace(/^.*?\} (\d+)$/, '$1'));
}

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

  return {

    setUp: function () {
      db._drop(cn);
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

      let shards = db._collection(cn ).shards(true);
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

      let shards = db._collection(cn ).shards(true);
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

      let shards = db._collection(cn ).shards(true);
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

      let shards = db._collection(cn ).shards(true);
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

      let shards = db._collection(cn ).shards(true);
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

      let shards = db._collection(cn ).shards(true);
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
        assertTrue(intermediateCommitsBefore[s] + 5 < intermediateCommitsAfter[s]);
      });
    },
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length >= 3) {
  jsunity.run(transactionDroppedFollowersSuite);
}
return jsunity.done();
