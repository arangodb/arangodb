/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
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
let db = arangodb.db;
let errors = arangodb.errors;
const request = require('@arangodb/request');

function getEndpointById(id) {
  const toEndpoint = (d) => (d.endpoint);
  const endpointToURL = (endpoint) => {
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    let pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  const instanceInfo = JSON.parse(internal.env.INSTANCEINFO);
  return instanceInfo.arangods.filter((d) => (d.id === id))
                              .map(toEndpoint)
                              .map(endpointToURL)[0];
}

function getEndpointsByType(type) {
  const isType = (d) => (d.role.toLowerCase() === type);
  const toEndpoint = (d) => (d.endpoint);
  const endpointToURL = (endpoint) => {
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    let pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  const instanceInfo = JSON.parse(internal.env.INSTANCEINFO);
  return instanceInfo.arangods.filter(isType)
                              .map(toEndpoint)
                              .map(endpointToURL);
}
  
/// @brief set failure point
function debugCanUseFailAt(endpoint) {
  let res = request.get({
    url: endpoint + '/_admin/debug/failat',
  });
  return res.status === 200;
}

/// @brief set failure point
function debugSetFailAt(endpoint, failAt) {
  let res = request.put({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error setting failure point";
  }
}

/// @brief remove failure point
function debugRemoveFailAt(endpoint, failAt) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure point";
  }
}

function debugClearFailAt(endpoint) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat',
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure points";
  }
}

function getChecksum(endpoint, name) {
  let res = request.get({
    url: endpoint + '/_api/collection/' + name + '/checksum',
  });
  return JSON.parse(res.body).checksum;
}

