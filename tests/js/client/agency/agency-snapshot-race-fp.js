/*jshint globalstrict:false, strict:true */
/*global assertEqual, assertTrue, assertNotEqual, ARGUMENTS */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const wait = require("internal").wait;
const request = require("@arangodb/request");

function agencySnapshotRaceSuite() {
  'use strict';

  const instanceInfo = JSON.parse(require('internal').env.INSTANCEINFO);
  const agencyServers = instanceInfo.arangods.map(arangod => arangod.url);

  let leaderUrl = agencyServers[0];

  // ---------------------------------------------------------------------------
  // helpers
  // ---------------------------------------------------------------------------

  function setFailAt(url, fp) {
    let res = request({url: url + "/_admin/debug/failat/" + fp,
                       method: "PUT"});
    assertEqual(200, res.statusCode,
                "Failed to set failure point " + fp + " on " + url);
  }

  function removeFailAt(url, fp) {
    let res = request({url: url + "/_admin/debug/failat/" + fp,
                       method: "DELETE"});
    assertEqual(200, res.statusCode,
                "Failed to remove failure point " + fp + " on " + url);
  }

  function clearFailAt(url) {
    request({url: url + "/_admin/debug/failat", method: "DELETE"});
  }

  function failurePointsAvailable() {
    let res = request({url: agencyServers[0] + "/_admin/debug/failat",
                       method: "GET"});
    return res.statusCode === 200;
  }

  function findLeader() {
    for (let attempt = 0; attempt < 60; ++attempt) {
      for (let url of agencyServers) {
        try {
          let res = request({url: url + "/_api/agency/config",
                             method: "GET", followRedirect: true});
          if (res.statusCode === 200) {
            let config = JSON.parse(res.body);
            if (config.leaderId !== "") {
              // map leaderId to URL via the pool
              let pool = config.configuration.pool;
              let leaderEndpoint = pool[config.leaderId];
              if (leaderEndpoint) {
                // convert endpoint to URL
                let lUrl = leaderEndpoint.replace("tcp://", "http://")
                                         .replace("ssl://", "https://");
                return {
                  leaderId: config.leaderId,
                  leaderUrl: lUrl,
                  compactionStepSize: config.configuration["compaction step size"],
                  compactionKeepSize: config.configuration["compaction keep size"],
                  followerUrls: agencyServers.filter(s => s !== lUrl)
                };
              }
            }
          }
        } catch (e) {
          // retry
        }
      }
      wait(1.0);
    }
    assertTrue(false, "Could not find agency leader within 60 seconds");
  }

  function accessAgency(api, list, timeout = 60) {
    let res;
    let startTime = new Date();
    while (true) {
      if (new Date() - startTime > 120000) {
        assertTrue(false, "Timeout in accessAgency");
      }
      res = request({url: leaderUrl + "/_api/agency/" + api,
                     method: "POST", followRedirect: false,
                     body: JSON.stringify(list),
                     headers: {"Content-Type": "application/json"},
                     timeout: timeout});
      if (res.statusCode === 307) {
        leaderUrl = res.headers.location;
        let l = 0;
        for (let i = 0; i < 3; ++i) {
          l = leaderUrl.indexOf('/', l + 1);
        }
        leaderUrl = leaderUrl.substring(0, l);
        continue;
      }
      if (res.statusCode === 503 || res.statusCode === 500) {
        wait(1.0);
        continue;
      }
      break;
    }
    try {
      res.bodyParsed = JSON.parse(res.body);
    } catch (e) {
      // ignore parse errors
    }
    return res;
  }

  function writeEntries(count) {
    let batch = [];
    for (let i = 0; i < count; ++i) {
      batch.push([{["/race/key" + Math.random()]: {"op": "set", "new": i}}]);
      if (batch.length >= 200) {
        let res = accessAgency("write", batch);
        assertEqual(200, res.statusCode, "write failed: " + JSON.stringify(res));
        batch = [];
      }
    }
    if (batch.length > 0) {
      let res = accessAgency("write", batch);
      assertEqual(200, res.statusCode, "write failed: " + JSON.stringify(res));
    }
  }

  function getAgencyConfig(url) {
    try {
      let res = request({url: url + "/_api/agency/config",
                         method: "GET", timeout: 30});
      if (res.statusCode === 200) {
        return JSON.parse(res.body);
      }
    } catch (e) {
      // ignore
    }
    return null;
  }

  function isAlive(url) {
    return getAgencyConfig(url) !== null;
  }

  function waitForCompaction(url, minCompactionIndex) {
    for (let attempt = 0; attempt < 60; ++attempt) {
      let config = getAgencyConfig(url);
      if (config && config.lastCompactionAt >= minCompactionIndex) {
        return config.lastCompactionAt;
      }
      wait(1.0);
    }
    assertTrue(false,
               "Compaction did not advance past " + minCompactionIndex +
               " on " + url + " within 60 seconds");
  }

  // ---------------------------------------------------------------------------
  // test suite
  // ---------------------------------------------------------------------------

  return {

    setUp: function () {
      // clear any leftover failure points on all agents
      agencyServers.forEach(url => clearFailAt(url));
    },

    tearDown: function () {
      agencyServers.forEach(url => clearFailAt(url));
    },

    //--------------------------------------------------------------------------
    // Reproduces the TOCTOU race in sendAppendEntriesRPC where the leader
    // reads lastCompactionAt, fetches old entries, then compaction advances
    // the snapshot, and loadLastCompactedSnapshot returns a newer snapshot.
    // The follower receives snapshot@N + entries@M (M << N), setting _cur=N
    // but appending entries at M, causing _cur > _log.back().index → crash.
    //--------------------------------------------------------------------------
    testSnapshotCompactionRace: function () {
      if (!failurePointsAvailable()) {
        return; // skip if not built with failure tests
      }

      let info = findLeader();
      leaderUrl = info.leaderUrl;
      let stepSize = info.compactionStepSize;

      assertTrue(info.followerUrls.length >= 1,
                 "Need at least one follower agent");
      let followerUrl = info.followerUrls[0];

      // Phase 1: Make the target follower fall behind by rejecting all
      // AppendEntries. The leader will reset _lastAckedIndex = 0 for this
      // follower, which later triggers snapshot send.
      setFailAt(followerUrl, "Agent::recvAppendEntriesRPC::drop");

      // Phase 2: Write enough entries to trigger at least one compaction
      // round on the leader. We write 4x the compaction step size.
      writeEntries(stepSize * 4);

      // Wait for compaction to advance the log front on the leader.
      // After compaction, the leader's firstIndex is well past 0, so when
      // the follower's _lastAckedIndex is 0 the leader will send a snapshot.
      waitForCompaction(info.leaderUrl, stepSize);

      // Phase 3: Set up the race window.
      // Block further compaction on the leader so we can accumulate debt.
      setFailAt(info.leaderUrl, "State::compact");

      // Write more entries to create compaction debt.
      writeEntries(stepSize * 2);

      // Activate the pause: leader will sleep 5 seconds between _state.get()
      // and loadLastCompactedSnapshot() when it needs to send a snapshot.
      setFailAt(info.leaderUrl, "Agent::sendAppendEntriesRPC::pauseAfterGetEntries");

      // Now release both: the follower (ready to receive) and compaction
      // (will run during the 5s pause, advancing the snapshot).
      removeFailAt(followerUrl, "Agent::recvAppendEntriesRPC::drop");
      removeFailAt(info.leaderUrl, "State::compact");

      // Phase 4: Wait for the race to play out.
      // The leader's sendAppendEntriesRPC will:
      //   1. Read lastCompactionAt (old) and fetch old entries
      //   2. Sleep 5 seconds (failure point)
      //   3. Compaction runs, advancing the snapshot
      //   4. loadLastCompactedSnapshot returns the new snapshot
      //   5. Leader sends inconsistent payload to follower
      // Without a fix, the follower crashes.
      wait(10);

      // Clean up the pause failure point.
      clearFailAt(info.leaderUrl);

      // Verify all agents are still alive.
      for (let url of agencyServers) {
        assertTrue(isAlive(url),
                   "Agent at " + url + " is not responding — likely crashed " +
                   "due to the snapshot/compaction race condition");
      }
    }
  };
}

jsunity.run(agencySnapshotRaceSuite);

return jsunity.done();
