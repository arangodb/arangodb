/*jshint strict: false, sub: true */
/*global print, assertTrue, assertEqual */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
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

const jwtSecret = 'haxxmann';
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

function connectToServer(leader) {
  arango.reconnect(leader, "_system", "root", "");
  db._flushCache();
};

// getEndponts works with any server
function getClusterEndpoints() {
  //let jwt = crypto.jwtEncode(options['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
  var res = request.get({
    url: baseUrl() + "/_api/cluster/endpoints",
    auth: {
      bearer: jwtRoot,
    }
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode'), JSON.stringify(res));
  assertEqual(res.statusCode, 200, JSON.stringify(res));
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
  assertTrue(res.hasOwnProperty('statusCode'));
  assertEqual(res.statusCode, 200);
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
  assertTrue(res.hasOwnProperty('statusCode'));
  assertEqual(res.statusCode, 200, JSON.stringify(res));
  assertTrue(res.hasOwnProperty('json'));
  return arangosh.checkRequestResult(res.json);
}

// check the servers are in sync with the leader
function checkInSync(leader, servers, ignore) {
  print("Checking in-sync state with lead: ", leader);
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
      print("All followers are in sync with: ", leader);
      return true;
    }
    wait(1.0);
  }
  print("Timeout waiting for followers of: ", leader);
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
  assertTrue(res.hasOwnProperty('statusCode'));
  assertEqual(res.statusCode, 200, JSON.stringify(res));
  return res.json.count;
}

function readAgencyValue(path) {
  let agents = instanceinfo.arangods.filter(arangod => arangod.role === "agent");
  assertTrue(agents.length > 0, "No agents present");
  print("Querying agency... (", path, ")");
  var res = request.post({
    url: agents[0].url + "/_api/agency/read",
    auth: {
      bearer: jwtSuperuser,
    },
    body: JSON.stringify([[path]])
  });
  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode'), JSON.stringify(res));
  assertEqual(res.statusCode, 200, JSON.stringify(res));
  assertTrue(res.hasOwnProperty('json'));
  //print("Agency response ", res.json);
  return arangosh.checkRequestResult(res.json);
}

// resolve leader from agency
function leaderInAgency() {
  let i = 10;
  do {
    let res = readAgencyValue("/arango/Plan/AsyncReplication/Leader");
    let uuid = res[0].arango.Plan.AsyncReplication.Leader;
    if (uuid && uuid.length > 0) {
      res = readAgencyValue("/arango/Supervision/Health");
      return res[0].arango.Supervision.Health[uuid].Endpoint;
    }
    internal.wait(1.0);
  } while (i-- > 0);
  throw "Unable to resole leader from agency";
}

function checkForFailover(leader) {
  print("Waiting for failover of ", leader);

  let oldLeaderUUID = "";
  let i = 5; // 5 * 5s == 25s
  do {
    let res = readAgencyValue("/arango/Supervision/Health");
    let srvHealth = res[0].arango.Supervision.Health;
    Object.keys(srvHealth).forEach(key => {
      let srv = srvHealth[key];
      if (srv['Endpoint'] === leader && srv.Status === 'FAILED') {
        print("Server ", key, "( ", leader, " ) is marked FAILED");
        oldLeaderUUID = key;
      }
    });
    if (oldLeaderUUID !== "") {
      break;
    }
    internal.wait(5.0);
  } while (i-- > 0);

  // now wait for new leader to appear
  let nextLeaderUUID = "";
  do {
    let res = readAgencyValue("/arango/Plan/AsyncReplication/Leader");
    nextLeaderUUID = res[0].arango.Plan.AsyncReplication.Leader;
    if (nextLeaderUUID !== oldLeaderUUID) {
      res = readAgencyValue("/arango/Supervision/Health");
      return res[0].arango.Supervision.Health[nextLeaderUUID].Endpoint;
    }
    internal.wait(5.0);
  } while (i-- > 0);
  print("Timing out, current leader value: ", nextLeaderUUID);
  throw "No failover occured";
}

