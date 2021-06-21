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
var arangodb = require("@arangodb");
var ERRORS = arangodb.errors;

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

  const leaderTick = getLoggerState(leader).state.lastLogTick;

  let loop = 100;
  while (loop-- > 0) {
    if (servers.every(check)) {
      print("All followers are in sync with lead: ", leader);
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
    },
    timeout: 300
  });

  assertTrue(res instanceof request.Response);
  //assertTrue(res.hasOwnProperty('statusCode'));
  assertEqual(res.statusCode, 200);
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

function setReadOnly(endpoint, ro) {
  print("Setting read-only ", ro);

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
      currentLead = leaderInAgency();
      print("connecting shell to leader ", currentLead);
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
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });

      currentLead = leaderInAgency();
      print("connecting shell to leader ", currentLead);
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

      suspended = instanceinfo.arangods.filter(arangod => arangod.endpoint === currentLead);
      suspended.forEach(arangod => {
        print("Suspending Leader: ", arangod.endpoint);
        assertTrue(suspendExternal(arangod.pid));
      });

      let oldLead = currentLead;
      // await failover and check that follower get in sync
      currentLead = checkForFailover(currentLead);
      //return;
      assertTrue(currentLead !== oldLead);
      print("Failover to new leader : ", currentLead);

      internal.wait(5); // settle down, heartbeat interval is 1s
      assertEqual(checkData(currentLead), 10000);
      print("New leader has correct data");

      // check the remaining followers get in sync
      assertTrue(checkInSync(currentLead, servers, oldLead));

      print("connecting shell to leader ", currentLead);
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
      assertTrue(currentLead !== oldLead);
      assertEqual(currentLead, firstLeader, "Did not fail to original leader");

      suspended.forEach(arangod => {
        print("Resuming: ", arangod.endpoint);
        assertTrue(continueExternal(arangod.pid));
      });
      suspended = [];

      assertTrue(checkInSync(currentLead, servers));
      assertEqual(checkData(currentLead), 10000);

      print("connecting shell to leader ", currentLead);
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
