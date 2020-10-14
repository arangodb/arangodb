/* jshint globalstrict:false, strict:false, unused: false */
/* global fail, assertEqual, assertTrue, assertFalse, assertNull, assertNotNull, arango, ARGUMENTS */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the replication
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
var analyzers = require("@arangodb/analyzers");
const db = arangodb.db;

const replication = require('@arangodb/replication');
const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const compareTicks = replication.compareTicks;
const console = require('console');
const internal = require('internal');
const masterEndpoint = arango.getEndpoint();
const slaveEndpoint = ARGUMENTS[0];

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
    includeSystem: true,
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
  applierConfiguration.force32mode = false;
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
      throw JSON.stringify(slaveState.state.lastError);
    }

    if (!slaveState.state.running) {
      console.topic('replication=error', 'slave is not running');
      break;
    }

    if (compareTicks(slaveState.state.lastAppliedContinuousTick, state.lastLogTick) >= 0 ||
        compareTicks(slaveState.state.lastProcessedContinuousTick, state.lastLogTick) >= 0) { // ||
        console.topic('replication=debug',
                      'slave has caught up. state.lastLogTick:', state.lastLogTick,
                      'slaveState.lastAppliedContinuousTick:', slaveState.state.lastAppliedContinuousTick,
                      'slaveState.lastProcessedContinuousTick:', slaveState.state.lastProcessedContinuousTick);
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
    
    testStatsInsert: function () {
      connectToMaster();

      compare(
        function (state) {
        },

        function (state) {
          db._create(cn);
          for (let i = 0; i < 1000; ++i) {
            db._collection(cn).insert({ _key: "test" + i });
          }
          internal.wal.flush(true, true);
        },

        function (state) {
          return true;
        },

        function (state) {
          let s = replication.applier.state().state;
          assertTrue(s.totalDocuments >= 1000);
          assertTrue(s.totalFetchTime > 0);
          assertTrue(s.totalApplyTime > 0);
          assertTrue(s.averageApplyTime > 0);
        }
      );
    },
    
    testStatsInsertUpdate: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
          for (let i = 0; i < 1000; ++i) {
            db._collection(cn).insert({ _key: "test" + i });
          }
          internal.wal.flush(true, true);
        },

        function (state) {
          for (let i = 0; i < 1000; ++i) {
            db._collection(cn).update("test" + i, { value: i });
          }
          internal.wal.flush(true, true);
        },

        function (state) {
          return true;
        },

        function (state) {
          let s = replication.applier.state().state;
          assertTrue(s.totalDocuments >= 1000);
          assertTrue(s.totalFetchTime > 0);
          assertTrue(s.totalApplyTime > 0);
          assertTrue(s.averageApplyTime > 0);
        }
      );
    },
    
    testStatsRemove: function () {
      connectToMaster();

      compare(
        function (state) {
        },

        function (state) {
          db._create(cn);
          for (let i = 0; i < 1000; ++i) {
            db._collection(cn).insert({ _key: "test" + i });
          }
          for (let i = 0; i < 1000; ++i) {
            db._collection(cn).remove("test" + i);
          }
          internal.wal.flush(true, true);
        },

        function (state) {
          return true;
        },

        function (state) {
          let s = replication.applier.state().state;
          assertTrue(s.totalDocuments >= 1000);
          assertTrue(s.totalRemovals >= 1000);
          assertTrue(s.totalFetchTime > 0);
          assertTrue(s.totalApplyTime > 0);
          assertTrue(s.averageApplyTime > 0);
        }
      );
    },

    testIncludeCollection: function () {
      connectToMaster();

      compare(
        function (state) {
          db._drop(cn);
          db._drop(cn + '2');
        },

        function (state) {
          db._create(cn);
          db._create(cn + '2');
          for (var i = 0; i < 100; ++i) {
            db._collection(cn).save({
              value: i
            });
            db._collection(cn + '2').save({
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
          assertNull(db._collection(cn + '2'));
        },

        {
          restrictType: 'include',
          restrictCollections: [cn]
        }
      );
    },

    testExcludeCollection: function () {
      connectToMaster();

      compare(
        function (state) {
          db._drop(cn);
          db._drop(cn + '2');
        },

        function (state) {
          db._create(cn);
          db._create(cn + '2');
          for (var i = 0; i < 100; ++i) {
            db._collection(cn).save({
              value: i
            });
            db._collection(cn + '2').save({
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
          assertNull(db._collection(cn + '2'));
        },

        {
          restrictType: 'exclude',
          restrictCollections: [cn + '2']
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test duplicate _key issue and replacement
    // //////////////////////////////////////////////////////////////////////////////

    testPrimaryKeyConflict: function () {
      connectToMaster();

      compare(
        function (state) {
          db._drop(cn);
          db._create(cn);
        },

        function (state) {
          // insert same record on slave that we will insert on the master
          connectToSlave();
          db[cn].insert({
            _key: 'boom',
            who: 'slave'
          });
          connectToMaster();
          db[cn].insert({
            _key: 'boom',
            who: 'master'
          });
        },

        function (state) {
          return true;
        },

        function (state) {
          // master document version must have one
          assertEqual('master', db[cn].document('boom').who);
        }
      );
    },

    testPrimaryKeyConflictInTransaction: function() {
      connectToMaster();

      compare(
        function(state) {
          db._drop(cn);
          db._create(cn);
        },

        function(state) {
          // insert same record on slave that we will insert on the master
          connectToSlave();
          db[cn].insert({ _key: "boom", who: "slave" });
          connectToMaster();
          db._executeTransaction({ 
            collections: { write: cn },
            action: function(params) {
              let db = require("internal").db;
              db[params.cn].insert({ _key: "meow", foo: "bar" });
              db[params.cn].insert({ _key: "boom", who: "master" });
            },
            params: { cn }
          });
        },

        function(state) {
          return true;
        },

        function(state) {
          assertEqual(2, db[cn].count());
          assertEqual("bar", db[cn].document("meow").foo);
          // master document version must have won
          assertEqual("master", db[cn].document("boom").who);
        }
      );
    },

    testSecondaryKeyConflict: function () {
      connectToMaster();

      compare(
        function (state) {
          db._drop(cn);
          db._create(cn);
          db[cn].ensureIndex({
            type: 'hash',
            fields: ['value'],
            unique: true
          });
        },

        function (state) {
          // insert same record on slave that we will insert on the master
          connectToSlave();
          db[cn].insert({
            _key: 'slave',
            value: 'one'
          });
          connectToMaster();
          db[cn].insert({
            _key: 'master',
            value: 'one'
          });
        },

        function (state) {
          return true;
        },

        function (state) {
          assertNull(db[cn].firstExample({
            _key: 'slave'
          }));
          assertNotNull(db[cn].firstExample({
            _key: 'master'
          }));
          assertEqual('master', db[cn].toArray()[0]._key);
          assertEqual('one', db[cn].toArray()[0].value);
        }
      );
    },

    testSecondaryKeyConflictInTransaction: function() {
      connectToMaster();

      compare(
        function(state) {
          db._drop(cn);
          db._create(cn);
          db[cn].ensureIndex({ type: "hash", fields: ["value"], unique: true });
        },

        function(state) {
          // insert same record on slave that we will insert on the master
          connectToSlave();
          db[cn].insert({ _key: "slave", value: "one" });
          connectToMaster();
          db._executeTransaction({ 
            collections: { write: cn },
            action: function(params) {
              let db = require("internal").db;
              db[params.cn].insert({ _key: "meow", value: "abc" });
              db[params.cn].insert({ _key: "master", value: "one" });
            },
            params: { cn }
          });
        },

        function(state) {
          return true;
        },

        function(state) {
          assertEqual(2, db[cn].count());
          assertEqual("abc", db[cn].document("meow").value);

          assertNull(db[cn].firstExample({ _key: "slave" }));
          assertNotNull(db[cn].firstExample({ _key: "master" }));
          let docs = db[cn].toArray();
          docs.sort(function(l, r) { 
            if (l._key !== r._key) {
              return l._key < r._key ? -1 : 1;
            }
            return 0;
          });
          assertEqual("master", docs[0]._key);
          assertEqual("one", docs[0].value);
        }
      );
    },

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
          db._collection(cn).properties({
            waitForSync: true
          });
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
    // / @brief test truncating a small collection
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateCollectionSmall: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          for (let i = 0; i < 1000; i++) {
            c.insert({
              value: i
            });
          }
        },

        function (state) {
          db._collection(cn).truncate({ compact: false });
        },

        function (state) {
          return true;
        },

        function (state) {
          const c = db._collection(cn);
          let x = 10;
          while (c.count() > 0 && x-- > 0) {
            internal.sleep(1);
          }
          assertEqual(c.count(), 0);
          assertEqual(c.all().toArray().length, 0);
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test truncating a bigger collection
    // //////////////////////////////////////////////////////////////////////////////

    testTruncateCollectionBigger: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];
          for (let i = 0; i < (32 * 1024 + 1); i++) {
            docs.push({
              value: i
            });
            if (docs.length >= 1000) {
              c.insert(docs);
              docs = [];
            }
          }
          c.insert(docs);
        },

        function (state) {
          db._collection(cn).truncate(); // should hit range-delete in rocksdb
        },

        function (state) {
          return true;
        },

        function (state) {
          assertEqual(db._collection(cn).count(), 0);
          assertEqual(db._collection(cn).all().toArray().length, 0);
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
    },

    testViewBasic: function () {
      connectToMaster();

      compare(
        function () { },
        function (state) {
          try {
            db._create(cn);
            let view = db._createView('UnitTestsSyncView', 'arangosearch', {});
            let links = {};
            links[cn] = {
              includeAllFields: true,
              fields: {
                text: { analyzers: ['text_en'] }
              }
            };
            view.properties({
              'links': links
            });
            state.arangoSearchEnabled = true;
          } catch (err) { }
        },
        function () { },
        function (state) {
          if (!state.arangoSearchEnabled) {
            return;
          }

          let view = db._view('UnitTestsSyncView');
          assertTrue(view !== null);
          let props = view.properties();
          assertEqual(Object.keys(props.links).length, 1);
          assertTrue(props.hasOwnProperty('links'));
          assertTrue(props.links.hasOwnProperty(cn));
        },
        {}
      );
    },

    testViewCreateWithLinks: function () {
      connectToMaster();

      compare(
        function () { },
        function (state) {
          try {
            db._create(cn);
            let links = {};
            links[cn] = {
              includeAllFields: true,
              fields: {
                text: { analyzers: ['text_en'] }
              }
            };
            db._createView('UnitTestsSyncView', 'arangosearch', { 'links': links });
            state.arangoSearchEnabled = true;
          } catch (err) { }
        },
        function () { },
        function (state) {
          if (!state.arangoSearchEnabled) {
            return;
          }

          let view = db._view('UnitTestsSyncView');
          assertTrue(view !== null);
          let props = view.properties();
          assertEqual(Object.keys(props.links).length, 1);
          assertTrue(props.hasOwnProperty('links'));
          assertTrue(props.links.hasOwnProperty(cn));
        },
        {}
      );
    },

    testViewRename: function () {
      connectToMaster();

      compare(
        function (state) {
          try {
            db._create(cn);
            let view = db._createView('UnitTestsSyncView', 'arangosearch', {});
            let links = {};
            links[cn] = {
              includeAllFields: true,
              fields: {
                text: { analyzers: ['text_en'] }
              }
            };
            view.properties({
              'links': links
            });
            state.arangoSearchEnabled = true;
          } catch (err) { }
        },
        function (state) {
          if (!state.arangoSearchEnabled) {
            return;
          }
          // rename view on master
          let view = db._view('UnitTestsSyncView');
          view.rename('UnitTestsSyncViewRenamed');
          view = db._view('UnitTestsSyncViewRenamed');
          assertTrue(view !== null);
          let props = view.properties();
          assertEqual(Object.keys(props.links).length, 1);
          assertTrue(props.hasOwnProperty('links'));
          assertTrue(props.links.hasOwnProperty(cn));
        },
        function (state) { },
        function (state) {
          if (!state.arangoSearchEnabled) {
            return;
          }

          let view = db._view('UnitTestsSyncViewRenamed');
          assertTrue(view !== null);
          let props = view.properties();
          assertEqual(Object.keys(props.links).length, 1);
          assertTrue(props.hasOwnProperty('links'));
          assertTrue(props.links.hasOwnProperty(cn));
        },
        {}
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test property update
    // //////////////////////////////////////////////////////////////////////////////
    testViewPropertiesUpdate: function () {
      connectToMaster();
      compare(
        function (state) { // masterFunc1
          try {
            db._create(cn);
            db._createView(cn + 'View', 'arangosearch');
            state.arangoSearchEnabled = true;
          } catch (err) {
            db._drop(cn);
          }
        },
        function (state) { // masterFunc2
          if (!state.arangoSearchEnabled) {
            return;
          }
          let view = db._view(cn + 'View', 'arangosearch');
          let links = {};
          links[cn] = {
            includeAllFields: true,
            fields: {
              text: { analyzers: ['text_en'] }
            }
          };
          view.properties({
            links
          });
        },
        function () { // slaveFuncOngoing
        }, // slaveFuncOngoing
        function (state) { // slaveFuncFinal
          if (!state.arangoSearchEnabled) {
            return;
          }

          var idx = db._collection(cn).getIndexes();
          assertEqual(1, idx.length); // primary

          let view = db._view(cn + 'View');
          assertTrue(view !== null);
          let props = view.properties();
          assertTrue(props.hasOwnProperty('links'));
          assertEqual(Object.keys(props.links).length, 1);
          assertTrue(props.links.hasOwnProperty(cn));
        });
    },

    testViewDrop: function () {
      connectToMaster();

      compare(
        function (state) {
          try {
            let view = db._createView('UnitTestsSyncView', 'arangosearch', {});
            state.arangoSearchEnabled = true;
          } catch (err) { }
        },
        function (state) {
          if (!state.arangoSearchEnabled) {
            return;
          }
          // rename view on master
          let view = db._view('UnitTestsSyncView');
          view.drop();
        },
        function (state) { },
        function (state) {
          if (!state.arangoSearchEnabled) {
            return;
          }

          let view = db._view('UnitTestsSyncView');
          let x = 10;
          while (view && x-- > 0) {
            internal.sleep(1);
            db._flushCache();
            view = db._view('UnitTestsSyncView');
          }
          assertNull(view);
        },
        {}
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test with views
    // //////////////////////////////////////////////////////////////////////////////

    testViewData: function () {
      connectToMaster();

      compare(
        function (state) { // masterFunc1
          try {
            let c = db._create(cn);
            let links = {};
            links[cn] = {
              includeAllFields: true,
              fields: {
                text: { analyzers: ['text_en'] }
              }
            };
            let view = db._createView(cn + 'View', 'arangosearch', { links: links });
            assertEqual(Object.keys(view.properties().links).length, 1);

            let docs = [];
            for (let i = 0; i < 5000; ++i) {
              docs.push({
                _key: 'test' + i,
                'value': i
              });
            }
            c.insert(docs);

            state.arangoSearchEnabled = true;
          } catch (err) {
            db._drop(cn);
          }
        },
        function (state) { // masterFunc2
          if (!state.arangoSearchEnabled) {
            return;
          }
          const txt = 'the red foxx jumps over the pond';
          db._collection(cn).save({
            _key: 'testxxx',
            'value': -1,
            'text': txt
          });
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5001, state.count);
        },
        function (state) { // slaveFuncOngoing
          if (!state.arangoSearchEnabled) {
            return;
          }
          let view = db._view(cn + 'View');
          assertTrue(view !== null);
          let props = view.properties();
          assertTrue(props.hasOwnProperty('links'));
          assertEqual(Object.keys(props.links).length, 1);
          assertTrue(props.links.hasOwnProperty(cn));
        }, // slaveFuncOngoing
        function (state) { // slaveFuncFinal
          if (!state.arangoSearchEnabled) {
            return;
          }

          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
          var idx = db._collection(cn).getIndexes();
          assertEqual(1, idx.length); // primary

          let view = db._view(cn + 'View');
          assertTrue(view !== null);
          let props = view.properties();
          assertTrue(props.hasOwnProperty('links'));
          assertEqual(Object.keys(props.links).length, 1);
          assertTrue(props.links.hasOwnProperty(cn));

          let res = db._query('FOR doc IN ' + view.name() + ' SEARCH doc.value >= 2500 OPTIONS { waitForSync: true } RETURN doc').toArray();
          assertEqual(2500, res.length);

          res = db._query('FOR doc IN ' + view.name() + ' SEARCH PHRASE(doc.text, "foxx jumps over", "text_en") OPTIONS { waitForSync: true } RETURN doc').toArray();
          assertEqual(1, res.length);
        });
    },
    testViewDataCustomAnalyzer: function () {
      connectToMaster();
      compare(
        function (state) { // masterFunc1
          try {
            analyzers.save('custom', 'identity', {});
            let c = db._create(cn);
            let links = {};
            links[cn] = {
              includeAllFields: true,
              fields: {
                text: { analyzers: ['custom'] }
              }
            };
            let view = db._createView(cn + 'View', 'arangosearch', { links: links });
            assertEqual(Object.keys(view.properties().links).length, 1);

            let docs = [];
            for (let i = 0; i < 5000; ++i) {
              docs.push({
                _key: 'test' + i,
                'value': i
              });
            }
            c.insert(docs);

            state.arangoSearchEnabled = true;
          } catch (err) {
            analyzers.remove("custom", true);
            db._drop(cn);
          } 
        },
        function (state) { // masterFunc2
          if (!state.arangoSearchEnabled) {
            return;
          }
          const txt = 'foxx';
          db._collection(cn).save({
            _key: 'testxxx',
            'value': -1,
            'text': txt
          });
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5001, state.count);
          analyzers.save('custom2', 'identity', {});
        },
        function (state) { // slaveFuncOngoing
          if (!state.arangoSearchEnabled) {
            return;
          }
          let view = db._view(cn + 'View');
          assertNotNull(view);
          let props = view.properties();
          assertTrue(props.hasOwnProperty('links'));
          assertEqual(Object.keys(props.links).length, 1);
          assertTrue(props.links.hasOwnProperty(cn));
          // do not check results. We need to trigger analyzers cache reload
          db._query('FOR doc IN ' + view.name() + ' SEARCH PHRASE(doc.text, "foxx", "custom") '
                    + ' OPTIONS { waitForSync: true } RETURN doc').toArray();
        }, // slaveFuncOngoing
        function (state) { // slaveFuncFinal
          if (!state.arangoSearchEnabled) {
            return;
          }

          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
          var idx = db._collection(cn).getIndexes();
          assertEqual(1, idx.length); // primary

          let view = db._view(cn + 'View');
          assertNotNull(view);
          let props = view.properties();
          assertTrue(props.hasOwnProperty('links'));
          assertEqual(Object.keys(props.links).length, 1);
          assertTrue(props.links.hasOwnProperty(cn));

          let res = db._query('FOR doc IN ' + view.name() + ' SEARCH doc.value >= 2500 OPTIONS { waitForSync: true } RETURN doc').toArray();
          assertEqual(2500, res.length);
          // analyzer here was added later so slave must properly reload its analyzers cache
          res = db._query('FOR doc IN ' + view.name() + ' SEARCH PHRASE(doc.text, TOKENS("foxx","custom2")[0], "custom") OPTIONS { waitForSync: true } RETURN doc').toArray();
          assertEqual(1, res.length);
        });
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
      }
      catch (err) {
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

      db._dropView('UnitTestsSyncView');
      db._dropView('UnitTestsSyncViewRenamed');
      db._dropView(cn + 'View');
      db._drop(cn);
      db._drop(cn2);
      db._analyzers.toArray().forEach(function(analyzer) {
        try { analyzers.remove(analyzer.name, true); } catch (err) {}
      });

      connectToSlave();
      replication.applier.stop();
      replication.applier.forget();

      db._dropView('UnitTestsSyncView');
      db._dropView('UnitTestsSyncViewRenamed');
      db._dropView(cn + 'View');
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
    docs.push({
      value: i
    });
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
      includeSystem: true
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

      const lastLogTick = replication.logger.state().state.lastUncommittedLogTick;

      // Section - Slave
      connectToSlave();

      // Give it some time to sync (eventually, should not do anything...)
      let i = 30;
      while (i-- > 0) {
        let state = replication.applier.state();
        if (!state.running) {
          console.topic('replication=error', 'slave is not running');
          break;
        }
        if (compareTicks(state.lastAppliedContinuousTick, lastLogTick) >= 0 ||
            compareTicks(state.lastProcessedContinuousTick, lastLogTick) >= 0) {
          console.topic('replication=error', 'slave has caught up');
          break;
        }
        internal.sleep(0.5);
      }

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
      
      const lastLogTick = replication.logger.state().state.lastUncommittedLogTick;

      // Section - Slave
      connectToSlave();

      // Give it some time to sync (eventually, should not do anything...)
      let i = 30;
      while (i-- > 0) {
        let state = replication.applier.state();
        if (!state.running) {
          console.topic('replication=error', 'slave is not running');
          break;
        }
        if (compareTicks(state.lastAppliedContinuousTick, lastLogTick) >= 0 ||
            compareTicks(state.lastProcessedContinuousTick, lastLogTick) >= 0) {
          console.topic('replication=error', 'slave has caught up');
          break;
        }
        internal.sleep(0.5);
      }

      i = 60;
      while (i-- > 0) {
        let state = replication.applier.state();
        if (!state.running) {
          // all good
          return;
        }
        internal.sleep(0.5);
      }

      fail();
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