// Testsuite that quickly checks some of the basic premises of
// the active failover functionality. It is designed as a quicker
// variant of the node resilience tests (for active failover).
// Things like Foxx resilience are not tested
function ActiveFailoverSuite() {
  let servers = getClusterEndpoints();
  assertTrue(servers.length >= 4, "This test expects four single instances");
  let firstLeader = servers[0];
  let suspended = [];
  let currentLead = leaderInAgency();

  return {
    setUp: function () {
      let col = db._create(cname);
      assertTrue(checkInSync(currentLead, servers));
      for (let i = 0; i < 10000; i++) {
        col.save({ attr: i});
      }
    },

    tearDown: function () {
      //db._collection(cname).drop();
      //serverTeardown();

      suspended.forEach(arangod => {
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });

      currentLead = leaderInAgency();
      print("connecting shell to leader ", currentLead);
      connectToServer(currentLead);
      if (db._collection(cname)) {
        db._drop(cname);
      }

      assertTrue(checkInSync(currentLead, servers));

      let endpoints = getClusterEndpoints();
      assertEqual(endpoints.length, servers.length);
      assertEqual(endpoints[0], currentLead);
    },

    // Basic test if followers get in sync
    testFollowerInSync: function () {
      assertEqual(servers[0], currentLead);

      let col = db._collection(cname);
      assertEqual(col.count(), 10000);
      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);
    },

    // Simple failover case: Leader is suspended, slave needs to 
    // take over within a reasonable amount of time
    testFailover: function () {

      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);

      suspended = instanceinfo.arangods.filter(arangod => arangod.endpoint === currentLead);
      suspended.forEach(arangod => {
        print("Suspending Leader: ", arangod.endpoint);
        assertTrue(suspendExternal(arangod.pid));
      });

      let oldLead = currentLead;
      // await failover and check that follower get in sync
      currentLead = checkForFailover(currentLead);
      assertTrue(currentLead !== oldLead);
      print("Failover to new leader : ", currentLead);

      internal.wait(5); // settle down, heartbeat interval is 1s
      assertEqual(checkData(currentLead), 10000);
      print("New leader has correct data");

      // check the remaining followers get in sync
      assertTrue(checkInSync(currentLead, servers, oldLead));

      // restart the old leader
      suspended.forEach(arangod => {
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });
      suspended = [];

      assertTrue(checkInSync(currentLead, servers));
    },

    // More complex case: We want to get the most up to date follower
    // Insert a number of documents, suspend n-1 followers for a few seconds.
    // We then suspend the leader and expect a specific follower to take over
    testFollowerSelection: function () {

      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);

      // we assume the second leader is still the leader
      let endpoints = getClusterEndpoints();
      assertEqual(endpoints.length, servers.length);
      assertEqual(endpoints[0], currentLead);

      print("Starting data creation task on ", currentLead, " (expect it to fail later)");
      connectToServer(currentLead);
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

      // pick a random follower
      let nextLead = endpoints[2]; // could be any one of them
      // suspend remaining followers
      print("Suspending followers, except one");
      suspended = instanceinfo.arangods.filter(arangod => arangod.role !== 'agent' &&
        arangod.endpoint !== currentLead &&
        arangod.endpoint !== nextLead);
      suspended.forEach(arangod => {
        print("Suspending: ", arangod.endpoint);
        assertTrue(suspendExternal(arangod.pid));
      });

      // check our leader stays intact, while remaining followers fail
      let i = 20;
      //let expected = servers.length - suspended.length; // should be 2
      do {
        endpoints = getClusterEndpoints();
        assertEqual(endpoints[0], currentLead, "Unwanted leadership failover");
        internal.wait(1.0); // Health status may take some time to change
      } while (endpoints.length !== 2 && i-- > 0);
      assertTrue(i > 0, "timed-out waiting for followers to fail");
      assertEqual(endpoints.length, 2);
      assertEqual(endpoints[1], nextLead); // this server must become new leader

      // resume followers
      print("Resuming followers");
      suspended.forEach(arangod => {
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });
      suspended = [];

      let upper = checkData(currentLead);
      print("Leader inserted ", upper, " documents so far");
      print("Suspending leader ", currentLead);
      instanceinfo.arangods.forEach(arangod => {
        if (arangod.endpoint === currentLead) {
          print("Suspending: ", arangod.endpoint);
          suspended.push(arangod);
          assertTrue(suspendExternal(arangod.pid));
        }
      });

      // await failover and check that follower get in sync
      let oldLead = currentLead;
      currentLead = checkForFailover(currentLead);
      assertEqual(currentLead, nextLead, "Did not fail to best in-sync follower");

      internal.wait(5); // settle down, heartbeat interval is 1s
      let cc = checkData(currentLead);
      // we expect to find documents within an acceptable range
      assertTrue(10000 <= cc && cc <= upper + 500, "Leader has too little or too many documents");
      print("Number of documents is in acceptable range");

      assertTrue(checkInSync(currentLead, servers, oldLead));
      print("Remaining followers are in sync");

      // Resuming stopped second leader
      print("Resuming server that still thinks it is leader (ArangoError 1004 is expected)");
      suspended.forEach(arangod => {
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });
      suspended = [];

      assertTrue(checkInSync(currentLead, servers));
    },

    // try to failback to the original leader
    testFailback: function() {
      if (currentLead === firstLeader) {
        return; // nevermind then
      }

      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);

      print("Suspending followers, except original leader");
      suspended = instanceinfo.arangods.filter(arangod => arangod.role !== 'agent' &&
        arangod.endpoint !== firstLeader);
      suspended.forEach(arangod => {
        print("Suspending: ", arangod.endpoint);
        assertTrue(suspendExternal(arangod.pid));
      });

      // await failover and check that follower get in sync
      currentLead = checkForFailover(currentLead);
      assertEqual(currentLead, firstLeader, "Did not fail to original leader");

      suspended.forEach(arangod => {
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });
      suspended = [];

      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);
    }

    // Try to cleanup everything that was created
    /*testCleanup: function () {

      let res = readAgencyValue("/arango/Plan/AsyncReplication/Leader");
      assertTrue(res !== null);
      let uuid = res[0].arango.Plan.AsyncReplication.Leader;
      res = readAgencyValue("/arango/Supervision/Health");
      let lead = res[0].arango.Supervision.Health[uuid].Endpoint;

      connectToServer(lead);
      db._drop(cname);

      assertTrue(checkInSync(lead, servers));
    }*/

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ActiveFailoverSuite);

return jsunity.done();
