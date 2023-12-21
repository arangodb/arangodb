/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertFalse, assertEqual, fail, instanceManager */

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
const {
  arangoClusterInfoFlush, 
  getDBServers,
  arangoClusterInfoGetCollectionInfo,
  arangoClusterInfoGetCollectionInfoCurrent,
  getEndpointById
} = require("@arangodb/test-helper");

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
    var cinfo = arangoClusterInfoGetCollectionInfo(database, collection);
    var shard = Object.keys(cinfo.shards)[0];
    return cinfo.shards[shard];
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief wait for synchronous replication
  ////////////////////////////////////////////////////////////////////////////////

  function waitForSynchronousReplication(database) {
    console.info("Waiting for synchronous replication to settle...");
    arangoClusterInfoFlush();
    cinfo = arangoClusterInfoGetCollectionInfo(database, cn);
    shards = Object.keys(cinfo.shards);
    var count = 0;
    var replicas;
    while (++count <= 300) {
      ccinfo = shards.map(
        s => arangoClusterInfoGetCollectionInfoCurrent(database, cn, s)
      );
      console.info("Plan:", cinfo.shards, "Current:", ccinfo.map(s => s.servers));
      replicas = ccinfo.map(s => s.servers.length);
      if (replicas.every(x => x > 1)) {
        console.info("Replication up and running!");
        return true;
      }
      wait(0.5);
      arangoClusterInfoFlush();
    }
    console.error("Replication did not finish");
    return false;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fail the follower
  ////////////////////////////////////////////////////////////////////////////////

  function failFollower() {
    var follower = cinfo.shards[shards[0]][1];
    var endpoint = getEndpointById(follower);
    let arangods = getDBServers();

    var pos = _.findIndex(arangods,
                          x => x.url === endpoint);
  
    assertTrue(pos >= 0);
    assertTrue(suspendExternal(arangods[pos].pid));
    console.info("Have failed follower", follower);
    return pos;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief heal the follower
  ////////////////////////////////////////////////////////////////////////////////

  function healFollower() {
    var follower = cinfo.shards[shards[0]][1];
    var endpoint = getEndpointById(follower);
    let arangods = getDBServers();

    var pos = _.findIndex(arangods,
                          x => x.url === endpoint);
    assertTrue(pos >= 0);
    assertTrue(continueExternal(arangods[pos].pid));
    console.info("Have healed follower", follower);
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

