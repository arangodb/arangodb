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
const _ = require("lodash");
const internal = require('internal');
const wait = require('internal').wait;
const request = require('@arangodb/request');

const colName = "repairDSLTestCollection";
const protoColName = "repairDSLTestProtoCollection";

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

const waitForReplicationFactor = function (collection) {
  const iterations = 120;
  const waitTime = 1.0;
  const maxTime = iterations * waitTime;


  for (let i = 0; i < iterations; i++) {
    global.ArangoClusterInfo.flush();
    const ci = global.ArangoClusterInfo.getCollectionInfo(internal.db._name(), collection._id);

    let allShardsInSync = Object.values(ci.shards).every(
      servers => servers.length === ci.replicationFactor
    );

    if (allShardsInSync) {
      return true;
    }

    wait(waitTime);
  }

  console.error(`Collection "${collection}" failed to get replicationFactor in sync after ${maxTime} sec`);
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
      success = false;
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


const waitForAllAgencyJobs = function () {
  const prefix = global.ArangoAgency.prefix();
  const paths = [
    "Target/ToDo/",
    "Target/Pending/",
  ].map(p => `${prefix}/${p}`);

  const waitInterval = 1.0;
  const maxWaitTime = 60;

  let unfinishedJobs = Infinity;
  let timeout = false;
  const start = Date.now();

  while(unfinishedJobs > 0 && ! timeout) {
    const duration = (Date.now() - start) / 1000;
    const result = global.ArangoAgency.read([paths]);
    const target = result[0][prefix]["Target"];

    timeout = duration > maxWaitTime;

    unfinishedJobs = target["ToDo"].length + target["Pending"].length;

    wait(waitInterval);
  }

  if (timeout) {
    const duration = (Date.now() - start) / 1000;
    console.error(`Timeout after waiting for ${duration}s on all agency jobs. `
      + `${unfinishedJobs} jobs aren't finished.`);
  }

  return unfinishedJobs === 0;
};


const expectEqualShardDistributionPlan = function (shardDist, protoShardDist) {
  const shardDistPlan = shardDist.Plan;
  const protoShardDistPlan = protoShardDist.Plan;

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

const createBrokenClusterState = function ({failOnOperation = null} = {}) {
  const replicationFactor = dbServerCount - 1;
  const protoCollection = internal.db._createDocumentCollection(protoColName,
    {replicationFactor: replicationFactor, numberOfShards: 16});
  let localColName = failOnOperation === null
    ? colName
    : colName + `---fail_on_operation_nr-${failOnOperation}`;
  const collection = internal.db._createDocumentCollection(localColName,
    {distributeShardsLike: protoCollection._id});

  expect(waitForPlanEqualCurrent(protoCollection)).to.be.true;
  expect(waitForPlanEqualCurrent(collection)).to.be.true;

  // IMPORTANT NOTE: Never do this in a real environment. Changing
  // distributeShardsLike will break your cluster!
  global.ArangoAgency.remove(`Plan/Collections/${internal.db._name()}/${collection._id}/distributeShardsLike`);

  global.ArangoClusterInfo.flush();
  const protoShardDist = internal.getCollectionShardDistribution(protoColName)[protoColName].Plan;
  const shardDist = internal.getCollectionShardDistribution(localColName)[localColName].Plan;
  let protoShards = Object.keys(protoShardDist)
    .sort((a, b) => a.localeCompare(b, 'POSIX', {numeric: true}));
  let shards = Object.keys(shardDist)
    .sort((a, b) => a.localeCompare(b, 'POSIX', {numeric: true}));

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
    server => ![protoShardInfo.leader]
      .concat(protoShardInfo.followers)
      .includes(server.serverName)
  );

  const freeDbServer = freeDbServers[0].serverId;
  const leaderDbServer = dbServerIdByName[shardInfo.leader];
  const followerDbServer = dbServerIdByName[shardInfo.followers[0]];

  let expectedCollections = {
    [`${internal.db._name()}/${localColName}`]: {
      "PlannedOperations": [
        {
          "BeginRepairsOperation": {
            "database": internal.db._name(),
            "collection": localColName,
            "distributeShardsLike": protoColName,
            "renameDistributeShardsLike": true,
            "replicationFactor": replicationFactor
          }
        },
        {
          "MoveShardOperation": {
            "database": internal.db._name(),
            "collection": localColName,
            "shard": shard,
            "from": leaderDbServer,
            "to": followerDbServer,
            "isLeader": false
          }
        },
        {
          "MoveShardOperation": {
            "database": internal.db._name(),
            "collection": localColName,
            "shard": shard,
            "from": freeDbServer,
            "to": leaderDbServer,
            "isLeader": true
          }
        },
        {
          "FixServerOrderOperation": {
            "database": "_system",
            "collection": localColName,
            "distributeShardsLike": protoColName,
            "shard": shard,
            "distributeShardsLikeShard": protoShard,
            "leader": leaderDbServer,
            "followers": [1, 2, 0].map(i => protoShardInfo.followers[i]).map(f => dbServerIdByName[f]),
            "distributeShardsLikeFollowers": protoShardInfo.followers.map(f => dbServerIdByName[f])
          }
        },
        {
          "FinishRepairsOperation": {
            "database": internal.db._name(),
            "collection": localColName,
            "distributeShardsLike": protoColName,
            "shards": _.zip(shards, protoShards).map(
              ([shard, protoShard]) => ({
                shard: shard,
                protoShard: protoShard,
                dbServers: [protoShardDist[protoShard].leader]
                  .concat(protoShardDist[protoShard].followers)
                  .map(server => dbServerIdByName[server])
              })
            )
          }
        }
      ],
      error: false
    }
  };

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
      creator: global.ArangoServerState.id(),
      isLeader: isLeader
    };
    global.ArangoAgency.set('Target/ToDo/' + id, moveShardTodo);
    return id;
  };

  expect(waitForPlanEqualCurrent(collection)).to.be.true;
  let jobId = postMoveShardJob(leaderDbServer, freeDbServer, true);
  let result = waitForAgencyJob(jobId);
  expect(result).to.equal(true);
  expect(waitForReplicationFactor(collection)).to.be.true;
  expect(waitForPlanEqualCurrent(collection)).to.be.true;

  jobId = postMoveShardJob(followerDbServer, leaderDbServer, false);
  result = waitForAgencyJob(jobId);
  expect(waitForReplicationFactor(collection)).to.be.true;

  expect(result).to.equal(true);

  expect(waitForPlanEqualCurrent(collection)).to.be.true;

  // IMPORTANT NOTE: Never do this in a real environment. Changing
  // distributeShardsLike will break your cluster!
  global.ArangoAgency.set(
    `Plan/Collections/${internal.db._name()}/${collection._id}/distributeShardsLike`,
    protoCollection._id
  );
  global.ArangoAgency.increaseVersion("Plan/Version");

  expect(waitForPlanEqualCurrent(collection)).to.be.true;
  return {collection, protoCollection, expectedCollections};
};


