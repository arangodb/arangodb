/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual, assertFalse */

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

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let errors = arangodb.errors;
let { getEndpointById,
      getEndpointsByType,
      getChecksum,
      getMetric
    } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

const cn = 'UnitTestsTransaction';

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

function transactionIntermediateCommitsBabiesFollowerSuite() {
  'use strict';

  let isReplication2 = false;

  let collectionInfo = () => {
    let shards = db._collection(cn).shards(true);
    let shardId = Object.keys(shards)[0];
    let leader = getEndpointById(shards[shardId][0]);
    let follower = getEndpointById(shards[shardId][1]);

    return [shardId, leader, follower];
  };

  return {

    setUp: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._drop(cn);
      db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      isReplication2 = db._properties().replicationVersion === "2";
    },

    tearDown: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._drop(cn);
    },
    
    testAqlIntermediateCommitsNoErrors: function () {
      let [shardId, leader, follower] = collectionInfo();
      
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_total");
      let intermediateCommitsBefore = getMetric(leader, "arangodb_intermediate_commits_total");
      db._query('FOR i IN 1..1000 INSERT { _key: CONCAT("test", i) } IN ' + cn, {}, {intermediateCommitCount: 100});

      // follower must not have been dropped
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_total");
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
      
      let intermediateCommitsAfter = getMetric(leader, "arangodb_intermediate_commits_total");
      assertEqual(intermediateCommitsBefore + 1, intermediateCommitsAfter);

      assertEqual(1000, db._collection(cn).count());
      assertInSync(leader, follower, shardId);
    },

    testAqlIntermediateCommitsWithFailurePoint: function () {
      let [shardId, leader, follower] = collectionInfo();
      // trigger a certain failure point on the leader
      IM.debugSetFailAt("insertLocal::fakeResult1", instanceRole.dbServer, leader);

      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_total");
      let intermediateCommitsBefore = getMetric(leader, "arangodb_intermediate_commits_total");

      try {
        db._query('FOR i IN 1..1000 INSERT { _key: CONCAT("test", i) } IN ' + cn, {}, {intermediateCommitCount: 100});
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
      }

      // follower must not have been dropped
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_total");
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
      
      // no intermediate commit must have happened
      let intermediateCommitsAfter = getMetric(leader, "arangodb_intermediate_commits_total");
      assertEqual(intermediateCommitsBefore, intermediateCommitsAfter);

      assertEqual(0, db._collection(cn).count());
      assertInSync(leader, follower, shardId);
    },
    
    testAqlIntermediateCommitsWithOtherFailurePoint: function () {
      let [shardId, leader, follower] = collectionInfo();
      // trigger a certain failure point on the leader
      IM.debugSetFailAt("insertLocal::fakeResult2", instanceRole.dbServer, leader);
      
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_total");
      let intermediateCommitsBefore = getMetric(leader, "arangodb_intermediate_commits_total");
      try {
        db._query('FOR i IN 1..1000 INSERT { _key: CONCAT("test", i) } IN ' + cn, {}, {intermediateCommitCount: 100});
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
      }

      // follower must not have been dropped
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_total");
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
      
      // no intermediate commit must have happened
      let intermediateCommitsAfter = getMetric(leader, "arangodb_intermediate_commits_total");
      assertEqual(intermediateCommitsBefore, intermediateCommitsAfter);

      assertEqual(0, db._collection(cn).count());
      assertInSync(leader, follower, shardId);
    },
    
    testAqlIntermediateCommitsWithRollback: function () {
      let [shardId, leader, follower] = collectionInfo();
      
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_total");
      let intermediateCommitsBefore = getMetric(leader, "arangodb_intermediate_commits_total");
      let didWork = false;
      try {
        db._query('FOR i IN 1..10000 INSERT { _key: CONCAT("test", i), value: ASSERT(i < 10000, "peng!") } IN ' + cn, {}, {intermediateCommitCount: 100});
        didWork = true;
      } catch (err) {
      }
      assertFalse(didWork);

      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_total");
      if (isReplication2) {
        assertEqual(droppedFollowersBefore, droppedFollowersAfter);
      } else {
        // a follower will be dropped here because we abort the transaction on the leader,
        // but have had intermediate commits on the follower
        assertEqual(droppedFollowersBefore + 1, droppedFollowersAfter);
      }
    
      // some intermediate commit must have happened (not 100, but 9, because AQL insert operates
      // in batch sizes of 1000)
      let intermediateCommitsAfter = getMetric(leader, "arangodb_intermediate_commits_total");
      assertEqual(intermediateCommitsBefore + 9, intermediateCommitsAfter);
      
      assertEqual(9000, db._collection(cn).count());
      assertInSync(leader, follower, shardId);
    },
  };
}