function getMetric(endpoint, name) {
  let res = request.get({
    url: endpoint + '/_admin/metrics',
  });
  let re = new RegExp("^" + name + "\\{");
  let matches = res.body.split('\n').filter((line) => !line.match(/^#/)).filter((line) => line.match(re));
  if (!matches.length) {
    throw "Metric " + name + " not found";
  }
  return Number(matches[0].replace(/^.*?\} (\d+)$/, '$1'));
}

function assertInSync(leader, follower, shardId) {
  const leaderChecksum = getChecksum(leader, shardId);
  let followerChecksum;
  let tries = 0;
  while (++tries < 120) {
    followerChecksum = getChecksum(follower, shardId);
    if (followerChecksum === leaderChecksum) {
      break;
    }
    internal.sleep(0.25);
  }
  assertEqual(leaderChecksum, followerChecksum);
}

function transactionIntermediateCommitsSingleSuite() {
  'use strict';
  const cn = 'UnitTestsTransaction';

  return {

    setUp: function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },

    tearDown: function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },
    
    testSoftAbort: function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let shards = db._collection(cn).shards(true);
      let shardId = Object.keys(shards)[0];
      let leader = getEndpointById(shards[shardId][0]);
      let follower = getEndpointById(shards[shardId][1]);
      debugSetFailAt(leader, "returnManagedTrxForceSoftAbort");
      debugSetFailAt(follower, "returnManagedTrxForceSoftAbort");
      
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_count");
      let intermediateCommitsBefore = getMetric(follower, "arangodb_intermediate_commits");

      const opts = {
        collections: {
          write: [cn]
        },
        intermediateCommitCount: 1000,
        options: { intermediateCommitCount: 1000 }
      };
      
      const trx = db._createTransaction(opts);
      const tc = trx.collection(cn);
      // the transaction will be soft-aborted after this call on the server
      let res = tc.insert({ _key: "test" });
      assertEqual("test", res._key);
      try {
        tc.insert({});
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_TRANSACTION_NOT_FOUND.code);
      }

      assertEqual(0, c.count());
      res = trx.abort();
      assertEqual("aborted", res.status);

      assertEqual(0, c.count());
      // follower must not have been dropped
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_count");
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
      
      let intermediateCommitsAfter = getMetric(follower, "arangodb_intermediate_commits");
      assertEqual(intermediateCommitsBefore, intermediateCommitsAfter);
      assertInSync(leader, follower, shardId);
    },
    
    // make follower execute intermediate commits (before the leader), but let the
    // transaction succeed
    testSingleAqlIntermediateCommitsOnFollower: function () {
      db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let shards = db._collection(cn).shards(true);
      let shardId = Object.keys(shards)[0];
      let leader = getEndpointById(shards[shardId][0]);
      let follower = getEndpointById(shards[shardId][1]);
      // disable intermediate commits on leader
      debugSetFailAt(leader, "noIntermediateCommits");
      // turn on intermediate commits on follower
      debugClearFailAt(follower, "noIntermediateCommits");
      
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_count");
      let intermediateCommitsBefore = getMetric(follower, "arangodb_intermediate_commits");
      db._query('FOR i IN 1..10000 INSERT { _key: CONCAT("test", i) } IN ' + cn, {}, {intermediateCommitCount: 1000});

      // follower must not have been dropped
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_count");
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
      
      let intermediateCommitsAfter = getMetric(follower, "arangodb_intermediate_commits");
      assertEqual(intermediateCommitsBefore + 10, intermediateCommitsAfter);
      
      assertInSync(leader, follower, shardId);
    },
    
    // make follower execute intermediate commits (before the leader), and let the
    // transaction fail
    testSingleAqlIntermediateCommitsOnFollowerWithRollback: function () {
      db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let shards = db._collection(cn).shards(true);
      let shardId = Object.keys(shards)[0];
      let leader = getEndpointById(shards[shardId][0]);
      let follower = getEndpointById(shards[shardId][1]);
      // disable intermediate commits on leader
      debugSetFailAt(leader, "noIntermediateCommits");
      // turn on intermediate commits on follower
      debugClearFailAt(follower, "noIntermediateCommits");
      
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_count");
      let intermediateCommitsBefore = getMetric(follower, "arangodb_intermediate_commits");
      let didWork = false;
      try {
        db._query('FOR i IN 1..10000 INSERT { _key: CONCAT("test", i), value: ASSERT(i < 10000, "peng!") } IN ' + cn, {}, {intermediateCommitCount: 1000});
        didWork = true;
      } catch (err) {
      }
      assertEqual(didWork, false);
      
      // follower must have been dropped
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_count");
      assertEqual(droppedFollowersBefore + 1, droppedFollowersAfter);
    
      let intermediateCommitsAfter = getMetric(follower, "arangodb_intermediate_commits");
      assertEqual(intermediateCommitsBefore + 9, intermediateCommitsAfter);
      
      assertInSync(leader, follower, shardId);
    },
    
    // make follower execute intermediate commits (before the leader), but make the
    // transaction succeed
    testSingleTransactionIntermediateCommitsOnFollower: function () {
      db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let shards = db._collection(cn).shards(true);
      let shardId = Object.keys(shards)[0];
      let leader = getEndpointById(shards[shardId][0]);
      let follower = getEndpointById(shards[shardId][1]);
      // disable intermediate commits on leader
      debugSetFailAt(leader, "noIntermediateCommits");
      // turn on intermediate commits on follower
      debugClearFailAt(follower, "noIntermediateCommits");
      
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_count");
      let intermediateCommitsBefore = getMetric(follower, "arangodb_intermediate_commits");
      db._executeTransaction({
        collections: { write: cn },
        action: function(params) {
          let db = require("internal").db;
          let c = db[params.cn];
          let docs = [];
          for (let i = 0; i < 10000; ++i) {
            docs.push({ _key: "test" + i });
            if (docs.length === 1000) {
              c.insert(docs);
              docs = [];
            }
          }
        },
        params: { cn },
        intermediateCommitCount: 1000,
      });
      
      // follower must not have been dropped
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_count");
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
    
      let intermediateCommitsAfter = getMetric(follower, "arangodb_intermediate_commits");
      assertEqual(intermediateCommitsBefore + 10, intermediateCommitsAfter);
      
      assertInSync(leader, follower, shardId);
    },
    
    // make follower execute intermediate commits (before the leader), and let the
    // transaction fail
    testSingleTransactionIntermediateCommitsOnFollowerWithRollback: function () {
      db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let shards = db._collection(cn).shards(true);
      let shardId = Object.keys(shards)[0];
      let leader = getEndpointById(shards[shardId][0]);
      let follower = getEndpointById(shards[shardId][1]);
      // disable intermediate commits on leader
      debugSetFailAt(leader, "noIntermediateCommits");
      // turn on intermediate commits on follower
      debugClearFailAt(follower, "noIntermediateCommits");
      
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_count");
      let intermediateCommitsBefore = getMetric(follower, "arangodb_intermediate_commits");
      let didWork = false;
      try {
        db._executeTransaction({
          collections: { write: cn },
          action: function(params) {
            let db = require("internal").db;
            let c = db[params.cn];
            let docs = [];
            for (let i = 0; i < 10000; ++i) {
              docs.push({ _key: "test" + i });
              if (docs.length === 1000) {
                c.insert(docs);
                docs = [];
              }
            }
            throw "peng!";
          },
          params: { cn },
          intermediateCommitCount: 1000,
        });
        didWork = true;
      } catch (ignore) {
      }
      assertEqual(didWork, false);

      // follower must have been dropped
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_count");
      assertEqual(droppedFollowersBefore + 1, droppedFollowersAfter);
    
      let intermediateCommitsAfter = getMetric(follower, "arangodb_intermediate_commits");
      assertEqual(intermediateCommitsBefore + 10, intermediateCommitsAfter);
      
      assertInSync(leader, follower, shardId);
    },
    
    // make follower execute intermediate commits (before the leader), and let the
    // transaction fail
    testSingleStreamingIntermediateCommitsOnFollowerWithRollback: function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let shards = db._collection(cn).shards(true);
      let shardId = Object.keys(shards)[0];
      let leader = getEndpointById(shards[shardId][0]);
      let follower = getEndpointById(shards[shardId][1]);
      // disable intermediate commits on leader
      debugSetFailAt(leader, "noIntermediateCommits");
      // turn on intermediate commits on follower
      debugClearFailAt(follower, "noIntermediateCommits");
      
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_count");
      let intermediateCommitsBefore = getMetric(follower, "arangodb_intermediate_commits");
      
      const opts = {
        collections: {
          write: [cn]
        },
        intermediateCommitCount: 1000,
        options: { intermediateCommitCount: 1000 }
      };
      
      const trx = db._createTransaction(opts);
      const tc = trx.collection(cn);
      let docs = [];
      for (let i = 0; i < 9950; ++i) {
        docs.push({ _key: "test" + i });
        if (docs.length === 50) {
          tc.insert(docs);
          docs = [];
        }
      }

      assertEqual(0, c.count());
      assertEqual(9950, tc.count());
      trx.abort();
      
      // follower must have been dropped
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_count");
      assertEqual(droppedFollowersBefore + 1, droppedFollowersAfter);
    
      let intermediateCommitsAfter = getMetric(follower, "arangodb_intermediate_commits");
      assertEqual(intermediateCommitsBefore + 9, intermediateCommitsAfter);
      
      assertEqual(0, c.count());
      assertInSync(leader, follower, shardId);
    },
    
  };
}

