/*jshint globalstrict:false, strict:false, unused: false */
/*global fail, assertEqual, assertTrue, assertFalse, assertNull, assertNotNull, arango, ARGUMENTS */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the global replication
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const errors = arangodb.errors;
const db = arangodb.db;

const replication = require("@arangodb/replication");
const compareTicks = replication.compareTicks;
const console = require("console");
const internal = require("internal");

const masterEndpoint = arango.getEndpoint();
const slaveEndpoint = ARGUMENTS[0];

const cn = "UnitTestsReplication";
const cn2 = "UnitTestsReplication2";

const connectToMaster = function() {
  arango.reconnect(masterEndpoint, db._name(), "root", "");
  db._flushCache();
};

const connectToSlave = function() {
  arango.reconnect(slaveEndpoint, db._name(), "root", "");
  db._flushCache();
};

const collectionChecksum = function(name) {
  var c = db._collection(name).checksum(true, true);
  return c.checksum;
};

const collectionCount = function(name) {
  return db._collection(name).count();
};

// Usage:
// masterFunc apply changes on the master before replication
// masterFunc2 apply changes on the master while replication is ongoing
// slaveFuncOngoing function is executed while replication is still ongoing, if this returns "wait" it means
//   it will be called again after a while, thereby we can wait until replication is synced up
// slaveFuncFinal function is executed after FuncOngoing has decided we are done, for final validation.
const compare = function(masterFunc, masterFunc2, slaveFuncOngoing, slaveFuncFinal, applierConfiguration) {
  var state = {};

  db._flushCache();
  masterFunc(state);

  connectToSlave();
  replication.globalApplier.stop();
  replication.globalApplier.forget();

  while (replication.globalApplier.state().state.running) {
    internal.wait(0.1, false);
  }
  
  applierConfiguration = applierConfiguration || {};
  applierConfiguration.endpoint = masterEndpoint;
  applierConfiguration.username = "root";
  applierConfiguration.password = "";
  applierConfiguration.includeSystem = applierConfiguration.includeSystem || false;

  var syncResult = replication.syncGlobal({
    endpoint: masterEndpoint,
    username: "root",
    password: "",
    verbose: true,
    includeSystem: applierConfiguration.includeSystem,
    keepBarrier: false,
    restrictType: applierConfiguration.restrictType,
    restrictCollections: applierConfiguration.restrictCollections
  });

  assertTrue(syncResult.hasOwnProperty('lastLogTick'));

  connectToMaster();
  masterFunc2(state);

  // use lastLogTick as of now
  state.lastLogTick = replication.logger.state().state.lastLogTick;

  if (!applierConfiguration.hasOwnProperty('chunkSize')) {
    applierConfiguration.chunkSize = 16384;
  }

  connectToSlave();

  replication.globalApplier.properties(applierConfiguration);
  replication.globalApplier.start(syncResult.lastLogTick);

  var printed = false, handled = false;

  while (true) {
    if (!handled) {
      var r = slaveFuncOngoing(state);
      if (r === "wait") {
        // special return code that tells us to hang on
        internal.wait(0.5, false);
        continue;
      }

      handled = true;
    }

    var slaveState = replication.globalApplier.state();

    if (slaveState.state.lastError.errorNum > 0) {
      console.log("slave has errored:", JSON.stringify(slaveState.state.lastError));
      break;
    }

    if (!slaveState.state.running) {
      console.log("slave is not running");
      break;
    }
    if (compareTicks(slaveState.state.lastAppliedContinuousTick, state.lastLogTick) >= 0 ||
        compareTicks(slaveState.state.lastProcessedContinuousTick, state.lastLogTick) >= 0) { // ||
     //          compareTicks(slaveState.state.lastAvailableContinuousTick, syncResult.lastLogTick) > 0) {
      console.log("slave has caught up. state.lastLogTick:", state.lastLogTick, "slaveState.lastAppliedContinuousTick:", slaveState.state.lastAppliedContinuousTick, "slaveState.lastProcessedContinuousTick:", slaveState.state.lastProcessedContinuousTick);
      break;
    }
      
    if (!printed) {
      console.log("waiting for slave to catch up");
      printed = true;
    }
    internal.wait(0.5, false);
  }

  db._flushCache();
  slaveFuncFinal(state);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Base Test Config. Identitical part for _system and other DB
////////////////////////////////////////////////////////////////////////////////

function BaseTestConfig() {
  'use strict';

  return {
    
    testIncludeCollection: function() {
      connectToMaster();

      compare(
        function(state) {
        },

        function(state) {
          db._create(cn);
          db._create(cn + "2");
          for (var i = 0; i < 100; ++i) {
            db._collection(cn).save({ value: i });
            db._collection(cn + "2").save({ value: i });
          }
          internal.wal.flush(true, true);
        },

        function(state) {
          return true;
        },

        function(state) {
          assertTrue(db._collection(cn).count() === 100);
          assertNull(db._collection(cn + "2"));
        },

        { restrictType: "include", restrictCollections: [cn] }
      );
    },
    
    testExcludeCollection: function() {
      connectToMaster();

      compare(
        function(state) {
        },

        function(state) {
          db._create(cn);
          db._create(cn + "2");
          for (var i = 0; i < 100; ++i) {
            db._collection(cn).save({ value: i });
            db._collection(cn + "2").save({ value: i });
          }
          internal.wal.flush(true, true);
        },

        function(state) {
          return true;
        },

        function(state) {
          assertTrue(db._collection(cn).count() === 100);
          assertNull(db._collection(cn + "2"));
        },

        { restrictType: "exclude", restrictCollections: [cn + "2"] }
      );
    },
   
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test collection creation
    ////////////////////////////////////////////////////////////////////////////////

    testCreateCollection: function() {
      connectToMaster();

      compare(
        function(state) {
        },

        function(state) {
          db._create(cn);
          for (var i = 0; i < 100; ++i) {
            db._collection(cn).save({
              value: i
            });
          }
          internal.wal.flush(true, true);
        },

        function(state) {
          return true;
        },

        function(state) {
          assertTrue(db._collection(cn).count() === 100);
        }
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test collection dropping
    ////////////////////////////////////////////////////////////////////////////////

    testDropCollection: function() {
      connectToMaster();

      compare(
        function(state) {
        },

        function(state) {
          db._create(cn);
          for (var i = 0; i < 100; ++i) {
            db._collection(cn).save({
              value: i
            });
          }
          db._drop(cn);
          internal.wal.flush(true, true);
        },

        function(state) {
          return true;
        },

        function(state) {
          assertNull(db._collection(cn));
        }
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test index creation
    ////////////////////////////////////////////////////////////////////////////////

    testCreateIndex: function() {
      connectToMaster();

      compare(
        function(state) {
        },

        function(state) {
          db._create(cn);
          db._collection(cn).ensureIndex({ type: "hash", fields: ["value"] });
        },

        function(state) {
          return true;
        },

        function(state) {
          var col = db._collection(cn);
          assertNotNull(col, "collection does not exist");
          var idx = col.getIndexes();
          assertEqual(2, idx.length);
          assertEqual("primary", idx[0].type);
          assertEqual("hash", idx[1].type);
          assertEqual(["value"], idx[1].fields);
        }
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test index dropping
    ////////////////////////////////////////////////////////////////////////////////

    testDropIndex: function() {
      connectToMaster();

      compare(
        function(state) {
        },

        function(state) {
          db._create(cn);
          var idx = db._collection(cn).ensureIndex({ type: "hash", fields: ["value"] });
          db._collection(cn).dropIndex(idx);
        },

        function(state) {
          return true;
        },

        function(state) {
          var idx = db._collection(cn).getIndexes();
          assertEqual(1, idx.length);
          assertEqual("primary", idx[0].type);
        }
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test renaming
    ////////////////////////////////////////////////////////////////////////////////

    testRenameCollection: function() {
      connectToMaster();

      compare(
        function(state) {
        },

        function(state) {
          db._create(cn);
          db._collection(cn).rename(cn + "Renamed");
        },

        function(state) {
          return true;
        },

        function(state) {
          assertNull(db._collection(cn));
          assertNotNull(db._collection(cn + "Renamed"));
        }
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test renaming
    ////////////////////////////////////////////////////////////////////////////////

    testChangeCollection: function() {
      connectToMaster();

      compare(
        function(state) {
        },

        function(state) {
          db._create(cn);
          assertFalse(db._collection(cn).properties().waitForSync);
          db._collection(cn).properties({ waitForSync: true });
        },

        function(state) {
          return true;
        },

        function(state) {
          assertTrue(db._collection(cn).properties().waitForSync);
        }
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test long transaction, blocking
    ////////////////////////////////////////////////////////////////////////////////

    testLongTransactionBlocking: function() {
      connectToMaster();

      compare(
        function(state) {
          db._create(cn);
        },

        function(state) {
          db._executeTransaction({
            collections: {
              write: cn
            },
            action: function(params) {
              var wait = require("internal").wait;
              var db = require("internal").db;
              var c = db._collection(params.cn);

              for (var i = 0; i < 10; ++i) {
                c.save({
                  test1: i,
                  type: "longTransactionBlocking",
                  coll: "UnitTestsReplication"
                });
                c.save({
                  test2: i,
                  type: "longTransactionBlocking",
                  coll: "UnitTestsReplication"
                });

                // intentionally delay the transaction
                wait(0.75, false);
              }
            },
            params: {
              cn: cn
            }
          });

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(20, state.count);
        },

        function(state) {
          // stop and restart replication on the slave
          assertTrue(replication.globalApplier.state().state.running);
          replication.globalApplier.stop();
          assertFalse(replication.globalApplier.state().state.running);

          internal.wait(0.5, false);
          replication.globalApplier.start();
          internal.wait(0.5, false);
          assertTrue(replication.globalApplier.state().state.running);

          return true;
        },

        function(state) {
          internal.wait(3, false);
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test long transaction, asynchronous
    ////////////////////////////////////////////////////////////////////////////////

    testLongTransactionAsync: function() {
      connectToMaster();

      compare(
        function(state) {
          db._create(cn);
        },

        function(state) {
          var func = db._executeTransaction({
            collections: {
              write: cn
            },
            action: function(params) {
              var wait = require("internal").wait;
              var db = require("internal").db;
              var c = db._collection(params.cn);

              for (var i = 0; i < 10; ++i) {
                c.save({
                  test1: i,
                  type: "longTransactionAsync",
                  coll: "UnitTestsReplication"
                });
                c.save({
                  test2: i,
                  type: "longTransactionAsync",
                  coll: "UnitTestsReplication"
                });

                // intentionally delay the transaction
                wait(3.0, false);
              }
            },
            params: {
              cn: cn
            }
          });

          state.task = require("@arangodb/tasks").register({
            name: "replication-test-async",
            command: String(func),
            params: {
              cn: cn
            }
          }).id;
        },

        function(state) {
          assertTrue(replication.globalApplier.state().state.running);

          connectToMaster();
          try {
            require("@arangodb/tasks").get(state.task);
            // task exists
            connectToSlave();
            return "wait";
          } catch (err) {
            // task does not exist. we're done
            state.lastLogTick = replication.logger.state().state.lastLogTick;
            state.checksum = collectionChecksum(cn);
            state.count = collectionCount(cn);
            assertEqual(20, state.count);
            connectToSlave();
            return true;
          }
        },

        function(state) {
          assertTrue(state.hasOwnProperty("count"));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test long transaction, asynchronous
    ////////////////////////////////////////////////////////////////////////////////

    testLongTransactionAsyncWithSlaveRestarts: function() {
      connectToMaster();

      compare(
        function(state) {
          db._create(cn);
        },

        function(state) {
          var func = db._executeTransaction({
            collections: {
              write: cn
            },
            action: function(params) {
              var wait = require("internal").wait;
              var db = require("internal").db;
              var c = db._collection(params.cn);

              for (var i = 0; i < 10; ++i) {
                c.save({
                  test1: i,
                  type: "longTransactionAsyncWithSlaveRestarts",
                  coll: "UnitTestsReplication"
                });
                c.save({
                  test2: i,
                  type: "longTransactionAsyncWithSlaveRestarts",
                  coll: "UnitTestsReplication"
                });

                // intentionally delay the transaction
                wait(0.75, false);
              }
            },
            params: {
              cn: cn
            }
          });

          state.task = require("@arangodb/tasks").register({
            name: "replication-test-async-with-restart",
            command: String(func),
            params: {
              cn: cn
            }
          }).id;
        },

        function(state) {
          // stop and restart replication on the slave
          assertTrue(replication.globalApplier.state().state.running);
          replication.globalApplier.stop();
          assertFalse(replication.globalApplier.state().state.running);

          connectToMaster();
          try {
            require("@arangodb/tasks").get(state.task);
            // task exists
            connectToSlave();

            internal.wait(0.5, false);
            replication.globalApplier.start();
            assertTrue(replication.globalApplier.state().state.running);
            return "wait";
          } catch (err) {
            // task does not exist anymore. we're done
            state.lastLogTick = replication.logger.state().state.lastLogTick;
            state.checksum = collectionChecksum(cn);
            state.count = collectionCount(cn);
            assertEqual(20, state.count);
            connectToSlave();
            replication.globalApplier.start();
            assertTrue(replication.globalApplier.state().state.running);
            return true;
          }
        },

        function(state) {
          assertEqual(state.count, collectionCount(cn));
        }
      );
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for _system
////////////////////////////////////////////////////////////////////////////////

function ReplicationSuite() {
  'use strict';
  let suite = BaseTestConfig();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief set up
  ////////////////////////////////////////////////////////////////////////////////

  suite.setUp = function() {
    connectToSlave();
    try {
      replication.global.stop();
      replication.global.forget();
    }
    catch (err) {
    }

    connectToMaster();

    db._drop(cn);
    db._drop(cn2);
    db._drop(cn + "Renamed");
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief tear down
  ////////////////////////////////////////////////////////////////////////////////

  suite.tearDown = function() {
    connectToMaster();

    db._drop(cn);
    db._drop(cn2);

    connectToSlave();
    replication.globalApplier.stop();
    replication.globalApplier.forget();

    db._drop(cn);
    db._drop(cn2);
    db._drop(cn + "Renamed");
  };

  suite.testSyncOfUsers = function() {
    connectToMaster();

    compare(
      function(state) {
        // Initially create a user which is unknown to the slave
        let users = require("@arangodb/users");
        users.save("alice","alice");
        users.grantDatabase("alice", "_system", "rw");
        users.grantCollection("alice", "_system", cn, "none");
        internal.wal.flush(true, true);
        // Just to validate everything is as expected
        assertTrue(users.exists("alice"));
        assertEqual("rw", users.permission("alice", "_system"));
        assertEqual("none", users.permission("alice", "_system", cn));
      },
      function(state) {
        let users = require("@arangodb/users");
        users.save("bob","bob");
        users.grantDatabase("bob", "_system", "ro");
        users.grantCollection("bob", "_system", cn, "rw");
        internal.wal.flush(true, true);
        // Just to validate everything is as expected
        assertTrue(users.exists("bob"));
        assertEqual("ro", users.permission("bob", "_system"));
        assertEqual("rw", users.permission("bob", "_system", cn));
      },
      function(state) {
        return true;
      },
      function(state) {
        let users = require("@arangodb/users");
        assertTrue(users.exists("root"));
        assertTrue(users.exists("bob"));
        assertEqual("ro", users.permission("bob", "_system"));
        assertEqual("rw", users.permission("bob", "_system", cn));
        assertTrue(users.exists("alice"));
        assertEqual("rw", users.permission("alice", "_system"));
        assertEqual("none", users.permission("alice", "_system", cn));
      },
      {
        includeSystem: true
      }
    );
  };

  return suite;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for other database
////////////////////////////////////////////////////////////////////////////////

function ReplicationOtherDBSuite() {
  'use strict';
  let suite = BaseTestConfig();
  const dbName = "UnitTestDB";

  // Setup documents to be stored on the master.

  let docs = [];
  for (let i = 0; i < 50; ++i) {
    docs.push({value: i});
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief set up
  ////////////////////////////////////////////////////////////////////////////////

  suite.setUp = function() {
    db._useDatabase("_system");
    connectToSlave();
    try {
      replication.globalApplier.stop();
      replication.globalApplier.forget();
    } catch (err) {
    }

    try {
      db._dropDatabase(dbName);
    } catch (e) {
    }
    
    db._createDatabase(dbName);

    connectToMaster();

    try {
      db._dropDatabase(dbName);
    } catch (e) {
    }
    db._createDatabase(dbName);
    db._useDatabase(dbName);
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief tear down
  ////////////////////////////////////////////////////////////////////////////////

  suite.tearDown = function() {
    db._useDatabase("_system");

    connectToSlave();
    db._useDatabase(dbName);

    replication.globalApplier.stop();
    replication.globalApplier.forget();
    
    db._useDatabase("_system");
    try {
      db._dropDatabase(dbName);
    } catch (e) {
    }

    connectToMaster();

    db._useDatabase("_system");
    try {
      db._dropDatabase(dbName);
    } catch (e) {
    }

  };

  // Shared function that sets up replication
  // of the collection and inserts 50 documents.
  const setupReplication = function() {
    // Section - Master
    connectToMaster();

    // Create the collection
    db._flushCache();
    db._create(cn);

    // Section - Follower
    connectToSlave();

    // Setup Replication
    replication.globalApplier.stop();
    replication.globalApplier.forget();

    while (replication.globalApplier.state().state.running) {
      internal.wait(0.1, false);
    }

    let config = {
      endpoint: masterEndpoint,
      username: "root",
      password: "",
      verbose: true,
      includeSystem: false,
      restrictType: "",
      restrictCollections: [],
      keepBarrier: false
    };

    replication.setupReplicationGlobal(config);

    // Section - Master
    connectToMaster();
    // Insert some documents
    db._collection(cn).save(docs);
    // Flush wal to trigger replication
    internal.wal.flush(true, true);
    internal.wait(6, false);
    // Use counter as indicator
    let count = collectionCount(cn);
    assertEqual(50, count);

    // Section - Slave
    connectToSlave();

    // Give it some time to sync
    internal.wait(6, false);
    // Now we should have the same amount of documents
    assertEqual(count, collectionCount(cn));
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief test dropping a database on slave while replication is ongoing
  ////////////////////////////////////////////////////////////////////////////////

  suite.testDropDatabaseOnSlaveDuringReplication = function() {
    setupReplication();

    // Section - Slave
    connectToSlave();
    
    assertTrue(replication.globalApplier.state().state.running);

    // Now do the evil stuff: drop the database that is replicating right now.
    db._useDatabase("_system");

    // This shall not fail.
    db._dropDatabase(dbName);

    // Section - Master
    connectToMaster();

    // Just write some more
    db._useDatabase(dbName);
    db._collection(cn).save(docs);
    internal.wal.flush(true, true);
    internal.wait(6, false);

    db._useDatabase("_system");

    // Section - Slave
    connectToSlave();

    // The DB should be gone and the server should be running.
    let dbs = db._databases();
    assertEqual(-1, dbs.indexOf(dbName));

    // We can setup everything here without problems.
    try {
      db._createDatabase(dbName);
    } catch (e) {
      assertFalse(true, "Could not recreate database on slave: " + e);
    }

    db._useDatabase(dbName);

    try {
      db._createDocumentCollection(cn);
    } catch (e) {
      assertFalse(true, "Could not recreate collection on slave: " + e);
    }

    // Collection should be empty
    assertEqual(0, collectionCount(cn));

    // now test if the replication is actually 
    // switched off
 
    // Section - Master
    connectToMaster();
    // Insert some documents
    db._collection(cn).save(docs);
    // Flush wal to trigger replication
    internal.wal.flush(true, true);

    // Section - Slave
    connectToSlave();

    // Give it some time to sync (eventually, should not do anything...)
    internal.wait(6, false);

    // Now should still have empty collection
    assertEqual(0, collectionCount(cn));
    
    assertFalse(replication.globalApplier.state().state.running);
  };

  suite.testDropDatabaseOnMasterDuringReplication = function() {
    var waitUntil = function(cb) {
      var tries = 0;
      while (tries++ < 60 * 2) {
        if (cb()) {
          return;
        }
        internal.wait(0.5, false);
      }
      assertFalse(true, "required condition not satisified: " + String(cb));
    };

    setupReplication();
    
    db._useDatabase("_system");
    connectToSlave();
    // wait until database is present on slave as well
    waitUntil(function() { return (db._databases().indexOf(dbName) !== -1); });

    // Section - Master
    // Now do the evil stuff: drop the database that is replicating from right now.
    connectToMaster();
    // This shall not fail.
    db._dropDatabase(dbName);
    
    db._useDatabase("_system");
    connectToSlave();
    waitUntil(function() { return (db._databases().indexOf(dbName) === -1); });

    // Now recreate a new database with this name
    connectToMaster();
    db._createDatabase(dbName);
    
    db._useDatabase(dbName);
    db._createDocumentCollection(cn);
    db._collection(cn).save(docs);

    // Section - Slave
    db._useDatabase("_system");
    connectToSlave();
    waitUntil(function() { return (db._databases().indexOf(dbName) !== -1); });
    // database now present on slave

    // Now test if the Slave did replicate the new database...
    db._useDatabase(dbName);
    // wait for collection to appear
    waitUntil(function() {
      let cc = db._collection(cn);
      return cc !== null && cc.count() >= 50; 
    });
    assertEqual(50, collectionCount(cn), "The slave inserted the new collection data into the old one, it skipped the drop.");
    
    assertTrue(replication.globalApplier.state().state.running);
  };

  return suite;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationSuite);
jsunity.run(ReplicationOtherDBSuite);

// TODO Add test for:
// Accessing globalApplier in non system database.
// Try to setup global repliaction in non system database.

return jsunity.done();
