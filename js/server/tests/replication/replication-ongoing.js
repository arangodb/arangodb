/*jshint globalstrict:false, strict:false, unused: false */
/*global fail, assertEqual, assertTrue, assertFalse, assertNull, assertNotNull, arango, ARGUMENTS */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the replication
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var errors = arangodb.errors;
var db = arangodb.db;

var replication = require("@arangodb/replication");
var console = require("console");
var internal = require("internal");
var masterEndpoint = arango.getEndpoint();
var slaveEndpoint = ARGUMENTS[0];

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ReplicationSuite() {
  'use strict';
  var cn = "UnitTestsReplication";
  var cn2 = "UnitTestsReplication2";

  var connectToMaster = function() {
    arango.reconnect(masterEndpoint, db._name(), "root", "");
    db._flushCache();
  };

  var connectToSlave = function() {
    arango.reconnect(slaveEndpoint, db._name(), "root", "");
    db._flushCache();
  };

  var collectionChecksum = function(name) {
    var c = db._collection(name).checksum(true, true);
    return c.checksum;
  };

  var collectionCount = function(name) {
    return db._collection(name).count();
  };

  var compareTicks = function(l, r) {
    var i;
    if (l === null) {
      l = "0";
    }
    if (r === null) {
      r = "0";
    }
    if (l.length !== r.length) {
      return l.length - r.length < 0 ? -1 : 1;
    }

    // length is equal
    for (i = 0; i < l.length; ++i) {
      if (l[i] !== r[i]) {
        return l[i] < r[i] ? -1 : 1;
      }
    }

    return 0;
  };

  var compare = function(masterFunc, masterFunc2, slaveFuncOngoing, slaveFuncFinal, applierConfiguration) {
    var state = {};

    db._flushCache();
    masterFunc(state);

    connectToSlave();
    replication.applier.stop();
    replication.applier.forget();

    internal.wait(1, false);

    var syncResult = replication.sync({
      endpoint: masterEndpoint,
      username: "root",
      password: "",
      verbose: true,
      includeSystem: false,
      keepBarrier: false
    });

    assertTrue(syncResult.hasOwnProperty('lastLogTick'));

    connectToMaster();
    masterFunc2(state);

    // use lastLogTick as of now
    state.lastLogTick = replication.logger.state().state.lastLogTick;

    applierConfiguration = applierConfiguration || {};
    applierConfiguration.endpoint = masterEndpoint;
    applierConfiguration.username = "root";
    applierConfiguration.password = "";

    if (!applierConfiguration.hasOwnProperty('chunkSize')) {
      applierConfiguration.chunkSize = 16384;
    }

    connectToSlave();

    replication.applier.properties(applierConfiguration);
    replication.applier.start(syncResult.lastLogTick);

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

      var slaveState = replication.applier.state();

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

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function() {
      connectToSlave();
      try {
        replication.applier.stop();
        replication.applier.forget();
      }
      catch (err) {
      }

      connectToMaster();

      db._drop(cn);
      db._drop(cn2);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function() {
      connectToMaster();

      db._drop(cn);
      db._drop(cn2);

      connectToSlave();
      replication.applier.stop();
      replication.applier.forget();

      db._drop(cn);
      db._drop(cn2);
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
          assertTrue(replication.applier.state().state.running);
          replication.applier.stop();
          assertFalse(replication.applier.state().state.running);

          internal.wait(0.5, false);
          replication.applier.start();
          internal.wait(0.5, false);
          assertTrue(replication.applier.state().state.running);

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
          assertTrue(replication.applier.state().state.running);

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
          assertTrue(replication.applier.state().state.running);
          replication.applier.stop();
          assertFalse(replication.applier.state().state.running);

          connectToMaster();
          try {
            require("@arangodb/tasks").get(state.task);
            // task exists
            connectToSlave();

            internal.wait(0.5, false);
            replication.applier.start();
            assertTrue(replication.applier.state().state.running);
            return "wait";
          } catch (err) {
            // task does not exist anymore. we're done
            state.lastLogTick = replication.logger.state().state.lastLogTick;
            state.checksum = collectionChecksum(cn);
            state.count = collectionCount(cn);
            assertEqual(20, state.count);
            connectToSlave();
            replication.applier.start();
            assertTrue(replication.applier.state().state.running);
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
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationSuite);

return jsunity.done();
