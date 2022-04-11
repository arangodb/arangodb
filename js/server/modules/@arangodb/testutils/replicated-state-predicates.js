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

const SH = require("@arangodb/testutils/replicated-state-helper");

const isError = function (value) {
  return value instanceof Error;
};

// This predicate waits for the replicated state to be ready, i.e. it waits for
// 1. all servers in _servers_ have received the snapshot.
// 2. the underlying replicated log has a leader
const serverReceivedSnapshotGeneration = function (database, logId, server, generation) {
  return function () {
    const {current} = SH.readReplicatedStateAgency(database, logId);

    if (current === undefined || current.participants === undefined) {
      return Error(`Current participants for ${database}/${logId} not available yet.`);
    }

    // check if the servers is in current
    if (current.participants[server] === undefined) {
      return Error(`Server ${server} not yet reported in Current`);
    }

    const sc = current.participants[server];
    if (generation !== sc.generation) {
      return Error(`Expected generation for ${server} in Plan is ${generation}, current is ${sc.generation}`);
    }

    if (sc.snapshot === undefined) {
      return Error(`Snapshot Information not available for server ${server}`);
    }

    if (sc.snapshot.status !== "Completed") {
      return Error(`Snapshot status is not yet "Completed": ${sc.snapshot.status}`);
    }

    return true;
  };
};


const replicatedStateIsReady = function (database, logId, servers) {
  return function () {
    const {plan} = SH.readReplicatedStateAgency(database, logId);

    if (plan === undefined) {
      return Error(`Plan is not yet defined for ${logId}`);
    }

    for (const server of servers) {
      if (plan.participants[server] === undefined) {
        return Error(`Server ${server} is not in Plan, all ${servers}`);
      }

      const wantedGeneration = plan.participants[server].generation;
      const predicate = serverReceivedSnapshotGeneration(database, logId, server, wantedGeneration);
      const result = predicate();
      if (isError(result)) {
        return result;
      }
    }

    return true;
  };
};

const replicatedStateVersionConverged = function (database, logId, expectedVersion) {
  return function () {
    const {current} = SH.readReplicatedStateAgency(database, logId);

    if (current === undefined || current.supervision === undefined) {
      return Error(`Current/supervision is not yet defined for ${logId}`);
    }

    const supervision = current.supervision;
    if (supervision.version === undefined || supervision.version < expectedVersion) {
      return Error(`Expected version ${expectedVersion}, found version ${supervision.value}`);
    }

    return true;
  };
};

const replicatedStateTargetLeaderIs = function (database, stateId, expectedLeader) {
  return function () {
    const currentLeader = SH.getReplicatedStateLeaderTarget(database, stateId);
    if (currentLeader === expectedLeader) {
      return true;
    } else {
      return new Error(`Expected state leader to switch to ${expectedLeader}, but is still ${currentLeader}`);
    }
  };
};

exports.replicatedStateIsReady = replicatedStateIsReady;
exports.serverReceivedSnapshotGeneration = serverReceivedSnapshotGeneration;
exports.replicatedStateVersionConverged = replicatedStateVersionConverged;
exports.replicatedStateTargetLeaderIs = replicatedStateTargetLeaderIs;
