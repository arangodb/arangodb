/*jshint globalstrict:false, strict:false, unused: false */
/*global fail, assertEqual, assertTrue, assertFalse, assertNull, arango, ARGUMENTS */

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
  };

  var connectToSlave = function() {
    arango.reconnect(slaveEndpoint, db._name(), "root", "");
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

    var includeSystem = true;
    var restrictType = "";
    var restrictCollections = [];
    applierConfiguration = applierConfiguration || { };

    if (typeof applierConfiguration === 'object') {
      if (applierConfiguration.hasOwnProperty("includeSystem")) {
        includeSystem = applierConfiguration.includeSystem;
      }
      if (applierConfiguration.hasOwnProperty("restrictType")) {
        restrictType = applierConfiguration.restrictType;
      }
      if (applierConfiguration.hasOwnProperty("restrictCollections")) {
        restrictCollections = applierConfiguration.restrictCollections;
      }
    }

    var keepBarrier = false;
    if (applierConfiguration.hasOwnProperty("keepBarrier")) {
      keepBarrier = applierConfiguration.keepBarrier; 
    }

    var syncResult = replication.sync({
      endpoint: masterEndpoint,
      username: "root",
      password: "",
      verbose: true,
      includeSystem: includeSystem,
      restrictType: restrictType,
      restrictCollections: restrictCollections,
      keepBarrier: keepBarrier
    });

    assertTrue(syncResult.hasOwnProperty('lastLogTick'));

    connectToMaster();
    masterFunc2(state);

    applierConfiguration = applierConfiguration || {};
    applierConfiguration.endpoint = masterEndpoint;
    applierConfiguration.username = "root";
    applierConfiguration.password = "";

    if (!applierConfiguration.hasOwnProperty('chunkSize')) {
      applierConfiguration.chunkSize = 16384;
    }

    connectToSlave();

    replication.applier.properties(applierConfiguration);
    if (keepBarrier) {
      replication.applier.start(syncResult.lastLogTick, syncResult.barrierId);
    }
    else {
      replication.applier.start(syncResult.lastLogTick);
    }

    var printed = false;

    while (true) {
      if (!slaveFuncOngoing(state)) {
        return;
      }

      var slaveState = replication.applier.state();

      if (!slaveState.state.running || slaveState.state.lastError.errorNum > 0) {
        break;
      }

      if (compareTicks(slaveState.state.lastAppliedContinuousTick, syncResult.lastLogTick) >= 0 ||
        compareTicks(slaveState.state.lastProcessedContinuousTick, syncResult.lastLogTick) >= 0) { // ||
        //          compareTicks(slaveState.state.lastAvailableContinuousTick, syncResult.lastLogTick) > 0) {
        break;
      }

      if (!printed) {
        console.log("waiting for slave to catch up");
        printed = true;
      }
      internal.wait(1.0, false);
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
    /// @brief test require from present
    ////////////////////////////////////////////////////////////////////////////////

    testRequireFromPresentFalse: function() {
      connectToMaster();

      compare(
        function(state) {
          db._create(cn);
        },

        function(state) {
          // flush the wal logs on the master so the start tick is not available
          // anymore when we start replicating
          for (var i = 0; i < 30; ++i) {
            db._collection(cn).save({
              value: i
            });
            internal.wal.flush(); //true, true);
          }
          db._collection(cn).save({
            value: i
          });
          internal.wal.flush(true, true);
          internal.wait(6, false);
        },

        function(state) {
          return true;
        },

        function(state) {
          // data loss on slave!
          assertTrue(db._collection(cn).count() < 25);
        }, {
          requireFromPresent: false,
          keepBarrier: false
        }
      );
    },
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test require from present, no barrier
    ////////////////////////////////////////////////////////////////////////////////

    testRequireFromPresentTrueNoBarrier : function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
        },

        function (state) {
          // flush the wal logs on the master so the start tick is not available
          // anymore when we start replicating
          for (var i = 0; i < 30; ++i) {
            db._collection(cn).save({ value: i });
            internal.wal.flush(); //true, true);
          }
          internal.wal.flush(true, true);
          internal.wait(6, false);
        },

        function (state) {
          // wait for slave applier to have started and detect the mess
          internal.wait(5, false);

          // slave should have failed
          assertFalse(replication.applier.state().state.running);
          return true;
        },

        function (state) {
          // data loss on slave!
          assertTrue(db._collection(cn).count() < 25);
        },
        { 
          requireFromPresent: true,
          keepBarrier: false
        }
      );
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test require from present
    ////////////////////////////////////////////////////////////////////////////////

    testRequireFromPresentTrue : function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
        },

        function (state) {
          // flush the wal logs on the master so the start tick is not available
          // anymore when we start replicating
          for (var i = 0; i < 30; ++i) {
            db._collection(cn).save({ value: i });
            internal.wal.flush(); //true, true);
          }
          internal.wal.flush(true, true);
          internal.wait(6, false);
          
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(30, state.count);
        },

        function (state) {
          // wait for slave applier to have started and run
          internal.wait(5, false);

          // slave should not have stopped
          assertTrue(replication.applier.state().state.running);
          return true;
        },

        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        },
        { 
          requireFromPresent: true,
          keepBarrier: true 
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
                  test1: i
                });
                c.save({
                  test2: i
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
                  test1: i
                });
                c.save({
                  test2: i
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
            name: "replication-test",
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
          } catch (err) {
            // task does not exist. we're done
            state.checksum = collectionChecksum(cn);
            state.count = collectionCount(cn);
            assertEqual(20, state.count);
            connectToSlave();
            return false;
          }

          connectToSlave();
          internal.wait(0.5, false);
          return true;
        },

        function(state) {
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
                  test1: i
                });
                c.save({
                  test2: i
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
            name: "replication-test",
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
          } catch (err) {
            // task does not exist. we're done
            state.checksum = collectionChecksum(cn);
            state.count = collectionCount(cn);
            assertEqual(20, state.count);
            connectToSlave();
            return false;
          }

          connectToSlave();

          internal.wait(0.5, false);
          replication.applier.start();
          assertTrue(replication.applier.state().state.running);
          return true;
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
