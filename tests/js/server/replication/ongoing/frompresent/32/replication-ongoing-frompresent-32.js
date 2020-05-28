/* jshint globalstrict:false, strict:false, unused: false */
/* global assertEqual, assertTrue, assertFalse, arango, ARGUMENTS */

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
var slaveEndpoint = ARGUMENTS[0];

var cn = 'UnitTestsReplication';
var cn2 = 'UnitTestsReplication2';

var connectToMaster = function () {
  reconnectRetry(masterEndpoint, db._name(), 'root', '');
  db._flushCache();
};

var connectToSlave = function () {
  reconnectRetry(slaveEndpoint, db._name(), 'root', '');
  db._flushCache();
};

var collectionChecksum = function (name) {
  var c = db._collection(name).checksum(true, true);
  return c.checksum;
};

var collectionCount = function (name) {
  return db._collection(name).count();
};

var compare = function (masterFunc, masterFunc2, slaveFuncOngoing, slaveFuncFinal, applierConfiguration) {
  var state = {};

  db._flushCache();
  masterFunc(state);

  connectToSlave();
  replication.applier.stop();
  replication.applier.forget();

  while (replication.applier.state().state.running) {
    internal.wait(0.1, false);
  }

  var includeSystem = true;
  var restrictType = '';
  var restrictCollections = [];
  applierConfiguration = applierConfiguration || { };

  if (typeof applierConfiguration === 'object') {
    if (applierConfiguration.hasOwnProperty('includeSystem')) {
      includeSystem = applierConfiguration.includeSystem;
    }
    if (applierConfiguration.hasOwnProperty('restrictType')) {
      restrictType = applierConfiguration.restrictType;
    }
    if (applierConfiguration.hasOwnProperty('restrictCollections')) {
      restrictCollections = applierConfiguration.restrictCollections;
    }
  }

  var keepBarrier = false;
  if (applierConfiguration.hasOwnProperty('keepBarrier')) {
    keepBarrier = applierConfiguration.keepBarrier;
  }

  var syncResult = replication.sync({
    endpoint: masterEndpoint,
    username: 'root',
    password: '',
    verbose: true,
    includeSystem,
    restrictType,
    restrictCollections,
    keepBarrier
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

  if (!applierConfiguration.hasOwnProperty('chunkSize')) {
    applierConfiguration.chunkSize = 16384;
  }

  connectToSlave();

  replication.applier.properties(applierConfiguration);
  if (keepBarrier) {
    replication.applier.start(syncResult.lastLogTick, syncResult.barrierId);
  } else {
    replication.applier.start(syncResult.lastLogTick);
  }

  var printed = false;

  while (true) {
    var r = slaveFuncOngoing(state);
    if (r === 'wait') {
      //  special return code that tells us to hang on
      internal.wait(0.5, false);
      continue;
    }
    if (!r) {
      break;
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

    if (compareTicks(slaveState.state.lastAppliedContinuousTick, state.lastLogTick) >= 0 ||
        compareTicks(slaveState.state.lastProcessedContinuousTick, state.lastLogTick) >= 0) {
      console.topic('replication=debug',
                    'slave has caught up. syncResult.lastLogTick:', state.lastLogTick,
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

  db._flushCache();
  slaveFuncFinal(state);
};

var stopApplier = function (dbName) {
  connectToSlave();
  try {
    db._useDatabase(dbName);

    replication.applier.stop();
    replication.applier.forget();
  } catch (err) {
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Basic test definition
// //////////////////////////////////////////////////////////////////////////////

function BaseTestConfig () {
  'use strict';

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test require from present
    // //////////////////////////////////////////////////////////////////////////////

    testRequireFromPresentFalse: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
        },

        function (state) {
          //  flush the wal logs on the master so the start tick is not available
          //  anymore when we start replicating
          for (var i = 0; i < 30; ++i) {
            db._collection(cn).save({
              value: i
            });
            internal.wal.flush();
          }
          db._collection(cn).save({
            value: i
          });
          internal.wal.flush(true, true);
          internal.wait(6, false);
        },

        function (state) {
          return true;
        },

        function (state) {
          //  rocksdb keeps wal longer
          let cc = db._collection(cn).count();
          assertEqual(cc, 31, 'rocksdb must keep wal, documents not there');
        }, {
          requireFromPresent: false,
          keepBarrier: false
        }
      );
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test require from present
    // //////////////////////////////////////////////////////////////////////////////

    testRequireFromPresentTrue: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
        },

        function (state) {
          //  flush the wal logs on the master so the start tick is not available
          //  anymore when we start replicating
          for (var i = 0; i < 30; ++i) {
            db._collection(cn).save({
              value: i
            });
            internal.wal.flush();
          }
          internal.wal.flush(true, true);
          internal.wait(6, false);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(30, state.count);
        },

        function (state) {
          //  wait for slave applier to have started and run
          internal.wait(5, false);

          //  slave should not have stopped
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

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test require from present, no barrier
    // //////////////////////////////////////////////////////////////////////////////

    testRequireFromPresentTrueNoBarrier: function () {
      connectToMaster();

      compare(
        function (state) {
          db._create(cn);
        },

        function (state) {
          //  flush the wal logs on the master so the start tick is not available
          //  anymore when we start replicating
          for (var i = 0; i < 30; ++i) {
            db._collection(cn).save({
              value: i
            });
            internal.wal.flush();
          }
          internal.wal.flush(true, true);
          internal.wait(6, false);
        },

        function (state) {
          //  wait for slave applier to have started and detect the mess
          return replication.applier.state().state.running;
        },

        function (state) {
          //  rocksdb keeps wal longer
          assertTrue(replication.applier.state().state.running, 'Applier should be running');
          assertEqual(db._collection(cn).count(), 30, 'rocksdb must keep wal');
        },
        {
          requireFromPresent: true,
          keepBarrier: false
        }
      );
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Test suite for _system database
// //////////////////////////////////////////////////////////////////////////////

function ReplicationSuite () {
  let suite = {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      stopApplier('_system');

      connectToMaster();

      db._drop(cn);
      db._drop(cn2);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      stopApplier('_system');

      connectToMaster();
      db._drop(cn);
      db._drop(cn2);

      connectToSlave();
      db._drop(cn);
      db._drop(cn2);
    }
  };
  deriveTestSuite(BaseTestConfig(), suite, '_Repl');

  return suite;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Test suite for other database
// //////////////////////////////////////////////////////////////////////////////

function ReplicationOtherDBSuite () {
  const dbName = 'UnitTestDB';

  let suite = {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      stopApplier(dbName);

      db._useDatabase('_system');
      connectToSlave();
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
      stopApplier(dbName);

      db._useDatabase('_system');
      connectToMaster();
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }

      db._useDatabase('_system');
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }
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
