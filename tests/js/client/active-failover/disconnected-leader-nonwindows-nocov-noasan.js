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
const arangosh = require('@arangodb/arangosh');
const crypto = require('@arangodb/crypto');
const request = require("@arangodb/request");
const arango = internal.arango;
const errors = internal.errors;
const db = internal.db;
const wait = internal.wait;
const compareTicks = require("@arangodb/replication").compareTicks;
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

const cname = "UnitTestActiveFailover";

function connectToServer(leader) {
  leader.connect();
  db._flushCache();
}

// getEndponts works with any server
function getClusterEndpoints() {
  //let jwt = crypto.jwtEncode(options['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
  var res = arango.GET_RAW("/_api/cluster/endpoints");
  assertTrue(res.parsedBody.hasOwnProperty('code'), JSON.stringify(res));
  assertEqual(res.parsedBody.code, 200, JSON.stringify(res));
  assertTrue(res.parsedBody.hasOwnProperty('endpoints'));
  assertTrue(res.parsedBody.endpoints instanceof Array);
  assertTrue(res.parsedBody.endpoints.length > 0);
  let endpoints = res.parsedBody.endpoints.map(e => e.endpoint);
  let ret = [];
  endpoints.forEach(endpoint => {
    ret.push(
      global.instanceManager.arangods.filter(
        arangod => arangod.endpoint === endpoint)[
        0]
    );
  });
  return ret;
}

