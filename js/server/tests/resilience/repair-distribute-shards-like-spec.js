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
  const iterations = 120;
  const waitTime = 1.0;
  const maxTime = iterations * waitTime;

  for (let i = 0; i < iterations; i++) {
    global.ArangoClusterInfo.flush();
    const shardDist = internal.getCollectionShardDistribution(collection._id);
    const Plan = shardDist[collection.name()].Plan;
    const Current = shardDist[collection.name()].Current;

    if (_.isObject(Plan) && _.isEqual(Plan, Current)) {
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

  const waitInterval = 1.0;
  const maxWaitTime = 60;

  let jobStopped = false;
  let success = false;

  const start = Date.now();

  while(! jobStopped) {
    const duration = (Date.now() - start) / 1000;
    const result = global.ArangoAgency.read([paths]);
    const target = result[0][prefix]["Target"];

    if (duration > maxWaitTime) {
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

    wait(waitInterval);
  }

  return success;
};


const expectCollectionPlanToEqualProto = function (collection, protoCollection) {
  global.ArangoClusterInfo.flush();
  const shardDist = internal.getCollectionShardDistribution(collection._id);
  const protoShardDist = internal.getCollectionShardDistribution(protoCollection._id);

  const shardDistPlan = shardDist[collection.name()].Plan;
  const protoShardDistPlan = protoShardDist[protoCollection.name()].Plan;

  let shards = Object.keys(shardDistPlan)
    .sort((a, b) => a.localeCompare(b, 'POSIX', {numeric:true}));
  let protoShards = Object.keys(protoShardDistPlan)
    .sort((a, b) => a.localeCompare(b, 'POSIX', {numeric:true}));

  expect(shards.length).to.equal(protoShards.length);

  for(const [shard, protoShard] of _.zip(shards, protoShards)) {
    expect(shard.leader).to.equal(protoShard.leader);
    let followers = shardDistPlan[shard].followers.slice().sort();
    let protoFollowers = protoShardDistPlan[protoShard].followers.slice().sort();

    expect(followers).to.deep.equal(protoFollowers);
  }
};


describe('Collections with distributeShardsLike', function () {
  afterEach(function() {
    internal.db._drop(colName);
    internal.db._drop(protoColName);
  });

  it('if newly created, should always be ok', function() {
    const protoCollection = internal.db._createDocumentCollection(protoColName,
      {replicationFactor: dbServerCount, numberOfShards: 3});
    const collection = internal.db._createDocumentCollection(colName,
      {distributeShardsLike: protoCollection._id});

    expect(waitForPlanEqualCurrent(protoCollection)).to.be.true;
    expect(waitForPlanEqualCurrent(collection)).to.be.true;

    const d = request.post(coordinator.url + '/_admin/repair/distributeShardsLike');

    let response = JSON.parse(d.body);

    expect(response).to.have.property("error", false);
    expect(response).to.have.property("code", 200);
    expect(response).to.have.property("message", "Nothing to do.");
  });


// - Create collection A
// - Create collection B with distributeShardsLike=A
// - Wait for both to be replicated
// - Use an agency transaction to rename distributeShardsLike
// - Use MoveShard Operations (and wait for them) to break the
//   distributeShardsLike assumptions. Use a DBServer order/permutation
//   depending on the servers available in the proto-shard and their order
//   to make this deterministic.
// - Use an agency transaction to restore distributeShardsLike
// - Execute repairs
  it('if broken, should be repaired', function() {
    const replicationFactor = dbServerCount - 1;
    const protoCollection = internal.db._createDocumentCollection(protoColName,
      {replicationFactor: replicationFactor, numberOfShards: 16});
    const collection = internal.db._createDocumentCollection(colName,
      {distributeShardsLike: protoCollection._id});

    expect(waitForPlanEqualCurrent(protoCollection)).to.be.true;
    expect(waitForPlanEqualCurrent(collection)).to.be.true;

    // IMPORTANT NOTE: Never do this in a real environment. Changing
    // distributeShardsLike will break your cluster!
    global.ArangoAgency.remove(`Plan/Collections/${internal.db._name()}/${collection._id}/distributeShardsLike`);

    global.ArangoClusterInfo.flush();
    const protoShardDist = internal.getCollectionShardDistribution(protoColName)[protoColName].Plan;
    const shardDist = internal.getCollectionShardDistribution(colName)[colName].Plan;
    let protoShards = Object.keys(protoShardDist)
      .sort((a, b) => a.localeCompare(b, 'POSIX', {numeric:true}));
    let shards = Object.keys(shardDist)
      .sort((a, b) => a.localeCompare(b, 'POSIX', {numeric:true}));

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
      server => ! [protoShardInfo.leader]
        .concat(protoShardInfo.followers)
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

    expect(waitForPlanEqualCurrent(collection)).to.be.true;
    let jobId = postMoveShardJob(leaderDbServer, freeDbServer, true);
    let result = waitForAgencyJob(jobId);
    expect(result).to.equal(true);
    expect(waitForPlanEqualCurrent(collection)).to.be.true;

    jobId = postMoveShardJob(followerDbServer, leaderDbServer, false);
    result = waitForAgencyJob(jobId);

    expect(result).to.equal(true);
    expect(waitForPlanEqualCurrent(collection)).to.be.true;

    // IMPORTANT NOTE: Never do this in a real environment. Changing
    // distributeShardsLike will break your cluster!
    global.ArangoAgency.set(
      `Plan/Collections/${internal.db._name()}/${collection._id}/distributeShardsLike`,
      protoCollection._id
    );

    expect(waitForPlanEqualCurrent(collection)).to.be.true;

    const d = request.post(coordinator.url + '/_admin/repair/distributeShardsLike');

    let response = JSON.parse(d.body);

    expect(response).to.have.property("error", false);
    expect(response).to.have.property("code", 200);
    expect(response).to.have.property("message", "Executing 5 operations");

    global.ArangoClusterInfo.flush();

    // Note: properties() returns the name of the collection in distributeShardsLike
    // instead of the id!
    expect(
      collection.properties().distributeShardsLike
    ).to.equal(protoCollection.name());

    expectCollectionPlanToEqualProto(collection, protoCollection);

    // TODO Should the exact operations be checked? Or do we leave that to the unit tests?
  });

});
