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
const {
  allServersHealthy,
} = require("@arangodb/testutils/replicated-logs-predicates");

const database = "replication2_supervision_test_db";
const IS_FAILED = true;
const IS_NOT_FAILED = false;

const getFailureOracleServerStatusOn = (oracleServerId) => {
  const endpoint = getServerUrl(oracleServerId);
  const status = request.get(`${endpoint}/_admin/cluster/failureOracle/status`);
  checkRequestResult(status);
  return status.json.result;
};

// `oracleServerId` is the server whose oracle is queried;
// `queriedServerId` is the server which status is looked at;
// `desiredStatus` is the "isFailed" status to wait for:
//                 pass IS_FAILED or IS_NOT_FAILED as `desiredStatus`.
const waitForFailureOracleServerStatusOn = (oracleServerId) => (queriedServerId, desiredStatus) => {
  const seconds = (x) => x * 1000;
  let queriedStatus;
  for (const start = Date.now(); Date.now() < start + seconds(10); sleep(1)) {
    const statuses = getFailureOracleServerStatusOn(oracleServerId);
    queriedStatus = statuses.isFailed[queriedServerId];
    if (queriedStatus === desiredStatus) {
      return;
    }
  }
  throw new Error(`Timeout while waiting for ${oracleServerId} to observe 'isFailed' of ${queriedServerId} to equal ${desiredStatus}. The last observed status was ${queriedStatus} instead.`);
};


const replicatedLogRegressionSuite = function () {
  const {setUpAll, tearDownAll, stopServer, continueServer, resumeAll} = (function () {
    let previousDatabase;
    let databaseExisted = true;
    const stoppedServers = {};

    return {
      setUpAll: function () {
        previousDatabase = db._name();
        if (!_.includes(db._databases(), database)) {
          db._createDatabase(database);
          databaseExisted = false;
        }
        db._useDatabase(database);
      },

      tearDownAll: function () {
        db._useDatabase(previousDatabase);
        if (!databaseExisted) {
          db._dropDatabase(database);
        }
      },
      stopServer: function (serverId) {
        if (stoppedServers[serverId] !== undefined) {
          throw Error(`${serverId} already stopped`);
        }
        stoppedServers[serverId] = true;
        helper.stopServer(serverId);
      },
      continueServer: function (serverId) {
        if (stoppedServers[serverId] === undefined) {
          throw Error(`${serverId} not stopped`);
        }
        helper.continueServer(serverId);
        delete stoppedServers[serverId];
      },
      resumeAll: function () {
        Object.keys(stoppedServers).forEach(function (key) {
          continueServer(key);
        });
      }
    };
  }());

  return {
    setUpAll, tearDownAll,
    setUp: registerAgencyTestBegin,
    tearDown: function (test) {
      resumeAll();
      waitFor(allServersHealthy());
      registerAgencyTestEnd(test);
    },

    // Regression test for CINFRA-263.
    // Previously, while a server was marked as failed, it was not allowed to
    // participate in a quorum. Thus possibly leading to a stall (at least until
    // the next insert).
    testFailedServerDoesNotPreventProgress: function () {
      const targetConfig = {
        writeConcern: 3,
        replicationFactor: 3,
        waitForSync: false,
      };
      const {logId, followers, leader} = helper.createReplicatedLog(database, targetConfig);
      const waitForFailureOracleServerStatus = waitForFailureOracleServerStatusOn(leader);
      waitForReplicatedLogAvailable(logId);
      const log = db._replicatedLog(logId);
      // stop one server
      stopServer(followers[0]);
      waitForFailureOracleServerStatus(followers[0], IS_FAILED);

      log.insert({foo: "bar"}, {dontWaitForCommit: true});
      // we have to insert two log entries here, reason:
      // Even though followers[0] is stopped, it will receive the AppendEntries message for log index 1.
      // It will stay in its tcp input queue. So when the server is continued below it will process
      // this message. However, the leader sees this message as still in flight and thus will never
      // send any updates again. By inserting yet another log entry, we can make sure that servers[2]
      // is the only server that has received log index 2.
      const {index: logIdx} = log.insert({foo: "baz"}, {dontWaitForCommit: true});

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
        const followerSpearhead = log.status().participants[leader].response.follower[followers[1]].spearhead.index;
        return logIdx === followerSpearhead
          || new Error(`Expected spearhead of ${followers[1]} to be ${logIdx}, but is ${followerSpearhead}.`);
      });

      // stop the other follower as well
      stopServer(followers[1]);
      waitForFailureOracleServerStatus(followers[1], IS_FAILED);

      // resume the other follower
      continueServer(followers[0]);
      waitForFailureOracleServerStatus(followers[0], IS_NOT_FAILED);

      // wait for followers[0] to have received the log entries, and the leader to take note of that
      waitFor(() => {
        const followerSpearhead = log.status().participants[leader].response.follower[followers[0]].spearhead.index;
        return logIdx === followerSpearhead
          || new Error(`Expected spearhead of ${followers[0]} to be ${logIdx}, but is ${followerSpearhead}.`);
      });

      { // followers[1] got the log entry before it was stopped, and followers[0] just got it now, so it should be committed.
        const status = log.status().participants[leader].response;
        assertIdentical(status.lastCommitStatus.reason, "NothingToCommit");
        assertIdentical(status.local.commitIndex, logIdx, `Commit index is ${status.local.commitIndex}, but should be exactly ${logIdx}`);
      }

      continueServer(followers[1]);
      waitForFailureOracleServerStatus(followers[1], IS_NOT_FAILED);
      {
        const status = log.status().participants[leader].response;
        assertIdentical(status.lastCommitStatus.reason, "NothingToCommit");
        assertIdentical(status.local.commitIndex, logIdx, `Commit index is ${status.local.commitIndex}, but should be exactly ${logIdx}`);
      }
      replicatedLogDeleteTarget(database, logId);
    },
  };
};

jsunity.run(replicatedLogRegressionSuite);
return jsunity.done();
