/*jshint globalstrict:true, strict:true, esnext: true */

'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const lh = require('@arangodb/testutils/replicated-logs-helper');
const lp = require('@arangodb/testutils/replicated-logs-predicates');
const jsunity = require('jsunity');
const {db} = require('@arangodb');
const _ = require('lodash');

const {
  assertTrue,
  assertFalse,
  assertEqual,
  assertInstanceOf,
} = jsunity.jsUnity.assertions;

const database = 'replication2_wfs_false_test_db';

const {
  setUpAll,
  tearDownAll,
  setUp,
  tearDown,
  stopServer,
  continueServer,
  stopServersWait,
  continueServerWait,
} = lh.testHelperFunctions(database);

const replicatedLogWaitForSyncFalseSupervisionSuite = function () {
  const config = {
    writeConcern: 2,
    softWriteConcern: 2,
    waitForSync: false,
  };
  const numberOfServers = 3;

  return {
    setUpAll,
    tearDownAll,
    setUp,
    tearDown,

    testWaitForSyncFalseSuccessfulElection: function () {
      /** @type {ArangoReplicatedLog} */
      const log = db._createReplicatedLog({
        config,
        numberOfServers,
      });
      const {plan} = lh.readReplicatedLogAgency(database, log.id());
      const oldLeader = plan.currentTerm.leader.serverId;
      const servers = Object.keys(plan.participantsConfig.participants);
      const followers = _.without(servers, oldLeader);

      lh.stopServer(oldLeader);

      lh.waitFor(lp.replicatedLogLeaderPlanChanged(database, log.id(), oldLeader));

      // insert a log entry
      const {
        index,
        result,
      } = log.insert({value: 1});
      assertTrue(result.commitIndex >= index);
      assertEqual(result.quorum.quorum.sort(), followers.sort());

      lh.continueServer(oldLeader);
    },

    testWaitForSyncFalseElectionFails: function () {
      /** @type {ArangoReplicatedLog} */
      const log = db._createReplicatedLog({
        config,
        numberOfServers,
      });
      const {plan} = lh.readReplicatedLogAgency(database, log.id());
      const leader = plan.currentTerm.leader.serverId;
      const servers = Object.keys(plan.participantsConfig.participants);
      const followers = _.without(servers, leader);

      stopServersWait(followers);
      const logIndex = log.insert({'I will': 'be missed'}, {dontWaitForCommit: true}).index;
      lh.waitFor(() => {
          // As both followers are stopped and writeConcern=2, the log cannot currently commit.
          const {participants: {[leader]: {response: status}}} = log.status();
          if (status.lastCommitStatus.reason !== 'QuorumSizeNotReached') {
            return new Error(`Expected status to be QuorumSizeNotReached, but is ${status.lastCommitStatus.reason}. Full status is: ${JSON.stringify(status)}`);
          }
          if (!(status.local.commitIndex < logIndex)) {
            return new Error(`Commit index is ${status.local.commitIndex}, but should be lower than ${JSON.stringify(logIndex)}.`);
          }
          return true;
        },
      );
      const {participants: {[leader]: {response: status}}} = log.status();
      assertEqual(status.local.spearhead.index, logIndex);
      assertTrue(status.local.commitIndex < logIndex);

      lh.triggerLeaderElection(database, log.id());

      lh.waitFor(() => {
        const {supervision: {response: {StatusReport}}} = log.status();
        if (!(StatusReport instanceof Array) || StatusReport.length < 1) {
          return Error('Still waiting for StatusReport to appear');
        }
        const type = StatusReport[StatusReport.length - 1].type;
        if (type !== 'LeaderElectionQuorumNotReached') {
          return Error('Expected LeaderElectionQuorumNotReached, but got ' + StatusReport.type);
        }
        return true;
      });

      continueServerWait(followers[0]);

      // Give the Supervision a little time. The result should *still* be
      // LeaderElectionQuorumNotReached, even with 2 servers online, as one of
      // them doesn't have a safe RebootId.
      internal.sleep(2);
      lh.waitFor(() => {
        let foo;
        const {supervision: {response: {StatusReport}}} = foo = log.status();
        if (!(StatusReport instanceof Array) || StatusReport.length < 1) {
          return Error('Still waiting for StatusReport to appear');
        }
        const type = StatusReport[StatusReport.length - 1].type;
        if (type !== 'LeaderElectionQuorumNotReached') {
          return Error('Expected LeaderElectionQuorumNotReached, but got ' + StatusReport.type);
        }
        return true;
      });

      let success = false;
      try {
        log.insert({refuse: "me"});
        success = true;
      } catch (e) {
        assertInstanceOf(Error, e);
        const {code, message} = internal.errors.ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED;
        assertEqual(code, e.errorNum);
        assertEqual(message, e.errorMessage);
      }
      assertFalse(success);

      continueServerWait(followers[1]);

      lh.waitFor(lp.replicatedLogLeaderEstablished(database, log.id(), undefined, servers));

      // insert a log entry
      const {
        index,
        result,
      } = log.insert({save: 'me'});
      assertTrue(result.commitIndex >= index);
    },
  };
};


jsunity.run(replicatedLogWaitForSyncFalseSupervisionSuite);
return jsunity.done();
