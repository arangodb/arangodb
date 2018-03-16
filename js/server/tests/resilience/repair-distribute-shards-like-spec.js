/* global describe, beforeEach, afterEach, it, instanceInfo, before */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
'use strict';

const expect = require('chai').expect;
const assert = require('chai').assert;
const _ = require("lodash");
const internal = require('internal');
const wait = require('internal').wait;
const request = require('@arangodb/request');

// const coordinatorName = "Coordinator0001";
const colName = "RepairDistributeShardsLikeTestCollection";
const protoColName = "RepairDistributeShardsLikeTestProtoCollection";

let coordinator = instanceInfo.arangods.filter(arangod => {
  return arangod.role === 'coordinator';
})[0];

let dbServerCount = instanceInfo.arangods.filter(arangod => {
  return arangod.role === 'dbserver';
}).length;

const waitForPlanEqualCurrent = function (collection) {
  const iterations = 180;
  const waitTime = 0.5;
  const maxTime = iterations * waitTime;

  for (let i = 0; i < iterations; i++) {
    const shardDist = internal.getCollectionShardDistribution(collection._id);
    const Plan = shardDist[collection.name()].Plan;
    const Current = shardDist[collection.name()].Current;
    if (Plan !== undefined && _.isEqual(Plan, Current)) {
      return true;
    }
    wait(waitTime);
  }

  console.error(`Collection "${collection}" failed to get plan in sync after ${maxTime} sec`);
  return false;
};

const waitForAgencyJob = function (jobId) {
  const prefix = global.ArangoAgency.prefix();
  const paths = [
    "Target/ToDo/" + jobId,
    "Target/Pending/" + jobId,
    "Target/Finished/" + jobId,
    "Target/Failed/" + jobId,
  ].map(p => `${prefix}/${p}`);

  const waitTime = 1.0;
  const maxTime = 60;

  let jobStopped = false;
  let success = false;

  const start = Date.now();

  while(! jobStopped) {
    const duration = (Date.now() - start) / 1000;
    const result = global.ArangoAgency.read([paths]);
    const target = result[0][prefix]["Target"];

    if (duration > maxTime) {
      console.error(`Timeout after waiting for ${duration}s on job "${jobId}"`);
      jobStopped = true;
      success = true;
    }
    else if (jobId in target["Finished"]) {
      jobStopped = true;
      success = true;
    }
    else if (jobId in target["Failed"]) {
      const reason = target["Failed"][jobId].reason;
      console.error(`Job "${jobId}" failed with: ${reason}`);
      jobStopped = true;
      success = false;
    }
    else if (jobId in target["ToDo"]) {
      jobStopped = false;
      success = false;
    }
    else if (jobId in target["Pending"]) {
      jobStopped = false;
      success = false;
    }
    else {
      console.error(`Job "${jobId}" vanished`);
      jobStopped = true;
      success = false;
    }

    wait(waitTime);
  }

  return success;
};


