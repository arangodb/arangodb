/*jshint strict: true */
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
const {assertTrue, assertEqual} = jsunity.jsUnity.assertions;
const _ = require('lodash');
const lh = require("@arangodb/testutils/replicated-logs-helper");
const lp = require("@arangodb/testutils/replicated-logs-predicates");
const lhttp = require("@arangodb/testutils/replicated-logs-http-helper");
const arangodb = require("@arangodb");
const {db} = arangodb;

const database = 'ReplLogsMaintenanceTest';

const {setUpAll, tearDownAll, setUp, tearDown} = lh.testHelperFunctions(database);

const replicationFactor = 3;
const planConfig = {
  effectiveWriteConcern: 2,
  waitForSync: false,
};

const checkCommitFailReasonReport = function () {
  return {
    setUpAll, tearDownAll, setUp, tearDown,

    testNothingToCommit: function () {
      const {logId} = lh.createReplicatedLogPlanOnly(database, planConfig, replicationFactor);
      lh.waitFor(lp.replicatedLogLeaderCommitFail(database, logId, undefined));
      lh.replicatedLogDeletePlan(database, logId);
    },

    testCheckExcludedServers: function () {
      const {logId, followers} = lh.createReplicatedLogPlanOnly(database, planConfig, replicationFactor);

      const [followerA, followerB] = _.sampleSize(followers, 2);
      lh.replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {
        [followerA]: {allowedInQuorum: false, forced: false},
        [followerB]: {allowedInQuorum: false, forced: false},
      });

      lh.waitFor(lp.replicatedLogLeaderCommitFail(database, logId, "NonEligibleServerRequiredForQuorum"));
      {
        const {current} = lh.readReplicatedLogAgency(database, logId);
        const status = current.leader.commitStatus;
        assertEqual(status.candidates[followerA], "notAllowedInQuorum");
        assertEqual(status.candidates[followerB], "notAllowedInQuorum");
      }

      lh.replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {
        [followerA]: {allowedInQuorum: false, forced: false},
        [followerB]: {allowedInQuorum: true, forced: false},
      });

      lh.waitFor(lp.replicatedLogLeaderCommitFail(database, logId, undefined));
      lh.replicatedLogDeletePlan(database, logId);
    },
  };
};

