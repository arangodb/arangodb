/*jshint strict: false, sub: true */
/*global arango, assertTrue, assertNotNull, assertNotUndefined, assertNotEqual */
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

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const wait = require('internal').wait;
const db = arangodb.db;
const internal = require('internal');
const request = require('@arangodb/request');
const foxxManager = require('@arangodb/foxx/manager');

const suspendExternal = internal.suspendExternal;
const continueExternal = internal.continueExternal;
const download = internal.download;
const pathForTesting = require('internal').pathForTesting;

const instanceInfo = JSON.parse(internal.env.INSTANCEINFO);

try {
  let globals = JSON.parse(process.env.ARANGOSH_GLOBALS);
  Object.keys(globals).forEach(g => {
    global[g] = globals[g];
  });
} catch (e) {
}

let executeOnServer = function(code) {
  let httpOptions = {};
  httpOptions.method = 'POST';
  httpOptions.timeout = 3600;

  httpOptions.returnBodyOnError = true;
  const reply = download(instanceInfo.url + '/_admin/execute?returnAsJSON=true',
      code,
      httpOptions);

  if (!reply.error && reply.code === 200) {
    return JSON.parse(reply.body);
  } else {
    throw new Error('Could not send to server ' + JSON.stringify(reply));
  }
};

function serverSetup() {
  let directory = require('./' + pathForTesting('client/assets/queuetest/dirname.js'));
  db._create('foxxqueuetest', {numberOfShards: 1, replicationFactor: 1});
  db.foxxqueuetest.insert({'_key': 'test', 'date': null, 'server': null});
  foxxManager.install(directory, '/queuetest');
  
  const serverCode = `
    const queues = require('@arangodb/foxx/queues');
    let queue = queues.create('q');
    queue.push({mount: '/queuetest', name: 'queuetest', 'repeatTimes': -1, 'repeatDelay': 1000}, {});
    `;

  executeOnServer(serverCode);
}

function serverTeardown() {
  const serverCode = `
    const queues = require('@arangodb/foxx/queues');
    queues.delete('q');
    `;

  executeOnServer(serverCode);
  foxxManager.uninstall('/queuetest');
  db._drop('foxxqueuetest');
}

function responseIsSuccess(res) {
  return res !== undefined && res.hasOwnProperty('status')
    && 200 <= res.status && res.status < 300;
}

function sendCanFailAtTo(endpoint) {
  const url = endpoint.replace(/^tcp:/, 'http:')
    + "/_admin/debug/failat";

  const res = request({method: 'GET', url: url});

  return responseIsSuccess(res);
}

function sendSetFailAtTo(endpoint, failurePoint) {
  const url = endpoint.replace(/^tcp:/, 'http:')
    + "/_admin/debug/failat/"
    + encodeURIComponent(failurePoint);

  const res = request({method: 'PUT', url: url});

  return responseIsSuccess(res);
}

function sendRemoveFailAtTo(endpoint, failurePoint) {
  const url = endpoint.replace(/^tcp:/, 'http:')
    + "/_admin/debug/failat/"
    + encodeURIComponent(failurePoint);

  const res = request({method: 'DELETE', url: url});

  return responseIsSuccess(res);
}

function FoxxmasterSuite() {
  return {
    setUpAll: function() {
      serverSetup();

      let document = db._collection('foxxqueuetest').document('test');
      let count = 0;

      // Even a minute could be not enough to start a Foxx service in a queue
      while (document.server == null && count++ < 12) {
        wait(10);
        document = db._collection('foxxqueuetest').document('test');
      }
    },

    tearDownAll : function () {
      serverTeardown();
    },

    testQueueWorks: function() {
      let document = db._collection('foxxqueuetest').document('test');
      assertNotNull(document.server);
      wait(2);
      assertNotEqual(document.date, db._collection('foxxqueuetest').document('test').date);
    },

    testQueueFailover: function() {
      let document = db._collection('foxxqueuetest').document('test');
      let server = document.server;
      assertNotNull(server);

      let instance = instanceInfo.arangods.filter(arangod => {
        if (arangod.role === 'agent') {
          return false;
        }
        let url = arangod.endpoint.replace(/tcp/, 'http') + '/_admin/server/id';
        let res = request({method: 'GET', url: url});
        let parsed = JSON.parse(res.body);
        if (parsed.id === server) {
          assertTrue(suspendExternal(arangod.pid));
        }
        return parsed.id === server;
      })[0];

      assertNotUndefined(instance);
      assertTrue(suspendExternal(instance.pid));

      let newEndpoint = instanceInfo.arangods.filter(arangod => {
        return arangod.role === 'coordinator' && arangod.pid !== instance.pid;
      })[0];
      arango.reconnect(newEndpoint.endpoint, db._name(), 'root', '');
      let waitInterval = 1;
      let waited = 0;
      let ok = false;
      while (waited <= 30) {
        document = db._collection('foxxqueuetest').document('test');
        let newServer = document.server;
        if (server !== newServer) {
          ok = true;
          break;
        }
        wait(waitInterval);
        waited += waitInterval;
      }
      assertTrue(continueExternal(instance.pid));
      // mop: currently supervision would run every 5s
      // vadim: but that is not guaranteed that a Foxx service will be srarted right after that, so the timeout of expected 'newServer' was raised to 30s
      if (!ok) {
        throw new Error('Supervision should have moved the Foxx queues and Foxx queues should have been started to run on a new coordinator');
      }
    },

    testQueueFailoverDuringJob: function() {
      let document = db._collection('foxxqueuetest').document('test');
      let server = document.server;
      assertNotNull(server);

      let instance = instanceInfo.arangods.filter(arangod => {
        if (arangod.role === 'agent') {
          return false;
        }
        let url = arangod.endpoint.replace(/tcp/, 'http') + '/_admin/server/id';
        let res = request({method: 'GET', url: url});
        let parsed = JSON.parse(res.body);
        return parsed.id === server;
      })[0];

      assertNotUndefined(instance);

      if (!sendCanFailAtTo(instance.endpoint)) {
        console.info("Failure tests disabled, skipping...");
        return;
      }

      // The process should now suspend while the next job is in progress
      assertTrue(
        sendSetFailAtTo(instance.endpoint, "foxxmaster::queuetest")
      );

      let newEndpoint = instanceInfo.arangods.filter(arangod => {
        return arangod.role === 'coordinator' && arangod.pid !== instance.pid;
      })[0];
      arango.reconnect(newEndpoint.endpoint, db._name(), 'root', '');
      let waitInterval = 1;
      let waited = 0;
      let ok = false;
      while (waited <= 30) {
        document = db._collection('foxxqueuetest').document('test');
        let newServer = document.server;
        if (server !== newServer) {
          ok = true;
          break;
        }
        wait(waitInterval);
        waited += waitInterval;
      }
      // Continue the process and remove the failure point
      assertTrue(continueExternal(instance.pid));
      assertTrue(
        sendRemoveFailAtTo(instance.endpoint, "foxxmaster::queuetest")
      );
      // mop: currently supervision would run every 5s
      // vadim: but that is not guaranteed that a Foxx service will be srarted right after that, so the timeout of expected 'newServer' was raised to 30s
      if (!ok) {
        throw new Error('Supervision should have moved the Foxx queues and Foxx queues should have been started to run on a new coordinator');
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(FoxxmasterSuite);

return jsunity.done();
