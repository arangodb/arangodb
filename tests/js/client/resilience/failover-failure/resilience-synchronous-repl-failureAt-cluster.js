/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertFalse, assertEqual, fail, arango, print */

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
/// @author Max Neunhoeffer
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const db = arangodb.db;
const ERRORS = arangodb.errors;
const _ = require("lodash");
const wait = require("internal").wait;
const instanceRoledbServer = 'dbserver';
const suspendExternal = require("internal").suspendExternal;
const continueExternal = require("internal").continueExternal;
const {
  getEndpointById,
  getServersByType,
  getDBServers,
  arangoClusterInfoFlush,
  arangoClusterInfoGetCollectionInfo,
  arangoClusterInfoGetCollectionInfoCurrent,
  getServerById
} = require('@arangodb/test-helper');
let IM = global.instanceManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function SynchronousReplicationSuite() {
  'use strict';
  var cn = "UnitTestSyncRep";
  var c;
  var cinfo;
  var ccinfo;
  var shards;
  var failedState = { leader: null, follower: null };

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
    print("Waiting for synchronous replication to settle...");
    arangoClusterInfoFlush();
    cinfo = arangoClusterInfoGetCollectionInfo(database, cn);
    shards = Object.keys(cinfo.shards);
    var count = 0;
    var replicas;
    while (++count <= 300) {
      ccinfo = shards.map(
        s => arangoClusterInfoGetCollectionInfoCurrent(database, cn, s)
      );
      print("Plan:", cinfo.shards, "Current:", ccinfo.map(s => s.servers));
      replicas = ccinfo.map(s => s.servers.length);
      if (replicas.every(x => x > 1)) {
        print("Replication up and running!");
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
      arangoClusterInfoFlush();
    }
    console.error("Replication did not finish");
    return false;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fail the follower
  ////////////////////////////////////////////////////////////////////////////////

  function failFollower(failAt = null, follower = null) {
    if (follower == null) follower = cinfo.shards[shards[0]][1];
    let arangods = IM.arangods.filter(arangod => { return arangod.id === follower; });
    assertTrue(arangods.length === 1, JSON.stringify({'follower': follower, 'inst': IM.getStructure()}));
    if (failAt) {
      IM.debugSetFailAt(failAt, '', follower);
      print("Have added failure in follower", follower, " at ", failAt);
    } else {
      assertTrue(suspendExternal(arangods[0].pid));
      print("Have failed follower", follower);
    }
    failedState.follower = { failAt: (failAt ? failAt : null), failedServer: follower };
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief heal the follower
  ////////////////////////////////////////////////////////////////////////////////

  function healFollower(failAt = null, follower = null) {
    if (follower == null) follower = cinfo.shards[shards[0]][1];

    let arangods = IM.arangods.filter(arangod => { return arangod.id === follower; });
    assertTrue(arangods.length === 1, JSON.stringify({'follower': follower, 'inst': IM.getStructure()}));
    if (failAt) {
      IM.debugRemoveFailAt(failAt, '', follower);
      print("Have removed failure in follower", follower, " at ", failAt);
    } else {
      assertTrue(continueExternal(arangods[0].pid));
      print("Have healed follower", follower);
    }
    failedState.follower = null;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fail the leader
  ////////////////////////////////////////////////////////////////////////////////

  function failLeader(failAt = null, leader = null) {
    if (leader == null) leader = cinfo.shards[shards[0]][0];
    let arangods = IM.arangods.filter(arangod => { return arangod.id === leader; });
    assertTrue(arangods.length === 1, JSON.stringify({'leader': leader, 'inst': IM.getStructure()}));
    if (failAt) {
      IM.debugSetFailAt(failAt, '', leader);
      print("Have failed leader", leader, " at ", failAt);
    } else {
      assertTrue(suspendExternal(arangods[0].pid));
      print("Have failed leader", leader);
    }
    failedState.leader = { failAt: (failAt ? failAt : null), failedServer: leader };
    return leader;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief heal the leader
  ////////////////////////////////////////////////////////////////////////////////

  function healLeader(failAt = null, leader = null) {
    if (leader == null) leader = cinfo.shards[shards[0]][0];
    let arangods = IM.arangods.filter(arangod => { return arangod.id === leader; });
    assertTrue(arangods.length === 1, JSON.stringify({'leader': leader, 'inst': IM.getStructure()}));
    if (failAt) {
      IM.debugRemoveFailAt(failAt, '', leader);
      print("Have removed failure in leader", leader, " at ", failAt);
    } else {
      assertTrue(continueExternal(arangods[0].pid));
      print("Have healed leader", leader);
    }
    failedState.leader = null;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief basic operations, with various failure modes:
  ////////////////////////////////////////////////////////////////////////////////

  function runBasicOperations(monkeyFn) {
    monkeyFn(1);

    // Insert with check:
    var id = c.insert({ Hallo: 12 });
    assertEqual(1, c.count());

    monkeyFn(2);

    var doc = c.document(id._key);
    assertEqual(12, doc.Hallo);

    monkeyFn(3);

    var ids = c.insert([{ Hallo: 13 }, { Hallo: 14 }]);
    assertEqual(3, c.count());
    assertEqual(2, ids.length);

    monkeyFn(4);

    var docs = c.document([ids[0]._key, ids[1]._key]);
    assertEqual(2, docs.length);
    assertEqual(13, docs[0].Hallo);
    assertEqual(14, docs[1].Hallo);

    monkeyFn(5);

    // Replace with check:
    c.replace(id._key, { "Hallo": 100 });

    monkeyFn(6);

    doc = c.document(id._key);
    assertEqual(100, doc.Hallo);

    monkeyFn(7);

    c.replace([ids[0]._key, ids[1]._key], [{ Hallo: 101 }, { Hallo: 102 }]);

    monkeyFn(8);

    docs = c.document([ids[0]._key, ids[1]._key]);
    assertEqual(2, docs.length);
    assertEqual(101, docs[0].Hallo);
    assertEqual(102, docs[1].Hallo);

    monkeyFn(9);

    // Update with check:
    c.update(id._key, { "Hallox": 105 });

    monkeyFn(10);

    doc = c.document(id._key);
    assertEqual(100, doc.Hallo);
    assertEqual(105, doc.Hallox);

    monkeyFn(11);

    c.update([ids[0]._key, ids[1]._key], [{ Hallox: 106 }, { Hallox: 107 }]);

    monkeyFn(12);

    docs = c.document([ids[0]._key, ids[1]._key]);
    assertEqual(2, docs.length);
    assertEqual(101, docs[0].Hallo);
    assertEqual(102, docs[1].Hallo);
    assertEqual(106, docs[0].Hallox);
    assertEqual(107, docs[1].Hallox);

    monkeyFn(13);

    // AQL:
    var q = db._query(`FOR x IN @@cn
                         FILTER x.Hallo > 0
                         SORT x.Hallo
                         RETURN {"Hallo": x.Hallo}`, { "@cn": cn });
    docs = q.toArray();
    assertEqual(3, docs.length);
    assertEqual([{ Hallo: 100 }, { Hallo: 101 }, { Hallo: 102 }], docs);

    monkeyFn(14);

    // Remove with check:
    c.remove(id._key);

    monkeyFn(15);

    try {
      doc = c.document(id._key);
      fail();
    }
    catch (e1) {
      assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, e1.errorNum);
    }

    monkeyFn(16);

    assertEqual(2, c.count());

    monkeyFn(17);

    c.remove([ids[0]._key, ids[1]._key]);

    monkeyFn(18);

    docs = c.document([ids[0]._key, ids[1]._key]);
    assertEqual(2, docs.length);
    assertTrue(docs[0].error);
    assertTrue(docs[1].error);

    monkeyFn(19);
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
      print("System collections use servers:", systemCollServers);
      while (true) {
        db._drop(cn);
        c = db._create(cn, {
          numberOfShards: 1, replicationFactor: 2,
          avoidServers: systemCollServers
        });
        var servers = findCollectionServers("_system", cn);
        print("Test collections uses servers:", servers);
        if (_.intersection(systemCollServers, servers).length === 0) {
          return;
        }
        print("Need to recreate collection to avoid system collection servers.");
        //waitForSynchronousReplication("_system");
        print("Synchronous replication has settled, now dropping again.");
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      var servers = getServersByType(instanceRoledbServer);
      IM.debugClearFailAt();
      if(failedState.leader != null) healLeader(failedState.leader.failAt, failedState.leader.failedServer);
      if(failedState.follower != null) healFollower(failedState.follower.failAt, failedState.follower.failedServer);
      db._drop(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check if a synchronously replicated collection gets online
    ////////////////////////////////////////////////////////////////////////////////

    testSetup: function () {
      for (var count = 0; count < 120; ++count) {
        let dbservers = getServersByType(instanceRoledbServer);
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
    /// @brief fail in place 1
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail1: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 1) {
          failFollower("LogicalCollection::insert");
        } else if (place === 18) {
          healFollower("LogicalCollection::insert");
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief fail in place 2
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail2: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 2) {
          failFollower("LogicalCollection::read");
        } else if (place === 18) {
          healFollower("LogicalCollection::read");
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief fail in place 3
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail3: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 3) {
          failFollower("LogicalCollection::insert");
        } else if (place === 18) {
          healFollower("LogicalCollection::insert");
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief fail in place 5
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail5: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 5) {
          failFollower("LogicalCollection::replace");
        } else if (place === 18) {
          healFollower("LogicalCollection::replace");
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief fail in place 7
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail7: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 7) {
          failFollower("LogicalCollection::replace");
        } else if (place === 18) {
          healFollower("LogicalCollection::replace");
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief fail in place 9
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail9: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 9) {
          failFollower("LogicalCollection::update");
        } else if (place === 18) {
          healFollower("LogicalCollection::update");
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief fail in place 14
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsFollowerFail14: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 14) {
          failFollower("LogicalCollection::remove");
        } else if (place === 18) {
          healFollower("LogicalCollection::remove");
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief follower fail in place 1 until 2, leader fail in 3
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsCombinedFail1_2_3: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 1) {
          failFollower("LogicalCollection::insert"); // replication fails
        } else if (place === 2) {
          healFollower("LogicalCollection::insert");
        } else if (place === 3) {
          assertTrue(waitForSynchronousReplication("_system"));
          failLeader();
        } else if (place === 19) {
          healLeader();
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief follower fail in place 3 until 4, leader fails in 4 after in-sync
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsCombinedFail3_4: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 3) {
          failFollower("LogicalCollection::insert"); // replication fails
        } else if (place === 4) {
          healFollower("LogicalCollection::insert");
          assertTrue(waitForSynchronousReplication("_system"));
          failLeader();
        } else if (place === 19) {
          healLeader();
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief follower fail in place 5 until 4, leader fails in 4 after in-sync
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsCombinedFail5_6: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 5) {
          failFollower("LogicalCollection::replace"); // replication fails
        } else if (place === 6) {
          healFollower("LogicalCollection::replace");
          assertTrue(waitForSynchronousReplication("_system"));
          failLeader();
        } else if (place === 19) {
          healLeader();
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief follower fail in place 7 until 8, leader fails in 8 after in-sync
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsCombinedFail7_8: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 7) {
          failFollower("LogicalCollection::replace"); // replication fails
        } else if (place === 8) {
          healFollower("LogicalCollection::replace");
          assertTrue(waitForSynchronousReplication("_system"));
          failLeader();
        } else if (place === 19) {
          healLeader();
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief follower fail in place 9 until 10, leader fails in 10 after in-sync
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsCombinedFail9_10: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 9) {
          failFollower("LogicalCollection::update"); // replication fails
        } else if (place === 10) {
          healFollower("LogicalCollection::update");
          assertTrue(waitForSynchronousReplication("_system"));
          failLeader();
        } else if (place === 19) {
          healLeader();
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief follower fail in place 11 until 12, leader fails in 12 after in-sync
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsCombinedFail11_12: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 11) {
          failFollower("LogicalCollection::update"); // replication fails
        } else if (place === 12) {
          healFollower("LogicalCollection::update");
          assertTrue(waitForSynchronousReplication("_system"));
          failLeader();
        } else if (place === 19) {
          healLeader();
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief follower fail in place 14 until 15, leader fails in 18 after in-sync
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsCombinedFail14_15: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 14) {
          failFollower("LogicalCollection::remove"); // replication fails
        } else if (place === 15) {
          healFollower("LogicalCollection::remove");
          assertTrue(waitForSynchronousReplication("_system"));
          failLeader();
        } else if (place === 19) {
          healLeader();
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief follower fail in place 17 until 18, leader fails in 18 after in-sync
    ////////////////////////////////////////////////////////////////////////////////

    testBasicOperationsCombinedFail17_18: function () {
      assertTrue(waitForSynchronousReplication("_system"));
      runBasicOperations((place) => {
        if (place === 17) {
          failFollower("LogicalCollection::remove"); // replication fails
        } else if (place === 18) {
          healFollower("LogicalCollection::remove");
          assertTrue(waitForSynchronousReplication("_system"));
          failLeader();
        } else if (place === 19) {
          healLeader();
        }
      });
      assertTrue(waitForSynchronousReplication("_system"));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief just to allow a trailing comma at the end of the last test
    ////////////////////////////////////////////////////////////////////////////////

    testDummy: function () {
      assertEqual(12, 12);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SynchronousReplicationSuite);
return jsunity.done();
