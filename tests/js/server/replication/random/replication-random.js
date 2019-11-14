/*jshint globalstrict:false, strict:false, unused: false */
/*global assertEqual, assertTrue, arango, ARGUMENTS */

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
/// @author Copyright 2017, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;

const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
const replication = require("@arangodb/replication");
const compareTicks = replication.compareTicks;
const console = require("console");
const internal = require("internal");
const masterEndpoint = arango.getEndpoint();
const slaveEndpoint = ARGUMENTS[0];

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ReplicationSuite() {
  'use strict';
  var cn = "UnitTestsReplication";
  var cn2 = "UnitTestsReplication2";
  var cn3 = "UnitTestsReplication3";

  var connectToMaster = function() {
    reconnectRetry(masterEndpoint, db._name(), "root", "");
    db._flushCache();
  };

  var connectToSlave = function() {
    reconnectRetry(slaveEndpoint, db._name(), "root", "");
    db._flushCache();
  };

  var collectionChecksum = function(name) {
    var c = db._collection(name).checksum(true, true);
    return c.checksum;
  };

  var collectionCount = function(name) {
    return db._collection(name).count();
  };

  var compare = function(masterFunc, masterFunc2, slaveFuncFinal) {
    var state = {};

    assertEqual(cn, db._name());
    db._flushCache();
    masterFunc(state);
    
    connectToSlave();
    assertEqual(cn, db._name());

    var syncResult = replication.sync({
      endpoint: masterEndpoint,
      username: "root",
      password: "",
      verbose: true,
      includeSystem: false,
      keepBarrier: true,
      requireFromPresent: true,
    });

    assertTrue(syncResult.hasOwnProperty('lastLogTick'));

    connectToMaster();
    masterFunc2(state);

    // use lastLogTick as of now
    state.lastLogTick = replication.logger.state().state.lastUncommittedLogTick;

    let applierConfiguration = {
      endpoint: masterEndpoint,
      username: "root",
      password: "", 
      requireFromPresent: true 
    };

    connectToSlave();
    assertEqual(cn, db._name());

    replication.applier.properties(applierConfiguration);
    replication.applier.start(syncResult.lastLogTick, syncResult.barrierId);

    var printed = false;

    while (true) {
      var slaveState = replication.applier.state();

      if (slaveState.state.lastError.errorNum > 0) {
        console.topic("replication=error", "slave has errored:", JSON.stringify(slaveState.state.lastError));
        throw JSON.stringify(slaveState.state.lastError);
      }

      if (!slaveState.state.running) {
        console.topic("replication=error", "slave is not running");
        break;
      }

      if (compareTicks(slaveState.state.lastAppliedContinuousTick, state.lastLogTick) >= 0 ||
          compareTicks(slaveState.state.lastProcessedContinuousTick, state.lastLogTick) >= 0) { // ||
        console.topic("replication=debug", "slave has caught up. state.lastLogTick:", state.lastLogTick, "slaveState.lastAppliedContinuousTick:", slaveState.state.lastAppliedContinuousTick, "slaveState.lastProcessedContinuousTick:", slaveState.state.lastProcessedContinuousTick);
        break;
      }
        
      if (!printed) {
        console.topic("replication=debug", "waiting for slave to catch up");
        printed = true;
      }
      internal.wait(0.5, false);
    }

    db._flushCache();
    slaveFuncFinal(state);
  };

  return {

    setUp: function() {
      db._useDatabase("_system");
      connectToMaster();
      try {
        db._dropDatabase(cn);
      } catch (err) {}

      db._createDatabase(cn);
      db._useDatabase(cn);

      db._create(cn); 
      db._create(cn2); 
      db._create(cn3); 
      
      db._useDatabase("_system");
      connectToSlave();
      
      try {
        db._dropDatabase(cn);
      } catch (err) {}

      db._createDatabase(cn);
    },

    tearDown: function() {
      db._useDatabase("_system");
      connectToMaster();

      db._useDatabase(cn);
      connectToSlave();
      replication.applier.stop();
      replication.applier.forget();
      
      db._useDatabase("_system");
      db._dropDatabase(cn);
    },
    
    testRandomTransactions: function() {
      let nextId = 0;
      let keys = { [cn]: [], [cn2]: [], [cn3]: [] };

      let nextKey = function() {
        return "test" + (++nextId);
      };

      let generateInsert = function(collections) {
        let c = collections[Math.floor(Math.random() * collections.length)];
        let key = nextKey();
        keys[c][key] = 1;

        return "db[" + JSON.stringify(c) + "].insert({ _key: " + JSON.stringify(key) + ", value: \"thisIsSomeStringValue\" });";
      };

      let generateUpdate = function(collections) {
        let c = collections[Math.floor(Math.random() * collections.length)];
        let all = Object.keys(keys[c]);
        if (all.length === 0) {
          // still empty, turn into an insert first
          return generateInsert(collections);
        }
        let key = all[Math.floor(Math.random() * all.length)];
        return "db[" + JSON.stringify(c) + "].update(" + JSON.stringify(key) + ", { value: \"thisIsSomeUpdatedStringValue\" });";
      };
      
      let generateReplace = function(collections) {
        let c = collections[Math.floor(Math.random() * collections.length)];
        let all = Object.keys(keys[c]);
        if (all.length === 0) {
          // still empty, turn into an insert first
          return generateInsert(collections);
        }
        let key = all[Math.floor(Math.random() * all.length)];
        return "db[" + JSON.stringify(c) + "].replace(" + JSON.stringify(key) + ", { value: \"thisIsSomeReplacedStringValue\" });";
      };
      
      let generateRemove = function(collections) {
        let c = collections[Math.floor(Math.random() * collections.length)];
        let all = Object.keys(keys[c]);
        if (all.length === 0) {
          // still empty, turn into an insert first
          return generateInsert(collections);
        }
        let key = all[Math.floor(Math.random() * all.length)];
        delete keys[c][key];
        return "db[" + JSON.stringify(c) + "].remove(" + JSON.stringify(key) + ");";
      };
      
      let generateTruncate = function(collections) {
        let c = collections[Math.floor(Math.random() * collections.length)];
        keys[c] = {};
        return "db[" + JSON.stringify(c) + "].truncate();";
      };

      let allOps = [
        { name: "insert", generate: generateInsert }, 
        { name: "update", generate: generateUpdate }, 
        { name: "replace", generate: generateReplace }, 
        { name: "remove", generate: generateRemove },
//        { name: "truncate", generate: generateTruncate }
      ]; 

      let createTransaction = function(state) {
        let trx = { collections: { read: [], write: [] } };

        // determine collections
        do {
          if (Math.random() >= 0.5) {
            trx.collections.write.push(cn);
          }
          if (Math.random() >= 0.5) {
            trx.collections.write.push(cn2);
          }
          if (Math.random() >= 0.5) {
            trx.collections.write.push(cn3);
          }
        } while (trx.collections.write.length === 0);

        let n = Math.floor(Math.random() * 100) + 1;
        let ops = "function() { let db = require('internal').db;\n";
        for (let i = 0; i < n; ++i) {
          ops += allOps[Math.floor(Math.random() * allOps.length)].generate(trx.collections.write) + "\n";
        }
        trx.action = ops + " }";
        return trx;
      };

      db._useDatabase(cn);
      connectToMaster();

      compare(
        function(state) {
        },

        function(state) {
          for (let i = 0; i < 10000; ++i) {
            let trx = createTransaction(state);
            db._executeTransaction(trx);
          }

          state.checksum = collectionChecksum(cn);
          state.checksum2 = collectionChecksum(cn2);
          state.checksum3 = collectionChecksum(cn3);
          state.count = collectionCount(cn);
          state.count2 = collectionCount(cn2);
          state.count3 = collectionCount(cn3);
        },

        function(state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.checksum2, collectionChecksum(cn2));
          assertEqual(state.checksum3, collectionChecksum(cn3));
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.count2, collectionCount(cn2));
          assertEqual(state.count3, collectionCount(cn3));
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