function transactionIntermediateCommitsBabiesSuite() {
  'use strict';
  let collectionInfo = () => {
    let shards = db._collection(cn).shards(true);
    let shardId = Object.keys(shards)[0];
    let leader = getEndpointById(shards[shardId][0]);
    return leader;
  };

  let buildDocuments = () => {
    let docs = [];
    for (let i = 1; i <= 1000; ++i) {
      docs.push({ _key: "test" + i });
    }
    return docs;
  };

  return {

    setUp: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._drop(cn);
      db._create(cn, { numberOfShards: 1, replicationFactor: 1 });
    },

    tearDown: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._drop(cn);
    },
    
    testDocumentsSingleArray: function () {
      let leader = collectionInfo();
      let docs = buildDocuments();
      let intermediateCommitsBefore = getMetric(leader, "arangodb_intermediate_commits_total");

      db._collection(cn).insert(docs);

      // no intermediate commits
      let intermediateCommitsAfter = getMetric(leader, "arangodb_intermediate_commits_total");
      assertEqual(intermediateCommitsBefore, intermediateCommitsAfter);

      assertEqual(1000, db._collection(cn).count());
    },
      
    testDocumentsSingleArrayIntermediateCommitsSuppressed: function () {
      let leader = collectionInfo();
      // trigger a certain failure point on the leader
      IM.debugSetFailAt("TransactionState::intermediateCommitCount100", instanceRole.dbServer, leader);
      
      let docs = buildDocuments();
      let intermediateCommitsBefore = getMetric(leader, "arangodb_intermediate_commits_total");
      db._collection(cn).insert(docs);

      // no intermediate commit must have happened
      let intermediateCommitsAfter = getMetric(leader, "arangodb_intermediate_commits_total");
      assertEqual(intermediateCommitsBefore, intermediateCommitsAfter);

      assertEqual(1000, db._collection(cn).count());
    },
    
    testDocumentsTransaction: function () {
      let leader = collectionInfo();
      let docs = buildDocuments();
      let intermediateCommitsBefore = getMetric(leader, "arangodb_intermediate_commits_total");

      let opts = { collections: { write: cn } }; 
      let trx = db._createTransaction(opts);

      try {
        trx.collection(cn).insert(docs);
        trx.commit();
      } catch (err) {
        trx.abort();
      }

      // no intermediate commits
      let intermediateCommitsAfter = getMetric(leader, "arangodb_intermediate_commits_total");
      assertEqual(intermediateCommitsBefore, intermediateCommitsAfter);
        
      assertEqual(1000, db._collection(cn).count());
    },
    
    testDocumentsTransactionIntermediateCommitsSuppressed: function () {
      let leader = collectionInfo();
      let docs = buildDocuments();
      let intermediateCommitsBefore = getMetric(leader, "arangodb_intermediate_commits_total");

      let opts = { collections: { write: cn }, intermediateCommitCount: 100, options: { intermediateCommitCount: 100 } }; 
      let trx = db._createTransaction(opts);

      try {
        trx.collection(cn).insert(docs);
        trx.commit();
      } catch (err) {
        trx.abort();
      }

      // no intermediate commits
      let intermediateCommitsAfter = getMetric(leader, "arangodb_intermediate_commits_total");
      assertEqual(intermediateCommitsBefore, intermediateCommitsAfter);
        
      assertEqual(1000, db._collection(cn).count());
    },
  };
}

if (IM.debugCanUseFailAt()) {
  // only execute if failure tests are available
  jsunity.run(transactionIntermediateCommitsBabiesFollowerSuite);
  jsunity.run(transactionIntermediateCommitsBabiesSuite);
}
return jsunity.done();
