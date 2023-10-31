/*jshint strict: false, sub: true */
/*global print, assertTrue, assertEqual, fail */
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
const arangosh = require('@arangodb/arangosh');
const crypto = require('@arangodb/crypto');
const request = require("@arangodb/request");
const arangodb = require("@arangodb");
const ERRORS = arangodb.errors;

const arango = internal.arango;
const compareTicks = require("@arangodb/replication").compareTicks;
const wait = internal.wait;
const db = internal.db;

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

const cname = "UnitTestActiveFailover";

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
  print(Date() + "Checking in-sync state with lead: ", leader);
  let check = (endpoint) => {
    if (endpoint === leader || endpoint === ignore) {
      return true;
    }

    let applier = getApplierState(endpoint);

    print(Date() + "Checking endpoint ", endpoint, " applier.state.running=", applier.state.running, " applier.endpoint=", applier.endpoint);
    return applier.state.running && applier.endpoint === leader &&
      (compareTicks(applier.state.lastAppliedContinuousTick, leaderTick) >= 0 ||
        compareTicks(applier.state.lastProcessedContinuousTick, leaderTick) >= 0);
  };

  const leaderTick = getLoggerState(leader).state.lastLogTick;

  let loop = 100;
  while (loop-- > 0) {
    if (servers.every(check)) {
      print(Date() + "All followers are in sync with lead: ", leader);
      return true;
    }
    wait(1.0);
  }
  print(Date() + "Timeout waiting for followers of: ", leader);
  return false;
}

function checkData(server) {
  print(Date() + "Checking data of ", server);
  let res = request.get({
    url: getUrl(server) + "/_api/collection/" + cname + "/count",
    auth: {
      bearer: jwtRoot,
    },
    timeout: 300
  });

  assertTrue(res instanceof request.Response);
  //assertTrue(res.hasOwnProperty('statusCode'));
  assertEqual(res.statusCode, 200);
  return res.json.count;
}

