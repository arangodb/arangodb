/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief dropping followers while replicating
// /
// / DISCLAIMER
// /
// / Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
let arangodb = require('@arangodb');
let db = arangodb.db;
let { getEndpointsByType,
      getEndpointById,
      debugCanUseFailAt,
      debugClearFailAt,
      debugSetFailAt,
      waitForShardsInSync
    } = require('@arangodb/test-helper');
      
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
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },

    tearDown: function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },
    
    testSingleOperationDropFollowerWhileReplicating: function() {
      let { c, leader, follower } = setupCollection();
     
      debugSetFailAt(getEndpointById(leader), "replicateOperationsDropFollower");
      
      c.insert({});
      
      debugClearFailAt(getEndpointById(leader), "replicateOperationsDropFollower");

      assertEqual(1, c.count());
      waitForShardsInSync(cn, 60);
    },
    
    testMultiOperationDropFollowerWhileReplicating: function() {
      let { c, leader, follower } = setupCollection();
     
      debugSetFailAt(getEndpointById(leader), "replicateOperationsDropFollower");
      
      c.insert([{}, {}, {}, {}]);
      
      debugClearFailAt(getEndpointById(leader), "replicateOperationsDropFollower");
      
      assertEqual(4, c.count());
      waitForShardsInSync(cn, 60);
    },
    
    testAqlDropFollowerWhileReplicating: function() {
      let { c, leader, follower } = setupCollection();
     
      debugSetFailAt(getEndpointById(leader), "replicateOperationsDropFollower");
     
      db._query("FOR i IN 1..10 INSERT {} INTO " + cn);
      
      debugClearFailAt(getEndpointById(leader), "replicateOperationsDropFollower");
      
      assertEqual(10, c.count());
      waitForShardsInSync(cn, 60);
    },
    
    testSingleOperationBuildEmptyTransactionBody: function() {
      let { c, leader, follower } = setupCollection();
     
      debugSetFailAt(getEndpointById(leader), "buildTransactionBodyEmpty");
      
      c.insert({});
      
      debugClearFailAt(getEndpointById(leader), "buildTransactionBodyEmpty");
      
      assertEqual(1, c.count());
      waitForShardsInSync(cn, 60);
    },
    
    testMultiOperationBuildEmptyTransactionBody: function() {
      let { c, leader, follower } = setupCollection();
     
      debugSetFailAt(getEndpointById(leader), "buildTransactionBodyEmpty");
      
      c.insert([{}, {}, {}, {}]);
      
      debugClearFailAt(getEndpointById(leader), "buildTransactionBodyEmpty");
      
      assertEqual(4, c.count());
      waitForShardsInSync(cn, 60);
    },
    
    testAqlBuildEmptyTransactionBody: function() {
      let { c, leader, follower } = setupCollection();
     
      debugSetFailAt(getEndpointById(leader), "buildTransactionBodyEmpty");
      
      db._query("FOR i IN 1..10 INSERT {} INTO " + cn);
      
      debugClearFailAt(getEndpointById(leader), "buildTransactionBodyEmpty");
      
      assertEqual(10, c.count());
      waitForShardsInSync(cn, 60);
    },
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(dropFollowersWhileReplicatingSuite);
}
return jsunity.done();
