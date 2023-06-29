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
const {assertEqual} = jsunity.jsUnity.assertions;
const arangodb = require("@arangodb");
const _ = require('lodash');
const db = arangodb.db;
const { deriveTestSuite } = require("@arangodb/test-helper-common");
const helper = require("@arangodb/testutils/replicated-logs-helper");
const lp = require("@arangodb/testutils/replicated-logs-predicates");
const {
  waitFor,
} = helper;

const database = "replication2_soft_write_concern_test_db";

const replicatedLogSuite = function ({waitForSync}) {
  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 3,
    waitForSync: waitForSync,
  };

  const replicationFactor = 3;

  const {setUpAll, tearDownAll, stopServerWait, continueServerWait, setUp, tearDown} =
    helper.testHelperFunctions(database, {replicationVersion: "2"});

  const createReplicatedLogAndWaitForLeader = function (database) {
    return helper.createReplicatedLog(database, targetConfig, replicationFactor);
  };

  return {
    setUpAll, tearDownAll,
    setUp, tearDown,

    testLogStartWithMissingFollower: function () {
      // TODO(CINFRA-793) this test does not work for waitForSync false
      let servers = _.sampleSize(helper.dbservers, replicationFactor);
      stopServerWait(servers[0]);

      // We should be able to create the log and use it even when one of the servers is unavailable.
      const {logId} = helper.createReplicatedLogInTarget(database, targetConfig, replicationFactor, servers);
      let log = db._replicatedLog(logId);
      let quorum = log.insert({foo: "bar"});
      assertEqual(quorum.result.quorum.quorum.length, 2);
    },

    testParticipantFailedOnInsert: function () {
      const {logId, servers, leader, term, followers} = createReplicatedLogAndWaitForLeader(database);
      // When a log is created, the followers haven't yet reported to Current.
      // The Supervision may immediately lower the effectiveWriteConcern,
      // because it assumes the participants are unavailable.
      // The following waits for the followers to report to Current.
      waitFor(lp.replicatedLogIsReady(database, logId, term, servers, leader));

      // Due to the writeConcern of 2, the leader may have established leadership when
      // only one of the followers was ready. While the committedParticipantsConfig's
      // effectiveWriteConcern becomes 2, the leadership has been established without
      // a 3rd participant. It is therefore possible that the next insert will be
      // committed with a quorum of 2.
      waitFor(() => {
        let {plan} = helper.readReplicatedLogAgency(database, logId);
        if (plan.participantsConfig.config.effectiveWriteConcern !== replicationFactor) {
          return Error(`effectiveWriteConcern not reached, found ${JSON.stringify(plan)}`);
        }
        return true;
      });

      // We need to wait for:
      // 1. The supervision to realize that all participants are ready and raise the
      // effectiveWriteConcern to 3.
      // 2. The leader's maintenance to realize that the effectiveWriteConcern has changed
      // and apply these changes to the activeParticipantsConfig. There is no way to wait on this,
      // as it is not reported to Current. We have to ping the log and wait for the
      // activeParticipantsConfig to be reflected in the committedParticipantsConfig.
      // We'll know everything is ready when the supervision's assumedWriteConcern is equal
      // to the replicationFactor.
      let log = db._replicatedLog(logId);
      waitFor(() => {
        log.ping();
        let {current} = helper.readReplicatedLogAgency(database, logId);
        if (current.supervision.assumedWriteConcern !== replicationFactor) {
          return Error(`assumedWriteConcern not reached, found ${JSON.stringify(current)}`);
        }
        return true;
      });

      let quorum = log.insert({foo: "bar"});
      assertEqual(quorum.result.quorum.quorum.length, 3, JSON.stringify({logId, leader, followers, quorum}));

      // now stop one server
      stopServerWait(followers[0]);

      quorum = log.insert({foo: "bar"});
      assertEqual(quorum.result.quorum.quorum.length, 2);

      continueServerWait(followers[0]);

      waitFor(function () {
        quorum = log.insert({foo: "bar"});
        if (quorum.result.quorum.quorum.length !== 3) {
          return Error(`quorum size not reached, found ${JSON.stringify(quorum.result)}`);
        }
        return true;
      });
    },
  };
};

let replicatedLogSuiteWfsTrue = {};
deriveTestSuite(replicatedLogSuite({waitForSync: true}), replicatedLogSuiteWfsTrue, "_WFS_TRUE");
jsunity.run(function() {
  return replicatedLogSuiteWfsTrue;
});
return jsunity.done();