let waitForJob = function (postJobRes) {
  expect(postJobRes).to.have.property("status", 202);
  expect(postJobRes).to.have.property("headers");
  expect(postJobRes.headers).to.have.property('x-arango-async-id');
  const jobId = postJobRes.headers['x-arango-async-id'];
  expect(jobId).to.be.a('string');

  const waitInterval = 1.0;
  const maxWaitTime = 120;

  const start = Date.now();

  let jobFinished = false;
  let timeoutExceeded = false;
  let putJobRes;

  while (!jobFinished && !timeoutExceeded) {
    const duration = (Date.now() - start) / 1000;
    timeoutExceeded = duration > maxWaitTime;

    putJobRes = request.put(coordinator.url + `/_api/job/${jobId}`);

    expect(putJobRes).to.have.property("status");

    if (putJobRes.status === 204) {
      wait(waitInterval);
    }
    else {
      jobFinished = true;
    }
  }

  if (jobFinished) {
    return putJobRes;
  }

  console.error(`Waiting for REST job timed out`);
  return undefined;
};


describe('Collections with distributeShardsLike', function () {
  afterEach(function() {
    internal.db._drop(colName);
    internal.db._collections()
      .map(col => col.name())
      .filter(col => col.startsWith(`${colName}---`))
      .forEach(col => internal.db._drop(col));
    internal.db._drop(protoColName);
    internal.debugClearFailAt();
  });

  it('if newly created, should always be ok', function() {
    const protoCollection = internal.db._createDocumentCollection(protoColName,
      {replicationFactor: dbServerCount, numberOfShards: 3});
    const collection = internal.db._createDocumentCollection(colName,
      {distributeShardsLike: protoCollection._id});

    expect(waitForPlanEqualCurrent(protoCollection)).to.be.true;
    expect(waitForPlanEqualCurrent(collection)).to.be.true;

    // Directly posting should generally not be used, as it is likely to timeout.
    // Setting the header "x-arango-async: store" instead is preferred.
    // In this case however it should return immediately, so a timeout would
    // be an error here. Also its good to have a test for a direct call, too.
    const d = request.post(coordinator.url + '/_admin/repair/distributeShardsLike');

    expect(d.status).to.equal(200);

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
    const { protoCollection, collection, expectedCollections }
      = createBrokenClusterState();

    { // Before executing repairs, check via GET if the planned operations
      // seem right.
      const d = request.get(coordinator.url + '/_admin/repair/distributeShardsLike');

      expect(d.status).to.equal(200);

      let response = JSON.parse(d.body);

      expect(response).to.have.property("error", false);
      expect(response).to.have.property("code", 200);
      expect(response).to.have.property("collections");

      const fullColName = internal.db._name() + '/' + collection.name();
      expect(response.collections).to.have.property(fullColName);
      expect(response.collections).to.have.all.keys([fullColName]);
      expect(response.collections[fullColName]).to.have.property('PlannedOperations');
      const plannedOperations = response.collections[fullColName]['PlannedOperations'];
      expect(plannedOperations).to.be.an('array').that.has.lengthOf(5);
    }

    const postJobRes = request.post(
      coordinator.url + '/_admin/repair/distributeShardsLike',
      {
        headers: { "x-arango-async": "store" }
      }
    );
    const jobRes = waitForJob(postJobRes);

    expect(jobRes).to.have.property("status", 200);

    let response = JSON.parse(jobRes.body);

    expect(response).to.have.property("error", false);
    expect(response).to.have.property("code", 200);
    expect(response).to.have.property("collections");
    expect(response.collections).to.eql(expectedCollections);

    global.ArangoClusterInfo.flush();

    // Note: properties() returns the name of the collection in distributeShardsLike
    // instead of the id!
    expect(
      collection.properties().distributeShardsLike
    ).to.equal(protoCollection.name());

    global.ArangoClusterInfo.flush();
    const shardDist = internal
      .getCollectionShardDistribution(collection._id)[collection.name()];
    const protoShardDist = internal
      .getCollectionShardDistribution(protoCollection._id)[protoCollection.name()];
    expectEqualShardDistributionPlan(shardDist, protoShardDist);
  });

  if (internal.debugCanUseFailAt()) {
    it('if interrupted, should complete repairs', function () {
      // In this test case, trigger an exception after the second operation,
      // i.e. after the first move shard operation, has been posted
      // (but not finished).
      const {protoCollection, collection, expectedCollections}
        = createBrokenClusterState({failOnOperation: 2});


      { // Before executing repairs, check via GET if the planned operations
        // seem right.
        const d = request.get(coordinator.url + '/_admin/repair/distributeShardsLike');

        expect(d.status).to.equal(200);

        let response = JSON.parse(d.body);

        expect(response).to.have.property("error", false);
        expect(response).to.have.property("code", 200);
        expect(response).to.have.property("collections");

        const fullColName = internal.db._name() + '/' + collection.name();
        expect(response.collections).to.have.property(fullColName);
        expect(response.collections).to.have.all.keys([fullColName]);
        expect(response.collections[fullColName]).to.have.property('PlannedOperations');
        const plannedOperations = response.collections[fullColName]['PlannedOperations'];
        expect(plannedOperations).to.be.an('array').that.has.lengthOf(5);
      }

      internal.debugSetFailAt("RestRepairHandler::executeRepairOperations");

      { // This request should fail
        const postJobRes = request.post(
          coordinator.url + '/_admin/repair/distributeShardsLike',
          {
            headers: {"x-arango-async": "store"}
          }
        );
        const jobRes = waitForJob(postJobRes);

        // jobRes =  [IncomingResponse 500 Internal Server Error 80 bytes "{"error":true,"errorMessage":"intentional debug error","code":500,"errorNum":22}"]

        expect(jobRes).to.have.property("status", 500);

        let response = JSON.parse(jobRes.body);

        expect(response).to.have.property("error", true);
        expect(response).to.have.property("errorMessage", internal.errors.ERROR_DEBUG.message);
        expect(response).to.have.property("errorNum", internal.errors.ERROR_DEBUG.code);
        expect(response).to.have.property("code", 500);
        expect(response).to.not.have.property("collections");

        global.ArangoClusterInfo.flush();
      }

      internal.debugClearFailAt();

      expect(waitForAllAgencyJobs());
      expect(waitForReplicationFactor(collection)).to.be.true;
      expect(waitForPlanEqualCurrent(collection)).to.be.true;

      { // Before executing repairs, check via GET if the planned operations
        // seem right.
        const d = request.get(coordinator.url + '/_admin/repair/distributeShardsLike');

        expect(d.status).to.equal(200);

        let response = JSON.parse(d.body);

        expect(response).to.have.property("error", false);
        expect(response).to.have.property("code", 200);
        expect(response).to.have.property("collections");

        const fullColName = internal.db._name() + '/' + collection.name();
        expect(response.collections).to.have.property(fullColName);
        expect(response.collections).to.have.all.keys([fullColName]);
        expect(response.collections[fullColName]).to.have.property('PlannedOperations');
        const plannedOperations = response.collections[fullColName]['PlannedOperations'];
        expect(plannedOperations).to.be.an('array').that.has.lengthOf(4);
      }

      { // This request should finish the repairs
        const postJobRes = request.post(
          coordinator.url + '/_admin/repair/distributeShardsLike',
          {
            headers: {"x-arango-async": "store"}
          }
        );
        const jobRes = waitForJob(postJobRes);

        expect(jobRes).to.have.property("status", 200);

        let response = JSON.parse(jobRes.body);

        const fullColName = internal.db._name() + '/' + collection.name();
        const originalExpectedOperations = expectedCollections[fullColName]['PlannedOperations'];

        expect(response).to.have.property("error", false);
        expect(response).to.have.property("code", 200);
        expect(response).to.have.property("collections");
        expect(response.collections).to.have.property(fullColName);
        expect(response.collections[fullColName]).to.have.property('PlannedOperations');
        const plannedOperations = response.collections[fullColName]['PlannedOperations'];
        expect(plannedOperations).to.be.an('array').that.has.lengthOf(4);
        expect(plannedOperations[0]).to.have.property('BeginRepairsOperation');
        expect(plannedOperations[0]['BeginRepairsOperation']).to.have.property('renameDistributeShardsLike', false);
        expect(plannedOperations[0]['BeginRepairsOperation']).to.eql({
          "database": internal.db._name(),
          "collection": collection.name(),
          "distributeShardsLike": protoCollection.name(),
          "renameDistributeShardsLike": false,
          "replicationFactor": protoCollection.properties().replicationFactor
        });
        expect(plannedOperations[1]).to.have.property('MoveShardOperation');
        expect(plannedOperations[1]).to.eql(originalExpectedOperations[2]);
        expect(plannedOperations[2]).to.have.property('FixServerOrderOperation');
        expect(plannedOperations[2]).to.eql(originalExpectedOperations[3]);
        expect(plannedOperations[3]).to.have.property('FinishRepairsOperation');
        expect(plannedOperations[3]).to.eql(originalExpectedOperations[4]);

        global.ArangoClusterInfo.flush();
      }

      // Note: properties() returns the name of the collection in distributeShardsLike
      // instead of the id!
      expect(
        collection.properties().distributeShardsLike
      ).to.equal(protoCollection.name());

      global.ArangoClusterInfo.flush();
      const shardDist = internal
        .getCollectionShardDistribution(collection._id)[collection.name()];
      const protoShardDist = internal
        .getCollectionShardDistribution(protoCollection._id)[protoCollection.name()];
      expectEqualShardDistributionPlan(shardDist, protoShardDist);
    });
  }

  it('if called via GET, only return planned operations', function() {
    const { protoCollection, collection, expectedCollections }
      = createBrokenClusterState();

    global.ArangoClusterInfo.flush();
    const previousShardDist = internal
      .getCollectionShardDistribution(collection._id)[collection.name()];

    const d = request.get(coordinator.url + '/_admin/repair/distributeShardsLike');

    expect(d.status).to.equal(200);

    let response = JSON.parse(d.body);

    expect(response).to.have.property("error", false);
    expect(response).to.have.property("code", 200);
    expect(response).to.have.property("collections");
    expect(response.collections).to.eql(expectedCollections);

    global.ArangoClusterInfo.flush();

    // Note: properties() returns the name of the collection in distributeShardsLike
    // instead of the id!
    expect(
      collection.properties().distributeShardsLike
    ).to.equal(protoCollection.name());

    global.ArangoClusterInfo.flush();
    const shardDist = internal
      .getCollectionShardDistribution(collection._id)[collection.name()];
    expectEqualShardDistributionPlan(shardDist, previousShardDist);
  });

});
