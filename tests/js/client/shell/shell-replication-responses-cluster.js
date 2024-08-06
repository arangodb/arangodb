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
const { debugCanUseFailAt, debugClearFailAt, debugSetFailAt, getEndpointById, getUrlById, getEndpointsByType } = require('@arangodb/test-helper');
const request = require('@arangodb/request');
const isReplication2 = db._properties().replicationVersion === "2";
const {ERROR_HTTP_NOT_ACCEPTABLE, ERROR_REPLICATION_REPLICATED_STATE_NOT_AVAILABLE} = internal.errors;
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

function followerResponsesSuite() {
  'use strict';
  const cn = 'UnitTestsCollection';

  const assertIsReplication2FollowerResponse = (body) => {
    const {error, code, errorNum} = body;
    assertTrue(error, `Should get error response, instead got ${JSON.stringify(body)}`);
    assertEqual(ERROR_HTTP_NOT_ACCEPTABLE.code, code, `Should get error response, instead got ${JSON.stringify(body)}`);
    assertEqual(ERROR_REPLICATION_REPLICATED_STATE_NOT_AVAILABLE.code, errorNum, `Should get error response, instead got ${JSON.stringify(body)}`);
  };

  return {

    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
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
      let url = getUrlById(follower);
      IM.debugSetFailAt("synchronousReplication::neverRefuseOnFollower", instanceRole.dbServer, endpoint);

      // send a single document replication insert request
      let response = request({
        method: "post",
        url: url + "/_api/document/" + shard + "?isSynchronousReplication=" + encodeURIComponent(leader),
        body: { _key: "test1" },
        json: true
      });

      if (isReplication2) {
        assertIsReplication2FollowerResponse(response.json);
      } else {
        // verify that response is empty
        assertEqual({}, response.json);
      }
      
      // send multi document replication insert request
      response = request({
        method: "post",
        url: url + "/_api/document/" + shard + "?isSynchronousReplication=" + encodeURIComponent(leader),
        body: [ { _key: "test2" }, { _key: "test3" } ],
        json: true
      });

      if (isReplication2) {
        assertIsReplication2FollowerResponse(response.json);
      } else {
        // verify that response is empty
        assertEqual({}, response.json);
      }
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
      let url = getUrlById(follower);
      IM.debugSetFailAt("synchronousReplication::neverRefuseOnFollower", instanceRole.dbServer, endpoint);

      // send a single document replication update request
      let response = request({
        method: "patch",
        url: url + "/_api/document/" + shard + "/test1?isSynchronousReplication=" + encodeURIComponent(leader),
        body: { _key: "test1" },
        json: true
      });

      if (isReplication2) {
        assertIsReplication2FollowerResponse(response.json);
      } else {
        // verify that response is empty
        assertEqual({}, response.json);
      }
      
      // send multi document replication update request
      response = request({
        method: "patch",
        url: url + "/_api/document/" + shard + "?isSynchronousReplication=" + encodeURIComponent(leader),
        body: [ { _key: "test2" }, { _key: "test3" } ],
        json: true
      });

      if (isReplication2) {
        assertIsReplication2FollowerResponse(response.json);
      } else {
        // verify that response is empty
        assertEqual({}, response.json);
      }
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
      let url = getUrlById(follower);
      IM.debugSetFailAt("synchronousReplication::neverRefuseOnFollower", instanceRole.dbServer, endpoint);

      // send a single document replication remove request
      let response = request({
        method: "delete",
        url: url + "/_api/document/" + shard + "/test1?isSynchronousReplication=" + encodeURIComponent(leader),
        body: {},
        json: true
      });

      if (isReplication2) {
        assertIsReplication2FollowerResponse(response.json);
      } else {
        // verify that response is empty
        assertEqual({}, response.json);
      }
      
      // send multi document replication remove request
      response = request({
        method: "delete",
        url: url + "/_api/document/" + shard + "?isSynchronousReplication=" + encodeURIComponent(leader),
        body: [ { _key: "test2" }, { _key: "test3" } ],
        json: true
      });

      if (isReplication2) {
        assertIsReplication2FollowerResponse(response.json);
      } else {
        // verify that response is empty
        assertEqual({}, response.json);
      }
    },
  };
}

if (IM.debugCanUseFailAt()) {
  jsunity.run(followerResponsesSuite);
}
return jsunity.done();
