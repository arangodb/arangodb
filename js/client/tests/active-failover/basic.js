/*jshint strict: false, sub: true */
/*global print, assertTrue, assertEqual */
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
const crypto = require('@arangodb/crypto');
const request = require("@arangodb/request");
const tasks = require("@arangodb/tasks");

const arango = internal.arango;
const compareTicks = require("@arangodb/replication").compareTicks;
const wait = internal.wait;
const db = internal.db;

const suspendExternal = internal.suspendExternal;
const continueExternal = internal.continueExternal;

const jwtSecret = "haxxmann";
const jwtSuperuser = crypto.jwtEncode(jwtSecret, {
  "server_id": "test",
  "iss": "arangodb",
  "exp": Math.floor(Date.now() / 1000) + 3600
}, 'HS256');
const jwtRoot = crypto.jwtEncode(jwtSecret, {
  "preferred_username": "root",
  "iss": "arangodb",
  "exp": Math.floor(Date.now() / 1000) + 3600
}, 'HS256');

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

function connectToLeader(leader) {
  arango.reconnect(leader, "_system", "root", "");
  db._flushCache();
};

// getEndponts works with any server
function getEndpoints() {
  //let jwt = crypto.jwtEncode(options['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
  var res = request.get({
    url: baseUrl() + "/_api/cluster/endpoints",
    auth: {
      bearer: jwtRoot,
    }
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode'), JSON.stringify(res));
  assertTrue(res.statusCode === 200, JSON.stringify(res));
  assertTrue(res.hasOwnProperty('json'));
  assertTrue(res.json.hasOwnProperty('endpoints'));
  assertTrue(res.json.endpoints instanceof Array);
  assertTrue(res.json.endpoints.length > 0);
  return res.json.endpoints.map(e => e.endpoint);
}

function getLoggerState(endpoint) {
  var res = request.get({
    url: getUrl(endpoint) + "/_db/_system/_api/replication/logger-state",
    auth: {
      bearer: jwtRoot,
    }
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode') && res.statusCode === 200);
  assertTrue(res.hasOwnProperty('json'));
  return arangosh.checkRequestResult(res.json);
}

function getApplierState(endpoint) {
  var res = request.get({
    url: getUrl(endpoint) + "/_db/_system/_api/replication/applier-state?global=true",
    auth: {
      bearer: jwtRoot,
    }
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode') && res.statusCode === 200);
  assertTrue(res.hasOwnProperty('json'));
  return arangosh.checkRequestResult(res.json);
}

// check the servers are in sync with the leader
function checkInSync(leader, servers, ignore) {
  print("Checking in-sync state with: ", leader);
  let check = (endpoint) => {
    if (endpoint === leader || endpoint === ignore) {
      return true;
    }

    let applier = getApplierState(endpoint);
    return applier.state.running && applier.endpoint === leader &&
      (compareTicks(applier.state.lastAppliedContinuousTick, leaderTick) >= 0 ||
        compareTicks(applier.state.lastProcessedContinuousTick, leaderTick) >= 0);
  };

  const leaderTick = getLoggerState(leader).state.lastLogTick;

  let loop = 100;
  while (loop-- > 0) {
    if (servers.every(check)) {
      print("All followers are in sync");
      return true;
    }
    wait(1.0);
  }
  print("Timeout waiting for followers");
  return false;
}

function checkData(server) {
  print("Checking data of ", server);
  let res = request.get({
    url: getUrl(server) + "/_api/collection/" + cname + "/count",
    auth: {
      bearer: jwtRoot,
    }
  });


  assertTrue(res instanceof request.Response);
  //assertTrue(res.hasOwnProperty('statusCode'));
  assertTrue(res.statusCode === 200);
  //assertTrue(res.hasOwnProperty('json'));
  //print ("sdsdssddata 2", res.json);
  return res.json.count;
}

function readAgencyValue(path) {
  print("Querying agency... (", path, ")");

  let agents = instanceinfo.arangods.filter(arangod => arangod.role === "agent");
  assertTrue(agents.length > 0);
  var res = request.post({
    url: agents[0].url + "/_api/agency/read",
    auth: {
      bearer: jwtSuperuser,
    },
    body: JSON.stringify([[path]])
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode') && res.statusCode === 200);
  assertTrue(res.hasOwnProperty('json'));
  //print("Agency response ", res.json);
  return arangosh.checkRequestResult(res.json);
}

function checkForFailover(leader) {
  print("Waiting for failover of ", leader);

  let oldLeader = "";
  let i = 5; // 5 * 5s == 25s
  do {
    let res = readAgencyValue("/arango/Supervision/Health");
    let srvHealth = res[0].arango.Supervision.Health;
    //print(srvHealth);
    Object.keys(srvHealth).forEach(key => {
      let srv = srvHealth[key];
      if (srv['Endpoint'] === leader && srv.Status === 'FAILED') {
        print("Server ", key, "( ", leader, " ) is marked FAILED");
        oldLeader = key;
      }
    });
    if (oldLeader !== "") {
      break;
    }
    internal.wait(5.0);
  } while (i-- > 0);

  let nextLeader = "";
  do {
    let res = readAgencyValue("/arango/Plan/AsyncReplication/Leader");
    nextLeader = res[0].arango.Plan.AsyncReplication.Leader;
    if (nextLeader !== oldLeader) {
      res = readAgencyValue("/arango/Supervision/Health");
      return res[0].arango.Supervision.Health[nextLeader].Endpoint;
    }
    internal.wait(5.0);
  } while (i-- > 0);
  throw "No failover occured";
}

// Testsuite that quickly checks some of the basic premises of
// the active failover functionality. It is designed as a quicker
// variant of the node resilience tests (for active failover).
// Things like Foxx resilience are not tested
function ActiveFailoverSuite() {
  let servers = getEndpoints();
  assertTrue(servers.length >= 4, "This test expects four single instances");
  let firstLeader = servers[0];
  let secondLeader = null;
  let collection = db._create(cname);
  print();

  return {
    setUp: function () {

      /*var task = tasks.register({ 
        name: "UnitTests1", 
        command: createData
      });*/

      /*serverSetup();
      wait(2.1);*/
    },

    tearDown: function () {
      //db._collection(cname).drop();
      //serverTeardown();
    },

    testFollowerInSync: function () {
      let col = db._collection(cname);
      let cc = col.count();
      for (let i = 0; i < 10000; i++) {
        col.save({ attr: i + cc });
      }
      assertTrue(checkInSync(firstLeader, servers));
      assertEqual(checkData(firstLeader), 10000);
    },

    // Simple failover case: Leader is suspended, slave needs to 
    // take over within a reasonable amount of time
    testFailover: function () {

      let suspended = instanceinfo.arangods.filter(arangod => arangod.endpoint === firstLeader);
      suspended.forEach(arangod => {
        print("Suspending Leader: ", arangod.endpoint);
        assertTrue(suspendExternal(arangod.pid));
      });

      // await failover and check that follower get in sync
      secondLeader = checkForFailover(firstLeader);
      assertTrue(firstLeader !== secondLeader);
      print("Failover to second leader : ", secondLeader);

      internal.wait(2.5); // settle down, heartbeat interval is 1s
      assertEqual(checkData(secondLeader), 10000);
      print("Second leader has correct data");

      // check the remaining followers get in sync
      assertTrue(checkInSync(secondLeader, servers, firstLeader));

      // restart the old leader
      suspended.forEach(arangod => {
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });

      assertTrue(checkInSync(secondLeader, servers));
    },

    // Failback case: We want to get our original leader back.
    // Insert a number of documents, suspend followers for a few seconds.
    // We then suspend the leader and expect the original lead to take over
    testFailback: function () {
      // we assume the second leader is still the leader
      let endpoints = getEndpoints();
      assertTrue(endpoints.length === servers.length);
      assertTrue(endpoints[0] === secondLeader);

      print("Starting data creation task on ", secondLeader, " (expect it to fail later)");
      connectToLeader(secondLeader);
      /// this task should stop once the server becomes a slave
      var task = tasks.register({
        name: "UnitTestsFailover",
        command: `
          const db = require('@arangodb').db;
          let col = db._collection("UnitTestActiveFailover");
          let cc = col.count();
          for (let i = 0; i < 1000000; i++) {
            col.save({attr: i + cc});
          }`
      });

      internal.wait(2.5);

      // suspend remaining followers
      print("Suspending followers, except original leader");
      let suspended = instanceinfo.arangods.filter(arangod => arangod.role !== 'agent' &&
        arangod.endpoint !== firstLeader &&
        arangod.endpoint !== secondLeader);
      suspended.forEach(arangod => {
        print("Suspending: ", arangod.endpoint);
        assertTrue(suspendExternal(arangod.pid));
      });

      // check our leader stays intact, while remaining followers fail
      let i = 30;
      let expected = servers.length - suspended.length;
      do {
        endpoints = getEndpoints();
        assertEqual(endpoints[0], secondLeader, "Unwanted leadership failover");
        internal.wait(1.0); // Health status may take some time to change
      } while (endpoints.length !== expected && i-- > 0);

      // resume followers
      print("Resuming followers");
      suspended.forEach(arangod => {
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });

      let upper = checkData(secondLeader);
      print("Leader inserted ", upper, " documents so far");
      print("Suspending leader ", secondLeader);
      instanceinfo.arangods.forEach(arangod => {
        if (arangod.endpoint === secondLeader) {
          print("Suspending: ", arangod.endpoint);
          assertTrue(suspendExternal(arangod.pid));
        }
      });

      // await failover and check that follower get in sync
      let lead = checkForFailover(secondLeader);
      assertTrue(lead === firstLeader, "Did not failback to original leader");
      print("Successful failback to: ", lead);

      internal.wait(2.5); // settle down, heartbeat interval is 1s
      let cc = checkData(firstLeader);
      // we expect to find documents within an acceptable range
      assertTrue(10000 <= cc && cc <= upper + 500, "Leader has too little or too many documents");
      print("Number of documents is in acceptable range");

      assertTrue(checkInSync(lead, servers, secondLeader));
      print("Remaining followers are in sync");

      // Resuming stopped second leader
      print("Resuming server that thinks it is leader (error is expected)");
      instanceinfo.arangods.forEach(arangod => {
        if (arangod.endpoint === secondLeader) {
          print("Resuming: ", arangod.endpoint);
          assertTrue(continueExternal(arangod.pid));
        }
      });

      assertTrue(checkInSync(lead, servers));
    },

    // Try to cleanup everything that was created
    testCleanup: function () {

      let res = readAgencyValue("/arango/Plan/AsyncReplication/Leader");
      assertTrue(res !== null);
      let uuid = res[0].arango.Plan.AsyncReplication.Leader;
      res = readAgencyValue("/arango/Supervision/Health");
      let lead = res[0].arango.Supervision.Health[uuid].Endpoint;

      connectToLeader(lead);
      db._drop(cname);

      assertTrue(checkInSync(lead, servers));
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ActiveFailoverSuite);

return jsunity.done();
