/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertNotEqual */

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

      let droppedFollowers = {};
      servers.forEach((serverId) => {
        let endpoint = getEndpointById(serverId);
        droppedFollowers[serverId] = getMetric(endpoint, "arangodb_dropped_followers_count");
      });

      for (let i = 0; i < 50; ++i) { 
        const trx = db._createTransaction(opts);
        const tc = trx.collection(cn);
        tc.insert(docs);
        let result = trx.commit();
        assertEqual("committed", result.status);
      }

      // follower must not have been dropped
      servers.forEach((serverId) => {
        let endpoint = getEndpointById(serverId);
        assertEqual(droppedFollowers[serverId], getMetric(endpoint, "arangodb_dropped_followers_count"));
      });

      assertEqual(1000 * 50, c.count());
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

      let droppedFollowers = {};
      servers.forEach((serverId) => {
        let endpoint = getEndpointById(serverId);
        droppedFollowers[serverId] = getMetric(endpoint, "arangodb_dropped_followers_count");
      });

      for (let i = 0; i < 50; ++i) { 
        const trx = db._createTransaction(opts);
        const tc = trx.collection(cn);
        tc.insert(docs);
        let result = trx.commit();
        assertEqual("committed", result.status);
      }

      // follower must not have been dropped
      servers.forEach((serverId) => {
        let endpoint = getEndpointById(serverId);
        assertEqual(droppedFollowers[serverId], getMetric(endpoint, "arangodb_dropped_followers_count"));
      });
      
      assertEqual(1000 * 50, c.count());
    },

  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length >= 3) {
  jsunity.run(transactionDroppedFollowersSuite);
}
return jsunity.done();