// TODO instead of a smart graph create two collections, one with distributeShardsLike:
// - Create collection A
// - Create collection B with distributeShardsLike=A
// - Wait for both to be replicated
// - Use an agency transaction to rename distributeShardsLike (with a comment
//   that this will break your database in production)
// - Use MoveShard Operations (and wait for them) to break the
//   distributeShardsLike assumptions. Use a DBServer order/permutation
//   depending on the servers available in the proto-shard and their order
//   to make this deterministic.
// - Use an agency transaction to restore distributeShardsLike (with a comment
//   that this will break your database in production)
// - Execute repairs
describe('Collections with distributeShardsLike', function () {
  afterEach(function() {
    internal.db._drop(colName);
    internal.db._drop(protoColName);
  });

  it('if newly created, should always be ok', function() {
    const collection = internal.db._createDocumentCollection(protoColName,
      {replicationFactor: dbServerCount, numberOfShards: 16});
    const protoCollection = internal.db._createDocumentCollection(colName,
      {distributeShardsLike: protoColName});

    expect(waitForPlanEqualCurrent(protoCollection)).to.be.true;
    expect(waitForPlanEqualCurrent(collection)).to.be.true;

    const d = request.post(coordinator.url + '/_admin/repair/distributeShardsLike');

    let response = JSON.parse(d.body);

    expect(response).to.have.property("error", false);
    expect(response).to.have.property("code", 200);
    expect(response).to.have.property("message", "Nothing to do.");
  });

  it('if broken, should be repaired', function() {
    const replicationFactor = dbServerCount - 1;
    internal.db._createDocumentCollection(protoColName,
      {replicationFactor: replicationFactor, numberOfShards: 16});

    internal.db._createDocumentCollection(colName,
      {distributeShardsLike: protoColName});

    const collection = internal.db._collection(colName);
    const protoCollection = internal.db._collection(protoColName);

    waitForPlanEqualCurrent(protoCollection);
    waitForPlanEqualCurrent(collection);

    global.ArangoAgency.remove(`Plan/Collections/${internal.db._name()}/${collection._id}/distributeShardsLike`);

    const protoShardDist = internal.getCollectionShardDistribution(protoColName)[protoColName].Plan;
    const shardDist = internal.getCollectionShardDistribution(colName)[colName].Plan;
    let protoShards = Object.keys(protoShardDist);
    let shards = Object.keys(shardDist);
    protoShards.sort((a, b) => a.localeCompare(b, 'POSIX', {numeric:true}));
    shards.sort((a, b) => a.localeCompare(b, 'POSIX', {numeric:true}));

    const dbServers = global.ArangoClusterInfo.getDBServers();
    const dbServerIdByName =
      dbServers.reduce(
        (nameToId, server) => {
          nameToId[server.serverName] = server.serverId;
          return nameToId;
        },
        {}
      );

    const protoShard = protoShards[0];
    const shard = shards[0];
    const protoShardInfo = protoShardDist[protoShard];
    const shardInfo = shardDist[shard];

    const freeDbServers = dbServers.filter(
      server => [
        protoShardDist[protoShard].leader
      ].concat(protoShardDist[protoShard].followers)
        .includes(server.serverName)
    );

    const postMoveShardJob = function (from, to, isLeader) {
      const id = global.ArangoClusterInfo.uniqid();
      const moveShardTodo = {
        type: 'moveShard',
        database: internal.db._name(),
        collection: collection._id,
        shard: shard,
        fromServer: from,
        toServer: to,
        jobId: id,
        timeCreated: (new Date()).toISOString(),
        creator: ArangoServerState.id(),
        isLeader: isLeader
      };
      global.ArangoAgency.set('Target/ToDo/' + id, moveShardTodo);
      return id;
    };

    const freeDbServer = freeDbServers[0].serverId;
    const leaderDbServer = dbServerIdByName[shardInfo.leader];
    const followerDbServer = dbServerIdByName[shardInfo.followers[0]];

    let jobId = postMoveShardJob(leaderDbServer, freeDbServer, true);
    waitForAgencyJob(jobId);
    jobId = postMoveShardJob(followerDbServer, leaderDbServer, false);
    waitForAgencyJob(jobId);

    global.ArangoAgency.set(
      `Plan/Collections/${internal.db._name()}/${collection._id}/distributeShardsLike`,
      protoColName
    );

    waitForPlanEqualCurrent(collection);

    const d = request.post(coordinator.url + '/_admin/repair/distributeShardsLike');

    let response = JSON.parse(d.body);

    expect(response).to.have.property("error", false);
    expect(response).to.have.property("code", 200);
    expect(response).to.have.property("message", "Executing x operations");
  });

});

/* example response:

{
  "error": false,
  "code": 200,
  "message": "Executing 3 operations",
  "Operations": [
    "BeginRepairsOperation\n{ database: _system\n, collection: _local_E (11020054)\n, protoCollection: V (11020039)\n, collectionReplicationFactor: 3\n, protoReplicationFactor: 2\n, renameDistributeShardsLike: 1\n}",
    "MoveShardOperation\n{ database: _system\n, collection: _local_E (11020054)\n, shard: s11020057\n, from: PRMR-287ae22f-92ba-4f10-a641-9e760f4f0e88\n, to: PRMR-8e8bf7e1-a792-4a54-b64f-95559009855a\n, isLeader: 0\n}",
    "FinishRepairsOperation\n{ database: _system\n, collection: _local_E (11020054)\n, protoCollection: V (11020039)\n, replicationFactor: 2\n}"
  ]
}

 */

