/*jshint strict: false, sub: true */
/*global print, assertTrue, assertEqual, assertNotEqual */
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
const console = require('console');
const expect = require('chai').expect;

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
}

function connectToServer(leader) {
  arango.reconnect(leader, "_system", "root", "");
  db._flushCache();
}

// getEndponts works with any server
function getClusterEndpoints() {
  //let jwt = crypto.jwtEncode(options['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
  var res = request.get({
    url: baseUrl() + "/_api/cluster/endpoints",
    auth: {
      bearer: jwtRoot,
    },
    timeout: 300
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
    },
    timeout: 300
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
    },
    timeout: 300
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

  const leaderTick = getLoggerState(leader).state.lastLogTick;

  let check = (endpoint) => {
    if (endpoint === leader || endpoint === ignore) {
      return true;
    }

    let applier = getApplierState(endpoint);

    print("Checking endpoint ", endpoint, " applier.state.running=", applier.state.running, " applier.endpoint=", applier.endpoint);
    return applier.state.running && applier.endpoint === leader &&
      (compareTicks(applier.state.lastAppliedContinuousTick, leaderTick) >= 0 ||
        compareTicks(applier.state.lastProcessedContinuousTick, leaderTick) >= 0);
  };

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

function checkData(server, allowDirty = false) {
  print("Checking data of ", server);
  // Async agency cache should have received it's data
  let trickleDown = request.get({ 
    url: getUrl(server) + "/_api/cluster/agency-cache", auth: { bearer: jwtRoot },
    headers: { "X-Arango-Allow-Dirty-Read": allowDirty }, timeout: 3 
  });
  require('internal').wait(2.0);
  let res = request.get({
    url: getUrl(server) + "/_api/collection/" + cname + "/count",
    auth: {
      bearer: jwtRoot,
    },
    headers: {
      "X-Arango-Allow-Dirty-Read": allowDirty
    },
    timeout: 300
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
    body: JSON.stringify([[path]]),
    timeout: 300
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
  let i = 24; // 24 * 5s == 120s
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

function waitUntilHealthStatusIs(isHealthy, isFailed) {
  print("Waiting for health status to be healthy: ", JSON.stringify(isHealthy), " failed: ", JSON.stringify(isFailed));
  // Wait 25 seconds, sleep 5 each run
  for (const start = Date.now(); (Date.now() - start) / 1000 < 25; internal.wait(5.0)) {
    let needToWait = false;
    let res = readAgencyValue("/arango/Supervision/Health");
    let srvHealth = res[0].arango.Supervision.Health;
    let foundFailed = 0;
    let foundHealthy = 0;
    for (const [_, srv] of Object.entries(srvHealth)) {
      if (srv.Status === 'FAILED') {
        // We have a FAILED server, that we do not expect
        if (!isFailed.indexOf(srv.Endpoint) === -1) {
          needToWait = true;
          break;
        }
        foundFailed++;
      } else {
        if (!isHealthy.indexOf(srv.Endpoint) === -1) {
          needToWait = true;
          break;
        }
        foundHealthy++;
      }
    }
    if (!needToWait && foundHealthy === isHealthy.length && foundFailed === isFailed.length) {
      return true;
    }
  }
  print("Timing out, could not reach desired state: ", JSON.stringify(isHealthy), " failed: ", JSON.stringify(isFailed));
  print("We only got: ", JSON.stringify(readAgencyValue("/arango/Supervision/Health")[0].arango.Supervision.Health));
  return false;
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
    setUpAll: function () {
      db._create(cname);
    },

    setUp: function () {
      assertTrue(checkInSync(currentLead, servers));

      let col = db._collection(cname);
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

      assertTrue(checkInSync(currentLead, servers));

      let i = 100;
      do {
        let endpoints = getClusterEndpoints();
        if (endpoints.length === servers.length && endpoints[0] === currentLead) {
          db._collection(cname).truncate({ compact: false });
          return ;
        }
        print("cluster endpoints not as expected: found =", endpoints, " expected =", servers);
        internal.wait(1); // settle down
      } while(i --> 0);

      let endpoints = getClusterEndpoints();
      print("endpoints: ", endpoints, " servers: ", servers);
      assertEqual(endpoints.length, servers.length);
      assertEqual(endpoints[0], currentLead);
      db._collection(cname).truncate({ compact: false });
    },

    tearDownAll: function () {
      if (db._collection(cname)) {
        db._drop(cname);
      }
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
      assertNotEqual(currentLead, oldLead);
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

      let upper = checkData(currentLead);
      let atLeast = 0;
      while (atLeast < upper) {
        internal.wait(1.0);
        //update atLeast
        atLeast = checkData(nextLead, true);
      }

      let healthyList = [currentLead, nextLead].concat(suspended.map(s => s.endpoint));
      // resume followers
      print("Resuming followers");
      suspended.forEach(arangod => {
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });
      suspended = [];

      // Wait until all servers report healthy again
      assertTrue(waitUntilHealthStatusIs(healthyList, []));

      print("Leader inserted ", upper, " documents so far desired follower has " , atLeast);
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
      assertNotEqual(currentLead, oldLead);

      let cc = checkData(currentLead);
      assertTrue(cc >= atLeast, "The new Leader has too few documents");
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
      /*if (checkData(currentLead) != 10000) {
        print("ERROR! DODEBUG")
        while(1){}
      }*/

      print("Suspending followers, except original leader");
      suspended = instanceinfo.arangods.filter(arangod => arangod.role !== 'agent' &&
        arangod.endpoint !== firstLeader);
      suspended.forEach(arangod => {
        print("Suspending: ", arangod.endpoint);
        assertTrue(suspendExternal(arangod.pid));
      });

      // await failover and check that follower get in sync
      let oldLead = currentLead;
      currentLead = checkForFailover(currentLead);
      assertNotEqual(currentLead, oldLead);
      assertEqual(currentLead, firstLeader, "Did not fail to original leader");

      suspended.forEach(arangod => {
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });
      suspended = [];

      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);
    },

    // Try to cleanup everything that was created
    /*testCleanup: function () {

      let res = readAgencyValue("/arango/Plan/AsyncReplication/Leader");
      assertNotEqual(res, null);
      let uuid = res[0].arango.Plan.AsyncReplication.Leader;
      res = readAgencyValue("/arango/Supervision/Health");
      let lead = res[0].arango.Supervision.Health[uuid].Endpoint;

      connectToServer(lead);
      db._drop(cname);

      assertTrue(checkInSync(lead, servers));
    }*/

    // Regression test. This endpoint was broken due to added checks in v8-cluster.cpp,
    // which allowed certain calls only in cluster mode, but not in active failover.
    testClusterHealth: function () {
      console.warn({currentLead: getUrl(currentLead)});
      const res = request.get({
        url: getUrl(currentLead) + "/_admin/cluster/health",
        auth: {
          bearer: jwtRoot,
        },
        timeout: 30
      });
      console.warn(JSON.stringify(res));
      console.warn(res.json);
      expect(res).to.be.an.instanceof(request.Response);
      // expect(res).to.be.have.property('statusCode', 200);
      expect(res).to.have.property('json');
      expect(res.json).to.include({error: false, code: 200});
      expect(res.json).to.have.property('Health');
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ActiveFailoverSuite);

return jsunity.done();
