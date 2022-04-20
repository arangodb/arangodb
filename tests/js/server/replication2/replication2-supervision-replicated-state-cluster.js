/*jshint strict: true */
/*global assertEqual */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const _ = require('lodash');
const lh = require("@arangodb/testutils/replicated-logs-helper");
const sh = require("@arangodb/testutils/replicated-state-helper");
const spreds = require("@arangodb/testutils/replicated-state-predicates");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");

const database = "replicated_state_test_bad_participants_database";

const replicatedStateSuite = function (stateType) {
  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    replicationFactor: 3,
    waitForSync: false,
  };
  const {setUpAll, tearDownAll, stopServerWait, setUp, tearDown} = lh.testHelperFunctions(database);
  const createReplicatedState = function () {
    return sh.createReplicatedStateTarget(database, targetConfig, stateType);
  };

  return {
    setUpAll, tearDownAll, setUp, tearDown,

    ["testReplaceBadServer_" + stateType]: function () {
      const {stateId, followers, servers, others} = createReplicatedState();
      const badServer = _.sample(followers);
      stopServerWait(badServer);

      const replacement = _.sample(others);
      sh.updateReplicatedStateTarget(database, stateId, function (target) {
        target.participants[replacement] = {};
        return target;
      });
      const activeServers = [replacement, ..._.without(servers, badServer)];
      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, activeServers));

      // now remove the old server
      sh.updateReplicatedStateTarget(database, stateId, function (target) {
        delete target.participants[badServer];
        return target;
      });
      lh.waitFor(spreds.replicatedStateServerIsGone(database, stateId, badServer));
    },

    ["testReplaceBadServerLeader_" + stateType]: function () {
      const {stateId, servers, others, leader} = createReplicatedState();
      const badServer = leader;
      stopServerWait(badServer);

      const replacement = _.sample(others);
      sh.updateReplicatedStateTarget(database, stateId, function (target) {
        target.participants[replacement] = {};
        return target;
      });
      const activeServers = [replacement, ..._.without(servers, badServer)];
      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, activeServers));

      // now remove the old server
      sh.updateReplicatedStateTarget(database, stateId, function (target) {
        delete target.participants[badServer];
        return target;
      });
      lh.waitFor(spreds.replicatedStateServerIsGone(database, stateId, badServer));
    },

    ["testRemoveAllFollowers_" + stateType]: function () {
      const {stateId, followers} = createReplicatedState();
      // remove both followers
      sh.updateReplicatedStateTarget(database, stateId, function (target) {
        for (const f of followers) {
          delete target.participants[f];
        }
        return target;
      });
      // wait for everything come to halt
      lh.waitFor(spreds.replicatedStateStatusAvailable(database, stateId));
      lh.waitFor(lpreds.replicatedLogLeaderCommitFail(database, stateId, "NonEligibleServerRequiredForQuorum"));


      const report = sh.getReplicatedStateStatus(database, stateId);
      assertEqual(1, report.length);
      assertEqual("LogParticipantNotYetGone", report[0].code);
      const secondFollower = report[0].participant;

      // now bring back one of the followers
      sh.updateReplicatedStateTarget(database, stateId, function (target) {
        target.participants[secondFollower] = {};
        return target;
      });

      // service should continue as normal
      lh.waitFor(lpreds.replicatedLogLeaderCommitFail(database, stateId, undefined));
    }
  };
};

const suiteWithState = function (stateType) {
  return function () {
    return replicatedStateSuite(stateType);
  };
};

jsunity.run(suiteWithState("black-hole"));
jsunity.run(suiteWithState("prototype"));
return jsunity.done();