function getLoggerState(url) {
  var res = request.get({
    url: url + "/_db/_system/_api/replication/logger-state",
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

function getApplierState(url) {
  var res = request.get({
    url: url + "/_db/_system/_api/replication/applier-state?global=true",
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
  print(Date() + "Checking in-sync state with lead: ", leader.url);

  const leaderTick = getLoggerState(leader.url).state.lastLogTick;

  let check = (arangod) => {
    if (arangod.url === leader.url || arangod.url === ignore) {
      return true;
    }

    let applier = getApplierState(arangod.url);
    print(Date() + "Checking endpoint ", arangod.url, " applier.state.running=", applier.state.running, " applier.endpoint=", applier.endpoint);
    return applier.state.running && applier.endpoint === leader.endpoint &&
      (compareTicks(applier.state.lastAppliedContinuousTick, leaderTick) >= 0 ||
        compareTicks(applier.state.lastProcessedContinuousTick, leaderTick) >= 0);
  };

  let loop = 100;
  while (loop-- > 0) {
    if (servers.every(check)) {
      print(Date() + "All followers are in sync with: ", leader.name);
      return true;
    }
    wait(1.0);
  }
  print(Date() + "Timeout waiting for followers of: ", leader.name);
  return false;
}

function readAgencyValue(path) {
  print(Date() + "Querying agency... (", path, ")");
  let res = global.instanceManager.getAgency("/_api/agency/read", 'POST', JSON.stringify([[path]]));
  assertTrue(res.hasOwnProperty('code'), JSON.stringify(res));
  assertEqual(res.code, 200, JSON.stringify(res));
  return JSON.parse(arangosh.checkRequestResult(res.body));
}

// resolve leader from agency
function leaderInAgency() {
  let i = 10;
  do {
    let res = readAgencyValue("/arango/Plan/AsyncReplication/Leader");
    let uuid = res[0].arango.Plan.AsyncReplication.Leader;
    if (uuid && uuid.length > 0) {
      res = readAgencyValue("/arango/Supervision/Health");
      let leaderEndpoint = res[0].arango.Supervision.Health[uuid].Endpoint;
      return global.instanceManager.arangods.filter(arangod => arangod.endpoint === leaderEndpoint)[0];
    }
    internal.wait(1.0);
  } while (i-- > 0);
  throw "Unable to resole leader from agency";
}

function waitUntilHealthStatusIs(isHealthy, isFailed) {
  let healthNames = [];
  isHealthy.forEach(arangod => healthNames.push(arangod.name));

  print(Date() + "Waiting for health status to be healthy: ", healthNames, " failed: ", JSON.stringify(isFailed));
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
        isHealthy.forEach(arangod => {
          
          if (arangod.endpoint === srv.Endpoint) {
            foundHealthy++;
          }
        });
      }
    }
    if (!needToWait && foundHealthy === isHealthy.length && foundFailed === isFailed.length) {
      return true;
    }
  }
  print(Date() + "Timing out, could not reach desired state: ", healthNames, " failed: ", JSON.stringify(isFailed));
  print(Date() + "We only got: ", JSON.stringify(readAgencyValue("/arango/Supervision/Health")[0].arango.Supervision.Health));
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
        debugClearFailAt(s.endpoint);
      });

      let currentLead = leaderInAgency();
      print(Date() + "connecting shell to leader ", currentLead.name);
      connectToServer(currentLead);
      
      db._drop(cname);

      let i = 100;
      do {
        let endpoints = getClusterEndpoints();
        if (endpoints.length === servers.length && endpoints[0].name === currentLead.name) {
          break;
        }
        print(Date() + "cluster endpoints not as expected: found =", endpoints.length, " expected =", servers.length);
        internal.wait(1); // settle down
      } while (i-- > 0);

      let endpoints = getClusterEndpoints();
      print(Date() + "endpoints: ", endpoints.length, " servers: ", servers.length);
      assertEqual(endpoints.length, servers.length);
      assertEqual(endpoints[0], currentLead);
      
      connectToServer(initialLead);

      // We need to restore the original leader, so our fuse against
      // accidential leader change doesn't blow.
      // the test framework does some post-checks, which queries all
      // sorts of REST endpoints. querying such endpoints is disallowed
      // on active failover followers, and will result in the post-tests
      // failing.
      // therefore it is necessary that we finish the test with the
      // leader being the same leader as when the test was started.
      currentLead = leaderInAgency();
      if (currentLead.url !== initialLead.url) {
        let toSuspend = global.instanceManager.arangods.filter(arangod => arangod.endpoint !== initialLead.endpoint && !arangod.isAgent());
        toSuspend.forEach(arangod => {
          print(`${Date()} Suspending Leader: ${arangod.name} ${arangod.pid} ${arangod.endpoint}`);
          assertTrue(arangod.suspend());
        });

        i = 100;
        do {
          currentLead = leaderInAgency();
          if (currentLead === initialLead) {
            break;
          }
          internal.wait(1.0);
        } while (i-- > 0);

        toSuspend.forEach(arangod => {
          print(`${Date()} Teardown: Resuming: ${arangod.name} ${arangod.pid}`);
          assertTrue(arangod.resume());
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
      if (!debugCanUseFailAt(oldLead.endpoint)) {
        return;
      }
     
      // establish connection to leader before we are making it 
      // unresponsive.
      connectToServer(oldLead);
      
      // we need to set this failure point on the leader so that we can
      // later reconnect to it. if we don't set this failure point, the
      // leader will switch into TRYAGAIN mode and every request to /_api/version
      // and /_api/database/current is denied. such requests happen during reconnects.
      debugSetFailAt(oldLead.endpoint, "CommTask::allowReconnectRequests");

      // we want this to speed up the test
      debugSetFailAt(oldLead.endpoint, "HeartbeatThread::reducedLeaderGracePeriod");

      // this failure point makes the leader not send its heartbeat
      // to the agency anymore
      print(Date() + "Setting failure point to block heartbeats from leader");
      debugSetFailAt(oldLead.endpoint, "HeartbeatThread::sendServerState");

      let currentLead;
      let i = 60;
      do {
        currentLead = leaderInAgency();
        if (currentLead.name !== oldLead.name) {
          break;
        }
        internal.sleep(1.0);
      } while (i-- > 0);

      print(Date() + "Leader has changed to", currentLead.name);

      // leader must have changed
      assertNotEqual(currentLead.name, oldLead.name);

      try {
        // this may or may not succeed.
        // collection creation will fail if the agency has not yet
        // elected a new leader. if the agency has already elected
        // a new leader, this collection creation however will succeed.
        // unfortunately we cannot reliably test this here.
        db._create(cname + "2");
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING.code, err.errorNum, err);
      }

      // clear failure points
      print(Date() + "Healing old leader", oldLead.name);
      debugRemoveFailAt(oldLead.endpoint, "HeartbeatThread::sendServerState");

      connectToServer(currentLead);

      print(Date() + "Waiting for old leader to become a follower");
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
