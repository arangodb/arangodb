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

const LH = require("@arangodb/testutils/replicated-logs-helper");
const _ = require("lodash");


const replicatedLogIsReady = function (database, logId, term, participants, leader) {
  return function () {
    let {current} = LH.readReplicatedLogAgency(database, logId);
    if (current === undefined) {
      return Error("current not yet defined");
    }

    for (const srv of participants) {
      if (!current.localStatus || !current.localStatus[srv]) {
        return Error(`Participant ${srv} has not yet reported to current.`);
      }
      if (current.localStatus[srv].term < term) {
        return Error(`Participant ${srv} has not yet acknowledged the current term; ` +
            `found = ${current.localStatus[srv].term}, expected = ${term}.`);
      }
    }

    if (leader !== undefined) {
      if (!current.leader) {
        return Error("Leader has not yet established its term");
      }
      if (current.leader.serverId !== leader) {
        return Error(`Wrong leader in current; found = ${current.leader.serverId}, expected = ${leader}`);
      }
      if (current.leader.term < term) {
        return Error(`Leader has not yet confirmed the term; found = ${current.leader.term}, expected = ${term}`);
      }
      if (!current.leader.leadershipEstablished) {
        return Error("Leader has not yet established its leadership");
      }
    }
    return true;
  };
};

const replicatedLogLeaderEstablished = function (database, logId, term, participants) {
  return function () {
    let {current} = LH.readReplicatedLogAgency(database, logId);
    if (current === undefined) {
      return Error("current not yet defined");
    }

    for (const srv of participants) {
      if (!current.localStatus || !current.localStatus[srv]) {
        return Error(`Participant ${srv} has not yet reported to current.`);
      }
      if (term !== undefined && current.localStatus[srv].term < term) {
        return Error(`Participant ${srv} has not yet acknowledged the current term; ` +
            `found = ${current.localStatus[srv].term}, expected = ${term}.`);
      }
    }

    if (!current.leader) {
      return Error("Leader has not yet established its term");
    }
    if (!current.leader.leadershipEstablished) {
      return Error("Leader has not yet established its leadership");
    }

    return true;
  };
};

const allServersHealthy = function () {
  return function () {
    for (const server of LH.dbservers) {
      const health = LH.getServerHealth(server);
      if (health !== "GOOD") {
        return Error(`${server} is ${health}`);
      }
    }

    return true;
  };
};

const checkServerHealth = function (serverId, value) {
  return function () {
    if (value === LH.getServerHealth(serverId)) {
      return true;
    }
    return Error(`${serverId} is not ${value}`);
  };
};

const serverHealthy = (serverId) => checkServerHealth(serverId, "GOOD");
const serverFailed = (serverId) => checkServerHealth(serverId, "FAILED");

const replicatedLogParticipantsFlag = function (database, logId, flags, generation = undefined) {
  return function () {
    let {current} = LH.readReplicatedLogAgency(database, logId);
    if (current === undefined) {
      return Error("current not yet defined");
    }
    if (!current.leader) {
      return Error("Leader has not yet established its term");
    }
    if (!current.leader.committedParticipantsConfig) {
      return Error("Leader has not yet committed any participants config");
    }
    if (generation !== undefined) {
      if (current.leader.committedParticipantsConfig.generation < generation) {
        return Error("Leader has not yet acked new generation; "
            + `found ${current.leader.committedParticipantsConfig.generation}, expected = ${generation}`);
      }
    }

    const participants = current.leader.committedParticipantsConfig.participants;
    for (const [p, v] of Object.entries(flags)) {
      if (v === null) {
        if (participants[p] !== undefined) {
          return Error(`Entry for server ${p} still present in participants flags`);
        }
      } else {
        if (!_.isEqual(v, participants[p])) {
          return Error(`Flags for participant ${p} are not as expected: ${JSON.stringify(v)} vs. ${JSON.stringify(participants[p])}`);
        }
      }
    }

    return true;
  };
};

const replicatedLogTargetVersion = function (database, logId, version) {
  return function () {
    let {current} = LH.readReplicatedLogAgency(database, logId);

    if (current === undefined) {
      return Error(`current not yet defined`);
    }
    if (!current.supervision) {
      return Error(`supervision not yet reported to current`);
    }
    if (!current.supervision.targetVersion) {
      return Error(`no version reported in current by supervision`);
    }
    if (current.supervision.targetVersion !== version) {
      return Error(`found version ${current.supervison.targetVersion}, expected ${version}`);
    }
    return true;
  };
};

const replicatedLogLeaderTargetIs = function (database, stateId, expectedLeader) {
  return function () {
    const currentLeader = LH.getReplicatedLogLeaderTarget(database, stateId);
    if (currentLeader === expectedLeader) {
      return true;
    } else {
      return new Error(`Expected log leader to switch to ${expectedLeader}, but is still ${currentLeader}`);
    }
  };
};

const replicatedLogLeaderPlanIs = function (database, stateId, expectedLeader) {
  return function () {
    const {leader: currentLeader} = LH.getReplicatedLogLeaderPlan(database, stateId);
    if (currentLeader === expectedLeader) {
      return true;
    } else {
      return new Error(`Expected log leader to switch to ${expectedLeader}, but is still ${currentLeader}`);
    }
  };
};

const replicatedLogLeaderCommitFail = function (database, logId, expected) {
  return function () {
    let {current} = LH.readReplicatedLogAgency(database, logId);
    if (current === undefined) {
      return Error("current not yet defined");
    }
    if (!current.leader) {
      return Error("Leader has not yet established its term");
    }

    const status = current.leader.commitStatus;
    if (expected === undefined) {
      if (status !== undefined) {
        return Error(`CommitStatus not yet cleared, current-value = ${status.reason}`);
      }
    } else {
      if (status === undefined) {
        return Error("CommitStatus not yet set.");
      } else if (status.reason !== expected) {
        return Error(`CommitStatus not as expected, found ${status.reason}; expected ${expected}`);
      }
    }

    return true;
  };
};

exports.replicatedLogIsReady = replicatedLogIsReady;
exports.allServersHealthy = allServersHealthy;
exports.serverHealthy = serverHealthy;
exports.serverFailed = serverFailed;
exports.replicatedLogLeaderEstablished = replicatedLogLeaderEstablished;
exports.replicatedLogParticipantsFlag = replicatedLogParticipantsFlag;
exports.replicatedLogTargetVersion = replicatedLogTargetVersion;
exports.replicatedLogLeaderTargetIs = replicatedLogLeaderTargetIs;
exports.replicatedLogLeaderPlanIs = replicatedLogLeaderPlanIs;
exports.replicatedLogLeaderCommitFail = replicatedLogLeaderCommitFail;
