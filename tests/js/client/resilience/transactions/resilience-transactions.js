/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertFalse, assertEqual, fail, instanceManager */

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
/// @author Simon Grätzer
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const db = arangodb.db;
const ERRORS = arangodb.errors;
const _ = require("lodash");
const wait = require("internal").wait;
const suspendExternal = require("internal").suspendExternal;
const continueExternal = require("internal").continueExternal;
const {
  getDBServers,
  getEndpointById
} = require("@arangodb/test-helper");
const CI = require('@arangodb/cluster-info');

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
    var cinfo = CI.getCollectionInfo(database, collection);
    var shard = Object.keys(cinfo.shards)[0];
    return cinfo.shards[shard];
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief wait for synchronous replication
  ////////////////////////////////////////////////////////////////////////////////

  function waitForSynchronousReplication(database) {
    console.info("Waiting for synchronous replication to settle...");
    CI.flush();
    cinfo = CI.getCollectionInfo(database, cn);
    shards = Object.keys(cinfo.shards);
    var count = 0;
    var replicas;
    while (++count <= 300) {
      ccinfo = shards.map(
        s => CI.getCollectionInfoCurrent(database, cn, s)
      );
      console.info("Plan:", cinfo.shards, "Current:", ccinfo.map(s => s.servers));
      replicas = ccinfo.map(s => s.servers.length);
      if (replicas.every(x => x > 1)) {
        console.info("Replication up and running!");
        return true;
      }
      wait(0.5);
      CI.flush();
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
  /// @brief fail the follower, transaction should succeed regardless
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

      let trx = db._createTransaction({
        collections: { write: [cn] }
      });
      trx.query('FOR i IN ' + cn + ' REMOVE i._key IN ' + cn);
      failFollower();
      wait(15.0);
      healFollower();
      trx.commit();

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

