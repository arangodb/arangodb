/*jshint strict: false, sub: true */
/*global print, arango, assertTrue, assertNotNull, assertNotUndefined */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2014 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

const arangodb = require('@arangodb');
const wait = require('internal').wait;
const db = arangodb.db;
const fs = require('fs');
const request = require("@arangodb/request");
const foxxManager = require('@arangodb/foxx/manager');
const expect = require('chai').expect;
const InstanceManager = require('@arangodb/testing/InstanceManager.js');
const endpointToURL = require('@arangodb/common.js').endpointToURL;

const suspendExternal = require('internal').suspendExternal;
const continueExternal = require("internal").continueExternal;
const download = require('internal').download;

let executeOnServer = function(endpoint, code) {
  let httpOptions = {};
  httpOptions.method = 'POST';
  httpOptions.timeout = 3600;

  httpOptions.returnBodyOnError = true;
  const reply = download(endpointToURL(endpoint) + '/_admin/execute?returnAsJSON=true',
      code,
      httpOptions);

  if (!reply.error && reply.code === 200) {
    return JSON.parse(reply.body);
  } else {
    throw new Error('Could not send to server ' + JSON.stringify(reply));
  }
};

function serverSetup(endpoint) {
  let directory = require('js/client/assets/queuetest/dirname.js');
  foxxManager.install(directory, '/queuetest');
  db._create('foxxqueuetest', {numberOfShards: 1, replicationFactor: 1});
  db.foxxqueuetest.insert({'_key': 'test', 'date': null, 'server': null});
  
  const serverCode = `
const queues = require('@arangodb/foxx/queues');

let queue = queues.create('q');
queue.push({mount: '/queuetest', name: 'queuetest', 'repeatTimes': -1, 'repeatDelay': 1000}, {});
`;
  executeOnServer(endpoint, serverCode);
}

function serverTeardown(endpoint) {
  const serverCode = `
const queues = require('@arangodb/foxx/queues');
`;
  executeOnServer(endpoint, serverCode);
  foxxManager.uninstall('/queuetest');
  db._drop('foxxqueuetest');
}

describe('FoxxmasterSuite', function() {
  let instanceManager;
  before(function() {
    instanceManager = new InstanceManager('foxxmaster');
    let endpoint = instanceManager.startCluster(1, 2, 2);
    arango.reconnect(endpoint, '_system');
  })
  
  after(function() {
    instanceManager.cleanup();
  })

  beforeEach(function() {
    serverSetup(instanceManager.getEndpoint());
    wait(2.1);
  })

  afterEach(function() {
    serverTeardown(instanceManager.getEndpoint());
  })
    
  it('should report the currently active coordinator', function() {
    let document = db._collection('foxxqueuetest').document('test');
    expect(document.server).to.not.be.null;
  })

  it('should fail over to a different coordinator', function() {
    let document = db._collection('foxxqueuetest').document('test');
    let server = document.server;

    expect(document.server).to.not.be.null;

    let instance = instanceManager.coordinators().filter(arangod => {
      let url = arangod.endpoint.replace(/tcp/, 'http') + '/_admin/server/id';
      let res = request({method: 'GET', url: url});
      let parsed = JSON.parse(res.body);
      return parsed.id === server;
    })[0];

    expect(instance).to.not.be.undefined;
    instanceManager.kill(instance);
    let newEndpoint = instanceManager.coordinators().filter(arangod => {
      return arangod.role === 'coordinator' && arangod.pid !== instance.pid;
    })[0];
    arango.reconnect(newEndpoint.endpoint, db._name(), 'root', '');
    let waitInterval = 0.1;
    let waited = 0;
    let ok = false;
    while (waited <= 20) {
      document = db._collection('foxxqueuetest').document('test');
      let newServer = document.server;
      if (server !== newServer) {
        ok = true;
        break;
      }
      wait(waitInterval);
      waited += waitInterval;
    }
    // mop: currently supervision would run every 5s
    if (!ok) {
      throw new Error('Supervision should have moved the foxxqueues and foxxqueues should have been started to run on a new coordinator');
    }
  })
});
