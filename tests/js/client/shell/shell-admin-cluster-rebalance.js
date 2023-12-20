/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, assertNotEqual, assertNotNull, assertFalse, arango */
'use strict';
// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDB Enterprise License Tests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB Inc, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB Inc, Cologne, Germany
// /
// / @author Lars Maier
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
const arangodb = require('@arangodb');
const db = arangodb.db;
const {getDBServers} = require('@arangodb/test-helper');
const internal = require('internal');
const database = "cluster_rebalance_db";

const suspendExternal = internal.suspendExternal;
const continueExternal = require("internal").continueExternal;
const wait = require("internal").wait;


function resignServer(server) {
  let res = arango.POST_RAW("/_admin/cluster/resignLeadership", {server});
  assertEqual(202, res.code);
  const id = res.parsedBody.id;

  let count = 10;
  while (--count >= 0) {
    require("internal").wait(5.0, false);
    res = arango.GET_RAW("/_admin/cluster/queryAgencyJob?id=" + id);
    if (res.code === 200 && res.parsedBody.status === "Finished") {
      return;
    }
  }
  assertTrue(false, `We failed to resign a leader in 50s. We cannot reliably test rebalancing of shards now.`);
}

function getRebalancePlan(moveLeaders, moveFollowers, leaderChanges) {
  const result = arango.POST('/_admin/cluster/rebalance', {
    version: 1,
    moveLeaders: moveLeaders,
    moveFollowers: moveFollowers,
    leaderChanges: leaderChanges,
    databasesExcluded: ["_system"],
  });
  assertEqual(result.code, 200);
  assertEqual(result.error, false);
  return result;
}

function getServersHealth() {
  let result = arango.GET_RAW('/_admin/cluster/health');
  assertTrue(result.parsedBody.hasOwnProperty("Health"));
  return result.parsedBody.Health;
}

function clusterRebalanceSuite() {
  let prevDB = null;
  return {
    setUpAll: function () {
      prevDB = db._name();
      db._createDatabase(database);
      db._useDatabase(database);
      for (let i = 0; i < 20; i++) {
        db._create("col" + i, {replicationFactor: 2});
      }

      // resign one server
      resignServer(getDBServers()[0].id);
    },

    tearDownAll: function () {
      for (let i = 0; i < 20; i++) {
        db._drop("col" + i);
      }
      db._useDatabase(prevDB);
      db._dropDatabase(database);
    },

    testGetImbalance: function () {
      let result = arango.GET('/_admin/cluster/rebalance');
      assertEqual(result.code, 200);
      assertEqual(result.error, false);
    },
    testCalcRebalanceVersion: function () {
      let result = arango.POST('/_admin/cluster/rebalance', {
        version: 3
      });
      assertEqual(result.code, 400);
    },
    testCalcRebalance: function () {
      let result = arango.POST('/_admin/cluster/rebalance', {
        version: 1,
        moveLeaders: true,
        moveFollowers: true,
        leaderChanges: true,
        databasesExcluded: ["_system"],
      });
      assertEqual(result.code, 200); // should be refused
      assertEqual(result.error, false); // should be refused
      const moves = result.result.moves;
      for (const job of moves) {
        assertNotEqual(job.database, "_system");
      }
    },

    testCalcRebalanceAndExecute: function () {
      let result = arango.POST('/_admin/cluster/rebalance', {
        version: 1,
        moveLeaders: true,
        moveFollowers: true,
        leaderChanges: true,
        databasesExcluded: ["_system"],
      });
      assertEqual(result.code, 200);
      assertEqual(result.error, false);
      let moves = result.result.moves;
      for (const job of moves) {
        assertNotEqual(job.database, "_system");
      }

      result = arango.POST('/_admin/cluster/rebalance/execute', {version: 1, moves});
      if (moves.length > 0) {
        assertEqual(result.code, 202);
      } else {
        assertEqual(result.code, 200);
      }
      assertEqual(result.error, false);

      // empty set of moves
      result = arango.POST('/_admin/cluster/rebalance/execute', {version: 1, moves: []});
      assertEqual(result.code, 200);
      assertEqual(result.error, false);

      while (true) {
        require('internal').wait(0.5);
        let result = arango.GET('/_admin/cluster/rebalance');
        assertEqual(result.code, 200);
        assertEqual(result.error, false);

        if (result.result.pendingMoveShards === 0 && result.result.todoMoveShards === 0) {
          break;
        }
      }
    },

    testExecuteRebalanceVersion: function () {
      let result = arango.POST('/_admin/cluster/rebalance/execute', {
        version: 3, moves: []
      });
      assertEqual(result.code, 400);
    },
  };
}

