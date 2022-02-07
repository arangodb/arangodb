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
          status.participants[leaderId].connection.errorCode === 0 && status.participants[leaderId].response.role === "leader") {
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
    softWriteConcern: 3,
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
      }
    };
  }());

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

    testAlexLars: function () {
      const {logId, followers} = createReplicatedLogAndWaitForLeader(database);
      waitForReplicatedLogAvailable(logId);

      let log = db._replicatedLog(logId);
      let quorum = log.insert({foo: "bar"}); // index 2
      assertEqual(quorum.result.quorum.quorum.length, 3);

      console.log("Stoppped server************************************");
      // now stop one server
      stopServer(followers[0]);

      quorum = log.insert({foo: "bar"}); // index 3
      assertEqual(quorum.result.quorum.quorum.length, 2);
      console.log("____________________________________________-WTF_____________________________");

      let x = db._replicatedLog(logId).status();
      console.log(x);
      console.log("Continued server************************************");
      continueServer(followers[0]);
      x = db._replicatedLog(logId).status();
      console.log(x);

      console.log("After continue server************************************");
      waitFor(function () {
        let quorum = log.insert({foo: "bar"}); // index 4
        x = db._replicatedLog(logId).status();
        console.log(x);
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
