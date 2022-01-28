/*jshint strict: true */
/*global assertTrue, assertEqual*/
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
const arangodb = require("@arangodb");
const _ = require('lodash');
const {sleep} = require('internal');
const db = arangodb.db;
const ERRORS = arangodb.errors;
const helper = require("@arangodb/testutils/replicated-logs-helper");

const {
  waitFor,
  readReplicatedLogAgency,
  replicatedLogSetTarget,
  replicatedLogDeleteTarget,
  replicatedLogIsReady,
  dbservers, getParticipantsObjectForServers,
  nextUniqueLogId, allServersHealthy,
  registerAgencyTestBegin, registerAgencyTestEnd,
  replicatedLogLeaderEstablished,
  replicatedLogUpdateTargetParticipants,
  replicatedLogParticipantsFlag,
} = helper;

const database = "replication2_supervision_test_db";

const waitForReplicatedLogAvailable = function (id) {
  while (true) {
    try {
      let status = db._replicatedLog(id).status();
      const leaderId = status.leaderId;
      if (leaderId !== undefined && status.participants !== undefined &&
          status.participants[leaderId].role === "leader") {
        break;
      }
      console.info("replicated log not yet available");
    } catch (err) {
      const errors = [
        ERRORS.ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED.code,
        ERRORS.ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND.code
      ];
      if (errors.indexOf(err.errorNum) === -1) {
        throw err;
      }
    }

    sleep(1);
  }
};

const replicatedLogLeaderElectionFailed = function (database, logId, term, servers) {
  return function () {
    let {current} = readReplicatedLogAgency(database, logId);
    if (current === undefined) {
      return Error("current not yet defined");
    }

    if (current.supervision === undefined || current.supervision.election === undefined) {
      return Error("supervision not yet updated");
    }

    let election = current.supervision.election;
    if (election.term !== term) {
      return Error("supervision report not yet available for current term; "
          + `found = ${election.term}; expected = ${term}`);
    }

    if (servers !== undefined) {
      // wait for all specified servers to have the proper error code
      for (let x of Object.keys(servers)) {
        if (election.details[x] === undefined) {
          return Error(`server ${x} not in election result`);
        }
        let code = election.details[x].code;
        if (code !== servers[x]) {
          return Error(`server ${x} reported with code ${code}, expected ${servers[x]}`);
        }
      }
    }

    return true;
  };
};

const replicatedLogSupervisionError = function (database, logId, errorCode) {
  return function () {
    let {current} = readReplicatedLogAgency(database, logId);

    if (current.supervision === undefined) {
      return Error(`supervision not yet defined`);
    }
    if (current.supervision.error === undefined) {
      return Error(`no error reported in supervision`);
    }
    if (current.supervision.error.code !== errorCode) {
      return Error(`reported supervision errorCode ${current.supervision.error.code} not as expected ${errorCode}`);
    }
    if (current.supervision.error.code === errorCode) {
      return true;
    }
    return false;
  };
};