/* example shard distribution:

{
  "RepairDistributeShardsLikeTestCollection": {
    "Plan": {
      "s5010126": {
        "leader": "DBServer0004",
        "followers": [
          "DBServer0003"
        ]
      },
      "s5010125": {
        "leader": "DBServer0005",
        "followers": [
          "DBServer0004"
        ]
      },
      "s5010124": {
        "leader": "DBServer0001",
        "followers": [
          "DBServer0005"
        ]
      },
      "s5010122": {
        "leader": "DBServer0003",
        "followers": [
          "DBServer0002"
        ]
      },
      "s5010121": {
        "leader": "DBServer0004",
        "followers": [
          "DBServer0003"
        ]
      },
      "s5010113": {
        "leader": "DBServer0002",
        "followers": [
          "DBServer0001"
        ]
      },
      "s5010123": {
        "leader": "DBServer0002",
        "followers": [
          "DBServer0001"
        ]
      },
      "s5010115": {
        "leader": "DBServer0005",
        "followers": [
          "DBServer0004"
        ]
      },
      "s5010112": {
        "leader": "DBServer0003",
        "followers": [
          "DBServer0002"
        ]
      },
      "s5010111": {
        "leader": "DBServer0004",
        "followers": [
          "DBServer0003"
        ]
      },
      "s5010120": {
        "leader": "DBServer0005",
        "followers": [
          "DBServer0004"
        ]
      },
      "s5010116": {
        "leader": "DBServer0004",
        "followers": [
          "DBServer0003"
        ]
      },
      "s5010114": {
        "leader": "DBServer0001",
        "followers": [
          "DBServer0005"
        ]
      },
      "s5010117": {
        "leader": "DBServer0003",
        "followers": [
          "DBServer0002"
        ]
      },
      "s5010118": {
        "leader": "DBServer0002",
        "followers": [
          "DBServer0001"
        ]
      },
      "s5010119": {
        "leader": "DBServer0001",
        "followers": [
          "DBServer0005"
        ]
      }
    },
    "Current": {
      "s5010126": {
        "leader": "DBServer0004",
        "followers": [
          "DBServer0003"
        ]
      },
      "s5010125": {
        "leader": "DBServer0005",
        "followers": [
          "DBServer0004"
        ]
      },
      "s5010124": {
        "leader": "DBServer0001",
        "followers": [
          "DBServer0005"
        ]
      },
      "s5010122": {
        "leader": "DBServer0003",
        "followers": [
          "DBServer0002"
        ]
      },
      "s5010121": {
        "leader": "DBServer0004",
        "followers": [
          "DBServer0003"
        ]
      },
      "s5010113": {
        "leader": "DBServer0002",
        "followers": [
          "DBServer0001"
        ]
      },
      "s5010123": {
        "leader": "DBServer0002",
        "followers": [
          "DBServer0001"
        ]
      },
      "s5010115": {
        "leader": "DBServer0005",
        "followers": [
          "DBServer0004"
        ]
      },
      "s5010112": {
        "leader": "DBServer0003",
        "followers": [
          "DBServer0002"
        ]
      },
      "s5010111": {
        "leader": "DBServer0004",
        "followers": [
          "DBServer0003"
        ]
      },
      "s5010120": {
        "leader": "DBServer0005",
        "followers": [
          "DBServer0004"
        ]
      },
      "s5010116": {
        "leader": "DBServer0004",
        "followers": [
          "DBServer0003"
        ]
      },
      "s5010114": {
        "leader": "DBServer0001",
        "followers": [
          "DBServer0005"
        ]
      },
      "s5010117": {
        "leader": "DBServer0003",
        "followers": [
          "DBServer0002"
        ]
      },
      "s5010118": {
        "leader": "DBServer0002",
        "followers": [
          "DBServer0001"
        ]
      },
      "s5010119": {
        "leader": "DBServer0001",
        "followers": [
          "DBServer0005"
        ]
      }
    }
  }
}
 */