function clusterRebalanceOtherOptionsSuite() {
  let prevDB = null;
  const numCols = 3;

  return {

    setUpAll: function () {
      prevDB = db._name();
      db._createDatabase(database);
      db._useDatabase(database);
      for (let i = 0; i < numCols; ++i) {
        db._create("col" + i, {numberOfShards: 2, replicationFactor: 2});
      }

      let docs = [];
      for (let i = 0; i < numCols; ++i) {
        for (let j = 0; j < 1000; ++j) {
          docs.push({"value": j});
        }
        db["col" + i].insert(docs);
        docs = [];
      }
    },

    tearDownAll: function () {
      db._useDatabase(prevDB);
      db._dropDatabase(database);
    },

    testCalcRebalanceStopServer: function () {
      const dbServers = global.instanceManager.arangods.filter(arangod => arangod.instanceRole === "dbserver");
      assertNotEqual(dbServers.length, 0);
      for (let i = 0; i < dbServers.length; ++i) {
        const dbServer = dbServers[i];
        assertTrue(dbServer.suspend());
        try {
          let serverHealth = null;
          let startTime = Date.now();
          let result = null;
          do {
            wait(2);
            result = getServersHealth();
            assertNotNull(result);
            serverHealth = result[dbServer.id].Status;
            assertNotNull(serverHealth);
            const timeElapsed = (Date.now() - startTime) / 1000;
            assertTrue(timeElapsed < 300, "Server expected status not acquired");
          } while (serverHealth !== "FAILED");
          const serverShortName = result[dbServer.id].ShortName;
          assertEqual(serverHealth, "FAILED");
          startTime = Date.now();
          let serverUsed;
          do {
            wait(2);
            serverUsed = false;
            for (let j = 0; j < numCols; ++j) {
              result = arango.GET("/_admin/cluster/shardDistribution").results["col" + j].Plan;
              for (let key of Object.keys(result)) {
                if (result[key].leader === serverShortName) {
                  serverUsed = true;
                  break;
                }
                result[key].followers.forEach(follower => {
                  if (follower === serverShortName) {
                    serverUsed = true;
                  }
                });
              }
              if (serverUsed === true) {
                break;
              }
              const timeElapsed = (Date.now() - startTime) / 1000;
              assertTrue(timeElapsed < 300, "Moving shards from server in ill state not acquired");
            }
          } while (serverUsed);

          result = getRebalancePlan(true, true, true);
          let moves = result.result.moves;
          for (const job of moves) {
            assertNotEqual(job.to, dbServer.id);
          }
        } finally {
          assertTrue(dbServer.resume());
          let serverHealth = null;
          const startTime = Date.now();
          do {
            wait(2);
            const result = getServersHealth();
            assertNotNull(result);
            serverHealth = result[dbServer.id].Status;
            assertNotNull(serverHealth);
            const timeElapsed = (Date.now() - startTime) / 1000;
            assertTrue(timeElapsed < 300, "Unable to get server " + dbServer.id + " in good state");
          } while (serverHealth !== "GOOD");
        }
      }
    },

    testCalcRebalanceNotMoveLeaders: function () {
      const plan = arango.GET("/_admin/cluster/shardDistribution").results["col1"].Plan;
      Object.entries(plan).forEach((shardInfo) => {
        const [shardName, servers] = shardInfo;
        const leader = servers.leader;
        let result = getServersHealth();
        let leaderId = null;
        Object.entries(result).forEach((serversHealth) => {
          const [serverId, value] = serversHealth;
          if (value.ShortName !== undefined && value.ShortName === leader) {
            leaderId = serverId;
          }
        });
        assertNotNull(leaderId);
        result = getRebalancePlan(false, true, false);
        let moves = result.result.moves;
        for (const job of moves) {
          if (job.shard === shardName) {
            assertNotEqual(job.from, leaderId);
            assertFalse(job.isLeader);
          }
        }
      });
    },

    testCalcRebalanceNotMoveFollowers: function () {
      const plan = arango.GET("/_admin/cluster/shardDistribution").results["col1"].Plan;
      Object.entries(plan).forEach((shardInfo) => {
        const [shardName, servers] = shardInfo;
        const follower = servers.followers[0]; // replication factor = 2, hence, 1 follower
        let result = getServersHealth();
        let followerId = null;
        Object.entries(result).forEach((serversHealth) => {
          const [serverId, value] = serversHealth;
          if (value.ShortName !== undefined && value.ShortName === follower) {
            followerId = serverId;
          }
        });
        assertNotNull(followerId);

        result = getRebalancePlan(true, false, true);
        let moves = result.result.moves;
        for (const job of moves) {
          if (job.shard === shardName) {
            assertNotEqual(job.from, followerId);
            assertTrue(job.isLeader);
          }
        }
      });
    },

  };
}