const replicatedLogSuite = function () {
  return {
    setUpAll, tearDownAll, setUp, tearDown,

    testCreateReplicatedLog: function () {
      const logId = lh.nextUniqueLogId();
      const servers = _.sampleSize(lh.dbservers, replicationFactor);
      const leader = servers[0];
      const term = 1;
      lh.replicatedLogSetPlan(database, logId, {
        id: logId,
        currentTerm: lh.createTermSpecification(term, servers, leader),
        participantsConfig: lh.createParticipantsConfig(1, planConfig, servers),
        properties: {implementation: {type: "black-hole", parameters: {}}}
      });

      // wait for all servers to have reported in current
      lh.waitFor(lp.replicatedLogIsReady(database, logId, term, servers, leader));

      lh.replicatedLogDeletePlan(database, logId);
    },

    testCreateReplicatedLogWithoutLeader: function () {
      const logId = lh.nextUniqueLogId();
      const servers = _.sampleSize(lh.dbservers, replicationFactor);
      const term = 1;
      lh.replicatedLogSetPlan(database, logId, {
        id: logId,
        currentTerm: lh.createTermSpecification(term, servers),
        participantsConfig: lh.createParticipantsConfig(1, planConfig, servers),
        properties: {implementation: {type: "black-hole", parameters: {}}}
      });

      // wait for all servers to have reported in current
      lh.waitFor(lp.replicatedLogIsReady(database, logId, term, servers));

      lh.replicatedLogDeletePlan(database, logId);
    },

    testAddParticipantFlag: function () {
      const {logId, followers} = lh.createReplicatedLogPlanOnly(database, planConfig, replicationFactor);

      // now update the excluded flag for one participant
      const follower = _.sample(followers);
      let newGeneration = lh.replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {
        [follower]: {
          allowedInQuorum: false,
          forced: false
        }
      });
      lh.waitFor(lp.replicatedLogParticipantsFlag(database, logId, {
        [follower]: {
          allowedInQuorum: false,
          allowedAsLeader: true,
          forced: false
        }
      }, newGeneration));

      newGeneration = lh.replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {
        [follower]: {
          allowedInQuorum: true,
          forced: false
        }
      });
      lh.waitFor(lp.replicatedLogParticipantGeneration(database, logId, newGeneration));

      lh.waitFor(lp.replicatedLogParticipantsFlag(database, logId, {
        [follower]: {
          allowedInQuorum: true,
          allowedAsLeader: true,
          forced: false
        }
      }, newGeneration));
      lh.replicatedLogDeletePlan(database, logId);
    },

    testUpdateTermInPlanLog: function () {
      const {logId, term, servers, leader} = lh.createReplicatedLogPlanOnly(database, planConfig, replicationFactor);
      const newTerm = lh.createTermSpecification(term + 1, servers, leader);
      lh.replicatedLogSetPlanTermConfig(database, logId, newTerm);

      // wait again for all servers to have acked term
      lh.waitFor(lp.replicatedLogIsReady(database, logId, term + 1, servers, leader));
      lh.replicatedLogDeletePlan(database, logId);
    },

    testUpdateTermInPlanLogWithNewLeader: function () {
      const {logId, term, servers} = lh.createReplicatedLogPlanOnly(database, planConfig, replicationFactor);
      // wait again for all servers to have acked term
      const otherLeader = servers[1];
      const newTerm = lh.createTermSpecification(term + 1, servers, otherLeader);
      lh.replicatedLogSetPlanTermConfig(database, logId, newTerm);
      lh.waitFor(lp.replicatedLogIsReady(database, logId, term + 1, servers, otherLeader));
      lh.replicatedLogDeletePlan(database, logId);
    },

    testAddParticipant: function () {
      const {logId, term, servers, remaining, leader} = lh.createReplicatedLogPlanOnly(database, planConfig, replicationFactor);
      // now add a participant to the participants configuration
      const newServer = _.sample(remaining);
      const newServers = [...servers, newServer];
      lh.replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {
        [newServer]: {}
      });
      lh.waitFor(lp.replicatedLogIsReady(database, logId, term, newServers, leader));
      lh.replicatedLogDeletePlan(database, logId);
    },

    testRemoveParticipant: function () {
      const logId = lh.nextUniqueLogId();
      const servers = _.sampleSize(lh.dbservers, replicationFactor);
      const remaining = _.difference(lh.dbservers, servers);
      const toBeRemoved = _.sample(remaining);
      const leader = servers[0];
      const term = 1;
      const newServers = [...servers, toBeRemoved];
      lh.replicatedLogSetPlan(database, logId, {
        id: logId,
        currentTerm: lh.createTermSpecification(term, newServers, leader),
        participantsConfig: lh.createParticipantsConfig(1, planConfig, newServers),
        properties: {implementation: {type: "black-hole", parameters: {}}}
      });

      // wait for all servers to have reported in current
      lh.waitFor(lp.replicatedLogIsReady(database, logId, term, newServers));
      lh.replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {
        [toBeRemoved]: null
      });
      lh.waitFor(lp.replicatedLogParticipantsFlag(database, logId, {[toBeRemoved]: null}));
      lh.replicatedLogDeletePlan(database, logId);
    },

    testReportSafeRebootIdsToCurrentAfterCreation: function () {
      const targetConfig = {
        writeConcern: 3,
        softWriteConcern: 3,
        waitForSync: false,
      };
      const log = db._createReplicatedLog({config: targetConfig});
      const {target, current} = lh.readReplicatedLogAgency(database, log.id());
      const expectedRebootIds = Object.fromEntries(
        Object.keys(target.participants).map(p => [p, lh.getServerRebootId(p)])
      );
      assertTrue(current.hasOwnProperty("safeRebootIds"));
      assertEqual(Object.keys(target.participants), Object.keys(current.safeRebootIds));
      assertEqual(expectedRebootIds, current.safeRebootIds);
    },

    testReportSafeRebootIdsToCurrentAfterBump: function () {
      const targetConfig = {
        writeConcern: 3,
        softWriteConcern: 3,
        waitForSync: false,
      };
      const log = db._createReplicatedLog({config: targetConfig});
      const {target, plan, current} = lh.readReplicatedLogAgency(database, log.id());
      const expectedRebootIds = Object.fromEntries(
        Object.keys(target.participants).map(p => [p, lh.getServerRebootId(p)])
      );
      assertTrue(current.hasOwnProperty("safeRebootIds"));
      assertEqual(Object.keys(target.participants), Object.keys(current.safeRebootIds));
      assertEqual(expectedRebootIds, current.safeRebootIds);
      const leader = plan.currentTerm.leader.serverId;
      const follower = _.sample(Object.keys(plan.participantsConfig.participants).filter(p => p !== leader));
      const newRebootId = lh.bumpServerRebootId(follower);
      assertTrue(expectedRebootIds[follower] < newRebootId);
      expectedRebootIds[follower] = newRebootId;
      lh.waitFor(() => {
        const {current} = lh.readReplicatedLogAgency(database, log.id());
        const reportedRebootId = current.safeRebootIds[follower];
        if (reportedRebootId === newRebootId) {
          return true;
        } else {
          return Error(`Reboot id of ${follower} is ${newRebootId}, but still reported as ${reportedRebootId}.`);
        }
      });
      const finalCurrent = lh.readReplicatedLogAgency(database, log.id()).current;
      assertEqual(expectedRebootIds, finalCurrent.safeRebootIds);
    },

    testReportSafeRebootIdsToCurrentAfterReplaceParticipant: function () {
      const targetConfig = {
        writeConcern: 3,
        softWriteConcern: 3,
        waitForSync: false,
      };
      const log = db._createReplicatedLog({config: targetConfig});
      const {target, plan, current} = lh.readReplicatedLogAgency(database, log.id());
      const expectedRebootIds = Object.fromEntries(
        Object.keys(target.participants).map(p => [p, lh.getServerRebootId(p)])
      );
      assertTrue(current.hasOwnProperty("safeRebootIds"));
      assertEqual(Object.keys(target.participants), Object.keys(current.safeRebootIds));
      assertEqual(expectedRebootIds, current.safeRebootIds);
      const leader = plan.currentTerm.leader.serverId;
      const participants = Object.keys(plan.participantsConfig.participants);
      const oldFollower = _.sample(participants.filter(p => p !== leader));
      const newFollower = _.sample(_.difference(lh.dbservers, participants));

      lh.replicatedLogUpdateTargetParticipants(database, log.id(), {
        [newFollower]: {
          allowedInQuorum: true,
          allowedAsLeader: true,
        },
        [oldFollower]: null,
      });

      const newFollowerRebootId = lh.getServerRebootId(newFollower);
      delete expectedRebootIds[oldFollower];
      expectedRebootIds[newFollower] = newFollowerRebootId;

      lh.waitFor(() => {
        const {current} = lh.readReplicatedLogAgency(database, log.id());
        if (!current.safeRebootIds.hasOwnProperty(newFollower)) {
          return Error(`Reboot id of ${newFollower} is not yet reported.`);
        } else if (current.safeRebootIds.hasOwnProperty(newFollower)) {
          return Error(`Reboot id of ${oldFollower} is still being reported.`);
        } else {
          return true;
        }
      });
      const finalCurrent = lh.readReplicatedLogAgency(database, log.id()).current;
      assertEqual(expectedRebootIds, finalCurrent.safeRebootIds);
    },
  };
};

