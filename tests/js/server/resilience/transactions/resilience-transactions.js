/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertFalse, assertEqual, fail, instanceInfo */

////////////////////////////////////////////////////////////////////////////////
/// @brief test synchronous replication in the cluster
///
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const db = arangodb.db;
const ERRORS = arangodb.errors;
const tasks = require("@arangodb/tasks");
const _ = require("lodash");
const wait = require("internal").wait;
const suspendExternal = require("internal").suspendExternal;
const continueExternal = require("internal").continueExternal;

function getDBServers() {
  var tmp = global.ArangoClusterInfo.getDBServers();
  var servers = [];
  for (var i = 0; i < tmp.length; ++i) {
    servers[i] = tmp[i].serverId;
  }
  return servers;
}

const tasksCompleted = () => {
  return 0 === tasks.get().filter((task) => {
    return (task.id.match(/^UnitTest/) || task.name.match(/^UnitTest/));
  }).length;
};
const waitForTasks = () => {
  const time = require("internal").time;
  const start = time();
  while (!tasksCompleted()) {
    if (time() - start > 300) { // wait for 5 minutes maximum
      fail("Timeout after 5 minutes");
    }
    require("internal").wait(0.5, false);
  }
  require('internal').wal.flush(true, true);
  // wait an extra second for good measure
  require("internal").wait(1.0, false);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ClusterTransactionSuite() {
  'use strict';
  var cn = "UnitTestClusterTrx";
  var c;
  var cinfo;
  var ccinfo;
  var shards;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find out servers for the system collections
  ////////////////////////////////////////////////////////////////////////////////

  function findCollectionServers(database, collection) {
    var cinfo = global.ArangoClusterInfo.getCollectionInfo(database, collection);
    var shard = Object.keys(cinfo.shards)[0];
    return cinfo.shards[shard];
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief wait for synchronous replication
  ////////////////////////////////////////////////////////////////////////////////

  function waitForSynchronousReplication(database) {
    console.info("Waiting for synchronous replication to settle...");
    global.ArangoClusterInfo.flush();
    cinfo = global.ArangoClusterInfo.getCollectionInfo(database, cn);
    shards = Object.keys(cinfo.shards);
    var count = 0;
    var replicas;
    while (++count <= 300) {
      ccinfo = shards.map(
        s => global.ArangoClusterInfo.getCollectionInfoCurrent(database, cn, s)
      );
      console.info("Plan:", cinfo.shards, "Current:", ccinfo.map(s => s.servers));
      replicas = ccinfo.map(s => s.servers.length);
      if (replicas.every(x => x > 1)) {
        console.info("Replication up and running!");
        return true;
      }
      wait(0.5);
      global.ArangoClusterInfo.flush();
    }
    console.error("Replication did not finish");
    return false;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fail the follower
  ////////////////////////////////////////////////////////////////////////////////

  function failFollower() {
    var follower = cinfo.shards[shards[0]][1];
    var endpoint = global.ArangoClusterInfo.getServerEndpoint(follower);
    // Now look for instanceInfo:
    var pos = _.findIndex(global.instanceInfo.arangods,
      x => x.endpoint === endpoint);
    assertTrue(pos >= 0);
    assertTrue(suspendExternal(global.instanceInfo.arangods[pos].pid));
    console.info("Have failed follower", follower);
    return pos;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief heal the follower
  ////////////////////////////////////////////////////////////////////////////////

  function healFollower() {
    var follower = cinfo.shards[shards[0]][1];
    var endpoint = global.ArangoClusterInfo.getServerEndpoint(follower);
    // Now look for instanceInfo:
    var pos = _.findIndex(global.instanceInfo.arangods,
      x => x.endpoint === endpoint);
    assertTrue(pos >= 0);
    assertTrue(continueExternal(global.instanceInfo.arangods[pos].pid));
    console.info("Have healed follower", follower);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fail the leader
  ////////////////////////////////////////////////////////////////////////////////

  function failLeader() {
    var leader = cinfo.shards[shards[0]][0];
    var endpoint = global.ArangoClusterInfo.getServerEndpoint(leader);
    // Now look for instanceInfo:
    var pos = _.findIndex(global.instanceInfo.arangods,
      x => x.endpoint === endpoint);
    assertTrue(pos >= 0);
    assertTrue(suspendExternal(global.instanceInfo.arangods[pos].pid));
    console.info("Have failed leader", leader);
    return leader;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief heal the follower
  ////////////////////////////////////////////////////////////////////////////////

  function healLeader() {
    var leader = cinfo.shards[shards[0]][0];
    var endpoint = global.ArangoClusterInfo.getServerEndpoint(leader);
    // Now look for instanceInfo:
    var pos = _.findIndex(global.instanceInfo.arangods,
      x => x.endpoint === endpoint);
    assertTrue(pos >= 0);
    assertTrue(continueExternal(global.instanceInfo.arangods[pos].pid));
    console.info("Have healed leader", leader);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the actual tests
  ////////////////////////////////////////////////////////////////////////////////

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var systemCollServers = findCollectionServers("_system", "_graphs");
      console.info("System collections use servers:", systemCollServers);
      while (true) {
        db._drop(cn);
        c = db._create(cn, {
          numberOfShards: 1, replicationFactor: 2,
          avoidServers: systemCollServers
        });
        var servers = findCollectionServers("_system", cn);
        console.info("Test collections uses servers:", servers);
        if (_.intersection(systemCollServers, servers).length === 0) {
          return;
        }
        console.info("Need to recreate collection to avoid system collection servers.");
        //waitForSynchronousReplication("_system");
        console.info("Synchronous replication has settled, now dropping again.");
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop(cn);
      //global.ArangoAgency.set('Target/FailedServers', {});
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check whether we have access to global.instanceInfo
    ////////////////////////////////////////////////////////////////////////////////
    testCheckInstanceInfo : function () {
      assertTrue(global.instanceInfo !== undefined);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check if a synchronously replicated collection gets online
    ////////////////////////////////////////////////////////////////////////////////

    testSetup : function () {
      for (var count = 0; count < 120; ++count) {
        let dbservers = getDBServers();
        if (dbservers.length === 5) {
          assertTrue(waitForSynchronousReplication("_system"));
          return;
        }
        console.log("Waiting for 5 dbservers to be present:", JSON.stringify(dbservers));
        wait(1.0);
      }
      assertTrue(false, "Timeout waiting for 5 dbservers.");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check transaction abort when a leader fails
    ////////////////////////////////////////////////////////////////////////////////
    /*testFailLeader: function () {
      assertTrue(waitForSynchronousReplication("_system"));

      let docs = [];
      let x = 0;
      while (x++ < 1000) {
        docs.push({_key: 'test' + x});
      }
      db._collection(cn).save(docs);
      assertEqual(db._collection(cn).count(), 1000);

      const cmd = `const db = require('internal').db; 
      var trx = { 
        collections: { write: ['${cn}'] }, 
        action: function () {
          const db = require('internal').db; 
          var ops = db._query('FOR i IN ${cn} REMOVE i._key IN ${cn}').getExtra().stats; 
          require('internal').sleep(25.0);
        }
      };
      db._executeTransaction(trx);`;
      
      tasks.register({ name: "UnitTestsSlowTrx", command: cmd });
      wait(2.0);
      failLeader();
      wait(15.0); // wait for longer than grace period
      healLeader();
      waitForTasks();

      // transaction should have been aborted
      assertTrue(waitForSynchronousReplication("_system"));
      assertEqual(db._collection(cn).count(), 1000);
      assertEqual(db._collection(cn).all().toArray().length, 1000);
    },*/
    
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fail the follower, transaction should succeeed regardless
  ////////////////////////////////////////////////////////////////////////////////
    testFailFollower: function () {
      assertTrue(waitForSynchronousReplication("_system"));

      let docs = [];
      let x = 0;
      while (x++ < 1000) {
        docs.push({_key: 'test' + x});
      }
      db._collection(cn).save(docs);
      assertEqual(db._collection(cn).count(), 1000);

      const cmd = `const db = require('internal').db; 
      var trx = { 
        collections: { write: ['${cn}'] }, 
        action: function () {
          const db = require('internal').db; 
          var ops = db._query('FOR i IN ${cn} REMOVE i._key IN ${cn}').getExtra().stats; 
          require('internal').sleep(25.0);
        }
      };
      db._executeTransaction(trx);`;
      
      tasks.register({ name: "UnitTestsSlowTrx", command: cmd });
      wait(2.0);
      failFollower();
      wait(15.0); // wait for longer than grace period
      healFollower();
      waitForTasks();

      // transaction should have been successful
      assertTrue(waitForSynchronousReplication("_system"));
      assertEqual(db._collection(cn).count(), 0);
      assertEqual(db._collection(cn).all().toArray().length, 0);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ClusterTransactionSuite);

return jsunity.done();

