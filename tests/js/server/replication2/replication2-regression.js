/*jshint strict: true */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const _ = require('lodash');
const {sleep} = require('internal');
const request = require('@arangodb/request');
const db = arangodb.db;
const helper = require("@arangodb/testutils/replicated-logs-helper");
const {assertTrue, assertIdentical} = jsunity.jsUnity.assertions;

const {
  waitForReplicatedLogAvailable,
  registerAgencyTestBegin, registerAgencyTestEnd,
  waitFor,
  getServerUrl,
  checkRequestResult,
  replicatedLogDeleteTarget,
} = helper;
const preds = require("@arangodb/testutils/replicated-logs-predicates");
const {
  allServersHealthy,
} = preds;

const database = "replication2_supervision_test_db";
const IS_FAILED = true;
const IS_NOT_FAILED = false;

const replicatedLogRegressionSuite = function () {

  const {setUpAll, tearDownAll, stopServerWait, continueServerWait, setUp, tearDown} =
    helper.testHelperFunctions(database, {replicationVersion: "2"});

  return {
    setUpAll, tearDownAll, setUp, tearDown,

    // Regression test for CINFRA-263.
    // Previously, while a server was marked as failed, it was not allowed to
    // participate in a quorum. Thus possibly leading to a stall (at least until
    // the next insert).
    testFailedServerDoesNotPreventProgress: function () {
      const targetConfig = {
        writeConcern: 3,
        waitForSync: false,
      };
      const {logId, followers, leader} = helper.createReplicatedLog(database, targetConfig);
      waitForReplicatedLogAvailable(logId);
      const log = db._replicatedLog(logId);
      // stop one server
      stopServerWait(followers[0]);

      log.insert({foo: "bar"}, {dontWaitForCommit: true});
      // we have to insert two log entries here, reason:
      // Even though followers[0] is stopped, it will receive the AppendEntries message for log index 1.
      // It will stay in its tcp input queue. So when the server is continued below it will process
      // this message. However, the leader sees this message as still in flight and thus will never
      // send any updates again. By inserting yet another log entry, we can make sure that servers[2]
      // is the only server that has received log index 2.
      const {index: logIdx} = log.insert({foo: "baz"}, {dontWaitForCommit: true});

      const spearheadOf = (follower) => {
        return log.status().participants[leader].response.follower[follower].spearhead.index;
      };
      waitFor(() => {
            // As followers[0] is stopped and writeConcern=3, the log cannot currently commit.
            const {participants: {[leader]: {response: status}}} = log.status();
            if (status.lastCommitStatus.reason !== "QuorumSizeNotReached") {
              return new Error(`Expected status to be QuorumSizeNotReached, but is ${status.lastCommitStatus.reason}. Full status is: ${JSON.stringify(status)}`);
            }
            if (!(status.local.commitIndex < logIdx)) {
              return new Error(`Commit index is ${status.local.commitIndex}, but should be lower than ${JSON.stringify(logIdx)}.`);
            }
            return true;
          }
      );

      // wait for followers[1] to have received the log entries, and the leader to take note of that
      waitFor(() => {
        const followerSpearhead = spearheadOf(followers[1]);
        return logIdx <= followerSpearhead
          || new Error(`Expected spearhead of ${followers[1]} to be at least ${logIdx}, but is ${followerSpearhead}.`);
      });

      // stop the other follower as well
      stopServerWait(followers[1]);

      // resume the other follower
      continueServerWait(followers[0]);

      // wait for followers[0] to have received the log entries, and the leader to take note of that
      waitFor(() => {
        const followerSpearhead = spearheadOf(followers[0]);
        return logIdx <= followerSpearhead
          || new Error(`Expected spearhead of ${followers[0]} to be at least ${logIdx}, but is ${followerSpearhead}.`);
      });

      { // followers[1] got the log entry before it was stopped, and followers[0] just got it now, so it should be committed.
        const status = log.status().participants[leader].response;
        assertTrue(logIdx <= status.local.commitIndex, `Commit index is ${status.local.commitIndex}, but should be at least ${logIdx}`);
      }

      continueServerWait(followers[1]);
      waitFor(() => {
        const status = log.status();
        const {participants: {[leader]: {response: {lastCommitStatus: {reason: commitReason}, local: {spearhead: {index: spearhead}, commitIndex}}}}}
          = status;
        if (commitReason !== 'NothingToCommit') {
          return Error(`Expected everything to be committed, but last commit reason is ${commitReason}. Status: ${JSON.stringify(status)}`);
        }
        if (commitIndex !== spearhead) {
          return Error(`Commit index is ${commitIndex}, but should be equal to the spearhead ${spearhead}. Status: ${JSON.stringify(status)}`);
        }
        return true;
      });

      replicatedLogDeleteTarget(database, logId);
    },

    testWriteConcernBiggerThanReplicationFactorOnCreate: function () {
      /*
        This test creates a replicated log with writeConcern 5 but only three
        participants. It then asserts that the supervision refuses with a
        StatusReport containing `TargetNotEnoughParticipants`.
        After that we modify the writeConcern in target.
        Then we wait for a leader to be established.
       */
      const replicationFactor = 3;
      const writeConcern = 5;
      const logId = helper.nextUniqueLogId();
      const servers = _.sampleSize(helper.dbservers, replicationFactor);
      helper.replicatedLogSetTarget(database, logId, {
        id: logId,
        config: {writeConcern, waitForSync: false},
        participants: helper.getParticipantsObjectForServers(servers),
        supervision: {maxActionsTraceLength: 20},
        properties: {implementation: {type: "black-hole", parameters: {}}},
      });

      waitFor(function () {
        const {current} = helper.readReplicatedLogAgency(database, logId);
        if (!current || !current.supervision || !current.supervision.StatusReport) {
          return Error("Status report not yet set");
        }

        for (const report of current.supervision.StatusReport) {
          if (report.type === "TargetNotEnoughParticipants") {
            return true;
          }
        }

        return Error("Missing status report for `TargetNotEnoughParticipants`");
      });

      helper.updateReplicatedLogTarget(database, logId, function (target) {
        target.config.writeConcern = 2;
      });
      waitFor(preds.replicatedLogLeaderEstablished(database, logId, undefined, servers));
      replicatedLogDeleteTarget(database, logId);
    },

    testWriteConcernBiggerThanReplicationFactor: function () {
      /*
        This test creates a replicated log with writeConcern 2 and 3 participants.
        Then we set the writeConcern to 5 and wait for the leader to fail
        to commit. After that we revert to 3 and expect the leader to commit.
       */
      const replicationFactor = 3;
      const writeConcern = 2;
      const logId = helper.nextUniqueLogId();
      const servers = _.sampleSize(helper.dbservers, replicationFactor);
      helper.replicatedLogSetTarget(database, logId, {
        id: logId,
        config: {writeConcern, waitForSync: false},
        participants: helper.getParticipantsObjectForServers(servers),
        supervision: {maxActionsTraceLength: 20},
        version: 1,
        properties: {implementation: {type: "black-hole", parameters: {}}},
      });
      waitFor(preds.replicatedLogLeaderEstablished(database, logId, undefined, servers));

      helper.updateReplicatedLogTarget(database, logId, function (target) {
        target.config.writeConcern = 5;
        target.version += 1;
      });

      waitFor(preds.replicatedLogLeaderCommitFail(database, logId, "FewerParticipantsThanWriteConcern"));

      let lastVersion;
      helper.updateReplicatedLogTarget(database, logId, function (target) {
        target.config.writeConcern = 2;
        lastVersion = target.version += 1;
      });

      waitFor(preds.replicatedLogTargetVersion(database, logId, lastVersion));
      replicatedLogDeleteTarget(database, logId);
    },
  };
};

jsunity.run(replicatedLogRegressionSuite);
return jsunity.done();
