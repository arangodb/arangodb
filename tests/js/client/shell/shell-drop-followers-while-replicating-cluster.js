/* jshint globalstrict:false, strict:false, maxlen: 200 */
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
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let arangodb = require('@arangodb');
let db = arangodb.db;
let { getServerById, waitForShardsInSync } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

function dropFollowersWhileReplicatingSuite() {
  'use strict';
  const cn = 'UnitTestsReplication';

  let setupCollection = function () {
    let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });

    let shards = c.shards(true);
    let shardName = Object.keys(shards)[0];
    let leader = shards[shardName][0];
    let follower = shards[shardName][1];

    return { c, leader, follower };
  };
     
  return {

    setUp: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._drop(cn);
    },

    tearDown: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._drop(cn);
    },
    
    testSingleOperationDropFollowerWhileReplicating: function() {
      let { c, leader, follower } = setupCollection();
      IM.debugSetFailAt("replicateOperationsDropFollower", '', leader);
      
      c.insert({});
      
      IM.debugClearFailAt("replicateOperationsDropFollower", '', leader);

      assertEqual(1, c.count());
      waitForShardsInSync(cn, 60, 1);
    },
    
    testMultiOperationDropFollowerWhileReplicating: function() {
      let { c, leader, follower } = setupCollection();
     
      IM.debugSetFailAt("replicateOperationsDropFollower", '', leader);
      
      c.insert([{}, {}, {}, {}]);
      
      IM.debugSetFailAt("replicateOperationsDropFollower", '', leader);
      
      assertEqual(4, c.count());
      waitForShardsInSync(cn, 60, 1);
    },
    
    testAqlDropFollowerWhileReplicating: function() {
      let { c, leader, follower } = setupCollection();
     
      IM.debugSetFailAt("replicateOperationsDropFollower", '', leader);
     
      db._query("FOR i IN 1..10 INSERT {} INTO " + cn);
      
      IM.debugSetFailAt("replicateOperationsDropFollower", '', leader);
      
      assertEqual(10, c.count());
      waitForShardsInSync(cn, 60, 1);
    },
    
    testSingleOperationBuildEmptyTransactionBody: function() {
      let { c, leader, follower } = setupCollection();
     
      IM.debugSetFailAt("buildTransactionBodyEmpty", '', leader);
      
      c.insert({});
      
      IM.debugSetFailAt("buildTransactionBodyEmpty", '', leader);
      
      assertEqual(1, c.count());
      waitForShardsInSync(cn, 60, 1);
    },
    
    testMultiOperationBuildEmptyTransactionBody: function() {
      let { c, leader, follower } = setupCollection();
     
      IM.debugSetFailAt("buildTransactionBodyEmpty", '', leader);
      
      c.insert([{}, {}, {}, {}]);
      
      IM.debugSetFailAt("buildTransactionBodyEmpty", '', leader);
      
      assertEqual(4, c.count());
      waitForShardsInSync(cn, 60, 1);
    },
    
    testAqlBuildEmptyTransactionBody: function() {
      let { c, leader, follower } = setupCollection();
     
      IM.debugSetFailAt("buildTransactionBodyEmpty", '', leader);
      
      db._query("FOR i IN 1..10 INSERT {} INTO " + cn);
      
      IM.debugSetFailAt("buildTransactionBodyEmpty", '', leader);
      
      assertEqual(10, c.count());
      waitForShardsInSync(cn, 60, 1);
    },
  };
}

if (IM.debugCanUseFailAt()) {
  // only execute if failure tests are available
  jsunity.run(dropFollowersWhileReplicatingSuite);
}
return jsunity.done();
