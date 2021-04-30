/*jshint strict: true */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const console = require('console');
const request = require('@arangodb/request');
const _ = require('lodash');
const {checkRequestResult} = require('@arangodb/arangosh');
const {assertEqual} = jsunity.jsUnity.assertions;

const getUrl = endpoint => endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');

const replicationApi = {
  createLog: (id, server) => {
    const res = request.post({
      url: getUrl(server.endpoint) + `/_api/log`,
      body: JSON.stringify({id}),
    });
    checkRequestResult(res);
  },

  becomeLeader: (id, server, {term, writeConcern, follower}) => {
    const res = request.post({
      url: getUrl(server.endpoint) + `/_api/log/${id}/becomeLeader`,
      body: JSON.stringify({term, writeConcern, follower: follower.map(server => server.id)}),
    });
    checkRequestResult(res);
  },

  becomeFollower: (id, server, {term, leader}) => {
    const res = request.post({
      url: getUrl(server.endpoint) + `/_api/log/${id}/becomeFollower`,
      body: JSON.stringify({term, leader: leader.id}),
    });
    checkRequestResult(res);
  },

  insert: (id, server, data) => {
    const res = request.post({
      url: getUrl(server.endpoint) + `/_api/log/${id}/insert`,
      body: JSON.stringify(data),
    });
    checkRequestResult(res);
    const result = res.json.result;
    assertEqual(["index", "term", "quorum"].sort(), Object.keys(result).sort());
    return result;
  },

  readEntry: (id, server, entry) => {
    const res = request.get({
      url: getUrl(server.endpoint) + `/_api/log/${id}/readEntry/${entry}`,
    });
    checkRequestResult(res);
    const result = res.json.result;
    assertEqual(["index", "payload", "term"].sort(), Object.keys(result).sort());
    return result;
  },

};

function testSuite() {
  const instanceInfo = global.instanceInfo;
  let serverA;
  let serverB;
  let serverC;
  const servers = [];

  return {
    setUpAll: function () {
      const isDBServer = (d) => (_.toLower(d.role) === 'dbserver');
      servers.push(...
        [serverA, serverB, serverC] = instanceInfo.arangods.filter(isDBServer)
      );
    },

    testReplicationMinimalInsert: function () {
      const id = 12;
      const term = 1;
      const writeConcern = 2;
      servers.forEach(server => replicationApi.createLog(id, server));
      replicationApi.becomeLeader(id, serverA, {term, writeConcern, follower: [serverB, serverC]});
      replicationApi.becomeFollower(id, serverB, {term, leader: serverA});
      replicationApi.becomeFollower(id, serverC, {term, leader: serverA});
      const entry = {foo: "bar"};
      const insertRes = replicationApi.insert(id, serverA, entry);
      const index = insertRes.index;
      assertEqual(1, index);
      const readRes = replicationApi.readEntry(id, serverA, index);
      assertEqual(entry, readRes.payload);
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();
