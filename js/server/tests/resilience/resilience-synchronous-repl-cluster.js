/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertFalse, assertEqual, fail, instanceInfo */

////////////////////////////////////////////////////////////////////////////////
/// @brief test synchronous replication in the cluster
///
/// @file js/server/tests/resilience/resilience-synchronous-replication-cluster.js
///
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const db = arangodb.db;
const ERRORS = arangodb.errors;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function SynchronousReplicationSuite () {
  'use strict';
  var cn = "UnitTestSyncRep";
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
/// @brief produce failure
////////////////////////////////////////////////////////////////////////////////

  function makeFailure(failure) {
    if (failure.follower) {
      failFollower();
    } else {
      failLeader();
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief heal failure
////////////////////////////////////////////////////////////////////////////////

  function healFailure(failure) {
    if (failure.follower) {
      healFollower();
    } else {
      healLeader();
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief basic operations, with various failure modes:
////////////////////////////////////////////////////////////////////////////////

  function runBasicOperations(failure, healing) {
    if (failure.place === 1) { makeFailure(failure); }

    // Insert with check:
    var id = c.insert({Hallo:12});
    assertEqual(1, c.count());

    if (healing.place === 1) { healFailure(healing); }
    if (failure.place === 2) { makeFailure(failure); }

    var doc = c.document(id._key);
    assertEqual(12, doc.Hallo);

    if (healing.place === 2) { healFailure(healing); }
    if (failure.place === 3) { makeFailure(failure); }

    var ids = c.insert([{Hallo:13}, {Hallo:14}]);
    assertEqual(3, c.count());
    assertEqual(2, ids.length);

    if (healing.place === 3) { healFailure(healing); }
    if (failure.place === 4) { makeFailure(failure); }

    var docs = c.document([ids[0]._key, ids[1]._key]);
    assertEqual(2, docs.length);
    assertEqual(13, docs[0].Hallo);
    assertEqual(14, docs[1].Hallo);

    if (healing.place === 4) { healFailure(healing); }
    if (failure.place === 5) { makeFailure(failure); }

    // Replace with check:
    c.replace(id._key, {"Hallo": 100});

    if (healing.place === 5) { healFailure(healing); }
    if (failure.place === 6) { makeFailure(failure); }

    doc = c.document(id._key);
    assertEqual(100, doc.Hallo);

    if (healing.place === 6) { healFailure(healing); }
    if (failure.place === 7) { makeFailure(failure); }

    c.replace([ids[0]._key, ids[1]._key], [{Hallo:101}, {Hallo:102}]);

    if (healing.place === 7) { healFailure(healing); }
    if (failure.place === 8) { makeFailure(failure); }

    docs = c.document([ids[0]._key, ids[1]._key]);
    assertEqual(2, docs.length);
    assertEqual(101, docs[0].Hallo);
    assertEqual(102, docs[1].Hallo);

    if (healing.place === 8) { healFailure(healing); }
    if (failure.place === 9) { makeFailure(failure); }

    // Update with check:
    c.update(id._key, {"Hallox": 105});

    if (healing.place === 9) { healFailure(healing); }
    if (failure.place === 10) { makeFailure(failure); }

    doc = c.document(id._key);
    assertEqual(100, doc.Hallo);
    assertEqual(105, doc.Hallox);

    if (healing.place === 10) { healFailure(healing); }
    if (failure.place === 11) { makeFailure(failure); }

    c.update([ids[0]._key, ids[1]._key], [{Hallox:106}, {Hallox:107}]);

    if (healing.place === 11) { healFailure(healing); }
    if (failure.place === 12) { makeFailure(failure); }

    docs = c.document([ids[0]._key, ids[1]._key]);
    assertEqual(2, docs.length);
    assertEqual(101, docs[0].Hallo);
    assertEqual(102, docs[1].Hallo);
    assertEqual(106, docs[0].Hallox);
    assertEqual(107, docs[1].Hallox);

    if (healing.place === 12) { healFailure(healing); }
    if (failure.place === 13) { makeFailure(failure); }

    // AQL:
    var q = db._query(`FOR x IN @@cn
                         FILTER x.Hallo > 0
                         SORT x.Hallo
                         RETURN {"Hallo": x.Hallo}`, {"@cn": cn});
    docs = q.toArray();
    assertEqual(3, docs.length);
    assertEqual([{Hallo:100}, {Hallo:101}, {Hallo:102}], docs);

    if (healing.place === 13) { healFailure(healing); }
    if (failure.place === 14) { makeFailure(failure); }

    // Remove with check:
    c.remove(id._key);

    if (healing.place === 14) { healFailure(healing); }
    if (failure.place === 15) { makeFailure(failure); }

    try {
      doc = c.document(id._key);
      fail();
    }
    catch (e1) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, e1.errorNum);
    }

    if (healing.place === 15) { healFailure(healing); }
    if (failure.place === 16) { makeFailure(failure); }

    assertEqual(2, c.count());

    if (healing.place === 16) { healFailure(healing); }
    if (failure.place === 17) { makeFailure(failure); }

    c.remove([ids[0]._key, ids[1]._key]);

    if (healing.place === 17) { healFailure(healing); }
    if (failure.place === 18) { makeFailure(failure); }

    docs = c.document([ids[0]._key, ids[1]._key]);
    assertEqual(2, docs.length);
    assertTrue(docs[0].error);
    assertTrue(docs[1].error);

    if (healing.place === 18) { healFailure(healing); }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual tests
////////////////////////////////////////////////////////////////////////////////

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var systemCollServers = findCollectionServers("_system", "_graphs");
      console.info("System collections use servers:", systemCollServers);
      while (true) {
        db._drop(cn);
        c = db._create(cn, {numberOfShards: 1, replicationFactor: 2,
                            avoidServers: systemCollServers});
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

    tearDown : function () {
      db._drop(cn);
      //global.ArangoAgency.set('Target/FailedServers', {});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check inquiry functionality
////////////////////////////////////////////////////////////////////////////////
    testInquiry : function () {
      console.warn("Checking inquiry");
      var writeResult = global.ArangoAgency.write(
        [[{"a":1},{"a":{"oldEmpty":true}},"INTEGRATION_TEST_INQUIRY_ERROR_503"]]);
      console.log(
        "Inquired successfully a matched precondition under 503 response from write");
      assertTrue(typeof writeResult === "object");
      assertTrue(writeResult !== null);
      assertTrue("results" in writeResult);
      assertTrue(writeResult.results[0]>0);
      try {
        writeResult = global.ArangoAgency.write(
          [[{"a":1},{"a":0},"INTEGRATION_TEST_INQUIRY_ERROR_503"]]);
        fail();
      } catch (e1) {
        console.log(
          "Inquired successfully a failed precondition under 503 response from write");
      }
      writeResult = global.ArangoAgency.write(
        [[{"a":1},{"a":1},"INTEGRATION_TEST_INQUIRY_ERROR_0"]]);
      console.log(
        "Inquired successfully a matched precondition under 0 response from write");
      try {
        writeResult = global.ArangoAgency.write(
          [[{"a":1},{"a":0},"INTEGRATION_TEST_INQUIRY_ERROR_0"]]);
        fail();
      } catch (e1) {
        console.log(
          "Inquired successfully a failed precondition under 0 response from write");
      }
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
/// @brief run a standard check without failures:
////////////////////////////////////////////////////////////////////////////////

    testBasicOperations : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({}, {});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief run a standard check with failures:
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFailureFollower : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      failFollower();
      runBasicOperations({}, {});
      healFollower();
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 1
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail1 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:1, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 2
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail2 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:2, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 3
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail3 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:3, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 4
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail4 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:4, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 5
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail5 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:5, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 6
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail6 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:6, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 7
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail7 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:7, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 8
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail8 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:8, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 9
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail9 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:9, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 10
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail10 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:10, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 11
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail11 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:11, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 12
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail12 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:12, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 13
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail13 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:13, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 14
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail14 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:14, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 15
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail15 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:15, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 16
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail16 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:16, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 17
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail17 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:17, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail in place 18
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail18 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:18, follower:true}, {place:18, follower:true});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief run a standard check with failures:
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFailureLeader : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      failLeader();
      runBasicOperations({}, {});
      healLeader();
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 1
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail1 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:1, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 2
////////////////////////////////////////////////////////////////////////////////
    testBasicOperationsLeaderFail2 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:2, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 3
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail3 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:3, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 4
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail4 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:4, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 5
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail5 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:5, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 6
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail6 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:6, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 7
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail7 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:7, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 8
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail8 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:8, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 9
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail9 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:9, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 10
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail10 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:10, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 11
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail11 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:11, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 12
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail12 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:12, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 13
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail13 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:13, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 14
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail14 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:14, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 15
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail15 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:15, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 16
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail16 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:16, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 17
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail17 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:17, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief fail leader in place 18
////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsLeaderFail18 : function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations({place:18, follower: false},
                         {place:18, follower: false});
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief just to allow a trailing comma at the end of the last test
////////////////////////////////////////////////////////////////////////////////

    testDummy : function () {
      assertEqual(12, 12);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SynchronousReplicationSuite);

return jsunity.done();
