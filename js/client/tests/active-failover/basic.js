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

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const internal = require('internal');
const request = require("@arangodb/request");
const pu = require('@arangodb/process-utils')

const arango = internal.arango;
const wait = internal.wait;
const db = internal.db;

const suspendExternal = require('internal').suspendExternal;
const continueExternal = require("internal").continueExternal;


function getUrl(endpoint) {
  return endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
}

function baseUrl() {
  return getUrl(arango.getEndpoint())
};

// getEndponts works with any server
function getEndpoints() {
  //let jwt = crypto.jwtEncode(options['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
  var res = request.get({
    url: baseUrl() + "/_api/cluster/endpoints",
    /*auth: {
      bearer: jwt,
    }*/
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode') && res.statusCode == 200);
  assertTrue(res.hasOwnProperty('json'));
  assertTrue(res.json.hasOwnProperty('endpoints'));
  assertTrue(res.json.endpoints instanceof Array);
  assertTrue(res.json.endpoints.length > 0);
  return res.json.endpoints.map( e => e.endpoint);
}
/*
try {
  let globals = JSON.parse(process.env.ARANGOSH_GLOBALS);
  Object.keys(globals).forEach(g => {
    global[g] = globals[g];
  });
} catch (e) {
}*/

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
/*
function serverSetup() {
  let directory = require('./js/client/assets/queuetest/dirname.js');
  foxxManager.install(directory, '/queuetest');
  db._create('foxxqueuetest', {numberOfShards: 1, replicationFactor: 1});
  db.foxxqueuetest.insert({'_key': 'test', 'date': null, 'server': null});
  
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
`;
  executeOnServer(serverCode);
  foxxManager.uninstall('/queuetest');
  db._drop('foxxqueuetest');
}*/

function ActiveFailoverSuite() {
  let leader = "";
  let collection;
  return {
    setUp: function() {
      let servers = getEndpoints();
      assertTrue(servers.length > 1);
      leader = servers[0];
      collection = db._create("UnitTestActiveFailover");

      /*serverSetup();
      wait(2.1);*/
    },

    tearDown : function () {
      serverTeardown();
    },

    testQueueWorks: function() {
      let document = db._collection('foxxqueuetest').document('test');
      assertNotNull(document.server);
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
      assertTrue(continueExternal(instance.pid));
      // mop: currently supervision would run every 5s
      if (!ok) {
        throw new Error('Supervision should have moved the foxxqueues and foxxqueues should have been started to run on a new coordinator');
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ActiveFailoverSuite);

return jsunity.done();