function readAgencyValue(path) {
  let agents = global.instanceManager.arangods.filter(arangod => arangod.instanceRole === "agent");
  assertTrue(agents.length > 0, "No agents present");
  print(Date() + "Querying agency... (", path, ")");
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
  //print(Date() + "Agency response ", res.json);
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
  print(Date() + "Waiting for failover of ", leader);

  let oldLeaderUUID = "";
  let i = 24; // 24 * 5s == 120s
  do {
    let res = readAgencyValue("/arango/Supervision/Health");
    let srvHealth = res[0].arango.Supervision.Health;
    Object.keys(srvHealth).forEach(key => {
      let srv = srvHealth[key];
      if (srv['Endpoint'] === leader && srv.Status === 'FAILED') {
        print(Date() + "Server ", key, "( ", leader, " ) is marked FAILED");
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
  print(Date() + "Timing out, current leader value: ", nextLeaderUUID);
  throw "No failover occured";
}

function setReadOnly(endpoint, ro) {
  print(Date() + "Setting read-only ", ro);

  let str = (ro === true ? "readonly" : "default");
  var res = request.put({
    url: getUrl(endpoint) + "/_db/_system/_admin/server/mode",
    auth: {
      bearer: jwtRoot,
    },
    body: {"mode" : str},
    json: true,
    timeout: 300
  });
  print(JSON.stringify(res));

  assertTrue(res instanceof request.Response);
  assertTrue(res.hasOwnProperty('statusCode'));
  assertEqual(res.statusCode, 200, JSON.stringify(res));
  assertTrue(res.hasOwnProperty('json'));
  let json = arangosh.checkRequestResult(res.json);
  assertTrue(json.hasOwnProperty('mode'));
  assertEqual(json.mode, (ro ? "readonly" : "default"));
}

// Testsuite that checks the read-only mode in the context
// of the active-failover setup
function ActiveFailoverSuite() {
  let servers;
  let firstLeader;
  let suspended = [];
  let currentLead;

  return {
    setUpAll: function () {
      servers = getClusterEndpoints();
      assertTrue(servers.length >= 4, "This test expects four single instances " + JSON.stringify(servers));
      firstLeader = servers[0];
      currentLead = leaderInAgency();
      db._create(cname);
    },

    setUp: function () {
      currentLead = leaderInAgency();
      print(Date() + "connecting shell to leader ", currentLead);
      connectToServer(currentLead);

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
        print(`${Date()} Resuming: ${arangod.name} ${arangod.pid}`);
        assertTrue(arangod.resume());
      });

      currentLead = leaderInAgency();
      print(Date() + "connecting shell to leader ", currentLead);
      connectToServer(currentLead);

      /*setReadOnly(currentLead, false);
      if (db._collection(cname)) {
        db._drop(cname);
      }*/

      setReadOnly(currentLead, false);
      assertTrue(checkInSync(currentLead, servers));

      let i = 100;
      do {
        let endpoints = getClusterEndpoints();
        if (endpoints.length === servers.length && endpoints[0] === currentLead) {
          db._collection(cname).truncate({ compact: false });
          return ;
        }
        print(Date() + "cluster endpoints not as expected: found =", endpoints, " expected =", servers);
        internal.wait(1); // settle down
      } while(i --> 0);

      let endpoints = getClusterEndpoints();
      print(Date() + "endpoints: ", endpoints, " servers: ", servers);
      assertEqual(endpoints.length, servers.length);
      assertEqual(endpoints[0], currentLead);
      db._collection(cname).truncate({ compact: false });
    },

    tearDownAll: function () {
      if (db._collection(cname)) {
        db._drop(cname);
      }
    },


    testReadFromLeader: function () {
      assertEqual(servers[0], currentLead);
      setReadOnly(currentLead, true);

      let col = db._collection(cname);
      assertEqual(col.count(), 10000);
      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);
    },

    testWriteToLeader: function () {
      assertEqual(servers[0], currentLead);
      setReadOnly(currentLead, true);

      let col = db._collection(cname);
      assertEqual(col.count(), 10000);
      assertTrue(checkInSync(currentLead, servers));

      try {
        col.save({abc:1337});
        fail();
      } catch(err) {
        print(err);
        assertEqual(ERRORS.ERROR_ARANGO_READ_ONLY.code, err.errorNum);
      }

      try {
        col.drop();
        fail();
      } catch(err) {
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, err.errorNum);
      }
    },

    // impossible as of now
    //testReadFromFollower: function () {
    //X-Arango-Allow-Dirty-Read: true
    //},

    testLeaderAfterFailover: function () {
      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);

      // set it read-only
      setReadOnly(currentLead, true);

      suspended = global.instanceManager.arangods.filter(arangod => arangod.endpoint === currentLead);
      suspended.forEach(arangod => {
        print(`${Date()} Suspending Leader: ${arangod.name} ${arangod.pid}`);
        assertTrue(arangod.suspend());
      });

      let oldLead = currentLead;
      // await failover and check that follower get in sync
      currentLead = checkForFailover(currentLead);
      //return;
      assertTrue(currentLead !== oldLead);
      print(Date() + "Failover to new leader : ", currentLead);

      internal.wait(5); // settle down, heartbeat interval is 1s
      assertEqual(checkData(currentLead), 10000);
      print(Date() + "New leader has correct data");

      // check the remaining followers get in sync
      assertTrue(checkInSync(currentLead, servers, oldLead));

      print(Date() + "connecting shell to leader ", currentLead);
      connectToServer(currentLead);

      let col = db._collection(cname);
      try {
        col.save({abc:1337});
        fail();
      } catch(err) {
        assertEqual(ERRORS.ERROR_ARANGO_READ_ONLY.code, err.errorNum);
      }

      try {
        col.drop();
        fail();
      } catch(err) {
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, err.errorNum);
      }
    },

    // Test failback case, mainly necessary so that testing.js won't talk
    // to the follower which rejects requests
    testFailback: function() {
      if (currentLead === firstLeader) {
        return; // nevermind then
      }

      setReadOnly(currentLead, true);

      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);

      print(Date() + "Suspending followers, except original leader");
      suspended = global.instanceManager.arangods.filter(arangod => arangod.instanceRole !== 'agent' &&
        arangod.endpoint !== firstLeader);
      suspended.forEach(arangod => {
        print(`${Date()} Suspending: ${arangod.name} ${arangod.pid}`);
        assertTrue(arangod.suspend());
      });

      // await failover and check that follower get in sync
      let oldLead = currentLead;
      currentLead = checkForFailover(currentLead);
      assertTrue(currentLead !== oldLead);
      assertEqual(currentLead, firstLeader, "Did not fail to original leader");

      suspended.forEach(arangod => {
        print(`${Date()} Resuming: ${arangod.name} ${arangod.pid}`);
        assertTrue(arangod.resume());
      });
      suspended = [];

      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);

      print(Date() + "connecting shell to leader ", currentLead);
      connectToServer(currentLead);

      let col = db._collection(cname);
      try {
        col.save({abc:1337});
        fail();
      } catch(err) {
        assertEqual(ERRORS.ERROR_ARANGO_READ_ONLY.code, err.errorNum);
      }

      try {
        col.drop();
        fail();
      } catch(err) {
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, err.errorNum);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ActiveFailoverSuite);

return jsunity.done();
