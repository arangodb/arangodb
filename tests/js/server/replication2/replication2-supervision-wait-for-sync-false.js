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
  stopServerWait,
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

      /*
        NOTE: The order of stops here is important.
        While one of the two servers is down, we are allowed
        to Increase this servers safeRebootID as long as we get
        a roundtrip with the other server. This means we
        can form a quorum with leader and the first stopped follower,
        in the case we do this update quick enough before the other server
        stops.
        This test continues follower[0] first. This server is clear
        to not be able to form a quorum with the leader. (It is required
        to have a dirty rebootId). If we would continue follower[1] first,
        we are depending on above race for the following test, we could
        commit with only leader and follower[1] while follower[0] is still down.
       */
      stopServerWait(followers[1]);
      stopServerWait(followers[0]);
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
        }
      );
      const {participants: {[leader]: {response: status}}} = log.status();
      // There is actually a race on the indexes in the log here, that has the same reason
      // why we need to be careful with stop / continue ordering of servers.
      // The leader will try to send a message as soon as it get's aware of a stopped server.
      // That is after the server is stopped, and agency times out on the servers heartbeat.
      // This message can be either before the expected logindex or after.
      // As we are shutting down 2 servers, we will have up to 2 messages in this race.
      // (Where the message of the first one only is highly unlikely)

      // We can be up to 2 entries further ten the expected logIndex
      assertTrue(status.local.spearhead.index < logIndex + 2, `Log Content: ${JSON.stringify(log.tail())}`);
      // We need to be at least on the expected logIndex
      assertTrue(status.local.spearhead.index >= logIndex, `Log Content: ${JSON.stringify(log.tail())}`);

      // We are unable to commit the insert while all servers are down
      assertTrue(status.local.commitIndex < logIndex);

      lh.triggerLeaderElection(database, log.id());

      lh.waitFor(() => {
        const {supervision: {response: {StatusReport}}} = log.status();
        if (!(StatusReport instanceof Array) || StatusReport.length < 1) {
          return Error('Still waiting for StatusReport to appear');
        }
        const type = StatusReport[StatusReport.length - 1].type;
        if (type !== 'LeaderElectionQuorumNotReached') {
          return Error('Expected LeaderElectionQuorumNotReached, but got ' + type);
        }
        return true;
      });

      continueServerWait(followers[0]);


      // Give the Supervision a little time. The result should *still* be
      // LeaderElectionQuorumNotReached, even with 2 servers online, as one of
      // them doesn't have a safe RebootId.
      internal.sleep(2);
      lh.waitFor(() => {
        const {supervision: {response: {StatusReport}}} = log.status();
        if (!(StatusReport instanceof Array) || StatusReport.length < 1) {
          return Error('Still waiting for StatusReport to appear');
        }
        const type = StatusReport[StatusReport.length - 1].type;
        if (type !== 'LeaderElectionQuorumNotReached') {
          return Error('Expected LeaderElectionQuorumNotReached, but got ' + type);
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
