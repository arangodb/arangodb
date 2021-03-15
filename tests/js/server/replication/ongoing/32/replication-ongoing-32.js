/* jshint globalstrict:false, strict:false, unused: false */
/* global assertEqual, assertTrue, assertFalse, assertNull, assertNotNull, arango, ARGUMENTS */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the replication
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Simon Grätzer
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var arangodb = require('@arangodb');
var db = arangodb.db;

var replication = require('@arangodb/replication');
const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
var deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
let compareTicks = replication.compareTicks;
var console = require('console');
var internal = require('internal');
var masterEndpoint = arango.getEndpoint();
const slaveEndpoint = ARGUMENTS[ARGUMENTS.length - 1];

const cn = 'UnitTestsReplication';
const cn2 = 'UnitTestsReplication2';

const connectToMaster = function () {
  reconnectRetry(masterEndpoint, db._name(), 'root', '');
  db._flushCache();
};

const connectToSlave = function () {
  reconnectRetry(slaveEndpoint, db._name(), 'root', '');
  db._flushCache();
};

const collectionChecksum = function (name) {
  var c = db._collection(name).checksum(true, true);
  return c.checksum;
};

const collectionCount = function (name) {
  return db._collection(name).count();
};

const compare = function (masterFunc, masterFunc2, slaveFuncOngoing, slaveFuncFinal, applierConfiguration) {
  var state = {};

  db._flushCache();
  masterFunc(state);

  connectToSlave();
  replication.applier.stop();
  replication.applier.forget();

  while (replication.applier.state().state.running) {
    internal.wait(0.1, false);
  }

  var syncResult = replication.sync({
    endpoint: masterEndpoint,
    username: 'root',
    password: '',
    verbose: true,
    includeSystem: false,
    keepBarrier: true
  });

  assertTrue(syncResult.hasOwnProperty('lastLogTick'));

  connectToMaster();
  masterFunc2(state);

  // use lastLogTick as of now
  state.lastLogTick = replication.logger.state().state.lastUncommittedLogTick;

  applierConfiguration = applierConfiguration || {};
  applierConfiguration.endpoint = masterEndpoint;
  applierConfiguration.username = 'root';
  applierConfiguration.password = '';
  applierConfiguration.force32mode = true;
  applierConfiguration.requireFromPresent = true;

  if (!applierConfiguration.hasOwnProperty('chunkSize')) {
    applierConfiguration.chunkSize = 16384;
  }

  connectToSlave();

  replication.applier.properties(applierConfiguration);
  replication.applier.start(syncResult.lastLogTick, syncResult.barrierId);

  var printed = false;
  var handled = false;

  while (true) {
    if (!handled) {
      var r = slaveFuncOngoing(state);
      if (r === 'wait') {
        // special return code that tells us to hang on
        internal.wait(0.5, false);
        continue;
      }

      handled = true;
    }

    var slaveState = replication.applier.state();

    if (slaveState.state.lastError.errorNum > 0) {
      console.topic('replication=error', 'slave has errored:', JSON.stringify(slaveState.state.lastError));
      break;
    }

    if (!slaveState.state.running) {
      console.topic('replication=error', 'slave is not running');
      break;
    }

    if ((compareTicks(slaveState.state.lastAppliedContinuousTick, state.lastLogTick) >= 0) ||
        (compareTicks(slaveState.state.lastProcessedContinuousTick, state.lastLogTick) >= 0)) {
      console.topic('replication=debug',
                    'slave has caught up. state.lastLogTick:',
                    state.lastLogTick,
                    'slaveState.lastAppliedContinuousTick:',
                    slaveState.state.lastAppliedContinuousTick,
                    'slaveState.lastProcessedContinuousTick:',
                    slaveState.state.lastProcessedContinuousTick);
      break;
    }

    if (!printed) {
      console.topic('replication=debug', 'waiting for slave to catch up');
      printed = true;
    }
    internal.wait(0.5, false);
  }

  internal.wait(1.0, false);
  db._flushCache();
  slaveFuncFinal(state);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Base Test Config. Identitical part for _system and other DB
// //////////////////////////////////////////////////////////////////////////////

function BaseTestConfig () {
  'use strict';

  return {
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test collection creation
    // //////////////////////////////////////////////////////////////////////////////

    testCreateCollection: function () {
      connectToMaster();

      compare(
        function (state) {
        },

        function (state) {
          db._create(cn);
          for (var i = 0; i < 100; ++i) {
            db._collection(cn).save({
              value: i
            });
          }
          internal.wal.flush(true, true);
        },

        function (state) {
          return true;
        },

        function (state) {
          assertTrue(db._collection(cn).count() === 100);
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test collection dropping
    // //////////////////////////////////////////////////////////////////////////////

    testDropCollection: function () {
      connectToMaster();

      compare(
        function (state) {
        },

        function (state) {
          db._create(cn);
          for (var i = 0; i < 100; ++i) {
            db._collection(cn).save({
              value: i
            });
          }
          db._drop(cn);
          internal.wal.flush(true, true);
        },

        function (state) {
          return true;
        },

        function (state) {
          assertNull(db._collection(cn));
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test index creation
    // //////////////////////////////////////////////////////////////////////////////

    testCreateIndex: function () {
      connectToMaster();

      compare(
        function (state) {
        },

        function (state) {
          db._create(cn);
          db._collection(cn).ensureIndex({
            type: 'hash',
            fields: ['value']
          });
        },

        function (state) {
          return true;
        },

        function (state) {
          var col = db._collection(cn);
          assertNotNull(col, 'collection does not exist');
          var idx = col.getIndexes();
          assertEqual(2, idx.length);
          assertEqual('primary', idx[0].type);
          assertEqual('hash', idx[1].type);
          assertEqual(['value'], idx[1].fields);
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test index dropping
    // //////////////////////////////////////////////////////////////////////////////

    testDropIndex: function () {
      connectToMaster();

      compare(
        function (state) {
        },

        function (state) {
          db._create(cn);
          var idx = db._collection(cn).ensureIndex({
            type: 'hash',
            fields: ['value']
          });
          db._collection(cn).dropIndex(idx);
        },

        function (state) {
          return true;
        },

        function (state) {
          var idx = db._collection(cn).getIndexes();
          assertEqual(1, idx.length);
          assertEqual('primary', idx[0].type);
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test renaming
    // //////////////////////////////////////////////////////////////////////////////

    testRenameCollection: function () {
      connectToMaster();

      compare(
        function (state) {
        },

        function (state) {
          db._create(cn);
          db._collection(cn).rename(cn + 'Renamed');
        },

        function (state) {
          return true;
        },

        function (state) {
          assertNull(db._collection(cn));
          assertNotNull(db._collection(cn + 'Renamed'));
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test renaming
    // //////////////////////////////////////////////////////////////////////////////

    testChangeCollection: function () {
      connectToMaster();

      compare(
        function (state) {
        },

        function (state) {
          db._create(cn);
          assertFalse(db._collection(cn).properties().waitForSync);
          db._collection(cn).properties({ waitForSync: true });
        },

        function (state) {
          return true;
        },

        function (state) {
          assertTrue(db._collection(cn).properties().waitForSync);
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test insert
    // //////////////////////////////////////////////////////////////////////////////

    testInsert: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
        },

        function (state) {
          for (let i = 0; i < 1000; ++i) {
            db[cn].insert({
              _key: 'test' + i,
              value: i
            });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);
        },

        function (state) {
        },

        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test remove
    // //////////////////////////////////////////////////////////////////////////////

    testRemove: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
        },

        function (state) {
          for (let i = 0; i < 1000; ++i) {
            db[cn].insert({
              _key: 'test' + i,
              value: i
            });
          }
          for (let i = 0; i < 100; ++i) {
            db[cn].remove('test' + i);
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(900, state.count);
        },

        function (state) {
        },

        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test long transaction, blocking
    // //////////////////////////////////////////////////////////////////////////////

    testLongTransactionBlocking: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
        },

        function (state) {
          db._executeTransaction({
            collections: {
              write: cn
            },
            action: function (params) {
              var wait = require('internal').wait;
              var db = require('internal').db;
              var c = db._collection(params.cn);

              for (var i = 0; i < 10; ++i) {
                c.save({
                  test1: i,
                  type: 'longTransactionBlocking',
                  coll: 'UnitTestsReplication'
                });
                c.save({
                  test2: i,
                  type: 'longTransactionBlocking',
                  coll: 'UnitTestsReplication'
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

        function (state) {
          // stop and restart replication on the slave
          assertTrue(replication.applier.state().state.running);
          replication.applier.stop();
          assertFalse(replication.applier.state().state.running);

          replication.applier.start();
          assertTrue(replication.applier.state().state.running);

          return true;
        },

        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test long transaction, asynchronous
    // //////////////////////////////////////////////////////////////////////////////

    testLongTransactionAsync: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
        },

        function (state) {
          var func = db._executeTransaction({
            collections: {
              write: cn
            },
            action: function (params) {
              var wait = require('internal').wait;
              var db = require('internal').db;
              var c = db._collection(params.cn);

              for (var i = 0; i < 10; ++i) {
                c.save({
                  test1: i,
                  type: 'longTransactionAsync',
                  coll: 'UnitTestsReplication'
                });
                c.save({
                  test2: i,
                  type: 'longTransactionAsync',
                  coll: 'UnitTestsReplication'
                });

                // intentionally delay the transaction
                wait(3.0, false);
              }
            },
            params: {
              cn: cn
            }
          });

          state.task = require('@arangodb/tasks').register({
            name: 'replication-test-async',
            command: String(func),
            params: {
              cn: cn
            }
          }).id;
        },

        function (state) {
          assertTrue(replication.applier.state().state.running);

          connectToMaster();
          try {
            require('@arangodb/tasks').get(state.task);
            // task exists
            connectToSlave();
            return 'wait';
          } catch (err) {
            // task does not exist. we're done
            state.lastLogTick = replication.logger.state().state.lastUncommittedLogTick;
            state.checksum = collectionChecksum(cn);
            state.count = collectionCount(cn);
            assertEqual(20, state.count);
            connectToSlave();
            return true;
          }
        },

        function (state) {
          assertTrue(state.hasOwnProperty('count'));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test long transaction, asynchronous
    // //////////////////////////////////////////////////////////////////////////////

    testLongTransactionAsyncWithSlaveRestarts: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
        },

        function (state) {
          var func = db._executeTransaction({
            collections: {
              write: cn
            },
            action: function (params) {
              var wait = require('internal').wait;
              var db = require('internal').db;
              var c = db._collection(params.cn);

              for (var i = 0; i < 10; ++i) {
                c.save({
                  test1: i,
                  type: 'longTransactionAsyncWithSlaveRestarts',
                  coll: 'UnitTestsReplication'
                });
                c.save({
                  test2: i,
                  type: 'longTransactionAsyncWithSlaveRestarts',
                  coll: 'UnitTestsReplication'
                });

                // intentionally delay the transaction
                wait(0.75, false);
              }
            },
            params: {
              cn: cn
            }
          });

          state.task = require('@arangodb/tasks').register({
            name: 'replication-test-async-with-restart',
            command: String(func),
            params: {
              cn: cn
            }
          }).id;
        },

        function (state) {
          // stop and restart replication on the slave
          assertTrue(replication.applier.state().state.running);
          replication.applier.stop();
          assertFalse(replication.applier.state().state.running);

          connectToMaster();
          try {
            require('@arangodb/tasks').get(state.task);
            // task exists
            connectToSlave();

            internal.wait(0.5, false);
            replication.applier.start();
            assertTrue(replication.applier.state().state.running);
            return 'wait';
          } catch (err) {
            // task does not exist anymore. we're done
            state.lastLogTick = replication.logger.state().state.lastUncommittedLogTick;
            state.checksum = collectionChecksum(cn);
            state.count = collectionCount(cn);
            assertEqual(20, state.count);
            connectToSlave();
            replication.applier.start();
            assertTrue(replication.applier.state().state.running);
            return true;
          }
        },

        function (state) {
          assertEqual(state.count, collectionCount(cn));
        }
      );
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite for _system
// //////////////////////////////////////////////////////////////////////////////

function ReplicationSuite () {
  'use strict';
  let suite = {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      connectToSlave();
      try {
        replication.applier.stop();
        replication.applier.forget();
      } catch (err) {
      }

      connectToMaster();

      db._drop(cn);
      db._drop(cn2);
      db._drop(cn + 'Renamed');
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      connectToMaster();

      db._drop(cn);
      db._drop(cn2);

      connectToSlave();
      replication.applier.stop();
      replication.applier.forget();

      db._drop(cn);
      db._drop(cn2);
      db._drop(cn + 'Renamed');
    }
  };

  deriveTestSuite(BaseTestConfig(), suite, '_Repl');

  return suite;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite for other database
// //////////////////////////////////////////////////////////////////////////////

function ReplicationOtherDBSuite () {
  'use strict';
  const dbName = 'UnitTestDB';

  // Setup documents to be stored on the master.

  let docs = [];
  for (let i = 0; i < 50; ++i) {
    docs.push({value: i});
  }

  // Shared function that sets up replication
  // of the collection and inserts 50 documents.
  const setupReplication = function () {
    // Section - Master
    connectToMaster();

    // Create the collection
    db._flushCache();
    db._create(cn);

    // Section - Follower
    connectToSlave();

    // Setup Replication
    replication.applier.stop();
    replication.applier.forget();

    while (replication.applier.state().state.running) {
      internal.wait(0.1, false);
    }

    let config = {
      endpoint: masterEndpoint,
      username: 'root',
      password: '',
      verbose: true,
      includeSystem: false
    };

    replication.setupReplication(config);

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

  let suite = {
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._useDatabase('_system');
      connectToSlave();
      try {
        replication.applier.stop();
        replication.applier.forget();
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
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._useDatabase('_system');

      connectToSlave();
      try {
        db._useDatabase(dbName);

        replication.applier.stop();
        replication.applier.forget();
      } catch (e) {
      }

      db._useDatabase('_system');
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }

      connectToMaster();

      db._useDatabase('_system');
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test dropping a database on slave while replication is ongoing
    // //////////////////////////////////////////////////////////////////////////////

    testDropDatabaseOnSlaveDuringReplication: function () {
      setupReplication();

      // Section - Slave
      connectToSlave();

      // Now do the evil stuff: drop the database that is replicating right now.
      db._useDatabase('_system');

      // This shall not fail.
      db._dropDatabase(dbName);

      // Section - Master
      connectToMaster();

      // Just write some more
      db._useDatabase(dbName);
      db._collection(cn).save(docs);
      internal.wal.flush(true, true);
      internal.wait(6, false);

      db._useDatabase('_system');

      // Section - Slave
      connectToSlave();

      // The DB should be gone and the server should be running.
      let dbs = db._databases();
      assertEqual(-1, dbs.indexOf(dbName));

      // We can setup everything here without problems.
      try {
        db._createDatabase(dbName);
      } catch (e) {
        assertFalse(true, 'Could not recreate database on slave: ' + e);
      }

      db._useDatabase(dbName);

      try {
        db._create(cn);
      } catch (e) {
        assertFalse(true, 'Could not recreate collection on slave: ' + e);
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
    },

    testDropDatabaseOnMasterDuringReplication: function () {
      setupReplication();

      // Section - Master
      // Now do the evil stuff: drop the database that is replicating from right now.
      connectToMaster();
      db._useDatabase('_system');

      // This shall not fail.
      db._dropDatabase(dbName);

      // The DB should be gone and the server should be running.
      let dbs = db._databases();
      assertEqual(-1, dbs.indexOf(dbName));

      // Now recreate a new database with this name
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      db._create(cn);
      db._collection(cn).save(docs);

      // Section - Slave
      connectToSlave();

      // Give it some time to sync (eventually, should not do anything...)
      internal.wait(6, false);

      // Now test if the Slave did replicate the new database directly...
      assertEqual(50, collectionCount(cn),
                  'The slave inserted the new collection data into the old one, it skipped the drop.');
    }
  };

  deriveTestSuite(BaseTestConfig(), suite, '_ReplOther');
  return suite;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationSuite);
jsunity.run(ReplicationOtherDBSuite);

return jsunity.done();
