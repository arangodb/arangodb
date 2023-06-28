/*jshint strict: false, sub: true */
/*global print, assertTrue, assertEqual, assertNotEqual, fail */
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
const fs = require('fs');
const path = require('path');
const utils = require('@arangodb/foxx/manager-utils');
const arango = internal.arango;
const errors = internal.errors;
const db = internal.db;
const wait = internal.wait;
const compareTicks = require("@arangodb/replication").compareTicks;
const suspendExternal = internal.suspendExternal;
const continueExternal = internal.continueExternal;
const { debugSetFailAt, debugClearFailAt, debugRemoveFailAt, debugCanUseFailAt } = require("@arangodb/test-helper");

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

function LeaderDisconnectedSuite() {
  const servers = getClusterEndpoints();
  assertTrue(servers.length >= 4, "This test expects four single instances");
  const initialLead = leaderInAgency();

  return {
    setUp: function () {
      let c = db._create(cname);
      let docs = [];
      for (let i = 0; i < 10000; i++) {
        docs.push({ attr: i});
      }
      c.insert(docs);
      assertTrue(checkInSync(leaderInAgency(), servers));
    },

    tearDown: function () {
      // clear all failure points
      servers.forEach((s) => {
        debugClearFailAt(s);
      });

      let currentLead = leaderInAgency();
      print("connecting shell to leader ", currentLead);
      connectToServer(currentLead);
      
      db._drop(cname);

      let i = 100;
      do {
        let endpoints = getClusterEndpoints();
        if (endpoints.length === servers.length && endpoints[0] === currentLead) {
          break;
        }
        print("cluster endpoints not as expected: found =", endpoints, " expected =", servers);
        internal.wait(1); // settle down
      } while (i-- > 0);

      let endpoints = getClusterEndpoints();
      print("endpoints: ", endpoints, " servers: ", servers);
      assertEqual(endpoints.length, servers.length);
      assertEqual(endpoints[0], currentLead);
      
      connectToServer(initialLead);

      // unfortunately the following is necessary to work against
      // the quirks of our test framework.
      // the test framework does some post-checks, which queries all
      // sorts of REST endpoints. querying such endpoints is disallowed
      // on active failover followers, and will result in the post-tests
      // failing.
      // therefore it is necessary that we finish the test with the
      // leader being the same leader as when the test was started.
      currentLead = leaderInAgency();
      if (currentLead !== initialLead) {
        let suspended = [];
        let toSuspend = instanceinfo.arangods.filter(arangod => arangod.endpoint !== initialLead && arangod.role !== 'agent');
        toSuspend.forEach(arangod => {
          print("Suspending servers: ", arangod.endpoint);
          assertTrue(suspendExternal(arangod.pid));
          suspended.push(toSuspend);
        });

        i = 100;
        do {
          currentLead = leaderInAgency();
          if (currentLead === initialLead) {
            break;
          }
          internal.wait(1.0);
        } while (i-- > 0);
      
        suspended.forEach(arangod => {
          print("Resuming: ", arangod.endpoint);
          assertTrue(continueExternal(arangod.pid));
        });

        // wait until the server itself has recognized that it is leader again
        i = 100;
        do {
          try {
            db._dropDatabase("doesNotExist");
          } catch (err) {
            if (err.errorNum !== errors.ERROR_CLUSTER_NOT_LEADER.code) {
              break;
            }
          }
          internal.wait(1.0);
        } while (i-- > 0);
      }

      assertEqual(currentLead, initialLead);
      // must drop again here
      db._drop(cname);

      assertTrue(waitUntilHealthStatusIs(servers, []));
      // wait for things to settle. we need to do this so all follow-up tests
      // (in other files) find the server to be ready again.
      internal.wait(3.0);
    },

    testLeaderCannotSendToAgency: function () {
      let oldLead = leaderInAgency();
      if (!debugCanUseFailAt(oldLead)) {
        return;
      }
     
      // establish connection to leader before we are making it 
      // unresponsive.
      connectToServer(oldLead);
      
      // we need to set this failure point on the leader so that we can
      // later reconnect to it. if we don't set this failure point, the
      // leader will switch into TRYAGAIN mode and every request to /_api/version
      // and /_api/database/current is denied. such requests happen during reconnects.
      debugSetFailAt(oldLead, "CommTask::allowReconnectRequests");

      // we want this to speed up the test
      debugSetFailAt(oldLead, "HeartbeatThread::reducedLeaderGracePeriod");

      // this failure point makes the leader not send its heartbeat
      // to the agency anymore
      print("Setting failure point to block heartbeats from leader");
      debugSetFailAt(oldLead, "HeartbeatThread::sendServerState");

      let currentLead;
      let i = 60;
      do {
        currentLead = leaderInAgency();
        if (currentLead !== oldLead) {
          break;
        }
        internal.sleep(1.0);
      } while (i-- > 0);

      print("Leader has changed to", currentLead);

      // leader must have changed
      assertNotEqual(currentLead, oldLead);

      try {
        db._create(cname + "2");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING.code, err.errorNum);
      }

      // clear failure points
      print("Healing old leader", oldLead);
      debugRemoveFailAt(oldLead, "HeartbeatThread::sendServerState");

      connectToServer(currentLead);

      print("Waiting for old leader to become a follower");
      // wait until old leader responds properly again
      i = 60;
      do {
        try {
          connectToServer(oldLead);
          // if this succeeds, the old leader is back in normal operations mode 
          db._collections();
          break;
        } catch (err) {
          // leader will refuse the connection while it is in TRYAGAIN mode.
        }
        internal.sleep(1.0);
      } while (i-- > 0);
          
      connectToServer(oldLead);
      db._collections();

      // we need this to make the post-tests succeed. these query extra
      // paths such as /_api/user, which is disallowed for followers in 
      // active failover mode
      connectToServer(currentLead);

      // make sure everything gets back into sync
      assertTrue(checkInSync(currentLead, servers));
    },

  };
}

jsunity.run(LeaderDisconnectedSuite);

return jsunity.done();
