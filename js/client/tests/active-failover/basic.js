/*jshint strict: false, sub: true */
/*global print, assertTrue, assertNotNull, assertNotUndefined */
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
const internal = require('internal');
const fs = require('fs');
const arangosh = require('@arangodb/arangosh');
const request = require("@arangodb/request");
const tasks = require("@arangodb/tasks");

const arango = internal.arango;
const compareTicks = require("@arangodb/replication").compareTicks;
const wait = internal.wait;
const db = internal.db;

const suspendExternal = internal.suspendExternal;
const continueExternal = internal.continueExternal;

if (!internal.env.hasOwnProperty('INSTANCEINFO')) {
  throw new Error('env.INSTANCEINFO was not set by caller!');
}
const instanceinfo = JSON.parse(internal.env.INSTANCEINFO);

const cname = "UnitTestActiveFailover";

/*try {
  let globals = JSON.parse(process.env.ARANGOSH_GLOBALS);
  Object.keys(globals).forEach(g => {
    global[g] = globals[g];
  });
} catch (e) {
}*/

function getUrl(endpoint) {
  return endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
}

function baseUrl() {
  return getUrl(arango.getEndpoint());
};

// getEndponts works with any server
function getEndpoints() {
  //let jwt = crypto.jwtEncode(options['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
  var res = request.get({
    url: baseUrl() + "/_api/cluster/endpoints"
    /*auth: {
      bearer: jwt,
    }*/
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode') && res.statusCode === 200);
  assertTrue(res.hasOwnProperty('json'));
  assertTrue(res.json.hasOwnProperty('endpoints'));
  assertTrue(res.json.endpoints instanceof Array);
  assertTrue(res.json.endpoints.length > 0);
  return res.json.endpoints.map( e => e.endpoint);
}

function getLoggerState(endpoint) {
  var res = request.get({
    url: getUrl(endpoint) + "/_db/_system/_api/replication/logger-state"
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode') && res.statusCode === 200);
  assertTrue(res.hasOwnProperty('json'));
  return arangosh.checkRequestResult(res.json);
}

function getApplierState(endpoint) {
  var res = request.get({
    url: getUrl(endpoint) + "/_db/_system/_api/replication/applier-state?global=true"
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode') && res.statusCode === 200);
  assertTrue(res.hasOwnProperty('json'));
  return arangosh.checkRequestResult(res.json);
}

// self contained function for use as task
function createData () {
  const db2 = require('@arangodb').db;
  let col = db2._collection("UnitTestActiveFailover");
  let cc = col.count();
  for (let i = 0; i < 10000; i++) {
    col.save({attr: i + cc});
  }
}

// check the servers are in sync with the leader
function checkInSync(leader, servers, ignore) {
  print ("Checking in-sync state with: ", leader);
  let check = (endpoint) => {
    if (endpoint === leader || endpoint === ignore) {
      return true;
    }
    let applier = getApplierState(endpoint);
    return applier.state.running && applier.endpoint === leader && 
          compareTicks(applier.state.lastProcessedContinuousTick, leaderTick);
  };
    
  const leaderTick = getLoggerState(leader).state.lastLogTick;
  let loop = 100;
  while(loop-- > 0) {
    if (servers.every(check)) {
      break;
    }
    wait(1.0);
  }
  return loop > 0;
}

function checkData(server, expected) {
  print ("Checking data of ", server);
  let res = request.get({
    url: getUrl(server) + "/_api/collection/" + cname + "/count"
  });
  print ("Done collection count ", expected);

  assertTrue(res instanceof request.Response);
  //assertTrue(res.hasOwnProperty('statusCode'));
  assertTrue(res.statusCode === 200);
  //assertTrue(res.hasOwnProperty('json'));
  print ("sdsdssddata 2", res.json);
  return res.json.count === expected;
}

function readAgencyValue(path) {
  print("Querying agency: ", path);

  let agents = instanceinfo.arangods.filter(arangod => arangod.role === "agent");
  assertTrue(agents.length > 0);
  var res = request.post({
    url: agents[0].url + "/_api/agency/read",
    body: JSON.stringify([[path]])
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode') && res.statusCode === 200);
  assertTrue(res.hasOwnProperty('json'));
  //print("Agency response ", res.json);
  return arangosh.checkRequestResult(res.json);
}

function checkForFailover(leader) {
  print("Checking for failover");

  let oldLeader = "";
  let i = 100;
  do {
    let res = readAgencyValue("/arango/Supervision/Health");
    let srvHealth = res[0].arango.Supervision.Health;
    //print(srvHealth);
    Object.keys(srvHealth).forEach(key => {
      let srv = srvHealth[key];
      if (srv['Endpoint'] === leader && srv.Status === 'FAILED') {
        oldLeader = key;
      }
    });
    if (oldLeader !== "") {
      break;
    }
    internal.wait(1.0);
  } while (i-- > 0);

  let nextLeader = "";
  do {
    let res = readAgencyValue("/arango/Plan/AsyncReplication/Leader");
    nextLeader = res[0].arango.Plan.AsyncReplication.Leader;
    if (nextLeader !== oldLeader) {
      res = readAgencyValue("/arango/Supervision/Health");
      return res[0].arango.Supervision.Health[nextLeader].Endpoint;
    }
    internal.wait(1.0);
  } while (i-- > 0);
  throw "No failover occured";
}


function checkForNewLeader(leader) {
  let i = 1000;
  do {
    let res = readAgencyValue("/arango/Supervision/Health");
    let srvHealth = res[0].arango.Supervision.Health;
    assertTrue(srvHealth instanceof Array);
    let srv = srvHealth.filter( server => server.Endpoint === leader);
    if (srv.Status === 'FAILED') {
      break;
    }
  } while (i-- > 0);
}

function ActiveFailoverSuite() {

  let servers = getEndpoints();
  assertTrue(servers.length > 2);
  let leader = servers[0];
  let collection = db._create(cname);
 

  return {
    setUp: function() {

      /*var task = tasks.register({ 
        name: "UnitTests1", 
        command: createData
      });*/

      /*serverSetup();
      wait(2.1);*/
    },

    tearDown : function () {
      //db._collection(cname).drop();
      //serverTeardown();
    },

    testFollowerInSync: function() {
      createData();
      assertTrue(checkInSync(leader, servers));
      checkData(leader, 10000);
    },

    testSuspendLeader: function() {
      //createData();
      //assertTrue(checkInSync(leader, servers));
      checkData(leader, 10000);

      let suspended = instanceinfo.arangods.filter(arangod => arangod.endpoint === leader);
      suspended.forEach(arangod => {
        assertTrue(suspendExternal(arangod.pid));
      });

      // await failover and check that follower get in sync
      let nextLeader = checkForFailover(leader);
      assertTrue(nextLeader !== leader);
      print ("Next Leader: ", nextLeader);
      checkData(nextLeader, 10000);
      print ("ddddddd");

      assertTrue(checkInSync(nextLeader, servers, leader));
      print ("bbbbbbbb");


      // restart the old leader
      leader = nextLeader;
      suspended.forEach(arangod => assertTrue(continueExternal(arangod.pid)));
      print ("kkkkkkkkkkkk");

      assertTrue(checkInSync(leader, servers));

      /*let instance = instanceInfo.arangods.filter(arangod => {
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
      })[0];*/


      //assertTrue(suspendExternal(arangod.pid));
    }

    
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ActiveFailoverSuite);

return jsunity.done();
