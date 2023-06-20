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
const helper = require("@arangodb/testutils/replicated-logs-helper");
const lp = require("@arangodb/testutils/replicated-logs-predicates");
const {
  waitFor,
  waitForReplicatedLogAvailable,
} = helper;

const database = "replication2_soft_write_concern_test_db";

const replicatedLogSuite = function () {

  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 3,
    waitForSync: false,
  };

  const {setUpAll, tearDownAll, stopServerWait, continueServerWait, setUp, tearDown} =
    helper.testHelperFunctions(database, {replicationVersion: "2"});

  const createReplicatedLogAndWaitForLeader = function (database) {
    return helper.createReplicatedLog(database, targetConfig);
  };

  return {
    setUpAll, tearDownAll,
    setUp, tearDown,

    testParticipantFailedOnInsert: function () {
      const {logId, servers, leader, term, followers} = createReplicatedLogAndWaitForLeader(database);
      // This test may not be stable. When a log is created, the followers
      // haven't yet reported to Current. The Supervision may immediately lower
      // the effectiveWriteConcern, because it assumes the participants are
      // unavailable.
      // The following waits for the followers to report to Current. However,
      // there might still be a race with the Supervision.
      // If the test fails because the quorum size is only 2, this is probably
      // the reason: Please report such a failure to team CINFRA, so we can try
      // again to stabilize the test.
      waitFor(lp.replicatedLogIsReady(database, logId, term, servers, leader));

      let log = db._replicatedLog(logId);
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

jsunity.run(replicatedLogSuite);
return jsunity.done();
