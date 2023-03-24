/*jshint strict: true */
/*global assertTrue, assertEqual*/
"use strict";

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

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const _ = require("lodash");
const db = arangodb.db;
const helper = require("@arangodb/testutils/replicated-logs-helper");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");

const {
  waitFor,
  readReplicatedLogAgency,
  replicatedLogSetTarget,
  replicatedLogDeleteTarget,
  dbservers,
  registerAgencyTestBegin,
  registerAgencyTestEnd,
  replicatedLogUpdateTargetParticipants,
  waitForReplicatedLogAvailable,
} = helper;
const {
  replicatedLogIsReady,
  replicatedLogLeaderEstablished,
  replicatedLogParticipantsFlag,
  replicatedLogTargetVersion,
} = lpreds;

const database = "replication2_supervision_test_db";

const replicatedLogLeaderElectionFailed = function (
    database,
    logId,
    term,
    servers
) {
  return function () {
    let {current} = readReplicatedLogAgency(database, logId);
    if (current === undefined) {
      return Error("current not yet defined");
    }

    if (
        current.supervision === undefined ||
        current.supervision.StatusReport === undefined
    ) {
      return Error("supervision not yet updated");
    }

    if (!Array.isArray(current.supervision.StatusReport)) {
      return Error("StatusReport is not an array");
    }

    if (current.supervision.StatusReport.length === 0) {
      return Error("StatusReport is empty");
    }

    let statusReport = current.supervision.StatusReport[0];
    if (statusReport.type !== "LeaderElectionQuorumNotReached") {
      return Error(
          "StatusReport is not the correct type " +
          `found = ${statusReport.type}, expected LeaderElectionQuorumNotReached`
      );
    }

    let election = statusReport.detail.election;
    if (election.term !== term) {
      return Error(
          "supervision report not yet available for current term; " +
          `found = ${election.term}; expected = ${term}`
      );
    }

    if (servers !== undefined) {
      // wait for all specified servers to have the proper error code
      for (let x of Object.keys(servers)) {
        if (election.details[x] === undefined) {
          return Error(`server ${x} not in election result`);
        }
        let code = election.details[x].code;
        if (code !== servers[x]) {
          return Error(
              `server ${x} reported with code ${code}, expected ${servers[x]}`
          );
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
    if (
        current.supervision.StatusReport === undefined ||
        current.supervision.StatusReport.length === 0
    ) {
      return Error(`no StatusReport available supervision`);
    }
    let statusReport = current.supervision.StatusReport[0];
    if (statusReport.type !== errorCode) {
      return Error(
          `reported supervision errorCode ${statusReport.type} not as expected ${errorCode}`
      );
    }
    if (statusReport.type === errorCode) {
      return true;
    }
    return false;
  };
};

const replicatedLogSuite = function () {
  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    waitForSync: false,
  };

  const {setUpAll, tearDownAll, stopServer, continueServer, resumeAll} =
      (function () {
        let previousDatabase,
            databaseExisted = true;
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
              throw Error(`${serverId} already stopped`);
            }
            helper.stopServer(serverId);
            stoppedServers[serverId] = true;
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
          },
        };
      })();

  const createReplicatedLogAndWaitForLeader = function (database) {
    return helper.createReplicatedLog(database, targetConfig);
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

  const setReplicatedLogVersionTarget = function (database, logId, version) {
    let {target} = readReplicatedLogAgency(database, logId);
    target.version = version;
    replicatedLogSetTarget(database, logId, target);
  };

  return {
    setUpAll,
    tearDownAll,
    setUp: registerAgencyTestBegin,
    tearDown: function (test) {
      resumeAll();
      waitFor(lpreds.allServersHealthy());
      registerAgencyTestEnd(test);
    },

    // This test stops the leader and a follower, then waits for a failover to happen
    testConvergedToVersion: function () {
      const {logId, followers, leader, term} =
          createReplicatedLogAndWaitForLeader(database);

      const targetVersion = 15;

      waitForReplicatedLogAvailable(logId);

      setReplicatedLogVersionTarget(database, logId, targetVersion);

      waitFor(replicatedLogTargetVersion(database, logId, targetVersion));
    },

    // This test stops the leader and a follower, then waits for a failover to happen
    testCheckSimpleFailover: function () {
      const {logId, followers, leader, term} =
          createReplicatedLogAndWaitForLeader(database);
      waitForReplicatedLogAvailable(logId);

      // This important to make sure that all followers have responded to all append entries messages
      // upto now. Otherwise, the insert below doesn't work as expect.
      waitFor(lpreds.replicatedLogReplicationCompleted(database, logId));
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
      waitFor(
          replicatedLogLeaderElectionFailed(database, logId, term + 1, {
            [leader]: 1,
            [followers[0]]: 1,
            [followers[1]]: 0,
          })
      );

      {
        // check election result
        const {current} = readReplicatedLogAgency(database, logId);
        const election = current.supervision.StatusReport[0].detail.election;
        assertEqual(election.term, term + 1);
        assertEqual(election.participantsRequired, 2);
        assertEqual(election.participantsAvailable, 1);
        const detail = election.details;
        assertEqual(detail[leader].code, 1);
        assertEqual(detail[followers[0]].code, 1);
        assertEqual(detail[followers[1]].code, 0);
      }
      // now resume, followers[1] has to become leader, because it's the only server with log entry 1 available
      continueServer(followers[0]);
      waitFor(
          replicatedLogIsReady(
              database,
              logId,
              term + 2,
              [followers[0], followers[1]],
              followers[1]
          )
      );

      replicatedLogDeleteTarget(database, logId);

      continueServer(leader);
    },

    // This test requests a specific server as leader in Target
    testChangeLeader: function () {
      const {logId, servers, term, followers} =
          createReplicatedLogAndWaitForLeader(database);

      // now change the leader
      const newLeader = _.sample(followers);
      setReplicatedLogLeaderTarget(database, logId, newLeader);
      waitFor(replicatedLogIsReady(database, logId, term, servers, newLeader));

      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [newLeader]: {
              allowedAsLeader: true,
              allowedInQuorum: true,
              forced: false,
            },
          })
      );
      {
        const {current} = readReplicatedLogAgency(database, logId);
        const actions = current.actions;
        // we expect the last three actions to be
        //  3. update participant flags with leader.forced = true
        //  2. dictate leadership with new leader
        //  3. update participant flags with leader.forced = false
        {
          const action = _.nth(actions, -3).desc;
          assertEqual(action.type, "UpdateParticipantFlagsAction");
          assertEqual(action.participant, newLeader);
          assertEqual(action.flags, {
            allowedAsLeader: true,
            allowedInQuorum: true,
            forced: true,
          });
        }
        {
          const action = _.nth(actions, -2).desc;
          assertEqual(action.type, "SwitchLeaderAction");
          assertEqual(action.leader.serverId, newLeader);
        }
        {
          const action = _.nth(actions, -1).desc;
          assertEqual(action.type, "UpdateParticipantFlagsAction");
          assertEqual(action.participant, newLeader);
          assertEqual(action.flags, {
            allowedAsLeader: true,
            allowedInQuorum: true,
            forced: false,
          });
        }
      }
      replicatedLogDeleteTarget(database, logId);
    },

    // This test adds and removes an excluded flag to a server in Target
    // and waits for the corresponding action in Current
    testRemoveAllowedInQuorumFlag: function () {
      const {logId, followers} =
          createReplicatedLogAndWaitForLeader(database);

      // now add the excluded flag to one of the servers
      const server = _.sample(followers);
      replicatedLogUpdateTargetParticipants(database, logId, {
        [server]: {allowedInQuorum: false},
      });

      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [server]: {
              allowedInQuorum: false,
              forced: false,
              allowedAsLeader: true,
            },
          })
      );

      // now remove the flag again
      replicatedLogUpdateTargetParticipants(database, logId, {
        [server]: {allowedInQuorum: true},
      });

      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [server]: {
              allowedInQuorum: true,
              forced: false,
              allowedAsLeader: true,
            },
          })
      );

      replicatedLogDeleteTarget(database, logId);
    },

    // This test adds a new participant to the replicated log
    testAddParticipant: function () {
      const {logId, servers} = createReplicatedLogAndWaitForLeader(database);

      // now add a new server, but with excluded flag
      const newServer = _.sample(_.difference(dbservers, servers));
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newServer]: {allowedInQuorum: false, allowedAsLeader: false},
      });

      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [newServer]: {
              allowedInQuorum: false,
              allowedAsLeader: false,
              forced: false,
            },
          })
      );

      replicatedLogDeleteTarget(database, logId);
    },

    // This test removes a participant from the replicated log
    testRemoveFollowerParticipant: function () {
      const {logId, servers, followers} =
          createReplicatedLogAndWaitForLeader(database);

      // first add a new server, but with excluded flag
      const newServer = _.sample(_.difference(dbservers, servers));
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newServer]: {allowedInQuorum: true, allowedAsLeader: true},
      });

      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [newServer]: {
              allowedInQuorum: true,
              allowedAsLeader: true,
              forced: false,
            },
          })
      );

      const removedServer = _.sample(followers);
      replicatedLogUpdateTargetParticipants(database, logId, {
        [removedServer]: null,
      });

      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [removedServer]: null,
          })
      );

      {
        const {current} = readReplicatedLogAgency(database, logId);
        const actions = current.actions;
        // we expect the last actions to be
        //  1. remove the server
        //  2. dictate leadership with new leader
        {
          const action = _.nth(actions, -2).desc;
          assertEqual(action.type, "UpdateParticipantFlagsAction");
          assertEqual(action.flags, {
            allowedAsLeader: true,
            allowedInQuorum: false,
            forced: false,
          });
        }
        {
          const action = _.nth(actions, -1).desc;
          assertEqual(action.type, "RemoveParticipantFromPlanAction");
          assertEqual(action.participant, removedServer);
        }
      }

      replicatedLogDeleteTarget(database, logId);
    },

    // This test first sets a new leader and waits for the leader exchange to take place.
    // Then the new leader is stopped, thus an election is triggered.
    // Although the now failed ex-leader remains in target, the election is expected to succeed.
    testChangeLeaderWithFailed: function () {
      const {logId, servers, term, followers} = createReplicatedLogAndWaitForLeader(database);

      const setLeader = followers[0];
      setReplicatedLogLeaderTarget(database, logId, setLeader);
      waitFor(replicatedLogIsReady(database, logId, term, servers, setLeader));
      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [setLeader]: {allowedAsLeader: true, allowedInQuorum: true, forced: false},
      }));

      // Stop current leader and wait for the leader to be changed
      stopServer(setLeader);

      // Supervision should notice that the target leader is failed and should not try to set it.
      const errorCode = "TargetLeaderFailed";
      waitFor(replicatedLogSupervisionError(database, logId, errorCode));

      waitFor(lpreds.replicatedLogLeaderPlanChanged(database, logId, setLeader));
      let {newLeader} = helper.getReplicatedLogLeaderPlan(database, logId);
      waitFor(replicatedLogIsReady(database, logId, term, servers, newLeader));

      replicatedLogUpdateTargetParticipants(database, logId, {
        [setLeader]: {allowedInQuorum: true, allowedAsLeader: true},
      });

      replicatedLogDeleteTarget(database, logId);
    },

    // This test first makes a follower excluded and then asks for this follower
    // to become the leader. It then removed the excluded flag and expects the
    // leadership to be transferred.
    testChangeLeaderWithExcluded: function () {
      const {logId, servers, term, followers} =
          createReplicatedLogAndWaitForLeader(database);

      // first make the new leader excluded
      const newLeader = followers[0];
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newLeader]: {allowedInQuorum: false, allowedAsLeader: false},
      });
      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [newLeader]: {
              allowedInQuorum: false,
              allowedAsLeader: false,
              forced: false,
            },
          })
      );

      // new we try to change to the new leader
      setReplicatedLogLeaderTarget(database, logId, newLeader);

      // nothing should happen (supervision should not elect leader)
      const errorCode = "TargetLeaderExcluded";
      waitFor(replicatedLogSupervisionError(database, logId, errorCode));

      replicatedLogUpdateTargetParticipants(database, logId, {
        [newLeader]: {allowedInQuorum: true, allowedAsLeader: true},
      });
      waitFor(replicatedLogIsReady(database, logId, term, servers, newLeader));

      replicatedLogDeleteTarget(database, logId);
    },

    // This test excludes follower A, waits for the flags to be committed and
    // requests that follower B shall become the leader.
    testChangeLeaderWithExcludedOtherFollower: function () {
      const {logId, servers, term, followers} =
          createReplicatedLogAndWaitForLeader(database);

      // first make one follower excluded
      const excludedFollower = followers[0];
      replicatedLogUpdateTargetParticipants(database, logId, {
        [excludedFollower]: {allowedAsLeader: false},
      });
      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [excludedFollower]: {
              allowedInQuorum: true,
              allowedAsLeader: false,
              forced: false,
            },
          })
      );

      // new we try to change to the new leader
      const newLeader = followers[1];
      setReplicatedLogLeaderTarget(database, logId, newLeader);
      waitFor(replicatedLogIsReady(database, logId, term, servers, newLeader));

      replicatedLogDeleteTarget(database, logId);
    },

    // This tests completely replaces a follower in Target with a different
    // server
    testReplaceFollowerWithNewFollower: function () {
      const {logId, servers, term, followers, leader} =
          createReplicatedLogAndWaitForLeader(database);

      const oldServer = _.sample(followers);
      const newServer = _.sample(_.difference(dbservers, servers));
      {
        let {target} = readReplicatedLogAgency(database, logId);
        // delete old server from target
        delete target.participants[oldServer];
        // add new server to target
        target.participants[newServer] = {
          allowedAsLeader: false,
          allowedInQuorum: false,
        };
        replicatedLogSetTarget(database, logId, target);
      }

      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [newServer]: {
              allowedAsLeader: false,
              allowedInQuorum: false,
              forced: false,
            },
            [oldServer]: null,
          })
      );

      // now remove the excluded flag
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newServer]: {allowedAsLeader: true, allowedInQuorum: true},
      });
      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [newServer]: {
              allowedAsLeader: true,
              allowedInQuorum: true,
              forced: false,
            },
            [oldServer]: null,
          })
      );

      // we expect still to have the same term and same leader
      waitFor(
          replicatedLogIsReady(
              database,
              logId,
              term,
              [..._.difference(servers, oldServer), newServer],
              leader
          )
      );
    },

    // This tests removes the participant that is currently the leader from Target
    // while simultaneously adding a new server that has the excluded flag set.
    // It then waits for the replicated log to converge and
    // then removes the excluded flag.
    testReplaceLeaderWithNewFollower: function () {
      const {logId, servers, term, leader, followers} =
          createReplicatedLogAndWaitForLeader(database);

      const newServer = _.sample(_.difference(dbservers, servers));
      {
        let {target} = readReplicatedLogAgency(database, logId);
        // delete old leader from target
        delete target.participants[leader];
        target.participants[newServer] = {
          allowedAsLeader: false,
          allowedInQuorum: false,
        };
        replicatedLogSetTarget(database, logId, target);
      }

      // Wait for the new participant to appear, while excluded
      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [newServer]: {
              allowedAsLeader: false,
              allowedInQuorum: false,
              forced: false,
            },
          })
      );

      // now remove the excluded flag
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newServer]: {allowedAsLeader: true, allowedInQuorum: true},
      });

      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [newServer]: {
              allowedAsLeader: true,
              allowedInQuorum: true,
              forced: false,
            },
          })
      );

      // we expect to have a new leader and the new follower
      waitFor(
          replicatedLogLeaderEstablished(database, logId, term + 1, [
            ...followers,
            newServer,
          ])
      );
    },

    // This test adds a new participant to the replicated log
    // and requests that this new participant shall become the leader.
    testChangeLeaderToNewFollower: function () {
      const {logId, servers, term, leader, followers} =
          createReplicatedLogAndWaitForLeader(database);

      const newServer = _.sample(_.difference(dbservers, servers));
      {
        let {target} = readReplicatedLogAgency(database, logId);
        // request this server to become leader
        target.leader = newServer;
        // delete old leader from target
        delete target.participants[leader];
        target.participants[newServer] = {
          allowedInQuorum: false,
          allowedAsLeader: false,
        };
        replicatedLogSetTarget(database, logId, target);
      }
      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [newServer]: {
              allowedInQuorum: false,
              allowedAsLeader: false,
              forced: false,
            },
          })
      );

      // the new leader is excluded
      const errorCode = "TargetLeaderExcluded";
      waitFor(replicatedLogSupervisionError(database, logId, errorCode));

      // now remove the excluded flag
      replicatedLogUpdateTargetParticipants(database, logId, {
        [newServer]: {allowedInQuorum: true, allowedAsLeader: true},
      });
      waitFor(
          replicatedLogParticipantsFlag(database, logId, {
            [newServer]: {
              allowedInQuorum: true,
              allowedAsLeader: true,
              forced: false,
            },
            [leader]: null,
          })
      );
      waitFor(
          replicatedLogIsReady(
              database,
              logId,
              term + 1,
              [...followers, newServer],
              newServer
          )
      );
    },

    // This tests requests a non-server as leader and expects the
    // supervision to fail
    testChangeLeaderToNonServer: function () {
      const {logId, servers, term, leader} =
          createReplicatedLogAndWaitForLeader(database);

      // now change the leader
      const otherServer = "Foo42";
      setReplicatedLogLeaderTarget(database, logId, otherServer);
      // The supervision should complain about the server not being a leader

      const errorCode = "TargetLeaderInvalid";
      waitFor(replicatedLogSupervisionError(database, logId, errorCode));

      // nothing should have happend
      waitFor(replicatedLogIsReady(database, logId, term, servers, leader));
      replicatedLogDeleteTarget(database, logId);
    },

    // This tests requests a non-participant server as leader and expects the
    // supervision to fail
    testChangeLeaderToNonFollower: function () {
      const {logId, servers, term, leader} =
          createReplicatedLogAndWaitForLeader(database);

      // now change the leader
      const otherServer = _.sample(_.difference(dbservers, servers));
      setReplicatedLogLeaderTarget(database, logId, otherServer);
      // The supervision should complain about the server not being a leader

      const errorCode = "TargetLeaderInvalid";
      waitFor(replicatedLogSupervisionError(database, logId, errorCode));
      //
      // nothing should have happend
      waitFor(replicatedLogIsReady(database, logId, term, servers, leader));
      replicatedLogDeleteTarget(database, logId);
    },

    // This test replaces all participants
    testChangeAllParticipants: function () {
      const {logId, servers, term, leader} =
          createReplicatedLogAndWaitForLeader(database);

      // now change the leader
      const otherServers = _.difference(dbservers, servers);
      const targetVersion = 1;

      {
        let {target} = readReplicatedLogAgency(database, logId);
        // delete old leader from target
        target.version = targetVersion;
        target.participants = Object.assign(
            {},
            ...otherServers.map((x) => ({[x]: {}}))
        );
        replicatedLogSetTarget(database, logId, target);
      }

      waitFor(replicatedLogTargetVersion(database, logId, targetVersion));

      const {current} = readReplicatedLogAgency(database, logId);
      waitFor(
          replicatedLogLeaderEstablished(database, logId, undefined, otherServers)
      );

      replicatedLogDeleteTarget(database, logId);
    },


    // This test excludes all participants from quorum. This is uncommittable. Wait for the leader to complain.
    // then allow two servers. We exepect the log to commit this config.
    testExcludeAllServers: function () {
      const {logId, servers} = createReplicatedLogAndWaitForLeader(database);

      // now exclude all participants
      const [first, second, third] = servers;
      replicatedLogUpdateTargetParticipants(database, logId, {
        [first]: {allowedInQuorum: false},
        [second]: {allowedInQuorum: false},
        [third]: {allowedInQuorum: false},
      });

      waitFor(lpreds.replicatedLogLeaderCommitFail(database, logId, "NonEligibleServerRequiredForQuorum"));

      replicatedLogUpdateTargetParticipants(database, logId, {
        [first]: {allowedInQuorum: true},
        [second]: {allowedInQuorum: true},
        [third]: {allowedInQuorum: true},
      });
      // service should continue as normal
      waitFor(lpreds.replicatedLogLeaderCommitFail(database, logId, undefined));

      replicatedLogDeleteTarget(database, logId);
    },

    testLogStatus: function () {
      const {logId, servers, leader, term, followers} =
          createReplicatedLogAndWaitForLeader(database);

      waitFor(replicatedLogIsReady(database, logId, term, servers, leader));
      waitForReplicatedLogAvailable(logId);

      const log = db._replicatedLog(logId);
      waitFor(function () {
        let globalStatus = log.status();
        if (globalStatus.leaderId !== leader) {
          return Error(
              `Current leader is reported as ${globalStatus.leader}, expected ${leader}`
          );
        }

        let localStatus = helper.getLocalStatus(database, logId, leader);
        if (localStatus.role !== "leader") {
          return Error("Designated leader does not report as leader");
        }

        const leaderData = globalStatus.participants[leader];
        if (leaderData.connection.errorCode !== 0) {
          return Error(
              `Connection to leader failed: ${leaderData.connection.errorCode} ${leaderData.connection.errorMessage}`
          );
        }

        if (!_.isEqual(leaderData.response, localStatus)) {
          return Error(
              "Copy of local status does not yet match actual local status, " +
              `found = ${JSON.stringify(
                  globalStatus.participants[leader]
              )}; expected = ${JSON.stringify(localStatus)}`
          );
        }
        localStatus = helper.getLocalStatus(database, logId, followers[1]);
        if (localStatus.role !== "follower") {
          return Error("Designated follower does not report as follower");
        }
        return true;
      });

      stopServer(leader);
      stopServer(followers[0]);

      waitFor(
          replicatedLogLeaderElectionFailed(database, logId, term + 1, {
            [leader]: 1,
            [followers[0]]: 1,
            [followers[1]]: 0,
          })
      );

      waitFor(function () {
        const globalStatus = log.status();
        const {current} = readReplicatedLogAgency(database, logId);
        const election = current.supervision.election;

        const supervisionData = globalStatus.supervision;
        if (supervisionData.connection.errorCode !== 0) {
          return Error(
              `Connection to supervision failed: ${supervisionData.connection.errorCode} ${supervisionData.connection.errorMessage}`
          );
        }

        if (!_.isEqual(election, supervisionData.response.election)) {
          return Error(
              "Coordinator not reporting latest state from supervision" +
              `found = ${globalStatus.supervision.election}; expected = ${election}`
          );
        }

        if (globalStatus.specification.source !== "RemoteAgency") {
          return Error(
              `Specification source is ${globalStatus.specification.source}, expected RemoteAgency`
          );
        }

        return true;
      });

      replicatedLogDeleteTarget(database, logId);
      continueServer(leader);
      continueServer(followers[0]);
    },

    testReplaceMultipleFollowers: function () {
      const {servers, leader, followers, logId, term} = createReplicatedLogAndWaitForLeader(database);
      const otherServer = _.difference(dbservers, servers);

      const newFollowers = _.sampleSize(otherServer, 2);
      helper.replicatedLogUpdateTargetParticipants(database, logId, {
        [followers[0]]: null,
        [followers[1]]: null,
        [newFollowers[0]]: {},
        [newFollowers[1]]: {},
      });

      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [followers[0]]: null,
        [followers[1]]: null,
        [newFollowers[0]]: {allowedInQuorum: true, allowedAsLeader: true, forced: false},
        [newFollowers[1]]: {allowedInQuorum: true, allowedAsLeader: true, forced: false},
      }));
      waitFor(replicatedLogIsReady(database, logId, term, [leader, ...newFollowers], leader));

      {
        const actions = helper.getLastNSupervisionActionsType(database, logId, 4);
        const counts = helper.countActionsByType(actions);
        const expected = {
          "RemoveParticipantFromPlanAction": 2,
          "UpdateParticipantFlagsAction": 2,
        };
        assertEqual(counts, expected);
      }

      replicatedLogDeleteTarget(database, logId);
    },

    testReplaceLeaderAndOneFollower: function () {
      const {servers, leader, followers, logId, term} = createReplicatedLogAndWaitForLeader(database);

      //const initialNumberOfActions = helper.getSupervisionActionTypes(database, logId).length;

      const otherServer = _.difference(dbservers, servers);
      const [newLeader, newFollower] = _.sampleSize(otherServer, 2);
      const oldFollower = followers[0];
      helper.updateReplicatedLogTarget(database, logId, function (target) {
        delete target.participants[oldFollower];
        delete target.participants[leader];
        target.participants[newLeader] = {};
        target.participants[newFollower] = {};
        target.leader = newLeader;
      });

      waitFor(replicatedLogParticipantsFlag(database, logId, {
        [oldFollower]: null,
        [leader]: null,
        [newLeader]: {allowedInQuorum: true, allowedAsLeader: true, forced: false},
        [newFollower]: {allowedInQuorum: true, allowedAsLeader: true, forced: false},
        [followers[1]]: {allowedInQuorum: true, allowedAsLeader: true, forced: false},
      }));
      waitFor(replicatedLogIsReady(database, logId, term + 1, [newLeader, newFollower, followers[1]], newLeader));

      /*
      TODO this is unstable right now
      const actions = helper.getSupervisionActionTypes(database, logId);
      const actionsRequired = actions.length - initialNumberOfActions;
      const last = _.takeRight(actions, actionsRequired);
      const counts = helper.countActionsByType(last);
      const expected = {
        "RemoveParticipantFromPlanAction": 2, // we removed two participants
        "AddParticipantToPlanAction": 2, // and added two other participants
        "SwitchLeaderAction": 1, // we have one leader change
        "UpdateParticipantFlagsAction": 6, // two for switch leader, two for each remove participant
      };
      assertEqual(counts, expected);
      */
      replicatedLogDeleteTarget(database, logId);
    },
  };
};

jsunity.run(replicatedLogSuite);
return jsunity.done();
