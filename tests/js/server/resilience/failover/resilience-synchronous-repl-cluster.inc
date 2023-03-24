/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertFalse, assertEqual, fail, instanceInfo */

////////////////////////////////////////////////////////////////////////////////
/// @brief test synchronous replication in the cluster
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

'use strict';

(function () {
  const arangodb = require("@arangodb");
  const db = arangodb.db;
  const ERRORS = arangodb.errors;
  const _ = require("lodash");
  const wait = require("internal").wait;
  const suspendExternal = require("internal").suspendExternal;
  const continueExternal = require("internal").continueExternal;

  let cn = "UnitTestSyncRep";
  let c;
  let cinfo;
  let ccinfo;
  let shards;
  let failedPIDs = [];

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
      replicas = ccinfo.map(s => [s.servers.length, s.failoverCandidates.length]);
      if (replicas.every(x => x[0] > 1 && x[0] === x[1])) {
        // This also checks that there are as many failoverCandidates
        // as there are followers in sync. This should eventually be
        // reached.
        console.info("Replication up and running!");
        // The following wait has a purpose, so please do not remove it.
        // We have just seen that all followers are in sync. However, this
        // means that the leader has told the agency so, it has not necessarily
        // responded to the followers, so they might still be in
        // SynchronizeShard. If we STOP the leader too quickly in a subsequent
        // test, then the follower might get stuck in SynchronizeShard
        // and the expected failover cannot happen. A second should be plenty
        // of time to receive the response and finish the SynchronizeShard
        // operation.
        wait(1);
        return true;
      }
      wait(0.5);
      global.ArangoClusterInfo.flush();
    }
    console.error("Replication did not finish");
    return false;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief heal any not fixed instances
  ////////////////////////////////////////////////////////////////////////////////
  function healAny() {
    failedPIDs.forEach(pid => {
      console.warn(`heal any: healing ${pid}`);
      assertTrue(continueExternal(pid));
    });
    failedPIDs = [];
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fail the follower
  ////////////////////////////////////////////////////////////////////////////////

  function failFollower() {
    var follower = cinfo.shards[shards[0]][1];
    var endpoint = global.ArangoClusterInfo.getServerEndpoint(follower);
    // Now look for instanceManager:
    var pos = _.findIndex(global.instanceManager.arangods,
      x => x.endpoint === endpoint);
    assertTrue(pos >= 0);
    var pid = global.instanceManager.arangods[pos].pid;
    failedPIDs.push(pid);
    assertTrue(suspendExternal(pid));
    console.info("Have failed follower", follower);
    return pos;
  }


  ////////////////////////////////////////////////////////////////////////////////
  /// @brief heal the follower
  ////////////////////////////////////////////////////////////////////////////////

  function healFollower() {
    var follower = cinfo.shards[shards[0]][1];
    var endpoint = global.ArangoClusterInfo.getServerEndpoint(follower);
    // Now look for instanceManager:
    var pos = _.findIndex(global.instanceManager.arangods,
      x => x.endpoint === endpoint);
    assertTrue(pos >= 0);
    var pid = global.instanceManager.arangods[pos].pid;
    assertTrue(continueExternal(pid));
    failedPIDs.splice(failedPIDs.indexOf(pid), 1);
    console.info("Have healed follower", follower);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fail the leader
  ////////////////////////////////////////////////////////////////////////////////

  function failLeader() {
    var leader = cinfo.shards[shards[0]][0];
    var endpoint = global.ArangoClusterInfo.getServerEndpoint(leader);
    // Now look for instanceManager:
    var pos = _.findIndex(global.instanceManager.arangods,
      x => x.endpoint === endpoint);
    assertTrue(pos >= 0);
    var pid = global.instanceManager.arangods[pos].pid;
    failedPIDs.push(pid);
    assertTrue(suspendExternal(pid));
    console.info("Have failed leader", leader);
    return leader;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief heal the follower
  ////////////////////////////////////////////////////////////////////////////////

  function healLeader() {
    var leader = cinfo.shards[shards[0]][0];
    var endpoint = global.ArangoClusterInfo.getServerEndpoint(leader);
    // Now look for instanceManager:
    var pos = _.findIndex(global.instanceManager.arangods,
      x => x.endpoint === endpoint);
    assertTrue(pos >= 0);
    var pid = global.instanceManager.arangods[pos].pid;
    assertTrue(continueExternal(pid));
    failedPIDs.splice(failedPIDs.indexOf(pid), 1);
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
    try {
      if (failure.place === 1) { makeFailure(failure); }

      // Insert with check:
      var id = c.insert({ Hallo: 12 });
      assertEqual(1, c.count());

      if (healing.place === 1) { healFailure(healing); }
      if (failure.place === 2) { makeFailure(failure); }

      var doc = c.document(id._key);
      assertEqual(12, doc.Hallo);

      if (healing.place === 2) { healFailure(healing); }
      if (failure.place === 3) { makeFailure(failure); }

      var ids = c.insert([{ Hallo: 13 }, { Hallo: 14 }]);
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
      c.replace(id._key, { "Hallo": 100 });

      if (healing.place === 5) { healFailure(healing); }
      if (failure.place === 6) { makeFailure(failure); }

      doc = c.document(id._key);
      assertEqual(100, doc.Hallo);

      if (healing.place === 6) { healFailure(healing); }
      if (failure.place === 7) { makeFailure(failure); }

      c.replace([ids[0]._key, ids[1]._key], [{ Hallo: 101 }, { Hallo: 102 }]);

      if (healing.place === 7) { healFailure(healing); }
      if (failure.place === 8) { makeFailure(failure); }

      docs = c.document([ids[0]._key, ids[1]._key]);
      assertEqual(2, docs.length);
      assertEqual(101, docs[0].Hallo);
      assertEqual(102, docs[1].Hallo);

      if (healing.place === 8) { healFailure(healing); }
      if (failure.place === 9) { makeFailure(failure); }

      // Update with check:
      c.update(id._key, { "Hallox": 105 });

      if (healing.place === 9) { healFailure(healing); }
      if (failure.place === 10) { makeFailure(failure); }

      doc = c.document(id._key);
      assertEqual(100, doc.Hallo);
      assertEqual(105, doc.Hallox);

      if (healing.place === 10) { healFailure(healing); }
      if (failure.place === 11) { makeFailure(failure); }

      c.update([ids[0]._key, ids[1]._key], [{ Hallox: 106 }, { Hallox: 107 }]);

      if (healing.place === 11) { healFailure(healing); }
      if (failure.place === 12) { makeFailure(failure); }

      docs = c.document([ids[0]._key, ids[1]._key]);
      assertEqual(2, docs.length);
      assertEqual(101, docs[0].Hallo);
      assertEqual(102, docs[1].Hallo);
      assertEqual(106, docs[0].Hallox);
      assertEqual(107, docs[1].Hallox);

      if (healing.place === 12) { healFailure(healing); }
      if (failure.place === 13) {
        makeFailure(failure);
        // Note that it is not guaranteed that an AQL query runs without an
        // error, directly after the leader of a collection which occurs in
        // the query is stopped. It is entirely possible that the coordinator
        // still picks the failed server, so we have to wait for the failover
        // to happen. The easiest way to achieve this is to read a document,
        // which can retry until the failover has happened:
        if (!failure.follower) {
          doc = c.document(id._key);
        }
      }

      // AQL:
      var q = db._query(`FOR x IN @@cn
                            FILTER x.Hallo > 0
                            SORT x.Hallo
                            RETURN {"Hallo": x.Hallo}`, { "@cn": cn });
      docs = q.toArray();
      assertEqual(3, docs.length);
      assertEqual([{ Hallo: 100 }, { Hallo: 101 }, { Hallo: 102 }], docs);

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
    } finally {
      healAny();
    }
  }

  function setUp() {
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
  }

  function tearDown() {
    db._drop(cn);
    //global.ArangoAgency.set('Target/FailedServers', {});
  }

  return {
    setUp: setUp,
    tearDown: tearDown,
    waitForSynchronousReplication: waitForSynchronousReplication,
    runBasicOperations: runBasicOperations,

    failLeader: failLeader,
    healLeader: healLeader,

    failFollower: failFollower,
    healFollower: healFollower,

    getCollection: () => c,
  };
}());



