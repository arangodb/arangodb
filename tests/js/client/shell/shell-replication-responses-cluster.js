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

const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const _ = require('lodash');
const db = arangodb.db;
const { debugCanUseFailAt, debugClearFailAt, debugSetFailAt, getEndpointById, getEndpointsByType } = require('@arangodb/test-helper');
const request = require('@arangodb/request');

function followerResponsesSuite() {
  'use strict';
  const cn = 'UnitTestsCollection';

  return {

    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },
    
    testInsert: function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });

      let shards = db._collection(cn).shards(true);
      let shard = Object.keys(shards)[0];
      let servers = shards[shard];

      let leader = servers[0];
      let follower = servers[1];
      
      let endpoint = getEndpointById(follower);
      debugSetFailAt(endpoint, "synchronousReplication::neverRefuseOnFollower");

      // send a single document replication insert request
      let response = request({
        method: "post",
        url: endpoint + "/_api/document/" + shard + "?isSynchronousReplication=" + encodeURIComponent(leader),
        body: { _key: "test1" },
        json: true
      });

      // verify that response is empty
      assertEqual({}, response.json);
      
      // send multi document replication insert request
      response = request({
        method: "post",
        url: endpoint + "/_api/document/" + shard + "?isSynchronousReplication=" + encodeURIComponent(leader),
        body: [ { _key: "test2" }, { _key: "test3" } ],
        json: true
      });

      // verify that response is empty
      assertEqual([], response.json);
    },
    
    testUpdate: function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);

      let shards = db._collection(cn).shards(true);
      let shard = Object.keys(shards)[0];
      let servers = shards[shard];

      let leader = servers[0];
      let follower = servers[1];
      
      let endpoint = getEndpointById(follower);
      debugSetFailAt(endpoint, "synchronousReplication::neverRefuseOnFollower");

      // send a single document replication update request
      let response = request({
        method: "patch",
        url: endpoint + "/_api/document/" + shard + "/test1?isSynchronousReplication=" + encodeURIComponent(leader),
        body: { _key: "test1" },
        json: true
      });

      // verify that response is empty
      assertEqual({}, response.json);
      
      // send multi document replication update request
      response = request({
        method: "patch",
        url: endpoint + "/_api/document/" + shard + "?isSynchronousReplication=" + encodeURIComponent(leader),
        body: [ { _key: "test2" }, { _key: "test3" } ],
        json: true
      });

      // verify that response is empty
      assertEqual([], response.json);
    },
    
    testRemove: function () {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);

      let shards = db._collection(cn).shards(true);
      let shard = Object.keys(shards)[0];
      let servers = shards[shard];

      let leader = servers[0];
      let follower = servers[1];
      
      let endpoint = getEndpointById(follower);
      debugSetFailAt(endpoint, "synchronousReplication::neverRefuseOnFollower");

      // send a single document replication remove request
      let response = request({
        method: "delete",
        url: endpoint + "/_api/document/" + shard + "/test1?isSynchronousReplication=" + encodeURIComponent(leader),
        body: {},
        json: true
      });

      // verify that response is empty
      assertEqual({}, response.json);
      
      // send multi document replication remove request
      response = request({
        method: "delete",
        url: endpoint + "/_api/document/" + shard + "?isSynchronousReplication=" + encodeURIComponent(leader),
        body: [ { _key: "test2" }, { _key: "test3" } ],
        json: true
      });

      // verify that response is empty
      assertEqual([], response.json);
    },
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  jsunity.run(followerResponsesSuite);
}
return jsunity.done();
