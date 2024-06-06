/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual, assertFalse, assertTrue */

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

let jsunity = require('jsunity');
let arangodb = require('@arangodb');
let internal = require('internal');
let _ = require('lodash');
let db = arangodb.db;
let { agency, getMetric, getEndpointById } = require('@arangodb/test-helper');
  
const cn = 'UnitTestsCollection';
  
function LeaderResignSuite() {
  'use strict';

  return {
    setUp: function () {
      let c = db._create(cn, {numberOfShards: 1, replicationFactor: 3});
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value: i});
      }
      c.insert(docs);
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testResignLeaderWhileWriteQueryRuns: function () {
      const c = db._collection(cn);
      const shards = db._collection(cn).shards(true);
      const shardId = Object.keys(shards)[0];

      const expectedShards = shards[shardId];
      const leader = getEndpointById(expectedShards[0]);

//      curl http://localhost:4001/_api/agency/write -d '[[{"/arango/Plan/Collections/_system/10043/shards/s10044":["_PRMR-434a57f2-199d-4289-be95-3ee6a2ec1ea2","PRMR-549e4fee-775f-4d8e-940c-afc3b4e5a7a9","PRMR-faa38bf9-5a6d-4e07-81be-ad59f8523ed9"], "/arango/Plan/Version":{"op":"increment"}}]]'; echo
      const planKey = `/arango/Plan/Collections/_system/${c._id}/shards/${shardId}`;
      let planValue = agency.call("read", [[planKey]])[0].arango.Plan.Collections._system[c._id].shards[shardId];
      assertEqual(expectedShards, planValue);
      
      let droppedFollowersBefore = getMetric(leader, "arangodb_dropped_followers_total");
      
      // make leader resign
      let body = {"/arango/Plan/Version":{op:"increment"}};
      let shardsWithResignedLeader = _.clone(expectedShards);
      // prepend underscore to server name
      shardsWithResignedLeader[0] = "_" + shardsWithResignedLeader[0];
      body[planKey] = {"op":"set", "new": shardsWithResignedLeader};
      agency.call("write", [[body]]);
  
      // wait for servers to pick up the changes
      internal.sleep(5);
      
      // reactivate leader
      body[planKey] = {"op":"set", "new": expectedShards};
      agency.call("write", [[body]]);
      
      // wait for servers to pick up the changes
      internal.sleep(5);

      // insert a single document. this must not drop the follower
      c.insert({});
      
      // no followers must have been dropped by the insert
      let droppedFollowersAfter = getMetric(leader, "arangodb_dropped_followers_total");
      assertEqual(droppedFollowersBefore, droppedFollowersAfter);
    },
      
  };
}

jsunity.run(LeaderResignSuite);
return jsunity.done();