function transactionIntermediateCommitsMultiSuite() {
  'use strict';
  const cn = 'UnitTestsTransaction';

  return {

    setUp: function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn + "1");
      db._drop(cn + "2");
    },

    tearDown: function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn + "1");
      db._drop(cn + "2");
    },
    
    // make follower execute intermediate commits (before the leader), but let the
    // transaction succeed
    testMultiAqlIntermediateCommitsOnFollower: function () {
      db._create(cn + "1", { numberOfShards: 1, replicationFactor: 2 });
      let shards1 = db._collection(cn + "1").shards(true);
      let shardId1 = Object.keys(shards1)[0];
      let leader1 = getEndpointById(shards1[shardId1][0]);
      let follower1 = getEndpointById(shards1[shardId1][1]);

      let shards2, shardId2, leader2, follower2;

      let tries = 0;
      // try to set up 2 collections with different leaders but the same follower
      // DB server
      while (tries++ < 250) {
        db._create(cn + "2", { numberOfShards: 1, replicationFactor: 2 });
      
        shards2 = db._collection(cn + "2").shards(true);
        shardId2 = Object.keys(shards2)[0];
        leader2 = getEndpointById(shards2[shardId2][0]);
        follower2 = getEndpointById(shards2[shardId2][1]);

        if (leader1 !== leader2 && follower1 === follower2) {
          break;
        }

        db._drop(cn + "2");
      }
      assertNotEqual(leader1, leader2);
      assertEqual(follower1, follower2);
      
      // disable intermediate commits on leaders
      debugSetFailAt(leader1, "noIntermediateCommits");
      debugSetFailAt(leader2, "noIntermediateCommits");
      // turn on intermediate commits on follower
      debugClearFailAt(follower1, "noIntermediateCommits");

      let droppedFollowersBefore1 = getMetric(leader1, "arangodb_dropped_followers_count");
      let droppedFollowersBefore2 = getMetric(leader2, "arangodb_dropped_followers_count");
      db._query('FOR i IN 1..10000 INSERT { _key: CONCAT("test", i) } IN ' + cn + '1 INSERT { _key: CONCAT("test", i) } IN ' + cn + '2', {}, {intermediateCommitCount: 1000});
      
      // follower must not have been dropped
      let droppedFollowersAfter1 = getMetric(leader1, "arangodb_dropped_followers_count");
      let droppedFollowersAfter2 = getMetric(leader2, "arangodb_dropped_followers_count");
      assertEqual(droppedFollowersBefore1, droppedFollowersAfter1);
      assertEqual(droppedFollowersBefore2, droppedFollowersAfter2);
    
      assertInSync(leader1, follower1, shardId1);
      assertInSync(leader2, follower2, shardId2);
    },
    
    // make follower execute intermediate commits (before the leader), and let the
    // transaction fail
    testMultiAqlIntermediateCommitsOnFollowerWithRollback: function () {
      db._create(cn + "1", { numberOfShards: 1, replicationFactor: 2 });
      let shards1 = db._collection(cn + "1").shards(true);
      let shardId1 = Object.keys(shards1)[0];
      let leader1 = getEndpointById(shards1[shardId1][0]);
      let follower1 = getEndpointById(shards1[shardId1][1]);

      let shards2, shardId2, leader2, follower2;

      let tries = 0;
      // try to set up 2 collections with different leaders but the same follower
      // DB server
      while (tries++ < 250) {
        db._create(cn + "2", { numberOfShards: 1, replicationFactor: 2 });
      
        shards2 = db._collection(cn + "2").shards(true);
        shardId2 = Object.keys(shards2)[0];
        leader2 = getEndpointById(shards2[shardId2][0]);
        follower2 = getEndpointById(shards2[shardId2][1]);

        if (leader1 !== leader2 && follower1 === follower2) {
          break;
        }

        db._drop(cn + "2");
      }
      assertNotEqual(leader1, leader2);
      assertEqual(follower1, follower2);
      
      // disable intermediate commits on leaders
      debugSetFailAt(leader1, "noIntermediateCommits");
      debugSetFailAt(leader2, "noIntermediateCommits");
      // turn on intermediate commits on follower
      debugClearFailAt(follower1, "noIntermediateCommits");

      let droppedFollowersBefore1 = getMetric(leader1, "arangodb_dropped_followers_count");
      let droppedFollowersBefore2 = getMetric(leader2, "arangodb_dropped_followers_count");
      let didWork = false;
      try {
        db._query('FOR i IN 1..10000 INSERT { _key: CONCAT("test", i), value: ASSERT(i < 10000, "peng!") } IN ' + cn + '1 INSERT { _key: CONCAT("test", i), value: ASSERT(i < 10000, "peng!") } IN ' + cn + '2', {}, {intermediateCommitCount: 1000});
        didWork = true;
      } catch (err) {
      }
      assertEqual(didWork, false);
      
      // follower must have been dropped
      let droppedFollowersAfter1 = getMetric(leader1, "arangodb_dropped_followers_count");
      let droppedFollowersAfter2 = getMetric(leader2, "arangodb_dropped_followers_count");
      assertEqual(droppedFollowersBefore1 + 1, droppedFollowersAfter1);
      assertEqual(droppedFollowersBefore2 + 1, droppedFollowersAfter2);
    
      assertInSync(leader1, follower1, shardId1);
      assertInSync(leader2, follower2, shardId2);
    },
    
    // make two leaders write to the same follower, using exclusive locks and
    // intermediate commits
    testMultiAqlIntermediateCommitsOnFollowerExclusive: function () {
      db._create(cn + "1", { numberOfShards: 1, replicationFactor: 2 });
      let shards1 = db._collection(cn + "1").shards(true);
      let shardId1 = Object.keys(shards1)[0];
      let leader1 = getEndpointById(shards1[shardId1][0]);
      let follower1 = getEndpointById(shards1[shardId1][1]);

      let shards2, shardId2, leader2, follower2;

      let tries = 0;
      // try to set up 2 collections with different leaders but the same follower
      // DB server
      while (tries++ < 250) {
        db._create(cn + "2", { numberOfShards: 1, replicationFactor: 2 });
      
        shards2 = db._collection(cn + "2").shards(true);
        shardId2 = Object.keys(shards2)[0];
        leader2 = getEndpointById(shards2[shardId2][0]);
        follower2 = getEndpointById(shards2[shardId2][1]);

        if (leader1 !== leader2 && follower1 === follower2) {
          break;
        }

        db._drop(cn + "2");
      }
      assertNotEqual(leader1, leader2);
      assertEqual(follower1, follower2);
      
      // disable intermediate commits on leaders
      debugSetFailAt(leader1, "noIntermediateCommits");
      debugSetFailAt(leader2, "noIntermediateCommits");
      // turn on intermediate commits on follower
      debugClearFailAt(follower1, "noIntermediateCommits");

      let droppedFollowersBefore1 = getMetric(leader1, "arangodb_dropped_followers_count");
      let droppedFollowersBefore2 = getMetric(leader2, "arangodb_dropped_followers_count");
      db._query('FOR i IN 1..10000 INSERT { _key: CONCAT("test", i) } IN ' + cn + '1 OPTIONS { exclusive: true } INSERT { _key: CONCAT("test", i) } IN ' + cn + '2 OPTIONS { exclusive: true }', {}, {intermediateCommitCount: 1000});
      
      // follower must not have been dropped
      let droppedFollowersAfter1 = getMetric(leader1, "arangodb_dropped_followers_count");
      let droppedFollowersAfter2 = getMetric(leader2, "arangodb_dropped_followers_count");
      assertEqual(droppedFollowersBefore1, droppedFollowersAfter1);
      assertEqual(droppedFollowersBefore2, droppedFollowersAfter2);
    
      assertInSync(leader1, follower1, shardId1);
      assertInSync(leader2, follower2, shardId2);
    },
    
    // make two leaders write to the same follower, using exclusive locks and
    // intermediate commits
    testMultiStreamingIntermediateCommitsOnFollowerExclusive: function () {
      let c1 = db._create(cn + "1", { numberOfShards: 1, replicationFactor: 2 });
      let shards1 = db._collection(cn + "1").shards(true);
      let shardId1 = Object.keys(shards1)[0];
      let leader1 = getEndpointById(shards1[shardId1][0]);
      let follower1 = getEndpointById(shards1[shardId1][1]);

      let shards2, shardId2, leader2, follower2;

      let tries = 0;
      // try to set up 2 collections with different leaders but the same follower
      // DB server
      let c2;
      while (tries++ < 250) {
        c2 = db._create(cn + "2", { numberOfShards: 1, replicationFactor: 2 });
      
        shards2 = db._collection(cn + "2").shards(true);
        shardId2 = Object.keys(shards2)[0];
        leader2 = getEndpointById(shards2[shardId2][0]);
        follower2 = getEndpointById(shards2[shardId2][1]);

        if (leader1 !== leader2 && follower1 === follower2) {
          break;
        }

        db._drop(cn + "2");
      }
      assertNotEqual(leader1, leader2);
      assertEqual(follower1, follower2);
      
      // disable intermediate commits on leaders
      debugSetFailAt(leader1, "noIntermediateCommits");
      debugSetFailAt(leader2, "noIntermediateCommits");
      // turn on intermediate commits on follower
      debugClearFailAt(follower1, "noIntermediateCommits");
      
      let droppedFollowersBefore1 = getMetric(leader1, "arangodb_dropped_followers_count");
      let droppedFollowersBefore2 = getMetric(leader2, "arangodb_dropped_followers_count");
      let intermediateCommitsBefore = getMetric(follower1, "arangodb_intermediate_commits");
      
      const opts = {
        collections: {
          write: [cn + "1", cn + "2" ]
        },
        intermediateCommitCount: 1000,
        options: { intermediateCommitCount: 1000 }
      };
      
      const trx = db._createTransaction(opts);
      const tc1 = trx.collection(cn + "1");
      const tc2 = trx.collection(cn + "2");
      let docs = [];
      for (let i = 0; i < 9950; ++i) {
        docs.push({ _key: "test" + i });
        if (docs.length === 50) {
          tc1.insert(docs);
          tc2.insert(docs);
          docs = [];
        }
      }
      assertEqual(9950, tc1.count());
      assertEqual(9950, tc2.count());
      assertEqual(0, c1.count());
      assertEqual(0, c2.count());
      trx.commit();
      
      assertEqual(9950, c1.count());
      assertEqual(9950, c2.count());
      
      let droppedFollowersAfter1 = getMetric(leader1, "arangodb_dropped_followers_count");
      let droppedFollowersAfter2 = getMetric(leader2, "arangodb_dropped_followers_count");
      let intermediateCommitsAfter = getMetric(follower1, "arangodb_intermediate_commits");
      assertEqual(droppedFollowersBefore1, droppedFollowersAfter1);
      assertEqual(droppedFollowersBefore2, droppedFollowersAfter2);
      assertEqual(intermediateCommitsBefore + Math.floor((9950 + 9950) / 1000), intermediateCommitsAfter);
    
      assertInSync(leader1, follower1, shardId1);
      assertInSync(leader2, follower2, shardId2);
    },
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(transactionIntermediateCommitsSingleSuite);
  if (ep.length >= 3) {
    jsunity.run(transactionIntermediateCommitsMultiSuite);
  }
}
return jsunity.done();
