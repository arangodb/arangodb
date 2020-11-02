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

  let re = new RegExp("^" + name ); 

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
   
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(transactionIntermediateCommitsSingleSuite);
}
return jsunity.done();
