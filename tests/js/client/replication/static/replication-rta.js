/* jshint globalstrict:false, strict:false, unused: false */
/* global ARGUMENTS */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Wilfried Goesgens
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {
  assertEqual, assertFalse, assertInstanceOf, assertNotEqual,
  assertNotNull, assertNull, assertTrue, fail
} = jsunity.jsUnity.assertions;
const fs = require('fs');
const arangodb = require('@arangodb');
const errors = arangodb.errors;
const db = arangodb.db;
const _ = require('lodash');
const userManager = require("@arangodb/users");

const replication = require('@arangodb/replication');
const compareTicks = require('@arangodb/replication-common').compareTicks;
const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const console = require('console');
const internal = require('internal');
const arango = internal.arango;

const cn = 'UnitTestsReplication';
const cn2 = 'UnitTestsReplication2';
const systemCn = '_UnitTestsReplicationSys';

// these must match the values in the Makefile!
const replicatorUser = 'replicator-user';
const replicatorPassword = 'replicator-password';
let IM = global.instanceManager;
const leaderEndpoint = IM.arangods[0].endpoint;
const followerEndpoint = IM.arangods[1].endpoint;
const ct = require('@arangodb/testutils/client-tools');

const connectToLeader = function() {
  reconnectRetry(leaderEndpoint, db._name(), replicatorUser, replicatorPassword);
};

const connectToFollower = function() {
  reconnectRetry(followerEndpoint, db._name(), 'root', '');
};

function rtaMakeCheckDataSuite() {
  let runRTAMakeCheckData = function(which, message) {
    let moreargv = ['--testFoxx', false]; // the follower doesn't spawn foxxes.
    let logFile = fs.join(fs.getTempPath(), `rta_out_${which}_${message.replace(' ', '_')}.log`);
    let rc = ct.run.rtaMakedata(IM.options, IM, which, message, logFile, moreargv);
    if (!rc.status) {
      IM.options.cleanup = false;
      let rx = new RegExp(/\\n/g);
      IM.options.cleanup = false;
      throw new Error(`replication-static.js: while ${message} \n${fs.read(logFile).replace(rx, '\n')}`);
    } else if (IM.options.cleanup) {
      fs.remove(logFile);
    }
  };
  let checkReplicationCatchup = function() {
    var state = {};
    let printed = false;
    connectToLeader();
    state.lastLogTick = replication.logger.state().state.lastUncommittedLogTick;
    IM.arangods[1].connect();
    while (true) {
      connectToFollower();
      var followerState = replication.globalApplier.state();
      if (followerState.state.lastError.errorNum > 0) {
        console.topic("replication=error", "follower has errored:", JSON.stringify(followerState.state.lastError));
        throw JSON.stringify(followerState.state.lastError);
      }

      if (!followerState.state.running) {
        throw new Error(`replication=error follower is not running: ${JSON.stringify(followerState)}`);
      }

      if (compareTicks(followerState.state.lastAppliedContinuousTick, state.lastLogTick) >= 0 ||
          compareTicks(followerState.state.lastProcessedContinuousTick, state.lastLogTick) >= 0) { // ||
        console.topic("replication=debug", "follower has caught up. state.lastLogTick:", state.lastLogTick, "followerState.lastAppliedContinuousTick:", followerState.state.lastAppliedContinuousTick, "followerState.lastProcessedContinuousTick:", followerState.state.lastProcessedContinuousTick);
        break;
      }
      
      if (!printed) {
        console.topic("replication=debug", "waiting for follower to catch up");
        printed = true;
      }
      internal.wait(0.5, false);
    }
  };
  return {
    setUp: function() {
      connectToFollower();
      replication.applier.stop();
      var syncResult = replication.setupReplicationGlobal({
        endpoint: leaderEndpoint,
        username: "root",
        password: "",
        verbose: true,
        includeSystem: true,
        incremental: true,
        autoResync: true,
        waitForSyncTimeout: 120,
        keepBarrier: true
      });
    },

    tearDown: function() {
      connectToFollower();
      replication.applier.stop();
    },
    tearDownAll: function() {
      connectToLeader();
    },
    testMakeDataLeader: function () {
      IM.endpoint = IM.arangods[0].endpoint;
      runRTAMakeCheckData(0, "creating data on leader");
    },
    testCheckDataLeader: function () {
      IM.endpoint = IM.arangods[0].endpoint;
      runRTAMakeCheckData(1, "checking data on leader");
      checkReplicationCatchup();
    },
    testCheckDataFollower: function () {
      IM.endpoint = IM.arangods[1].endpoint;
      runRTAMakeCheckData(1, "checking data on follower");
    },
    testClearData: function () {
      if (IM.options.cleanup) {
        IM.endpoint = IM.arangods[0].endpoint;
        runRTAMakeCheckData(2, "cleaning up");
        checkReplicationCatchup();
      }
    }
  };
}

jsunity.run(rtaMakeCheckDataSuite);
return jsunity.done();
