/* global describe, beforeEach, afterEach, it, instanceManager, before */
// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Andreas Streichardt
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////
'use strict';

const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse, assertNotEqual} = jsunity.jsUnity.assertions;
const arango = require("@arangodb").arango;
const db = require("@arangodb").db;
const { deriveTestSuite} = require('@arangodb/test-helper-common');
const CI = require('@arangodb/cluster-info');
const {
  getDBServerEndpoints, 
  getDBServers
} = require('@arangodb/test-helper');
const expect = require('chai').expect;
const assert = require('chai').assert;
const colName = "UnitTestDistributionTest";
const _ = require("lodash");
const wait = require("internal").wait;
const waitFor = require("@arangodb/testutils/replicated-logs-helper").waitFor;
const dbname = "shardDistDB";

let dbServerCount = getDBServerEndpoints().length;

function ShardDistributionTest({replVersion}) {

  let distribution;
  const nrShards = 16;

  function waitForClusterHealth() {
    // First wait until the cluster is complete, otherwise the creation
    // of the collection with replicationFactor dbServerCount will
    // fail, we use the health api:
    let count = 0;
    while (true) {
      if (++count >= 300) {
        throw "Did not find " + dbServerCount + " dbServers within 5 mins.";
      }
      let res = arango.GET('/_admin/cluster/health');
      assertTrue(res.Health);
      let health = res.Health;
      let serverCount = 0;
      let serverIds = Object.keys(health);
      for (let i = 0; i < serverIds.length; ++i) {
        if (serverIds[i].slice(0, 4) === "PRMR" &&
            health[serverIds[i]].Status === "GOOD") {
          serverCount += 1;
        }
      }
      if (serverCount >= dbServerCount) {
        break;
      }
      require("internal").wait(1);
    }
    db._create(colName, {replicationFactor: dbServerCount, numberOfShards: nrShards});
    let res = arango.GET('/_admin/cluster/shardDistribution');
    assertTrue(res.results);
    assertTrue(res.results[colName]);
    distribution = res.results[colName];
  }

  const followCollection = 'UnitTestDistributionFollower';
  const numberOfShards = 12;

  const cleanUp = function () {
    db._drop(followCollection);
  };

  const shardNumber = function (shard) {
    // Each shard starts with 's'
    expect(shard[0]).to.equal('s');
    // And is followed by a numeric value
    const nr = parseInt(shard.slice(1));
    expect(nr).to.be.above(0);
    return nr;
  };

  const sortShardsNumericly = function (l, r) {
    return shardNumber(l) - shardNumber(r);
  };

  const compareDistributions = function () {
    let res = arango.GET('/_admin/cluster/shardDistribution');
    assertTrue(res.results);
    let dist = res.results;

    const origPlan = dist[colName].Plan;
    const folPlan = dist[followCollection].Plan;
    expect(Object.keys(origPlan)).to.have.length.of(Object.keys(folPlan).length);

    let origCur = dist[colName].Current;
    let folCur = dist[followCollection].Current;
    let origShards = Object.keys(origCur).sort(sortShardsNumericly);
    let folShards = Object.keys(folCur).sort(sortShardsNumericly);

    if (replVersion === "2" && origShards.length !== folShards.length) {
      // For replication2, shards are reported to Current as they are created.
      // We have to wait until all shards are created before we can compare the distributions.
      waitFor(() => {
        res = arango.GET('/_admin/cluster/shardDistribution');
        assertTrue(res.results);
        let dist = res.results;

        origCur = dist[colName].Current;
        folCur = dist[followCollection].Current;
        origShards = Object.keys(origCur).sort(sortShardsNumericly);
        folShards = Object.keys(folCur).sort(sortShardsNumericly);

        if (origShards.length !== folShards.length) {
          return Error("Shard count does not match: " + origShards.length + " vs " + folShards.length);
        }
        return true;
      }, 100);
    }

    // Now we have all shard names sorted in alphabetical ordering.
    // It needs to be guaranteed that leader + follower of each shard in this ordering is identical.
    expect(origShards).to.have.length.of(folShards.length);
    for (let i = 0; i < origShards.length; ++i) {
      const oneOrigShard = origCur[origShards[i]];
      const oneFolShard = folCur[folShards[i]];
      // Leader has to be identical
      expect(oneOrigShard.leader).to.equal(oneFolShard.leader);
      // Follower Order does not matter, but needs to be the same servers
      expect(oneOrigShard.followers.sort()).to.deep.equal(oneFolShard.followers.sort());
    }
  };

  const replicationFactor = 3;
  // Note here: We have to make sure that numberOfShards * replicationFactor is not disible by the number of DBServers

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief order the cluster to clean out a server:
  ////////////////////////////////////////////////////////////////////////////////

  const cleanOutServer = function (id) {
    var body = {"server": id};
    try {
      return arango.POST("/_admin/cluster/cleanOutServer", body).error;
    } catch (err) {
      console.error(
          "Exception for POST /_admin/cluster/cleanOutServer:", err.stack);
      return false;
    }
  };

  const getCleanedOutServers = function () {
    try {
      let res = arango.GET('/_admin/cluster/numberOfServers');
      if (res.code !== 200) {
        return {cleanedServers: []};
      }
      var body = res;
      if (typeof body === "string") {
        body = JSON.parse(body);
      }
      if (typeof body !== "object" ||
          !body.hasOwnProperty("cleanedServers") ||
          typeof body.cleanedServers !== "object") {
        return {cleanedServers: []};
      }
      return body;
    } catch (err) {
      console.error(
          "Exception for GET /_admin/cluster/numberOfServers:", err.stack);
      return {cleanedServers: []};
    }
  };

  const waitForCleanout = function (id) {
    let count = 600;
    while (--count > 0) {
      let obj = getCleanedOutServers();
      if (obj.cleanedServers.indexOf(id) >= 0) {
        console.info(
            "Success: Server " + id + " cleaned out after " + (600 - count) + " seconds");
        return true;
      }
      wait(1.0);
    }
    console.error(
        "Failed: Server " + id + " not cleaned out after 600 seconds");
    return false;
  };

  const waitForSynchronousReplication = function (collection) {
    if (replVersion === "2") {
      return true;
    }
    CI.flush();
    var cinfo = CI.getCollectionInfo(
        dbname, collection);
    var shards = Object.keys(cinfo.shards);
    var replFactor = cinfo.shards[shards[0]].length;
    var count = 0;
    while (++count <= 600) {
      var ccinfo = shards.map(
          s => CI.getCollectionInfoCurrent(
              dbname, collection, s)
      );
      let replicas = ccinfo.map(s => s.servers);
      if (_.every(replicas, x => x.length === replFactor)) {
        return true;
      }
      if (count % 60 === 0) {
        console.info("waitForSynchronousReplication: cinfo:",
            JSON.stringify(cinfo), ", replicas: ",
            JSON.stringify(replicas));
      }
      wait(0.5);
      CI.flush();
    }
    console.error(`Collection "${collection}" failed to get all followers in sync after 600 sec`);
    return false;
  };


  return {
    setUpAll: function () {
      db._createDatabase(dbname, {replicationVersion: replVersion});
      db._useDatabase(dbname);
      db._drop(colName);
      if (replVersion !== "2") {
        waitForClusterHealth();
      }
    },
    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbname);
    },
    tearDown: function () {
      cleanUp();
      db._drop(colName);
    },

    testProperlyDistributeShards: function () {
      db._drop(colName);
      db._create(colName, {replicationFactor: 2, numberOfShards: 16});
      let res = arango.GET('/_admin/cluster/shardDistribution');
      assertTrue(res.results);
      let distribution = res.results;

      let leaders = Object.keys(distribution[colName].Current).reduce((current, shardKey) => {
        let shard = distribution[colName].Current[shardKey];
        if (current.indexOf(shard.leader) === -1) {
          current.push(shard.leader);
        }
        return current;
      }, []);
      expect(leaders).to.have.length.of.at.least(2);
    },

    testCurrentPlanTopLevel: function () {
      if (replVersion === "2") {
        return;
      }

      expect(distribution).to.have.all.keys(["Current", "Plan"]);
      assert.isObject(distribution.Current, 'The Current has to be an object');
      assert.isObject(distribution.Plan, 'The Current has to be an object');
    },

    testIdenticalShardsPlanCurrent: function () {
      if (replVersion === "2") {
        return;
      }

      let keys = Object.keys(distribution.Plan);
      expect(keys.length).to.equal(nrShards);
      // Check that keys (shardnames) are identical
      expect(distribution.Current).to.have.all.keys(distribution.Plan);
    },

    testShardPlanFormat: function () {
      if (replVersion === "2") {
        return;
      }

      _.forEach(distribution.Plan, function (info, shard) {
        if (info.hasOwnProperty('progress')) {
          expect(info).to.have.all.keys(['leader', 'progress', 'followers']);
          expect(info.progress).to.have.all.keys(['total', 'current']);
        } else {
          expect(info).to.have.all.keys(['leader', 'followers']);
        }
        expect(info.leader).to.match(/^DBServer|_/);
        // Note that it is possible that the leader is for a short time
        // a resigned leader starting with an "_".
        assert.isArray(info.followers, 'The followers need to be an array');
        // We have one replica for each server, except the leader
        expect(info.followers.length).to.equal(dbServerCount - 1);
        _.forEach(info.followers, function (follower) {
          expect(follower).to.match(/^DBServer/);
        });
      });
    },

    testShardCurrentFormat: function () {
      if (replVersion === "2") {
        return;
      }

      _.forEach(distribution.Current, function (info, shard) {
        expect(info).to.have.all.keys(['leader', 'followers']);

        expect(info.leader).to.match(/^DBServer|_/);
        assert.isArray(info.followers, 'The followers need to be an array');

        // We have at most one replica per db server. They may not be in sync yet.
        expect(info.followers.length).to.below(dbServerCount);
        _.forEach(info.followers, function (follower) {
          expect(follower).to.match(/^DBServer/);
        });
      });
    },

    testShouldDistributeAcrossAllServers: function () {
      if (replVersion === "2") {
        return;
      }

      let leaders = new Set();
      _.forEach(distribution.Plan, function (info, shard) {
        leaders.add(info.leader);
      });
      assertEqual(leaders.size, Math.min(dbServerCount, nrShards));
    },

    testDistributeShardsLikeDistribution: function () {
      cleanUp();
      db._create(colName, {replicationFactor, numberOfShards});
      db._create(followCollection, {
        replicationFactor,
        numberOfShards,
        distributeShardsLike: colName
      });
      assertTrue(waitForSynchronousReplication(followCollection));
      compareDistributions();
    },

    testDistributeShardsLikeOnIdenticalServers: function () {
      db._create(colName, {replicationFactor, numberOfShards});
      assertTrue(waitForSynchronousReplication(colName));
      db._create(followCollection, {
        replicationFactor,
        numberOfShards,
        distributeShardsLike: colName
      });
      assertTrue(waitForSynchronousReplication(followCollection));
      compareDistributions();
    },

    testDistributeShardsLikeFailover: function () {
      if (replVersion === "2") {
        return;
      }
      db._create(colName, {replicationFactor, numberOfShards});
      assertTrue(waitForSynchronousReplication(colName));
      let server = getDBServers()[1].id;
      // Clean out the server that is scheduled second.
      assertFalse(cleanOutServer(server));
      assertTrue(waitForCleanout(server));
      assertTrue(waitForSynchronousReplication(colName));
      // Now we have moved around some shards.
      db._create(followCollection, {replicationFactor, numberOfShards, distributeShardsLike: colName});
      assertTrue(waitForSynchronousReplication(followCollection));
      compareDistributions();
    }
  };
}

jsunity.run(function ShardDistributionTest_R1() {
  let derivedSuite = {};
  deriveTestSuite(ShardDistributionTest({replVersion: "1"}), derivedSuite, "_R1");
  return derivedSuite;
});

jsunity.run(function ShardDistributionTest_R2() {
  let derivedSuite = {};
  deriveTestSuite(ShardDistributionTest({replVersion: "2"}), derivedSuite, "_R2");
  return derivedSuite;
});

return jsunity.done();