const replicatedLogSuite = function () {

  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    replicationFactor: 3,
    waitForSync: false,
  };

  const {setUpAll, tearDownAll, stopServer, continueServer, resumeAll} = (function () {
    let previousDatabase, databaseExisted = true;
    let stoppedServers = {};
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
          throw Error(`{serverId} already stopped`);
        }
        helper.stopServer(serverId);
        stoppedServers[serverId] = true;
      },
      continueServer: function (serverId) {
        if (stoppedServers[serverId] === undefined) {
          throw Error(`{serverId} not stopped`);
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

  const createReplicatedLog = function (database) {
    const logId = nextUniqueLogId();
    const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
    const leader = servers[0];
    const term = 1;
    const followers = _.difference(servers, [leader]);
    replicatedLogSetTarget(database, logId, {
      id: logId,
      config: targetConfig,
      leader,
      participants: getParticipantsObjectForServers(servers),
    });
    return {logId, servers, leader, term, followers};
  };

  const getReplicatedLogLeaderPlan = function (database, logId) {
    let {plan} = readReplicatedLogAgency(database, logId);
    if (!plan.currentTerm) {
      throw Error("no current term in plan");
    }
    if (!plan.currentTerm.leader) {
      throw Error("current term has no leader");
    }
    const leader = plan.currentTerm.leader.serverId;
    const term = plan.currentTerm.term;
    return {leader, term};
  };

  const createReplicatedLogAndWaitForLeader = function (database) {
    const logId = nextUniqueLogId();
    const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
    replicatedLogSetTarget(database, logId, {
      id: logId,
      config: targetConfig,
      participants: getParticipantsObjectForServers(servers),
      supervision: {maxActionsTraceLength: 20},
    });

    waitFor(replicatedLogLeaderEstablished(database, logId, undefined, servers));

    const {leader, term} = getReplicatedLogLeaderPlan(database, logId);
    const followers = _.difference(servers, [leader]);
    return {logId, servers, leader, term, followers};
  };

  const setReplicatedLogLeaderTarget = function (database, logId, leader) {
    let {target} = readReplicatedLogAgency(database, logId);
    if (leader !== null) {
      target.leader = leader;
    } else {
      delete target.leader;
    }
    replicatedLogSetTarget(database, logId, target);
  };

  return {
    setUpAll, tearDownAll,
    setUp: registerAgencyTestBegin,
    tearDown: function (test) {
      resumeAll();
      waitFor(allServersHealthy());
      registerAgencyTestEnd(test);
    },

    // This test stops the leader and a follower, then waits for a failover to happen
    testCheckSimpleFailover: function () {
      const {logId, followers, leader, term} = createReplicatedLogAndWaitForLeader(database);
      waitForReplicatedLogAvailable(logId);

      // now stop one server
      stopServer(followers[0]);

      // we should still be able to write
      {
        let log = db._replicatedLog(logId);
        // we have to insert two log entries here, reason:
        // Even though followers[0] is stopped, it will receive the AppendEntries message for log index 1.
        // It will stay in its tcp input queue. So when the server is continued below it will process
        // this message. However, the leader sees this message as still in flight and thus will never
        // send any updates again. By inserting yet another log entry, we can make sure that servers[2]
        // is the only server that has received log index 2.
        log.insert({foo: "bar"});
        let quorum = log.insert({foo: "bar"});
        assertTrue(quorum.result.quorum.quorum.indexOf(followers[0]) === -1);
      }

      // now stop the leader
      stopServer(leader);

      // since writeConcern is 2, there is no way a new leader can be elected
      waitFor(replicatedLogLeaderElectionFailed(database, logId, term + 1, {
        [leader]: 1,
        [followers[0]]: 1,
        [followers[1]]: 0,
      }));

      {
        // check election result
        const {current} = readReplicatedLogAgency(database, logId);
        const election = current.supervision.election;
        assertEqual(election.term, term + 1);
        assertEqual(election.participantsRequired, 2);
        assertEqual(election.participantsAvailable, 1);
        const detail = election.details;
        assertEqual(detail[leader].code, 1);
        assertEqual(detail[followers[0]].code, 1);
        assertEqual(detail[followers[1]].code, 0);
      }

      // now resume, followers[1 has to become leader, because it's the only server with log entry 1 available
      continueServer(followers[0]);
      waitFor(replicatedLogIsReady(database, logId, term + 2, [followers[0], followers[1]], followers[1]));

      replicatedLogDeleteTarget(database, logId);

      continueServer(leader);
    },

    // This test requests a specific server as leader in Target
    testChangeLeader: function () {
      const {logId, servers, term, followers} = createReplicatedLogAndWaitForLeader(database);

      // now change the leader
      const newLeader = _.sample(followers);
      setReplicatedLogLeaderTarget(database, logId, newLeader);
      waitFor(replicatedLogIsReady(database, logId, term, servers, newLeader));

      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [newLeader]: {excluded: false, forced: false},
      }));
      {
        const {current} = readReplicatedLogAgency(database, logId);
        const actions = current.supervision.actions;
        // we expect the last three actions to be
        //  3. update participant flags with leader.forced = true
        //  2. dictate leadership with new leader
        //  3. update participant flags with leader.forced = false
        {
          const action = _.nth(actions, -3).desc;
          assertEqual(action.type, 'UpdateParticipantFlags');
          assertEqual(action.participant, newLeader);
          assertEqual(action.flags, {excluded: false, forced: true});
        }
        {
          const action = _.nth(actions, -2).desc;
          assertEqual(action.type, 'DictateLeaderAction');
          assertEqual(action.newTerm.leader.serverId, newLeader);
        }
        {
          const action = _.nth(actions, -1).desc;
          assertEqual(action.type, 'UpdateParticipantFlags');
          assertEqual(action.participant, newLeader);
          assertEqual(action.flags, {excluded: false, forced: false});
        }
      }
      replicatedLogDeleteTarget(database, logId);
    },

    // This test adds and removes an excluded flag to a server in Target
    // and waits for the corresponding action in Current
    testAddExcludedFlag: function () {
      const {logId, followers} = createReplicatedLogAndWaitForLeader(database);

      // now add the excluded flag to one of the servers
      const server = _.sample(followers);
      replicatedLogUpdateTargetParticipants(database, logId, {
        [server]: {excluded: true},
      });

      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [server]: {excluded: true, forced: false},
      }));

      // now remove the flag again
      replicatedLogUpdateTargetParticipants(database, logId, {
        [server]: {excluded: false},
      });

      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [server]: {excluded: false, forced: false},
      }));

      replicatedLogDeleteTarget(database, logId);
    },

    // This test adds a new participant to the replicated log
    testAddParticipant: function () {
      const {logId, servers} = createReplicatedLogAndWaitForLeader(database);

      // now add a new server, but with excluded flag
      const newServer = _.sample(_.difference(dbservers, servers));
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newServer]: {excluded: true},
      });

      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [newServer]: {excluded: true, forced: false},
      }));

      replicatedLogDeleteTarget(database, logId);
    },

    // This test first makes a follower excluded and then asks for this follower
    // to become the leader. It then removed the excluded flag and expects the
    // leadership to be transferred.
    testChangeLeaderWithExcluded: function () {
      const {logId, servers, term, followers} = createReplicatedLogAndWaitForLeader(database);

      // first make the new leader excluded
      const newLeader = followers[0];
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newLeader]: {excluded: true},
      });
      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [newLeader]: {excluded: true, forced: false},
      }));

      // new we try to change to the new leader
      setReplicatedLogLeaderTarget(database, logId, newLeader);

      // nothing should happen (supervision should not elect leader)
      waitFor(replicatedLogLeaderElectionFailed(database, logId, term, {
        [newLeader]: 3,
      }));

      replicatedLogUpdateTargetParticipants(database, logId, {
        [newLeader]: {excluded: false},
      });
      waitFor(replicatedLogIsReady(database, logId, term, servers, newLeader));

      replicatedLogDeleteTarget(database, logId);
    },

    // This test excludes follower A, waits for the flags to be committed and
    // requests that follower B shall become the leader.
    testChangeLeaderWithExcludedOtherFollower: function () {
      const {logId, servers, term, followers} = createReplicatedLogAndWaitForLeader(database);

      // first make one follower excluded
      const excludedFollower = followers[0];
      replicatedLogUpdateTargetParticipants(database, logId, {
        [excludedFollower]: {excluded: true},
      });
      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [excludedFollower]: {excluded: true, forced: false},
      }));

      // new we try to change to the new leader
      const newLeader = followers[1];
      setReplicatedLogLeaderTarget(database, logId, newLeader);
      waitFor(replicatedLogIsReady(database, logId, term, servers, newLeader));

      replicatedLogDeleteTarget(database, logId);
    },

    // This tests completely replaces a follower in Target with a different
    // server
    testReplaceFollowerWithNewFollower: function () {
      const {logId, servers, term, followers, leader} = createReplicatedLogAndWaitForLeader(database);

      const oldServer = _.sample(followers);
      const newServer = _.sample(_.difference(dbservers, servers));
      {
        let {target} = readReplicatedLogAgency(database, logId);
        // delete old server from target
        delete target.participants[oldServer];
        // add new server to target
        target.participants[newServer] = {excluded: true};
        replicatedLogSetTarget(database, logId, target);
      }

      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [newServer]: {excluded: true, forced: false},
        [oldServer]: null,
      }));

      // now remove the excluded flag
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newServer]: {excluded: false},
      });
      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [newServer]: {excluded: false, forced: false},
        [oldServer]: null,
      }));

      // we expect still to have the same term and same leader
      waitFor(replicatedLogIsReady(database, logId, term, [..._.difference(servers, oldServer), newServer], leader));
    },

    // This tests replaces the leader in Target with a new server that has the
    // excluded flag set. It then waits for the replicated log to converge and
    // then removes the excluded flag.
    testReplaceLeaderWithNewFollower: function () {
      const {logId, servers, term, leader, followers} = createReplicatedLogAndWaitForLeader(database);

      const newServer = _.sample(_.difference(dbservers, servers));
      {
        let {target} = readReplicatedLogAgency(database, logId);
        // delete old leader from target
        delete target.participants[leader];
        target.participants[newServer] = {excluded: true};
        replicatedLogSetTarget(database, logId, target);
      }
      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [newServer]: {excluded: true, forced: false},
        [leader]: null,
      }));

      // now remove the excluded flag
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newServer]: {excluded: false},
      });
      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [newServer]: {excluded: false, forced: false},
        [leader]: null,
      }));

      // we expect to have a new leader and the new follower
      waitFor(replicatedLogLeaderEstablished(database, logId, term + 1, [...followers, newServer]));
    },

    // This test replaces the old leader in target with a new server that is excluded
    // and additionally requests that this new server shall become the leader.
    // It expects the supervision to fail and then removes the excluded flag.
    testChangeLeaderToNewFollower: function () {
      const {logId, servers, term, leader, followers} = createReplicatedLogAndWaitForLeader(database);

      const newServer = _.sample(_.difference(dbservers, servers));
      {
        let {target} = readReplicatedLogAgency(database, logId);
        // request this server to become leader
        target.leader = newServer;
        // delete old leader from target
        delete target.participants[leader];
        target.participants[newServer] = {excluded: true};
        replicatedLogSetTarget(database, logId, target);
      }
      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [newServer]: {excluded: true, forced: false},
      //  [leader]: null,
      }));

      waitFor(replicatedLogLeaderElectionFailed(database, logId, term + 1, {
        [newServer]: 3,
        [servers[0]]: 0,
        [servers[1]]: 0,
        [servers[2]]: 0,
      }));

      // now remove the excluded flag
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newServer]: {excluded: false},
      });
      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [newServer]: {excluded: false, forced: false},
        [leader]: null,
      }));
      waitFor(replicatedLogIsReady(database, logId, term + 1, [...followers, newServer], newServer));
    },

    // This tests requests a non-server as leader and expects the
    // supervision to fail
    testChangeLeaderToNonServer: function () {
      const {logId, servers, term, leader} = createReplicatedLogAndWaitForLeader(database);

      // now change the leader
      const otherServer = "Foo42";
      setReplicatedLogLeaderTarget(database, logId, otherServer);
      // The supervision should complain about the server not being a leader

      const errorCode = 0; // TARGET_LEADER_INVALID
      waitFor(replicatedLogSupervisionError(database, logId, errorCode));

      // nothing should have happend
      waitFor(replicatedLogIsReady(database, logId, term, servers, leader));
      replicatedLogDeleteTarget(database, logId);
    },

    // This tests requests a non-participant server as leader and expects the
    // supervision to fail
    testChangeLeaderToNonFollower: function () {
      const {logId, servers, term, leader} = createReplicatedLogAndWaitForLeader(database);

      // now change the leader
      const otherServer = _.sample(_.difference(dbservers, servers));
      setReplicatedLogLeaderTarget(database, logId, otherServer);
      // The supervision should complain about the server not being a leader

      const errorCode = 0; // TARGET_LEADER_INVALID
      waitFor(replicatedLogSupervisionError(database, logId, errorCode));
      //
      // nothing should have happend
      waitFor(replicatedLogIsReady(database, logId, term, servers, leader));
      replicatedLogDeleteTarget(database, logId);
    },

    testLogStatus: function () {
      const {logId, servers, leader, term, followers} = createReplicatedLogAndWaitForLeader(database);

      waitFor(replicatedLogIsReady(database, logId, term, servers, leader));
      waitForReplicatedLogAvailable(logId);

      let log = db._replicatedLog(logId);
      let globalStatus = log.status();
      assertEqual(globalStatus.leaderId, leader);
      let localStatus = helper.getLocalStatus(database, logId, leader);
      assertEqual(localStatus.role, "leader");
      assertEqual(globalStatus.participants[leader], localStatus);
      localStatus = helper.getLocalStatus(database, logId, followers[1]);
      assertEqual(localStatus.role, "follower");

      stopServer(leader);
      stopServer(followers[0]);

      waitFor(replicatedLogLeaderElectionFailed(database, logId, term + 1, {
        [leader]: 1,
        [followers[0]]: 1,
        [followers[1]]: 0,
      }));

      globalStatus = log.status();
      const {current} = readReplicatedLogAgency(database, logId);
      const election = current.supervision.election;

      assertEqual(election, globalStatus.supervision.election);

      replicatedLogDeleteTarget(database, logId);
      continueServer(leader);
      continueServer(servers[1]);
    },
  };
};

jsunity.run(replicatedLogSuite);
return jsunity.done();