const checkLocalStatusStatusCode = function (database, logId, servers, code, errorNum) {
  for (const server of servers) {
    const response = lhttp.localStatus(server, database, logId);
    if (response.code !== code) {
      return Error(`Server ${server} returned code ${response.code}, expected ${code}`);
    }
    if (response.errorNum !== errorNum) {
      return Error(`Server ${server} returned errorNum ${response.errorNum}, expected ${errorNum}`);
    }
  }
  return true;
};

const hasLocalStatusStatusCode = function (database, logId, servers, code, errorNum) {
  return function () {
    return checkLocalStatusStatusCode(database, logId, servers, code, errorNum);
  };
};

const assertLocalStatusStatusCode = function (database, logId, servers, code, errorNum) {
  const res = checkLocalStatusStatusCode(database, logId, servers, code, errorNum);
  if (res !== true) {
    throw res;
  }
};

const replicatedLogDropSuite = function () {
  return {
    setUpAll, tearDownAll, setUp, tearDown,

    testCreateDropReplicatedLog: function () {
      const {logId, servers} = lh.createReplicatedLogPlanOnly(database, planConfig, replicationFactor);
      lh.waitFor(hasLocalStatusStatusCode(database, logId, servers, 200, undefined));

      lh.replicatedLogDeletePlan(database, logId);

      // wait for current to be gone as well
      //lh.waitFor(lp.replicatedLogIsGone(database, logId));

      // we expect all servers to report 404
      lh.waitFor(hasLocalStatusStatusCode(database, logId, servers, 404, 1418));
    },

  };
};

jsunity.run(replicatedLogSuite);
jsunity.run(replicatedLogDropSuite);
jsunity.run(checkCommitFailReasonReport);
return jsunity.done();