function clusterRebalanceWithMovesToMakeSuite() {
  let prevDB = null;
  const cn = "col1";

  return {

    setUpAll: function () {
      prevDB = db._name();
      db._createDatabase(database);
      db._useDatabase(database);
    },

    tearDownAll: function () {
      db._useDatabase(prevDB);
      db._dropDatabase(database);
    },


    testCalcRebalanceAfterUnbalanced: function () {
      const start = internal.time();
      const end = start + 300;
      for (let i = 1; i <= 3; ++i) {
        const toServer = "DBServer000" + i;
        db._create(cn, {numberOfShards: 8, replicationFactor: 1});
        try {
          const plan = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;
          Object.entries(plan).forEach((shardInfo) => {
            const [shardName, servers] = shardInfo;
            const leader = servers.leader;
            if (leader === toServer) {
              return;
            }
            let moveShardJob = {
              database: database,
              collection: cn,
              shard: shardName,
              fromServer: leader,
              toServer: toServer,
              isLeader: true,
              remainsFollower: false
            };
            const result = arango.POST("/_admin/cluster/moveShard", moveShardJob);
            assertEqual(result.code, 202);
            while (true) {
              if (internal.time() >= end) {
                assertFalse(true, "test timed out");
              }
              let res2 = arango.GET(`/_admin/cluster/queryAgencyJob?id=${result.id}`);
              if (res2.status === "Finished") {
                break;
              }
              internal.wait(0.5);
            }
          });
          const plan2 = arango.GET("/_admin/cluster/shardDistribution").results[cn].Plan;
          Object.entries(plan2).forEach((shardInfo) => {
            const [, servers] = shardInfo;
            const leader = servers.leader;
            assertEqual(leader, toServer);
          });
          const result = getRebalancePlan(true, true, true);
          const moves = result.result.moves;
          assertTrue(moves.length > 0);
        } finally {
          db._drop(cn);
        }
      }
    },
  };
}

jsunity.run(clusterRebalanceSuite);
jsunity.run(clusterRebalanceOtherOptionsSuite);
jsunity.run(clusterRebalanceWithMovesToMakeSuite);
return jsunity.done();
